import pandas as pd
from sklearn.metrics import confusion_matrix
import matplotlib.pyplot as plt
import numpy as np

# -------------------------------
# 1. CSV 로드
# -------------------------------
df = pd.read_csv("log_test.csv")

# -------------------------------
# 1-1. Inf / NaN 처리
# -------------------------------
df["ttc"] = df["ttc"].replace([np.inf, -np.inf], np.nan)
df = df.dropna(subset=["ttc"])      # TTC가 없는 row는 제거

# 또는 아래처럼 처리 가능 (원하면 조정 가능)
# df["ttc"] = df["ttc"].clip(upper=5.0)

# -------------------------------
# 2. 시나리오별 성능 분석
# -------------------------------
scenarios = df["scenario"].unique()

print("===== Scenario-based Statistics =====")
for sc in scenarios:
    subset = df[df["scenario"] == sc]

    print(f"\n--- Scenario {sc} ---")
    print("Total samples:", len(subset))
    print("Mean TTC:", subset["ttc"].mean())
    print("Mean v_rel:", subset["v_rel"].mean())
    print("Mean delta_thr_raw:", subset["delta_thr_raw"].mean())
    print("Misop flag ratio:", subset["misop_flag"].mean())

        # 🔥 시나리오별 accel_detected 비율
    if "accel_detected" in subset.columns:
        accel_ratio = subset["accel_detected"].mean()
        print("Accel detected ratio:", accel_ratio)

# -------------------------------
# 🔥 2-1. 전체 accel_detected 비율 출력
# -------------------------------
if "accel_detected" in df.columns:
    overall_accel_ratio = df["accel_detected"].mean()
    print("\n===== Overall accel_detected statistics =====")
    print(f"Overall accel_detected ratio: {overall_accel_ratio:.4f}")

# -------------------------------
# 3. Ground Truth 생성
# -------------------------------
def make_ground_truth(row):
    if row["scenario"] in [2, 3]:
        return 1  # true misoperation
    return 0

df["gt_misop"] = df.apply(make_ground_truth, axis=1)

# -------------------------------
# 4. Confusion Matrix 출력
# -------------------------------
y_true = df["gt_misop"]
y_pred = df["misop_flag"]

cm = confusion_matrix(y_true, y_pred, labels=[0,1])
print("\n===== Confusion Matrix (misop_flag vs ground truth) =====")
print(cm)

# -------------------------------
# 5. TTC 히스토그램
# -------------------------------
plt.hist(df["ttc"], bins=40, color='blue', alpha=0.7)
plt.title("TTC Distribution")
plt.xlabel("TTC (sec)")
plt.ylabel("Count")
plt.savefig("ttc_hist.png")
plt.clf()

# -------------------------------
# 6. v_rel 히스토그램
# -------------------------------
plt.hist(df["v_rel"], bins=40, color='green', alpha=0.7)
plt.title("Relative Speed (v_rel) Distribution")
plt.xlabel("v_rel (m/s)")
plt.ylabel("Count")
plt.savefig("vrel_hist.png")
plt.clf()

print("\nSaved plots: ttc_hist.png, vrel_hist.png")
