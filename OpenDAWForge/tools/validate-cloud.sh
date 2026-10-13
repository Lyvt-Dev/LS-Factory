#!/usr/bin/env bash
set -euo pipefail
source /workspace/.tools/activate.sh
cd /workspace/LS-Factory/OpenDAWForge
ctest --preset debug
xvfb-run --auto-servernum --server-args='-screen 0 1600x1000x24 -nolisten tcp' \
    'build/debug/LSFactory_artefacts/Debug/LS Factory' --smoke-test "$PWD/build/smoke"
xvfb-run --auto-servernum --server-args='-screen 0 1600x1000x24 -nolisten tcp' \
    'build/debug/LSFactory_artefacts/Debug/LS Factory' --plugin-smoke-test \
    "$PWD/build/debug/ForgeGainFixture_artefacts/Debug/VST3/Forge Gain Fixture.vst3" "$PWD/build/plugin-smoke"
'build/debug/LSFactory_artefacts/Debug/LS Factory' --factory-smoke-test "$PWD/build/factory-smoke"
