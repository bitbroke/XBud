import subprocess
import urllib.request
import urllib.parse
import json
import base64
import time
import re
import sys

print("Starting SSH tunnel (Pinggy)...")
process = subprocess.Popen(
    ["ssh", "-p", "443", "-R0:localhost:5050", "a.pinggy.io", "-o", "StrictHostKeyChecking=no"],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True
)

public_url = None
# Pinggy/localhost.run outputs to stderr or stdout depending on version, so we check both or just read a few lines
for _ in range(15):
    line = process.stdout.readline()
    if not line:
        line = process.stderr.readline()
    print("SSH Output:", line.strip())
    
    match = re.search(r'(https?://[a-zA-Z0-9.-]+\.lhr\.life)', line)
    if match:
        public_url = match.group(1)
        break
    
    match2 = re.search(r'(https?://[a-zA-Z0-9.-]+\.pinggy\.link)', line)
    if match2:
        public_url = match2.group(1)
        break
    
    time.sleep(1)

if not public_url:
    print("Failed to get public URL from SSH tunnel.")
    process.terminate()
    sys.exit(1)

print(f"Got Public URL: {public_url}")

import os

account_sid = os.environ.get('TWILIO_ACCOUNT_SID', 'YOUR_TWILIO_ACCOUNT_SID')
auth_token = os.environ.get('TWILIO_AUTH_TOKEN', 'YOUR_TWILIO_AUTH_TOKEN')
phone_number_sid = os.environ.get('TWILIO_PHONE_NUMBER_SID', 'YOUR_PHONE_NUMBER_SID')
voice_url = f'{public_url}/answer'

# Configure Twilio
print("Configuring Twilio Webhook...")
b64_auth = base64.b64encode(f"{account_sid}:{auth_token}".encode()).decode()
update_url = f'https://api.twilio.com/2010-04-01/Accounts/{account_sid}/IncomingPhoneNumbers/{phone_number_sid}.json'
update_data = urllib.parse.urlencode({'VoiceUrl': voice_url, 'VoiceMethod': 'POST'}).encode()

update_req = urllib.request.Request(update_url, data=update_data, method='POST')
update_req.add_header('Authorization', f'Basic {b64_auth}')
update_req.add_header('Content-Type', 'application/x-www-form-urlencoded')

try:
    urllib.request.urlopen(update_req)
except Exception as e:
    print("Failed to configure webhook:", e)

# Trigger Call
print("Triggering outbound call to your phone...")
call_url = f'https://api.twilio.com/2010-04-01/Accounts/{account_sid}/Calls.json'
call_data = urllib.parse.urlencode({
    'To': os.environ.get('TO_PHONE_NUMBER', '+1234567890'), 
    'From': os.environ.get('TWILIO_PHONE_NUMBER', '+1234567890'), 
    'Url': voice_url
}).encode()

call_req = urllib.request.Request(call_url, data=call_data, method='POST')
call_req.add_header('Authorization', f'Basic {b64_auth}')
call_req.add_header('Content-Type', 'application/x-www-form-urlencoded')

try:
    with urllib.request.urlopen(call_req) as f:
        print('SUCCESS: ' + f.read().decode())
except Exception as e:
    print('Failed to trigger call:', e)

print("Tunnel is open and call is initiated. Press Ctrl+C to exit.")
try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    process.terminate()
