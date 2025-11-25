#!/usr/bin/env python3
import json, argparse
from urllib.parse import urlparse
from paho.mqtt.publish import single as mqtt_single

def parse_broker(broker: str):
    if broker.startswith(("tcp://", "mqtt://")):
        u = urlparse(broker)
        return u.hostname, (u.port or 1883)
    if ":" in broker:
        host, port = broker.split(":", 1)
        return host, int(port)
    return broker, 1883

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--broker", default="wyjae.sytes.net:4341")
    ap.add_argument("--topic",  default="roadcast/control/speed/A")
    ap.add_argument("--speed",  type=float, default=0.0)
    args = ap.parse_args()

    host, port = parse_broker(args.broker)
    payload = json.dumps({"speed": args.speed}, ensure_ascii=False)

    mqtt_single(
        topic=args.topic,
        payload=payload,
        hostname=host,
        port=port,
        qos=1,
    )
    print(f"[OK] published to {host}:{port} topic='{args.topic}' payload={payload}")

if __name__ == "__main__":
    main()
