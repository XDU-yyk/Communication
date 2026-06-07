#!/bin/bash
# ============================================================
# OLSR 仿真 — Ubuntu VM 编译运行脚本
# 使用方法：
#   chmod +x run.sh
#   ./run.sh [scenario] [nUavs] [simTime] [seed]
#
# 示例：
#   ./run.sh static 30 60 1       # G0 静态基线测试
#   ./run.sh mobility 30 60 1     # G1 低速移动测试
# ============================================================

SCENARIO=${1:-static}
N_UAVS=${2:-30}
SIM_TIME=${3:-60}
SEED=${4:-1}
RESULTS_DIR="results"
NS3_DIR="${NS3_DIR:-$HOME/ns-allinone-3.43/ns-3.43}"

echo "=========================================="
echo " OLSR Simulation - Ubuntu VM"
echo "=========================================="
echo " Scenario:  ${SCENARIO}"
echo " UAVs:      ${N_UAVS}"
echo " Duration:  ${SIM_TIME}s"
echo " Seed:      ${SEED}"
echo " NS-3:      ${NS3_DIR}"
echo " Output:    ${RESULTS_DIR}/"
echo "=========================================="

# 检查 ns-3 路径
if [ ! -d "$NS3_DIR" ]; then
    echo "[ERROR] NS-3 directory not found: ${NS3_DIR}"
    echo "  Set NS3_DIR env var or edit the script."
    exit 1
fi

# 创建结果目录
mkdir -p ${RESULTS_DIR}

# 复制脚本到 ns-3 scratch 目录
echo "[Step 1] Copying olsr-sim.cc to ns-3 scratch..."
cp olsr-sim.cc "${NS3_DIR}/scratch/"

# 编译
echo "[Step 2] Building..."
cd "${NS3_DIR}"
./ns3 build 2>&1 | tail -20

if [ $? -ne 0 ]; then
    echo "[ERROR] Build failed."
    exit 1
fi

# 运行
echo "[Step 3] Running simulation..."
./ns3 run "scratch/olsr-sim --scenario=${SCENARIO} --nUavs=${N_UAVS} --simTime=${SIM_TIME} --seed=${SEED} --resultsDir=${RESULTS_DIR}" 2>&1

if [ $? -ne 0 ]; then
    echo "[ERROR] Simulation failed."
    exit 1
fi

# 复制结果回 week1.0
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
echo "[Step 4] Copying results back to ${SCRIPT_DIR}..."
cp -r "${RESULTS_DIR}"/* "${SCRIPT_DIR}/${RESULTS_DIR}/"

# 画图（如需）
echo "[Step 5] Generating plots..."
cd "${SCRIPT_DIR}"
python3 plot_results.py --results-dir ${RESULTS_DIR} --output-dir figures --scenario ${SCENARIO}

echo "=========================================="
echo " Done! Results in:"
echo "   ${SCRIPT_DIR}/${RESULTS_DIR}/"
echo "   ${SCRIPT_DIR}/figures/"
echo "=========================================="
