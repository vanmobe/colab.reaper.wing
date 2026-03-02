#!/usr/bin/env python3
"""Scan all 48 channels for stereo link status via /ch/XX/clink"""

import socket
import time
import struct
import threading

WING_IP = "192.168.10.210"
WING_OSC_PORT = 2223
LISTEN_PORT = 9099

results = {}
lock = threading.Lock()

def parse_osc(data):
    try:
        null_idx = data.find(b'\x00')
        if null_idx > 0:
            addr = data[:null_idx].decode('utf-8', errors='ignore')
            comma_idx = data.find(b',', null_idx)
            if comma_idx >= 0:
                tag_end = data.find(b'\x00', comma_idx)
                tag = data[comma_idx:tag_end].decode('utf-8', errors='ignore')
                data_start = ((tag_end + 4) // 4) * 4
                value = None
                if 'i' in tag and data_start + 4 <= len(data):
                    value = struct.unpack('>i', data[data_start:data_start+4])[0]
                elif 'f' in tag and data_start + 4 <= len(data):
                    value = struct.unpack('>f', data[data_start:data_start+4])[0]
                elif 's' in tag:
                    str_end = data.find(b'\x00', data_start)
                    value = data[data_start:str_end].decode('utf-8', errors='ignore') if str_end > 0 else ''
                return addr, value
    except:
        pass
    return None, None

def listener(sock):
    sock.settimeout(0.3)
    while True:
        try:
            data, _ = sock.recvfrom(1024)
            addr, val = parse_osc(data)
            if addr:
                with lock:
                    results[addr] = val
        except socket.timeout:
            pass
        except:
            break

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(('0.0.0.0', LISTEN_PORT))

t = threading.Thread(target=listener, args=(sock,), daemon=True)
t.start()
time.sleep(0.1)

# Query all 48 channels
for ch in range(1, 49):
    addr = "/ch/{:02d}/clink".format(ch)
    msg = bytearray()
    path_bytes = addr.encode('utf-8') + b'\x00'
    padding = (4 - len(path_bytes) % 4) % 4
    msg.extend(path_bytes)
    msg.extend(b'\x00' * padding)
    msg.extend(b',\x00\x00\x00')
    sock.sendto(bytes(msg), (WING_IP, WING_OSC_PORT))
    time.sleep(0.02)

time.sleep(3)

print("\n=== STEREO STATUS ALL CHANNELS ===")
print("{:<6} {:<14} {}".format("CH", "Raw Value", "Status"))
print("-" * 42)
for ch in range(1, 49):
    addr = "/ch/{:02d}/clink".format(ch)
    val = results.get(addr, None)
    if val is None:
        status = "NO RESPONSE"
    else:
        # Wing returns string '0'/'1' packed as int32 big-endian
        # 0x30000000 = ASCII '0' = MONO, 0x31000000 = ASCII '1' = STEREO
        first_byte = (val >> 24) & 0xFF
        if first_byte == 0x30:  # ASCII '0'
            status = "MONO"
        elif first_byte == 0x31:  # ASCII '1'
            status = "STEREO  <--"
        elif val == 0:
            status = "MONO (int 0)"
        elif val == 1:
            status = "STEREO (int 1)  <--"
        else:
            status = "UNKNOWN (val={})".format(val)
    print("{:<6} {:<14} {}".format(ch, str(val), status))

sock.close()
