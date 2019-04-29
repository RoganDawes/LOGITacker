#include "logitacker_keyboard_map.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_KEYBOARD_MAP
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();


#define isutf(c) (((c)&0xC0)!=0x80)



int u8_strlen(char *s);
uint32_t u8_nextchar(char *s, int *i);



int u8_strlen(char *s)
{
    int count = 0;
    int i = 0;

    while (u8_nextchar(s, &i) != 0)
        count++;

    return count;
}


/* reads the next utf-8 sequence out of a string, updating an index */
uint32_t u8_nextchar(char *s, int *i)
{
    uint32_t ch = 0;
    int sz = 0;

    do {
        ch <<= 8;
        ch += (unsigned char)s[(*i)++];
        sz++;
    } while (s[*i] && !isutf(s[*i]));

    return ch;
}
char * test_key = "ÜHello world with \xF0\x9D\x84\x9E abcÜ\x00";


void logitacker_keyboard_map_test(void) {
    NRF_LOG_INFO("helper map");
    NRF_LOG_INFO("Testkey (len %d, wlen %d): %s", strlen(test_key), test_key);
    NRF_LOG_HEXDUMP_INFO(test_key, strlen(test_key));

    int len = strlen(test_key);
    int pos = 0;


    while (pos < len ) {
        uint32_t c_utf = u8_nextchar(test_key, &pos);
        NRF_LOG_INFO("utf8: %.8X %c", c_utf, c_utf);
    }

    NRF_LOG_INFO("MYHID_KEY_A: %s", KEYCODE_TO_STR(MYHID_KEY_A));
    NRF_LOG_INFO("MYHID_KEY_0: %s", KEYCODE_TO_STR(MYHID_KEY_0));
    NRF_LOG_INFO("MYHID_KEY_1: %s", KEYCODE_TO_STR(MYHID_KEY_1));
    NRF_LOG_INFO("MYHID_KEY_Z: %s", KEYCODE_TO_STR(MYHID_KEY_Z));
    NRF_LOG_INFO("Key 5: %s", KEYCODE_TO_STR(5));
    NRF_LOG_INFO("Key 6: %s", KEYCODE_TO_STR(6));
    NRF_LOG_INFO("Key 7: %s", KEYCODE_TO_STR(7));
}
