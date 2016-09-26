
// PUBLIC API
#include <disir/disir.h>

#include "test_helper.h"


// Test mold API with empty mold.
class ContextRestrictionMoldKeyvalStringTest : public testing::Test
{
    void SetUp()
    {
        status = dc_mold_begin (&context_mold);
        ASSERT_STATUS (DISIR_STATUS_OK, status);

        // Setup string keyval
        status = dc_begin (context_mold, DISIR_CONTEXT_KEYVAL, &context_keyval);
        ASSERT_STATUS (DISIR_STATUS_OK, status);
        status = dc_set_value_type (context_keyval, DISIR_VALUE_TYPE_STRING);
        ASSERT_STATUS (DISIR_STATUS_OK, status);
        status = dc_begin (context_keyval, DISIR_CONTEXT_RESTRICTION, &context_restriction);
        ASSERT_STATUS (DISIR_STATUS_OK, status);
    }

    void TearDown()
    {

        if (context)
        {
            dc_destroy (&context);
        }
        if (context_mold)
        {
            dc_destroy (&context_mold);
        }
        if (context_keyval)
        {
            dc_destroy (&context_keyval);
        }
        if (context_restriction)
        {
            dc_destroy (&context_restriction);
        }
    }

public:
    enum disir_status status;
    const char *error;
    struct disir_context *context = NULL;;
    struct disir_context *context_mold = NULL;
    struct disir_context *context_keyval = NULL;
    struct disir_context *context_restriction = NULL;
};


TEST_F (ContextRestrictionMoldKeyvalStringTest,
        set_type_inclusive_entry_max_on_keyval_shall_succeed)
{
    status = dc_set_restriction_type (context_restriction, DISIR_RESTRICTION_INC_ENTRY_MAX);
    ASSERT_STATUS (DISIR_STATUS_OK, status);
}

TEST_F (ContextRestrictionMoldKeyvalStringTest,
        set_type_inclusive_entry_min_on_keyval_shall_succeed)
{
    status = dc_set_restriction_type (context_restriction, DISIR_RESTRICTION_INC_ENTRY_MIN);
    ASSERT_STATUS (DISIR_STATUS_OK, status);
}

TEST_F (ContextRestrictionMoldKeyvalStringTest,
        set_type_exclusive_value_enum_on_string_keyval_shall_fail)
{
    status = dc_set_restriction_type (context_restriction, DISIR_RESTRICTION_EXC_VALUE_ENUM);
    ASSERT_STATUS (DISIR_STATUS_WRONG_VALUE_TYPE, status);

    error = dc_context_error (context_restriction);
    ASSERT_STREQ ("cannot set restriction ENUM on STRING KEYVAL.", error);
}

TEST_F (ContextRestrictionMoldKeyvalStringTest,
        set_type_exclusive_value_range_on_string_keyval_shall_fail)
{
    status = dc_set_restriction_type (context_restriction, DISIR_RESTRICTION_EXC_VALUE_RANGE);
    ASSERT_STATUS (DISIR_STATUS_WRONG_VALUE_TYPE, status);

    error = dc_context_error (context_restriction);
    ASSERT_STREQ ("cannot set restriction RANGE on STRING KEYVAL.", error);
}

TEST_F (ContextRestrictionMoldKeyvalStringTest,
        set_type_exclusive_value_numeric_on_string_keyval_shall_fail)
{
    status = dc_set_restriction_type (context_restriction, DISIR_RESTRICTION_EXC_VALUE_NUMERIC);
    ASSERT_STATUS (DISIR_STATUS_WRONG_VALUE_TYPE, status);

    error = dc_context_error (context_restriction);
    ASSERT_STREQ ("cannot set restriction NUMERIC on STRING KEYVAL.", error);
}
