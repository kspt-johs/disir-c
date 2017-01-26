#include <stdlib.h>
#include <string.h>

#include <disir/disir.h>
#include <disir/io.h>

#include "config.h"
#include "disir_private.h"
#include "log.h"
#include "mqueue.h"
#include "multimap.h"


// STATIC INTERNAL
// String hashing function for the multimap
// http://www.cse.yorku.ca/~oz/hash.html
static unsigned long djb2 (char *str)
{
    unsigned long hash = 5381;
    char c;
    while( (c = *str++) ) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// STATIC INTERNAL
static enum disir_status
recurse_elements_valid (struct disir_context *context,
                        struct disir_collection *collection)
{
    enum disir_status status;
    enum disir_status invalid;
    struct disir_collection *col;
    struct disir_context *element;

    invalid = (context->CONTEXT_STATE_INVALID == 1 ? DISIR_STATUS_INVALID_CONTEXT
                                                   : DISIR_STATUS_OK);

    status = dc_get_elements (context, &col);
    if (status != DISIR_STATUS_OK)
    {
        log_error ("Failed to retrieve elements from context: %s",
                    disir_status_string (status));
    }

    while (status == DISIR_STATUS_OK)
    {
        status = dc_collection_next (col, &element);
        if (status == DISIR_STATUS_EXHAUSTED)
        {
            status = DISIR_STATUS_OK;
            break;
        }
        if (status != DISIR_STATUS_OK)
        {
            log_error ("Failed to retrieve element from collection: %s",
                       disir_status_string (status));
            break;
        }

        status = dc_context_valid (element);
        if (status == DISIR_STATUS_INVALID_CONTEXT)
        {
            if (collection)
            {
                dc_collection_push_context (collection, element);
            }
            invalid = status;
        }

        // Recurse the sections
        if (dc_context_type (element) == DISIR_CONTEXT_SECTION)
        {
            status = recurse_elements_valid (element, collection);
            if (status == DISIR_STATUS_INVALID_CONTEXT)
            {
                invalid = status;
            }
        }

        dc_putcontext (&element);
        status = DISIR_STATUS_OK;
    }

    dc_collection_finished (&col);

    return (status == DISIR_STATUS_OK ? invalid : status);
}

//! PUBLIC API
enum disir_status
disir_config_read (struct disir_instance *instance, const char *entry_id,
                   struct disir_mold *mold, struct disir_config **config)
{
    enum disir_status status;
    struct disir_plugin_internal *plugin;

    plugin = NULL;

    if (instance == NULL || entry_id == NULL || config == NULL)
    {
        log_debug (0, "invoked with NULL argument(s). instance (%p), entry_id (%p), config (%p)",
                      instance, entry_id, config);
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    TRACE_ENTER ("instance (%p) entry_id (%s) mold (%p) config (%p)", instance, entry_id, mold, config);

    MQ_FOREACH (instance->dio_plugin_queue,
    ({
        if (entry->pi_plugin.dp_config_query == NULL)
        {
            log_debug (1, "Plugin '%s' does not implement config_query", entry->pi_name);
            continue;
        }
        status = entry->pi_plugin.dp_config_query (instance, &entry->pi_plugin, entry_id);
        if (status == DISIR_STATUS_NOT_EXIST)
            continue;
        if (status != DISIR_STATUS_EXISTS)
        {
            log_warn ("Plugin '%s' (named %s, type %s) failed to query for entry '%s': %s",
                      entry->pi_name, entry->pi_plugin.dp_name,
                      entry->pi_plugin.dp_type, entry_id), disir_status_string (status);
            continue;
        }

        plugin = entry;
        break;
    }));

    if (plugin)
    {
        if (plugin->pi_plugin.dp_config_read)
        {
            *config = NULL;
            status = plugin->pi_plugin.dp_config_read (instance, &plugin->pi_plugin,
                                                       entry_id, mold, config);

            // Config is loaded - we inject the loaded pluginname into the structure.
            if (*config != NULL)
            {
                (*config)->cf_plugin_name = strdup (plugin->pi_name);
            }
        }
        else
        {
            status = DISIR_STATUS_NO_CAN_DO;
            log_debug (1, "Plugin '%s' does not implement config_read", plugin->pi_name);
        }
    }
    else
    {
        disir_error_set (instance, "No plugin contains config entry '%s'", entry_id);
        status = DISIR_STATUS_NOT_EXIST;
    }

    TRACE_EXIT ("status: %s", disir_status_string (status));
    return status;
}

//! PUBLIC API
enum disir_status
disir_config_write (struct disir_instance *instance, const char *entry_id,
                    struct disir_config *config)
{
    enum disir_status status;
    struct disir_plugin_internal *plugin;

    plugin = NULL;

    if (instance == NULL || entry_id == NULL || config == NULL)
    {
        log_debug (0, "invoked with NULL argument(s). instance (%p), entry_id (%p), config (%p)",
                      instance, entry_id, config);
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    TRACE_ENTER ("instance (%p) entry_id (%s) config (%p)", instance, entry_id, config);

    if (config->cf_plugin_name == NULL)
    {
        log_warn ("Cannot persis config (%p), missing loaded plugin name.", config);
        disir_error_set (instance, "Config not loaded through plugin API."
                                " Cannot infere origin plugin."
                                " Please use plugin specific write operation instead.");
        return DISIR_STATUS_NO_CAN_DO;
    }

    MQ_FOREACH (instance->dio_plugin_queue,
    ({
        if (strcmp (entry->pi_name, config->cf_plugin_name) == 0)
        {
            plugin = entry;
            break;
        }
    }));

    if (plugin)
    {
        if (plugin->pi_plugin.dp_config_write)
        {
            status = plugin->pi_plugin.dp_config_write (instance, &plugin->pi_plugin,
                                                        entry_id, config);
        }
        else
        {
            status = DISIR_STATUS_NO_CAN_DO;
            log_debug (1, "Plugin '%s' does not implement config_write", plugin->pi_name);
        }
    }
    else
    {
        log_warn ("The plugin '%s' associated with Config (%p) is not loaded.",
                  config->cf_plugin_name, config);
        disir_error_set (instance, "No loaded plugin available: '%s'.", config->cf_plugin_name);
        status = DISIR_STATUS_NOT_EXIST;
    }

    TRACE_EXIT ("status: %s", disir_status_string (status));
    return status;
}

//! PUBLIC API
enum disir_status
disir_config_entries (struct disir_instance *instance, struct disir_entry **entries)
{
    enum disir_status status;
    struct multimap *map;
    struct disir_entry *queue;
    struct disir_entry *query;
    struct disir_entry *current;

    queue = NULL;
    query = NULL;
    current = NULL;

    if (instance == NULL || entries == NULL)
    {
        log_debug (0, "invoked with NULL argument(s). instance(%p), entries (%p)",
                      instance, entries);
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    map = multimap_create ((int (*)(const void *, const void *)) strcmp,
                           (unsigned long (*)(const void*)) djb2);
    if (map == NULL)
    {
        status = DISIR_STATUS_NO_MEMORY;
        goto out;
    }

    MQ_FOREACH (instance->dio_plugin_queue,
    ({
        if (entry->pi_plugin.dp_config_entries == NULL)
        {
            log_debug (1, "Plugin '%s' does not implement config_entries.", entry->pi_name);
            continue;
        }

        status = entry->pi_plugin.dp_config_entries (instance,
                                                     &entry->pi_plugin, &query);
        if (status != DISIR_STATUS_OK)
        {
            log_warn ("Plugin '%s' queried for config entries failed with status: %s",
                      entry->pi_name, disir_status_string (status));
            continue;
        }

        // iterate each entry, add them to queue if they are unique
        current = query;
        while (current != NULL)
        {

            query = current->next;

            if (multimap_contains_key (map, (void *)current->de_entry_name) == 0)
            {
                // Add entry to queue
                MQ_ENQUEUE (queue, current);
                multimap_push_value (map, (void *)current->de_entry_name, NULL);
            }
            else
            {
                // Duplicate entry that a previous plugin provides. Forget about it.
                disir_entry_finished (&current);
            }

            current = query;
        }
    }));

    *entries = queue;

    status = DISIR_STATUS_OK;
    // FALL-THROUGH
out:
    if (map)
        multimap_destroy (map, NULL, NULL);
    return status;
}

//! PUBLIC API
enum disir_status
disir_config_finished (struct disir_config **config)
{
    enum disir_status status;
    struct disir_context *context;

    if (config == NULL || *config == NULL)
        return DISIR_STATUS_INVALID_ARGUMENT;


    context = (*config)->cf_context;
    status = dc_destroy (&context);
    if (status == DISIR_STATUS_OK)
        *config = NULL;
    return status;
}

//! PUBLIC API
enum disir_status
disir_config_valid (struct disir_config *config, struct disir_collection **collection)
{
    enum disir_status status;
    struct disir_collection *col;

    col = NULL;
    if (config == NULL)
    {
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    if (collection)
    {
        col = dc_collection_create ();
    }

    status = recurse_elements_valid (config->cf_context, col);

    // Iterate the config - retrieve all children and call dx_context_valid on it.
    // Retrieve element strorage - pass it to recursive function

    if (collection)
    {
        if (dc_collection_size (col))
        {
            *collection = col;
        }
        else
        {
            dc_collection_finished (&col);
        }
    }

    return status;
}

