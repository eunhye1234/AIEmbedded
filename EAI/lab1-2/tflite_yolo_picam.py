import cv2
import numpy as np
import tflite_runtime.interpreter as tflite

import RPi.GPIO as GPIO
import time


# ===== Constants =====
IMG_SIZE = 416
SCORE_THRESH = 0.2
IOU_THRESH = 0.3


ACCEL_REGION = {
    "xmin": 60,
    "ymin": 200,
    "xmax": 312,
    "ymax": 400
}

BRAKE_REGION = {
    "xmin": 63,
    "ymin": 41,
    "xmax": 278,
    "ymax": 211
}

def is_in_region(box, region):
    ymin, xmin, ymax, xmax = box
    return (
        xmin >= region["xmin"] and
        xmax <= region["xmax"] and
        ymin >= region["ymin"] and
        ymax <= region["ymax"]
    )

# ===== IoU & NMS =====
def iou(b1, b2):
    y1, x1 = max(b1[0], b2[0]), max(b1[1], b2[1])
    y2, x2 = min(b1[2], b2[2]), min(b1[3], b2[3])
    inter = max(0, y2 - y1) * max(0, x2 - x1)
    union = (b1[2]-b1[0])*(b1[3]-b1[1]) + (b2[2]-b2[0])*(b2[3]-b2[1]) - inter
    return inter / union if union > 0 else 0

def nms(boxes, scores, classes, iou_t=IOU_THRESH):
    idxs = np.argsort(scores)[::-1]
    keep = []
    while idxs.size > 0:
        i = idxs[0]
        keep.append(i)
        ious = np.array([iou(boxes[i], boxes[j]) for j in idxs[1:]])
        idxs = idxs[1:][~((classes[idxs[1:]] == classes[i]) & (ious > iou_t))]
    return boxes[keep], scores[keep], classes[keep]

# ===== Box Filtering + NMS =====
def filter_boxes(box_xywh, scores):
    boxes, confs, clses = [], [], []
    for i in range(box_xywh.shape[0]):
        cls_scores = scores[i]
        cls_id, score = np.argmax(cls_scores), np.max(cls_scores)
        if score < SCORE_THRESH: continue
        cx, cy, w, h = box_xywh[i]
        xmin, ymin = cx - w/2, cy - h/2
        xmax, ymax = cx + w/2, cy + h/2
        boxes.append([ymin, xmin, ymax, xmax])
        confs.append(score)
        clses.append(cls_id)
    if boxes:
        return nms(np.array(boxes), np.array(confs), np.array(clses))
    return np.array([]), np.array([]), np.array([])

# ===== Visualization =====
def visualize(frame, boxes, scores, classes, labels):
    h, w = frame.shape[:2]
    np.random.seed(42)
    colors = {i: tuple(np.random.randint(0,255,3).tolist()) for i in labels}
    for box, score, cls in zip(boxes, scores, classes):
        ymin, xmin, ymax, xmax = box
        x1, y1 = int(xmin / IMG_SIZE * w), int(ymin / IMG_SIZE * h)
        x2, y2 = int(xmax / IMG_SIZE * w), int(ymax / IMG_SIZE * h)
        color = colors[cls]
        cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
        label = f"{labels.get(cls, str(cls))}:{score:.2f}"
        (tw, th), _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.7, 2)
        cv2.rectangle(frame, (x1, y1 - th - 5), (x1 + tw, y1), color, -1)
        cv2.putText(frame, label, (x1, y1 - 3), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255,255,255), 2)

# ===== Main =====
interpreter = tflite.Interpreter(model_path="yolov4-tiny.tflite")
interpreter.allocate_tensors()
inp, out = interpreter.get_input_details(), interpreter.get_output_details()

labels = {i: n for i, n in enumerate([
    'person','bicycle','car','motorbike','aeroplane','bus','train','truck','boat','traffic light',
    'fire hydrant','stop sign','parking meter','bench','bird','cat','dog','horse','sheep','cow',
    'elephant','bear','zebra','giraffe','backpack','umbrella','handbag','tie','suitcase','frisbee',
    'skis','snowboard','sports ball','kite','baseball bat','baseball glove','skateboard','surfboard','tennis racket',
    'bottle','wine glass','cup','fork','knife','spoon','bowl','banana','apple','sandwich','orange',
    'broccoli','carrot','hot dog','pizza','donut','cake','chair','sofa','pottedplant','bed',
    'diningtable','toilet','tvmonitor','laptop','mouse','remote','keyboard','cell phone','microwave','oven',
    'toaster','sink','refrigerator','book','clock','vase','scissors','teddy bear','hair drier','toothbrush'
])}

cap = cv2.VideoCapture(0)

if not cap.isOpened():
    raise SystemExit("Camera not opened")

prev_time = time.time()

while True:
    ret, frame = cap.read()
    if not ret: break

    img = cv2.resize(frame, (IMG_SIZE, IMG_SIZE))
    input_data = np.expand_dims(img.astype(np.float32) / 255.0, axis=0)
    interpreter.set_tensor(inp[0]['index'], input_data)
    interpreter.invoke()

    loc = interpreter.get_tensor(out[0]['index'])
    cls = interpreter.get_tensor(out[1]['index'])
    boxes, scores, clses = filter_boxes(loc.squeeze(), cls.squeeze())

    visualize(frame, boxes, scores, clses, labels)

    # ===== CAR DETECTION 조건 처리 =====
    accel_detected = False
    brake_detected = False

    print("Detected bounding boxes:")

    for box, cls in zip(boxes, clses):

        label_name = labels[int(cls)]

        ymin, xmin, ymax, xmax = box
        print(f"  {label_name}: xmin={xmin:.1f}, ymin={ymin:.1f}, xmax={xmax:.1f}, ymax={ymax:.1f}")

        # === ACCEL 감지 ===
        if is_in_region(box, ACCEL_REGION):
            accel_detected = True

        # === BRAKE 감지 ===
        if is_in_region(box, BRAKE_REGION):
            brake_detected = True

    # ======= 화면 표시 & FLAG 생성 =======

        # ACCEL
        if accel_detected:
            text = "ACCEL DETECTED"
            (tw, th), _ = cv2.getTextSize(text, cv2.FONT_HERSHEY_SIMPLEX, 1.0, 3)
            cv2.rectangle(frame, (10, 10), (10 + tw + 10, 10 + th + 20), (0, 255, 0), -1)
            cv2.putText(frame, text, (20, 10 + th + 5),
                cv2.FONT_HERSHEY_SIMPLEX, 1.0, (255, 255, 255), 3)

            # main.cpp로 트리거 전송
            with open("/tmp/accel_detected.flag", "w") as f:
                f.write(str(time.time()))
                print("ACCEL FLAG SENT")

        # BRAKE
        if brake_detected:
            text = "BRAKE DETECTED"
            (tw, th), _ = cv2.getTextSize(text, cv2.FONT_HERSHEY_SIMPLEX, 1.0, 3)
            cv2.rectangle(frame, (10, 10), (10 + tw + 10, 10 + th + 20), (0, 0, 255), -1)
            cv2.putText(frame, text, (20, 10 + th + 5),
                cv2.FONT_HERSHEY_SIMPLEX, 1.0, (255, 255, 255), 3)

            with open("/tmp/brake_detected.flag", "w") as f:
                f.write(str(time.time()))
                print("BRAKE FLAG SENT")



    cv2.imshow("YOLOv4-Tiny Detection", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'): break

    # === FPS 측정 ===
    curr_time = time.time()
    fps = 1 / (curr_time - prev_time)
    prev_time = curr_time

    print(f"FPS: {fps:.2f}")


cap.release()
cv2.destroyAllWindows()
