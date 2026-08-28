#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_file="$root/src/main.cpp"
for token in 'WC_TREEVIEWW' 'MSFTEDIT_CLASS' 'CMD_TOGGLE' 'RegisterOpen' 'DynamicComplete' 'CreateJobObjectW' 'JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE' 'WM_GETMINMAXINFO' '1200,800' 'BS_OWNERDRAW' 'PyLiteGutter' 'DeferWindowPos' 'WM_DPICHANGED' 'SetWindowTheme' 'TrackPopupMenuEx'; do
  rg -q "$token" "$source_file"
done
! rg -q '内容待定' "$source_file"
! rg -q 'WS_EX_COMPOSITED' "$source_file"
! rg -q '<dpiAware xmlns=.*>PerMonitorV2' "$root/resources/app.manifest"
test -f "$root/UI_REDESIGN_SPEC_FOR_CODEX.txt"
rg -q 'PYTHONIOENCODING' "$source_file"
rg -q -- '--register-open-with' "$source_file"
echo "All structure tests passed"
