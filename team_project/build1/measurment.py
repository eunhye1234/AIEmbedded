import pandas as pd
import numpy as np

# =============================
# 1. Load CSV
# =============================
df = pd.read_csv("log.csv")

# TTC inf 제거
df["ttc"] = df["ttc"].replace([np.inf, -np.inf], np.nan)
df = df.dropna(subset=["ttc"])

# =============================
# 2. 거리 변화량(dist_diff) 계산
# =============================
df["dist_diff"] = df["distance_cm"].diff()

# 음수 변화(뒤로 움직인 것) 제거
df["dist_diff"] = df["dist_diff"].apply(lambda x: x if x is not None and x > 0 else 0)

# 절댓값으로 변경
df["dist_diff"] = df["dist_diff"].abs()

# =============================
# 3. 4개 버킷 정의
# =============================

# ① High-Risk (진짜 급발진 위험 패턴)
bucket_hr = df[(df["ttc"] <= 1.8) & (df["dist_diff"] >= 15)]

# ② TTC-only 위험 (TTC 낮지만 거리 변화 없음 → 오탐 가능)
bucket_ttc_only = df[(df["ttc"] <= 1.8) & (df["dist_diff"] < 15)]

# ③ TTC Miss 후보 (거리 변화 크지만 TTC는 높음)
bucket_ttc_miss = df[(df["ttc"] > 1.8) & (df["dist_diff"] >= 15)]

# ④ 정상 가속 (정상 조건, 오탐 측정)
bucket_normal = df[(df["ttc"] >= 3.0) & (df["dist_diff"] <= 10)]

# =============================
# 4. 각 버킷 통계 계산 함수
# =============================
def bucket_stats(name, bucket):
    if len(bucket) == 0:
        return {
            "bucket": name,
            "N": 0,
            "misop_ratio": None,
            "mean_ttc": None,
            "mean_dist_diff": None,
            "mean_delta_thr_raw": None,
            "accel_detected_ratio": None
        }
    return {
        "bucket": name,
        "N": len(bucket),
        "misop_ratio": bucket["misop_flag"].mean(),
        "mean_ttc": bucket["ttc"].mean(),
        "mean_dist_diff": bucket["dist_diff"].mean(),
        "mean_delta_thr_raw": bucket["delta_thr_raw"].mean(),
        "accel_detected_ratio": bucket["accel_detected"].mean()
    }

# =============================
# 5. 테이블 생성
# =============================
results = pd.DataFrame([
    bucket_stats("High-Risk (TTC<=1.8 & dist>=15)", bucket_hr),
    bucket_stats("TTC-Only 위험 (TTC<=1.8 & dist<15)", bucket_ttc_only),
    bucket_stats("TTC Miss 후보 (TTC>1.8 & dist>=15)", bucket_ttc_miss),
    bucket_stats("정상 가속 (TTC>=3 & dist<=10)", bucket_normal),
])

print("\n===== 성능평가 4개 버킷 결과 =====")
print(results.to_string(index=False))

# =============================
# 6. False Positive Rate (정상 구간 기준)
# =============================
FP = len(bucket_normal[bucket_normal["misop_flag"] == 1])
TN = len(bucket_normal[bucket_normal["misop_flag"] == 0])
FPR = FP / (FP + TN) if (FP + TN) > 0 else 0

print("\n===== False Positive Rate (정상 가속 조건) =====")
print(f"False Positive Rate(FPR): {FPR:.4f}")
print(f"FP: {FP}, TN: {TN}, Total: {FP+TN}")




#====================================================

df = pd.read_csv("log.csv")

# TTC inf 제거
df["ttc"] = df["ttc"].replace([np.inf, -np.inf], np.nan)
df = df.dropna(subset=["ttc"])

# dist_diff 계산 (음수 제거 → 후진 제거)
df["dist_diff_raw"] = df["distance_cm"].diff()
df["dist_diff"] = df["dist_diff_raw"].apply(lambda x: x if x is not None and x > 0 else 0)

# -------------------------------
# 버킷 정의
# -------------------------------
bucket_high_risk = df[(df["ttc"] <= 1.8) & (df["dist_diff"] >= 15)]
bucket_ttc_only = df[(df["ttc"] <= 1.8) & (df["dist_diff"] < 15)]
bucket_ttc_miss = df[(df["ttc"] > 1.8) & (df["dist_diff"] >= 15)]

# 전체 위험 영역 = TTC<=1.8 또는 dist>=15
bucket_total_risk = df[(df["ttc"] <= 1.8) | (df["dist_diff"] >= 15)]

# =====================================================
# ① High-Risk인데 탐지되지 않은 수 (FN)
# =====================================================

HR_total = len(bucket_high_risk)
HR_FN = len(bucket_high_risk[bucket_high_risk["misop_flag"] == 0])
HR_TP = len(bucket_high_risk[bucket_high_risk["misop_flag"] == 1])

HR_detection_rate = HR_TP / HR_total if HR_total > 0 else None

print("\n===== 1) High-Risk 구간 탐지 성능 =====")
print(f"High-Risk 샘플 수: {HR_total}")
print(f"탐지됨 (TP): {HR_TP}")
print(f"탐지 안됨 (FN): {HR_FN}")
print(f"탐지 성공률: {HR_detection_rate:.4f}")


# =====================================================
# ② TTC-only 위험인데 dist 기준으로는 위험 아님 → 재분류 비율
# =====================================================

TTC_only_count = len(bucket_ttc_only)
TTC_only_detected = bucket_ttc_only["misop_flag"].sum()

TTC_only_false_alarm_rate = TTC_only_detected / TTC_only_count if TTC_only_count > 0 else None

print("\n===== 2) TTC-only 위험 판단의 한계 =====")
print(f"TTC-only 위험 샘플 수: {TTC_only_count}")
print(f"여기서 misop_flag=1 (오탐): {TTC_only_detected}")
print(f"TTC-only 오탐 비율: {TTC_only_false_alarm_rate:.4f}")


# =====================================================
# ③ 전체 위험 중 TTC-miss 후보 비율
# =====================================================

total_risk_count = len(bucket_total_risk)
ttc_miss_count = len(bucket_ttc_miss)

ttc_miss_ratio = ttc_miss_count / total_risk_count if total_risk_count > 0 else None

print("\n===== 3) 전체 위험 중 TTC-miss 후보 비율 =====")
print(f"전체 위험 샘플 수: {total_risk_count}")
print(f"TTC-miss 후보(dist>=15, ttc>1.8): {ttc_miss_count}")
print(f"TTC-miss 비율: {ttc_miss_ratio:.4f}")

