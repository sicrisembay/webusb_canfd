/*!
 * \file can.c
 *
 * \author Sicris Rey Embay
 */
#include "stm32g4xx_hal.h"
#include "board_api.h"
#include "can.h"
#include "main.h"
#include "usb_device/frameParser/frameParser.h"
#include "usb_device/webusb.h"
#include "commandParser/commandParser.h"

#define CAN_TX_BIT          (0x01)
#define CAN_RX_BIT          (0x02)

#define CAN_STACK_SIZE          (256)

FDCAN_HandleTypeDef hfdcan1;
static TaskHandle_t canTask = NULL;
static StackType_t can_stack[CAN_STACK_SIZE];
static StaticTask_t can_taskdef;
static ARBIT_BITRATE_T arbit_bps = ARBIT_1MHZ;
static DATA_BITRATE_T data_bps = DATA_1MHZ;
static bool txInProgress = false;

typedef struct {
    FDCAN_RxHeaderTypeDef header;
    uint8_t data[64];
} rx_queue_element_t;

typedef struct {
    uint32_t prescaler;
    uint32_t sjw;
    uint32_t tseg1;
    uint32_t tseg2;
} timing_config_t;


#define CAN_TX_QUEUE_LENGTH     (3)
#define CAN_TX_ELEMENT_SZ       sizeof(tx_queue_element_t)
static StaticQueue_t canTxStaticQueue;
uint8_t canTxQueueStorageArea[CAN_TX_QUEUE_LENGTH * CAN_TX_ELEMENT_SZ];
static QueueHandle_t canTxQHandle;

#define CAN_RX_QUEUE_LENGTH     (3)
#define CAN_RX_ELEMENT_SZ       sizeof(rx_queue_element_t)
static StaticQueue_t canRxStaticQueue;
uint8_t canRxQueueStorageArea[CAN_RX_QUEUE_LENGTH * CAN_RX_ELEMENT_SZ];
static QueueHandle_t canRxQHandle;

/*
 * NOTE:
 *   Peripheral Clock is 84MHz (168MHz with prescaler of DIV2)
 *   Clock tolerance value is assumed 4687.5ppm (review datasheet)
 *   Node delay is 180ns (review transceiver worst delay)
 *
 *   Sampling Point is 85.71%
 *
 */
static timing_config_t const DEFAULT_ARBITRATION_TIMING[N_SUPPORTED_ARBIT_BITRATE] = {
    {
        /* ARBIT_500KHZ */
        .prescaler = 1,
        .sjw = 24,
        .tseg1 = 143,
        .tseg2 = 24
    },
    {
        /* ARBIT_1MHZ */
        .prescaler = 1,
        .sjw = 12,
        .tseg1 = 71,
        .tseg2 = 12
    },
};

static timing_config_t const DEFAULT_DATA_TIMING[N_SUPPORTED_DATA_BITRATE] = {
    {
        /* DATA_500KHZ */
        .prescaler = 6,
        .sjw = 4,
        .tseg1 = 23,
        .tseg2 = 4
    },
    {
        /* DATA_1MHZ */
        .prescaler = 3,
        .sjw = 4,
        .tseg1 = 23,
        .tseg2 = 4
    },
};

static const uint8_t DLCtoBytes[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

static void can_task(void * pxParam);

void HAL_FDCAN_MspInit(FDCAN_HandleTypeDef* hfdcan)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    if(hfdcan->Instance==FDCAN1) {
        /*
         * Initializes the peripherals clocks
         */
        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
        PeriphClkInit.FdcanClockSelection = RCC_FDCANCLKSOURCE_PCLK1;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
            __disable_irq();
            while(1);
        }
        /* Peripheral clock enable */
        __HAL_RCC_FDCAN_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        /*
         * FDCAN1 GPIO Configuration
         * PB8-BOOT0------> FDCAN1_RX
         * PB9      ------> FDCAN1_TX
         */
        GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN1;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}

void CAN_init(void)
{
    canTxQHandle = xQueueCreateStatic(
                                CAN_TX_QUEUE_LENGTH,
                                CAN_TX_ELEMENT_SZ,
                                canTxQueueStorageArea,
                                &canTxStaticQueue);
    ASSERT_ME(canTxQHandle != NULL);

    canRxQHandle = xQueueCreateStatic(
                                CAN_RX_QUEUE_LENGTH,
                                CAN_RX_ELEMENT_SZ,
                                canRxQueueStorageArea,
                                &canRxStaticQueue);
    ASSERT_ME(canRxQHandle != NULL);

    hfdcan1.Instance = FDCAN1;
    hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV2;
    hfdcan1.Init.FrameFormat = FDCAN_FRAME_FD_NO_BRS;
    hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission = DISABLE;
    hfdcan1.Init.TransmitPause = DISABLE;
    hfdcan1.Init.ProtocolException = DISABLE;
    hfdcan1.Init.NominalPrescaler = DEFAULT_ARBITRATION_TIMING[arbit_bps].prescaler;
    hfdcan1.Init.NominalSyncJumpWidth = DEFAULT_ARBITRATION_TIMING[arbit_bps].sjw;
    hfdcan1.Init.NominalTimeSeg1 = DEFAULT_ARBITRATION_TIMING[arbit_bps].tseg1;
    hfdcan1.Init.NominalTimeSeg2 = DEFAULT_ARBITRATION_TIMING[arbit_bps].tseg2;
    hfdcan1.Init.DataPrescaler = DEFAULT_DATA_TIMING[data_bps].prescaler;
    hfdcan1.Init.DataSyncJumpWidth = DEFAULT_DATA_TIMING[data_bps].sjw;
    hfdcan1.Init.DataTimeSeg1 = DEFAULT_DATA_TIMING[data_bps].tseg1;
    hfdcan1.Init.DataTimeSeg2 = DEFAULT_DATA_TIMING[data_bps].tseg2;
    hfdcan1.Init.StdFiltersNbr = 0;
    hfdcan1.Init.ExtFiltersNbr = 0;
    hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    ASSERT_ME(HAL_FDCAN_Init(&hfdcan1) == HAL_OK);

    canTask = xTaskCreateStatic(
                        can_task,
                        "can-task",
                        CAN_STACK_SIZE,
                        NULL,
                        configMAX_PRIORITIES - 2,
                        can_stack,
                        &can_taskdef
                        );
}


static void can_task(void * pxParam)
{
    rx_queue_element_t rxElement;
    tx_queue_element_t txElement;
    uint32_t notifyValue = 0;
    uint8_t canDeviceToHost[CFG_TUD_VENDOR_EPSIZE];
    uint16_t packetSequence = 0;
    txInProgress = false;

    while(1) {
        if(pdPASS == xTaskNotifyWait(
                        pdFALSE,
                        UINT32_MAX,
                        &notifyValue,
                        portMAX_DELAY)) {
            if((notifyValue & CAN_TX_BIT) != 0) {
                if(pdTRUE == xQueueReceive(canTxQHandle, &txElement, 0)) {
                    if(HAL_OK == HAL_FDCAN_AddMessageToTxFifoQ(
                            &hfdcan1,
                            &(txElement.header),
                            &(txElement.data[0]))) {
                        txInProgress = true;
                    }
                } else {
                    txInProgress = false;
                }
            }
            if((notifyValue & CAN_RX_BIT) != 0) {
                if(pdTRUE == xQueueReceive(canRxQHandle, &rxElement, 0)) {
                    uint8_t checksum = 0;
                    uint32_t idx = 0;
                    bool multiplePacket = false;
                    const uint8_t dlc = DLCtoBytes[(uint8_t)((rxElement.header.DataLength >> 16) & 0x0F)];
                    const uint8_t maxpayload = CFG_TUD_VENDOR_EPSIZE - SZ_USB_BYTES_IN_PACKET - SZ_FRAME_OVERHEAD - SZ_COMMAND_OVERHEAD;
                    //const uint8_t frameSize = SZ_FRAME_OVERHEAD + SZ_COMMAND_OVERHEAD + dlc;
                    uint8_t frameSize = 0;

                    uint8_t command = 0;
                    if(rxElement.header.FDFormat == FDCAN_CLASSIC_CAN) {
                        /* Classic CAN */
                        if(rxElement.header.IdType == FDCAN_STANDARD_ID) {
                            /* Base Frame */
                            command = COMMAND_DEVICE_TO_HOST_CAN_STANDARD;
                        } else {
                            /* Extended Frame */
                            command = COMMAND_DEVICE_TO_HOST_CAN_EXTENDED;
                        }
                    } else {
                        /* CAN FD */
                        if(rxElement.header.IdType == FDCAN_STANDARD_ID) {
                            /* Base Frame */
                            command = COMMAND_DEVICE_TO_HOST_FD_STANDARD;
                        } else {
                            /* Extended Frame */
                            command = COMMAND_DEVICE_TO_HOST_FD_EXTENDED;
                        }
                    }

                    if(dlc > maxpayload) {
                        /* Exceeds the Endpoint Size */
                        multiplePacket = true;
                        frameSize = CFG_TUD_VENDOR_EPSIZE - 1;
                    } else {
                        multiplePacket = false;
                        frameSize = SZ_FRAME_OVERHEAD + SZ_COMMAND_OVERHEAD + dlc;
                    }

                    /*
                     * First Packet
                     */
                    memset(canDeviceToHost, 0, sizeof(canDeviceToHost));
                    // Frame Prefix -->
                    canDeviceToHost[OFFSET_USB_BYTES_IN_PACKET] = frameSize;
                    canDeviceToHost[OFFSET_TAG_SOF] = TAG_SOF;
                    canDeviceToHost[OFFSET_LENGTH] = (uint8_t)(frameSize & 0xFF);
                    canDeviceToHost[OFFSET_LENGTH + 1] = (uint8_t)(((frameSize) >> 8) & 0xFF);
                    canDeviceToHost[OFFSET_PKT_SEQ] = (uint8_t)(packetSequence & 0xFF);
                    canDeviceToHost[OFFSET_PKT_SEQ + 1] = (uint8_t)((packetSequence >> 8) & 0xFF);
                    packetSequence++;
                    // <-- Frame Prefix
                    // Payload -->
                    canDeviceToHost[OFFSET_PAYLOAD + OFFSET_COMMAND_ID] = command;
                    canDeviceToHost[OFFSET_PAYLOAD + OFFSET_MSGID] =
                            (uint8_t)(rxElement.header.Identifier & 0xFF);
                    canDeviceToHost[OFFSET_PAYLOAD + OFFSET_MSGID + 1] =
                            (uint8_t)((rxElement.header.Identifier >> 8) & 0xFF);
                    canDeviceToHost[OFFSET_PAYLOAD + OFFSET_MSGID + 2] =
                            (uint8_t)((rxElement.header.Identifier >> 16) & 0xFF);
                    canDeviceToHost[OFFSET_PAYLOAD + OFFSET_MSGID + 3] =
                            (uint8_t)((rxElement.header.Identifier >> 24) & 0xFF);
                    canDeviceToHost[OFFSET_PAYLOAD + OFFSET_DLC] = dlc;
                    for(idx = 0; idx < (multiplePacket ? maxpayload : dlc); idx++) {
                        canDeviceToHost[OFFSET_PAYLOAD + OFFSET_DATA + idx] =
                                rxElement.data[idx];
                    }
                    // <-- Payload
                    checksum = 0;
                    for(idx = 0; idx < (frameSize - 1); idx++) {
                        checksum += canDeviceToHost[idx + OFFSET_TAG_SOF];
                    }
                    canDeviceToHost[frameSize] = (uint8_t)((~checksum) + 1);

                    // Send to WebUSB queue
                    if(webusb_sendEp(&canDeviceToHost[0])) {
                        if(multiplePacket) {
                            /*
                             * Second Packet
                             */
                            /// TODO
                        }
                    } else {
                        /// TODO: handle error
                    }
                }
            }
        }
    }
}

/*
 * NOTE: This called from the interrupt
 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    rx_queue_element_t rxElement = {0};
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET) {
        /* Retrieve Rx messages from RX FIFO0 */
        if(HAL_OK == HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &(rxElement.header), &(rxElement.data[0]))) {
            xQueueSendFromISR(canRxQHandle, &rxElement, &xHigherPriorityTaskWoken);
            xTaskNotifyFromISR(canTask, CAN_RX_BIT, eSetBits, &xHigherPriorityTaskWoken);
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*
 * NOTE: This called from the interrupt
 */
void HAL_FDCAN_TxFifoEmptyCallback(FDCAN_HandleTypeDef *hfdcan)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(canTask, CAN_TX_BIT, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


bool CAN_configure(ARBIT_BITRATE_T arb_bps, DATA_BITRATE_T dat_bps)
{
    if(HAL_FDCAN_GetState(&hfdcan1) != HAL_FDCAN_STATE_READY) {
        return false;
    }

    hfdcan1.Init.NominalPrescaler = DEFAULT_ARBITRATION_TIMING[arb_bps].prescaler;
    hfdcan1.Init.NominalSyncJumpWidth = DEFAULT_ARBITRATION_TIMING[arb_bps].sjw;
    hfdcan1.Init.NominalTimeSeg1 = DEFAULT_ARBITRATION_TIMING[arb_bps].tseg1;
    hfdcan1.Init.NominalTimeSeg2 = DEFAULT_ARBITRATION_TIMING[arb_bps].tseg2;
    hfdcan1.Init.DataPrescaler = DEFAULT_DATA_TIMING[dat_bps].prescaler;
    hfdcan1.Init.DataSyncJumpWidth = DEFAULT_DATA_TIMING[dat_bps].sjw;
    hfdcan1.Init.DataTimeSeg1 = DEFAULT_DATA_TIMING[dat_bps].tseg1;
    hfdcan1.Init.DataTimeSeg2 = DEFAULT_DATA_TIMING[dat_bps].tseg2;
    if(HAL_FDCAN_Init(&hfdcan1) != HAL_OK) {
        return false;
    }
    arbit_bps = arb_bps;
    data_bps = dat_bps;

    return true;
}

bool CAN_start(void)
{
    if(HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        return false;
    }

    if(HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_TX_FIFO_EMPTY, FDCAN_TX_BUFFER0) != HAL_OK) {
        return false;
    }

    NVIC_EnableIRQ(FDCAN1_IT0_IRQn);

    return true;
}

bool CAN_stop(void)
{
    NVIC_DisableIRQ(FDCAN1_IT0_IRQn);

    if(HAL_FDCAN_DeactivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != HAL_OK) {
        return false;
    }

    if(HAL_FDCAN_Stop(&hfdcan1) != HAL_OK) {
        return false;
    }

    return true;
}

bool CAN_send(tx_queue_element_t * pElem)
{
    if(pdTRUE != xQueueSend(canTxQHandle, pElem, 0)) {
        return false;
    }
    if(!txInProgress){
        xTaskNotify(canTask, CAN_TX_BIT, eSetBits);
    }

    return true;
}

