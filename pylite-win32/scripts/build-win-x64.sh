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
mkdir -p "$build" "$output"
rm -f "$build/core_tests" "$build/app-res.o" "$output/PyLite.exe"

g++ -std=c++17 -O2 -Wall -Wextra "$root/tests/core_tests.cpp" -o "$build/core_tests"
"$build/core_tests"
bash "$root/tests/structure_tests.sh"
python3 "$root/tests/validate_manifest.py" "$root/resources/app.manifest"

(cd "$root/resources" && "$windres" -I . -I "$toolroot/usr/x86_64-w64-mingw32/include" app.rc "$build/app-res.o")
export PATH="$toolroot/usr/bin:$PATH"
"$compiler" \
  -std=c++17 -O2 -DNDEBUG -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE \
  -B"$toolroot/usr/lib/gcc/x86_64-w64-mingw32/13-posix/" \
  -B"$toolroot/usr/x86_64-w64-mingw32/bin/" \
  --sysroot="$toolroot" \
  "$root/src/main.cpp" "$build/app-res.o" \
  -municode -mwindows -static -static-libgcc -static-libstdc++ \
  -lcomctl32 -lcomdlg32 -lshell32 -lshlwapi -lole32 -luuid -ladvapi32 -lgdi32 -luser32 -luxtheme \
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
