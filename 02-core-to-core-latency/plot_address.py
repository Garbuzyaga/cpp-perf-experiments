# Bar chart from address_latency.csv
# Usage: python plot_address.py "i7-11700F, CPU 0 <-> CPU 2"
 
import csv
import sys
 
import matplotlib.pyplot as plt
 
title = sys.argv[1] if len(sys.argv) > 1 else ""
 
with open("address_latency.csv") as f:
    rows = [(int(i), float(v)) for i, v in csv.reader(f)]
 
idx = [r[0] for r in rows]
ns = [r[1] for r in rows]
 
fig, ax = plt.subplots(figsize=(10, 5), dpi=150)
ax.bar(idx, ns, color="#3b6ea5")
ax.set_xlabel("cache line in buffer (64 B apart)")
ax.set_ylabel("one-way latency, ns")
ax.set_ylim(0, max(ns) * 1.15)
ax.set_title(f"Same two cores, different addresses  {title}".strip())
fig.tight_layout()
fig.savefig("address_latency.png")
print("saved address_latency.png")
 