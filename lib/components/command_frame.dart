import 'dart:typed_data';

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
const int kCmdSendCan = 0x10;

final Uint8List connectPacket = Uint8List.fromList(
    [kTagSof, 0x08, 0x00, 0x00, 0x00, kCmdConnect, 0x01, 0xF7]);
final Uint8List disconnectPacket = Uint8List.fromList(
    [kTagSof, 0x08, 0x00, 0x00, 0x00, kCmdConnect, 0x02, 0xF6]);
