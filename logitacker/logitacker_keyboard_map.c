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

typedef struct {
    const char * p_pos;
} str_to_hid_report_seq_ctx_t;

/* Iterator, parses a rune from null terminated in_str on every call and returns an array of HID keyboard reports
 * which hold the needed key-presses to produce this rune (with respect to language layout
 * - input p_ctx                    : context with iterator data (f.e. current position in string)
 * - input in_str                   : the UTF-8 encoded string to use (support for non ASCII characters like 'Ü' depends on language layout)
 * - output p_out_next_report_seq   : pointer to array of resulting keyboard reports for current rune
 * - output out_next_rep_seq_len    : overall size of resulting hid_keyboard_report_t[]
 * - input in_layout                : keyboard language layout to use (currently LANGUAGE_LAYOUT_US / LANGUAGE_LAYOUT_DE)
 */

// ToDo: append a key release report to every sequence after a rune
uint32_t utf8_str_next_hid_report_seq(str_to_hid_report_seq_ctx_t * p_ctx, char * in_str, hid_keyboard_report_t ** p_out_next_report_seq, uint32_t * out_next_rep_seq_len, keyboard_language_layout_t in_layout) {
    // ToDo: error checks for NULL params

    if (p_ctx->p_pos == NULL) {
        // first run, set to start of string
        p_ctx->p_pos = in_str;
    }

    // if byte at p_pos is 0x00 we reached the end of the zero terminated string during last call and return an error this time
    if (*p_ctx->p_pos == 0x00) {
        // reached end of string
        p_ctx->p_pos = NULL; //reusable
        *p_out_next_report_seq = (void*) HID_REPORT_SEQUENCE_RELEASE;
        *out_next_rep_seq_len = sizeof(HID_REPORT_SEQUENCE_RELEASE);
        return NRF_ERROR_NULL;
    }


    uint32_t c_utf; //stores decoded unicode codepoint (wchar) of next UTF-8 rune

    //read UTF-8 rune and advance p_pos to next one
    p_ctx->p_pos = utf8DecodeRune(p_ctx->p_pos, 0, &c_utf);

    //ToDo: check if c_utf contains an error value

    // map rune to output reports
    if (wchar_to_hid_report_seq(p_out_next_report_seq, out_next_rep_seq_len, in_layout, c_utf) == NRF_SUCCESS) {
        //NRF_LOG_INFO("NO REPORT FOR %lc", c_utf);

        // reports are updated, let's return success
        return NRF_SUCCESS;
    } else {
        // something went wrong, likely we can't translate, we return success anyways, but with a KEY_RELEASE report
        // sequence (this allows going on with the remaining string, in case a mapping for a single rune is missing)
        *p_out_next_report_seq = (void*) HID_REPORT_SEQUENCE_RELEASE;
        *out_next_rep_seq_len = sizeof(HID_REPORT_SEQUENCE_RELEASE);
        return NRF_SUCCESS;

    }

}


char * test_key = "ÜÄüäHello world with abcÜ";

void test_string_to_reports(void) {
    char * teststr = "Hello World ÜÄÖ!";
    str_to_hid_report_seq_ctx_t ctx = {0};;

    hid_keyboard_report_t * rep_seq_result = {0};
    uint32_t rep_seq_result_size = 0;

    int runecount = 0;
    char countstr[16];
    while (utf8_str_next_hid_report_seq(&ctx, teststr, &rep_seq_result, &rep_seq_result_size, LANGUAGE_LAYOUT_DE) == NRF_SUCCESS) {
        sprintf(countstr, "rune %d:", runecount++);
        NRF_LOG_INFO("%s", nrf_log_push(countstr));
        NRF_LOG_HEXDUMP_INFO(rep_seq_result, rep_seq_result_size); //log !raw! resulting report array
    }
}


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

    char buf[256] = {0};

    p_pos = test_key;
    while (*p_pos != 0x00) {
        uint32_t c_utf;
        p_pos = utf8DecodeRune(p_pos, 0, &c_utf);
        sprintf(buf, "c_utf %.8lx, Ü %.8x", c_utf, L'Ü');
        if (wchar_to_hid_report_seq(&rep_seq,&rep_seq_size, LANGUAGE_LAYOUT_DE, c_utf) != NRF_SUCCESS) {
            NRF_LOG_INFO("NO REPORT FOR %lc", c_utf);

        } else {
            NRF_LOG_INFO("REPORT FOR %s..", nrf_log_push(buf));
            NRF_LOG_HEXDUMP_INFO(rep_seq, rep_seq_size);
        }
    }


    //
    test_string_to_reports(); //iterarte over string and produce reports
}
