#include "unity/src/unity.h"
#include <string.h>

#include "../src/ProsecCoAP.h"

namespace
{
    void assertBytesEqual(const uint8_t *expected, const uint8_t *actual, size_t length)
    {
        TEST_ASSERT_EQUAL_INT(0, memcmp(expected, actual, length));
    }

    void assertOptionValue(const Coap::Option &option, const uint8_t *expectedValue, size_t expectedLength)
    {
        TEST_ASSERT_EQUAL(expectedLength, option.length);
        assertBytesEqual(expectedValue, option.value, expectedLength);
    }
}

void setUp(void)
{
}

void tearDown(void)
{
}

void test_constructor_sets_default_header(void)
{
    Coap::Message message;

    TEST_ASSERT_EQUAL(COAP_VERSION, message.getVersion());
    TEST_ASSERT_EQUAL(Coap::MessageType::NON, message.getType());
    TEST_ASSERT_EQUAL(Coap::MessageCode::EMPTY, message.getCode());
    TEST_ASSERT_EQUAL(COAP_HEADER_SIZE, message.getLength());
    TEST_ASSERT_EQUAL(0, message.getTokenLength());
}

void test_constructor_with_parameters_sets_expected_values(void)
{
    Coap::Message message(Coap::MessageType::CON, Coap::MessageCode::GET, 0x1234);

    TEST_ASSERT_EQUAL(Coap::MessageType::CON, message.getType());
    TEST_ASSERT_EQUAL(Coap::MessageCode::GET, message.getCode());
    TEST_ASSERT_EQUAL(0x1234, message.getId());
}

void test_setters_update_type_code_and_id(void)
{
    Coap::Message message;

    message.setType(Coap::MessageType::RST);
    message.setCode(Coap::MessageCode::CONTENT);
    message.setId(0xABCD);

    TEST_ASSERT_EQUAL(Coap::MessageType::RST, message.getType());
    TEST_ASSERT_EQUAL(Coap::MessageCode::CONTENT, message.getCode());
    TEST_ASSERT_EQUAL(0xABCD, message.getId());
}

void test_setToken_sets_and_overwrites_token(void)
{
    Coap::Message message;
    const uint8_t tokenA[] = {0xAA, 0xBB, 0xCC, 0xDD};
    const uint8_t tokenB[] = {0x01, 0x02};

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.setToken(tokenA, sizeof(tokenA)));
    TEST_ASSERT_EQUAL(sizeof(tokenA), message.getTokenLength());
    TEST_ASSERT_EQUAL(COAP_HEADER_SIZE + sizeof(tokenA), message.getLength());
    assertBytesEqual(tokenA, message.getToken(), sizeof(tokenA));

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.setToken(tokenB, sizeof(tokenB)));
    TEST_ASSERT_EQUAL(sizeof(tokenB), message.getTokenLength());
    TEST_ASSERT_EQUAL(COAP_HEADER_SIZE + sizeof(tokenB), message.getLength());
    assertBytesEqual(tokenB, message.getToken(), sizeof(tokenB));
}

void test_setToken_rejects_tokens_longer_than_spec_limit(void)
{
    Coap::Message message;
    const uint8_t validToken[] = {0x10, 0x11};
    uint8_t tooLongToken[COAP_MAX_TOKEN_LENGTH + 1] = {0};

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.setToken(validToken, sizeof(validToken)));

    TEST_ASSERT_EQUAL(Coap::ErrorCode::INVALID_ARGUMENT, message.setToken(tooLongToken, sizeof(tooLongToken)));
    TEST_ASSERT_EQUAL(sizeof(validToken), message.getTokenLength());
    assertBytesEqual(validToken, message.getToken(), sizeof(validToken));
}

void test_matchesToken_requires_matching_content_and_length(void)
{
    Coap::Message message;
    const uint8_t token[] = {0x44, 0x55, 0x66};
    const uint8_t differentToken[] = {0x44, 0x55, 0x67};

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.setToken(token, sizeof(token)));

    TEST_ASSERT_TRUE(message.matchesToken(token, sizeof(token)));
    TEST_ASSERT_FALSE(message.matchesToken(differentToken, sizeof(differentToken)));
    TEST_ASSERT_FALSE(message.matchesToken(token, sizeof(token) - 1));
}

void test_addOption_keeps_options_ordered_by_number(void)
{
    Coap::Message message;
    const uint8_t queryValue[] = {'u', 'n', 'i', 't'};
    const uint8_t pathValue[] = {'t', 'e', 'm', 'p'};

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.addOption(Coap::OptionNumber::URI_QUERY, queryValue, sizeof(queryValue)));
    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.addOption(Coap::OptionNumber::URI_PATH, pathValue, sizeof(pathValue)));

    Coap::OptionIterator it = message.getOptionIterator();
    Coap::Option option;

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, it.next(option));
    TEST_ASSERT_EQUAL(Coap::OptionNumber::URI_PATH, option.number);
    assertOptionValue(option, pathValue, sizeof(pathValue));

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, it.next(option));
    TEST_ASSERT_EQUAL(Coap::OptionNumber::URI_QUERY, option.number);
    assertOptionValue(option, queryValue, sizeof(queryValue));

    TEST_ASSERT_EQUAL(Coap::ErrorCode::NOT_FOUND, it.next(option));
}

void test_addOption_rejects_duplicate_single_instance_option(void)
{
    Coap::Message message;
    const uint8_t formatA[] = {0x32};
    const uint8_t formatB[] = {0x2A};

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK,
                      message.addOption(Coap::OptionNumber::CONTENT_FORMAT, formatA, sizeof(formatA)));
    TEST_ASSERT_EQUAL(Coap::ErrorCode::NOT_SUPPORTED,
                      message.addOption(Coap::OptionNumber::CONTENT_FORMAT, formatB, sizeof(formatB)));

    Coap::Option option;
    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.getOption(Coap::OptionNumber::CONTENT_FORMAT, option));
    assertOptionValue(option, formatA, sizeof(formatA));
}

void test_addPayload_roundtrip_and_rejects_second_payload(void)
{
    Coap::Message message;
    const uint8_t payload[] = {'h', 'e', 'l', 'l', 'o'};

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.addPayload(payload, sizeof(payload)));

    const uint8_t *readPayload = nullptr;
    size_t readLength = 0;
    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.getPayload(readPayload, readLength));
    TEST_ASSERT_EQUAL(sizeof(payload), readLength);
    assertBytesEqual(payload, readPayload, readLength);

    TEST_ASSERT_EQUAL(Coap::ErrorCode::NOT_SUPPORTED, message.addPayload(payload, sizeof(payload)));
}

void test_addPath_and_getPath_roundtrip(void)
{
    Coap::Message message;
    String path;

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.addPath("/sensors/temp?unit=celsius&scale=metric"));
    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.getPath(path));
    TEST_ASSERT_EQUAL_STRING("/sensors/temp?unit=celsius&scale=metric", path.c_str());
}

void test_addPath_rejects_invalid_query_separator_position(void)
{
    Coap::Message message;

    TEST_ASSERT_EQUAL(Coap::ErrorCode::INVALID_ARGUMENT, message.addPath("/sensors/temp&unit=celsius"));
}

void test_getMaxAge_returns_default_when_option_absent(void)
{
    Coap::Message message;
    uint32_t age = 0;

    TEST_ASSERT_EQUAL(Coap::ErrorCode::NOT_FOUND, message.getMaxAge(age));
    TEST_ASSERT_EQUAL(60, age);
}

void test_setMaxAge_stores_minimal_encoding_and_reads_back(void)
{
    Coap::Message message;
    uint32_t age = 0;

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.setMaxAge(0x012345));
    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.getMaxAge(age));
    TEST_ASSERT_EQUAL(0x012345, age);
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_constructor_sets_default_header);
    RUN_TEST(test_constructor_with_parameters_sets_expected_values);
    RUN_TEST(test_setters_update_type_code_and_id);
    RUN_TEST(test_setToken_sets_and_overwrites_token);
    RUN_TEST(test_setToken_rejects_tokens_longer_than_spec_limit);
    RUN_TEST(test_matchesToken_requires_matching_content_and_length);
    RUN_TEST(test_addOption_keeps_options_ordered_by_number);
    RUN_TEST(test_addOption_rejects_duplicate_single_instance_option);
    RUN_TEST(test_addPayload_roundtrip_and_rejects_second_payload);
    RUN_TEST(test_addPath_and_getPath_roundtrip);
    RUN_TEST(test_addPath_rejects_invalid_query_separator_position);
    RUN_TEST(test_getMaxAge_returns_default_when_option_absent);
    RUN_TEST(test_setMaxAge_stores_minimal_encoding_and_reads_back);

    return UNITY_END();
}

#include "../src/detail/Detail.cpp"
#include "../src/Observers.cpp"
#include "../src/ProsecCoAP.cpp"
