/*!
 * \file can.c
 *
 * \author Sicris Rey Embay
 */
#include "stm32g4xx_hal.h"
#include "board_api.h"
#include "can.h"
#include "main.h"

#define CAN_TEST            (1)
#define CAN_STACK_SIZE      (256)

FDCAN_HandleTypeDef hfdcan1;
static TaskHandle_t canTask = NULL;
static StackType_t can_stack[CAN_STACK_SIZE];
static StaticTask_t can_taskdef;

#if (CAN_TEST == 1) /* TEST */
FDCAN_RxHeaderTypeDef RxHeader;
uint8_t RxData[8];
FDCAN_TxHeaderTypeDef TxHeader;
uint8_t TxData[8];
#endif /* CAN_TEST */

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
    hfdcan1.Instance = FDCAN1;
    hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV2;
    hfdcan1.Init.FrameFormat = FDCAN_FRAME_FD_NO_BRS;
    hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission = DISABLE;
    hfdcan1.Init.TransmitPause = DISABLE;
    hfdcan1.Init.ProtocolException = DISABLE;
    hfdcan1.Init.NominalPrescaler = 1;
    hfdcan1.Init.NominalSyncJumpWidth = 12;
    hfdcan1.Init.NominalTimeSeg1 = 71;
    hfdcan1.Init.NominalTimeSeg2 = 12;
    hfdcan1.Init.DataPrescaler = 3;
    hfdcan1.Init.DataSyncJumpWidth = 4;
    hfdcan1.Init.DataTimeSeg1 = 23;
    hfdcan1.Init.DataTimeSeg2 = 4;
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
    uint8_t txTestData = 0;

    TxHeader.Identifier = 0x321;
    TxHeader.IdType = FDCAN_STANDARD_ID;
    TxHeader.TxFrameType = FDCAN_DATA_FRAME;
    TxHeader.DataLength = FDCAN_DLC_BYTES_8;
    TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
    TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
    TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    TxHeader.MessageMarker = 0;

    ASSERT_ME(HAL_FDCAN_Start(&hfdcan1) == HAL_OK);
    ASSERT_ME(HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) == HAL_OK);

    NVIC_EnableIRQ(FDCAN1_IT0_IRQn);

    while(1) {
        vTaskDelay(5000);
#if (CAN_TEST)
        for(int i = 0; i < 8; i++) {
            TxData[i] = txTestData++;
        }
        ASSERT_ME(HAL_OK == HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHeader, TxData));
#endif
    }
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET) {
        /* Retrieve Rx messages from RX FIFO0 */
#if (CAN_TEST)
        ASSERT_ME(HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK);
#endif
    }
}
