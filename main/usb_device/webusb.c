/*
 * webusb.c
 *
 *  Created on: Sep 3, 2023
 *      Author: Sicris
 */

#include "stdbool.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "tusb.h"
#include "webusb.h"
#include "frameParser/frameParser.h"
#include "usb_descriptors.h"
#include "bsp/board_api.h"

#define USB_DEVICE_STACK_SIZE           (384)
#define USB_CLASS_STACK_SIZE            (256)
#define URL  "sicrisembay.github.io/webusb_canfd/"

#define EVENT_CDC_AVAILABLE_BIT         (0x00000001)
#define EVENT_VENDOR_AVAILABLE_BIT      (0x00000002)

#define WEBUSB_TX_QUEUE_LENGTH          (10)
#define WEBUSB_TX_ELEMENT_SZ            CFG_TUD_VENDOR_TX_BUFSIZE
static StaticQueue_t webUsbTxStaticQueue;
uint8_t webUsbTxQueueStorageArea[WEBUSB_TX_QUEUE_LENGTH * WEBUSB_TX_ELEMENT_SZ];
static QueueHandle_t webUsbTxQHandle;

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

static bool bInit = false;
static bool webusb_connected = false;

/* USB Device Task */
static TaskHandle_t deviceTask = NULL;
static StackType_t usb_device_stack[USB_DEVICE_STACK_SIZE];
static StaticTask_t usb_device_taskdef;

static TaskHandle_t classTask = NULL;
static StackType_t  usb_class_stack[USB_CLASS_STACK_SIZE];
static StaticTask_t usb_class_taskdef;

static TimerHandle_t blinky_tm = NULL;
static StaticTimer_t blinky_tmdef;

const tusb_desc_webusb_url_t desc_url = {
    .bLength         = 3 + sizeof(URL) - 1,
    .bDescriptorType = 3, // WEBUSB URL type
    .bScheme         = 1, // 0: http, 1: https
    .url             = URL
};

static void usb_device_task(void * pxParam);
static void usb_class_task(void * pxParam);
static void led_blinky_cb(TimerHandle_t xTimer);

void webusb_init(void)
{
    if(!bInit) {
        blinky_tm = xTimerCreateStatic(
                            "usb blinky",
                            pdMS_TO_TICKS(BLINK_NOT_MOUNTED),
                            true,
                            NULL,
                            led_blinky_cb,
                            &blinky_tmdef
                            );
        deviceTask = xTaskCreateStatic(
                            usb_device_task,
                            "usb-device",
                            USB_DEVICE_STACK_SIZE,
                            NULL,
                            configMAX_PRIORITIES - 1,
                            usb_device_stack,
                            &usb_device_taskdef
                            );
        classTask = xTaskCreateStatic(
                            usb_class_task,
                            "usb-class",
                            USB_CLASS_STACK_SIZE,
                            NULL,
                            configMAX_PRIORITIES - 2,
                            usb_class_stack,
                            &usb_class_taskdef
                            );
        webUsbTxQHandle = xQueueCreateStatic(
                            WEBUSB_TX_QUEUE_LENGTH,
                            WEBUSB_TX_ELEMENT_SZ,
                            webUsbTxQueueStorageArea,
                            &webUsbTxStaticQueue
                            );
        xTimerStart(blinky_tm, 0);

        bInit = true;
    }
}


void webusb_set_connect_state(bool isConnected)
{
    webusb_connected = isConnected;
    const char * pConnectString = "WebUSB interface connected\r\n";
    const char * pDisconnectString = "WebUSB interface disconnected\r\n";

    // Always lit LED if connected
    if ( webusb_connected ) {
        xQueueReset(webUsbTxQHandle);
        // HACK: prime WebUSB EPIN where first CAN packet is lost -->
        uint8_t buf[64];
        memset(buf, 0, sizeof(buf));
        tud_vendor_write(buf, sizeof(buf));
        // <-- End HACK

        board_led_write(true);
        xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_ALWAYS_ON), 0);
        if (tud_cdc_connected()) {
            tud_cdc_write_str(pConnectString);
            tud_cdc_write_flush();
        }
    } else {
        xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
        if (tud_cdc_connected()) {
            tud_cdc_write_str(pDisconnectString);
            tud_cdc_write_flush();
        }
    }
}

bool webusb_sendEp(uint8_t * pBuffer)
{
    bool ret = true;
    bool available = (tud_vendor_write_available() >= CFG_TUD_VENDOR_TX_BUFSIZE);
    bool queueNotEmpty = (uxQueueMessagesWaiting(webUsbTxQHandle) > 0);

    if(queueNotEmpty || !available) {
        ret = ret && (pdTRUE == xQueueSend(webUsbTxQHandle, pBuffer, 0));
        if(available) {
            ret = ret && (pdTRUE == xQueueReceive(webUsbTxQHandle, pBuffer, 0));
        }
    }

    if(available) {
        ret = ret && (CFG_TUD_VENDOR == tud_vendor_write(pBuffer, CFG_TUD_VENDOR_TX_BUFSIZE));
    }

    return (ret);
}

//--------------------------------------------------------------------+
// USB Device Task
//--------------------------------------------------------------------+
static void usb_device_task(void * pxParam)
{
    (void)pxParam;
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

        if (tud_vendor_available()) {
            // Set event bit to process vendor task
            xTaskNotify(classTask, EVENT_VENDOR_AVAILABLE_BIT, eSetBits);
        }
        tud_vendor_write_flush();

        if(tud_cdc_available()) {
            // Set event bit to process cdc task
            xTaskNotify(classTask, EVENT_CDC_AVAILABLE_BIT, eSetBits);
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
// USB CDC Callbacks
//--------------------------------------------------------------------+
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
// USB Vendor (WebUSB) callbacks
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

void tud_vendor_tx_cb(uint8_t itf, uint32_t sent_bytes)
{
    uint8_t sendEpPacket[CFG_TUD_VENDOR_TX_BUFSIZE];

    if(tud_vendor_write_available() == CFG_TUD_VENDOR_TX_BUFSIZE) {
        /* Empty */
        if(pdTRUE != xQueueReceive(webUsbTxQHandle, &sendEpPacket, 0)) {
            /* empty */
            memset(sendEpPacket, 0, CFG_TUD_VENDOR_TX_BUFSIZE);
        }
        tud_vendor_write(sendEpPacket, CFG_TUD_VENDOR_TX_BUFSIZE);
    }
}

void tud_vendor_rx_cb(uint8_t itf)
{

}

//--------------------------------------------------------------------+
// USB Class Device
//   * calls WebUSB class
//   * calls CDC class
//--------------------------------------------------------------------+
static void usb_class_task(void * pxParam)
{
    (void)pxParam;
    uint32_t event = 0;
    uint8_t buf[64];

    while(1) {
        if(pdTRUE == xTaskNotifyWait(0, 0xFFFFFFFF, &event, portMAX_DELAY)) {
            if((event & EVENT_CDC_AVAILABLE_BIT) != 0) {
                while (tud_cdc_available()) {
                    // read and echo back
                    uint32_t count = tud_cdc_read(buf, sizeof(buf));
                    /// TODO: implement CLI
                }
            }
            if((event & EVENT_VENDOR_AVAILABLE_BIT) != 0) {
                uint32_t count = tud_vendor_read(buf, sizeof(buf));
                if(count > 1) {
                    /* push the receive data to frame parser */
                    frame_parser_receive(&buf[1], buf[0]);
                    frame_parser_process();
                }
            }
        }
    }
}


static void led_blinky_cb(TimerHandle_t xTimer)
{
    (void) xTimer;
    static bool led_state = false;

    board_led_write(led_state);
    led_state = !led_state;
}

