import 'dart:typed_data';

const String kStrCan = 'CAN';
const String kStrCanFd = 'CAN-FD';

enum CanType { CAN, CANFD }

enum CanIdType { BASE, EXTENDED }

enum CanNominalRate { NRATE250, NRATE500, NRATE1000 }

class CanMessage {
  final int id;
  final CanType canType;
  final CanIdType idType;
  final int length;
  final Uint8List data;

  CanMessage(
      {required this.id,
      required this.canType,
      required this.idType,
      required this.length,
      required this.data});
}
