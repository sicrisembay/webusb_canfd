import 'dart:typed_data';

import 'package:flutter/foundation.dart';
import 'package:usb_device/usb_device.dart';
import 'command_frame.dart';

const int kVendor = 0xCAFE;
const int kProduct = 0x4011;
const int kConfiguration = 1;
const int kInterface = 2;
const int kEndpoint = 3;

class WebUsbDevice {
  final UsbDevice _usbDevice = UsbDevice();
  late dynamic _pairedDevice;
  bool _isSupported = false;
  bool _isConnected = false;

  WebUsbDevice() {}

  bool isConnected() {
    return _isConnected;
  }

  Future<void> getDevices() async {}

  Future<void> connect() async {
    _isSupported = await _usbDevice.isSupported();
    if (_isSupported == false) {
      throw 'WebUSB not supported!';
    }
    if (!_isConnected) {
      _pairedDevice = await _usbDevice.requestDevices(
          [DeviceFilter(vendorId: kVendor, productId: kProduct)]);
      await _usbDevice.open(_pairedDevice);
      await _usbDevice.selectConfiguration(_pairedDevice, kConfiguration);
      await _usbDevice.claimInterface(_pairedDevice, kInterface);
      USBOutTransferResult usbOutTransferResult = await _usbDevice.transferOut(
          _pairedDevice, kEndpoint, connectPacket.buffer);
      if (usbOutTransferResult.status == StatusResponse.ok) {
        _isConnected = true;
      }
    }
  }

  Future<void> disconnect() async {
    if (_isConnected) {
      try {
        await _usbDevice.transferOut(
            _pairedDevice, kEndpoint, disconnectPacket.buffer);
        await _usbDevice.releaseInterface(_pairedDevice, kInterface);
        await _usbDevice.close(_pairedDevice);
      } catch (e) {
        if (kDebugMode) {
          print(e);
        }
      }
    }
    _isConnected = false;
  }

  Future<int> sendStandardCan(int msgId, Uint8List payload) async {
    if (_isConnected) {
      if ((msgId < 0) || (msgId >= 0x7FF)) {
        throw 'Invalid msgId value.';
      }
      if (payload.lengthInBytes > 8) {
        throw 'Payload length exceeds 8 bytes';
      }
      final int len = payload.lengthInBytes + kSizeFrameOverhead + 1 + 2 + 1;
      Uint8List webusbPacket = Uint8List(len);
      webusbPacket[0] = kTagSof;
      webusbPacket[1] = (len & 0x00FF);
      webusbPacket[2] = (len >> 8) & 0x00FF;
      webusbPacket[3] = 0; // seq not used
      webusbPacket[4] = 0;
      webusbPacket[5] = kCmdSendCan;
      webusbPacket[6] = msgId & 0x00FF;
      webusbPacket[7] = (msgId >> 8) & 0x00FF;
      webusbPacket[8] = payload.lengthInBytes;
      for (int i = 0; i < payload.lengthInBytes; i++) {
        webusbPacket[9 + i] = payload[i];
      }
      int checksum = 0;
      for (int i = 0; i < (len - 1); i++) {
        checksum += webusbPacket[i];
      }
      webusbPacket[len - 1] = (~checksum + 1) & 0x00FF;
      USBOutTransferResult usbOutTransferResult = await _usbDevice.transferOut(
          _pairedDevice, kEndpoint, webusbPacket.buffer);
      if (usbOutTransferResult.status != StatusResponse.ok) {
        throw 'USB failed to transfer out';
      }
      return usbOutTransferResult.bytesWritten;
    } else {
      throw 'Not Connected.';
    }
  }

  Future<String> receiveStandardCAN() async {
    USBInTransferResult inTransferResult =
        await _usbDevice.transferIn(_pairedDevice, kEndpoint, 64);
    print(inTransferResult.status.toString());
    if (inTransferResult.status == StatusResponse.ok) {
      print(inTransferResult.data[0].toString());
    }
    return (inTransferResult.data.toString());
  }
}


    // usbDevice = UsbDevice();
    // pairedDevice = null;

                //   print('I am pressed');
                //   isSupported = await usbDevice!.isSupported();
                //   print('Web USB supported: ' + isSupported.toString());
                //   pairedDevice = await usbDevice!.requestDevices(
                //       [DeviceFilter(vendorId: 0xCAFE, productId: 0x4011)]);
                //   USBDeviceInfo devInfo =
                //       await usbDevice!.getPairedDeviceInfo(pairedDevice);
                //   print('Product: ' + devInfo.productName);
                //   print('Manufacturer: ' + devInfo.manufacturerName);
                //   print('Version: ' +
                //       devInfo.deviceVersionMajor.toString() +
                //       '.' +
                //       devInfo.deviceVersionMinor.toString() +
                //       '.' +
                //       devInfo.deviceVersionSubMinor.toString());
                //   List<USBConfiguration> availableConfigurations =
                //       await usbDevice!.getAvailableConfigurations(pairedDevice);
                //   print('USB Configurations: ' +
                //       availableConfigurations.length.toString());
                //   int i = 0;
                //   while (i < availableConfigurations.length) {
                //     print('Configuration:' + i.toString());
                //     print(availableConfigurations[i]);
                //     i++;
                //   }
                //   var utf8encoder = Utf8Encoder();
                //   var list = utf8encoder.convert('Hello World.\r\n');

                //   await usbDevice!.open(pairedDevice);
                //   await usbDevice!.selectConfiguration(pairedDevice, 1);
                //   await usbDevice!.claimInterface(pairedDevice, 2);
                //   print('Transfer Result: ' +
                //       usbOutTransferResult.bytesWritten.toString() +
                //       ' written');
                //   print(usbOutTransferResult.status.toString());
                //   while (true) {
                //     USBInTransferResult usbInTransferResult =
                //         await usbDevice!.transferIn(pairedDevice, 3, 64);
                //     if (usbInTransferResult.status ==
                //         StatusResponse.empty_data) {
                //       break;
                //     }
                //     print(Utf8Decoder().convert(usbInTransferResult.data));
                //   }

                  // try {
                  //   if (pairedDevice != null) {
                  //     var list =
                  //         const Utf8Encoder().convert('Goodbye World.\r\n');
                  //     USBOutTransferResult usbOutTransferResult =
                  //         await usbDevice!
                  //             .transferOut(pairedDevice, 3, list.buffer);
                  //     print(usbOutTransferResult.status);
                  //     await usbDevice!.releaseInterface(pairedDevice, 2);
                  //     await usbDevice!.close(pairedDevice);
                  //     pairedDevice = null;
                  //   }
                  // } catch (e) {
                  //   print(e);
                  // }
