import os
import urllib.request, urllib.parse, base64, urllib.error, json

account_sid = os.environ.get('TWILIO_ACCOUNT_SID', 'YOUR_TWILIO_ACCOUNT_SID')
auth_token = os.environ.get('TWILIO_AUTH_TOKEN', 'YOUR_TWILIO_AUTH_TOKEN')
url = f'https://api.twilio.com/2010-04-01/Accounts/{account_sid}/Calls.json'

data = urllib.parse.urlencode({
    'To': os.environ.get('TO_PHONE_NUMBER', '+1234567890'), 
    'From': os.environ.get('TWILIO_PHONE_NUMBER', '+1234567890'), 
    'Url': 'https://e4912b2d5e1cb4.lhr.life/answer'
}).encode()

req=urllib.request.Request(url, data=data, method='POST')
req.add_header('Authorization', f'Basic {base64.b64encode(f"{account_sid}:{auth_token}".encode()).decode()}')

try:
    with urllib.request.urlopen(req) as f:
        print('SUCCESS: ' + f.read().decode())
except urllib.error.HTTPError as e:
    print('ERROR: ' + e.read().decode())
