#!/usr/bin/env python3
"""
Discover available OSC endpoints on Wing console.
Send requests to common paths and see what responds.
"""

import socket
import time
import struct

WING_IP = "192.168.10.210"
WING_PORT = 9000
LOCAL_PORT = 9001

def create_osc_message(path):
    """Create a minimal OSC message"""
    msg = bytearray()
    
    # Path (null-terminated, padded to 4-byte boundary)
    path_bytes = path.encode('utf-8') + b'\x00'
    padding = (4 - len(path_bytes) % 4) % 4
    msg.extend(path_bytes)
    msg.extend(b'\x00' * padding)
    
    # Type tag string (empty)
    type_tag = b',\x00'
    padding = (4 - len(type_tag) % 4) % 4
    msg.extend(type_tag)
    msg.extend(b'\x00' * (padding - 1))
    
    return bytes(msg)

def send_osc(sock, path):
    """Send OSC message to Wing"""
    msg = create_osc_message(path)
    sock.sendto(msg, (WING_IP, WING_PORT))

def listen_for_response(sock, timeout=0.5):
    """Listen for responses"""
    sock.settimeout(timeout)
    responses = []
    try:
        while True:
            data, addr = sock.recvfrom(1024)
            responses.append(data)
    except socket.timeout:
        pass
    return responses

# Paths that ARE known to work in the existing code
working_paths = [
    "/ch/01/clink",           # Channel stereo link (known to work)
    "/ch/01/name",            # Channel name (known to work)
    "/io/in/USR/1/user/grp",  # USR routing group (known to work)
    "/io/in/USR/1/user/in",   # USR routing input (known to work)
]

# Additional discovery paths
discovery_paths = [
    # Try to find stereo info in channel data
    "/ch/01/stereo",
    "/ch/01/stereolink", 
    "/ch/01/link",
    
    # Try channel-based USR stereo
    "/ch/01/source/stereo",
    "/ch/01/source/link",
    "/ch/01/in/stereo",
    "/ch/01/in/clink",
    
    # Info endpoints
    "/info",
    "/xinfo",
    
    # USR info paths
    "/io/in/USR/info",
    "/io/in/USR/1/info",
]

print("=" * 70)
print("Wing OSC Path Discovery")
print("=" * 70)
print(f"Testing {WING_IP}:{WING_PORT}")
print()

# Perform handshake first
print("Performing Wing handshake...")
handshake_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    handshake_sock.sendto(b"WING?", (WING_IP, 2222))
    handshake_sock.settimeout(1.5)
    try:
        data, addr = handshake_sock.recvfrom(1024)
        print(f"✓ Handshake successful: {data[:50]}")
    except socket.timeout:
        print("✗ Handshake timeout (Wing may not be responding)")
finally:
    handshake_sock.close()

print()

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('0.0.0.0', LOCAL_PORT))

working_found = []
responding_found = []

try:
    print("Testing KNOWN working paths:")
    print("-" * 70)
    for path in working_paths:
        print(f"  {path:<40}", end=" ", flush=True)
        send_osc(sock, path)
        time.sleep(0.05)
        responses = listen_for_response(sock, timeout=0.3)
        if responses:
            print(f"✓ (response: {len(responses[0])} bytes)")
            working_found.append(path)
        else:
            print(f"✗")
    
    print()
    print("Testing DISCOVERY paths:")
    print("-" * 70)
    for path in discovery_paths:
        print(f"  {path:<40}", end=" ", flush=True)
        send_osc(sock, path)
        time.sleep(0.05)
        responses = listen_for_response(sock, timeout=0.3)
        if responses:
            print(f"✓ (response: {len(responses[0])} bytes)")
            responding_found.append(path)
        else:
            print(f"✗")
    
    print()
    print("=" * 70)
    print("RESULTS:")
    print("=" * 70)
    print(f"Known paths working: {len(working_found)}/{len(working_paths)}")
    if responding_found:
        print(f"New working paths found: {len(responding_found)}")
        for p in responding_found:
            print(f"  ✓ {p}")
    else:
        print("No new working paths found")
    
    print()
    print("ANALYSIS:")
    print("-" * 70)
    if not responding_found:
        print("❌ NO path was found to query USR stereo status!")
        print()
        print("Possibilities:")
        print("1. Wing doesn't expose USR stereo via OSC")
        print("2. Stereo must be inferred from channel configuration")
        print("3. May need to check channel's stereo_linked flag")
        print("4. Alternate approach: assume USR odd numbers are stereo")
    

finally:
    sock.close()
