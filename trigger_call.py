import subprocess
import urllib.request
import urllib.parse
import json
import time
import re

print("Starting localtunnel...")
# Start localtunnel as a subprocess
process = subprocess.Popen(
    ["npx", "localtunnel", "--port", "5050"],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True,
    shell=True
)

# Read the URL from localtunnel output
localtunnel_url = None
for i in range(10):
    line = process.stdout.readline()
    if "your url is:" in line:
        localtunnel_url = line.split("your url is:")[1].strip()
        break
    time.sleep(1)

if not localtunnel_url:
    print("Failed to start localtunnel. Ensure you have internet access.")
    process.terminate()
    exit(1)

print(f"Localtunnel is running at: {localtunnel_url}")

import os

# Trigger Twilio Call
account_sid = os.environ.get("TWILIO_ACCOUNT_SID", "YOUR_TWILIO_ACCOUNT_SID")
auth_token = os.environ.get("TWILIO_AUTH_TOKEN", "YOUR_TWILIO_AUTH_TOKEN")

url = f"https://api.twilio.com/2010-04-01/Accounts/{account_sid}/Calls.json"
data = urllib.parse.urlencode({
    "To": os.environ.get("TO_PHONE_NUMBER", "+1234567890"),
    "From": os.environ.get("TWILIO_PHONE_NUMBER", "+1234567890"),
    "Url": f"{localtunnel_url}/answer"
}).encode("utf-8")

# Setup Basic Auth
import base64
auth_str = f"{account_sid}:{auth_token}"
b64_auth = base64.b64encode(auth_str.encode()).decode()

req = urllib.request.Request(url, data=data, method="POST")
req.add_header("Authorization", f"Basic {b64_auth}")
req.add_header("Content-Type", "application/x-www-form-urlencoded")

print("Ringing your phone...")
try:
    with urllib.request.urlopen(req) as response:
        resp_data = json.loads(response.read().decode())
        print(f"Call initiated successfully! Status: {resp_data.get('status')}")
except Exception as e:
    print(f"Failed to initiate call: {e}")

# Keep the script running so localtunnel stays alive for the call duration
print("Waiting for the call to finish. Press Ctrl+C to stop.")
try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    process.terminate()
