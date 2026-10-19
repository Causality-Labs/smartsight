#!/usr/bin/env bash
# Build the image (if needed) and open a shell with the firmware dir mounted at /workspace
set -euo pipefail

IMAGE=imx8mp-m7-dev
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FW_DIR="$(dirname "$SCRIPT_DIR")"

if [[ "${1:-}" == "--rebuild" ]] || ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    docker build \
        --build-arg USER_UID="$(id -u)" \
        --build-arg USER_GID="$(id -g)" \
        -t "$IMAGE" "$SCRIPT_DIR"
    [[ "${1:-}" == "--rebuild" ]] && shift
fi

# Pass through USB serial adapters / debug probes if present
DEVICES=()
for d in /dev/ttyUSB* /dev/ttyACM*; do
    [[ -e "$d" ]] && DEVICES+=(--device "$d")
done

exec docker run --rm -it \
    -v "$FW_DIR":/workspace \
    "${DEVICES[@]}" \
    "$IMAGE" "$@"
