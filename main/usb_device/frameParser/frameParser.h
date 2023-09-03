/*
 * frameParser.h
 *
 *  Created on: Sep 3, 2023
 *      Author: Sicris
 */

#ifndef USB_DEVICE_FRAMEPARSER_FRAMEPARSER_H_
#define USB_DEVICE_FRAMEPARSER_FRAMEPARSER_H_

#include "stdint.h"
#include "FreeRTOS.h"
#include "semphr.h"

#ifndef CONFIG_CMD_FRAME_SIZE
#define CONFIG_CMD_FRAME_SIZE           (128)
#endif /* CONFIG_CMD_FRAME_SIZE */

typedef int32_t (* cbValidFrame)(uint32_t);

typedef struct {
    cbValidFrame callback;
    uint8_t * pCommandBuffer;
    SemaphoreHandle_t mutex;
} frame_valid_cb_t;

void frame_parser_init(frame_valid_cb_t * pCallbackDef);
void frame_parser_receive(uint8_t *pBuf, uint32_t len);
void frame_parser_process(void);
void frame_parser_format_frame(uint8_t *pBuf, uint32_t len);

#endif /* USB_DEVICE_FRAMEPARSER_FRAMEPARSER_H_ */
