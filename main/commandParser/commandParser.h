/*
 * commandParser.h
 *
 *  Created on: Sep 3, 2023
 *      Author: Sicris
 */

#ifndef COMMANDPARSER_COMMANDPARSER_H_
#define COMMANDPARSER_COMMANDPARSER_H_

#define OFFSET_COMMAND_ID               (0x00)
#define OFFSET_MSGID                    (0x01)
#define OFFSET_DLC                      (0x05)
#define OFFSET_DATA                     (0x06)
#define SZ_COMMAND_OVERHEAD             (1 + (4 + 1)) // 1byte command + 4byte msgID + 1byte DLC

/* COMMAND: CAN_SEND (0x10) ***************************************************/
#define COMMAND_CAN_SEND                (0x10)
#define SZMAX_CMD_CAN_SEND              (SZ_COMMAND_OVERHEAD + 8) // +8bytes payload

/* COMMAND: CAN_DEVICE_TO_HOST (0x20) ****************************************/
#define COMMAND_DEVICE_TO_HOST_CAN_STANDARD     (0x20)
#define COMMAND_DEVICE_TO_HOST_CAN_EXTENDED     (0x21)
#define COMMAND_DEVICE_TO_HOST_FD_STANDARD      (0x22)
#define COMMAND_DEVICE_TO_HOST_FD_EXTENDED      (0x23)

void command_parser_init(void);

#endif /* COMMANDPARSER_COMMANDPARSER_H_ */
