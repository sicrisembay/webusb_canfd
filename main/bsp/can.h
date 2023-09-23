/*!
 * \file can.h
 *
 * \author Sicris Rey Embay
 */
#ifndef CAN_H
#define CAN_H

#include "stm32g4xx_hal.h"

typedef enum {
    ARBIT_500KHZ = 0,
    ARBIT_1MHZ,
    N_SUPPORTED_ARBIT_BITRATE
} ARBIT_BITRATE_T;

typedef enum {
    DATA_500KHZ = 0,
    DATA_1MHZ,
    N_SUPPORTED_DATA_BITRATE
} DATA_BITRATE_T;

typedef struct {
    FDCAN_TxHeaderTypeDef header;
    uint8_t data[64];   // max CAN-FD payload size
} tx_queue_element_t;


void CAN_init(void);
bool CAN_configure(ARBIT_BITRATE_T arb_bps, DATA_BITRATE_T dat_bps);
bool CAN_start(void);
bool CAN_stop(void);
bool CAN_send(tx_queue_element_t * pElem);


#endif /* CAN_H */
