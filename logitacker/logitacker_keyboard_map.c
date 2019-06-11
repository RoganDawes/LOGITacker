#include "helper.h"
#include "nrf_log_ctrl.h"
#include "stdlib.h"
#include "logitacker_keyboard_map.h"
#include "utf.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_KEYBOARD_MAP
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();


/* maps the given HID keycode to a string representation */
void modcode_to_str(char * p_result, const HID_mod_code_t modcode) {
    ASSERT(p_result);

    p_result[0] = 0;

    if (modcode == 0x00) {
        strcat(p_result, "NONE");
        return;
    }

    uint8_t token_count = 0;

    if ((modcode & HID_MOD_KEY_LEFT_CONTROL) > 0) {
        if (token_count == 0) strcat(p_result, "(LEFT_CONTROL");
        token_count++;
    }

    if ((modcode & HID_MOD_KEY_LEFT_SHIFT) > 0) {
        if (token_count == 0) {
            strcat(p_result, "(LEFT_SHIFT");
        } else {
            strcat(p_result, " | LEFT_SHIFT");
        }
        token_count++;
    }

    if ((modcode & HID_MOD_KEY_LEFT_ALT) > 0) {
        if (token_count == 0) {
            strcat(p_result, "(LEFT_ALT");
        } else {
            NRF_LOG_INFO("LEFT_ALT appended");
            strcat(p_result, " | LEFT_ALT");
        }
        token_count++;
    }

    if ((modcode & HID_MOD_KEY_LEFT_GUI) > 0) {
        if (token_count == 0) {
            strcat(p_result, "(LEFT_GUI");
        } else {
            strcat(p_result, " | LEFT_GUI");
        }
        token_count++;
    }

    if ((modcode & HID_MOD_KEY_RIGHT_CONTROL) > 0) {
        if (token_count == 0) {
            strcat(p_result, "(RIGHT_CONTROL");
        } else {
            strcat(p_result, " | RIGHT_CONTROL");
        }
        token_count++;
    }

    if ((modcode & HID_MOD_KEY_RIGHT_SHIFT) > 0) {
        if (token_count == 0) {
            strcat(p_result, "(RIGHT_SHIFT");
        } else {
            strcat(p_result, " | RIGHT_SHIFT");
        }
        token_count++;
    }

    if ((modcode & HID_MOD_KEY_RIGHT_ALT) > 0) {
        if (token_count == 0) {
            strcat(p_result, "(RIGHT_ALT");
        } else {
            strcat(p_result, " | RIGHT_ALT");
        }
        token_count++;
    }

    if ((modcode & HID_MOD_KEY_RIGHT_GUI) > 0) {
        if (token_count == 0) {
            strcat(p_result, "(RIGHT_GUI");
        } else {
            strcat(p_result, " | RIGHT_GUI");
        }
    }

    strcat(p_result, ")");
}


/* maps the given HID keycode to a string representation */
char* keycode_to_str(logitacker_keyboard_map_hid_keys_t keycode) {
    switch (keycode) {
        ALL_KEYCODES(KEYCODE_SWITCH_CASE)
        default:
            return "UNKNOWN HID KEY";
    }
}
/* maps the given HID keycode string to hid keycode */
#define KEYCODE_IF_STRCMP(nameval, val) if (strcmp(STRINGIFY(nameval), key_str) == 0) return val;
logitacker_keyboard_map_hid_keys_t str_to_keycode(char * key_str) {

    ALIAS_KEYCODES(KEYCODE_IF_STRCMP)
    ALL_KEYCODES(KEYCODE_IF_STRCMP)

    NRF_LOG_INFO("No mapping for %s", nrf_log_push(key_str));
    return 0x00; // NONE
}

uint32_t logitacker_keyboard_map_combo_str_to_hid_report(char const *in_str,
                                                       hid_keyboard_report_t *p_out_report,
                                                       logitacker_keyboard_map_lang_t in_layout) {
    VERIFY_TRUE(in_str != NULL, NRF_ERROR_NULL);
    VERIFY_TRUE(p_out_report != NULL, NRF_ERROR_NULL);

    //clear output report
    memset(p_out_report, 0x00, sizeof(hid_keyboard_report_t));

    //hid_keyboard_report_t tmp_report = {0};
    char tmp[256];
    char * str_copy = tmp;

    uint8_t resultKeyCount = 0; // keeps track of number of keys added to the report (max are 6)

    strncpy(str_copy, in_str, sizeof(tmp)-1);

    // tokenize str
    char * token;
    int i = 0;
    while (resultKeyCount < 6 && (token = helper_strsep(&str_copy, " ")) != NULL) {
        NRF_LOG_INFO("Token %d: %s", i++, nrf_log_push(token));

        // if the token has length 1, it is likely an ASCII char and we want to stay language agnostic (f.e. a 'Y' for DE layout should result in HID_KEY_Z)
        if (strlen(token) == 1) {
            // if [A-Z] turn to lower
            if (token[0] >= 'A' && token[0] <= 'Z') token[0] += 0x20; // turn lower

            //convert char to wchar
            char tokenStr[2] = {token[0], 0x00};
            uint32_t c_utf; //stores decoded unicode codepoint (wchar) of next UTF-8 rune
            utf8DecodeRune(tokenStr, 0, &c_utf);

            // retrieve HID reports for current wchar (language agnostic)
            hid_keyboard_report_t *p_hid_report_sequence = NULL;
            uint32_t hid_report_sequence_len = 0;
            uint32_t err = logitacker_keyboard_map_wc_to_hid_reports(&p_hid_report_sequence, &hid_report_sequence_len, in_layout, c_utf);

            if (err != NRF_SUCCESS || hid_report_sequence_len == 0) continue; // skip this token if it doesn't result in a report sequence

            // we only regard the first report of the sequence in our combo (no dead key support on this path, multiple calls to press have to be used to emulate dead keys)
            p_out_report->mod |= p_hid_report_sequence[0].mod; //copy modifier
            for (uint8_t keypos=0; keypos < 6 && resultKeyCount < 6; keypos++) {
                uint8_t keyCode = p_hid_report_sequence[0].keys[keypos];
                if (keyCode != 0x00) {
                    // no empty key, add to resulting report
                    p_out_report->keys[resultKeyCount++] = keyCode;
                }
            }
            continue; // go on with next token
        } // end of handling of single char tokens

        // try to map token directly to HID keycode
        uint8_t keyCode = str_to_keycode(token);
        if (keyCode != 0x00) {
            // if we have a modifier key (0xe0..0xe7) handle it like this, otherwise handle it as hid_key
            if (keyCode > 0xdf && keyCode < 0xe8) {
                // translate keyCode corresponding to modifier key to respective mask bit in modifier byte of output report
                switch (keyCode) {
                    case HID_KEY_RIGHTALT:
                        p_out_report->mod |= HID_MOD_KEY_RIGHT_ALT;
                        break;
                    case HID_KEY_RIGHTCTRL:
                        p_out_report->mod |= HID_MOD_KEY_RIGHT_CONTROL;
                        break;
                    case HID_KEY_RIGHTSHIFT:
                        p_out_report->mod |= HID_MOD_KEY_RIGHT_SHIFT;
                        break;
                    case HID_KEY_RIGHTMETA:
                        p_out_report->mod |= HID_MOD_KEY_RIGHT_GUI;
                        break;
                    case HID_KEY_LEFTALT:
                        p_out_report->mod |= HID_MOD_KEY_LEFT_ALT;
                        break;
                    case HID_KEY_LEFTCTRL:
                        p_out_report->mod |= HID_MOD_KEY_LEFT_CONTROL;
                        break;
                    case HID_KEY_LEFTSHIFT:
                        p_out_report->mod |= HID_MOD_KEY_LEFT_SHIFT;
                        break;
                    case HID_KEY_LEFTMETA:
                        p_out_report->mod |= HID_MOD_KEY_LEFT_GUI;
                        break;
                    default:
                        break;
                }

            } else {
                // no empty key, add to resulting report
                p_out_report->keys[resultKeyCount++] = keyCode;
            }
        }

    }

    return NRF_SUCCESS;
}

#define LAYOUT_SWITCH_CASE(nameval, val) case nameval: {*p_out_report_seq=(void*)val; *out_rep_seq_len=sizeof(val) ;return NRF_SUCCESS; }

/* maps the given wchar to respective HID report sequence (currently only US,DE layout) */
uint32_t logitacker_keyboard_map_wc_to_hid_reports(hid_keyboard_report_t **p_out_report_seq, uint32_t *out_rep_seq_len,
                                                   logitacker_keyboard_map_lang_t in_layout, wchar_t in_rune) {

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


/* Iterator, parses a rune from null terminated in_str on every call and returns an array of HID keyboard reports
 * which hold the needed key-presses to produce this rune (with respect to language layout
 * - input p_ctx                    : context with iterator data (f.e. current position in string)
 * - input in_str                   : the UTF-8 encoded string to use (support for non ASCII characters like 'Ü' depends on language layout)
 * - output p_out_next_report_seq   : pointer to array of resulting keyboard reports for current rune
 * - output out_next_rep_seq_len    : overall size of resulting hid_keyboard_report_t[]
 * - input in_layout                : keyboard language layout to use (currently LANGUAGE_LAYOUT_US / LANGUAGE_LAYOUT_DE)
 */

// ToDo: append a key release report to every sequence after a rune
uint32_t logitacker_keyboard_map_u8_str_to_hid_reports(logitacker_keyboard_map_u8_str_parser_ctx_t *p_ctx, char const *in_str,
                                                       hid_keyboard_report_t **p_out_next_report_seq,
                                                       uint32_t *out_next_rep_seq_len,
                                                       logitacker_keyboard_map_lang_t in_layout) {
    // ToDo: error checks for NULL params

    if (p_ctx->p_pos == NULL) {
        // first run, set to start of string
        p_ctx->p_pos = in_str;
        p_ctx->append_release = false;
    }

    // check if we need to add a key release frame for this iteration
    if (p_ctx->append_release) {
        *p_out_next_report_seq = (void*) HID_REPORT_SEQUENCE_RELEASE;
        *out_next_rep_seq_len = sizeof(HID_REPORT_SEQUENCE_RELEASE);
        p_ctx->append_release = false;
        return NRF_SUCCESS;

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
    if (logitacker_keyboard_map_wc_to_hid_reports(p_out_next_report_seq, out_next_rep_seq_len, in_layout, c_utf) == NRF_SUCCESS) {
        p_ctx->append_release=true;
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

/*

char * test_key = "ÜÄüäHello world with abcÜ";

void test_string_to_reports(void) {
    char * teststr = "Hello World ÜÄÖ!";
    logitacker_keyboard_map_u8_str_parser_ctx_t ctx = {0};

    hid_keyboard_report_t * rep_seq_result = {0};
    uint32_t rep_seq_result_size = 0;

    int runecount = 0;
    char countstr[16];
    while (logitacker_keyboard_map_u8_str_to_hid_reports(&ctx, teststr, &rep_seq_result, &rep_seq_result_size,
                                                         LANGUAGE_LAYOUT_DE) == NRF_SUCCESS) {
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
    if (logitacker_keyboard_map_wc_to_hid_reports(&rep_seq, &rep_seq_size, LANGUAGE_LAYOUT_US, L'A') != NRF_SUCCESS) {
        NRF_LOG_INFO("NO REPORT FOR 'A'");
    } else {
        NRF_LOG_INFO("REPORT FOR 'A'..");
        NRF_LOG_HEXDUMP_INFO(rep_seq, rep_seq_size);
    }

    if (logitacker_keyboard_map_wc_to_hid_reports(&rep_seq, &rep_seq_size, LANGUAGE_LAYOUT_US, L'\n') != NRF_SUCCESS) {
        NRF_LOG_INFO("NO REPORT FOR 'Ü'");
    } else {
        NRF_LOG_INFO("REPORT FOR '\n'..");
        NRF_LOG_HEXDUMP_INFO(rep_seq, rep_seq_size);
    }

    NRF_LOG_INFO("TEST ALL")
    for (wchar_t c=0; c<0x7f; c++) {
        if (logitacker_keyboard_map_wc_to_hid_reports(&rep_seq, &rep_seq_size, LANGUAGE_LAYOUT_DE, c) != NRF_SUCCESS) {
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
        if (logitacker_keyboard_map_wc_to_hid_reports(&rep_seq, &rep_seq_size, LANGUAGE_LAYOUT_DE, c_utf) != NRF_SUCCESS) {
            NRF_LOG_INFO("NO REPORT FOR %lc", c_utf);

        } else {
            NRF_LOG_INFO("REPORT FOR %s..", nrf_log_push(buf));
            NRF_LOG_HEXDUMP_INFO(rep_seq, rep_seq_size);
        }
    }


    //
    test_string_to_reports(); //iterarte over string and produce reports
}
*/


logitacker_keyboard_map_lang_t logitacker_keyboard_map_lang_from_str(char * lang_str) {
    if (lang_str == NULL) goto lab_default;

    if (strcmp(lang_str, "de") == 0 || strcmp(lang_str, "DE") == 0 ) return LANGUAGE_LAYOUT_DE;
    if (strcmp(lang_str, "us") == 0 || strcmp(lang_str, "US") == 0 ) return LANGUAGE_LAYOUT_US;

    lab_default:
    NRF_LOG_WARNING("unknown language layout '%s' ... using 'us' as default", nrf_log_push(lang_str));
    return LANGUAGE_LAYOUT_US; // default
}