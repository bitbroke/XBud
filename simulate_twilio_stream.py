import asyncio
import websockets
import json
import base64
import time
import urllib.request

async def test_websocket():
    uri = "ws://localhost:5050/stream"
    print(f"Connecting to {uri}...")
    try:
        async with websockets.connect(uri) as websocket:
            print("Connected! Sending mock audio data...")
            
            # Send 3 seconds of dummy audio (24000 bytes of mu-law)
            # We mock the payload by sending silence or just random bytes 
            # since the server gracefully mocks the AI if dependencies are missing.
            dummy_payload = base64.b64encode(b'\x00' * 8000).decode('utf-8')
            
            for _ in range(5):
                message = {
                    "event": "media",
                    "media": {
                        "payload": dummy_payload
                    }
                }
                await websocket.send(json.dumps(message))
                print("Sent audio chunk...")
                await asyncio.sleep(1)
            
            # Send stop event
            await websocket.send(json.dumps({"event": "stop"}))
            print("Test complete. Check your dashboard!")
            
    except Exception as e:
        print(f"Connection failed: {e}")

asyncio.run(test_websocket())
