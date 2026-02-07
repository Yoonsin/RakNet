#!/usr/bin/python
from curses import ERR
import decimal
import sys
sys.dont_write_bytecode = True
import glob
from bcc import BPF
import sysv_ipc
import pyroute2
from pyroute2 import IPRoute, NetNS, IPDB, NSPopen, protocols
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
import subprocess
import numpy as np
from datetime import datetime

ETH_P_IP = 0x0800  # IPv4
INGRESS = 0xffff0000  # parent/handle for ingress qdisc
IFACE_TOTAL_RATE = "1gbit"
TC_SCRR_PATH = "/home/parts/iproute2-scrr/tc/tc" #os.path.expanduser("~/iproute2/tc/tc")
bpf_class_id = 100
bpf_class_map = {}
bpf = BPF(src_file="net_monitor.c")
packet_cnt = bpf.get_table('packet_cnt')    # retrieve packet_cnt map
ip_to_class_map = bpf.get_table('ip_to_class_map')   # retrieve ip_to_class_map map
def h(major, minor=0):
    # "1:"  -> 0x00010000,  "1:1" -> 0x00010001
    return (major << 16) | minor

def ip_to_hex_string(ip_string: str) -> str:
    """
    Converts an IPv4 address string to its hexadecimal string representation.

    Args:
        ip_string: The IPv4 address string (e.g., "192.168.0.0").

    Returns:
        The hexadecimal representation as a string (e.g., "0xc0a80000"),
        or an error message if the input is invalid.
    """
    try:
        # 1. Convert the IP string to its 4-byte packed binary representation.
        #    e.g., "192.168.0.0" -> b'\xc0\xa8\x00\x00'
        packed_ip = socket.inet_aton(ip_string)

        # 2. Unpack the 4 bytes into a single 32-bit unsigned integer.
        #    The '!' ensures network byte order (big-endian).
        #    b'\xc0\xa8\x00\x00' -> (3232235520,)
        ip_integer = struct.unpack("!I", packed_ip)[0]

        # 3. Format the integer as a hexadecimal string with a "0x" prefix.
        #    3232235520 -> "0xc0a80000"
        return f'0x{ip_integer:x}'

    except OSError:
        return "Error: Invalid IP address format."

def ip_to_int(ip_addr):
        return struct.unpack("!I", socket.inet_aton(ip_addr))[0]

def handle_str_to_int(handle_str: str) -> int:
    """
    "1:100" -> 0x10064
    """
    try:
        # 1. "1:100" ["1", "100"]
        parts = handle_str.split(':')
        
        major = int(parts[0])
        minor = int(parts[1])
        
        handle_int = (major << 16) | minor
        
        return handle_int

    except (ValueError, IndexError):
        print(f"error: '{handle_str}'isn't correct 'major:minor' format. ")
        return 0

def run_tc_command(tc_binary_path, args_str):
    global INTERFACE 
    cmd = ["sudo", tc_binary_path] + args_str.split()
    
    if "dev" not in args_str:
        cmd.extend(["dev", INTERFACE])
    try:
        subprocess.run(cmd, check=True, capture_output=True, text=True, timeout=5)
    except subprocess.CalledProcessError as e:
        print(f"Error executing: {' '.join(cmd)}")
        print(f"Stderr: {e.stderr}")
        raise 
    except subprocess.TimeoutExpired:
        print(f"Timeout executing: {' '.join(cmd)}")
        raise

def decimal_to_human(input_value):
    try:
        decimal_ip = int(input_value)
        ip_string = str(ipaddress.IPv4Address(decimal_ip))
        return ip_string
    except ValueError:
        return "Invalid IP"

def read_app_info(data):
    #port|userCnt*UserData*UserData*UserData... 
	#UserData = IP/Platform/RTT/FPS
	#20123 | 1 * 192.168.1.3/PC/3/144 ... 
    tmp = data.split('|')
    port = tmp[0]
    user_cnt = int(tmp[1])
    method_mask = int(tmp[2].split('*')[0])
    user_data = tmp[2].split('*')[1:]
    return port, user_cnt, method_mask, user_data
    #ip : key / value : platform, rtt, lastping,  fps

def print_event(cpu, data, size):
    event = bpf["events"].event(data)
    print("class_id %-16s " % (event.len))

def help():
    print("execute: {0} <net_interface>".format(sys.argv[0]))
    print("e.g.: {0} eno1\n".format(sys.argv[0]))
    print("  <qdisc_type> can be one of: default, prio, htb, bpf, scrr, hls")
    print("  <flow_type> can be one of : egress, ingress, both")
    exit(1)

#========================================================================
def setup_fq_codel(ipr, idx, **kwargs):
    #""" FQ-CODsingaporeEL is the default, so no specific setup is needed. """
    print("-> Using kernel default fq_codel qdisc. No setup needed.")
    pass

def teardown_fq_codel(ipr, idx, **kwargs):
    # """ All qdiscs are removed by the main teardown, so no individual action is needed. """
    pass

def setup_prio(ipr, idx, **kwargs):
    FLOW1_IP = kwargs.get('FLOW1_IP')
    FLOW2_IP = kwargs.get('FLOW2_IP')
    # """ PRIO Qdisc: Gives high priority to FLOW1_IP. """
    default_priomap = [1, 2, 2, 2, 1, 2, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

    print("Setting up PRIO qdisc...")
    ipr.tc("add", "prio", idx, "1:" , bands=3, priomap=default_priomap)
    
    ipr.tc("add", "fq_codel", idx, parent=0x10001, fqc_limit = 10240, fqc_flows = 1024, fqc_quantum = 1514, fqc_ecn = 64)
    ipr.tc("add", "fq_codel", idx, parent=0x10002, fqc_limit = 10240, fqc_flows = 1024, fqc_quantum = 1514, fqc_ecn = 64)
    ipr.tc("add", "fq_codel", idx, parent=0x10003, fqc_limit = 10240, fqc_flows = 1024, fqc_quantum = 1514, fqc_ecn = 64)

    ipr.tc("add-filter", "u32", idx, parent=0x10000, prio=1, protocol=protocols.ETH_P_IP, target=0x10001, keys=[f"{ip_to_hex_string(FLOW1_IP)}/0xffffffff+16"])
    ipr.tc("add-filter", "u32", idx, parent=0x10000, prio=2, protocol=protocols.ETH_P_IP, target=0x10002, keys=[f"{ip_to_hex_string(FLOW2_IP)}/0xffffffff+16"])
    ipr.tc("add-filter", "u32", idx,parent=0x10000,prio=3,protocol=protocols.ETH_P_ALL, target=0x10003, keys=["0x0/0x0+0"])
    # 0xffffffff = 255.255.255.255 (/32)
    # 12 = Source network field bit offset
    # 16 = Destination network field bit offset

    print("PRIO qdisc & u32 filter setup is complete.")

def teardown_prio(ipr, idx, **kwargs):
    #""" All qdiscs are removed by the main teardown, so no individual action is needed. """
    pass

def setup_htb(ipr, idx, **kwargs):
    # """ HTB Qdisc: Assigns higher bandwidth to FLOW1_IP. """
    FLOW1_IP = kwargs.get('FLOW1_IP')
    FLOW2_IP = kwargs.get('FLOW2_IP')
    print("Setting up HTB qdisc...")
    ipr.tc("add", "htb", idx, 0x10000, default=0x10030)

    ipr.tc("add-class", "htb", idx, 0x10001, parent=0x10000, rate="500mbit")
    #egress data
    ipr.tc("add-class", "htb", idx, 0x10010, parent=0x10001, rate="100mbit", burst=1024 * 6, prio=1)
    ipr.tc("add-class", "htb", idx, 0x10020, parent=0x10001, rate="70kbit", burst=1024 * 6, prio=2)
    ipr.tc("add-class", "htb", idx, 0x10030, parent=0x10001, rate="20kbit", burst=1024 * 6, prio=3)
    
    ipr.tc("add", "fq_codel", idx, parent=0x10010, fqc_limit = 10240, fqc_flows = 1024, fqc_quantum = 1514, fqc_ecn = 64)
    ipr.tc("add", "fq_codel", idx, parent=0x10020, fqc_limit = 10240, fqc_flows = 1024, fqc_quantum = 1514, fqc_ecn = 64)
    ipr.tc("add", "fq_codel", idx, parent=0x10030, fqc_limit = 10240, fqc_flows = 1024, fqc_quantum = 1514, fqc_ecn = 64)
    
    # (option) flow 2 netem
    # ipr.tc("add", "fq_codel", idx, parent=0x10010, fqc_limit = 10240, fqc_flows = 1024, fqc_quantum = 1514, fqc_ecn = 64)
    # ipr.tc("add", "netem", idx, parent=0x10020, handle=0x1100000, delay=1000000, jitter=0 )
    # ipr.tc("add", "fq_codel", idx, parent=0x1100000, fqc_limit = 10240, fqc_flows = 1024, fqc_quantum = 1514, fqc_ecn = 64)

    ipr.tc("add-filter", "u32", idx, parent=0x10000, prio=1, protocol=protocols.ETH_P_IP, target=0x10010, keys=[f"{ip_to_hex_string(FLOW1_IP)}/0xffffffff+16"])
    ipr.tc("add-filter", "u32", idx, parent=0x10000, prio=2, protocol=protocols.ETH_P_IP, target=0x10020, keys=[f"{ip_to_hex_string(FLOW2_IP)}/0xffffffff+16"])

    print("HTB qdisc & u32 filter setup is complete.")

def teardown_htb(ipr, idx, **kwargs):
    #""" All qdiscs are removed by the main teardown, so no individual action is needed. """
    pass

def setup_bpf(ipr, idx, **kwargs):
    #""" BPF Qdisc: Attaches an eBPF program as an egress filter. """
    bpf = kwargs.get('bpf')
    print("Setting up BPF filter...")
    # Load the eBPF program and add the filter
    
    # htb root
    ipr.tc("add", "htb", idx, 0x10000, default=0x10030)
    ipr.tc("add-class", "htb", idx, 0x10001, parent=0x10000, rate=IFACE_TOTAL_RATE)
    
    # htb default class 
    ipr.tc("add-class", "htb", idx, 0x10030, parent=0x10001, rate=IFACE_TOTAL_RATE, burst=1024 * 6, prio=3)
    ipr.tc("add", "fq_codel", idx, parent=0x10030, fqc_limit = 10240, fqc_flows = 1024, fqc_quantum = 1514, fqc_ecn = 64)

    # ebpf egress filter
    fn_egress_filter = bpf.load_func("handle_egress", BPF.SCHED_CLS)
    #ipr.tc("add-filter", "bpf", idx, fd=fn_egress_filter.fd, name=fn_egress_filter.name, parent=0x10000, prio=1, direct_action=True)    
    ipr.tc("add-filter", "bpf", idx, fd=fn_egress_filter.fd, name=fn_egress_filter.name, parent=0x10000, prio=1)    
    
    print("BPF filter has been attached to fq_codel qdisc.")
    print("Setting up bpf qdisc...")

def start_bpf(ipr, idx, user_data):
    #assign ip_list to delay
    global bpf_class_id
    for ip in user_data.keys():
        class_id_num = bpf_class_id # 100
        class_id_hex_str = f"{class_id_num:x}" # 100 -> "64", 256 -> "100"
        class_id_str = str(class_id_num) 
        minor_id_int = int(class_id_str, 16) # 0x100
        class_handle_str = f"1:{class_id_str}"   # "1:100"
        netem_handle_str = f"{class_id_str}:"   # "100:"
        class_handle_int = minor_id_int # 0x100 (256)

        ipr.tc("add-class", "htb", idx, class_handle_str, parent=0x10001, rate=IFACE_TOTAL_RATE, burst=1024 * 6, prio=1)
        ipr.tc("add", "netem", idx, parent=class_handle_str, handle=netem_handle_str, delay=0, jitter=0 )
        ipr.tc("add", "fq_codel", idx, parent=netem_handle_str, fqc_limit = 10240, fqc_flows = 1024, fqc_quantum = 1514, fqc_ecn = 64)

        bpf_class_map[ip] = [class_id_num, class_handle_str]
        bpf_class_id += 1 # 101

        ip_int = ip_to_int(ip);
        ip_to_class_map[ip_to_class_map.Key(ip_int)] =  ip_to_class_map.Leaf(class_handle_int)
        print(f"[start_bpf] c_format : {ip_int} / py_format : {ip} / class_id_minor : {hex(class_handle_int)} ")

    #print("start gotta!")

def change_bpf(ipr, idx, ip, delay_val, jitter_val = 0):
    class_handle_str = bpf_class_map[ip][1]
    #delay_val in us 
    ipr.tc("change", "netem", idx, parent=class_handle_str, delay=(delay_val*1000), jitter=(jitter_val*1000))
    #print("change gotta!")

def teardown_bpf(ipr, idx, **kwargs):
    #""" All qdiscs are removed by the main teardown, so no individual action is needed. """
    pass

def setup_scrr(ipr, idx, **kwargs):
    print("-> Using kernel default scrr qdisc. No setup needed.")
    print("Setting up SCRR qdisc using subprocess...")
    try:
        run_tc_command(TC_SCRR_PATH, f"qdisc replace dev {INTERFACE} root scrr")
        print("SCRR qdisc setup is complete.")
    except Exception as e:
        print(f"Error setting up SCRR: {e}")
        exit(1)

    pass

def teardown_scrr(ipr, idx, **kwargs):
    # """ All qdiscs are removed by the main teardown, so no individual action is needed. """
    print("Tearing down SCRR qdisc...")
    try:
        run_tc_command(TC_SCRR_PATH, f"qdisc del dev {INTERFACE} root")
    except Exception as e:
        print(f"-> Could not remove SCRR qdisc (may already be gone): {e}")
    pass

def setup_hls(ipr, idx, **kwargs):
    """
    - 1: (0x10000) (root)
        - 1:1 (0x10001) (main class)
            - 1:16 (0x10010) (prio 1) -> FLOW1_IP
            - 1:32 (0x10020) (prio 2) -> FLOW2_IP
            - 1:48 (0x10030) (prio 3) -> default
    
    - HTB prio 1 -> HLS weight 100
    - HTB prio 2 -> HLS weight 50
    - HTB prio 3 -> HLS weight 10
    """
    FLOW1_IP = kwargs.get('FLOW1_IP')
    FLOW2_IP = kwargs.get('FLOW2_IP')
    print("Setting up HLS qdisc (via subprocess)...")
    
    try:
        run_tc_command(TC_SCRR_PATH, f"qdisc add root handle 1: hls default 48")
        run_tc_command(TC_SCRR_PATH, f"class add parent 1: classid 1:1 hls weight 1000 max_packet_size 1500")

        # (HTB prio 1 -> HLS weight 100)
        run_tc_command(TC_SCRR_PATH, f"class add parent 1:1 classid 1:16 hls weight 100 max_packet_size 1500")
        
        # (HTB prio 2 -> HLS weight 50)
        run_tc_command(TC_SCRR_PATH, f"class add parent 1:1 classid 1:32 hls weight 50 max_packet_size 1500")

        # (HTB prio 3 -> HLS weight 10) (default)
        run_tc_command(TC_SCRR_PATH, f"class add parent 1:1 classid 1:48 hls weight 10 max_packet_size 1500")
        
        run_tc_command(TC_SCRR_PATH, f"qdisc add parent 1:16 fq_codel limit 10240 flows 1024 quantum 1514 ecn drop_batch 64")
        run_tc_command(TC_SCRR_PATH, f"qdisc add parent 1:32 fq_codel limit 10240 flows 1024 quantum 1514 ecn drop_batch 64")
        run_tc_command(TC_SCRR_PATH, f"qdisc add parent 1:48 fq_codel limit 10240 flows 1024 quantum 1514 ecn drop_batch 64")

        # FLOW1_IP -> 1:16
        run_tc_command(TC_SCRR_PATH, f"filter add parent 1: prio 1 protocol ip u32 match ip dst {FLOW1_IP} flowid 1:16")
        # FLOW2_IP -> 1:32
        run_tc_command(TC_SCRR_PATH, f"filter add parent 1: prio 2 protocol ip u32 match ip dst {FLOW2_IP} flowid 1:32")

        print("HLS qdisc & u32 filter setup is complete.")

    except Exception as e:
        print(f"Error setting up HLS: {e}")
        try:
            run_tc_command(TC_SCRR_PATH, f"qdisc del root")
        except:
            pass 
        exit(1)

def teardown_hls(ipr, idx, **kwargs):
    print("Tearing down HLS qdisc...")
    try:
        run_tc_command(TC_SCRR_PATH, f"qdisc del dev {INTERFACE} root")
    except Exception as e:
        print(f"-> Could not remove HLS qdisc (may already be gone): {e}")
    pass

#========================================================================

INTERFACE = "eno1"
FLOW = "ingress"
if len(sys.argv) != 4:
    help()
    
INTERFACE = sys.argv[1]
QDISC_CHOICE = sys.argv[2]
FLOW_CHOICE = sys.argv[3]

qdisc_map = {
        "default": (setup_fq_codel, teardown_fq_codel),
        "prio": (setup_prio, teardown_prio),
        "htb": (setup_htb, teardown_htb),
        "bpf": (setup_bpf, teardown_bpf),
        "scrr": (setup_scrr, teardown_scrr),
        "hls": (setup_hls, teardown_hls)
 }

flow_list = {"ingress", "egress", "both"}

if QDISC_CHOICE not in qdisc_map:
        print(f"Error: Invalid qdisc type '{QDISC_CHOICE}'")
        help()

if FLOW_CHOICE not in flow_list:
        print(f"Error: Invalid Flow type '{FLOW_CHOICE}'")
        help()

# Select the setup/teardown functions to use
setup_func, teardown_func = qdisc_map[QDISC_CHOICE]

# --- Initial Setup ---
OUTPUT_INTERVAL = 1 # seconds (float avilable)
OUTPUT_DIR = "/home/parts/stats"
timestamp_str = datetime.now().strftime("%m%d_%H%M")
output_filename = f"net_monitor_log_{timestamp_str}.txt"
OUTPUT_FILEPATH = os.path.join(OUTPUT_DIR, output_filename)
log_buffer = []
LOG_BUFFER_SIZE = 100 
FLOW1_IP = "192.168.1.8" #(Mobile)
FLOW2_IP = "192.168.1.3" #(PC)
ipr = IPRoute()

try:
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print(f"-> Log files will be saved to: {OUTPUT_FILEPATH}")
except OSError as e:
    print(f"Error creating directory {OUTPUT_DIR}: {e}")
    exit(1)

# --- Resource Cleanup and TC/BPF Setup ---
try:
    idx = ipr.link_lookup(ifname=INTERFACE)[0]
except IndexError:
    print(f"Error: Interface '{INTERFACE}' not found.")
    exit(1)

print("Cleaning up any leftover resources before starting...")

#root egress
#default qidsc is fq_codel
#ref : https://github.com/svinota/pyroute2/blob/master/pyroute2/netlink/rtnl/tcmsg/sched_template.py
#ref : https://github.com/svinota/pyroute2/issues/801 one by one
try:
    ipr.tc('del', idx)
    print("-> all qdisc delete.")
except Exception as e:
    print(f"-> No old root qdisc (OK). : {e}")

try:
    ipr.tc('del', 'ingress', idx)
    print("-> ingress qdisc delete.")
except Exception as e:
    print(f"-> No old ingress qdisc (OK). : {e}")

try:
    ipr.tc('del', 'clsact', idx)
    print("-> clsact qdisc delete.")
except Exception as e:
    print(f"-> No old clsact qdisc (OK). : {e}")


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

#ingress (common for all modes)
#TODO : tcx_egress is more fast than tc_egress..?
#TODO : jemini present this code - ip.tc( "add", "link", idx, "egress", fd=bpf_prog_fd, name=bpf_prog_name ) is True?
if FLOW_CHOICE in ["ingress", "both"]:
    print(f"-> Attaching INGRESS filter (Flow: {FLOW_CHOICE})...")
    try:
        ipr.tc("add", "ingress", idx, "ffff:")
    except Exception:
        pass 

    fn_ingress_filter = bpf.load_func("handle_ingress", BPF.SCHED_CLS)
    ipr.tc("add-filter", "bpf", idx, ":1", fd=fn_ingress_filter.fd, name=fn_ingress_filter.name, parent="ffff:", action="ok", classid=1)

if FLOW_CHOICE in ["egress", "both"]:
    if QDISC_CHOICE != "bpf":
        print(f"-> Attaching EGRESS filter (Flow: {FLOW_CHOICE}, QDisc: {QDISC_CHOICE})...")
        try:
            ipr.tc("add", "clsact", idx)
        except Exception:
            pass
        fn_egress_filter = bpf.load_func("handle_egress", BPF.SCHED_CLS)
        try:
            ipr.tc("add-filter", "bpf", idx, ":1", fd=fn_egress_filter.fd, 
                   name=fn_egress_filter.name, 
                   parent="ffff:fff3", action="ok", classid=1)
            print("-> EGRESS filter attached to clsact.")
        except Exception as e:
            print(f"-> Failed to attach egress filter: {e}")
    else:
        print("-> EGRESS filter already attached by setup_bpf().")
print("BPF & TC rules have been set up.")

#========================================================================

sem = CSemaphore(key=888)
shm = CShmReader(key=777, size = 1200)
setup_func(ipr, idx, FLOW1_IP=FLOW1_IP, FLOW2_IP=FLOW2_IP, bpf=bpf)
port = 0
user_cnt = 0
user_data = {}
strat_time_ns = time.monotonic_ns()
server_ip = "192.168.1.2"
NANO_TO_SEC = 1000000000
NANO_TO_MSEC = 1000000
is_game_start = False

#=============================Method=====================================
rtt_total_count = 0      
rtt_total_mean = 0.0    
rtt_total_m2 = 0.0      

rtt_cumulative_mean = 0.0
rtt_cumulative_std_dev = 0.0

congest_cnt = 1 # mobile device cnt
method_mask = 0

# (1 << 0) = 1 (001)
# (1 << 1) = 2 (010)
# (1 << 2) = 4 (100)
METHOD_1 = 1;
METHOD_2 = 2;
METHOD_3 = 4;


def engress_delay(ipr,idx):
    global user_data
    global congest_cnt 
    
    current_rtts = []
    valid_ips = []
    
    for ip, data in user_data.items():
        if len(data) > 1 and isinstance(data[1], str) and data[1].isdigit():
            rtt_val = int(data[1])
            if rtt_val > 0: 
                current_rtts.append(rtt_val)
                valid_ips.append(ip)

    if not current_rtts:
        return

    batch_mean = np.mean(current_rtts)
    batch_std = np.std(current_rtts)

    raw_threshold = batch_mean - batch_std
    
    threshold = max(raw_threshold, 1.0) 

    print(f"Stats: Mean={batch_mean:.2f}, Std={batch_std:.2f}, Threshold={threshold:.2f}")

    for ip in valid_ips:
        try:
            data = user_data[ip]
            current_rtt = int(data[1])
            
            delay_val = 0
            
            if current_rtt <= threshold:                
                safe_rtt = max(current_rtt, 1)
                calculated_delay = congest_cnt * (threshold / safe_rtt)
                delay_val = int(calculated_delay)
                delay_val = min(delay_val, 100) 

            if delay_val > 0:
                print(f"Apply Delay to {ip}: RTT={current_rtt}ms <= Thr={threshold:.1f} -> Add {delay_val}ms")
            
            change_bpf(ipr, idx, ip, delay_val)
            
        except (ValueError, IndexError) as e:
            print(f"[ERROR] Processing IP {ip}: {e}")
def ingress_delay(ipr,idx):
    pass

def congestion_control(ipr,idx,is_ingress):
    global congest_cnt
    global user_data
    
    if is_ingress == true:
        pass
    else:
        pass

    

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
                port, user_cnt, method_mask, pre_user_data = read_app_info(data)
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
                
                result = f'{flow_dir} / source address : {src} / destination address : {dst} / total packets : {v.packets} / sum packets (bytes) : {v.bytes} / avg packets (byte) : {v.avgBytes} / last duration (ms): {v.duration/NANO_TO_MSEC} / Bps (average, byte) : {throughput_bps : .2f} / Bps (instant,byte) : {v.throughput_instant : .2f} \
                platform : {user[0]} / RTT (ms) : {user[1]} / RTT (ms,last) : {user[2]} / avg FPS : {user[3]} / capture time (ms) : {v.ts_last/NANO_TO_MSEC}'
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

                if(is_game_start == False and QDISC_CHOICE == "bpf"):
                    is_game_start = True
                    start_bpf(ipr, idx, user_data)

        for k,v in ip_to_class_map.items():
            src = decimal_to_human(str(k.value & 0xFFFFFFFF))
            class_id = v.value
            #print(f"[while] c_format : {k.value} / py_format : {src} / class_id : {class_id} " )

        if user_data and QDISC_CHOICE == "bpf":
            if (method_mask & METHOD_1):
                engress_delay(ipr,idx)
            if (method_mask & METHOD_3):
                congestion_control(ipr, idx, False)
          
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
       ipr.tc('del', idx)
       print("-> TC rules removed.")
    except Exception as e:
       print(f"-> Could not remove TC rules (may already be gone): {e}")

    try:
       ipr.tc('del', 'ingress', idx)
       print("-> TC ingress rules removed.")
    except Exception as e:
       print(f"-> Could not remove TC ingress rules (may already be gone): {e}")
    
    try:
        ipr.tc('del', 'clsact', idx)
        print("-> clsact qdisc delete.")
    except Exception as e:
        print(f"-> No old clsact qdisc (OK). : {e}")

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
    

