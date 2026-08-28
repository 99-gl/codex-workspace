#include "../src/core.hpp"
#include <iostream>
#include <stdexcept>
using namespace pylite;
void ok(bool v,const char*m){if(!v)throw std::runtime_error(m);}
int main(){
 auto c=StaticCompletions("def hello():\n x=1\nclass World:\n pass\nimport json\n"); ok(std::find(c.begin(),c.end(),"print")!=c.end(),"builtin"); ok(std::find(c.begin(),c.end(),"hello")!=c.end(),"function"); ok(std::find(c.begin(),c.end(),"World")!=c.end(),"class"); ok(std::find(c.begin(),c.end(),"json")!=c.end(),"import");
 auto i=ParseImports("import numpy as np\nfrom pathlib import Path as P, PurePath\n"); ok(i.aliases["np"]=="numpy","alias"); ok(i.aliases["P"]=="pathlib.Path","from alias"); ok(i.aliases["PurePath"]=="pathlib.PurePath","from");
 ok(AttributeContext("x = torch.nn.")=="torch.nn","nested attribute"); ok(ResolveModule(ParseImports("import torch\n"),"torch.nn")=="torch.nn","resolve nested");
 ok(DetectPep263("# -*- coding: gbk -*-\nprint('x')") == "gbk","pep263"); ok(ParsePythonVersion("3.13.2\r\n")=="Python 3.13","version");
 ok(QuoteWindowsArg(L"C:\\中文 路径\\hello.py")==L"\"C:\\中文 路径\\hello.py\"","quote");
 LimitedBuffer b(10); b.Append("12345\n67890\nabc"); ok(b.Size()<=10,"buffer limit");
 std::vector<std::string> s={"_x","z","a","__x"};std::sort(s.begin(),s.end(),PublicFirst);ok(s[0]=="a"&&s[1]=="z","sort");
 std::cout<<"All core tests passed\n";
}
