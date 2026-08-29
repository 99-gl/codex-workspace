#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_file="$root/src/main.cpp"
for token in 'WC_TREEVIEWW' 'MSFTEDIT_CLASS' 'CMD_TOGGLE' 'CMD_TEST' 'ID_STATEMENT' 'ID_TAB_INPUT' 'RunWorker' 'WriteFile\(inWrite' 'test_solution.py' 'RegisterOpen' 'DynamicComplete' 'CreateJobObjectW' 'JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE' 'WM_GETMINMAXINFO' '1200,800' 'BS_OWNERDRAW' 'DeferWindowPos' 'WM_DPICHANGED' 'SetWindowTheme' 'TrackPopupMenuEx' 'Scintilla_RegisterClasses' 'SCI_SETILEXER' 'SC_MARGIN_NUMBER' 'SC_TECHNOLOGY_DIRECTWRITE' 'SCN_UPDATEUI'; do
  rg -q "$token" "$source_file"
done
! rg -q '内容待定' "$source_file"
! rg -q 'WS_EX_COMPOSITED' "$source_file"
! rg -q '<dpiAware xmlns=.*>PerMonitorV2' "$root/resources/app.manifest"
test -f "$root/UI_REDESIGN_SPEC_FOR_CODEX.txt"
rg -q 'PYTHONIOENCODING' "$source_file"
rg -q -- '--register-open-with' "$source_file"
rg -q 'dyLineSpacing=28' "$source_file"
! rg -q 'dyLineSpacing=330' "$source_file"
for removed in 'PyLiteGutter' 'gGutter' 'gPopup' 'ColorRange' 'void Highlight' 'EM_GETFIRSTVISIBLELINE' 'gLastEditTick'; do
  ! rg -q "$removed" "$source_file"
done
test -f "$root/vendor/scintilla/License.txt"
test -f "$root/vendor/lexilla/License.txt"
problem="$root/problems/0300-longest-increasing-subsequence"
test -f "$problem/description.md"
test -f "$problem/solution.py"
test -f "$problem/test_solution.py"
rg -q '最长递增子序列' "$problem/description.md"
rg -q 'subprocess.run' "$problem/test_solution.py"
rg -q '566' "$root/vendor/scintilla/version.txt"
rg -q '553' "$root/vendor/lexilla/version.txt"
echo "All structure tests passed"
