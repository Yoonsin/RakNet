#!/usr/bin/python
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

ETH_P_IP = 0x0800  # IPv4

def h(major, minor=0):
    # "1:"  -> 0x00010000,  "1:1" -> 0x00010001
    return (major << 16) | minor

INGRESS = 0xffff0000  # parent/handle for ingress qdisc

#========================================================================#

def help():
    print("execute: {0} <net_interface>".format(sys.argv[0]))
    print("e.g.: {0} eno1\n".format(sys.argv[0]))
    exit(1)


INTERFACE = "eno1"
if len(sys.argv) != 2:
    help()
elif len(sys.argv) == 2:
    INTERFACE = sys.argv[1]
    #TODO : qdisc list selection

#========================================================================#
OUTPUT_INTERVAL = 1 # seconds (float avilable)
OUTPUT_DIR = "/home/parts/stats"
timestamp_str = datetime.now().strftime("%m%d_%H%M")
output_filename = f"net_monitor_log_{timestamp_str}.txt"
OUTPUT_FILEPATH = os.path.join(OUTPUT_DIR, output_filename)

log_buffer = []
LOG_BUFFER_SIZE = 100 

try:
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print(f"-> Log files will be saved to: {OUTPUT_FILEPATH}")
except OSError as e:
    print(f"Error creating directory {OUTPUT_DIR}: {e}")
    exit(1)

#========================================================================

ipr = IPRoute()
try:
    idx = ipr.link_lookup(ifname=INTERFACE)[0]
except IndexError:
    print(f"Error: Interface '{INTERFACE}' not found.")
    exit(1)

print("Cleaning up any leftover resources before starting...")

#root egress
try:
    ipr.tc("del", "prio", idx, "1:")
    print("-> Old root qdisc removed.")
except Exception:
    print("-> No old root qdisc (OK).")

#ingress 
try:
    ipr.tc("del", "ingress", idx, "ffff:")
    print("-> Old ingress qdisc removed.")
except Exception:
    print("-> No old ingress qdisc (OK).")

try:
    CShmReader(key=777, size=1200).clear()
    print("-> Old shared memory cleaned up.")
except sysv_ipc.ExistentialError:
    print("-> No old shared memory to clean up.")
try:
    CSemaphore(key=888).clear()
    print("-> Old semaphore cleaned up.")
except sysv_ipc.ExistentialError:
    print("-> No old semaphore to clean up.")

#========================================================================

def set_egress(name):
    pass

def shot_down_egress():
    pass

FLOW1_IP = "192.168.1.5" #(Mobile)
FLOW2_IP = "192.168.1.3" #(PC)

bpf = BPF(src_file="net_monitor.c")
#egress
# fn_egress_filter = bpf.load_func("handle_egress", BPF.SCHED_CLS)
# ipr.tc("add", "sfq", idx, "1:")
# ipr.tc("add-filter", "bpf", idx, ":1", fd=fn_egress_filter.fd,name=fn_egress_filter.name, parent="1:", action="ok", classid=1)
default_priomap = [1, 2, 2, 2, 1, 2, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
ipr.tc("add", "prio", idx, "1:" , bands=3, priomap=default_priomap)

prot = socket.htons(ETH_P_IP)

flow1_ip_hex = struct.unpack("!I", socket.inet_aton(FLOW1_IP))[0]
flow2_ip_hex = struct.unpack("!I", socket.inet_aton(FLOW2_IP))[0]

keys1 = [f'0x{flow1_ip_hex:x}/0xffffffff+16']
keys2 = [f'0x{flow2_ip_hex:x}/0xffffffff+16']

ipr.tc("add-filter", "u32", idx, 
           parent="1:",
           prio=10,
           protocol=prot,
           target="1:1", 
           keys=keys1)         

ipr.tc("add-filter", "u32", idx,
           parent="1:",
           prio=20,
           protocol= prot,
           target="1:2",
           keys=keys2)         

#ingress
#TODO : tcx_egress is more fast than tc_egress..?
#TODO : jemini present this code - ip.tc( "add", "link", idx, "egress", fd=bpf_prog_fd, name=bpf_prog_name ) is True?
fn_ingress_filter = bpf.load_func("handle_ingress", BPF.SCHED_CLS)
ipr.tc("add", "ingress", idx, "ffff:")
ipr.tc("add-filter", "bpf", idx, ":1", fd=fn_ingress_filter.fd,name=fn_ingress_filter.name, parent="ffff:", action="ok", classid=1)

print("BPF & TC rules have been set up.")

#========================================================================
sem = CSemaphore(key=888)
shm = CShmReader(key=777, size = 1200)
packet_cnt = bpf.get_table('packet_cnt')    # retrieve packet_cnt map
port = 0
user_cnt = 0
user_data = {}
strat_time_ns = time.monotonic_ns()
server_ip = "192.168.1.2"
NANO_TO_SEC = 1000000000
NANO_TO_MSEC = 1000000

def decimal_to_human(input_value):
    try:
        decimal_ip = int(input_value)
        ip_string = str(ipaddress.IPv4Address(decimal_ip))
        return ip_string
    except ValueError:
        return "Invalid input"

def read_app_info(data):
    #port|userCnt*UserData*UserData*UserData... 
	#UserData = IP/Platform/RTT/FPS
	#20123 | 1 * 192.168.1.3/PC/3/144 ... 
    tmp = data.split('|')
    port = tmp[0]
    user_cnt = tmp[1].split('*')[0]
    user_data = tmp[1].split('*')[1:]
    return port, user_cnt, user_data
    #ip : key / value : platform, rtt, fps

def print_event(cpu, data, size):
    event = bpf["events"].event(data)
    print("packet size %-16s " % (event.len))

#========================================================================

#bpf["events"].open_perf_buffer(print_event) 
try:
    print("\nMonitoring started... Press Ctrl+C to exit.")
    while True :
        time.sleep(OUTPUT_INTERVAL)
        #bpf.perf_buffer_poll()

        sem.release()
        if(sem.wait()):
            #print("Semaphore acquired, reading shared memory...")
            data, error = shm.doReadShm()
            if(error):
                pass #Same or Empty
                #print("[ERROR] %s", data)
            else:
                print ("Data read from shared memory:", data)
                port, user_cnt, pre_user_data = read_app_info(data)
                for item in pre_user_data:
                    parts = item.split('/')
                    ip = parts[0]
                    other = parts[1:]
                    user_data[ip] = other
                #print(user_data)
        else:
            print("Semaphore not acquired, skipping shared memory read.")

        #print(len(packet_cnt));
        for k, v in packet_cnt.items():
            src = decimal_to_human(str(k.srcip & 0xFFFFFFFF))
            dst = decimal_to_human(str(k.dstip & 0xFFFFFFFF))

            ingress_ok = (src in user_data and dst == server_ip) 
            egress_ok = (dst in user_data and src == server_ip)

            if(ingress_ok or egress_ok):
                elapsed = (v.ts_last - v.ts_init)
                throughput_bps = 0
                if elapsed > 0:
                    throughput_bps = (v.bytes * NANO_TO_SEC) / elapsed
                user = user_data[src if ingress_ok else dst]
                flow_dir = "[ingress]" if ingress_ok else "[egress]"
                #TODO : Create Instant Throughput 
                result = f'{flow_dir} / source address : {src} / destination address : {dst} / total packets : {v.packets} / sum packets (bytes) : {v.bytes} / avg packets (byte) : {v.avgBytes} / last duration (ms): {v.duration/NANO_TO_MSEC} / Bps : {throughput_bps : .2f} \
                platform : {user[0]} / RTT (ms) : {user[1]} / avg FPS : {user[2]} / capture time (ms) : {v.ts_last/NANO_TO_MSEC}'
                #print(result)

                log_buffer.append(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] {result}\n")
                if len(log_buffer) >= LOG_BUFFER_SIZE:
                    try:
                        with open(OUTPUT_FILEPATH, 'a') as f:
                            f.writelines(log_buffer)
                        log_buffer.clear()
                        # print(f"DEBUG: Flushed {LOG_BUFFER_SIZE} logs to file.") 
                    except IOError as e:
                        print(f"\n[ERROR] Could not write buffer to log file {OUTPUT_FILEPATH}: {e}")

        #packet_cnt.clear()
       
       
        
except KeyboardInterrupt:
    print("\nCtrl+C detected. Cleaning up resources...")

finally:
    print("Final release of the semaphore followed by a 5 second pause")
    
    # Ensure semaphore is released to avoid deadlock
    sem.release()
    time.sleep(5)
    print("Final acquisition of the semaphore")
    sem.wait()
    shm.clear()
    sem.clear()
    
    # Clean up BPF and TC rules
    print("-> Shared memory and semaphore removed.")
    try:
       ipr.tc("del", "prio", idx, "1:")
       ipr.tc("del", "ingress", idx, "ffff:")
       print("-> TC rules removed.")
    except Exception as e:
       print(f"-> Could not remove TC rules (may already be gone): {e}")
    
    # Write any remaining logs in the buffer to file
    if log_buffer:
        print(f"Writing remaining {len(log_buffer)} logs to file...")
        try:
            with open(OUTPUT_FILEPATH, 'a') as f:
                f.writelines(log_buffer)
            log_buffer.clear()
        except IOError as e:
            print(f"\n[ERROR] Could not write final logs to file: {e}")

    print("Cleanup & Log finished. Exiting.")
    sys.stdout.close()
    

