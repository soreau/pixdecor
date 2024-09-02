#!/usr/bin/python3

import os
import sys
from wayfire import WayfireSocket
from wayfire.extra.wpe import WPE

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} <view-id>")
    exit(1)

sock = WayfireSocket()
wpe = WPE(sock)

wpe.shade_toggle(int(sys.argv[1]))
