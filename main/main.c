/*!
 * \file main.c
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "timers.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
#define USBD_STACK_SIZE         (3*configMINIMAL_STACK_SIZE)
#define CLASS_STACK_SIZE        (2*configMINIMAL_STACK_SIZE)

/*
 * Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
    BLINK_NOT_MOUNTED = 250,
    BLINK_MOUNTED     = 500,
    BLINK_SUSPENDED   = 750,

    BLINK_ALWAYS_ON   = UINT32_MAX,
    BLINK_ALWAYS_OFF  = 0
};


#define URL  "example.tinyusb.org/webusb-serial/index.html"

const tusb_desc_webusb_url_t desc_url =
{
  .bLength         = 3 + sizeof(URL) - 1,
  .bDescriptorType = 3, // WEBUSB URL type
  .bScheme         = 1, // 0: http, 1: https
  .url             = URL
};

static bool web_serial_connected = false;

#if configSUPPORT_STATIC_ALLOCATION
StaticTimer_t blinky_tmdef;
StackType_t  usb_device_stack[USBD_STACK_SIZE];
StaticTask_t usb_device_taskdef;
StackType_t  usb_class_stack[CLASS_STACK_SIZE];
StaticTask_t usb_class_taskdef;
#endif

TimerHandle_t blinky_tm;

//------------- prototypes -------------//
static void led_blinky_cb(TimerHandle_t xTimer);
static void usb_device_task(void* param);
void class_task(void* params);

/*------------- MAIN -------------*/
int main(void)
{
    board_init();

#if configSUPPORT_STATIC_ALLOCATION
    blinky_tm = xTimerCreateStatic(NULL, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), true, NULL, led_blinky_cb, &blinky_tmdef);
    /*
     * Create a task for tinyusb device stack
     */
    xTaskCreateStatic(usb_device_task, "usb-device", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES-1, usb_device_stack, &usb_device_taskdef);
    /*
     * Create Class task
     */
    xTaskCreateStatic(class_task, "usb-class", CLASS_STACK_SIZE, NULL, configMAX_PRIORITIES-2, usb_class_stack, &usb_class_taskdef);

#endif
    xTimerStart(blinky_tm, 0);

    vTaskStartScheduler();
}


// USB Device Driver task
// This top level thread process all usb events and invoke callbacks
static void usb_device_task(void* param)
{
    (void) param;

    // init device stack on configured roothub port
    // This should be called after scheduler/kernel is started.
    // Otherwise it could cause kernel issue since USB IRQ handler does use RTOS queue API.
    tud_init(BOARD_TUD_RHPORT);

    if (board_init_after_tusb) {
        board_init_after_tusb();
    }

    // RTOS forever loop
    while (1) {
        // put this thread to waiting state until there is new events
        tud_task();

        // following code only run if tud_task() process at least 1 event
        tud_cdc_write_flush();
    }
}


// send characters to both CDC and WebUSB
void echo_all(uint8_t buf[], uint32_t count)
{
    // echo to web serial
    if ( web_serial_connected ) {
        tud_vendor_write(buf, count);
        tud_vendor_write_flush();
    }

    // echo to cdc
    if ( tud_cdc_connected() ) {
        for(uint32_t i=0; i<count; i++) {
            tud_cdc_write_char(buf[i]);

            if ( buf[i] == '\r' ) {
                tud_cdc_write_char('\n');
            }
        }
        tud_cdc_write_flush();
    }
}


//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
}


// Invoked when device is unmounted
void tud_umount_cb(void)
{
    xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), 0);
}


// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_SUSPENDED), 0);
}


// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    if (tud_mounted()) {
        xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
    } else {
        xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), 0);
    }
}


//--------------------------------------------------------------------+
// WebUSB use vendor class
//--------------------------------------------------------------------+
// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
    // nothing to with DATA & ACK stage
    if (stage != CONTROL_STAGE_SETUP) return true;

    switch (request->bmRequestType_bit.type) {
        case TUSB_REQ_TYPE_VENDOR:
            switch (request->bRequest) {
                case VENDOR_REQUEST_WEBUSB:
                    // match vendor request in BOS descriptor
                    // Get landing page url
                    return tud_control_xfer(rhport, request, (void*)(uintptr_t) &desc_url, desc_url.bLength);

                case VENDOR_REQUEST_MICROSOFT:
                    if ( request->wIndex == 7 ) {
                        // Get Microsoft OS 2.0 compatible descriptor
                        uint16_t total_len;
                        memcpy(&total_len, desc_ms_os_20+8, 2);
                        return tud_control_xfer(rhport, request, (void*)(uintptr_t) desc_ms_os_20, total_len);
                    } else {
                        return false;
                    }

                default:
                    break;
            }
            break;

        case TUSB_REQ_TYPE_CLASS:
            if (request->bRequest == 0x22) {
                // Webserial simulate the CDC_REQUEST_SET_CONTROL_LINE_STATE (0x22) to connect and disconnect.
                web_serial_connected = (request->wValue != 0);

                // Always lit LED if connected
                if ( web_serial_connected ) {
                    board_led_write(true);
                    xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_ALWAYS_ON), 0);
                    tud_vendor_write_str("\r\nWebUSB interface connected\r\n");
                    tud_vendor_write_flush();
                } else {
                    xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
                }

                // response with status OK
                return tud_control_status(rhport, request);
            }
            break;

        default:
            break;
    }

    // stall unknown request
    return false;
}


void webserial_task(void)
{
    if (web_serial_connected) {
        if (tud_vendor_available()) {
            uint8_t buf[64];
            uint32_t count = tud_vendor_read(buf, sizeof(buf));

            if(count) {
                // echo back to both web serial and cdc
                echo_all(buf, count);
            }
        }
    }
}


//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void cdc_task(void)
{
    // connected() check for DTR bit
    // Most but not all terminal client set this when making connection
    if ( tud_cdc_connected() ) {
        // There are data available
        while ( tud_cdc_available() ) {
            uint8_t buf[64];
            // read and echo back
            uint32_t count = tud_cdc_read(buf, sizeof(buf));
            (void) count;

            // Echo back
            // Note: Skip echo by commenting out write() and write_flush()
            // for throughput test e.g
            //    $ dd if=/dev/zero of=/dev/ttyACM0 count=10000
            tud_cdc_write(buf, count);
        }
        tud_cdc_write_flush();
    }
}


//--------------------------------------------------------------------+
// USB Class Device
//--------------------------------------------------------------------+
void class_task(void* params)
{
    (void) params;

    // RTOS forever loop
    while ( 1 ) {
        cdc_task();
        webserial_task();
        vTaskDelay(1);
    }
}


// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    (void) itf;
    (void) rts;

    // TODO set some indicator
    if ( dtr ) {
        // Terminal connected
    } else {
        // Terminal disconnected
    }
}


// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
    (void) itf;
}


//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
static void led_blinky_cb(TimerHandle_t xTimer)
{
  (void) xTimer;
  static bool led_state = false;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
