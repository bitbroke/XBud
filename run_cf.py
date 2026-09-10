import subprocess
import re
import time
import os

print("Starting cloudflared tunnel...")
# Run cloudflared
p = subprocess.Popen(['cloudflared.exe', 'tunnel', '--url', 'http://localhost:5050'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

url = None
start = time.time()
while time.time() - start < 20:
    line = p.stdout.readline()
    if not line:
        break
    match = re.search(r'(https://[a-zA-Z0-9.-]+\.trycloudflare\.com)', line)
    if match:
        url = match.group(1)
        break

if url:
    with open('cf_url.txt', 'w') as f:
        f.write(url)
    print("Got Cloudflare URL:", url)
    try:
        while True:
            time.sleep(10)
    except:
        p.terminate()
else:
    with open('cf_url.txt', 'w') as f:
        f.write("FAILED")
    print("Failed to get Cloudflare URL")
    p.terminate()
