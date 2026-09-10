import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/ble_service.dart';

class DashboardScreen extends StatelessWidget {
  const DashboardScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final bleService = context.watch<BleService>();

    return Scaffold(
      appBar: AppBar(
        title: const Text('EAR-BRAIN'),
        actions: [
          IconButton(
            icon: Icon(
              bleService.isConnected ? Icons.bluetooth_connected : Icons.bluetooth_disabled,
              color: bleService.isConnected ? const Color(0xFF00E5FF) : Colors.grey,
            ),
            onPressed: () {},
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            // Status Card
            Card(
              child: Padding(
                padding: const EdgeInsets.all(20.0),
                child: Column(
                  children: [
                    const Icon(Icons.earbuds, size: 64, color: Color(0xFF00E5FF)),
                    const SizedBox(height: 16),
                    Text(
                      bleService.deviceStatus,
                      style: const TextStyle(fontSize: 16, color: Colors.white70),
                    ),
                    if (!bleService.isConnected) ...[
                      const SizedBox(height: 20),
                      ElevatedButton.icon(
                        onPressed: bleService.connectMock,
                        icon: const Icon(Icons.bluetooth),
                        label: const Text("Pair Device"),
                      )
                    ]
                  ],
                ),
              ),
            ),
            
            const SizedBox(height: 24),
            
            // Intelligence Feed
            const Text(
              "INTELLIGENCE FEED",
              style: TextStyle(
                fontSize: 14, 
                fontWeight: FontWeight.bold, 
                color: Colors.white54,
                letterSpacing: 1.5,
              ),
            ),
            const SizedBox(height: 12),
            
            Expanded(
              child: bleService.actionItems.isEmpty
                  ? Center(
                      child: Text(
                        bleService.isConnected 
                            ? "Waiting for next call..." 
                            : "Connect case to sync data",
                        style: const TextStyle(color: Colors.white30),
                      ),
                    )
                  : ListView.builder(
                      itemCount: bleService.actionItems.length,
                      itemBuilder: (context, index) {
                        final item = bleService.actionItems[index];
                        return Card(
                          margin: const EdgeInsets.only(bottom: 12),
                          child: ListTile(
                            leading: CircleAvatar(
                              backgroundColor: item['priority'] == 'high' 
                                  ? Colors.redAccent.withOpacity(0.2) 
                                  : Colors.blueAccent.withOpacity(0.2),
                              child: Icon(
                                Icons.assignment_turned_in,
                                color: item['priority'] == 'high' ? Colors.redAccent : Colors.blueAccent,
                              ),
                            ),
                            title: Text(item['what']),
                            subtitle: Text("Assignee: ${item['who']} • Due: ${item['when']}"),
                            trailing: const Icon(Icons.chevron_right, color: Colors.white30),
                          ),
                        );
                      },
                    ),
            ),
          ],
        ),
      ),
    );
  }
}
