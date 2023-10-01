import 'package:flutter/material.dart';
import '../constants.dart';

class BottomButton extends StatelessWidget {
  final VoidCallback onTap;
  final String label;

  BottomButton({required this.label, required this.onTap});

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: onTap,
      child: Container(
        child: Text(
          label,
          style: kLargeButtonTextStyle,
        ),
        alignment: Alignment.center,
        margin: EdgeInsets.only(top: 10.0),
        color: kBottomContainerColor,
        width: double.infinity,
        height: kBottomContainerHeight,
      ),
    );
  }
}
