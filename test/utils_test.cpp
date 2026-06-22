#include "unity/src/unity.h"
#include "../src/Utils.h"

void setUp(void)
{
    // set stuff up here
}

void tearDown(void)
{
    // clean stuff up here
}

void test_read_uint16(void)
{
    const uint16_t expected = 0xABCD;
    uint8_t stream[2];
    Coap::Utils::toNetworkByteOrder(expected, stream);
    uint16_t result = Coap::Utils::read_uint16(stream);
    TEST_ASSERT_EQUAL(expected, result);
}

void test_read_uint32(void)
{
    const uint32_t expected = 0xABCDEF12;
    uint8_t stream[4];
    Coap::Utils::toNetworkByteOrder(expected, stream);
    uint32_t result = Coap::Utils::read_uint32(stream);
    TEST_ASSERT_EQUAL(expected, result);
}

void test_read_uint64(void)
{
    const uint64_t expected = 0x0123456701234567;
    uint8_t stream[8];
    Coap::Utils::toNetworkByteOrder(expected, stream);
    uint64_t result = Coap::Utils::read_uint64(stream);
    TEST_ASSERT_EQUAL(expected, result);
}

void test_read_float(void)
{
    const float expected = 42.2256;
    uint8_t stream[4];
    Coap::Utils::toNetworkByteOrder(expected, stream);
    float result = Coap::Utils::read_float(stream);
    TEST_ASSERT_EQUAL(expected, result);
}

void test_read_double(void)
{
    const double expected = 42.2256;
    uint8_t stream[8];
    Coap::Utils::toNetworkByteOrder(expected, stream);
    float result = Coap::Utils::read_double(stream);
    TEST_ASSERT_EQUAL(expected, result);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_read_uint16);
    RUN_TEST(test_read_uint32);
    RUN_TEST(test_read_uint64);
    RUN_TEST(test_read_float);
    return UNITY_END();
}