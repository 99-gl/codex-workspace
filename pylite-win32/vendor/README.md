# Vendored editor sources

PyLite vendors only the source files needed to statically link its editor control and Python lexer into the single Windows executable.

- Scintilla 5.6.6: `https://www.scintilla.org/scite566.tgz` (the official SciTE source bundle contains Scintilla)
- Lexilla 5.5.3: `https://www.scintilla.org/lexilla553.tgz`
- SciTE 5.6.6 source archive SHA-256: `eaf566bfb489328760e2f0d49addbed4a59eeaf5ffea1645710a0f51770a9ac5`
- Lexilla 5.5.3 source archive SHA-256: `4d9e64263c337034a06f9c67f330c605764cac02aee83c06f6c21f9527a71628`

The upstream licenses are preserved in `scintilla/License.txt` and `lexilla/License.txt`. No Scintilla or Lexilla DLL is shipped.
