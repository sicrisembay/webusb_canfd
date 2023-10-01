import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:webusb_canfd/components/can_message.dart';

const int kUsbBytesInPacket = 1;
/*
 * Frame Format
 *   TAG        : 1 byte
 *   Length     : 2 bytes
 *   Packet Seq : 2 bytes
 *   Payload    : N Bytes
 *   Checksum   : 1 byte
 */
// Size in bytes
const int kSizeTagSof = 1;
const int kSizeLength = 2;
const int kSizePacktSeq = 2;
const int kSizeCheckSum = 1;
const int kSizeFrameOverhead =
    kSizeTagSof + kSizeLength + kSizePacktSeq + kSizeCheckSum;

const int kTagSof = 0xff;
const int kCmdConnect = 0x01;
const int kCmdHostToDevice = 0x10;
const int kCmdDeviceToHostCanStandard = 0x20;
const int kCmdDeviceToHostCanExtended = 0x21;
const int kCmdDeviceToHostFdStandard = 0x22;
const int kCmdDeviceToHostFdExtended = 0x23;

const int _bufferSize = 1024;

final Uint8List connectPacket = Uint8List.fromList(
    [0x08, kTagSof, 0x08, 0x00, 0x00, 0x00, kCmdConnect, 0x01, 0xF7]);
final Uint8List disconnectPacket = Uint8List.fromList(
    [0x08, kTagSof, 0x08, 0x00, 0x00, 0x00, kCmdConnect, 0x02, 0xF6]);

class FrameParser {
  final Uint8List _buffer = Uint8List(_bufferSize);
  final _controller = StreamController<CanMessage>.broadcast();
  int _wrPtr = 0;
  int _rdPtr = 0;

  FrameParser() {}

  Stream<CanMessage> get stream => _controller.stream;

  void addNProcess(Uint8List data) {
    int availableBytes = 0;
    int length = 0;
    int sum = 0;

    if (data.length <= 1) {
      return;
    }

    final int count = data[0];
    if (count == 0) {
      return;
    }

    /* Add to buffer */
    for (int i = 0; i < count; i++) {
      int tmp = (_wrPtr + 1) % _bufferSize;
      if (tmp == _rdPtr) {
        // full
        break;
      }
      _buffer[_wrPtr] = data[1 + i];
      _wrPtr = tmp;
    }

    /* Process Buffer */
    while (_wrPtr != _rdPtr) {
      /* Check start of command TAG */
      if (kTagSof != _buffer[_rdPtr]) {
        // Skip character
        _rdPtr = (_rdPtr + 1) % _bufferSize;
        continue;
      }

      /* Get available bytes in the buffer */
      if (_wrPtr >= _rdPtr) {
        availableBytes = _wrPtr - _rdPtr;
      } else {
        availableBytes = (_wrPtr + _bufferSize) - _rdPtr;
      }
      if (availableBytes < kSizeFrameOverhead) {
        /* Can't be less than frame overhead */
        break;
      }
      // See if the packet size byte is valid.  A command packet must be at
      // least four bytes and can not be larger than the receive buffer size.
      Uint8List listLength = Uint8List(2);
      listLength[0] = _buffer[(_rdPtr + 1) % _bufferSize];
      listLength[1] = _buffer[(_rdPtr + 2) % _bufferSize];
      length = listLength.buffer.asByteData().getUint16(0, Endian.little);

      if ((length < kSizeFrameOverhead) || (length > (_bufferSize - 1))) {
        // The packet size is too large, so either this is not the start of
        // a packet or an invalid packet was received.  Skip this start of
        // command packet tag.
        _rdPtr = (_rdPtr + 1) % _bufferSize;

        // Keep scanning for a start of command packet tag.
        continue;
      }

      // If the entire command packet is not in the receive buffer then stop
      if (availableBytes < length) {
        break;
      }

      // The entire command packet is in the receive buffer, so compute its
      // checksum.
      sum = 0;
      for (int idx = 0; idx < length; idx++) {
        sum += _buffer[(_rdPtr + idx) % _bufferSize];
      }

      // Skip this packet if the checksum is not correct (that is, it is
      // probably not really the start of a packet).
      if ((sum & 0xFF) != 0) {
        // Skip this character
        _rdPtr = (_rdPtr + 1) % _bufferSize;
        // Keep scanning for a start of command packet tag.
        continue;
      }

      // A valid command packet was received, so process it now.
      _processValidFrame(_rdPtr, length);

      // Done with processing this command packet.
      _rdPtr = (_rdPtr + length) % _bufferSize;
    }
  }

  void _processValidFrame(int index, int len) {
    if (len < kSizeFrameOverhead) {
      return;
    }
    /* Remove overhead from frame */
    index = (index + kSizeTagSof + kSizeLength + kSizePacktSeq) % _bufferSize;
    len = len - kSizeFrameOverhead;

    int cmd = _buffer[index];
    switch (cmd) {
      case kCmdDeviceToHostCanStandard:
        {
          Uint8List listMsgId = Uint8List(4);
          listMsgId[0] = _buffer[(index + 1) % _bufferSize];
          listMsgId[1] = _buffer[(index + 2) % _bufferSize];
          listMsgId[2] = _buffer[(index + 3) % _bufferSize];
          listMsgId[3] = _buffer[(index + 4) % _bufferSize];
          int msgId = listMsgId.buffer.asByteData().getUint32(0, Endian.little);
          int dlc = _buffer[(index + 5) % _bufferSize];
          Uint8List payload = Uint8List(dlc);
          for (int i = 0; i < dlc; i++) {
            int indexPayload = (index + 6 + i) % _bufferSize;
            payload[i] = _buffer[indexPayload];
          }

          _controller.sink.add(CanMessage(
              id: msgId,
              canType: CanType.CAN,
              idType: CanIdType.BASE,
              length: dlc,
              data: payload));

          if (kDebugMode) {
            print('msgId: ' + msgId.toString());
            print('dlc: ' + dlc.toString());
            print(payload);
          }
          break;
        }
      default:
        {
          print('Unknown command');
          break;
        }
    }
  }
}
