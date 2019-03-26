#ifndef HID_H__
#define HID_H__


// custom commands
#define HID_COMMAND_SET_RF_MODE 1
#define HID_COMMAND_SET_CHANNEL 3
#define HID_COMMAND_GET_CHANNEL 4
#define HID_COMMAND_SET_ADDRESS 5
#define HID_COMMAND_GET_ADDRESS 6

#define HID_GENERIC_INTERFACE  0                        // HID generic class interface number.
#define HID_GENERIC_EPIN       NRF_DRV_USBD_EPIN1       // HID generic class endpoint number.
#define REPORT_IN_QUEUE_SIZE    1                       // Number of reports defined in report descriptor.

/**
 * @brief Size of maximum output report. HID generic class will reserve
 *        this buffer size + 1 memory space. 
 *
 * Maximum value of this define is 63 bytes. Library automatically adds
 * one byte for report ID. This means that output report size is limited
 * to 64 bytes.
 */
#define REPORT_OUT_MAXSIZE  64
#define REPORT_IN_MAXSIZE REPORT_OUT_MAXSIZE
#define HID_GENERIC_EP_COUNT  1                         // HID generic class endpoints count.
// List of HID generic class endpoints.
#define ENDPOINT_LIST()                                      \
(                                                            \
        HID_GENERIC_EPIN                                     \
)

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


// User event handler.
static void usbd_hid_event_handler(app_usbd_class_inst_t const * p_inst, app_usbd_hid_user_event_t event);


#endif