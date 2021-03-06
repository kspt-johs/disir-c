// external public includes
#include <stdlib.h>
#include <stdio.h>

// public disir interface
#include <disir/disir.h>
#include <disir/context.h>

// private
#include "context_private.h"
#include "mold.h"
#include "documentation.h"
#include "mqueue.h"
#include "log.h"


//! PUBLIC API
struct disir_context *
dc_mold_getcontext (struct disir_mold *mold)
{
    if (mold == NULL)
    {
        log_debug (0, "invoked with NULL mold pointer.");
        return NULL;
    }

    dx_context_incref (mold->mo_context);

    return mold->mo_context;
}

//! PUBLIC API
enum disir_status
dc_mold_begin (struct disir_context **mold)
{
    struct disir_context *context;

    if (mold == NULL)
    {
        log_debug (0, "invoked with NULL mold pointer.");
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    context = dx_context_create (DISIR_CONTEXT_MOLD);
    if (context == NULL)
    {
        log_error ("failed to allocate context for mold.");
        return DISIR_STATUS_NO_MEMORY;
    }

    context->cx_mold = dx_mold_create (context);
    if (context->cx_mold == NULL)
    {
        log_error ("failed to allocate mold for context.");
        dx_context_destroy (&context);
        return DISIR_STATUS_NO_MEMORY;
    }

    // Set root context to self (such that children can inherit)
    context->cx_root_context = context;

    *mold = context;
    return DISIR_STATUS_OK;
}

// PUBLIC API
enum disir_status
dc_mold_finalize (struct disir_context **context, struct disir_mold **mold)
{
    enum disir_status status;

    TRACE_ENTER ("context: %p, mold: %p", context, mold);

    status = CONTEXT_DOUBLE_NULL_INVALID_TYPE_CHECK (context);
    if (status != DISIR_STATUS_OK)
    {
        // Already looged
        return status;
    }
    if (mold == NULL)
    {
        log_debug (0, "invoked with NULL mold pointer.");
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    if (dx_context_type_sanify ((*context)->cx_type) != DISIR_CONTEXT_MOLD)
    {
        dx_log_context (*context, "Cannot call %s() on top-level context( %s )",
                        __func__, dc_context_type_string (*context));
        return DISIR_STATUS_WRONG_CONTEXT;
    }

    // Perform full mold validation.
    status = dx_validate_context (*context);
    // Only set state if the validate operation went as planned
    if (status == DISIR_STATUS_OK || status == DISIR_STATUS_INVALID_CONTEXT)
    {
        *mold = (*context)->cx_mold;
        (*context)->CONTEXT_STATE_FINALIZED = 1;
        (*context)->CONTEXT_STATE_CONSTRUCTING = 0;

        // We do not decref context refcount on finalize
        // Deprive the user of his context reference.
        *context = NULL;
    }
    else
    {
        log_fatal_context (*context, "failed internally with status %s",
                                     disir_status_string (status));
        status = DISIR_STATUS_INTERNAL_ERROR;
    }

    TRACE_EXIT ("status: %s, *mold: %p", disir_status_string (status), mold);
    return status;
}


//! INTERNAL API
struct disir_mold *
dx_mold_create (struct disir_context *context)
{
    struct disir_mold *mold;

    mold = calloc (1, sizeof (struct disir_mold));
    if (mold == NULL)
    {
        goto error;
    }

    mold->mo_reference_count = 1;
    mold->mo_context = context;
    mold->mo_elements = dx_element_storage_create ();
    if (mold->mo_elements == NULL)
    {
        goto error;
    }

    // Initialize version to 1.0
    mold->mo_version.sv_major = 1;
    mold->mo_version.sv_minor = 0;

    return mold;
error:
    if (mold && mold->mo_elements)
    {
        dx_element_storage_destroy (&mold->mo_elements);
    }
    if (mold)
    {
        free (mold);
    }

    return NULL;
}

//! INTERNAL API
enum disir_status
dx_mold_destroy (struct disir_mold **mold)
{
    enum disir_status status;
    struct disir_context *context;
    struct disir_documentation *doc;
    struct disir_collection *collection;

    if (mold == NULL || *mold == NULL)
    {
        log_debug (0, "invoked with NULL mold pointer.");
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    // Destroy all element_storage children
    status = dx_element_storage_get_all ((*mold)->mo_elements, &collection);
    if (status == DISIR_STATUS_OK)
    {
        while (dx_collection_next_noncoalesce (collection, &context) != DISIR_STATUS_EXHAUSTED)
        {
            dx_context_decref (&context);
            dc_putcontext (&context);
        }
        dc_collection_finished (&collection);
    }
    else
    {
        log_warn ("failed to get_all from internal element storage: %s",
                  disir_status_string (status));
    }

    // Destroy element storage in the mold.
    dx_element_storage_destroy (&(*mold)->mo_elements);

    // Destroy the documentation associated with the mold.
    while ((doc = MQ_POP ((*mold)->mo_documentation_queue)))
    {
        context = doc->dd_context;
        dc_destroy (&context);
    }

    free (*mold);
    *mold = NULL;

    return DISIR_STATUS_OK;
}

//! INTERNAL API
enum disir_status
dx_mold_update_version (struct disir_mold *mold, struct disir_version *version)
{
    struct disir_version fact;
    char buffer[32];

    if (mold == NULL || version == NULL)
    {
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    fact.sv_major = mold->mo_version.sv_major;
    fact.sv_minor = mold->mo_version.sv_minor;

    if (mold->mo_version.sv_major < version->sv_major)
    {
        fact.sv_major = version->sv_major;
        fact.sv_minor = version->sv_minor;
    }
    else if (mold->mo_version.sv_major == version->sv_major &&
             mold->mo_version.sv_minor < version->sv_minor)
    {
        fact.sv_minor = version->sv_minor;
    }

    mold->mo_version.sv_major = fact.sv_major;
    mold->mo_version.sv_minor = fact.sv_minor;

    log_debug (6, "mold (%p) version sat to: %s",
               mold, dc_version_string (buffer, 32, &fact));

    return DISIR_STATUS_OK;
}

//! PUBLIC API
enum disir_status
dc_mold_get_version (struct disir_mold *mold, struct disir_version *version)
{
    if (mold == NULL || version == NULL)
    {
        log_debug (0, "invoked with NULL pointer(s)");
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    version->sv_major = mold->mo_version.sv_major;
    version->sv_minor = mold->mo_version.sv_minor;

    return DISIR_STATUS_OK;
}

