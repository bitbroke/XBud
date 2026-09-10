import subprocess
import re
import time
import sys

print("Starting pinggy tunnel...")
process = subprocess.Popen(
    ["ssh", "-p", "443", "-R0:localhost:5050", "a.pinggy.io", "-o", "StrictHostKeyChecking=no"],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True
)

url = None
# Pinggy prints a QR code and URL. We need to read enough lines to catch the URL.
start = time.time()
while time.time() - start < 15:
    # Read both stdout and stderr (non-blocking if possible, but pinggy prints fast)
    line = process.stdout.readline()
    if not line:
        line = process.stderr.readline()
    
    if line:
        match = re.search(r'(https?://[a-zA-Z0-9.-]+\.pinggy\.link)', line)
        if match:
            url = match.group(1)
            break

if url:
    with open("pinggy_url.txt", "w") as f:
        f.write(url)
    print(f"Tunnel established: {url}")
    # Keep process alive
    try:
        while True:
            time.sleep(10)
    except KeyboardInterrupt:
        process.terminate()
else:
    with open("pinggy_url.txt", "w") as f:
        f.write("FAILED")
    print("Failed to establish tunnel.")
    process.terminate()
