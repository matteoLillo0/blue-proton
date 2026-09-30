#!/usr/bin/env bash
# Esporta in ONNX per il detector un modello YOLOv8/YOLO11 di ultralytics:
# - uno pubblico pre-addestrato su COCO (classe "cow" = id 19), scaricato per nome;
# - oppure il NOSTRO, passando il file dei pesi .pt (es. best.pt dall'addestramento).
#
# Uso: tools/export_model.sh [modello | file.pt] [dimensione]
#   tools/export_model.sh                 # yolo11n, 640x640 -> models/yolo11n_640.onnx
#   tools/export_model.sh yolo11n 320     # piu' veloce sulla UNO Q, meno preciso su animali lontani
#   tools/export_model.sh ~/runs/detect/train/weights/best.pt 320   # -> models/best_320.onnx
#
# Serve solo `uv` (https://docs.astral.sh/uv/): crea un ambiente Python temporaneo,
# non installa nulla nel sistema.
set -euo pipefail

model="${1:-yolo11n}"
size="${2:-640}"
root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

if [[ "$model" == *.pt ]]; then
    if [[ ! -f "$model" ]]; then
        echo "File dei pesi non trovato: $model" >&2
        exit 1
    fi
    # Copia nella cartella di lavoro: ultralytics scrive l'.onnx accanto al .pt.
    name="$(basename "$model" .pt)"
    cp "$model" "$work/$name.pt"
    model="$name"
fi
out="$root/models/${model}_${size}.onnx"

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
echo "Uso: --model $out --model-size $size  (con un modello nostro a classe unica anche --class-id 0)"
