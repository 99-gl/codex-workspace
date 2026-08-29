#include "ILexer.h"
#include "LexerModule.h"

#include "python_lexer.hpp"

extern const Lexilla::LexerModule lmPython;

Scintilla::ILexer5 *CreatePythonLexer() {
  return lmPython.Create();
}
