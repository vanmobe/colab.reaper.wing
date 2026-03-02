#!/usr/bin/env python3
"""
Probe Wing console to find the correct OSC path for USR stereo link status.
Tests various path combinations to determine what the Wing responds to.
"""

import socket
import time
import struct
import sys

WING_IP = "192.168.10.210"
WING_PORT = 9000
LOCAL_PORT = 9001

def create_osc_message(path, *args):
    """Create a minimal OSC message"""
    msg = bytearray()
    
    # Path (null-terminated, padded to 4-byte boundary)
    path_bytes = path.encode('utf-8') + b'\x00'
    padding = (4 - len(path_bytes) % 4) % 4
    msg.extend(path_bytes)
    msg.extend(b'\x00' * padding)
    
    # Type tag string (just markers, no actual args for now)
    type_tag = b','
    for arg in args:
        if isinstance(arg, int):
            type_tag += b'i'
        elif isinstance(arg, str):
            type_tag += b's'
    
    type_tag += b'\x00'
    padding = (4 - len(type_tag) % 4) % 4
    msg.extend(type_tag)
    msg.extend(b'\x00' * padding)
    
    # Arguments
    for arg in args:
        if isinstance(arg, int):
            msg.extend(struct.pack('>i', arg))
        elif isinstance(arg, str):
            arg_bytes = arg.encode('utf-8') + b'\x00'
            padding = (4 - len(arg_bytes) % 4) % 4
            msg.extend(arg_bytes)
            msg.extend(b'\x00' * padding)
    
    return bytes(msg)

def send_osc(sock, path, *args):
    """Send OSC message to Wing"""
    msg = create_osc_message(path, *args)
    sock.sendto(msg, (WING_IP, WING_PORT))

def listen_for_response(sock, timeout=1.0):
    """Listen for responses"""
    sock.settimeout(timeout)
    responses = []
    try:
        while True:
            data, addr = sock.recvfrom(1024)
            responses.append((data, addr))
    except socket.timeout:
        pass
    return responses

# Test paths for USR stereo status
test_paths = [
    # Main candidates
    "/io/in/USR/{}/clink",
    "/io/in/USR/{}/stereo",
    "/io/in/USR/{}/link",
    "/io/in/USR/{}/linked",
    
    # Try without numeric placeholder
    "/io/in/USR/clink",
    "/io/in/USR/stereo",
    "/io/in/USR/link",
    
    # Try alternate structure
    "/io/in/USR/{}/user/clink",
    "/io/in/USR/{}/user/stereo",
    
    # Try channel-based approach (in case it's per-channel, not per-USR)
    "/ch/{}/usr/stereo",
    "/ch/{}/usr/link",
    
    # Try simpler paths
    "/USR/{}/clink",
    "/USR/{}/stereo",
    
    # Try request/query paths
    "/query/usr/{}/stereo",
    "/query/usr/{}/link",
]

print("=" * 60)
print("Wing USR Stereo Link Status Probe")
print("=" * 60)
print(f"Connecting to Wing at {WING_IP}:{WING_PORT}")
print(f"Listening on port {LOCAL_PORT}")
print()

# Perform handshake first
print("Performing Wing handshake on port 2222...")
handshake_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    handshake_sock.sendto(b"WING?", (WING_IP, 2222))
    handshake_sock.settimeout(1.5)
    try:
        data, addr = handshake_sock.recvfrom(1024)
        print(f"✓ Handshake successful!")
    except socket.timeout:
        print("✗ Handshake timeout (Wing may not be responding)")
finally:
    handshake_sock.close()

print()

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('0.0.0.0', LOCAL_PORT))

try:
    for path_template in test_paths:
        # For paths with placeholders, test with a few USR numbers
        if '{}' in path_template:
            test_nums = [1, 25, 26]  # Test 1 (first), 25 (middle), 26 (pair of 25)
        else:
            test_nums = [None]
        
        for num in test_nums:
            if num is not None:
                path = path_template.format(num)
            else:
                path = path_template
            
            print(f"Testing: {path:<50}", end=" ", flush=True)
            
            # Send query
            send_osc(sock, path)
            time.sleep(0.1)
            
            # Listen for response
            responses = listen_for_response(sock, timeout=0.5)
            
            if responses:
                print(f"✓ GOT RESPONSE ({len(responses)} message{'s' if len(responses) > 1 else ''})")
                for data, addr in responses:
                    print(f"   From {addr}: {len(data)} bytes")
                    # Try to parse response
                    try:
                        if len(data) > 0:
                            # Print first bytes as hex
                            hex_str = ' '.join(f'{b:02x}' for b in data[:min(32, len(data))])
                            print(f"   Data: {hex_str}")
                    except:
                        pass
            else:
                print("✗ No response")
            
            print()
    
    print("=" * 60)
    print("Probe complete. Check responses above for working paths.")
    print("=" * 60)

finally:
    sock.close()
