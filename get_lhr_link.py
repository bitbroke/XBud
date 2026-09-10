import subprocess
import time
import re

process = subprocess.Popen(
    ["ssh", "-R", "80:localhost:5050", "nokey@localhost.run", "-o", "StrictHostKeyChecking=accept-new"],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True
)

public_url = None
for _ in range(15):
    line = process.stdout.readline()
    if not line:
        line = process.stderr.readline()
    
    print(f"[SSH]: {line.strip()}")
    match = re.search(r'(https?://[a-zA-Z0-9.-]+\.lhr\.life)', line)
    if match:
        public_url = match.group(1)
        break
    time.sleep(1)

if public_url:
    with open("my_url.txt", "w") as f:
        f.write(public_url)
    try:
        while True:
            time.sleep(10)
    except:
        pass
