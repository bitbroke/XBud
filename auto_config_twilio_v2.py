import os
import urllib.request
import json
import base64

account_sid = os.environ.get('TWILIO_ACCOUNT_SID', 'YOUR_TWILIO_ACCOUNT_SID')
auth_token = os.environ.get('TWILIO_AUTH_TOKEN', 'YOUR_TWILIO_AUTH_TOKEN')
url = f'https://api.twilio.com/2010-04-01/Accounts/{account_sid}/IncomingPhoneNumbers.json'

req = urllib.request.Request(url)
req.add_header('Authorization', f'Basic {base64.b64encode(f"{account_sid}:{auth_token}".encode()).decode()}')

try:
    response = urllib.request.urlopen(req)
    data = json.loads(response.read().decode())
    
    if data['incoming_phone_numbers']:
        number_sid = data['incoming_phone_numbers'][0]['sid']
        print("Found SID:", number_sid)
        
        # Now update it
        voice_url = 'https://e4912b2d5e1cb4.lhr.life/answer'
        update_url = f'https://api.twilio.com/2010-04-01/Accounts/{account_sid}/IncomingPhoneNumbers/{number_sid}.json'
        update_data = urllib.parse.urlencode({'VoiceUrl': voice_url, 'VoiceMethod': 'POST'}).encode()
        
        update_req = urllib.request.Request(update_url, data=update_data, method='POST')
        update_req.add_header('Authorization', f'Basic {base64.b64encode(f"{account_sid}:{auth_token}".encode()).decode()}')
        update_req.add_header('Content-Type', 'application/x-www-form-urlencoded')
        
        urllib.request.urlopen(update_req)
        print("Successfully configured!")
    else:
        print("No incoming phone numbers found.")
except Exception as e:
    print("Error:", e)
