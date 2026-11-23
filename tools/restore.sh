#!/usr/bin/env bash
# 回滚辅助脚本：用「用户自己提前备份的原厂镜像」整片写回 16MB Flash。
#
# 注意：本仓库不包含任何原厂固件备份（那属于你的设备私有数据）。
# 使用前请把你当初备份的 factory 镜像放到 releases/ 下并命名为 factory_full_16mb.bin。
# 整片写回会清掉 NVS（Wi-Fi 凭据 / 设备绑定），需重新配网。
#
# 用法: ./tools/restore.sh [串口]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${1:-/dev/cu.usbmodem2101}"
BACKUP="$ROOT/releases/factory_full_16mb.bin"

[[ -f "$BACKUP" ]] || {
    echo "✗ 找不到 $BACKUP"
    echo "  请把你自己的原厂 16MB 备份放到该路径后再运行。"
    echo "  若还没备份：esptool --chip esp32s3 --port $PORT read_flash 0x0 0x1000000 factory_full_16mb.bin"
    exit 1; }
[[ -e "$PORT" ]] || { echo "✗ 串口不存在"; exit 1; }

echo "⚠️  将把 $BACKUP 整片写回 16MB Flash。"
echo "    当前 GrokBot 固件会被覆盖，NVS 凭据也会清掉。"
read -r -p "输入 YES 继续: " ans
[[ "$ans" == "YES" ]] || { echo "已取消"; exit 0; }

python3 -m esptool --chip esp32s3 --port "$PORT" --baud 921600 \
    write_flash 0x0 "$BACKUP"
echo "✅ 已写回。复位后应为原厂固件。"
