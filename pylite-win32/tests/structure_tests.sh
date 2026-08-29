#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_file="$root/src/main.cpp"
for token in 'WC_TREEVIEWW' 'MSFTEDIT_CLASS' 'CMD_TOGGLE' 'CMD_TEST' 'ID_STATEMENT' 'ID_TAB_INPUT' 'RunWorker' 'WriteFile\(inWrite' 'test_solution.py' 'RegisterOpen' 'DynamicComplete' 'CreateJobObjectW' 'JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE' 'WM_GETMINMAXINFO' '1200,800' 'BS_OWNERDRAW' 'DeferWindowPos' 'WM_DPICHANGED' 'SetWindowTheme' 'TrackPopupMenuEx' 'Scintilla_RegisterClasses' 'SCI_SETILEXER' 'SC_MARGIN_NUMBER' 'SC_TECHNOLOGY_DIRECTWRITE' 'SCN_UPDATEUI'; do
  rg -q "$token" "$source_file"
done
! rg -q '内容待定' "$source_file"
! rg -q 'WS_EX_COMPOSITED' "$source_file"
! rg -q 'SetWindowSubclass\(gEdit|gSuppressTabCharacter|AcceptCompletion' "$source_file"
rg -q 'HandleAutoPair' "$source_file"
rg -q 'SCI_AUTOCSETIGNORECASE,TRUE' "$source_file"
rg -q 'CompleteImport' "$source_file"
! rg -q '<dpiAware xmlns=.*>PerMonitorV2' "$root/resources/app.manifest"
test -f "$root/UI_REDESIGN_SPEC_FOR_CODEX.txt"
rg -q 'PYTHONIOENCODING' "$source_file"
rg -q -- '--register-open-with' "$source_file"
rg -q 'dyLineSpacing=28' "$source_file"
! rg -q 'dyLineSpacing=330' "$source_file"
rg -Fq 'gRight=480' "$source_file"
rg -Fq 'JsonInt(s,"right",480),160,1600' "$source_file"
! rg -Fq 'JsonInt(s,"right",360),160,420' "$source_file"
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
[[ "$(find "$root/problems" -mindepth 1 -maxdepth 1 -type d | wc -l)" -eq 41 ]]
[[ "$(find "$root/problems" -mindepth 2 -maxdepth 2 -type f | wc -l)" -eq 123 ]]
[[ "$(find "$root/problems" -mindepth 2 -maxdepth 2 -name description.md | wc -l)" -eq 41 ]]
[[ "$(find "$root/problems" -mindepth 2 -maxdepth 2 -name solution.py | wc -l)" -eq 41 ]]
[[ "$(find "$root/problems" -mindepth 2 -maxdepth 2 -name test_solution.py | wc -l)" -eq 41 ]]
[[ "$(rg -l '## 题目描述' "$root/problems" -g 'description.md' | wc -l)" -eq 41 ]]
[[ "$(rg -l '## 输入格式' "$root/problems" -g 'description.md' | wc -l)" -eq 41 ]]
[[ "$(rg -l '## 输出格式' "$root/problems" -g 'description.md' | wc -l)" -eq 41 ]]
[[ "$(rg -l 'subprocess.run' "$root/problems" -g 'test_solution.py' | wc -l)" -eq 41 ]]
! rg -q '来源|进阶|https?://' "$root/problems" -g 'description.md'
[[ "$(find "$root/problems" -mindepth 2 -maxdepth 2 -name solution.py ! -path '*/0300-longest-increasing-subsequence/*' -size 0c | wc -l)" -eq 40 ]]
rg -q '566' "$root/vendor/scintilla/version.txt"
rg -q '553' "$root/vendor/lexilla/version.txt"
echo "All structure tests passed"
