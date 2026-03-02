#!/usr/bin/env python3
"""Probe the /io/in/{grp}/{num}/mode path for source stereo status."""
import socket
import struct
import time

WING_IP = '192.168.10.210'
WING_PORT = 2223
LISTEN_PORT = 9099


def pad4(s: bytes) -> bytes:
    n = len(s)
    remainder = n % 4
    if remainder == 0:
        return s + b'\x00\x00\x00\x00'
    return s + b'\x00' * (4 - remainder)


def make_osc_get(path: str) -> bytes:
    return pad4(path.encode('ascii')) + pad4(b',')


def parse_response(data: bytes):
    """Parse Wing OSC response and return a human-readable value."""
    # Extract path
    i = 0
    while i < len(data) and data[i] != 0:
        i += 1
    # Skip to type tag string (next 4-aligned boundary)
    i = ((i // 4) + 1) * 4
    if i >= len(data) or data[i] != ord(','):
        return f'[no type tag] {data.hex()}'
    tags = b''
    j = i + 1
    while j < len(data) and data[j] != 0:
        tags += bytes([data[j]])
        j += 1
    # Skip to arguments (next 4-aligned boundary)
    j = ((j // 4) + 1) * 4
    # Return based on first meaningful tag
    if b's' in tags:
        sval = b''
        k = j
        while k < len(data) and data[k] != 0:
            sval += bytes([data[k]])
            k += 1
        return f'"{sval.decode("ascii", errors="replace")}"'
    elif b'i' in tags:
        return str(struct.unpack('>i', data[j:j+4])[0])
    elif b'f' in tags:
        return str(struct.unpack('>f', data[j:j+4])[0])
    return f'[raw] {data[j:].hex()}'


def query(sock: socket.socket, path: str) -> str:
    msg = make_osc_get(path)
    sock.sendto(msg, (WING_IP, WING_PORT))
    try:
        sock.settimeout(0.6)
        data, _ = sock.recvfrom(4096)
        return parse_response(data)
    except socket.timeout:
        return 'NO RESPONSE'


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('0.0.0.0', LISTEN_PORT))

    # Handshake: send /? to confirm Wing is reachable
    print("Handshake (/?):", query(sock, '/?'))
    time.sleep(0.2)

    tests = [
        # CH10=A:33, CH11=A:34 - CH10 wrongly shown as stereo
        '/io/in/A/33/mode',
        '/io/in/A/34/mode',
        # CH19=A:19 (stereo), CH20=A:21 (mono per user)
        '/io/in/A/19/mode',
        '/io/in/A/20/mode',
        '/io/in/A/21/mode',
        # CH8=USR:25 (stereo)
        '/io/in/USR/25/mode',
        '/io/in/USR/26/mode',
        # CH25-28 = A:41,43,45,47 shown as stereo
        '/io/in/A/41/mode',
        '/io/in/A/42/mode',
        '/io/in/A/43/mode',
        '/io/in/A/44/mode',
    ]

    print("Testing /io/in/{grp}/{num}/mode paths on Wing console...")
    print(f"{'PATH':<35} {'RESULT'}")
    print('-' * 55)
    for path in tests:
        result = query(sock, path)
        print(f'{path:<35} {result}')
        time.sleep(0.15)

    sock.close()


if __name__ == '__main__':
    main()
