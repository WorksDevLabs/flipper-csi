#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
cc=${CC:-cc}
flags="-std=c11 -O1 -g -Wall -Wextra -Werror"
if [ "${SANITIZE:-0}" = 1 ]; then flags="$flags -fsanitize=address,undefined -fno-omit-frame-pointer"; fi
$cc $flags tools/test_motion.c esp32/main/csi_motion.c -Iesp32/main -lm -o "$build_dir/motion"
"$build_dir/motion"
$cc $flags tools/test_regression.c esp32/main/csi_motion.c flipper_app/csi_protocol.c -Iesp32/main -Iflipper_app -lm -o "$build_dir/regression"
"$build_dir/regression"

$cc $flags tools/test_pro.c esp32/main/csi_motion.c flipper_app/csi_protocol.c flipper_app/csi_pro.c -Iesp32/main -Iflipper_app -lm -o "$build_dir/pro"
"$build_dir/pro"
python3 -m unittest discover -s tools -p 'test_replay.py'
