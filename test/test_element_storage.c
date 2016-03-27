
#include "element_storage.h"

const char *keyval_names[] = {
    "carfight",
    "snitch",
    "powertools",
    "riverbed",
    "rocks",
    "aspen",
    "thundra",
    "hellfire",
    "borean",
    "durotar",
    "thrall",
    "milk",
    "porridge",
    "flowers",
    NULL
};

int
setup_element_storage (void **state)
{
    struct disir_element_storage *storage;

    storage = dx_element_storage_create ();
    if (storage == NULL)
        return (-1);

    *state = storage;

    return (0);
}

int
teardown_element_storage (void **state)
{
    struct disir_element_storage *storage;

    storage = *state;
    dx_element_storage_destroy (&storage);

    return (0);
}

static void
test_element_storage_basic(void **state)
{
    enum disir_status status;
    struct disir_element_storage *storage;
    struct disir_element_storage *null_storage;

    LOG_TEST_START

    storage = dx_element_storage_create ();
    assert_non_null (storage);

    // Invalid arguments check
    status = dx_element_storage_destroy (NULL);
    assert_int_equal (status, DISIR_STATUS_INVALID_ARGUMENT);
    null_storage = NULL;
    status = dx_element_storage_destroy (&null_storage);
    assert_int_equal (status, DISIR_STATUS_INVALID_ARGUMENT);

    status = dx_element_storage_destroy (&storage);
    assert_null (storage);
    assert_int_equal (status, DISIR_STATUS_OK);

    LOG_TEST_END
}

static void
test_element_storage_add (void **state)
{
    enum disir_status status;
    dc_t *context;
    struct disir_element_storage *storage;
    const char *key;
    int i;

    storage = *state;

    LOG_TEST_START

    // Add X number of dc_t *context to storage, by name.
    for (i = 0, key = keyval_names[i]; key != NULL; i++, key = keyval_names[i])
    {
        context = dx_context_create (DISIR_CONTEXT_CONFIG); // XXX: Should be DISIR_CONTEXT_KEYVAL
        assert_non_null (context);
        status = dx_element_storage_add (storage, key, context);
        assert_int_equal (status, DISIR_STATUS_OK);
        // Ref count should be 2 when inserted into element storage
        assert_int_equal (context->cx_refcount, 2);

        // Decref the context, to free up the memoey again (when element storage is destroyed)
        dx_context_decref (context);
        assert_int_equal (context->cx_refcount, 1);
    }

    LOG_TEST_END
}


const struct CMUnitTest disir_element_storage_tests[] = {
  cmocka_unit_test(test_element_storage_basic),
  cmocka_unit_test_setup_teardown(test_element_storage_add,
          setup_element_storage, teardown_element_storage),
};

