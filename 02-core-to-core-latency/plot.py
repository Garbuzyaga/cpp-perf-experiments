# Heatmap from core_latency.csv
# Usage: python plot.py "i7-11700F"
# Needs: pip install matplotlib
 
import csv
import sys
 
import matplotlib.pyplot as plt
 
title = sys.argv[1] if len(sys.argv) > 1 else ""
 
with open("core_latency.csv") as f:
    m = [[float(x) for x in row] for row in csv.reader(f)]
 
n = len(m)
fig, ax = plt.subplots(figsize=(9, 8), dpi=150)
masked = [[None if i == j else m[i][j] for j in range(n)] for i in range(n)]
im = ax.imshow([[v if v is not None else float("nan") for v in r] for r in masked], cmap="viridis")
 
for i in range(n):
    for j in range(n):
        if i != j:
            ax.text(j, i, f"{m[i][j]:.0f}", ha="center", va="center", fontsize=7, color="white")
 
ax.set_xticks(range(n))
ax.set_yticks(range(n))
ax.set_xlabel("CPU")
ax.set_ylabel("CPU")
ax.set_title(f"Core-to-core latency, ns (one way)  {title}".strip())
fig.colorbar(im, ax=ax, shrink=0.8)
fig.tight_layout()
fig.savefig("core_latency.png")
print("saved core_latency.png")
 