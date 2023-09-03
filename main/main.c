/*!
 * \file main.c
 */
#include "FreeRTOS.h"
#include "bsp/board_api.h"
#include "webusb.h"
#include "commandParser/commandParser.h"

/*------------- MAIN -------------*/
int main(void)
{
    board_init();
    webusb_init();
    command_parser_init();

    vTaskStartScheduler();
}

