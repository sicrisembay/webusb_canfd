#include "stdbool.h"
#include "stdint.h"
#include "stm32g4xx_hal.h"
#include "bsp/board_api.h"

static FDCAN_HandleTypeDef hfdcan1;


void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}


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
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN1;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}


static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /*
     *  Configure the main internal regulator output voltage
     */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

    /*
     *  Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     *
     * HSI RC = 16MHz
     * SYSCLK = 168MHz
     *
     * Enable HSI48 for USB
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSI48;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
    RCC_OscInitStruct.PLL.PLLN = 21;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        __disable_irq();
        while(1);
    }

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
        __disable_irq();
        while(1);
    }
}


static void BoardGpio_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();   // GREEN and LED use GPIOA

    /* Initialize Green LED */
    HAL_GPIO_WritePin(GREEN_LED_Port, GREEN_LED_Pin, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = GREEN_LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GREEN_LED_Port, &GPIO_InitStruct);
    /* Initialize Blue LED */
    HAL_GPIO_WritePin(BLUE_LED_Port, BLUE_LED_Pin, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = BLUE_LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BLUE_LED_Port, &GPIO_InitStruct);
}


static void BoardCAN_Config(void)
{
    hfdcan1.Instance = FDCAN1;
    hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV1;
    hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission = ENABLE;
    hfdcan1.Init.TransmitPause = DISABLE;
    hfdcan1.Init.ProtocolException = DISABLE;
    hfdcan1.Init.NominalPrescaler = 16;
    hfdcan1.Init.NominalSyncJumpWidth = 1;
    hfdcan1.Init.NominalTimeSeg1 = 2;
    hfdcan1.Init.NominalTimeSeg2 = 2;
    hfdcan1.Init.DataPrescaler = 1;
    hfdcan1.Init.DataSyncJumpWidth = 1;
    hfdcan1.Init.DataTimeSeg1 = 1;
    hfdcan1.Init.DataTimeSeg2 = 1;
    hfdcan1.Init.StdFiltersNbr = 0;
    hfdcan1.Init.ExtFiltersNbr = 0;
    hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
    {
        __disable_irq();
        while(1);
    }
}


void board_init()
{
    HAL_Init();
    SystemClock_Config();
    BoardGpio_Config();
#if 0
    BoardCAN_Config();
#endif
}


void board_led_write(bool isOn)
{
    if(isOn) {
        HAL_GPIO_WritePin(GREEN_LED_Port, GREEN_LED_Pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(GREEN_LED_Port, GREEN_LED_Pin, GPIO_PIN_SET);
    }

}
