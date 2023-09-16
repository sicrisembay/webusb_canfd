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

/*
 * Command Format
 *  Command     : 1 byte
 *  Payload     : CONFIG_CMD_FRAME_SIZE - 1
 */

#define OFFSET_COMMAND_ID               (0x00)


/* COMMAND: 0x01 *************************************************************/
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
                webusb_set_connect_state(commandBuffer.param.raw[0] == 0x01);
            }
            break;
        }
        default: {
            break;
        }
    }

    return 0;
}
