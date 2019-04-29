#ifndef LOGITACKER_KEYBOARD_MAP_H__
#define LOGITACKER_KEYBOARD_MAP_H__

#include "stdint.h"
#include "nordic_common.h"



typedef uint32_t utf8_char_t;
typedef char HID_key_code_t;
typedef char HID_mod_code_t;

typedef struct  {
    HID_key_code_t keycode;
    char *name;
} hid_key_list_entry_t;

/*
 * https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf
 *
 * see table 12
 */


#define ALL_KEYCODES(PROCESSING_FUNC) \
    PROCESSING_FUNC(NONE,0) \
    PROCESSING_FUNC(ERROR_ROLLOVER,1) \
    PROCESSING_FUNC(POST_FAIL,2) \
    PROCESSING_FUNC(ERROR_UNDEFINED,3) \
    PROCESSING_FUNC(A,4) \
    PROCESSING_FUNC(B,5) \
    PROCESSING_FUNC(C,6) \
    PROCESSING_FUNC(D,7) \
    PROCESSING_FUNC(E,8) \
    PROCESSING_FUNC(F,9) \
    PROCESSING_FUNC(G,10) \
    PROCESSING_FUNC(H,11) \
    PROCESSING_FUNC(I,12) \
    PROCESSING_FUNC(J,13) \
    PROCESSING_FUNC(K,14) \
    PROCESSING_FUNC(L,15) \
    PROCESSING_FUNC(M,16) \
    PROCESSING_FUNC(N,17) \
    PROCESSING_FUNC(O,18) \
    PROCESSING_FUNC(P,19) \
    PROCESSING_FUNC(Q,20) \
    PROCESSING_FUNC(R,21) \
    PROCESSING_FUNC(S,22) \
    PROCESSING_FUNC(T,23) \
    PROCESSING_FUNC(U,24) \
    PROCESSING_FUNC(V,25) \
    PROCESSING_FUNC(W,26) \
    PROCESSING_FUNC(X,27) \
    PROCESSING_FUNC(Y,28) \
    PROCESSING_FUNC(Z,29) \
    PROCESSING_FUNC(1,0x1e) \
    PROCESSING_FUNC(2,0x1f) \
    PROCESSING_FUNC(3,0x20) \
    PROCESSING_FUNC(4,0x21) \
    PROCESSING_FUNC(5,0x22) \
    PROCESSING_FUNC(6,0x23) \
    PROCESSING_FUNC(7,0x24) \
    PROCESSING_FUNC(8,0x25) \
    PROCESSING_FUNC(9,0x26) \
    PROCESSING_FUNC(0,0x27) \
    PROCESSING_FUNC(ENTER,0x28) \
    PROCESSING_FUNC(ESCAPE,0x29) \
    PROCESSING_FUNC(BACKSPACE,0x2a) \
    PROCESSING_FUNC(TAB,0x2b) \
    PROCESSING_FUNC(SPACE,0x2c) \
    PROCESSING_FUNC(MINUS,0x2d) \
    PROCESSING_FUNC(EQUAL,0x2e) \
    PROCESSING_FUNC(LEFTBRACE,0x2f) \
    PROCESSING_FUNC(RIGHTBRACE,0x30) \
    PROCESSING_FUNC(BACKSLASH,0x31) \
    PROCESSING_FUNC(HASHTILDE,0x32) \
    PROCESSING_FUNC(SEMICOLON,0x33) \
    PROCESSING_FUNC(APOSTROPHE,0x34) \
    PROCESSING_FUNC(GRAVE,0x35) \
    PROCESSING_FUNC(COMMA,0x36) \
    PROCESSING_FUNC(DOT,0x37) \
    PROCESSING_FUNC(SLASH,0x38) \
    PROCESSING_FUNC(CAPSLOCK,0x39) \



#define KEYCODE_ASSIGN_ENUM(name, val) CONCAT_2(MYHID_KEY_,name)=val,
#define KEYCODE_ADD_ARRAY(nameval, val) { .keycode=val, .name=STRINGIFY(CONCAT_2(MYHID_KEY_,nameval))},

enum keys {
    ALL_KEYCODES(KEYCODE_ASSIGN_ENUM)
};


static const hid_key_list_entry_t key_list[] = {
    ALL_KEYCODES(KEYCODE_ADD_ARRAY)
};


#define KEYCODE_TO_STR(keycode_val) ({char* retval="UNKNOWN HID KEY"; for(int keypos=0; keypos<sizeof(key_list)/sizeof(key_list[0]);keypos++) { if (key_list[keypos].keycode == keycode_val) { retval=key_list[keypos].name; break; } }; retval;})


#define HID_MOD_KEY_LEFT_CONTROL        0x01
#define HID_MOD_KEY_LEFT_SHIFT          0x02
#define HID_MOD_KEY_LEFT_ALT            0x04
#define HID_MOD_KEY_LEFT_GUI            0x08
#define HID_MOD_KEY_RIGHT_CONTROL       0x10
#define HID_MOD_KEY_RIGHT_SHIFT         0x20
#define HID_MOD_KEY_RIGHT_ALT           0x40
#define HID_MOD_KEY_RIGHT_GUI           0x80

#define HID_KEY_RESERVED                0x00
#define HID_KEY_ERROR_ROLLOVER          0x01
#define HID_KEY_POST_FAIL               0x02
#define HID_KEY_ERROR_UNDEFINED         0x03
#define HID_KEY_A                       0x04
#define HID_KEY_B                       0x05
#define HID_KEY_C                       0x06
#define HID_KEY_D                       0x07 // Keyboard d and D
#define HID_KEY_E                       0x08 // Keyboard e and E
#define HID_KEY_F                       0x09 // Keyboard f and F
#define HID_KEY_G                       0x0a // Keyboard g and G
#define HID_KEY_H                       0x0b // Keyboard h and H
#define HID_KEY_I                       0x0c // Keyboard i and I
#define HID_KEY_J                       0x0d // Keyboard j and J
#define HID_KEY_K                       0x0e // Keyboard k and K
#define HID_KEY_L                       0x0f // Keyboard l and L
#define HID_KEY_M                       0x10 // Keyboard m and M
#define HID_KEY_N                       0x11 // Keyboard n and N
#define HID_KEY_O                       0x12 // Keyboard o and O
#define HID_KEY_P                       0x13 // Keyboard p and P
#define HID_KEY_Q                       0x14 // Keyboard q and Q
#define HID_KEY_R                       0x15 // Keyboard r and R
#define HID_KEY_S                       0x16 // Keyboard s and S
#define HID_KEY_T                       0x17 // Keyboard t and T
#define HID_KEY_U                       0x18 // Keyboard u and U
#define HID_KEY_V                       0x19 // Keyboard v and V
#define HID_KEY_W                       0x1a // Keyboard w and W
#define HID_KEY_X                       0x1b // Keyboard x and X
#define HID_KEY_Y                       0x1c // Keyboard y and Y
#define HID_KEY_Z                       0x1d // Keyboard z and Z

#define HID_KEY_1                       0x1e // Keyboard 1 and !
#define HID_KEY_2                       0x1f // Keyboard 2 and @
#define HID_KEY_3                       0x20 // Keyboard 3 and #
#define HID_KEY_4                       0x21 // Keyboard 4 and $
#define HID_KEY_5                       0x22 // Keyboard 5 and %
#define HID_KEY_6                       0x23 // Keyboard 6 and ^
#define HID_KEY_7                       0x24 // Keyboard 7 and &
#define HID_KEY_8                       0x25 // Keyboard 8 and *
#define HID_KEY_9                       0x26 // Keyboard 9 and (
#define HID_KEY_0                       0x27 // Keyboard 0 and )

#define HID_KEY_ENTER                   0x28 // Keyboard Return (ENTER)
#define HID_KEY_ESC                     0x29 // Keyboard ESCAPE
#define HID_KEY_BACKSPACE               0x2a // Keyboard DELETE (Backspace)
#define HID_KEY_TAB                     0x2b // Keyboard Tab
#define HID_KEY_SPACE                   0x2c // Keyboard Spacebar
#define HID_KEY_MINUS                   0x2d // Keyboard - and _
#define HID_KEY_EQUAL                   0x2e // Keyboard and +
#define HID_KEY_LEFTBRACE               0x2f // Keyboard [ and {
#define HID_KEY_RIGHTBRACE              0x30 // Keyboard ] and }
#define HID_KEY_BACKSLASH               0x31 // Keyboard \ and |
#define HID_KEY_HASHTILDE               0x32 // Keyboard Non-US # and ~
#define HID_KEY_SEMICOLON               0x33 // Keyboard ; and :
#define HID_KEY_APOSTROPHE              0x34 // Keyboard ' and "
#define HID_KEY_GRAVE                   0x35 // Keyboard ` and ~
#define HID_KEY_COMMA                   0x36 // Keyboard , and <
#define HID_KEY_DOT                     0x37 // Keyboard . and >
#define HID_KEY_SLASH                   0x38 // Keyboard / and ?
#define HID_KEY_CAPSLOCK                0x39 // Keyboard Caps Lock

#define HID_KEY_F1                      0x3a // Keyboard F1
#define HID_KEY_F2                      0x3b // Keyboard F2
#define HID_KEY_F3                      0x3c // Keyboard F3
#define HID_KEY_F4                      0x3d // Keyboard F4
#define HID_KEY_F5                      0x3e // Keyboard F5
#define HID_KEY_F6                      0x3f // Keyboard F6
#define HID_KEY_F7                      0x40 // Keyboard F7
#define HID_KEY_F8                      0x41 // Keyboard F8
#define HID_KEY_F9                      0x42 // Keyboard F9
#define HID_KEY_F10                     0x43 // Keyboard F10
#define HID_KEY_F11                     0x44 // Keyboard F11
#define HID_KEY_F12                     0x45 // Keyboard F12

#define HID_KEY_SYSRQ                   0x46 // Keyboard Print Screen
#define HID_KEY_SCROLLLOCK              0x47 // Keyboard Scroll Lock
#define HID_KEY_PAUSE                   0x48 // Keyboard Pause
#define HID_KEY_INSERT                  0x49 // Keyboard Insert
#define HID_KEY_HOME                    0x4a // Keyboard Home
#define HID_KEY_PAGEUP                  0x4b // Keyboard Page Up
#define HID_KEY_DELETE                  0x4c // Keyboard Delete Forward
#define HID_KEY_END                     0x4d // Keyboard End
#define HID_KEY_PAGEDOWN                0x4e // Keyboard Page Down
#define HID_KEY_RIGHT                   0x4f // Keyboard Right Arrow
#define HID_KEY_LEFT                    0x50 // Keyboard Left Arrow
#define HID_KEY_DOWN                    0x51 // Keyboard Down Arrow
#define HID_KEY_UP                      0x52 // Keyboard Up Arrow

#define HID_KEY_NUMLOCK                 0x53 // Keyboard Num Lock and Clear
#define HID_KEY_KPSLASH                 0x54 // Keypad /
#define HID_KEY_KPASTERISK              0x55 // Keypad *
#define HID_KEY_KPMINUS                 0x56 // Keypad -
#define HID_KEY_KPPLUS                  0x57 // Keypad +
#define HID_KEY_KPENTER                 0x58 // Keypad ENTER
#define HID_KEY_KP1                     0x59 // Keypad 1 and End
#define HID_KEY_KP2                     0x5a // Keypad 2 and Down Arrow
#define HID_KEY_KP3                     0x5b // Keypad 3 and PageDn
#define HID_KEY_KP4                     0x5c // Keypad 4 and Left Arrow
#define HID_KEY_KP5                     0x5d // Keypad 5
#define HID_KEY_KP6                     0x5e // Keypad 6 and Right Arrow
#define HID_KEY_KP7                     0x5f // Keypad 7 and Home
#define HID_KEY_KP8                     0x60 // Keypad 8 and Up Arrow
#define HID_KEY_KP9                     0x61 // Keypad 9 and Page Up
#define HID_KEY_KP0                     0x62 // Keypad 0 and Insert
#define HID_KEY_KPDOT                   0x63 // Keypad . and Delete

#define HID_KEY_102ND                   0x64 // Keyboard Non-US \ and |
#define HID_KEY_COMPOSE                 0x65 // Keyboard Application
#define HID_KEY_POWER                   0x66 // Keyboard Power
#define HID_KEY_KPEQUAL                 0x67 // Keypad =

#define HID_KEY_F13                     0x68 // Keyboard F13
#define HID_KEY_F14                     0x69 // Keyboard F14
#define HID_KEY_F15                     0x6a // Keyboard F15
#define HID_KEY_F16                     0x6b // Keyboard F16
#define HID_KEY_F17                     0x6c // Keyboard F17
#define HID_KEY_F18                     0x6d // Keyboard F18
#define HID_KEY_F19                     0x6e // Keyboard F19
#define HID_KEY_F20                     0x6f // Keyboard F20
#define HID_KEY_F21                     0x70 // Keyboard F21
#define HID_KEY_F22                     0x71 // Keyboard F22
#define HID_KEY_F23                     0x72 // Keyboard F23
#define HID_KEY_F24                     0x73 // Keyboard F24

#define HID_KEY_OPEN                    0x74 // Keyboard Execute
#define HID_KEY_HELP                    0x75 // Keyboard Help
#define HID_KEY_PROPS                   0x76 // Keyboard Menu
#define HID_KEY_FRONT                   0x77 // Keyboard Select
#define HID_KEY_STOP                    0x78 // Keyboard Stop
#define HID_KEY_AGAIN                   0x79 // Keyboard Again
#define HID_KEY_UNDO                    0x7a // Keyboard Undo
#define HID_KEY_CUT                     0x7b // Keyboard Cut
#define HID_KEY_COPY                    0x7c // Keyboard Copy
#define HID_KEY_PASTE                   0x7d // Keyboard Paste
#define HID_KEY_FIND                    0x7e // Keyboard Find
#define HID_KEY_MUTE                    0x7f // Keyboard Mute
#define HID_KEY_VOLUMEUP                0x80 // Keyboard Volume Up
#define HID_KEY_VOLUMEDOWN              0x81 // Keyboard Volume Down
//                    0x82  Keyboard Locking Caps Lock
//                    0x83  Keyboard Locking Num Lock
//                    0x84  Keyboard Locking Scroll Lock
#define HID_KEY_KPCOMMA                 0x85 // Keypad Comma
//                    0x86  Keypad Equal Sign (only used on AS400, otherwise 0x67)
#define HID_KEY_RO                      0x87 // Keyboard International1
#define HID_KEY_KATAKANAHIRAGANA        0x88 // Keyboard International2
#define HID_KEY_YEN                     0x89 // Keyboard International3
#define HID_KEY_HENKAN                  0x8a // Keyboard International4
#define HID_KEY_MUHENKAN                0x8b // Keyboard International5
#define HID_KEY_KPJPCOMMA               0x8c // Keyboard International6
//                    0x8d  Keyboard International7
//                    0x8e  Keyboard International8
//                    0x8f  Keyboard International9
#define HID_KEY_HANGEUL                 0x90 // Keyboard LANG1
#define HID_KEY_HANJA                   0x91 // Keyboard LANG2
#define HID_KEY_KATAKANA                0x92 // Keyboard LANG3
#define HID_KEY_HIRAGANA                0x93 // Keyboard LANG4
#define HID_KEY_ZENKAKUHANKAKU          0x94 // Keyboard LANG5
//                    0x95  Keyboard LANG6
//                    0x96  Keyboard LANG7
//                    0x97  Keyboard LANG8
//                    0x98  Keyboard LANG9
//                    0x99  Keyboard Alternate Erase
//                    0x9a  Keyboard SysReq/Attention
//                    0x9b  Keyboard Cancel
//                    0x9c  Keyboard Clear
//                    0x9d  Keyboard Prior
//                    0x9e  Keyboard Return
//                    0x9f  Keyboard Separator
//                    0xa0  Keyboard Out
//                    0xa1  Keyboard Oper
//                    0xa2  Keyboard Clear/Again
//                    0xa3  Keyboard CrSel/Props
//                    0xa4  Keyboard ExSel

//                    0xb0  Keypad 00
//                    0xb1  Keypad 000
//                    0xb2  Thousands Separator
//                    0xb3  Decimal Separator
//                    0xb4  Currency Unit
//                    0xb5  Currency Sub-unit
#define HID_KEY_KPLEFTPARENTHESE        0xb6 // Keypad (
#define HID_KEY_KPRIGHTPARENTHESE       0xb7 // Keypad )
//                    0xb8  Keypad {
//                    0xb9  Keypad }
//                    0xba  Keypad Tab
//                    0xbb  Keypad Backspace
//                    0xbc  Keypad A
//                    0xbd  Keypad B
//                    0xbe  Keypad C
//                    0xbf  Keypad D
//                    0xc0  Keypad E
//                    0xc1  Keypad F
//                    0xc2  Keypad XOR
//                    0xc3  Keypad ^
//                    0xc4  Keypad %
//                    0xc5  Keypad <
//                    0xc6  Keypad >
//                    0xc7  Keypad &
//                    0xc8  Keypad &&
//                    0xc9  Keypad |
//                    0xca  Keypad ||
//                    0xcb  Keypad :
//                    0xcc  Keypad #
//                    0xcd  Keypad Space
//                    0xce  Keypad @
//                    0xcf  Keypad !
//                    0xd0  Keypad Memory Store
//                    0xd1  Keypad Memory Recall
//                    0xd2  Keypad Memory Clear
//                    0xd3  Keypad Memory Add
//                    0xd4  Keypad Memory Subtract
//                    0xd5  Keypad Memory Multiply
//                    0xd6  Keypad Memory Divide
//                    0xd7  Keypad +/-
//                    0xd8  Keypad Clear
//                    0xd9  Keypad Clear Entry
//                    0xda  Keypad Binary
//                    0xdb  Keypad Octal
//                    0xdc  Keypad Decimal
//                    0xdd  Keypad Hexadecimal

#define HID_KEY_LEFTCTRL                0xe0 // Keyboard Left Control
#define HID_KEY_LEFTSHIFT               0xe1 // Keyboard Left Shift
#define HID_KEY_LEFTALT                 0xe2 // Keyboard Left Alt
#define HID_KEY_LEFTMETA                0xe3 // Keyboard Left GUI
#define HID_KEY_RIGHTCTRL               0xe4 // Keyboard Right Control
#define HID_KEY_RIGHTSHIFT              0xe5 // Keyboard Right Shift
#define HID_KEY_RIGHTALT                0xe6 // Keyboard Right Alt
#define HID_KEY_RIGHTMETA               0xe7 // Keyboard Right GUI



void logitacker_keyboard_map_test(void);

#endif //HELPER_MAP_H__
