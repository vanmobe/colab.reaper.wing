#!/usr/bin/env python3
"""
Test OSC paths locally to find the correct way to query USR stereo status.
Performs handshake, then queries various paths and captures responses.
"""

import socket
import time
import struct
import threading
from collections import defaultdict

WING_IP = "192.168.10.210"
WING_OSC_PORT = 2223  # Wing's OSC listen port from config, not 9000!
WING_HANDSHAKE_PORT = 2222
LISTEN_PORT = 2224  # Use different port to receive responses (avoid conflict)

# Store responses by address
responses_by_path = defaultdict(list)
responses_lock = threading.Lock()

def create_osc_message(path):
    """Create /subscribe OSC message"""
    msg = bytearray()
    # /subscribe path
    addr = b"/subscribe\x00\x00\x00"  # null-padded to 4-byte boundary
    msg.extend(addr)
    # Type tag: ,s (string)
    msg.extend(b",s\x00\x00")
    # String argument: the path to subscribe to
    path_bytes = path.encode('utf-8') + b'\x00'
    padding = (4 - len(path_bytes) % 4) % 4
    msg.extend(path_bytes)
    msg.extend(b'\x00' * padding)
    return bytes(msg)

def parse_osc_address(data):
    """Extract OSC address from message"""
    try:
        null_idx = data.find(b'\x00')
        if null_idx > 0:
            return data[:null_idx].decode('utf-8', errors='ignore')
    except:
        pass
    return None

def extract_int32(data):
    """Try to extract int32 value from OSC message"""
    try:
        # Find type tag
        comma_idx = data.find(b',')
        if comma_idx < 0:
            return None
        
        # Find end of type tag string
        type_tag_end = data.find(b'\x00', comma_idx)
        if type_tag_end < 0:
            return None
        
        # Align to 4-byte boundary
        data_start = ((type_tag_end + 4) // 4) * 4
        
        if data_start + 4 <= len(data):
            return struct.unpack('>i', data[data_start:data_start+4])[0]
    except:
        pass
    return None

def listener_thread(sock):
    """Listen for incoming OSC messages"""
    sock.settimeout(0.5)
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            address = parse_osc_address(data)
            if address:
                value = extract_int32(data)
                with responses_lock:
                    responses_by_path[address].append({
                        'value': value,
                        'bytes': len(data),
                        'timestamp': time.time()
                    })
        except socket.timeout:
            pass
        except:
            break

print("=" * 70)
print("Local OSC Path Testing")
print("=" * 70)
print()

# Step 1: Handshake
print("[1] Performing Wing handshake on port {}...".format(WING_HANDSHAKE_PORT))
handshake_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    handshake_sock.sendto(b"WING?", (WING_IP, WING_HANDSHAKE_PORT))
    handshake_sock.settimeout(2)
    data, addr = handshake_sock.recvfrom(1024)
    handshake_response = data.decode('utf-8', errors='ignore')
    print("✓ Handshake successful!")
    print("  Response: {}".format(handshake_response.split('\n')[0]))
except Exception as e:
    print("✗ Handshake failed: {}".format(e))
    exit(1)
finally:
    handshake_sock.close()

print()

print("[2] Setting up OSC listener on port {}...".format(LISTEN_PORT))
listen_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
listen_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
listen_sock.bind(('0.0.0.0', LISTEN_PORT))
print("✓ Listener ready")

# Start listener thread FIRST (before queries)
listener = threading.Thread(target=listener_thread, args=(listen_sock,), daemon=True)
listener.start()
time.sleep(0.2)  # Give listener time to start

print()

# Step 3: Test various paths
print("[3] Testing OSC paths for stereo status...\n")

# According to OSC documentation, we need to SUBSCRIBE to parameters, not query them
# The Wing then sends asynchronous updates

test_cases = [
    # Subscribe to channel stereo link (for reference)
    ("/subscribe /ch/01/config/link", "Subscribe to Channel 01 stereo link (config/link)"),
    ("/subscribe /ch/01/clink", "Subscribe to Channel 01 stereo link (clink)"),
    
    # Subscribe to USR stereo paths - various candidates
    ("/subscribe /io/in/USR/25/config/link", "Subscribe to USR 25 (config/link)"),
    ("/subscribe /io/in/USR/25/link", "Subscribe to USR 25 (link)"),
    ("/subscribe /io/in/USR/25/clink", "Subscribe to USR 25 (clink)"),
    ("/subscribe /io/in/USR/25/stereo", "Subscribe to USR 25 (stereo)"),
    
    # Zero-padded
    ("/subscribe /io/in/USR/025/config/link", "Subscribe to USR 025 (config/link)"),
    ("/subscribe /io/in/USR/025/clink", "Subscribe to USR 025 (clink)"),

    # Channel approach
    ("/subscribe /ch/08/config/link", "Subscribe to Channel 08 (uses USR25)"),
    ("/subscribe /ch/08/clink", "Subscribe to Channel 08 (clink)"),
]

# Test direct OSC queries for stereo-related paths
# (direct queries work, not /subscribe)
print("[3a] Testing stereo detection paths (direct queries)...\n")

stereo_test_paths = [
    ("/ch/01/config/link", "Channel 01 config/link"),
    ("/ch/01/clink", "Channel 01 clink"),
    ("/ch/08/config/link", "Channel 08 config/link"),
    ("/ch/08/clink", "Channel 08 clink"),
    ("/io/in/USR/25/config/link", "USR 25 config/link"),
    ("/io/in/USR/25/link", "USR 25 link"),
    ("/io/in/USR/25/clink", "USR 25 clink"),
    ("/io/in/USR/25/stereo", "USR 25 stereo"),
    ("/ch/01/mix/fader", "Channel 01 fader (sanity check)"),
]

sent_count = 0
for query, description in stereo_test_paths:
    try:
        # Create simple OSC message
        msg = bytearray()
        path_bytes = query.encode('utf-8') + b'\x00'
        padding = (4 - len(path_bytes) % 4) % 4
        msg.extend(path_bytes)
        msg.extend(b'\x00' * padding)
        msg.extend(b',\x00\x00\x00')  # Empty type tag
        
        listen_sock.sendto(bytes(msg), (WING_IP, WING_OSC_PORT))
        print("→ Sent: {:<40} ({:<25})".format(query, description))
        sent_count += 1
        time.sleep(0.05)
    except Exception as e:
        print("✗ Error sending {}: {}".format(query, e))

print("\n[3b] Waiting for responses (3 seconds)...\n")
time.sleep(3)

print("=" * 70)
print("RESULTS:")
print("=" * 70)

# Check which paths got responses
successful_paths = []
for path in sorted(responses_by_path.keys()):
    responses = responses_by_path[path]
    print("\n✓ Got {} response(s) from: {}".format(len(responses), path))
    for resp in responses:
        if resp['value'] is not None:
            print("  Value: {} ({})".format(resp['value'], 
                  'LINKED/STEREO' if resp['value'] else 'UNLINKED/MONO'))
        else:
            print("  ({} bytes, couldn't parse value)".format(resp['bytes']))
    successful_paths.append(path)

print()
print("=" * 70)
print("SUMMARY:")
print("=" * 70)

if not successful_paths:
    print("\n❌ NO RESPONSES received from any path!")
    print("\nPossibilities:")
    print("  1. Wing is not responding (check connection, IP)")
    print("  2. Need to subscribe first using /subscribe")
    print("  3. Stereo info might be in different parameter")
    print("\nNext step: Check Wing IP={} is correct".format(WING_IP))
else:
    print("\n✓ Wing responded to {} path(s):".format(len(successful_paths)))
    for path in successful_paths:
        print("  • {}".format(path))
    
    # Try to identify the best path
    usr_paths = [p for p in successful_paths if '/io/in/USR/' in p]
    ch_paths = [p for p in successful_paths if '/ch/' in p and '/io/in/' not in p]
    
    if usr_paths:
        print("\n✓ USR stereo queries WORK! Use: {}".format(usr_paths[0]))
    elif ch_paths:
        print("\n⚠ Channel-based queries work, but not USR-based")
        print("  Might need to query by channel instead of USR input")

listen_sock.close()
