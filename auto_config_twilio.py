import os
import urllib.request
import urllib.parse
import base64

account_sid = os.environ.get('TWILIO_ACCOUNT_SID', 'YOUR_TWILIO_ACCOUNT_SID')
auth_token = os.environ.get('TWILIO_AUTH_TOKEN', 'YOUR_TWILIO_AUTH_TOKEN')
phone_number_sid = os.environ.get('TWILIO_PHONE_NUMBER_SID', 'YOUR_PHONE_NUMBER_SID')
voice_url = 'https://e4912b2d5e1cb4.lhr.life/answer'

url = f'https://api.twilio.com/2010-04-01/Accounts/{account_sid}/IncomingPhoneNumbers/{phone_number_sid}.json'
data = urllib.parse.urlencode({'VoiceUrl': voice_url, 'VoiceMethod': 'POST'}).encode()

req = urllib.request.Request(url, data=data, method='POST')
b64_auth = base64.b64encode(f"{account_sid}:{auth_token}".encode()).decode()
req.add_header('Authorization', f'Basic {b64_auth}')
req.add_header('Content-Type', 'application/x-www-form-urlencoded')

try:
    with urllib.request.urlopen(req) as response:
        print("Successfully updated Twilio webhook to:", voice_url)
except Exception as e:
    print("Error:", e)
