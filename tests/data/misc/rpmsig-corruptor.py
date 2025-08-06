#!/usr/bin/env python3

import sys
import base64

DEBUG = False

def debug(s):
    if DEBUG:
        print(s, file=sys.stderr)

if len(sys.argv) == 2: # corrupt all signatures
    corrupt_index = -1
elif len(sys.argv) == 3: # corrupt one signature
    if sys.argv[2] == 'all':
        corrupt_index = -1
    else:
        corrupt_index = int(sys.argv[2])
else:
    print('Unexpected args', file=sys.stderr)
    sys.exit(1)

rpm_file = sys.argv[1]

with open(rpm_file, 'rb') as f:
    data = f.read()

sigheader = data.index(b'\x8e\xad\xe8')
debug(data[sigheader:sigheader+12])
entries_num = int.from_bytes(data[sigheader+8:sigheader+12])
debug(f'Number of entries_num: {entries_num}')
entries_start = sigheader + 16
size_offset = -1
gpg_offset = -1
gpg_num = -1
for i in range(entries_num):
    entry_start = entries_start + i*16
    tag = int.from_bytes(data[entry_start:entry_start+4])
    typ = int.from_bytes(data[entry_start+4:entry_start+8])
    offset = int.from_bytes(data[entry_start+8:entry_start+12])
    count = int.from_bytes(data[entry_start+12:entry_start+16])
    if tag == 1000:
        size_offset = offset
    elif tag == 278:
        gpg_offset = offset
        gpg_num = count
    debug(f'{i} {tag} {typ} {offset} {count}')
payload_start = entries_start + entries_num*16 + 16
size = int.from_bytes(data[payload_start+size_offset:payload_start+size_offset+4])
debug(f'size {size}')
gpg_last = payload_start + gpg_offset
if corrupt_index == -1:
    corrupt_index = range(gpg_num)
else:
    corrupt_index = [corrupt_index]
for i in range(gpg_num):
    start = gpg_last
    end = data.index(b'\x00', start)
    if i in corrupt_index:
        debug(f"Corrupting signature {i}")
        data_b64 = data[start:end]
        data_raw = base64.b64decode(data_b64)
        index = (end-start) // 2
        corrupted_raw = bytearray(data_raw)
        corrupted_raw[index] = ~ corrupted_raw[index] & 0xFF
        corrupted_b64 = base64.b64encode(corrupted_raw)
        data = data[:start] + corrupted_b64 + data[end:]

    #debug(f'{i}: start {start} end {end} {h}')
    gpg_last = end + 1
gpg = int.from_bytes(data[payload_start+gpg_offset:payload_start+gpg_offset+4])

with open(rpm_file, 'wb') as f:
    f.write(data)
