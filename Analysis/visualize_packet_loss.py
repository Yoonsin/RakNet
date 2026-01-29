import pandas as pd
import matplotlib.pyplot as plt
import os

# Setup Paths
base_dir = r"Reference/20260127 (Method_3)"
output_dir = "Analysis"
os.makedirs(output_dir, exist_ok=True)

# ---------------------------------------------------------
# Load Data
# ---------------------------------------------------------
def load_data(suffix):
    try:
        pc_pl = pd.read_csv(os.path.join(base_dir, f"PC/Network_PacketLoss{suffix}.csv"))
        mob_pl = pd.read_csv(os.path.join(base_dir, f"Mobile/Network_PacketLoss{suffix}.csv"))
        return pc_pl, mob_pl
    except FileNotFoundError as e:
        print(f"Error loading {suffix}: {e}")
        return None, None

pc_pl_1, mob_pl_1 = load_data("(pc1m1)")
pc_pl_2, mob_pl_2 = load_data("(pc1m1o10)")

# ---------------------------------------------------------
# Normalize Time
# ---------------------------------------------------------
def normalize(df):
    if df is not None and not df.empty:
        col = 'Timestamp(ms)'
        if col in df.columns:
            df['Elapsed(s)'] = (df[col] - df[col].iloc[0]) / 1000.0
    return df

for df in [pc_pl_1, mob_pl_1, pc_pl_2, mob_pl_2]:
    normalize(df)

# ---------------------------------------------------------
# Packet Loss Comparison Chart
# ---------------------------------------------------------
plt.figure(figsize=(12, 6))

# Experiment 1 (Baseline)
plt.plot(pc_pl_1['Elapsed(s)'], pc_pl_1['PacketLoss'], 
         label='Exp1: PC', color='blue', linestyle=':', alpha=0.6)
plt.plot(mob_pl_1['Elapsed(s)'], mob_pl_1['PacketLoss'], 
         label='Exp1: Mobile', color='red', linestyle=':', alpha=0.6)

# Experiment 2 (Congested)
# Using 'o-' markers to make data points distinct if sparse
plt.plot(pc_pl_2['Elapsed(s)'], pc_pl_2['PacketLoss'], 
         label='Exp2: PC', color='blue', linestyle='-', linewidth=1.5, alpha=0.8)
plt.plot(mob_pl_2['Elapsed(s)'], mob_pl_2['PacketLoss'], 
         label='Exp2: Mobile', color='red', linestyle='-', linewidth=2)

plt.title('Packet Loss Ratio Comparison (0.0 - 1.0)', fontsize=14)
plt.xlabel('Time (s)', fontsize=12)
plt.ylabel('Packet Loss Ratio', fontsize=12)
plt.ylim(-0.05, 1.05) # Fixed Y-axis from 0 to 1
plt.grid(True, linestyle='--', alpha=0.5)
plt.legend(loc='center right')

plt.tight_layout()
plt.savefig(os.path.join(output_dir, 'Integrated_PacketLoss_Comparison.png'))
plt.close()

print("Generated 'Integrated_PacketLoss_Comparison.png'")
