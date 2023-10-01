import 'dart:typed_data';
import 'dart:ui';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:webusb_canfd/components/webusb.dart';
import 'package:webusb_canfd/screens/connect_sreen.dart';
import 'package:webusb_canfd/screens/main_screen.dart';

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
      scrollBehavior: AppCustomScrollBehavior(),
      initialRoute: ConnectScreen.id,
      routes: {
        ConnectScreen.id: (context) => const ConnectScreen(),
        MainScreen.id: (context) => const MainScreen()
      },
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
          ],
        ),
      ),
    );
  }
}

class AppCustomScrollBehavior extends MaterialScrollBehavior {
  @override
  Set<PointerDeviceKind> get dragDevices => {
        PointerDeviceKind.touch,
        PointerDeviceKind.mouse,
        PointerDeviceKind.trackpad,
      };
}
