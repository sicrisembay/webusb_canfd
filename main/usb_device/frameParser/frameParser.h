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

/* Since WebUSB TransferIN must be the same size as endpoint */
#define SZ_USB_BYTES_IN_PACKET          (1) // ALWAYS offest 0 in EP buffer

/*
 * Frame Format
 *   TAG        : 1 byte
 *   Length     : 4 bytes
 *   Packet Seq : 4 bytes
 *   Payload    : N Bytes
 *   Checksum   : 1 byte
 */
// Size in bytes
#define SZ_TAG_SOF                      (1)
#define SZ_LENGTH                       (2)
#define SZ_PKT_SEQ                      (2)
#define SZ_CHECKSUM                     (1)
#define SZ_FRAME_OVERHEAD               (SZ_TAG_SOF + SZ_LENGTH + SZ_PKT_SEQ + SZ_CHECKSUM)

// Offset
#define OFFSET_USB_BYTES_IN_PACKET      (0)
#define OFFSET_TAG_SOF                  (OFFSET_USB_BYTES_IN_PACKET + SZ_USB_BYTES_IN_PACKET)
#define OFFSET_LENGTH                   (OFFSET_TAG_SOF + SZ_TAG_SOF)
#define OFFSET_PKT_SEQ                  (OFFSET_LENGTH + SZ_LENGTH)
#define OFFSET_PAYLOAD                  (OFFSET_PKT_SEQ + SZ_PKT_SEQ)

#define TAG_SOF                         (0xFF)  //!< Value used as Start of frame

typedef int32_t (* cbValidFrame)(uint32_t);

typedef struct {
    cbValidFrame callback;
    uint8_t * pCommandBuffer;
    SemaphoreHandle_t mutex;
} frame_valid_cb_t;

void frame_parser_init(frame_valid_cb_t * pCallbackDef);
bool frame_parser_receive(uint8_t *pBuf, uint32_t len);
void frame_parser_process(void);

#endif /* USB_DEVICE_FRAMEPARSER_FRAMEPARSER_H_ */
