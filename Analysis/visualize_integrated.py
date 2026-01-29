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
        pc_tp = pd.read_csv(os.path.join(base_dir, f"PC/Network_Throughput{suffix}.csv"))
        mob_tp = pd.read_csv(os.path.join(base_dir, f"Mobile/Network_Throughput{suffix}.csv"))
        return pc_tp, mob_tp
    except FileNotFoundError as e:
        print(f"Error loading {suffix}: {e}")
        return None, None

pc_tp_1, mob_tp_1 = load_data("(pc1m1)")
pc_tp_2, mob_tp_2 = load_data("(pc1m1o10)")

# ---------------------------------------------------------
# Normalize Time
# ---------------------------------------------------------
def normalize(df):
    if df is not None and not df.empty:
        col = 'Timestamp(ms)'
        if col in df.columns:
            df['Elapsed(s)'] = (df[col] - df[col].iloc[0]) / 1000.0
    return df

for df in [pc_tp_1, mob_tp_1, pc_tp_2, mob_tp_2]:
    normalize(df)

# ---------------------------------------------------------
# NEW: Integrated Comparison Chart (PC/Mobile x Exp1/Exp2)
# ---------------------------------------------------------
plt.figure(figsize=(12, 6))

# Experiment 1 (Baseline) - Dotted Lines
plt.plot(pc_tp_1['Elapsed(s)'], pc_tp_1['Throughput(B/s)'] / 1024.0, 
         label='Exp1: PC (Baseline)', color='blue', linestyle=':', alpha=0.6)
plt.plot(mob_tp_1['Elapsed(s)'], mob_tp_1['Throughput(B/s)'] / 1024.0, 
         label='Exp1: Mobile (Baseline)', color='red', linestyle=':', alpha=0.6)

# Experiment 2 (Congested) - Solid Lines (Thicker)
plt.plot(pc_tp_2['Elapsed(s)'], pc_tp_2['Throughput(B/s)'] / 1024.0, 
         label='Exp2: PC (Congested)', color='blue', linewidth=2)
plt.plot(mob_tp_2['Elapsed(s)'], mob_tp_2['Throughput(B/s)'] / 1024.0, 
         label='Exp2: Mobile (Congested)', color='red', linewidth=2)

plt.title('Throughput Survival Analysis: PC vs Mobile (Baseline vs Congested)', fontsize=14)
plt.xlabel('Time (s)', fontsize=12)
plt.ylabel('Throughput (KB/s)', fontsize=12)
plt.grid(True, linestyle='--', alpha=0.7)
plt.legend(loc='upper right')

# Add annotation to highlight the "Starvation"
plt.annotate('Mobile Starvation', xy=(30, 0), xytext=(40, 500),
             arrowprops=dict(facecolor='red', shrink=0.05),
             fontsize=12, color='red', fontweight='bold')

plt.tight_layout()
plt.savefig(os.path.join(output_dir, 'Integrated_Throughput_Comparison.png'))
plt.close()

print("Generated 'Integrated_Throughput_Comparison.png'")
