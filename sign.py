from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization

# 1. Load private key
with open("private_key.pem", "rb") as f:
    private_key = serialization.load_pem_private_key(f.read(), password=None)

# 2. Hash and sign firmware.bin
with open("firmware.bin", "rb") as f:
    firmware_data = f.read()

signature = private_key.sign(firmware_data, ec.ECDSA(hashes.SHA256()))

# 3. Save signature
with open("firmware.sig", "wb") as f:
    f.write(signature)

print("[SUCCESS] firmware.bin signed! Generated firmware.sig")