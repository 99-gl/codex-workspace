#!/usr/bin/env bash
# Usage:
#   ./scripts/build-win-x64.sh
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
workspace_root="$(cd "$root/.." && pwd)"
toolroot="${PYLITE_MINGW_ROOT:-$workspace_root/.bootstrap/mingw}"
compiler="$toolroot/usr/bin/x86_64-w64-mingw32-g++-posix"
windres="$toolroot/usr/bin/x86_64-w64-mingw32-windres"
if [[ ! -x "$compiler" || ! -x "$windres" ]]; then
  echo "MinGW-w64 toolchain not found. Set PYLITE_MINGW_ROOT to its extracted root." >&2
  exit 2
fi
command -v g++ >/dev/null || { echo "Host g++ is required for core tests." >&2; exit 2; }
command -v python3 >/dev/null || { echo "Python 3 is required for manifest validation." >&2; exit 2; }

build="$root/build"
output="$root/artifacts/win-x64"
vendor_objects="$build/vendor-objects"
mkdir -p "$build" "$output" "$vendor_objects"
rm -f "$build/core_tests" "$build/app-res.o" "$output/PyLite.exe" "$vendor_objects"/*.o

[[ "$(tr -d '\r\n' < "$root/vendor/scintilla/version.txt")" == "566" ]] || { echo "Expected Scintilla 5.6.6 sources" >&2; exit 2; }
[[ "$(tr -d '\r\n' < "$root/vendor/lexilla/version.txt")" == "553" ]] || { echo "Expected Lexilla 5.5.3 sources" >&2; exit 2; }

g++ -std=c++17 -O2 -Wall -Wextra "$root/tests/core_tests.cpp" -o "$build/core_tests"
"$build/core_tests"
bash "$root/tests/structure_tests.sh"
python3 "$root/tests/validate_manifest.py" "$root/resources/app.manifest"

(cd "$root/resources" && "$windres" -I . -I "$toolroot/usr/x86_64-w64-mingw32/include" app.rc "$build/app-res.o")
export PATH="$toolroot/usr/bin:$PATH"
common_flags=(
  -std=c++17 -O2 -DNDEBUG -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE
  -B"$toolroot/usr/lib/gcc/x86_64-w64-mingw32/13-posix/"
  -B"$toolroot/usr/x86_64-w64-mingw32/bin/"
  --sysroot="$toolroot"
)
scintilla_sources=(
  src/AutoComplete.cxx src/CallTip.cxx src/CaseConvert.cxx src/CaseFolder.cxx
  src/CellBuffer.cxx src/ChangeHistory.cxx src/CharacterCategoryMap.cxx src/CharacterType.cxx
  src/CharClassify.cxx src/ContractionState.cxx src/DBCS.cxx src/Decoration.cxx
  src/Document.cxx src/EditModel.cxx src/Editor.cxx src/EditView.cxx src/Geometry.cxx
  src/Indicator.cxx src/KeyMap.cxx src/LineMarker.cxx src/MarginView.cxx src/PerLine.cxx
  src/PositionCache.cxx src/RESearch.cxx src/RunStyles.cxx src/ScintillaBase.cxx src/Selection.cxx src/Style.cxx
  src/UndoHistory.cxx src/UniConversion.cxx src/UniqueString.cxx src/ViewStyle.cxx src/XPM.cxx
  win32/HanjaDic.cxx win32/PlatWin.cxx win32/ListBox.cxx win32/SurfaceGDI.cxx
  win32/SurfaceD2D.cxx win32/ScintillaWin.cxx
)
lexilla_sources=(
  lexlib/Accessor.cxx lexlib/CharacterCategory.cxx lexlib/CharacterSet.cxx
  lexlib/DefaultLexer.cxx lexlib/InList.cxx lexlib/LexAccessor.cxx lexlib/LexerBase.cxx
  lexlib/LexerModule.cxx lexlib/LexerSimple.cxx lexlib/PropSetSimple.cxx
  lexlib/StyleContext.cxx lexlib/WordList.cxx lexers/LexPython.cxx
)
vendor_link_objects=()
for source in "${scintilla_sources[@]}"; do
  object="$vendor_objects/scintilla_${source//\//_}.o"
  "$compiler" "${common_flags[@]}" \
    -I"$root/vendor/scintilla/include" -I"$root/vendor/scintilla/src" -I"$root/vendor/scintilla/win32" \
    -c "$root/vendor/scintilla/$source" -o "$object"
  vendor_link_objects+=("$object")
done
for source in "${lexilla_sources[@]}"; do
  object="$vendor_objects/lexilla_${source//\//_}.o"
  "$compiler" "${common_flags[@]}" \
    -I"$root/vendor/scintilla/include" -I"$root/vendor/lexilla/include" -I"$root/vendor/lexilla/lexlib" \
    -c "$root/vendor/lexilla/$source" -o "$object"
  vendor_link_objects+=("$object")
done
"$compiler" \
  "${common_flags[@]}" \
  -I"$root/vendor/scintilla/include" -I"$root/vendor/lexilla/include" -I"$root/vendor/lexilla/lexlib" \
  "$root/src/main.cpp" "$root/src/python_lexer.cpp" "$build/app-res.o" "${vendor_link_objects[@]}" \
  -municode -mwindows -static -static-libgcc -static-libstdc++ \
  -lcomctl32 -lcomdlg32 -lshell32 -lshlwapi -lole32 -luuid -ladvapi32 -lgdi32 -luser32 -luxtheme -limm32 -loleaut32 \
  -o "$output/PyLite.exe"

file_count="$(find "$output" -maxdepth 1 -type f | wc -l)"
only_name="$(find "$output" -maxdepth 1 -type f -printf '%f')"
if [[ "$file_count" -ne 1 || "$only_name" != "PyLite.exe" ]]; then
  echo "Publish directory must contain only PyLite.exe" >&2
  exit 3
fi
file "$output/PyLite.exe" | rg -q 'PE32\+ executable \(GUI\) x86-64'
cp "$output/PyLite.exe" "$root/PyLite.exe"
echo "EXE: $output/PyLite.exe"
du -h "$output/PyLite.exe"
sha256sum "$output/PyLite.exe"
