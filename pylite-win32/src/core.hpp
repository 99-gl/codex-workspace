#pragma once
#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace pylite {

inline const std::vector<std::string>& Keywords() {
    static const std::vector<std::string> v={"False","None","True","and","as","assert","async","await","break","case","class","continue","def","del","elif","else","except","finally","for","from","global","if","import","in","is","lambda","match","nonlocal","not","or","pass","raise","return","try","while","with","yield"}; return v;
}
inline const std::vector<std::string>& Builtins() {
    static const std::vector<std::string> v={"abs","all","any","ascii","bin","bool","breakpoint","bytearray","bytes","callable","chr","classmethod","compile","complex","delattr","dict","dir","divmod","enumerate","eval","exec","filter","float","format","frozenset","getattr","globals","hasattr","hash","help","hex","id","input","int","isinstance","issubclass","iter","len","list","locals","map","max","memoryview","min","next","object","oct","open","ord","pow","print","property","range","repr","reversed","round","set","setattr","slice","sorted","staticmethod","str","sum","super","tuple","type","vars","zip","__import__"}; return v;
}
struct Imports { std::map<std::string,std::string> aliases; std::set<std::string> names; };
inline Imports ParseImports(const std::string& text) {
    Imports out; std::istringstream in(text); std::string line;
    std::regex imp(R"(^\s*import\s+([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*)(?:\s+as\s+([A-Za-z_]\w*))?)");
    std::regex from(R"(^\s*from\s+([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*)\s+import\s+(.+)$)");
    while(std::getline(in,line)) { std::smatch m;
        if(std::regex_search(line,m,imp)){ auto mod=m[1].str(); auto alias=m[2].matched?m[2].str():mod.substr(0,mod.find('.')); out.aliases[alias]=mod; out.names.insert(alias); continue; }
        if(std::regex_search(line,m,from)){ auto base=m[1].str(); std::istringstream parts(m[2].str()); std::string p;
            while(std::getline(parts,p,',')){ p=std::regex_replace(p,std::regex(R"(^\s+|\s+$)"),""); std::smatch a; if(std::regex_match(p,a,std::regex(R"(([A-Za-z_]\w*)(?:\s+as\s+([A-Za-z_]\w*))?)"))){auto orig=a[1].str(), name=a[2].matched?a[2].str():orig; out.names.insert(name); out.aliases[name]=base+"."+orig;}}
        }
    } return out;
}
inline std::vector<std::string> StaticCompletions(const std::string& text,const std::string& prefix="") {
    std::set<std::string> names(Keywords().begin(),Keywords().end()); names.insert(Builtins().begin(),Builtins().end()); auto im=ParseImports(text); names.insert(im.names.begin(),im.names.end());
    std::regex decl(R"(^\s*(?:async\s+)?(?:def|class)\s+([A-Za-z_]\w*)|^\s*([A-Za-z_]\w*)\s*=)",std::regex::multiline); for(auto i=std::sregex_iterator(text.begin(),text.end(),decl);i!=std::sregex_iterator();++i) names.insert((*i)[1].matched?(*i)[1].str():(*i)[2].str());
    std::vector<std::string> out; for(auto& n:names) if(prefix.empty()||n.rfind(prefix,0)==0) out.push_back(n); if(out.size()>100) out.resize(100); return out;
}
inline std::string AttributeContext(const std::string& beforeCaret) {
    std::smatch m; if(std::regex_search(beforeCaret,m,std::regex(R"(([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*)\.$)"))) return m[1].str(); return {};
}
inline std::string ResolveModule(const Imports& imports,const std::string& expression) {
    auto dot=expression.find('.'); auto root=expression.substr(0,dot); auto it=imports.aliases.find(root); if(it==imports.aliases.end()) return {}; return it->second+(dot==std::string::npos?"":expression.substr(dot));
}
inline std::string DetectPep263(const std::string& bytes) {
    auto end1=bytes.find('\n'), end2=end1==std::string::npos?bytes.size():bytes.find('\n',end1+1); auto head=bytes.substr(0,end2==std::string::npos?bytes.size():end2);
    std::smatch m; if(std::regex_search(head,m,std::regex(R"(coding\s*[:=]\s*([-\w.]+))",std::regex::icase))){auto e=m[1].str(); std::transform(e.begin(),e.end(),e.begin(),[](unsigned char c){return std::tolower(c);}); return e;} return "utf-8";
}
inline std::string ParsePythonVersion(const std::string& s) { std::smatch m; return std::regex_search(s,m,std::regex(R"((\d+)\.(\d+)(?:\.\d+)?)"))?"Python "+m[1].str()+"."+m[2].str():""; }
inline std::wstring QuoteWindowsArg(const std::wstring& arg){ if(arg.empty()) return L"\"\""; if(arg.find_first_of(L" \t\"")==std::wstring::npos) return arg; std::wstring o=L"\""; unsigned sl=0; for(wchar_t c:arg){if(c==L'\\'){sl++;continue;} if(c==L'\"'){o.append(sl*2+1,L'\\');o+=c;sl=0;}else{o.append(sl,L'\\');sl=0;o+=c;}} o.append(sl*2,L'\\');o+=L'\"';return o;}
class LimitedBuffer { size_t limit_; std::string data_; public: explicit LimitedBuffer(size_t n):limit_(n){} void Append(const std::string&s){data_+=s;if(data_.size()>limit_){auto cut=data_.size()-limit_;auto nl=data_.find('\n',cut);data_.erase(0,nl==std::string::npos?cut:nl+1);}} const std::string& Str()const{return data_;} size_t Size()const{return data_.size();} };
inline bool PublicFirst(const std::string&a,const std::string&b){bool au=!a.empty()&&a[0]=='_',bu=!b.empty()&&b[0]=='_';return au!=bu?!au:a<b;}
}
