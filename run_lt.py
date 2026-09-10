import subprocess
import re
import time
import sys

p = subprocess.Popen(['npx.cmd', 'localtunnel', '--port', '5050', '--local-host', '127.0.0.1'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, shell=True)
url = None
start = time.time()

while time.time() - start < 15:
    line = p.stdout.readline()
    print("LT OUT:", line.strip())
    match = re.search(r'(https://[a-zA-Z0-9.-]+\.loca\.lt)', line)
    if match:
        url = match.group(1)
        break

if url:
    with open('lt_url.txt', 'w') as f:
        f.write(url)
    print("Got LT URL:", url)
    try:
        while True:
            time.sleep(10)
    except:
        p.terminate()
else:
    with open('lt_url.txt', 'w') as f:
        f.write("FAILED")
    print("Failed to get LT URL")
    p.terminate()
