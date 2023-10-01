import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_expandable_table/flutter_expandable_table.dart';
import 'package:webusb_canfd/components/bottom_button.dart';
import 'package:webusb_canfd/components/can_message.dart';
import 'package:webusb_canfd/globals.dart';
import 'package:webusb_canfd/screens/connect_sreen.dart';

class MainScreen extends StatefulWidget {
  const MainScreen({super.key});
  static const String id = 'MainScreen';

  @override
  State<MainScreen> createState() => _MainScreenState();
}

class _MainScreenState extends State<MainScreen> {
  late ExpandableTableController controller;
  Map<int, CanMessageStat> canMap = {};

  _buildCell(String content, {CellBuilder? builder}) {
    return ExpandableTableCell(
        child: (builder != null)
            ? null
            : DefaultCellCard(
                child: Center(child: Text(content)),
              ));
  }

  @override
  void initState() {
    // Header
    List<ExpandableTableHeader> headers = [
      ExpandableTableHeader(cell: _buildCell('Type'), width: 70.0),
      ExpandableTableHeader(cell: _buildCell('Length'), width: 70.0),
      ExpandableTableHeader(cell: _buildCell('Cycle Time'), width: 100.0),
      ExpandableTableHeader(cell: _buildCell('Count'), width: 100.0),
      ExpandableTableHeader(cell: _buildCell('Data'), width: 250.0),
    ];
    controller = ExpandableTableController(
        firstHeaderCell: _buildCell('CAN-ID'),
        firstColumnWidth: 50.0,
        headerHeight: 30.0,
        headers: headers,
        rows: []);

    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(20.0),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            mainAxisAlignment: MainAxisAlignment.center,
            children: <Widget>[
              Row(
                children: [
                  const Text('Receive',
                      style: TextStyle(
                          fontSize: 20.0, fontWeight: FontWeight.bold)),
                  TextButton(
                      onPressed: () {
                        canMap.clear();
                      },
                      child: const Text('RESET'))
                ],
              ),
              const SizedBox(
                height: 20.0,
              ),
              StreamBuilder<CanMessage>(
                  stream: frameParser.stream,
                  builder: (context, snapshot) {
                    switch (snapshot.connectionState) {
                      case ConnectionState.active:
                        {
                          CanMessage? canMsg = snapshot.data;
                          CanMessageStat canStat;
                          if (canMsg != null) {
                            canStat = CanMessageStat(canMsg: canMsg);
                            CanMessageStat? old = canMap[canMsg.id];
                            if (old != null) {
                              /* already found in map */
                              canStat.count = old.count + 1;
                            } else {
                              canStat.count = 1;
                            }
                            canMap[canMsg.id] = canStat;
                          }
                          List<int> keys = canMap.keys.toList();
                          final List<ExpandableTableRow> rows =
                              List<ExpandableTableRow>.generate(
                                  keys.length,
                                  (index) => ExpandableTableRow(
                                        firstCell: _buildCell('${keys[index]}'),
                                        cells: [
                                          _buildCell(
                                              '${canMap[keys[index]]?.canMsg.canType.name}'),
                                          _buildCell(
                                              '${canMap[keys[index]]?.canMsg.length}'),
                                          _buildCell('0'),
                                          _buildCell(
                                              '${canMap[keys[index]]?.count}'),
                                          _buildCell(
                                              '${canMap[keys[index]]?.canMsg.data.toString()}'),
                                        ],
                                      ));
                          controller.rows = rows;
                          break;
                        }
                      default:
                        {
                          break;
                        }
                    }
                    return Expanded(
                        flex: 4,
                        child: ExpandableTable(
                          controller: controller,
                        ));
                  }),
              Expanded(
                flex: 1,
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    const Text('Transmit',
                        style: TextStyle(
                            fontSize: 20.0, fontWeight: FontWeight.bold)),
                    TextButton(
                        onPressed: () async {
                          await webUsbDevice.sendStandardCan(0x123,
                              Uint8List.fromList([1, 2, 3, 4, 5, 6, 7, 8]));
                        },
                        child: Text('SEND TEST PACKET'))
                  ],
                ),
              ),
              const SizedBox(
                height: 20.0,
              ),
              BottomButton(
                  label: 'DISCONNECT',
                  onTap: () async {
                    await webUsbDevice.disconnect();
                    if (Navigator.canPop(context)) {
                      Navigator.pop(context);
                    } else {
                      Navigator.pushNamed(context, ConnectScreen.id);
                    }
                  }),
            ],
          ),
        ),
      ),
    );
  }
}

class DefaultCellCard extends StatelessWidget {
  final Widget child;

  const DefaultCellCard({Key? key, required this.child}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.all(1),
      child: child,
    );
  }
}

class CanMessageStat {
  int count = 0;
  final CanMessage canMsg;

  CanMessageStat({required this.canMsg});
}
