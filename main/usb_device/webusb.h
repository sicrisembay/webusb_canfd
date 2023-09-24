/*
 * webusb.h
 *
 *  Created on: Sep 3, 2023
 *      Author: Sicris
 */

#ifndef USB_DEVICE_WEBUSB_H_
#define USB_DEVICE_WEBUSB_H_

void webusb_init(void);
void webusb_set_connect_state(bool isConnected);
bool webusb_sendEp(uint8_t * pBuffer);

#endif /* USB_DEVICE_WEBUSB_H_ */
