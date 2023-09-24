import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:isolate_manager/isolate_manager.dart';
import 'package:usb_device/usb_device.dart';
import 'command_frame.dart';

const int kVendor = 0xCAFE;
const int kProduct = 0x4011;
const int kConfiguration = 1;
const int kInterface = 2;
const int kEndpoint = 3;

final UsbDevice _usbDevice = UsbDevice();
final FrameParser frameParser = FrameParser();
late dynamic _pairedDevice;
bool _isConnected = false;
bool _isConnectedPrevious = false;

@pragma('vm:entry-point')
void isolateReceiveUSB(dynamic params) async {
  final channel = IsolateManagerController(params);

  while (true) {
    if (_isConnected) {
      if (!_isConnectedPrevious) {
        print('WebUSB connected');
      }
      USBInTransferResult inTransferResult =
          await _usbDevice.transferIn(_pairedDevice, kEndpoint, 64);
      if (inTransferResult.status == StatusResponse.ok) {
        if (inTransferResult.data[0] == 0) {
          /* received dummy */
          await Future.delayed(const Duration(milliseconds: 10));
        } else {
          frameParser.addNProcess(inTransferResult.data);
        }
      } else if (inTransferResult.status == StatusResponse.stall) {
        await _usbDevice.clearHalt(_pairedDevice, 'in', kEndpoint);
      } else {
        print(inTransferResult.status);
      }
      _isConnectedPrevious = true;
    } else {
      if (_isConnectedPrevious) {
        _isConnectedPrevious = false;
        await _usbDevice.transferOut(
            _pairedDevice, kEndpoint, disconnectPacket.buffer);
        await _usbDevice.releaseInterface(_pairedDevice, kInterface);
        await _usbDevice.close(_pairedDevice);
        print('WebUSB disconnected');
      }
      await Future.delayed(const Duration(seconds: 1));
    }
  }
}

class WebUsbDevice {
  final isolateManager =
      IsolateManager.createOwnIsolate(isDebug: true, isolateReceiveUSB);
  bool _isSupported = false;

  WebUsbDevice() {
    _usbDevice.setOnConnectionCallback((p0) async {
      print('setOnConnectionCallback called');
    });
    _usbDevice.setOnDisconnectCallback((p0) {
      _isConnected = false;
      print('setOnDisconnectCallback called. _isConnected: ' +
          _isConnected.toString());
    });
    isolateManager.start();
    isolateManager.stream.listen((message) {
      print('Received from isolate: ' + message);
    });
  }

  bool isConnected() {
    return _isConnected;
  }

  Future<void> getDevices() async {}

  Future<void> dispose() async {
    await isolateManager.stop();
  }

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
        await isolateManager.sendMessage('test-message');
      }
    }
  }

  Future<void> disconnect() async {
    if (_isConnected) {
      try {
        _isConnected = false;
        // await _usbDevice.transferOut(
        //     _pairedDevice, kEndpoint, disconnectPacket.buffer);
        // await _usbDevice.releaseInterface(_pairedDevice, kInterface);
        // await _usbDevice.close(_pairedDevice);
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
      final int len = payload.lengthInBytes +
          kSizeFrameOverhead +
          1 + // cmd
          2 + // msgID
          1; // dlc
      Uint8List webusbPacket = Uint8List(len + 1); // +1 for bytes in USB packet
      webusbPacket[0] = len;
      webusbPacket[1] = kTagSof;
      webusbPacket[2] = (len & 0x00FF);
      webusbPacket[3] = (len >> 8) & 0x00FF;
      webusbPacket[4] = 0; // seq not used
      webusbPacket[5] = 0;
      webusbPacket[6] = kCmdHostToDevice;
      webusbPacket[7] = msgId & 0x00FF;
      webusbPacket[8] = (msgId >> 8) & 0x00FF;
      webusbPacket[9] = payload.lengthInBytes;
      for (int i = 0; i < payload.lengthInBytes; i++) {
        webusbPacket[10 + i] = payload[i];
      }
      int checksum = 0;
      for (int i = 0; i < (len - 1); i++) {
        checksum += webusbPacket[i + 1];
      }
      webusbPacket[len] = (~checksum + 1) & 0x00FF;
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
}
