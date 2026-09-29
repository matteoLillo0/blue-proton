#!/usr/bin/env bash
# Scarica un modello YOLO pre-addestrato su COCO e lo esporta in ONNX per il detector.
# COCO contiene la classe "cow" (id 19): serve finche' non c'e' il modello nostro.
#
# Uso: tools/export_model.sh [modello] [dimensione]
#   tools/export_model.sh                 # yolo11n, 640x640 -> models/yolo11n_640.onnx
#   tools/export_model.sh yolo11n 320     # piu' veloce sulla UNO Q, meno preciso su animali lontani
#
# Serve solo `uv` (https://docs.astral.sh/uv/): crea un ambiente Python temporaneo,
# non installa nulla nel sistema.
set -euo pipefail

model="${1:-yolo11n}"
size="${2:-640}"
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/models/${model}_${size}.onnx"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

mkdir -p "$root/models"
cd "$work"

# PyTorch solo CPU: la versione di default si porta dietro CUDA (gigabyte inutili qui).
# opset 12 e input statico: compatibili con OpenCV DNN 4.x (quella di Debian sulla UNO Q).
uv run --quiet --python 3.12 --no-project \
    --index https://download.pytorch.org/whl/cpu --index-strategy unsafe-best-match \
    --with "ultralytics==8.3.*" --with onnx --with onnxslim \
    python - "$model" "$size" <<'EOF'
import sys
from ultralytics import YOLO

model, size = sys.argv[1], int(sys.argv[2])
YOLO(f"{model}.pt").export(format="onnx", imgsz=size, opset=12, simplify=True, dynamic=False)
EOF

mv "$work/${model}.onnx" "$out"
echo "Modello esportato: $out"
