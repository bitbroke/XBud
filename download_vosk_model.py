import urllib.request
import zipfile
import os

url = "https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip"
zip_path = "vosk-model.zip"
extract_dir = "model"

print(f"Downloading Vosk model from {url}...")
urllib.request.urlretrieve(url, zip_path)

print(f"Extracting {zip_path}...")
with zipfile.ZipFile(zip_path, 'r') as zip_ref:
    zip_ref.extractall(".")

print("Renaming extracted folder to 'model'...")
extracted_folder = "vosk-model-small-en-us-0.15"
if os.path.exists(extracted_folder):
    if os.path.exists(extract_dir):
        print(f"Directory '{extract_dir}' already exists. Skipping rename.")
    else:
        os.rename(extracted_folder, extract_dir)
        print("Done!")

if os.path.exists(zip_path):
    os.remove(zip_path)
    print("Cleaned up zip file.")
