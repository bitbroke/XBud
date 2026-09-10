import 'package:flutter/material.dart';

void main() {
  runApp(const EarBrainApp());
}

class EarBrainApp extends StatelessWidget {
  const EarBrainApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Ear-Brain',
      theme: ThemeData(
        brightness: Brightness.Dark,
        primarySwatch: Colors.blue,
        scaffoldBackgroundColor: const Color(0xFF121212),
        appBarTheme: const AppBarTheme(
          backgroundColor: Color(0xFF1E1E1E),
          elevation: 0,
        ),
      ),
      home: const DashboardScreen(),
    );
  }
}

class DashboardScreen extends StatefulWidget {
  const DashboardScreen({super.key});

  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  // Stub for BLE connection state
  bool _isConnected = false;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Ear-Brain', style: TextStyle(fontWeight: FontWeight.bold)),
        actions: [
          Padding(
            padding: const EdgeInsets.all(16.0),
            child: Icon(
              _isConnected ? Icons.bluetooth_connected : Icons.bluetooth_disabled,
              color: _isConnected ? Colors.blue : Colors.grey,
            ),
          )
        ],
      ),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(Icons.earbuds, size: 100, color: Colors.blueGrey),
            const SizedBox(height: 24),
            Text(
              _isConnected ? 'Connected to Case' : 'Looking for Ear-Brain Case...',
              style: const TextStyle(fontSize: 18),
            ),
            const SizedBox(height: 48),
            if (!_isConnected)
              ElevatedButton(
                onPressed: () {
                  setState(() => _isConnected = true);
                },
                child: const Text('Connect Simulator'),
              )
            else
              ElevatedButton(
                onPressed: () {
                  Navigator.push(
                    context,
                    MaterialPageRoute(builder: (context) => const CallLogScreen()),
                  );
                },
                child: const Text('View Call Logs'),
              )
          ],
        ),
      ),
    );
  }
}

class CallLogScreen extends StatelessWidget {
  const CallLogScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Recent Calls')),
      body: ListView(
        children: const [
          ListTile(
            leading: Icon(Icons.phone_callback, color: Colors.green),
            title: Text('+91 98765 43210'),
            subtitle: Text('Today, 2:30 PM • 14 mins'),
            trailing: Icon(Icons.chevron_right),
          ),
          ListTile(
            leading: Icon(Icons.phone_forwarded, color: Colors.blue),
            title: Text('Rohan Sharma (Vendor)'),
            subtitle: Text('Yesterday, 11:15 AM • 8 mins'),
            trailing: Icon(Icons.chevron_right),
          ),
        ],
      ),
    );
  }
}
