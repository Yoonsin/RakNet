import pandas as pd
import matplotlib.pyplot as plt
import os

# Setup Paths
base_dir = r"Reference/20260127 (Method_3)"
output_dir = "Analysis"
os.makedirs(output_dir, exist_ok=True)

# ---------------------------------------------------------
# 1. Load Data (Experiment 1 & 2 Overall)
# ---------------------------------------------------------
def load_data(suffix, server_folder):
    try:
        pc_tp = pd.read_csv(os.path.join(base_dir, f"PC/Network_Throughput{suffix}.csv"))
        mob_tp = pd.read_csv(os.path.join(base_dir, f"Mobile/Network_Throughput{suffix}.csv"))
        server = pd.read_csv(os.path.join(base_dir, f"Server/{server_folder}/serverStatLogs.csv"))
        return pc_tp, mob_tp, server
    except FileNotFoundError as e:
        print(f"Error loading {suffix}: {e}")
        return None, None, None

pc_tp_1, mob_tp_1, server_1 = load_data("(pc1m1)", "(p1m1)")
pc_tp_2, mob_tp_2, server_2 = load_data("(pc1m1o10)", "(p1m1o10)")

# ---------------------------------------------------------
# 2. Load Per-Client Server Logs (Exp 2 Only)
# ---------------------------------------------------------
try:
    server_dir_2 = os.path.join(base_dir, r"Server/(p1m1o10)")
    
    # Files identified based on user IP info
    # PC: 192.168.1.15
    # Mobile: 192.168.1.8
    # Other: 192.168.1.35 (Picking one representative)
    
    srv_pc = pd.read_csv(os.path.join(server_dir_2, "serverStatLogs_192.168.1.15-51195.csv"))
    srv_mob = pd.read_csv(os.path.join(server_dir_2, "serverStatLogs_192.168.1.8-36697.csv"))
    srv_other = pd.read_csv(os.path.join(server_dir_2, "serverStatLogs_192.168.1.35-51219.csv"))
    
except FileNotFoundError as e:
    print(f"Error loading per-client logs: {e}")
    exit(1)

# ---------------------------------------------------------
# Normalize Time
# ---------------------------------------------------------
def normalize(df):
    if df is not None and not df.empty:
        col = 'Timestamp(ms)'
        if col in df.columns:
            df['Elapsed(s)'] = (df[col] - df[col].iloc[0]) / 1000.0
    return df

for df in [pc_tp_1, mob_tp_1, server_1, pc_tp_2, mob_tp_2, server_2, srv_pc, srv_mob, srv_other]:
    normalize(df)

# ---------------------------------------------------------
# Chart Generation
# ---------------------------------------------------------

# --- Part 1: Previous Comparison Charts ---

# Mobile Throughput Impact (Exp 1 vs Exp 2)
plt.figure(figsize=(10, 5))
plt.plot(mob_tp_1['Elapsed(s)'], mob_tp_1['Throughput(B/s)'] / 1024.0, label='Exp 1 (Baseline)', color='green')
plt.plot(mob_tp_2['Elapsed(s)'], mob_tp_2['Throughput(B/s)'] / 1024.0, label='Exp 2 (Congested)', color='red', linestyle='--')
plt.title('Impact of Load on Mobile Throughput (Client View)', fontsize=14)
plt.xlabel('Time (s)', fontsize=12)
plt.ylabel('Received Throughput (KB/s)', fontsize=12)
plt.grid(True, alpha=0.5)
plt.legend()
plt.tight_layout()
plt.savefig(os.path.join(output_dir, 'Comp_Mobile_Throughput_Impact.png'))
plt.close()

# Server Buffer Growth (Exp 1 vs Exp 2)
plt.figure(figsize=(10, 5))
plt.plot(server_1['Elapsed(s)'], server_1['SendBuf(Low)'] / 1024.0, label='Exp 1 (Total Buffer)', color='green')
plt.plot(server_2['Elapsed(s)'], server_2['SendBuf(Low)'] / 1024.0, label='Exp 2 (Total Buffer)', color='red', linestyle='--')
plt.title('Global Server Congestion (Total Low Priority Buffer)', fontsize=14)
plt.xlabel('Time (s)', fontsize=12)
plt.ylabel('Buffer Size (KB)', fontsize=12)
plt.grid(True, alpha=0.5)
plt.legend()
plt.tight_layout()
plt.savefig(os.path.join(output_dir, 'Comp_Server_Buffer_Growth.png'))
plt.close()


# --- Part 2: NEW Server Per-Client Analysis ---

# 1. Server Sending Rate (Per Client)
plt.figure(figsize=(12, 6))
plt.plot(srv_pc['Elapsed(s)'], srv_pc['ActualBytesSent'] / 1024.0, label='To PC (1.15)', color='blue')
plt.plot(srv_mob['Elapsed(s)'], srv_mob['ActualBytesSent'] / 1024.0, label='To Mobile (1.8)', color='red')
plt.plot(srv_other['Elapsed(s)'], srv_other['ActualBytesSent'] / 1024.0, label='To Other (1.35)', color='gray', linestyle=':', alpha=0.7)

plt.title('Server Output per Client (Fairness Check)', fontsize=14)
plt.xlabel('Time (s)', fontsize=12)
plt.ylabel('Bytes Sent by Server (KB/s)', fontsize=12)
plt.grid(True, alpha=0.5)
plt.legend()
plt.tight_layout()
plt.savefig(os.path.join(output_dir, 'Server_PerClient_Output.png'))
plt.close()

# 2. Server Buffer Backlog (Per Client) - THE SMOKING GUN
plt.figure(figsize=(12, 6))
plt.plot(srv_pc['Elapsed(s)'], srv_pc['SendBuf(Low)'] / 1024.0, label='For PC (1.15)', color='blue')
plt.plot(srv_mob['Elapsed(s)'], srv_mob['SendBuf(Low)'] / 1024.0, label='For Mobile (1.8)', color='red')
plt.plot(srv_other['Elapsed(s)'], srv_other['SendBuf(Low)'] / 1024.0, label='For Other (1.35)', color='gray', linestyle=':', alpha=0.7)

plt.title('Server Buffer Backlog per Client (Bottleneck Identification)', fontsize=14)
plt.xlabel('Time (s)', fontsize=12)
plt.ylabel('Buffered Data (KB)', fontsize=12)
plt.grid(True, alpha=0.5)
plt.legend()
plt.tight_layout()
plt.savefig(os.path.join(output_dir, 'Server_PerClient_Buffer.png'))
plt.close()

print("Comparison charts and Per-Client Server analysis generated.")
