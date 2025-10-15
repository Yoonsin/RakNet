#!/usr/bin/env python3
from curses import ERR
import decimal
import sys
sys.dont_write_bytecode = True
import glob
from bcc import BPF
import sysv_ipc
import pyroute2
from pyroute2 import IPRoute
import time
from ast import literal_eval
from shared_mem import CSemaphore, CShmReader
from ctypes import *
import ctypes as ct
import sys
import socket
import os
import struct
import ipaddress
import ctypes
from datetime import datetime
import socket
from pyroute2.netlink.rtnl import tcmsg

def get_pyroute2_qdiscs():

    return sorted(tcmsg.modules.keys())

if __name__ == "__main__":
    qdisc_list = get_pyroute2_qdiscs()

    print("-" * 70)

    columns = 4
    max_len = max(len(q) for q in qdisc_list) + 2
    for i, qdisc in enumerate(qdisc_list):
        print(f"{qdisc:<{max_len}}", end="")
        if (i + 1) % columns == 0 or i == len(qdisc_list) - 1:
            print()

    print("-" * 70)
  
