import pandas as pd
from sklearn.metrics import confusion_matrix
import matplotlib.pyplot as plt
import numpy as np


# 1. CSV 로드
df = pd.read_csv("log_test.csv")


# 1-1. Inf / NaN 처리
df["ttc"] = df["ttc"].replace([np.inf, -np.inf], np.nan)
df = df.dropna(subset=["ttc"])      # TTC가 없는 row 제거


# 2. 시나리오별 통계
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

    # 시나리오별 accel_detected 비율
    if "accel_detected" in subset.columns:
        accel_ratio = subset["accel_detected"].mean()
        print("Accel detected ratio:", accel_ratio)


# 전체 accel_detected 비율
if "accel_detected" in df.columns:
    overall_accel_ratio = df["accel_detected"].mean()
    print("\n===== Overall accel_detected statistics =====")
    print(f"Overall accel_detected ratio: {overall_accel_ratio:.4f}")


# 3. Ground Truth 생성
def make_ground_truth(row):
    # 시나리오 2,3 = 오조작 상황 (실험설정)
    if row["scenario"] in [2, 3]:
        return 1
    return 0

df["gt_misop"] = df.apply(make_ground_truth, axis=1)


# 4. 거리 변화량 dist_diff 생성
df["dist_diff"] = df["distance_cm"].diff().abs()


# 4-1. 필터링 조건
filtered = df[(df["dist_diff"] >= 15) & (df["ttc"] <= 1.8)]

print("\n===== Filtered Rows Statistics =====")
print(f"Total filtered samples: {len(filtered)}")


# 4-2. Confusion Matrix (Filtered)
if len(filtered) > 0:
    y_true = filtered["gt_misop"]
    y_pred = filtered["misop_flag"]

    cm = confusion_matrix(y_true, y_pred, labels=[0,1])
    print("\n===== Confusion Matrix (Filtered Rows) =====")
    print(cm)
else:
    print("No rows satisfy the filter conditions (dist_diff>=15 & ttc<=1.8)")


# 5. 전체 misop_flag=1 비율
overall_misop_ratio = df["misop_flag"].mean()
print("\n===== Overall misop_flag ratio =====")
print(f"전체 데이터에서 misop_flag=1 비율: {overall_misop_ratio:.4f}")


# 6. 필터링 조건에서 misop_flag=1 비율
if len(filtered) > 0:
    filtered_misop_ratio = filtered["misop_flag"].mean()
    print("\n===== Filtered misop_flag ratio =====")
    print(f"dist_diff>=15 & ttc<=1.8 조건에서 misop_flag=1 비율: {filtered_misop_ratio:.4f}")
else:
    print("\n===== Filtered misop_flag ratio =====")
    print("필터 조건을 만족하는 row가 없습니다.")



# =========================================
# False Positive Rate (정상 가속 조건 기반)
# TTC >= 3  AND  dist_diff >= 6
# =========================================
# normal_cond = df[(df["ttc"] >= 3.0) & (df["dist_diff"] >= 6)]
normal_cond = df[(df["ttc"] >= 1.8)]

FP = len(normal_cond[normal_cond["misop_flag"] == 1])
TN = len(normal_cond[normal_cond["misop_flag"] == 0])

if (FP + TN) > 0:
    FPR = FP / (FP + TN)
else:
    FPR = 0

print("\n===== False Positive Rate (정상 가속 조건 기준) =====")
print(f"False Positive Rate(FPR): {FPR:.4f}")
print(f"FP: {FP}, TN: {TN}")
print(f"Total normal samples: {len(normal_cond)}")



# =========================================
# Detection Latency 계산
# =========================================
latency_list = []

# accel_detected 이후 misop_flag가 최초 1 되는 구간 탐색
for idx in df.index:
    if df.loc[idx, "accel_detected"] == 1 and df.loc[idx, "misop_flag"] == 1:
        latency = df.loc[idx, "accel_latency"]
        latency_list.append(latency)

# 결과 출력
if len(latency_list) > 0:
    avg_latency = np.mean(latency_list)
    print("\n===== Detection Latency =====")
    print(f"Average latency: {avg_latency:.4f} sec")
    print(f"Samples: {len(latency_list)}")
else:
    print("\n===== Detection Latency =====")
    print("No accel→misop transition detected")


# =========================================
# TTC 단독 기준의 한계 분석
# 3가지 조건 비교
# =========================================
df["dist_raw"] = df["distance_cm"].diff()    # 현재 - 이전

# 급접근을 양수로 만들기 위해 부호 반전
df["dist_diff"] = -df["dist_raw"]

# 뒤로 움직여서 멀어지면(dist_diff < 0) 제거(0 처리)
df.loc[df["dist_diff"] < 0, "dist_diff"] = 0

df = df.drop(columns=["dist_raw"])

cond_true_risk = df[(df["dist_diff"] >= 15) & (df["ttc"] <= 1.8)]
cond_ttc_false_alarm = df[(df["dist_diff"] < 15) & (df["ttc"] <= 1.8)]
cond_ttc_miss = df[(df["dist_diff"] >= 15) & (df["ttc"] > 1.8)]

print("\n===== TTC 한계 분석 =====")

print(f"① True Risk (dist_diff>=15 & ttc<=1.8): {len(cond_true_risk)} rows")
print(f"   - misop_flag=1 비율: {cond_true_risk['misop_flag'].mean():.3f}")

print(f"\n② TTC False Alarm (dist_diff<15 & ttc<=1.8): {len(cond_ttc_false_alarm)} rows")
print(f"   - misop_flag=1 비율: {cond_ttc_false_alarm['misop_flag'].mean():.3f}")

print(f"\n③ TTC Miss Case (dist_diff>=15 & ttc>1.8): {len(cond_ttc_miss)} rows")
print(f"   - misop_flag=1 비율: {cond_ttc_miss['misop_flag'].mean():.3f}")

# 원하면 CSV로 저장도 가능
cond_true_risk.to_csv("true_risk.csv", index=False)
cond_ttc_false_alarm.to_csv("ttc_false_alarm.csv", index=False)
cond_ttc_miss.to_csv("ttc_miss.csv", index=False)

print("\nSaved: true_risk.csv, ttc_false_alarm.csv, ttc_miss.csv")



