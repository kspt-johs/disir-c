
#include <disir/disir.h>
#include <disir/collection.h>

void
test_context_get_elements (void **state)
{
    enum disir_status status;
    dc_t *invalid;
    dc_t *schema_context;
    dcc_t *collection;
    struct disir_schema *schema;

    LOG_TEST_START

    // Setup schema
    status = dc_schema_begin (&schema_context);
    assert_int_equal (status, DISIR_STATUS_OK);
    status = dc_add_keyval_string (schema_context, "keyval1",
                                   "keyval1_value", "keyval1_doc", NULL);
    assert_int_equal (status, DISIR_STATUS_OK);
    status = dc_add_keyval_string (schema_context, "keyval2", "keyval2_value",
                                   "keyval2_doc", NULL);
    assert_int_equal (status, DISIR_STATUS_OK);
    status = dc_add_keyval_string (schema_context, "keyval3", "keyval3_value",
                                   "keyval3_doc", NULL);
    assert_int_equal (status, DISIR_STATUS_OK);
    status = dc_add_keyval_string (schema_context, "keyval4", "keyval4_value",
                                   "keyval4_doc", NULL);
    assert_int_equal (status, DISIR_STATUS_OK);
    status = dc_schema_finalize (&schema_context, &schema);
    assert_int_equal (status, DISIR_STATUS_OK);
    schema_context = dc_schema_getcontext (schema);
    assert_non_null (schema_context);

    // Invalid input check
    status = dc_get_elements (NULL, NULL);
    assert_int_equal (status, DISIR_STATUS_INVALID_ARGUMENT);
    status = dc_get_elements (schema_context, NULL);
    assert_int_equal (status, DISIR_STATUS_INVALID_ARGUMENT);
    status = dc_get_elements (NULL, &collection);
    assert_int_equal (status, DISIR_STATUS_INVALID_ARGUMENT);

    // enumerate all DISIR_CONTEST_* and attempt to get elements from invalid context type.
    invalid = dx_context_create (DISIR_CONTEXT_CONFIG);
    assert_non_null (invalid);
    while (invalid->cx_type != DISIR_CONTEXT_UNKNOWN)
    {
        // valid types
        if (invalid->cx_type == DISIR_CONTEXT_CONFIG ||
            invalid->cx_type == DISIR_CONTEXT_SCHEMA ||
            invalid->cx_type == DISIR_CONTEXT_SECTION)
        {
            invalid->cx_type++;
            continue;
        }

        status = dc_get_elements (invalid, &collection);
        assert_int_equal (status, DISIR_STATUS_WRONG_CONTEXT);

        invalid->cx_type++;
    }

    // Valid query from schema
    status = dc_get_elements (schema_context, &collection);
    assert_int_equal (status, DISIR_STATUS_OK);
    assert_int_equal (dc_collection_size (collection), 4);

    // TODO: Query SECTION and CONFIG

    // Cleanup
    dx_context_destroy (&invalid);
    status = dc_destroy (&schema_context);
    assert_int_equal (status, DISIR_STATUS_OK);
    status = dc_collection_finished (&collection);
    assert_int_equal (status, DISIR_STATUS_OK);

    LOG_TEST_END
}

const struct CMUnitTest disir_context_query_tests[] = {
    cmocka_unit_test (test_context_get_elements),
};

