/*
 * frameParser.c
 *
 *  Created on: Sep 3, 2023
 *      Author: Sicris
 */

#include "stdbool.h"
#include "frameParser.h"

#ifndef CONFIG_PARSER_RX_BUF_SIZE
#define CONFIG_PARSER_RX_BUF_SIZE       (1024)
#endif /* CONFIG_PARSER_RX_BUF_SIZE */

#ifndef CONFIG_PARSER_TX_BUF_SIZE
#define CONFIG_PARSER_TX_BUF_SIZE       (512)
#endif /* CONFIG_PARSER_TX_BUF_SIZE */

#ifndef CONFIG_CMD_FRAME_SIZE
#define CONFIG_CMD_FRAME_SIZE           (128)
#endif /* CONFIG_CMD_FRAME_SIZE */

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
#define SZ_LENGTH                       (4)
#define SZ_PKT_SEQ                      (4)
#define SZ_CHECKSUM                     (1)
#define SZ_FRAME_OVERHEAD               (SZ_TAG_SOF + SZ_LENGTH + SZ_PKT_SEQ + SZ_CHECKSUM)

// Offset
#define OFFSET_TAG_SOF                  (0)
#define OFFSET_LENGTH                   (SZ_TAG_SOF)
#define OFFSET_PKT_SEQ                  (OFFSET_LENGTH + SZ_LENGTH)
#define OFFSET_PAYLOAD                  (OFFSET_PKT_SEQ + SZ_PKT_SEQ)

#define TAG_SOF                         (0xFF)  //!< Value used as Start of frame


#define FRAME_RX_OVERHEAD               (7)  /* 7 bytes */
#define FRAME_RX_PAYLOAD_CMD_OFFSET     (5)
#define FRAME_RX_PAYLOAD_PARAM_OFFSET   (6)

static bool bInit = false;
static uint32_t rdPtr = 0;
static uint32_t wrPtr = 0;
static uint8_t rxFrameBuffer[CONFIG_PARSER_RX_BUF_SIZE];
static uint32_t packetSeq = 0;
static SemaphoreHandle_t xParserMutex = NULL;
static StaticSemaphore_t xMutexBuffer;
static frame_valid_cb_t validFrameCb = {0};


static void ProcessValidFrame(uint32_t index, uint32_t len)
{
    uint32_t i = 0;

    if(len <= SZ_FRAME_OVERHEAD) {
        return;
    }

    /* Remove overhead from frame */
    index = (index + SZ_TAG_SOF + SZ_LENGTH + SZ_PKT_SEQ) % CONFIG_PARSER_RX_BUF_SIZE;
    len = len - SZ_FRAME_OVERHEAD;

    if(len > CONFIG_CMD_FRAME_SIZE) {
        return;
    }

    /* Prepare Data and execute handler callback */
    xSemaphoreTakeRecursive(validFrameCb.mutex, portMAX_DELAY);
    for(i = 0; i < len; i++) {
        validFrameCb.pCommandBuffer[i] = rxFrameBuffer[index];
        index = (index + 1) % CONFIG_PARSER_RX_BUF_SIZE;
    }
    validFrameCb.callback(len);
    xSemaphoreGiveRecursive(validFrameCb.mutex);
}


void frame_parser_init(frame_valid_cb_t * pCallbackDef)
{
    configASSERT(pCallbackDef);

    if(!bInit) {
        xParserMutex = xSemaphoreCreateRecursiveMutexStatic( &xMutexBuffer );
        configASSERT(xParserMutex);

        rdPtr = 0U;
        wrPtr = 0U;
        packetSeq = 0U;

        validFrameCb.callback = pCallbackDef->callback;
        configASSERT(validFrameCb.callback);
        validFrameCb.pCommandBuffer = pCallbackDef->pCommandBuffer;
        configASSERT(validFrameCb.pCommandBuffer);
        validFrameCb.mutex = pCallbackDef->mutex;
        configASSERT(validFrameCb.mutex);

        bInit = true;
    }
}


void frame_parser_receive(uint8_t *pBuf, uint32_t len)
{
    uint32_t i = 0;

    if(!bInit) {
        return;
    }

    xSemaphoreTakeRecursive(xParserMutex, portMAX_DELAY);

    for(i = 0; i < len; i++) {
        rxFrameBuffer[wrPtr] = pBuf[i];
        wrPtr = (wrPtr + 1) % CONFIG_PARSER_RX_BUF_SIZE;
    }

    xSemaphoreGiveRecursive(xParserMutex);
}


void frame_parser_process(void)
{
    uint32_t availableBytes = 0;
    uint32_t length = 0;
    uint32_t idx = 0;
    uint8_t sum = 0;

    if(!bInit) {
        return;
    }

    xSemaphoreTakeRecursive(xParserMutex, portMAX_DELAY);

    while(wrPtr != rdPtr) {
        /* Check start of command TAG */
        if(TAG_SOF != rxFrameBuffer[rdPtr])
        {
            // Skip character
            rdPtr = (rdPtr + 1) % CONFIG_PARSER_RX_BUF_SIZE;
            continue;
        }

        /* Get available bytes in the buffer */
        if(wrPtr >= rdPtr) {
            availableBytes = wrPtr - rdPtr;
        } else {
            availableBytes = (wrPtr + CONFIG_PARSER_RX_BUF_SIZE) - rdPtr;
        }
        if(availableBytes < SZ_FRAME_OVERHEAD) {
            /* Can't be less than frame overhead */
            break;
        }
        // See if the packet size byte is valid.  A command packet must be at
        // least four bytes and can not be larger than the receive buffer size.
        length = (uint32_t)(rxFrameBuffer[(rdPtr+1)%CONFIG_PARSER_RX_BUF_SIZE]) +
                ((uint32_t)(rxFrameBuffer[(rdPtr+2)%CONFIG_PARSER_RX_BUF_SIZE]) << 8) +
                ((uint32_t)(rxFrameBuffer[(rdPtr+3)%CONFIG_PARSER_RX_BUF_SIZE]) << 16) +
                ((uint32_t)(rxFrameBuffer[(rdPtr+4)%CONFIG_PARSER_RX_BUF_SIZE]) << 24);

        if((length < SZ_FRAME_OVERHEAD) || (length > (CONFIG_PARSER_RX_BUF_SIZE-1)))
        {
            // The packet size is too large, so either this is not the start of
            // a packet or an invalid packet was received.  Skip this start of
            // command packet tag.
            rdPtr = (rdPtr + 1) % CONFIG_PARSER_RX_BUF_SIZE;

            // Keep scanning for a start of command packet tag.
            continue;
        }

        // If the entire command packet is not in the receive buffer then stop
        if(availableBytes < length)
        {
            break;
        }

        // The entire command packet is in the receive buffer, so compute its
        // checksum.
        for(idx = 0, sum = 0; idx < length; idx++)
        {
            sum += rxFrameBuffer[(rdPtr + idx)%CONFIG_PARSER_RX_BUF_SIZE];
        }

        // Skip this packet if the checksum is not correct (that is, it is
        // probably not really the start of a packet).
        if(sum != 0)
        {
            // Skip this character
            rdPtr = (rdPtr + 1) % CONFIG_PARSER_RX_BUF_SIZE;

            // Keep scanning for a start of command packet tag.
            continue;
        }

        // A valid command packet was received, so process it now.
        ProcessValidFrame(rdPtr, length);

        // Done with processing this command packet.
        rdPtr = (rdPtr + length) % CONFIG_PARSER_RX_BUF_SIZE;
    }

    xSemaphoreGiveRecursive(xParserMutex);
}


void frame_parser_format_frame(uint8_t *pBuf, uint32_t len)
{
    uint32_t i;
    uint8_t sum = 0;

    xSemaphoreTakeRecursive(xParserMutex, portMAX_DELAY);

    /*
     * Start of Frame
     */
    pBuf[OFFSET_TAG_SOF] = TAG_SOF;
    /*
     * Set Length
     */
    for(i = 0; i < SZ_LENGTH; i++) {
        pBuf[OFFSET_LENGTH+i] = (uint8_t)((len >> (8*i)) & 0xFF);
    }

    /*
     * Set Packet Sequence
     */
    for(i = 0; i < SZ_PKT_SEQ; i++) {
        pBuf[OFFSET_PKT_SEQ + i] = (uint8_t)((packetSeq >> (8*i)) & 0xFF);
    }
    packetSeq++;

    /*
     * Calculate Checksum
     */
    for(i = 0; i < (len-1); i++) {
        sum += pBuf[i];
    }
    pBuf[i] = (uint8_t)((~sum) + 1);

    xSemaphoreGiveRecursive(xParserMutex);
}
