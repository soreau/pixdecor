#!/usr/bin/python3

import os
import sys
from wayfire_socket import *

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} <app_id>")
    exit(1)

addr = os.getenv('WAYFIRE_SOCKET')

commands_sock = WayfireSocket(addr)

for view in commands_sock.list_views():
    if view["app-id"] == sys.argv[1]:
        commands_sock.shade_toggle(view["id"])
