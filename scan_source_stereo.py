#!/usr/bin/env python3
"""
Query channel sources then stereo status on those sources.
Step 1: /ch/XX/in/conn/grp  -> e.g. "A", "USR", "B"
Step 2: /ch/XX/in/conn/in   -> e.g. 8
Step 3: /io/in/{grp}/{num}/clink -> stereo status
"""

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
                elif 's' in tag or len(tag) <= 2:
                    # Try string - Wing often returns values as strings
                    str_end = data.find(b'\x00', data_start)
                    raw = data[data_start:str_end] if str_end > data_start else data[data_start:data_start+4]
                    # Also try as packed ASCII int
                    if data_start + 4 <= len(data):
                        int_val = struct.unpack('>I', data[data_start:data_start+4])[0]
                        first = (int_val >> 24) & 0xFF
                        if 32 <= first <= 126:  # printable ASCII
                            # Extract string bytes until null
                            s = ''
                            for i in range(data_start, min(data_start+32, len(data))):
                                b = data[i]
                                if b == 0:
                                    break
                                if 32 <= b <= 126:
                                    s += chr(b)
                            value = s if s else int_val
                        else:
                            value = int_val
                    else:
                        value = raw.decode('utf-8', errors='ignore')
                return addr, value
    except Exception as e:
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

# --- Setup ---
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(('0.0.0.0', LISTEN_PORT))

t = threading.Thread(target=listener, args=(sock,), daemon=True)
t.start()
time.sleep(0.1)

print("Step 1: Querying channel sources (/ch/XX/in/conn/grp and /in)...")
for ch in range(1, 41):
    ch_str = "{:02d}".format(ch)
    send_query(sock, "/ch/{}/in/conn/grp".format(ch_str))
    send_query(sock, "/ch/{}/in/conn/in".format(ch_str))
    time.sleep(0.02)

time.sleep(3)

# Parse sources
def decode_wing_string(val):
    """Wing returns strings packed as big-endian int32 ASCII bytes.
    e.g. 0x31000000 = '1', 0x32350000 = '25', 0x41000000 = 'A'"""
    if isinstance(val, (int, float)):
        b = int(val).to_bytes(4, 'big')
        s = b.rstrip(b'\x00').decode('ascii', errors='ignore').strip()
        return s if s else str(int(val))
    return str(val) if val else ''

print("\nStep 2: Resolving sources...")
ch_sources = {}
for ch in range(1, 41):
    ch_str = "{:02d}".format(ch)
    grp_raw = results.get("/ch/{}/in/conn/grp".format(ch_str))
    inp_raw = results.get("/ch/{}/in/conn/in".format(ch_str))
    grp = decode_wing_string(grp_raw) if grp_raw is not None else None
    inp = decode_wing_string(inp_raw) if inp_raw is not None else None
    ch_sources[ch] = (grp, inp)
    if grp or inp:
        print("  CH{:02d}: grp={:<8} in={}".format(ch, str(grp), str(inp)))

# Collect unique sources to query stereo on
print("\nStep 3: Querying stereo status on sources...")
source_paths = set()
for ch, (grp, inp) in ch_sources.items():
    if grp and inp and grp != 'OFF':
        source_paths.add("/io/in/{}/{}/clink".format(grp, inp))

for path in sorted(source_paths):
    send_query(sock, path)
    time.sleep(0.02)

time.sleep(3)

# --- Print full report ---
print("\n" + "=" * 60)
print("CHANNEL STEREO STATUS (via source)")
print("=" * 60)
print("{:<6} {:<10} {:<6} {:<35} {}".format("CH", "Grp", "In", "Source Path", "Stereo?"))
print("-" * 75)

for ch in range(1, 41):
    grp, inp = ch_sources[ch]
    if grp and inp and grp != 'OFF':
        path = "/io/in/{}/{}/clink".format(grp, inp)
        val = results.get(path)
        if val is None:
            stereo_str = "NO RESPONSE"
        else:
            first_byte = (int(val) >> 24) & 0xFF if isinstance(val, (int, float)) else 0
            if first_byte == 0x31 or val == 1 or str(val).startswith('1'):
                stereo_str = "STEREO <--"
            else:
                stereo_str = "mono"
        source_path = path
    elif grp == 'OFF' or not grp:
        stereo_str = "OFF / no source"
        source_path = "-"
        inp = inp or "-"
    else:
        stereo_str = "no source"
        source_path = "-"
    print("{:<6} {:<10} {:<6} {:<35} {}".format(ch, str(grp), str(inp), source_path, stereo_str))

sock.close()
