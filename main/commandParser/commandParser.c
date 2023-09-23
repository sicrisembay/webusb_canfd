/*
 * commandParser.c
 *
 *  Created on: Sep 3, 2023
 *      Author: Sicris
 */

#include "stdbool.h"
#include "commandParser.h"
#include "frameParser/frameParser.h"
#include "usb_device/webusb.h"
#include "bsp/can.h"

/*
 * Command Format
 *  Command     : 1 byte
 *  Payload     : CONFIG_CMD_FRAME_SIZE - 1
 */


/* COMMAND: CONNECT (0x01) ****************************************************/
#define COMMAND_CONNECT                 (0x01)
#define SZ_CMD_CONNECT                  (1 + 1)  // 1byte command + 1byte parameter
/* Param0
 *  0x01: Connect
 *  0x02: Disconnect
 */

typedef struct __attribute__ ((packed)) {
    uint8_t commandId;
    union {
        uint8_t raw[CONFIG_CMD_FRAME_SIZE - 1];
    } param;
} command_t;


static bool bInit = false;
static command_t __attribute__ ((aligned (4))) commandBuffer;
static SemaphoreHandle_t xParserMutex = NULL;
static StaticSemaphore_t xMutexBuffer;
static int32_t commandHandler(uint32_t length);


void command_parser_init(void)
{
    frame_valid_cb_t validCb;

    if(!bInit) {
        xParserMutex = xSemaphoreCreateRecursiveMutexStatic( &xMutexBuffer );
        configASSERT(xParserMutex);

        validCb.callback = commandHandler;
        validCb.pCommandBuffer = (uint8_t *)(&commandBuffer);
        validCb.mutex = xParserMutex;

        frame_parser_init(&validCb);

        bInit = true;
    }
}


static int32_t commandHandler(uint32_t length)
{
    switch(commandBuffer.commandId) {
        case COMMAND_CONNECT: {
            if(SZ_CMD_CONNECT == length) {
                if(commandBuffer.param.raw[0] == 0x01) {
                    /* Connect */
                    webusb_set_connect_state(true);
                    if(CAN_configure(ARBIT_1MHZ, DATA_1MHZ)) {
                        CAN_start();
                    }
                } else {
                    /* Disconnect */
                    webusb_set_connect_state(false);
                    CAN_stop();
                }
            }
            break;
        }
        case COMMAND_CAN_SEND: {
            if((length >= SZ_COMMAND_OVERHEAD) && (length <= SZMAX_CMD_CAN_SEND)) {
                tx_queue_element_t txElement;
                uint8_t dlc = commandBuffer.param.raw[2];
                if(dlc > 8) {
                    /* classic CAN max payload is 8 bytes */
                    break;
                }
                /* header */
                txElement.header.Identifier = 0x07FF & ((uint16_t)commandBuffer.param.raw[0] +
                                        (((uint16_t)commandBuffer.param.raw[1]) << 8));
                txElement.header.IdType = FDCAN_STANDARD_ID;
                txElement.header.TxFrameType = FDCAN_DATA_FRAME;
                txElement.header.DataLength = ((uint32_t)dlc) << 16U;
                txElement.header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
                txElement.header.BitRateSwitch = FDCAN_BRS_OFF;
                txElement.header.FDFormat = FDCAN_CLASSIC_CAN;
                txElement.header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
                txElement.header.MessageMarker = 0;
                /* payload */
                for(uint32_t i = 0; i < dlc; i++) {
                    txElement.data[i] = commandBuffer.param.raw[3 + i];
                }
                CAN_send(&txElement);
            }
            break;
        }
        default: {
            break;
        }
    }

    return 0;
}
