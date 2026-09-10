import subprocess
import time
import re

process = subprocess.Popen(
    ["ssh", "-p", "443", "-R0:localhost:5050", "a.pinggy.io", "-o", "StrictHostKeyChecking=no"],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True
)

public_url = None
for _ in range(15):
    line = process.stdout.readline()
    if not line:
        line = process.stderr.readline()
    
    match = re.search(r'(https?://[a-zA-Z0-9.-]+\.lhr\.life)', line)
    if match:
        public_url = match.group(1)
        break
    match2 = re.search(r'(https?://[a-zA-Z0-9.-]+\.pinggy\.link)', line)
    if match2:
        public_url = match2.group(1)
        break
    time.sleep(1)

if public_url:
    with open("public_link.txt", "w") as f:
        f.write(public_url)
    
    # keep alive
    try:
        while True:
            time.sleep(10)
    except:
        pass
