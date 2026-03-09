#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage: bash scripts/run_overnight_soak.sh [options]

Options:
  --duration-sec N      Run duration in seconds (default: 28800)
  --rounds N            Override rounds. If omitted, duration mode controls stop.
  --interval-ms N       Delay after each send in milliseconds (default: 0)
  --startup-wait-ms N   Wait before clients start in milliseconds (default: 50)
  --node1-port N        Port for node1 (default: 50051)
  --node2-port N        Port for node2 (default: 50052)
  --build-dir PATH      Build directory (default: /workspace/build-soak)
  --build-type TYPE     CMake build type (default: RelWithDebInfo)
  --sample-sec N        Metrics sampling interval in seconds (default: 5)
  --skip-build          Reuse existing binary
  --help                Show this message
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

DURATION_SEC=28800
ROUNDS=""
INTERVAL_MS=0
STARTUP_WAIT_MS=50
NODE1_PORT=50051
NODE2_PORT=50052
BUILD_DIR="${ROOT_DIR}/build-soak"
BUILD_TYPE="RelWithDebInfo"
SAMPLE_SEC=5
SKIP_BUILD=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --duration-sec)
      DURATION_SEC="$2"
      shift 2
      ;;
    --rounds)
      ROUNDS="$2"
      shift 2
      ;;
    --interval-ms)
      INTERVAL_MS="$2"
      shift 2
      ;;
    --startup-wait-ms)
      STARTUP_WAIT_MS="$2"
      shift 2
      ;;
    --node1-port)
      NODE1_PORT="$2"
      shift 2
      ;;
    --node2-port)
      NODE2_PORT="$2"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --build-type)
      BUILD_TYPE="$2"
      shift 2
      ;;
    --sample-sec)
      SAMPLE_SEC="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
RUN_DIR="${ROOT_DIR}/soak/${TIMESTAMP}"
LOG_DIR="${RUN_DIR}/logs"
METRICS_DIR="${RUN_DIR}/metrics"
BINARY_PATH="${BUILD_DIR}/pingpong_demo"

mkdir -p "${LOG_DIR}" "${METRICS_DIR}"

if [[ "${SKIP_BUILD}" -ne 1 ]]; then
  CMAKE_CONFIGURE_ARGS=(
    -S "${ROOT_DIR}"
    -B "${BUILD_DIR}"
    -DBUILD_TESTING=ON
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
  )
  if command -v ninja >/dev/null 2>&1; then
    CMAKE_CONFIGURE_ARGS+=(-G Ninja)
  fi

  cmake "${CMAKE_CONFIGURE_ARGS[@]}"
  cmake --build "${BUILD_DIR}" -j"$(nproc)"
fi

if [[ ! -x "${BINARY_PATH}" ]]; then
  echo "binary not found: ${BINARY_PATH}" >&2
  exit 1
fi

RUN_ARGS=(
  --duration-sec "${DURATION_SEC}"
  --interval-ms "${INTERVAL_MS}"
  --startup-wait-ms "${STARTUP_WAIT_MS}"
  --node1-port "${NODE1_PORT}"
  --node2-port "${NODE2_PORT}"
)

if [[ -n "${ROUNDS}" ]]; then
  RUN_ARGS+=(--rounds "${ROUNDS}")
fi

cat > "${RUN_DIR}/run_config.txt" <<EOF
timestamp=${TIMESTAMP}
build_dir=${BUILD_DIR}
build_type=${BUILD_TYPE}
binary=${BINARY_PATH}
duration_sec=${DURATION_SEC}
rounds=${ROUNDS:-auto}
interval_ms=${INTERVAL_MS}
startup_wait_ms=${STARTUP_WAIT_MS}
node1_port=${NODE1_PORT}
node2_port=${NODE2_PORT}
sample_sec=${SAMPLE_SEC}
EOF

"${BINARY_PATH}" "${RUN_ARGS[@]}" > "${LOG_DIR}/app.log" 2>&1 &
APP_PID=$!
echo "${APP_PID}" > "${LOG_DIR}/app.pid"

PIDSTAT_PID=""
MONITOR_PID=""

cleanup() {
  set +e
  if kill -0 "${APP_PID}" 2>/dev/null; then
    kill "${APP_PID}" 2>/dev/null || true
  fi
  if [[ -n "${PIDSTAT_PID}" ]]; then
    kill "${PIDSTAT_PID}" 2>/dev/null || true
  fi
  if [[ -n "${MONITOR_PID}" ]]; then
    kill "${MONITOR_PID}" 2>/dev/null || true
  fi
}

trap cleanup EXIT

if command -v pidstat >/dev/null 2>&1; then
  pidstat -r -u -d -h -p "${APP_PID}" "${SAMPLE_SEC}" > "${METRICS_DIR}/pidstat.log" 2>&1 &
  PIDSTAT_PID=$!
fi

(
  while kill -0 "${APP_PID}" 2>/dev/null; do
    TS="$(date '+%F %T')"

    echo "[${TS}]" >> "${METRICS_DIR}/ps.log"
    ps -o pid,ppid,%cpu,%mem,rss,vsz,nlwp,etime,stat,cmd -p "${APP_PID}" >> "${METRICS_DIR}/ps.log"

    if [[ -r "/proc/${APP_PID}/status" ]]; then
      echo "[${TS}]" >> "${METRICS_DIR}/status.log"
      awk '/VmRSS|VmSize|Threads/ {print}' "/proc/${APP_PID}/status" >> "${METRICS_DIR}/status.log"
    fi

    echo "[${TS}]" >> "${METRICS_DIR}/ports.log"
    ss -lntp | grep -E ":(${NODE1_PORT}|${NODE2_PORT})\\b" >> "${METRICS_DIR}/ports.log" || true

    if [[ -f /sys/fs/cgroup/memory.current ]]; then
      echo "${TS} $(cat /sys/fs/cgroup/memory.current)" >> "${METRICS_DIR}/cgroup-memory.log"
    fi

    if [[ -f /sys/fs/cgroup/cpu.stat ]]; then
      echo "[${TS}]" >> "${METRICS_DIR}/cgroup-cpu.log"
      cat /sys/fs/cgroup/cpu.stat >> "${METRICS_DIR}/cgroup-cpu.log"
    fi

    sleep "${SAMPLE_SEC}"
  done
)&
MONITOR_PID=$!

set +e
wait "${APP_PID}"
RC=$?
set -e

cleanup
trap - EXIT

sleep 1
echo "rc=${RC}" > "${LOG_DIR}/exit.code"
ss -lntp | grep -E ":(${NODE1_PORT}|${NODE2_PORT})\\b" > "${METRICS_DIR}/ports-after-exit.log" || true

echo "soak run completed"
echo "run_dir=${RUN_DIR}"
echo "exit_code=${RC}"
