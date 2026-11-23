#!/usr/bin/env bash
# 一键刷机 · GrokBot for Waveshare ESP32-S3-Touch-AMOLED-2.16 (v6)
#
# 保留 NVS：只重写 bootloader / 分区表 / app / assets，跳过 0x9000 的 NVS，
# 因此已配网的 Wi-Fi 凭据与设备绑定 token 不会丢失，开机自动重连。
# 若首次使用（板子还是原厂固件），刷完后会自动进入配网热点。
#
# 用法: ./tools/flash.sh [串口]
#   默认串口 /dev/cu.usbmodem2101（macOS）；Linux 用 /dev/ttyUSB0。
# 需要: python3 + esptool + pyserial  (pip3 install esptool pyserial)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PARTS="$ROOT/releases/parts"
PORT="${1:-/dev/cu.usbmodem2101}"

echo "设备   : Waveshare ESP32-S3-Touch-AMOLED-2.16"
echo "固件   : GrokBot v6"
echo "串口   : $PORT"
echo "NVS    : 保留（不清 Wi-Fi 凭据 / 设备绑定）"
echo

for f in bootloader.bin partition-table.bin ota_data_initial.bin app.bin generated_assets.bin; do
    [[ -f "$PARTS/$f" ]] || { echo "✗ 缺少 $PARTS/$f"; exit 1; }
done
[[ -e "$PORT" ]] || { echo "✗ 串口不存在：$PORT（检查 USB 线 / 换端口）"; exit 1; }
python3 -c "import esptool, serial" 2>/dev/null || {
    echo "✗ 缺少 esptool/pyserial：pip3 install esptool pyserial"; exit 1; }

echo "校验芯片型号..."
mkdir -p "$ROOT/logs"
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 115200 --connect-attempts 3 chip-id \
  > "$ROOT/logs/chip_check.log" 2>&1 || true
grep -q "ESP32-S3" "$ROOT/logs/chip_check.log" || {
    echo "✗ 芯片不是 ESP32-S3，已中止"; tail -20 "$ROOT/logs/chip_check.log"; exit 1; }

echo
echo "⚠️  将覆盖 bootloader / 分区表 / app / assets；0x9000 NVS 保留。"
read -r -p "输入 YES 继续: " ans
[[ "$ans" == "YES" ]] || { echo "已取消，未改动硬件"; exit 0; }

ESPTOOL="python3 -m esptool --chip esp32s3 --port $PORT --baud 921600"

echo ">>> 1/4 bootloader (0x0)"
$ESPTOOL write_flash -z 0x0 "$PARTS/bootloader.bin"
echo ">>> 2/4 分区表 (0x8000)"
$ESPTOOL write_flash 0x8000 "$PARTS/partition-table.bin"
echo ">>> 3/4 otdata (0xd000)"
$ESPTOOL write_flash 0xd000 "$PARTS/ota_data_initial.bin"
echo ">>> 4/4 app (0x20000) + assets (0x800000)，保留 NVS"
$ESPTOOL write_flash -z 0x20000 "$PARTS/app.bin" 0x800000 "$PARTS/generated_assets.bin"

echo
echo "✅ 刷入完成。"
echo "   · 已配网板子：直接连原 Wi-Fi，无配网热点。"
echo "   · 全新板子：出现热点 Xiaozhi-XXXX，浏览器打开 192.168.4.1 配网。"
echo "   · 恢复原厂：./tools/restore.sh（注意会清掉 NVS 凭据）。"
