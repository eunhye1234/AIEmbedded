import pandas as pd
import matplotlib.pyplot as plt

# CSV 불러오기
df = pd.read_csv("log.csv")

print(df.head())

# ----------- Graph 1: raw vs cmd  ----------------
plt.figure(figsize=(10, 5))
plt.plot(df['raw_percent'], label='Throttle RAW (%)')
plt.plot(df['cmd_percent'], label='Throttle CMD (%)')
plt.xlabel("Sample index")
plt.ylabel("Percent (%)")
plt.title("Throttle RAW vs CMD")
plt.legend()
plt.grid(True)
plt.savefig("raw_vs_cmd.png", dpi=200)
plt.close()

# ----------- Graph 2: TTC vs CMD -----------------
plt.figure(figsize=(10, 5))
plt.plot(df['ttc'], label='TTC (s)')
plt.plot(df['cmd_percent'], label='Throttle CMD (%)')
plt.xlabel("Sample index")
plt.ylabel("Value")
plt.title("TTC vs Throttle CMD")
plt.legend()
plt.grid(True)
plt.savefig("ttc_vs_cmd.png", dpi=200)
plt.close()

print("PNG files saved:")
print(" - raw_vs_cmd.png")
print(" - ttc_vs_cmd.png")
