from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization

# 1. Generate an ECDSA private key (secp256r1)
private_key = ec.generate_private_key(ec.SECP256R1())

# 2. Save the private key to 'private_key.pem'
with open("private_key.pem", "wb") as f:
    f.write(private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.TraditionalOpenSSL,
        encryption_algorithm=serialization.NoEncryption()
    ))

# 3. Extract and save the public key to 'public_key.pem'
public_key = private_key.public_key()
with open("public_key.pem", "wb") as f:
    f.write(public_key.public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo
    ))

print("Success! Generated:")
print(" - private_key.pem (Keep safe on your PC)")
print(" - public_key.pem  (Copy its content to ESP32)")