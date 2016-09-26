// External public includes
#include <stdlib.h>
#include <stdio.h>

// Public disir interface
#include <disir/disir.h>
#include <disir/context.h>

// Private
#include "context_private.h"
#include "config.h"
#include "section.h"
#include "keyval.h"
#include "mold.h"
#include "log.h"
#include "element_storage.h"
#include "restriction.h"


//! STATIC API
//!
//! \return DISIR_STATUS_INTERNAL_ERROR if invoked with non-element bearing context.
//! \return DISIR_STATUS_INVALID_CONTEXT if this context does not fulfill min/max restrictions
//!         of its children.
//! \return DISIR_STATUS_OK on success.
//!
static enum disir_status
validate_inclusive_restrictions (struct disir_context *context)
{
    enum disir_status status ;
    enum disir_status invalid;
    struct disir_collection *mold_collection;
    struct disir_collection *config_elements;
    const char *name;
    struct disir_context *element;
    int max;
    int min;
    int size;

    min = max = 0;

    invalid = DISIR_STATUS_OK;
    element = NULL;
    mold_collection = NULL;
    name = NULL;

    switch (dc_context_type (context))
    {
    case DISIR_CONTEXT_CONFIG:
        status = dc_get_elements (context->cx_config->cf_mold->mo_context, &mold_collection);
        break;
    case DISIR_CONTEXT_SECTION:
        status = dc_get_elements (context->cx_section->se_mold_equiv, &mold_collection);
        break;
    default:
        log_fatal ("Invoked internal validate with incorrect context %s.",
                   dc_context_type_string (context));
        status = DISIR_STATUS_INTERNAL_ERROR;
    }
    if (status != DISIR_STATUS_OK)
    {
        log_error ("cannot get mold equivalent entries for %s.", dc_context_type_string (context));
        return status;
    }

    do
    {
        if (element)
        {
            dc_putcontext (&element);
        }

        status = dc_collection_next (mold_collection, &element);
        if (status == DISIR_STATUS_EXHAUSTED)
        {
            status = DISIR_STATUS_OK;
            break;
        }

        if (status != DISIR_STATUS_OK)
            break;

        // Query how many elements in config there are of element.name
        dc_get_name (element, &name, NULL);
        status = dc_find_elements (context, name, &config_elements);
        if (status != DISIR_STATUS_OK && status != DISIR_STATUS_NOT_EXIST)
        {
            break;
        }
        if (status == DISIR_STATUS_OK)
        {
            size = dc_collection_size (config_elements);
            dc_collection_finished (&config_elements);
        }
        else
        {
            // No entries
            size = 0;
        }

        // find maximum/minimum number required.
        dx_restriction_entries_value (element, DISIR_RESTRICTION_INC_ENTRY_MIN,
                &(context)->cx_config->cf_version, &min);
        dx_restriction_entries_value (element, DISIR_RESTRICTION_INC_ENTRY_MAX,
                &(context)->cx_config->cf_version, &max);


        // Minimum restriction not fufilled.
        if (size < min)
        {
            context->CONTEXT_STATE_INVALID = 1;
            // XXX Issue error report entry
            log_debug (2, "violated minimum restriction (count %d vx min %d)", size, min);
            invalid = DISIR_STATUS_INVALID_CONTEXT;
        }

        // Maximum restriction not fufilled
        if (size > max && max != 0)
        {
            context->CONTEXT_STATE_INVALID = 1;
            // XXX Issue error report entry
            log_debug (2, "violated maximum restriction (count %d vx max %d)", size, max);
            invalid = DISIR_STATUS_INVALID_CONTEXT;
        }
    } while (1);
    if (mold_collection)
    {
        dc_collection_finished (&mold_collection);
    }

    return (status == DISIR_STATUS_OK ? invalid : status);
}

//! STATIC API
//!
//! \return DISIR_STATUS_INTERNAL_ERROR for non-handled DISIR_VALUE_TYPE
//! \return status of dx_restriction_exclusive_value_check
//!
static enum disir_status
validate_exclusive_restrictions (struct disir_context *context)
{
    enum disir_status status;

    switch (dc_value_type (context))
    {
    case DISIR_VALUE_TYPE_INTEGER:
    {
        status = dx_restriction_exclusive_value_check (context,
                                                       context->cx_keyval->kv_value.dv_integer,
                                                       0);
        break;
    }
    case DISIR_VALUE_TYPE_FLOAT:
    {
        status = dx_restriction_exclusive_value_check (context, 0,
                                                       context->cx_keyval->kv_value.dv_float);
        break;

    }
    case DISIR_VALUE_TYPE_ENUM:
    {
        // TODO: Support enum type check.
        status = DISIR_STATUS_INTERNAL_ERROR;
        break;
    }
    case DISIR_VALUE_TYPE_STRING:
    case DISIR_VALUE_TYPE_BOOLEAN:
        // Nothing to do for string and boolean
        status = DISIR_STATUS_OK;
        break;
    default:
    {
        log_warn ("Checking exclusive restrictions of unknown value type: %s.",
                  dc_value_type_string (context));
        status = DISIR_STATUS_INTERNAL_ERROR;
    }
    }

    return status;
}

//! STATIC API
//!
//! \return DISIR_STATUS_INTERNAL_ERROR if context is not KEYVAL or SECTION.
//! \return DISIR_STATUS_RESTRICTION_VIOLATED if maximum entries for this context
//!         is violated in parent restriction.
//! \return DISIR_STATUS_OK if parent is not finalized.
//! \return DISIR_STATUS_OK on success.
//!
static enum disir_status
validate_finalized_parent_constructing_child_violating_max_restriction (struct disir_context *child)
{
    int max;
    int current_entries_count;
    struct disir_collection *collection;
    char *name;

    collection = NULL;
    max = 0;
    current_entries_count  = 0;

    // Restriction will be violated - but we allow it.
    if (child->cx_parent_context->CONTEXT_STATE_FINALIZED == 0)
    {
        return DISIR_STATUS_OK;
    }

    // Get name of current entry
    if (dc_context_type (child) == DISIR_CONTEXT_KEYVAL)
    {
        name = child->cx_keyval->kv_name.dv_string;
    }
    else if (dc_context_type (child) == DISIR_CONTEXT_SECTION)
    {
        name = child->cx_section->se_name.dv_string;
    }
    else
    {
        log_fatal ("Context unhandled (%s): %s", dc_context_type_string (child), __FILE__);
        return DISIR_STATUS_INTERNAL_ERROR;
    }

    // Query all entries in parent with our name. Check if it is exhausted.
    dx_restriction_entries_value (child, DISIR_RESTRICTION_INC_ENTRY_MAX, NULL, &max);
    dc_find_elements (child->cx_parent_context, name, &collection);
    if (collection)
    {
        current_entries_count = dc_collection_size (collection);
        dc_collection_finished (&collection);
    }

    if (max != 0 && max <= current_entries_count)
    {
        // Dis-allow finalizing this entry.
        dx_log_context (child, "Maximum entries of %d exceeded", max);
        return DISIR_STATUS_RESTRICTION_VIOLATED;
    }

    return DISIR_STATUS_OK;
}

//! STATIC API
//!
//! \return DISIR_STATUS_ELEMENTS_INVALID if any of context' children are not valid.
//! \return DISIR_STATUS_OK when all children are valid.
//!
static enum disir_status
validate_children (struct disir_context *context)
{
    enum disir_status status;
    enum disir_status status_validate;
    enum disir_status invalid;
    struct disir_context *element;
    struct disir_collection *collection;

    invalid = DISIR_STATUS_OK;
    status_validate = DISIR_STATUS_OK;
    element = NULL;
    collection = NULL;

    // Invoke recursively on children
    status = dc_get_elements (context, &collection);
    if (status != DISIR_STATUS_OK)
    {
        log_debug (2, "validating children failed to retrieve elements with status: %s",
                      disir_status_string (status));
        return status;
    }

    do
    {
        // Put back the element resource from last iteration
        if (element)
        {
            status = dc_putcontext (&element);
            if (status != DISIR_STATUS_OK)
                break;
        }

        // Break out if a serious error occurred in last iteration
        if (status_validate != DISIR_STATUS_OK && status_validate != DISIR_STATUS_INVALID_CONTEXT)
        {
            break;
        }

        status = dc_collection_next (collection, &element);
        if (status == DISIR_STATUS_EXHAUSTED)
        {
            status = DISIR_STATUS_OK;
            break;
        }
        else if (status != DISIR_STATUS_OK)
        {
            break;
        }

        status_validate = dx_validate_context (element);
        invalid = (status_validate != DISIR_STATUS_OK ? DISIR_STATUS_ELEMENTS_INVALID : invalid);
    } while (1);

    if (collection)
    {
        dc_collection_finished (&collection);
    }

    return (status == DISIR_STATUS_OK ? invalid : status);
}


//! STATIC API
//!
//! \return DISIR_STATUS_MOLD_MISSING if keyval doesnt have a mold equivalent.
//! \return DISIR_STATUS_RESTRICTION_VIOLATED if keyval is finalized
//!         and violating parent max restrictions.
//! \return DISIR_STATUS_WRONG_VALUE_TYPE if mold value type differs from keyval value type.
//! \return DISIR_STATUS_OK on valid keyval.
//!
static enum disir_status
validate_config_keyval (struct disir_context *keyval)
{
    enum disir_status status;
    struct disir_context *mold_equiv;

    mold_equiv = keyval->cx_keyval->kv_mold_equiv;

    // XXX: Should we check for empty name? This scenario is unlikely.

    // If no mold equivalent, this entry is invalid.
    if (mold_equiv == NULL)
    {
        dx_log_context (keyval, "KEYVAL missing mold equivalent entry for name '%s'.",
                                keyval->cx_keyval->kv_name.dv_string);
        return DISIR_STATUS_MOLD_MISSING;
    }

    status = validate_finalized_parent_constructing_child_violating_max_restriction (keyval);
    if (status != DISIR_STATUS_OK)
    {
        return status;
    }

    // Ensure that the correct type is assigned.
    // This might occur when constructing the config keyval. (unserializing)
    if (dc_value_type (mold_equiv) != dc_value_type (keyval))
    {
        dx_log_context (keyval, "Value type '%s' assigned. Expecting '%s'",
                                dc_value_type_string (mold_equiv), dc_value_type_string (keyval));
        return DISIR_STATUS_WRONG_VALUE_TYPE;
    }

    // Verify value
    status = validate_exclusive_restrictions (keyval);
    if (status != DISIR_STATUS_OK)
    {
        return status;
    }

    return DISIR_STATUS_OK;
}

//! STATIC API
//!
//! \return DISIR_STATUS_MOLD_MISSING if keyval doesnt have a mold equivalent.
//! \return DISIR_STATUS_RESTRICTION_VIOLATED if keyval is finalized
//!         and violating parent max restrictions.
//! \return DISIR_STATUS_WRONG_VALUE_TYPE if mold value type differs from section value type.
//! \return DISIR_STATUS_OK on valid section.
//!
static enum disir_status
validate_config_section (struct disir_context *section)
{
    enum disir_status status;
    struct disir_context *mold_equiv;

    mold_equiv = section->cx_section->se_mold_equiv;

    // If no mold equivalent, this entry is invalid.
    if (mold_equiv == NULL)
    {
        dx_log_context (section, "SECTION missing mold equivalent entry for name '%s'.",
                                 section->cx_section->se_name.dv_string);
        return DISIR_STATUS_MOLD_MISSING;
    }

    status = validate_finalized_parent_constructing_child_violating_max_restriction (section);
    if (status != DISIR_STATUS_OK)
    {
        return status;
    }

    // Ensure that the correct type is assigned.
    // This might occur when constructing the config section. (unserializing)
    if (dc_value_type (mold_equiv) != dc_value_type (section))
    {
        dx_log_context (section, "Value type '%s' assigned. Expecting '%s'",
                                 dc_value_type_string (mold_equiv),
                                 dc_value_type_string (section));
        return DISIR_STATUS_WRONG_VALUE_TYPE;
    }

    status = validate_inclusive_restrictions (section);
    return status;
}


//! STATIC API
//!
//! Check various conditions on the input context. Return codes indicate
//! the nature of its invalidity. Will also check validity of child contexts.
//!
//! \return DISIR_STATUS_ELEMENTS_INVALID if any of the contexts children are invalid.
//! \return DISIR_STATUS_MOLD_MISSING if a context whose top-level is CONFIG, is missing
//!         a mold equivalent entry.
//! \return DISIR_STATUS_WRONG_VALUE_TYPE if the mold equivalent context type differs
//!         from the input context type.
//! \return DISIR_STATUS_OK on success.
//!
static enum disir_status
validate_context_validity (struct disir_context *context)
{
    enum disir_status status;
    enum disir_status invalid;

    invalid = DISIR_STATUS_OK;
    status = DISIR_STATUS_OK;

    switch (dc_context_type (context))
    {
    case DISIR_CONTEXT_CONFIG:
    {
        status = validate_inclusive_restrictions (context);
        invalid = (status == DISIR_STATUS_INVALID_CONTEXT ? status : invalid);

        if (status != DISIR_STATUS_OK && status != DISIR_STATUS_INVALID_CONTEXT)
        {
            // Something is horribly wrong - break out.
            break;
        }
        // FALL-THROUGH
    }
    case DISIR_CONTEXT_MOLD:
    {
        // TODO: Clear error reports
        status = validate_children (context);
        break;
    }
    case DISIR_CONTEXT_SECTION:
    {
        if (dc_context_type (context->cx_root_context) == DISIR_CONTEXT_CONFIG)
        {
            // Invoke config operation
            invalid = validate_config_section (context);
        }
        else
        {
            // Nothing to check for a mold operation?
        }

        if (invalid != DISIR_STATUS_INVALID_CONTEXT ||
            invalid != DISIR_STATUS_RESTRICTION_VIOLATED ||
            invalid != DISIR_STATUS_MOLD_MISSING ||
            invalid != DISIR_STATUS_WRONG_VALUE_TYPE)
        {
            // Break out - something horrible happend
            break;
        }

        status = validate_children (context);
        break;
    }
    case DISIR_CONTEXT_KEYVAL:
    {
        if (dc_context_type (context->cx_root_context) == DISIR_CONTEXT_CONFIG)
        {
            // Invoke config operation
            invalid = validate_config_keyval (context);
        }
        else if (dc_context_type (context->cx_root_context) == DISIR_CONTEXT_MOLD)
        {
            // Invoke mold operation
            // Cannot add without known type.
            if (dx_value_type_sanify(context->cx_keyval->kv_value.dv_type) == DISIR_VALUE_TYPE_UNKNOWN)
            {
                dx_log_context (context, "Missing type component for keyval.");
                invalid = DISIR_STATUS_WRONG_VALUE_TYPE;;
            }

            // Additional restrictions apply for keyvals added to a root mold
            // Only check if we are not already in erroneous state
            if (invalid == DISIR_STATUS_OK &&
                context->cx_keyval->kv_default_queue == NULL)
            {
                dx_log_context (context, "Missing default entry for keyval.");
                invalid = DISIR_STATUS_INVALID_CONTEXT;
            }
        }
        break;
    }
    default:
    {
        log_fatal ("Invoked validate operation on unhandled context %s.",
                   dc_context_type_string (context));
        status = DISIR_STATUS_INTERNAL_ERROR;
    }
    }

    return (status != DISIR_STATUS_OK ? status : invalid);
}

//! INTERNAL API
enum disir_status
dx_validate_context (struct disir_context *context)
{
    enum disir_status status;

    TRACE_ENTER ("context %s", dc_context_type_string(context));

    status = DISIR_STATUS_OK;

    // Optimistically clear INVALID state
    // The below checks will return it to invalid state if checks fail.
    context->CONTEXT_STATE_INVALID = 0;

    status = validate_context_validity (context);
    if (status == DISIR_STATUS_INVALID_CONTEXT ||
        status == DISIR_STATUS_WRONG_VALUE_TYPE ||
        status == DISIR_STATUS_MOLD_MISSING ||
        status == DISIR_STATUS_RESTRICTION_VIOLATED)
    {
        context->CONTEXT_STATE_INVALID = 1;
        if (context->cx_parent_context &&
            context->cx_parent_context->CONTEXT_STATE_CONSTRUCTING)
        {
            status = DISIR_STATUS_INVALID_CONTEXT;
        }
    }

    TRACE_EXIT ("status: %s", disir_status_string (status));
    return status;
}
