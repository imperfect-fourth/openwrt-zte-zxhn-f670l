#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Wrap a JFFS2 image in a ZTE ZXHN F670L firmware bank header."""

import os
import struct
import sys
import time
import zlib

BLOCK = 0x20000
X = 0x28
PAYLOAD = 0x140 + X


def put(buf, off, data):
    buf[off:off + len(data)] = data


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: zte-bank.py <jffs2> <version> <output>")
    jffs2 = open(sys.argv[1], "rb").read()
    version = sys.argv[2].encode()[:15]

    # U-Boot scans the bank from offset 0x140; the JFFS2 starts on the next eraseblock
    body = bytes(BLOCK - PAYLOAD) + jffs2
    size = len(body)
    end = PAYLOAD + size

    hdr = bytearray(0x140)
    put(hdr, 0x00, struct.pack("<4I", 0x99999999, 0x44444444, 0x55555555, 0xaaaaaaaa))
    put(hdr, 0x10, struct.pack("<I", X))
    put(hdr, 0x4c, version)
    put(hdr, 0x68, struct.pack("<5I", 1, end, size, PAYLOAD, zlib.crc32(body)))
    put(hdr, 0x80, struct.pack("<I", end))
    # Both stock bank locations, as in the stock headers
    put(hdr, 0x88, struct.pack("<2I", 0x01a00000, 0x02000000))
    put(hdr, 0x98, struct.pack("<2I", 0x03a00000, 0x02000000))
    put(hdr, 0xa8, b"ZXHN F670L " + version)
    put(hdr, 0x104, struct.pack("<I", 1))
    built = int(os.environ.get("SOURCE_DATE_EPOCH", time.time()))
    put(hdr, 0x10c, time.strftime("%Y%m%d%H%M%S", time.gmtime(built)).encode())
    # 0x11c = 0 marks the bank bootable
    put(hdr, 0x120, struct.pack("<6I", 0xffffffff, 0xffffffff, 0x08000000,
                                0x0001ffff, 0, 0))
    put(hdr, 0x138, struct.pack("<2I", 0xf8000001, 0x00002001))
    put(hdr, 0x108, struct.pack("<I", zlib.crc32(hdr[0x3c:0x108])))

    with open(sys.argv[3], "wb") as f:
        f.write(hdr)
        f.write(bytes(X))
        f.write(body)


if __name__ == "__main__":
    main()
