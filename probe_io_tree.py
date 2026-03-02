#!/usr/bin/env python3
"""Navigate the Wing's live JSON node tree to find io/in structure."""
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


def parse_node_response(data: bytes):
    """Parse a Wing node response (returns list of child node names)."""
    i = 0
    while i < len(data) and data[i] != 0:
        i += 1
    i = ((i // 4) + 1) * 4
    if i >= len(data):
        return f'[empty response] {data.hex()}'
    if data[i] != ord(','):
        return f'[no type tag] {data[i:].hex()}'
    tags = b''
    j = i + 1
    while j < len(data) and data[j] != 0:
        tags += bytes([data[j]])
        j += 1
    j = ((j // 4) + 1) * 4

    results = []
    for tag in tags:
        if chr(tag) == 's':
            sval = b''
            while j < len(data) and data[j] != 0:
                sval += bytes([data[j]])
                j += 1
            j = ((j // 4) + 1) * 4
            if sval:
                results.append(sval.decode('ascii', errors='replace'))
        elif chr(tag) == 'i':
            results.append(f'int:{struct.unpack(">i", data[j:j+4])[0]}')
            j += 4
        elif chr(tag) == 'f':
            results.append(f'float:{struct.unpack(">f", data[j:j+4])[0]:.4f}')
            j += 4
    if results:
        return results
    return f'[raw tags={tags}] {data[j:].hex()}'


def query_node(sock: socket.socket, path: str):
    msg = make_osc_get(path)
    sock.sendto(msg, (WING_IP, WING_PORT))
    try:
        sock.settimeout(1.0)
        data, _ = sock.recvfrom(4096)
        return parse_node_response(data)
    except socket.timeout:
        return 'NO RESPONSE'


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('0.0.0.0', LISTEN_PORT))

    print("=== Probing Wing node tree for io/in structure ===")
    # Handshake
    print("\nHandshake (/?):", query_node(sock, '/?' ))
    time.sleep(0.3)

    # 1. Root node
    print("/ (root):")
    print(" ", query_node(sock, '/'))
    time.sleep(0.2)

    # 2. /io node
    print("\n/io:")
    print(" ", query_node(sock, '/io'))
    time.sleep(0.2)

    # 3. /io/in node
    print("\n/io/in:")
    print(" ", query_node(sock, '/io/in'))
    time.sleep(0.2)

    # 4. Try various capitalizations for group names
    for grp in ['A', 'a', 'USR', 'usr', 'LCL', 'lcl', 'SC', 'AES']:
        path = f'/io/in/{grp}'
        res = query_node(sock, path)
        print(f'\n/io/in/{grp}:')
        print(" ", res)
        time.sleep(0.15)

    # 5. Try /io/in/A/1
    for path in ['/io/in/A/1', '/io/in/A/01', '/io/in/a/1']:
        print(f'\n{path}:')
        print(" ", query_node(sock, path))
        time.sleep(0.15)

    # 6. Try /srcs or /src as an alternative (some consoles use different trees)
    for path in ['/srcs', '/src', '/io/src', '/cfg/src']:
        print(f'\n{path}:')
        print(" ", query_node(sock, path))
        time.sleep(0.15)

    # 7. Try altsw which the PDF says is in /io
    print("\n/io/altsw:")
    print(" ", query_node(sock, '/io/altsw'))
    time.sleep(0.15)

    sock.close()


if __name__ == '__main__':
    main()
