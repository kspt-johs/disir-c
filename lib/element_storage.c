#include <string.h>

#include <disir/disir.h>
#include <multimap.h>
#include <list.h>
#include <errno.h>

#include "context_private.h"
#include "collection.h"
#include "element_storage.h"
#include "log.h"

//!
//! Disir Element Storage holds the Disir Keyvals and Disir Sections
//!

//!
//! NOTES:
//!  * Would like to have some operation to insert and remove by index.
//!     That would hekp making a GUI tool to interactively add/remove
//!     entries to the config file more dynamic.
//!
//!

//! Make the element storage a complete ADT to the entire library
//! This way, we can really modify the internals without too much fuzz
//! around the codebase on this rather important interface
struct disir_element_storage
{
    // A multimap that holds all elements inserted by key.
    // Multiple values may be associated with a key, of which are stored
    // in insertion order.
    // Key is a string name of the element to store. We make a copy of the key
    // upon insertion, since foul things may happend if we reference the name
    // stored inside a context object that has been freed.
    // The map allows us to quickly retrieve named entries.
    struct multimap *es_map;

    // List to simply keep a insertion order of all keyval and section contexts
    // The list lets us iterate all child context in order of insertion - important
    // for the sake of consistency when exposing the raw dump of all children.
    struct list     *es_list;
};

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

//! INTERNAL API
struct disir_element_storage *
dx_element_storage_create (void)
{
    struct disir_element_storage *storage;

    storage = calloc(1, sizeof (struct disir_element_storage));
    if (storage == NULL)
    {
        goto error;
    }

    storage->es_map = multimap_create ((int (*)(const void *, const void*)) strcmp,
                                       (unsigned long (*)(const void*)) djb2);
    if (storage->es_map == NULL)
    {
        goto error;
    }

    storage->es_list = list_create ();
    if (storage->es_list == NULL)
    {
        goto error;
    }

    return storage;
error:
    if (storage && storage->es_map)
        multimap_destroy (storage->es_map, NULL, NULL);

    if (storage && storage->es_list)
        list_destroy (&storage->es_list);

    if (storage)
        free (storage);

    return NULL;
}

//! INTERNAL API
//! Will dc_destroy all stored contexts
//! Will make sure to decref the refcount added by insertion into element storage
enum disir_status
dx_element_storage_destroy (struct disir_element_storage **storage)
{
    list_iterator_t *iter;
    struct disir_context *context;

    if (storage == NULL || *storage == NULL)
    {
        log_debug (0, "invoked with storage NULL pointer.");
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    iter = list_iterator_create ((*storage)->es_list, LIST_HEAD);
    if (iter == NULL)
    {
        log_warn ("list_iterator_create allocation failed.");
        return DISIR_STATUS_NO_MEMORY;
    }

    // Destroy each context stored in the element storage (through the list)
    while ((context = list_iterator_next (iter)))
    {
        // Ignore return code - we just want to destroy and get out of town.
        // Destroying the context will decref our reference on it.
        if (context)
        {
            dc_destroy (&context);
        }
    }

    // Destroy iterator and element_storage data structures
    // multimap key is heap allocated upon insertion - free it upon deletion.
    list_iterator_destroy (&iter);
    list_destroy (&(*storage)->es_list);
    multimap_destroy ((*storage)->es_map, free, NULL);

    free (*storage);
    *storage = NULL;
    return DISIR_STATUS_OK;;
}

//! INTERNAL API
int32_t
dx_element_storage_numentries (struct disir_element_storage *storage)
{
    if (storage == NULL)
        return (-1);

    // Get the number of entries from the multimap
    return multimap_size (storage->es_map);
}

//! INTERNAL API
//! Make a copy of the input name to use as key for multimap. Only allocate if no such
//! key exist in the map.
//! Will increment context refcount.
enum disir_status
dx_element_storage_add (struct disir_element_storage *storage,
                        const char * name,
                        struct disir_context *context)
{
    enum disir_status status;
    int res;
    int keys_in_map;
    char *key;

    if (storage == NULL || name == NULL || context == NULL)
    {
        log_debug (0, "invoked with NULL pointer(s) (storage %p, name %p, context %p)",
                   storage, name, context);
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    keys_in_map = multimap_contains_key (storage->es_map, name);
    if (keys_in_map == 0)
    {
        // Map does not contain a key with this name. Allocate space to store
        // key in, so we can safely access the key memory even if the appointed context
        // is destroyed whilst in our storage.
        key = malloc (strlen (name) + 1); // XXX: Is there a more safe way to allocate this memory?
        memcpy(key, name, strlen (name) + 1);
    }

    // Add to map - will check if it already exists or not.
    res = multimap_push_value (storage->es_map,
            (keys_in_map ? name : key),
            context);
    if (res == (-ENOMEM))
    {
        log_warn ("multimap_push_value failed to allocate sufficient memory.");
        status = DISIR_STATUS_NO_MEMORY;
        goto map_error;
    }
    else if (res != 0)
    {
        log_warn ("attempted to add context (%p) to element storage which already exists",
                context);
        status = DISIR_STATUS_EXISTS;
        goto map_error;
    }

    // Add to list for chronological ordering.
    if (list_rpush (storage->es_list, context))
    {
        status = DISIR_STATUS_INTERNAL_ERROR;
        goto list_error;
    }

    dx_context_incref (context);

    return DISIR_STATUS_OK;;
list_error:
    multimap_remove_value (storage->es_map,
        (keys_in_map ? name : key),
        free, // Free's the allocated memory above, if removing last element with this key name.
        NULL);

map_error:
    if (keys_in_map == 0)
    {
        // Allocated on heap - free it.
        free (key);
    }

    return status;
}

enum disir_status
dx_element_storage_remove (struct disir_element_storage *storage,
                           const char * const name,
                           struct disir_context *context)
{
    multimap_remove_value (storage->es_map, name, free, context);
    if (list_remove (storage->es_list, context))
    {
        dx_context_decref (&context);
    }

    return DISIR_STATUS_OK;
}

enum disir_status
dx_element_storage_get (struct disir_element_storage *storage,
                        const char * const name,
                        struct disir_collection **collection)
{
    enum disir_status status;
    struct disir_context *context;
    struct disir_collection *col;
    struct multimap_value_iterator *iter;

    iter = multimap_fetch (storage->es_map, name);
    if (iter == NULL)
    {
        return DISIR_STATUS_NOT_EXIST;
    }
    col = dc_collection_create ();
    if (collection == NULL)
    {
        status = DISIR_STATUS_NO_MEMORY;
        goto error;
    }

    while ((context = multimap_iterator_next (iter)))
    {
        status = dc_collection_push_context (col, context);
        if (status != DISIR_STATUS_OK)
        {
            goto error;
        }
    }

    multimap_iterator_destroy (iter);

    *collection = col;
    return DISIR_STATUS_OK;
error:

    if (col)
    {
        dc_collection_finished (&col);
    }
    if (iter)
    {
        multimap_iterator_destroy (iter);
    }

    return status;
}

// INTERNAL API
enum disir_status
dx_element_storage_get_all (struct disir_element_storage *storage,
                            struct disir_collection **collection)
{
    enum disir_status status;
    struct disir_context *context;
    struct disir_collection *coll;
    struct list_iterator *iter;

    coll = NULL;
    iter = NULL;

    if (storage == NULL)
    {
        log_debug (0, "invoked with storage NULL pointer.");
        return DISIR_STATUS_INVALID_ARGUMENT;
    }
    if (collection == NULL)
    {
        log_debug (0, "invoked with collection NULL pointer.");
        return DISIR_STATUS_INVALID_ARGUMENT;
    }

    iter = list_iterator_create (storage->es_list, LIST_HEAD);
    if (iter == NULL)
    {
        log_warn ("in element_storage (%p) - list_iterator_create failed", storage);
        status = DISIR_STATUS_NO_MEMORY;
        goto error;
    }

    coll = dc_collection_create ();
    if (coll == NULL)
    {
        log_warn (
            "in element_storage (%p) - dc_collection_create failed to allocate sufficient memory",
            storage);
        status = DISIR_STATUS_NO_MEMORY;
        goto error;
    }

    while ((context = list_iterator_next (iter)))
    {
        status = dc_collection_push_context (coll, context);
        if (status != DISIR_STATUS_OK)
            goto error;
    }

    list_iterator_destroy (&iter);

    *collection = coll;

    return DISIR_STATUS_OK;
error:
    if (iter)
        list_iterator_destroy (&iter);
    if (coll)
    {
        dc_collection_finished (&coll);
    }

    return status;
}

//! INTERNAL API
enum disir_status
dx_element_storage_get_first(struct disir_element_storage *storage,
                             const char *name,
                             struct disir_context **context)
{
    struct disir_context *keyval;

    keyval = multimap_get_first (storage->es_map, name);
    if (keyval == NULL)
    {
        return DISIR_STATUS_NOT_EXIST;
    }

    *context = keyval;
    return DISIR_STATUS_OK;
}


