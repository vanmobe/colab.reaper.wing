#!/usr/bin/env python3
import pypdf
import sys

path = '/Users/benvanmol/Documents/code/CO_LAB/colab.reaper.recordwing/docs/OSC_Documentation.pdf'
reader = pypdf.PdfReader(path)
text = ''
for page in reader.pages:
    text += page.extract_text() + '\n'
lines = text.split('\n')

mode = sys.argv[1] if len(sys.argv) > 1 else 'link'

if mode == 'link':
    print(f"=== Total pages: {len(reader.pages)}, lines: {len(lines)} ===\n")
    print("=== 'link' and 'stereo' occurrences ===")
    for i, line in enumerate(lines):
        l = line.lower()
        if ('link' in l or 'stereo' in l) and len(line.strip()) > 3:
            print(f'L{i:4d}: {line}')

elif mode == 'query':
    print("=== object model / query mechanism ===")
    for i, line in enumerate(lines):
        l = line.lower()
        if any(kw in l for kw in ['node', 'object', 'query', '/node', 'get ', '?', 'model']):
            if len(line.strip()) > 3:
                print(f'L{i:4d}: {line}')

elif mode == 'io':
    print("=== /io/ paths ===")
    for i, line in enumerate(lines):
        if '/io/' in line and len(line.strip()) > 3:
            for j in range(max(0,i-1), min(len(lines),i+3)):
                print(f'L{j:4d}: {lines[j]}')
            print('---')

elif mode == 'ha':
    print("=== HA / head amp section ===")
    for i, line in enumerate(lines):
        l = line.lower()
        if ' ha ' in l or '/ha' in l or 'head amp' in l or l.strip() == 'ha':
            for j in range(max(0,i-1), min(len(lines),i+6)):
                print(f'L{j:4d}: {lines[j]}')
            print('---')

elif mode == 'toc':
    print("=== First 200 lines (TOC/structure) ===")
    for i, line in enumerate(lines[:200]):
        if line.strip():
            print(f'L{i:4d}: {line}')

elif mode == 'dump':
    start = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    end = int(sys.argv[3]) if len(sys.argv) > 3 else start + 80
    for i in range(start, min(end, len(lines))):
        print(f'L{i:4d}: {lines[i]}')
