import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os
import numpy as np

# Setup Paths
base_dir = r"Reference/20260127 (Method_3)"
output_dir = "Analysis"
os.makedirs(output_dir, exist_ok=True)

# ---------------------------------------------------------
# Load Data
# ---------------------------------------------------------
def load_throughput(suffix):
    try:
        pc = pd.read_csv(os.path.join(base_dir, f"PC/Network_Throughput{suffix}.csv"))
        mob = pd.read_csv(os.path.join(base_dir, f"Mobile/Network_Throughput{suffix}.csv"))
        
        # Add Group Labels
        pc['Group'] = f'PC {suffix}'
        mob['Group'] = f'Mobile {suffix}'
        
        # Convert Bytes to KB
        pc['Throughput_KB'] = pc['Throughput(B/s)'] / 1024.0
        mob['Throughput_KB'] = mob['Throughput(B/s)'] / 1024.0
        
        return pc, mob
    except FileNotFoundError:
        return None, None

# Load Datasets
pc1, mob1 = load_throughput("(pc1m1)")
pc2, mob2 = load_throughput("(pc1m1o10)")

# Combine into one DataFrame
df_all = pd.concat([pc1, mob1, pc2, mob2], ignore_index=True)

# Rename Groups for Readability
group_map = {
    'PC (pc1m1)': 'Exp1: PC (Baseline)',
    'Mobile (pc1m1)': 'Exp1: Mobile (Baseline)',
    'PC (pc1m1o10)': 'Exp2: PC (Congested)',
    'Mobile (pc1m1o10)': 'Exp2: Mobile (Congested)'
}
df_all['Group'] = df_all['Group'].replace(group_map)

# ---------------------------------------------------------
# Chart 1: Box Plot (Distribution Comparison)
# ---------------------------------------------------------
plt.figure(figsize=(10, 6))
sns.boxplot(x='Group', y='Throughput_KB', data=df_all, palette=['skyblue', 'lightcoral', 'blue', 'red'])

plt.title('Throughput Distribution Comparison (No Time Alignment)', fontsize=14)
plt.ylabel('Throughput (KB/s)', fontsize=12)
plt.xlabel('')
plt.xticks(rotation=15)
plt.grid(True, axis='y', linestyle='--', alpha=0.5)
plt.tight_layout()
plt.savefig(os.path.join(output_dir, 'Stat_BoxPlot_Throughput.png'))
plt.close()

# ---------------------------------------------------------
# Chart 2: ECDF (Cumulative Distribution)
# ---------------------------------------------------------
plt.figure(figsize=(10, 6))
sns.ecdfplot(data=df_all, x='Throughput_KB', hue='Group', palette=['skyblue', 'lightcoral', 'blue', 'red'], linewidth=2)

plt.title('Empirical Cumulative Distribution Function (ECDF)', fontsize=14)
plt.xlabel('Throughput (KB/s)', fontsize=12)
plt.ylabel('Proportion of Time', fontsize=12)
plt.grid(True, linestyle='--', alpha=0.5)

# Annotation for interpretation
plt.text(0.5, 0.5, "Curves to the LEFT = Lower Performance\nCurves to the RIGHT = Higher Performance", 
         transform=plt.gca().transAxes, fontsize=10, bbox=dict(facecolor='white', alpha=0.8))

plt.tight_layout()
plt.savefig(os.path.join(output_dir, 'Stat_ECDF_Throughput.png'))
plt.close()

print("Generated Statistical Charts (Box Plot & ECDF)")
