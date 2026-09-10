import subprocess
import re
import time
import sys

print("Starting localhost.run tunnel...")
process = subprocess.Popen(
    ["ssh", "-R", "80:localhost:5050", "nokey@localhost.run", "-o", "StrictHostKeyChecking=accept-new"],
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True
)

url = None
for _ in range(15):
    line = process.stdout.readline()
    if not line:
        break
    match = re.search(r'(https://[a-zA-Z0-9.-]+\.lhr\.life)', line)
    if match:
        url = match.group(1)
        break

if url:
    with open("stable_url.txt", "w") as f:
        f.write(url)
    print(f"Tunnel established: {url}")
    # Keep process alive
    try:
        while True:
            time.sleep(10)
    except KeyboardInterrupt:
        process.terminate()
else:
    with open("stable_url.txt", "w") as f:
        f.write("FAILED")
    print("Failed to establish tunnel.")
    process.terminate()
