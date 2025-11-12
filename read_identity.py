#!/usr/bin/env python3
import struct
import sys

def read_echo_identity(filepath="echo_identity.dat"):
    try:
        with open(filepath, 'rb') as f:
            username_len = struct.unpack('<I', f.read(4))[0]
            
            if username_len > 255:
                return None, None
            
            username = f.read(username_len).decode('utf-8')
            
            public_key = f.read(32)
            
            fingerprint = public_key[:16].hex()
            
            return username, fingerprint
    except:
        return None, None

if __name__ == "__main__":
    username, fingerprint = read_echo_identity()
    if username and fingerprint:
        print(f"{username}|{fingerprint}")
    else:
        sys.exit(1)