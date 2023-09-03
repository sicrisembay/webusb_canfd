/*!
 * \file main.c
 */
#include "FreeRTOS.h"
#include "webusb.h"
#include "bsp/board_api.h"

/*------------- MAIN -------------*/
int main(void)
{
    board_init();
    webusb_init();

    vTaskStartScheduler();
}








