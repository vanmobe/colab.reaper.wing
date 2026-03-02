#!/usr/bin/env python3
"""Probe various stereo-link paths on physical inputs to find what Wing responds to."""

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
                if len(data) >= data_start + 4:
                    raw = data[data_start:data_start+4]
                    int_val = struct.unpack('>I', raw)[0]
                    # Decode as packed ASCII string
                    s = raw.rstrip(b'\x00').decode('ascii', errors='ignore').strip()
                    if s:
                        value = s
                    else:
                        value = int_val
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

def send_query(sock, path):
    msg = bytearray()
    path_bytes = path.encode('utf-8') + b'\x00'
    padding = (4 - len(path_bytes) % 4) % 4
    msg.extend(path_bytes)
    msg.extend(b'\x00' * padding)
    msg.extend(b',\x00\x00\x00')
    sock.sendto(bytes(msg), (WING_IP, WING_OSC_PORT))

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(('0.0.0.0', LISTEN_PORT))
t = threading.Thread(target=listener, args=(sock,), daemon=True)
t.start()
time.sleep(0.1)

# Probe candidate paths on a few known inputs: A:1, A:19, USR:25
test_inputs = [("A", 1), ("A", 19), ("A", 35), ("USR", 25)]
stereo_suffixes = [
    "/clink",
    "/config/link",
    "/link",
    "/stereo",
    "/setup/link",
    "/preamp/link",
    "/inst/link",
]

paths = []
for grp, num in test_inputs:
    for suffix in stereo_suffixes:
        path = "/io/in/{}/{}{}".format(grp, num, suffix)
        paths.append(path)
        send_query(sock, path)
        time.sleep(0.02)

# Also try channel config/link vs clink
for ch in [1, 8, 19, 21]:
    ch_str = "{:02d}".format(ch)
    for sfx in ["/clink", "/config/link"]:
        path = "/ch/{}{}".format(ch_str, sfx)
        paths.append(path)
        send_query(sock, path)
        time.sleep(0.02)

time.sleep(3)

print("=== PROBE RESULTS ===")
print("Responded:")
for path in paths:
    val = results.get(path)
    if val is not None:
        print("  ✓  {:<50} = {}".format(path, val))

print("\nNo response:")
for path in paths:
    if results.get(path) is None:
        print("  ✗  {}".format(path))

sock.close()
