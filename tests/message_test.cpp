#include "unity/src/unity.h"
#include <string.h>

#include "../src/ProsecCoAP.h"

namespace
{
    void handlerA(Coap::Message &, IPAddress, uint16_t)
    {
    }

    void handlerB(Coap::Message &, IPAddress, uint16_t)
    {
    }

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

void testConstructorSetsDefaultHeader(void)
{
    Coap::Message message;

    TEST_ASSERT_EQUAL(COAP_VERSION, message.getVersion());
    TEST_ASSERT_EQUAL(Coap::MessageType::NON, message.getType());
    TEST_ASSERT_EQUAL(Coap::MessageCode::EMPTY, message.getCode());
    TEST_ASSERT_EQUAL(COAP_HEADER_SIZE, message.getLength());
    TEST_ASSERT_EQUAL(0, message.getTokenLength());
}

void testConstructorWithParametersSetsExpectedValues(void)
{
    Coap::Message message(Coap::MessageType::CON, Coap::MessageCode::GET, 0x1234);

    TEST_ASSERT_EQUAL(Coap::MessageType::CON, message.getType());
    TEST_ASSERT_EQUAL(Coap::MessageCode::GET, message.getCode());
    TEST_ASSERT_EQUAL(0x1234, message.getId());
}

void testSettersUpdateTypeCodeAndId(void)
{
    Coap::Message message;

    message.setType(Coap::MessageType::RST);
    message.setCode(Coap::MessageCode::CONTENT);
    message.setId(0xABCD);

    TEST_ASSERT_EQUAL(Coap::MessageType::RST, message.getType());
    TEST_ASSERT_EQUAL(Coap::MessageCode::CONTENT, message.getCode());
    TEST_ASSERT_EQUAL(0xABCD, message.getId());
}

void testSetTokenSetsAndOverwritesToken(void)
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

void testSetTokenRejectsTokensLongerThanSpecLimit(void)
{
    Coap::Message message;
    const uint8_t validToken[] = {0x10, 0x11};
    uint8_t tooLongToken[COAP_MAX_TOKEN_LENGTH + 1] = {0};

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.setToken(validToken, sizeof(validToken)));

    TEST_ASSERT_EQUAL(Coap::ErrorCode::INVALID_ARGUMENT, message.setToken(tooLongToken, sizeof(tooLongToken)));
    // Expect the token to be the first one.
    TEST_ASSERT_EQUAL(sizeof(validToken), message.getTokenLength());
    assertBytesEqual(validToken, message.getToken(), sizeof(validToken));
}

void testSetTokenRejectsMalformedExistingTokenLength(void)
{
    Coap::Message message;
    const uint8_t originalToken[] = {0x10, 0x11};
    const uint8_t replacementToken[] = {0x22, 0x33};

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.setToken(originalToken, sizeof(originalToken)));

    uint8_t *raw = const_cast<uint8_t *>(message.asRaw());
    raw[0] = (raw[0] & 0xF0) | 0x0F; // Deliberately corrupt the token length to an invalid value.

    // Expect a MALFORMED_MESSAGE error due to the corrupted token length.
    TEST_ASSERT_EQUAL(Coap::ErrorCode::MALFORMED_MESSAGE, message.setToken(replacementToken, sizeof(replacementToken)));
    assertBytesEqual(originalToken, message.getToken(), sizeof(originalToken)); // The token should remain unchanged despite the malformed header.
}

void testMatchesTokenRequiresMatchingContentAndLength(void)
{
    Coap::Message message;
    const uint8_t token[] = {0x44, 0x55, 0x66};
    const uint8_t differentToken[] = {0x44, 0x55, 0x67};

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.setToken(token, sizeof(token)));

    TEST_ASSERT_TRUE(message.matchesToken(token, sizeof(token)));
    TEST_ASSERT_FALSE(message.matchesToken(differentToken, sizeof(differentToken)));
    TEST_ASSERT_FALSE(message.matchesToken(token, sizeof(token) - 1));
}

void testAddOptionKeepsOptionsOrderedByNumber(void)
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

void testAddOptionRejectsDuplicateSingleInstanceOption(void)
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

void testAddPayloadRoundtripAndRejectsSecondPayload(void)
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

void testAddPathAndGetPathRoundtrip(void)
{
    Coap::Message message;
    char insufficientPath[10];
    char path[64];

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.addPath("/sensors/temp?unit=celsius&scale=metric"));
    TEST_ASSERT_EQUAL(Coap::ErrorCode::BUFFER_TOO_SMALL,
                      message.getPath(insufficientPath, sizeof(insufficientPath)));
    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.getPath(path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING("/sensors/temp?unit=celsius&scale=metric", path);
}

void testAddPathRejectsInvalidQuerySeparatorPosition(void)
{
    Coap::Message message;

    TEST_ASSERT_EQUAL(Coap::ErrorCode::INVALID_ARGUMENT, message.addPath("/sensors/temp&unit=celsius"));
}

void testGetQuery(void)
{
    Coap::Message message;
    const uint8_t queryA[] = {'s', 'c', 'a', 'l', 'e', '=', 'm', 'e', 't', 'r', 'i', 'c'};
    const uint8_t queryB[] = {'u', 'n', 'i', 't', '=', 'c', 'e', 'l', 's', 'i', 'u', 's'};
    char insufficientValue[7];
    char value[16];

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.addOption(Coap::OptionNumber::URI_QUERY, queryA, sizeof(queryA)));
    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.addOption(Coap::OptionNumber::URI_QUERY, queryB, sizeof(queryB)));

    TEST_ASSERT_EQUAL(Coap::ErrorCode::BUFFER_TOO_SMALL,
                      message.getQuery("unit", 4, insufficientValue, sizeof(insufficientValue)));
    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK,
                      message.getQuery("unit", 4, value, sizeof(value)));
    TEST_ASSERT_EQUAL_STRING("celsius", value);
}

void testGetMaxAgeReturnsDefaultWhenOptionAbsent(void)
{
    Coap::Message message;
    uint32_t age = 0;

    TEST_ASSERT_EQUAL(Coap::ErrorCode::NOT_FOUND, message.getMaxAge(age));
    TEST_ASSERT_EQUAL(60, age);
}

void testSetMaxAgeStoresMinimalEncodingAndReadsBack(void)
{
    Coap::Message message;
    uint32_t age = 0;

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.setMaxAge(0x012345));
    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.getMaxAge(age));
    TEST_ASSERT_EQUAL(0x012345, age);
}

void testUriRegistryFindMatchesSecondRegisteredPath(void)
{
    Coap::Detail::UriRegistry registry;
    Coap::Message message;
    Coap::Callback callback = nullptr;

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, registry.add("a/b", handlerA));
    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, registry.add("c/d", handlerB));
    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, message.addPath("c/d"));

    TEST_ASSERT_EQUAL(Coap::ErrorCode::OK, registry.find(message, callback));
    TEST_ASSERT_TRUE(callback == handlerB);
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(testConstructorSetsDefaultHeader);
    RUN_TEST(testConstructorWithParametersSetsExpectedValues);
    RUN_TEST(testSettersUpdateTypeCodeAndId);
    RUN_TEST(testSetTokenSetsAndOverwritesToken);
    RUN_TEST(testSetTokenRejectsTokensLongerThanSpecLimit);
    RUN_TEST(testSetTokenRejectsMalformedExistingTokenLength);
    RUN_TEST(testMatchesTokenRequiresMatchingContentAndLength);
    RUN_TEST(testAddOptionKeepsOptionsOrderedByNumber);
    RUN_TEST(testAddOptionRejectsDuplicateSingleInstanceOption);
    RUN_TEST(testAddPayloadRoundtripAndRejectsSecondPayload);
    RUN_TEST(testAddPathAndGetPathRoundtrip);
    RUN_TEST(testAddPathRejectsInvalidQuerySeparatorPosition);
    RUN_TEST(testGetQuery);
    RUN_TEST(testGetMaxAgeReturnsDefaultWhenOptionAbsent);
    RUN_TEST(testSetMaxAgeStoresMinimalEncodingAndReadsBack);
    RUN_TEST(testUriRegistryFindMatchesSecondRegisteredPath);

    return UNITY_END();
}

#include "../src/detail/Detail.cpp"
#include "../src/Observers.cpp"
#include "../src/ProsecCoAP.cpp"
