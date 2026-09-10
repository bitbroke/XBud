import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'dart:convert';
import 'dart:async';

class BleService extends ChangeNotifier {
  // Service UUIDs matching the C++ orbitd daemon
  final String serviceUuid = "0000EB01-0000-1000-8000-00805f9b34fb";
  final String charActionItems = "0000EB11-0000-1000-8000-00805f9b34fb";
  
  bool _isConnected = false;
  bool get isConnected => _isConnected;

  String _deviceStatus = "Disconnected";
  String get deviceStatus => _deviceStatus;

  List<Map<String, dynamic>> _actionItems = [];
  List<Map<String, dynamic>> get actionItems => _actionItems;

  void connectMock() {
    _deviceStatus = "Connecting to Ear-Brain Case...";
    notifyListeners();
    
    // Simulate connection delay
    Timer(const Duration(seconds: 2), () {
      _isConnected = true;
      _deviceStatus = "Connected (Battery: 85%)";
      
      // Simulate receiving JSON action item from the case's Gemma 1B model
      _simulateIncomingActionItem();
      notifyListeners();
    });
  }

  void _simulateIncomingActionItem() {
    Timer(const Duration(seconds: 5), () {
      final String simulatedJson = '''
      [
        {
          "who": "Rahul",
          "what": "Send the Q3 financial report",
          "when": "Tomorrow",
          "priority": "high",
          "confidence": 0.95
        }
      ]
      ''';
      
      List<dynamic> parsed = jsonDecode(simulatedJson);
      _actionItems.addAll(parsed.cast<Map<String, dynamic>>());
      notifyListeners();
    });
  }

  void toggleConsent() {
    // In production, writes to CHAR_CONSENT_STATE (0xEB21)
    debugPrint("Toggled DPDP Consent State on Hardware");
  }
}
