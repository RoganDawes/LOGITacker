#include <libraries/log/nrf_log_ctrl.h>
#include "stdlib.h"
#include "logitacker_keyboard_map.h"
#include "utf.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_KEYBOARD_MAP
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();


/* maps the given HID keycode to a string representation */
char* keycode_to_str(enum keys keycode) {
    switch (keycode) {
        ALL_KEYCODES(KEYCODE_SWITCH_CASE)
        default:
            return "UNKNOWN HID KEY";
    }
}

#define LAYOUT_SWITCH_CASE(nameval, val) case nameval: {*p_out_report_seq=(void*)val; *out_rep_seq_len=sizeof(val) ;return NRF_SUCCESS; }

/* maps the given wchar to respective HID report sequence (currently only US,DE layout) */
uint32_t wchar_to_hid_report_seq(hid_keyboard_report_t ** p_out_report_seq, uint32_t * out_rep_seq_len, keyboard_language_layout_t in_layout, wchar_t in_rune) {

    if (in_layout == LANGUAGE_LAYOUT_US) {
        switch (in_rune) {
            LAYOUT_US(LAYOUT_SWITCH_CASE)
            default:
                return NRF_ERROR_INVALID_PARAM;
        }

    } else if (in_layout == LANGUAGE_LAYOUT_DE) {
        switch (in_rune) {
            LAYOUT_DE(LAYOUT_SWITCH_CASE)
            default:
                return NRF_ERROR_INVALID_PARAM;
        }
    } else {
        return NRF_ERROR_INVALID_PARAM;
    }

    return NRF_SUCCESS;
}


char * test_key = "ÜÄiìéèHello world with abcÜ";



void logitacker_keyboard_map_test(void) {
    NRF_LOG_HEXDUMP_INFO(test_key, strlen(test_key));
    NRF_LOG_INFO("Testkey (len %d): %s", strlen(test_key), test_key);
    NRF_LOG_INFO(test_key);



    static wchar_t CP_UE = L'Ü';

    const char * p_pos = test_key;
    while (*p_pos != 0x00) {
        uint32_t c_utf;
        p_pos = utf8DecodeRune(p_pos, 0, &c_utf);
        char mb[4] = { 0 };
        utf8EncodeRune(c_utf, mb);
        NRF_LOG_INFO("utf8 (unicode %.8x): %s", c_utf, nrf_log_push(mb));

    }
    NRF_LOG_INFO("MYHID_KEY_A: %s", keycode_to_str(HID_KEY_A));
    NRF_LOG_INFO("MYHID_KEY_0: %s", keycode_to_str(HID_KEY_0));
    NRF_LOG_INFO("MYHID_KEY_1: %s", keycode_to_str(HID_KEY_1));
    NRF_LOG_INFO("MYHID_KEY_Z: %s", keycode_to_str(HID_KEY_Z));
    NRF_LOG_INFO("Key 5: %s", keycode_to_str(5));
    NRF_LOG_INFO("Key 6: %s", keycode_to_str(6));
    NRF_LOG_INFO("Key 7: %s", keycode_to_str(7));
    NRF_LOG_INFO("Ü: %.8x", CP_UE);

    hid_keyboard_report_t * rep_seq = {0};
    uint32_t rep_seq_size = 0;
    if (wchar_to_hid_report_seq(&rep_seq,&rep_seq_size,LANGUAGE_LAYOUT_US,L'A') != NRF_SUCCESS) {
        NRF_LOG_INFO("NO REPORT FOR 'A'");
    } else {
        NRF_LOG_INFO("REPORT FOR 'A'..");
        NRF_LOG_HEXDUMP_INFO(rep_seq, rep_seq_size);
    }

    if (wchar_to_hid_report_seq(&rep_seq,&rep_seq_size, LANGUAGE_LAYOUT_US, L'\n') != NRF_SUCCESS) {
        NRF_LOG_INFO("NO REPORT FOR 'Ü'");
    } else {
        NRF_LOG_INFO("REPORT FOR '\n'..");
        NRF_LOG_HEXDUMP_INFO(rep_seq, rep_seq_size);
    }

    NRF_LOG_INFO("TEST ALL")
    for (wchar_t c=0; c<0x7f; c++) {
        if (wchar_to_hid_report_seq(&rep_seq,&rep_seq_size, LANGUAGE_LAYOUT_DE, c) != NRF_SUCCESS) {
            NRF_LOG_INFO("NO REPORT FOR %lc", c);
        } else {
            NRF_LOG_INFO("REPORT FOR %lc..", c);
            NRF_LOG_HEXDUMP_INFO(rep_seq, rep_seq_size);
        }

    }


    /*
    NRF_LOG_INFO("rep ABC mod %.x keys (count %x)", REPORT_UPPER_ABC.mod, (uint32_t*) REPORT_UPPER_ABC.keys);
    NRF_LOG_HEXDUMP_INFO(&REPORT_UPPER_ABC, sizeof(REPORT_UPPER_ABC));
    */
    /*
    HID_RUNE_DE_00000050 = HID_REPORT_SEQUENCE_US_LOWER_Y
    HID_RUNE_DE_000000DC = HID_REPORT_SEQUENCE_US_LOWER_Y
    DEF_RUNE(L'z', DE, HID_REPORT_SEQUENCE_US_LOWER_Y);
    DEF_RUNE(L'Ü', DE, HID_REPORT_SEQUENCE_US_LOWER_Y);
    */
}
