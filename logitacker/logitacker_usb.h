#ifndef LOGITACKER_USB_H
#define LOGITACKER_USB_H

#include "app_usbd_hid_generic.h"
#include "app_usbd.h"
#include "app_usbd_hid.h"


/**
 * @brief Enable USB power detection
 */
#ifndef USBD_POWER_DETECTION

#ifdef NRF52840_MDK_DONGLE
#define USBD_POWER_DETECTION false
#elif NRF52840_MDK
#define USBD_POWER_DETECTION true
#elif BOARD_PCA10059
#define USBD_POWER_DETECTION false
#else
#define USBD_POWER_DETECTION false
#endif

#endif //POWER_DETECTION

/*

// custom commands
#define HID_COMMAND_SET_RF_MODE 1
#define HID_COMMAND_SET_CHANNEL 3
#define HID_COMMAND_GET_CHANNEL 4
#define HID_COMMAND_SET_ADDRESS 5
#define HID_COMMAND_GET_ADDRESS 6
*/

// Note: configured using sdk_config for cli_cdc_acm
//   #define NRF_CLI_CDC_ACM_COMM_INTERFACE 0
//   #define NRF_CLI_CDC_ACM_COMM_EPIN NRF_DRV_USBD_EPIN2
//   #define NRF_CLI_CDC_ACM_DATA_INTERFACE 1
//   #define NRF_CLI_CDC_ACM_DATA_EPIN NRF_DRV_USBD_EPIN1
//   #define NRF_CLI_CDC_ACM_DATA_EPOUT NRF_DRV_USBD_EPOUT1


#define LOGITACKER_USB_HID_GENERIC_INTERFACE  2                        // HID generic class interface number.
#define LOGITACKER_USB_HID_GENERIC_EPIN       NRF_DRV_USBD_EPIN3       // HID generic class endpoint number.
#define LOGITACKER_USB_HID_GENERIC_OUT_REPORT_MAXSIZE  64
#define LOGITACKER_USB_HID_GENERIC_REPORT_IN_QUEUE_SIZE    1                       // Number of reports defined in report descriptor.
#define LOGITACKER_USB_HID_GENERIC_IN_REPORT_MAXSIZE LOGITACKER_USB_HID_GENERIC_OUT_REPORT_MAXSIZE
#define LOGITACKER_USB_HID_GENERIC_INTERFACE_ENDPOINT_LIST()         \
(                                                            \
        LOGITACKER_USB_HID_GENERIC_EPIN                                     \
)


#define LOGITACKER_USB_HID_KEYBOARD_INTERFACE 3
#define LOGITACKER_USB_HID_KEYBOARD_EPIN       NRF_DRV_USBD_EPIN4
#define LOGITACKER_USB_HID_KEYBOARD_OUT_REPORT_MAXSIZE  1
#define LOGITACKER_USB_HID_KEYBOARD_IN_REPORT_MAXSIZE  8
#define LOGITACKER_USB_HID_KEYBOARD_REPORT_IN_QUEUE_SIZE    1
#define LOGITACKER_USB_HID_KEYBOARD_INTERFACE_ENDPOINT_LIST()       \
(                                                                   \
        LOGITACKER_USB_HID_KEYBOARD_EPIN                            \
)

#define LOGITACKER_USB_HID_MOUSE_INTERFACE 4
#define LOGITACKER_USB_HID_MOUSE_EPIN       NRF_DRV_USBD_EPIN5
#define LOGITACKER_USB_HID_MOUSE_BUTTON_COUNT 2

/**
 * @brief Size of maximum output report. HID generic class will reserve
 *        this buffer size + 1 memory space.
 *
 * Maximum value of this define is 63 bytes. Library automatically adds
 * one byte for report ID. This means that output report size is limited
 * to 64 bytes.
 */
#define LOGITACKER_USB_HID_INTERFACE_ENDPOINT_COUNT  1                         // HID generic class endpoints count.

// creates HID report descriptor with vendor defined input and output report of 'sizebytes' length (no report ID)
#define APP_USBD_HID_RAW_REPORT_DSC_SIZE(sizebytes) {                \
0x06, 0x00, 0xFF,  /* Usage Page (Vendor Defined 0xFF00) */    \
0x09, 0x01,        /* Usage (0x01) */    \
0xA1, 0x01,        /* Collection (Application) */   \
0x09, 0x01,        /* Usage (0x01) */   \
0x15, 0x00,        /* Logical Minimum (0) */   \
0x26, 0xFF, 0x00,  /* Logical Maximum (255) */   \
0x75, 0x08,        /* Report Size (8)  */  \
0x95, sizebytes,   /* Report Count (64) */   \
0x81, 0x02,        /* Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position) */   \
0x09, 0x02,        /* Usage (0x02) */   \
0x15, 0x00,        /* Logical Minimum (0) */   \
0x26, 0xFF, 0x00,  /* Logical Maximum (255) */   \
0x75, 0x08,        /* Report Size (8) */   \
0x95, sizebytes,   /* Report Count (64) */   \
0x91, 0x02,        /* Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)  */  \
0xC0,              /* End Collection */   \
}

const app_usbd_hid_generic_t m_app_hid_generic;


// User event handler.
//static void usbd_hid_generic_event_handler(app_usbd_class_inst_t const * p_inst, app_usbd_hid_user_event_t event);


uint32_t logitacker_usb_init();
uint32_t logitacker_usb_write_generic_input_report(const void * p_buf, size_t size);
uint32_t logitacker_usb_write_keyboard_input_report(const void * p_buf);
uint32_t logitacker_usb_write_mouse_input_report(const void * p_buf);


#endif //LOGITACKER_USB_H
