#!/usr/bin/env python3
"""
Listen for OSC messages from Wing to discover available endpoints and stereo paths.
This probe will stay running and log all OSC messages received.
"""

import socket
import time
import struct
import sys
import threading

WING_IP = "192.168.10.210"
WING_PORT = 9000
LISTEN_PORT = 9001

def osc_parse_message(data):
    """Try to parse OSC message address"""
    try:
        # OSC messages start with the address (null-terminated string)
        null_idx = data.find(b'\x00')
        if null_idx > 0:
            address = data[:null_idx].decode('utf-8', errors='ignore')
            return address
    except:
        pass
    return None

def create_osc_message(path):
    """Create a minimal OSC message"""
    msg = bytearray()
    path_bytes = path.encode('utf-8') + b'\x00'
    padding = (4 - len(path_bytes) % 4) % 4
    msg.extend(path_bytes)
    msg.extend(b'\x00' * padding)
    type_tag = b',\x00\x00\x00'  # Empty type tag
    msg.extend(type_tag)
    return bytes(msg)

print("=" * 70)
print("Wing OSC Message Monitor")
print("=" * 70)
print(f"Listening on port {LISTEN_PORT} for messages from {WING_IP}:{WING_PORT}")
print()

# Perform handshake
print("Performing Wing handshake...")
handshake_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    handshake_sock.sendto(b"WING?", (WING_IP, 2222))
    handshake_sock.settimeout(1.5)
    data, addr = handshake_sock.recvfrom(1024)
    print(f"✓ Handshake successful: {data.decode('utf-8', errors='ignore')}\n")
except:
    print("✗ Handshake failed\n")
finally:
    handshake_sock.close()

# Create listening socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('0.0.0.0', LISTEN_PORT))
sock.settimeout(0.5)

# Send various queries to Wing
queries = [
    "/ch/01/name",
    "/ch/01/clink",
    "/io/in/USR/1/user/grp",
    "/io/in/USR/1/user/in",
    "/io/in/USR/1/clink",
    "/io/in/USR/25/clink",
    "/io/in/USR/25/link",
]

print("Sending test queries to Wing...")
for query in queries:
    try:
        msg = create_osc_message(query)
        sock.sendto(msg, (WING_IP, WING_PORT))
        print(f"  Sent: {query}")
    except Exception as e:
        print(f"  Error sending {query}: {e}")
    time.sleep(0.05)

print("\nListening for responses (30 seconds)...\n")
print("-" * 70)

start_time = time.time()
message_count = 0
usr_clink_found = False

try:
    while time.time() - start_time < 30:
        try:
            data, addr = sock.recvfrom(1024)
            address = osc_parse_message(data)
            if address:
                message_count += 1
                print(f"✓ {address:<50} ({len(data)} bytes)")
                
                # Check if this is stereo data
                if "clink" in address:
                    print(f"  ^ STEREO LINK MESSAGE! (might contain stereo status)")
                    if "USR" in address:
                        usr_clink_found = True
                        # Try to extract the value
                        if len(data) > 8:
                            try:
                                # Skip address and type tag, read first int32
                                idx = len(address) + 1
                                idx = ((idx + 4) // 4) * 4  # Align to 4 bytes
                                idx = ((idx + 4) // 4) * 4  # Skip type tag
                                if idx + 4 <= len(data):
                                    value = struct.unpack('>i', data[idx:idx+4])[0]
                                    print(f"    Value: {value} ({'STEREO' if value else 'MONO'})")
                            except:
                                pass
        except socket.timeout:
            pass

except KeyboardInterrupt:
    print("\n(interrupted)")

print("-" * 70)
print()
print("=" * 70)
print("RESULTS:")
print("=" * 70)
print(f"Total messages received: {message_count}")
print(f"USR stereo (clink) path working: {'YES' if usr_clink_found else 'NO'}")
print()

if message_count == 0:
    print("❌ ERROR: No responses from Wing!")
    print("The Wing might not be responding to OSC queries.")
    print("Try these troubleshooting steps:")
    print("  1. Verify Wing IP address is correct (currently: {})".format(WING_IP))
    print("  2. Check Wing console is powered on and connected to network")
    print("  3. Verify firewall isn't blocking port 9000")
    print("  4. Try running the plugin in REAPER and monitoring its output")
elif usr_clink_found:
    print("✓ USR stereo (clink) path is working!")
    print("  The plugin can now query stereo status via /io/in/USR/[N]/clink")
else:
    print("⚠ Wing responded to queries but not for USR stereo endpoints")
    print("  This might mean:")
    print("    - Wing doesn't expose stereo via /io/in/USR/N/clink")
    print("    - Try other paths like /io/in/USR/N/link")
    print("    - Or stereo might only be queryable differently")

sock.close()
