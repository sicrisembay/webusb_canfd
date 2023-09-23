import 'dart:typed_data';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:webusb_canfd/components/webusb.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'WebUSB CAN-FD',
      theme: ThemeData.dark(),
      home: const MyHomePage(title: 'WebUSB CAN-FD'),
    );
  }
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({super.key, required this.title});

  final String title;

  @override
  State<MyHomePage> createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  final WebUsbDevice webUsbDevice = WebUsbDevice();
  late String rxTestPacket;

  @override
  void initState() {
    /// TODO
    super.initState();
  }

  @override
  void dispose() {
    webUsbDevice.disconnect();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.title),
      ),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: <Widget>[
            Expanded(
              flex: 1,
              child: const Text(
                'Connection Test',
                style: TextStyle(fontSize: 40.0),
              ),
            ),
            Expanded(
              flex: 1,
              child: Column(children: [
                const Text(
                  'Green LED lights up.',
                ),
                TextButton(
                  onPressed: () async {
                    await webUsbDevice.connect();
                  },
                  child: Text('CONNECT'),
                ),
              ]),
            ),
            Expanded(
              flex: 1,
              child: Column(
                children: [
                  const Text('Green LED blinks.'),
                  TextButton(
                      onPressed: () async {
                        webUsbDevice.disconnect();
                      },
                      child: Text('DISCONNECT')),
                ],
              ),
            ),
            Expanded(
              flex: 1,
              child: Column(children: [
                const Text('Send Test Standard CAN packet.'),
                TextButton(
                    onPressed: () async {
                      await webUsbDevice.sendStandardCan(
                          0x123, Uint8List.fromList([1, 2, 3, 4, 5, 6, 7, 8]));
                    },
                    child: const Text('SEND TEST PACKET')),
              ]),
            ),
            Expanded(
                flex: 1,
                child: Column(
                  children: [
                    const Text('Recieve one packet'),
                    TextButton(
                        onPressed: () async {
                          rxTestPacket =
                              await webUsbDevice.receiveStandardCAN();
                          print(rxTestPacket);
                        },
                        child: Text('RECEIVE TEXT PACKET')),
                  ],
                )),
            // Expanded(
            //   flex: 10,
            //   child: Column(children: [
            //     const Text('CAN received packet'),
            //     Expanded(
            //       child: Container(
            //         padding: EdgeInsets.all(10.0),
            //         child: CustomScrollView(slivers: [
            //           SliverList(
            //             delegate: SliverChildBuilderDelegate(
            //               // The builder function returns a ListTile with a title that
            //               // displays the index of the current item.
            //               (context, index) =>
            //                   ListTile(title: Text('Item #$index')),
            //               // Builds 1000 ListTiles
            //               childCount: 1000,
            //             ),
            //           ),
            //         ]),
            //       ),
            //     ),
            //   ]),
            // ),
          ],
        ),
      ),
    );
  }
}
