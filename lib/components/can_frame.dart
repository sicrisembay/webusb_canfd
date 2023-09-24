import 'dart:typed_data';

class CanFrame {
  final int msgId;
  final int dlc;
  final Uint8List payload;

  CanFrame({required this.msgId, required this.dlc, required this.payload});
}
