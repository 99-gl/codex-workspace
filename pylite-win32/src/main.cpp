#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <thread>
#include <atomic>
#include "Scintilla.h"
#include "SciLexer.h"
#include "core.hpp"
#include "python_lexer.hpp"
#include "ui_theme.hpp"

using namespace pylite; namespace fs=std::filesystem;
constexpr int ID_TREE=100,ID_EDIT=101,ID_OUTPUT=102,ID_SAVE=103,ID_PY=104,ID_RUN=105,ID_CLEAR=106,ID_TOGGLE=107,ID_STATUS=108,ID_TEST=109,ID_INPUT=110,ID_STATEMENT=111,ID_TAB_INPUT=112,ID_TAB_OUTPUT=113;
enum {CMD_NEW=200,CMD_OPEN,CMD_FOLDER,CMD_SAVE,CMD_SAVEAS,CMD_EXIT,CMD_UNDO,CMD_REDO,CMD_CUT,CMD_COPY,CMD_PASTE,CMD_ALL,CMD_RUN,CMD_TEST,CMD_STOP,CMD_TOGGLE,CMD_SELECTPY,CMD_REGISTER,CMD_UNREGISTER,CMD_ABOUT};
constexpr UINT WM_APPEND=WM_APP+1,WM_RUNEND=WM_APP+2,WM_INTERPRETER=WM_APP+3,WM_COMPLETIONS=WM_APP+4;
HINSTANCE gInst{}; HWND gMain{},gTree{},gEdit{},gOutput{},gInput{},gStatement{},gPy{},gRun{},gTest{},gSave{},gFolder{},gClear{},gToggle{},gInputTab{},gOutputTab{},gTooltip{}; HFONT gUiFont{},gUiMediumFont{},gCodeFont{},gIconFont{}; HBRUSH gAppBrush{},gPanelBrush{},gOutputBrush{},gHoverBrush{},gSelectedBrush{},gPrimaryBrush{},gPrimaryHoverBrush{},gSuccessBrush{},gErrorBrush{},gFocusBrush{},gMutedBrush{}; HPEN gBorderPen{},gDividerPen{}; HMENU gMenuBar{}; HIMAGELIST gSystemImageList{};
std::wstring gFile,gRoot,gProblemDir,gInterpreter,gEncoding=L"UTF-8",gStatusText=L"就绪",gPositionText=L"Ln 1, Col 1    UTF-8    Python",gCompletionKey; bool gDirty=false,gOutputCollapsed=false,gRunning=false,gRunningTests=false,gShowInput=false,gFormatting=false; int gLeft=230,gRight=480,gSavedOutput=180; int gDrag=0,gHotMenu=-1,gLastExit=-1; POINT gDragStart{}; int gDragValue{}; UINT gDpi=96;
HANDLE gProcess=nullptr,gJob=nullptr; std::atomic<unsigned> gCompletionRequest{0}; std::map<std::wstring,std::vector<std::wstring>> gModuleCache; std::mutex gCacheMutex;
int S(int value);void ApplyParagraphSpacing(HWND edit);void ConfigureEditor();void UpdateLineNumberMargin();void Layout();void LoadProblemForFile(const std::wstring&path);

std::wstring W(const std::string&s){if(s.empty())return{};int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);std::wstring o(n,0);MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),o.data(),n);return o;}
std::string U8(const std::wstring&s){if(s.empty())return{};int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);std::string o(n,0);WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),o.data(),n,nullptr,nullptr);return o;}
std::wstring BaseName(const std::wstring&p){auto n=fs::path(p).filename().wstring();return n.empty()?L"未命名.py":n;}
std::wstring SettingsPath(){wchar_t p[MAX_PATH]{};SHGetFolderPathW(nullptr,CSIDL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,p);fs::path d=fs::path(p)/L"PyLite";std::error_code ec;fs::create_directories(d,ec);return(d/L"settings.json").wstring();}
void SetStatus(const std::wstring&s){gStatusText=s;if(gMain){RECT r;GetClientRect(gMain,&r);RECT status{0,r.bottom-pylite_ui::Scale(24,gDpi),r.right,r.bottom};InvalidateRect(gMain,&status,FALSE);}}
void UpdateTitle(){std::wstring t=BaseName(gFile)+(gDirty?L" * — PyLite":L" — PyLite");SetWindowTextW(gMain,t.c_str());}
sptr_t Sci(unsigned message,uptr_t wParam=0,sptr_t lParam=0){return gEdit?SendMessage(gEdit,message,(WPARAM)wParam,(LPARAM)lParam):0;}
std::string EditorBytes(){const auto n=(size_t)Sci(SCI_GETTEXTLENGTH);std::string s(n+1,'\0');Sci(SCI_GETTEXT,n+1,(sptr_t)s.data());s.resize(n);return s;}
std::wstring EditorText(){return W(EditorBytes());}
void SetEditorText(const std::wstring&s){gFormatting=true;auto bytes=U8(s);Sci(SCI_SETTEXT,0,(sptr_t)bytes.c_str());Sci(SCI_EMPTYUNDOBUFFER);Sci(SCI_SETSAVEPOINT);Sci(SCI_COLOURISE,0,-1);gDirty=false;gFormatting=false;UpdateLineNumberMargin();UpdateTitle();}
void SaveSettings(){auto settings=SettingsPath();std::ofstream f(settings.c_str(),std::ios::binary);f<<"{\n  \"python\": \"";for(char c:U8(gInterpreter)){if(c=='\\'||c=='\"')f<<'\\';f<<c;}f<<"\",\n  \"left\": "<<gLeft<<",\n  \"right\": "<<gRight<<",\n  \"output\": "<<gSavedOutput<<",\n  \"collapsed\": "<<(gOutputCollapsed?"true":"false")<<",\n  \"folder\": \"";for(char c:U8(gRoot)){if(c=='\\'||c=='\"')f<<'\\';f<<c;}f<<"\"\n}\n";}
std::string JsonString(const std::string&s,const char*key){std::smatch m;std::regex r(std::string("\\\"")+key+"\\\"\\s*:\\s*\\\"((?:\\\\.|[^\\\"])*)\\\"");if(!std::regex_search(s,m,r))return{};std::string o;bool esc=false;for(char c:m[1].str()){if(esc){o+=c;esc=false;}else if(c=='\\')esc=true;else o+=c;}return o;}
int JsonInt(const std::string&s,const char*k,int d){std::smatch m;return std::regex_search(s,m,std::regex(std::string("\\\"")+k+"\\\"\\s*:\\s*(\\d+)"))?std::stoi(m[1].str()):d;}
void LoadSettings(){auto settings=SettingsPath();std::ifstream f(settings.c_str(),std::ios::binary);if(!f)return;std::string s((std::istreambuf_iterator<char>(f)),{});try{gInterpreter=W(JsonString(s,"python"));gRoot=W(JsonString(s,"folder"));gLeft=std::clamp(JsonInt(s,"left",230),160,420);gRight=std::clamp(JsonInt(s,"right",480),160,1600);gSavedOutput=std::clamp(JsonInt(s,"output",180),90,440);gOutputCollapsed=s.find("\"collapsed\": true")!=std::string::npos;}catch(...){gLeft=230;gRight=480;gSavedOutput=180;gOutputCollapsed=false;}}

bool DecodeFile(const std::wstring&path,std::wstring&out){std::ifstream f(fs::path(path),std::ios::binary);if(!f){MessageBoxW(gMain,L"无法读取文件。",L"PyLite",MB_ICONERROR);return false;}std::string b((std::istreambuf_iterator<char>(f)),{});UINT cp=CP_UTF8;int off=0;if(b.size()>=3&&(unsigned char)b[0]==0xEF&&(unsigned char)b[1]==0xBB&&(unsigned char)b[2]==0xBF){off=3;gEncoding=L"UTF-8 BOM";}else{auto e=DetectPep263(b);if(e=="gbk"||e=="gb2312"||e=="cp936"){cp=936;gEncoding=L"GBK";}else if(e=="latin-1"||e=="iso-8859-1"){cp=28591;gEncoding=L"Latin-1";}else gEncoding=L"UTF-8";}int flags=cp==CP_UTF8?MB_ERR_INVALID_CHARS:0;int n=MultiByteToWideChar(cp,flags,b.data()+off,(int)b.size()-off,nullptr,0);if(n<=0&&b.size()>off){MessageBoxW(gMain,L"文件编码无法识别，未打开以避免损坏。",L"编码错误",MB_ICONERROR);return false;}out.resize(n);MultiByteToWideChar(cp,flags,b.data()+off,(int)b.size()-off,out.data(),n);return true;}
bool SaveTo(const std::wstring&path){auto s=EditorText();UINT cp=gEncoding==L"GBK"?936:gEncoding==L"Latin-1"?28591:CP_UTF8;int n=WideCharToMultiByte(cp,WC_NO_BEST_FIT_CHARS,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);std::string b(n,0);BOOL used=FALSE;WideCharToMultiByte(cp,WC_NO_BEST_FIT_CHARS,s.data(),(int)s.size(),b.data(),n,nullptr,&used);if(used&&cp!=CP_UTF8){MessageBoxW(gMain,L"当前编码无法表示部分字符，请另存为 UTF-8。",L"编码错误",MB_ICONERROR);return false;}std::ofstream f(fs::path(path),std::ios::binary|std::ios::trunc);if(!f){MessageBoxW(gMain,L"无法保存文件。",L"PyLite",MB_ICONERROR);return false;}if(gEncoding==L"UTF-8 BOM")f.write("\xEF\xBB\xBF",3);f.write(b.data(),b.size());gFile=path;Sci(SCI_SETSAVEPOINT);gDirty=false;UpdateTitle();SetStatus(L"已保存");return true;}
std::wstring FileDialog(bool save){wchar_t p[32768]{};if(!gFile.empty())wcscpy_s(p,gFile.c_str());OPENFILENAMEW o{sizeof(o)};o.hwndOwner=gMain;o.lpstrFile=p;o.nMaxFile=32768;o.lpstrFilter=L"Python 文件 (*.py;*.pyw)\0*.py;*.pyw\0所有文件\0*.*\0";o.lpstrDefExt=L"py";o.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);return(save?GetSaveFileNameW(&o):GetOpenFileNameW(&o))?p:L"";}
std::wstring FolderDialog(){IFileDialog*d=nullptr;std::wstring r;if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&d)))){DWORD o;d->GetOptions(&o);d->SetOptions(o|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM);if(SUCCEEDED(d->Show(gMain))){IShellItem*i=nullptr;if(SUCCEEDED(d->GetResult(&i))){PWSTR p=nullptr;if(SUCCEEDED(i->GetDisplayName(SIGDN_FILESYSPATH,&p))){r=p;CoTaskMemFree(p);}i->Release();}}d->Release();}return r;}
bool ConfirmDiscard(){if(!gDirty)return true;int r=MessageBoxW(gMain,L"当前文件有未保存修改，是否保存？",L"PyLite",MB_YESNOCANCEL|MB_ICONWARNING);if(r==IDCANCEL)return false;if(r==IDNO)return true;SendMessage(gMain,WM_COMMAND,CMD_SAVE,0);return !gDirty;}

void AddDummy(HTREEITEM h){TVINSERTSTRUCTW t{};t.hParent=h;t.hInsertAfter=TVI_LAST;t.item.mask=TVIF_TEXT|TVIF_PARAM;t.item.pszText=(LPWSTR)L"";t.item.lParam=0;TreeView_InsertItem(gTree,&t);}
int SystemIcon(const fs::path&p,bool dir){SHFILEINFOW info{};DWORD attr=dir?FILE_ATTRIBUTE_DIRECTORY:FILE_ATTRIBUTE_NORMAL;auto list=(HIMAGELIST)SHGetFileInfoW(p.c_str(),attr,&info,sizeof(info),SHGFI_SYSICONINDEX|SHGFI_SMALLICON|SHGFI_USEFILEATTRIBUTES);if(list&&!gSystemImageList){gSystemImageList=list;TreeView_SetImageList(gTree,gSystemImageList,TVSIL_NORMAL);}return info.iIcon;}
HTREEITEM AddTree(HTREEITEM parent,const fs::path&p,bool dir){auto*stored=new std::wstring(p.wstring());TVINSERTSTRUCTW t{};t.hParent=parent;t.hInsertAfter=TVI_LAST;t.item.mask=TVIF_TEXT|TVIF_PARAM|TVIF_CHILDREN|TVIF_IMAGE|TVIF_SELECTEDIMAGE;auto label=p.filename().wstring();if(label.empty())label=p.wstring();t.item.pszText=(LPWSTR)label.c_str();t.item.lParam=(LPARAM)stored;t.item.cChildren=dir?1:0;t.item.iImage=t.item.iSelectedImage=SystemIcon(p,dir);auto h=TreeView_InsertItem(gTree,&t);if(dir)AddDummy(h);return h;}
void LoadChildren(HTREEITEM h,const std::wstring&path){TreeView_DeleteItem(gTree,TreeView_GetChild(gTree,h));std::vector<fs::directory_entry> v;std::error_code ec;for(fs::directory_iterator it(path,fs::directory_options::skip_permission_denied,ec),end;it!=end&&!ec;it.increment(ec)){auto n=it->path().filename().wstring();if(n==L".git"||n==L"__pycache__")continue;v.push_back(*it);}std::sort(v.begin(),v.end(),[](auto&a,auto&b){if(a.is_directory()!=b.is_directory())return a.is_directory();auto x=a.path().filename().wstring(),y=b.path().filename().wstring();std::transform(x.begin(),x.end(),x.begin(),towlower);std::transform(y.begin(),y.end(),y.begin(),towlower);return x<y;});for(auto&e:v)AddTree(h,e.path(),e.is_directory(ec));}
void SetRoot(const std::wstring&root){gRoot=root;TreeView_DeleteAllItems(gTree);if(root.empty())return;auto h=AddTree(TVI_ROOT,fs::path(root),true);LoadChildren(h,root);TreeView_Expand(gTree,h,TVE_EXPAND);SaveSettings();}

std::wstring WindowText(HWND h){int n=GetWindowTextLengthW(h);std::vector<wchar_t> buffer((size_t)n+1);GetWindowTextW(h,buffer.data(),n+1);return std::wstring(buffer.data(),(size_t)n);}
std::wstring ReadUtf8(const fs::path&path){std::ifstream f(path,std::ios::binary);if(!f)return{};std::string bytes((std::istreambuf_iterator<char>(f)),{});if(bytes.size()>=3&&(unsigned char)bytes[0]==0xEF&&(unsigned char)bytes[1]==0xBB&&(unsigned char)bytes[2]==0xBF)bytes.erase(0,3);return W(bytes);}
std::wstring ExecutableDirectory(){wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,ARRAYSIZE(path));return fs::path(path).parent_path().wstring();}
std::wstring BundledProblems(){auto p=fs::path(ExecutableDirectory())/L"problems";return fs::exists(p)?p.wstring():L"";}

struct StatementSpan{LONG start{},length{};int points{};bool bold{},code{};};
void CleanInlineMarkdown(std::wstring&line){
  line=std::regex_replace(line,std::wregex(L"\\[([^\\]]+)\\]\\(([^\\)]+)\\)"),L"$1 ($2)");
  line.erase(std::remove(line.begin(),line.end(),L'`'),line.end());
  for(const auto*marker:{L"**",L"__"}){size_t pos=0;while((pos=line.find(marker,pos))!=std::wstring::npos)line.erase(pos,2);}
  if(line.rfind(L"> ",0)==0)line.erase(0,2);
}
std::wstring HtmlToText(std::wstring html){
  html=std::regex_replace(html,std::wregex(L"<(br|BR)\\s*/?>"),L"\r\n");
  html=std::regex_replace(html,std::wregex(L"</(p|P|div|DIV|h[1-6]|H[1-6]|li|LI)>"),L"\r\n");
  html=std::regex_replace(html,std::wregex(L"<(li|LI)[^>]*>"),L"• ");
  html=std::regex_replace(html,std::wregex(L"<[^>]+>"),L"");
  const std::pair<const wchar_t*,const wchar_t*> entities[]={{L"&lt;",L"<"},{L"&gt;",L">"},{L"&amp;",L"&"},{L"&quot;",L"\""},{L"&nbsp;",L" "}};
  for(auto&e:entities){size_t pos=0;while((pos=html.find(e.first,pos))!=std::wstring::npos){html.replace(pos,wcslen(e.first),e.second);pos+=wcslen(e.second);}}
  return html;
}
void RenderStatement(const fs::path&path){
  if(!gStatement)return;auto source=ReadUtf8(path);if(source.empty()){SetWindowTextW(gStatement,L"题面为空或无法读取。");return;}
  auto ext=path.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),towlower);
  if(ext==L".html"||ext==L".htm")source=HtmlToText(source);
  std::wistringstream stream(source);std::wstring line,text;std::vector<StatementSpan> spans;bool code=false;
  while(std::getline(stream,line)){
    if(!line.empty()&&line.back()==L'\r')line.pop_back();
    if(ext==L".md"&&line.rfind(L"```",0)==0){code=!code;continue;}
    if(line.empty()&&!code)continue;
    int heading=0;if(ext==L".md"){while(heading<(int)line.size()&&line[(size_t)heading]==L'#')heading++;if(heading>0&&heading<=6&&(int)line.size()>heading&&line[(size_t)heading]==L' ')line.erase(0,(size_t)heading+1);else heading=0;if(line.rfind(L"- ",0)==0)line.replace(0,2,L"• ");if(!code)CleanInlineMarkdown(line);}
    LONG start=(LONG)text.size();text+=line;text+=L"\r";
    if(heading)spans.push_back({start,(LONG)line.size(),heading==1?18:(heading==2?14:12),true,false});
    else if(code)spans.push_back({start,(LONG)line.size()+1,10,false,true});
  }
  SetWindowTextW(gStatement,text.c_str());SendMessage(gStatement,EM_SETSEL,0,-1);
  CHARFORMAT2W base{sizeof(base)};base.dwMask=CFM_FACE|CFM_SIZE|CFM_COLOR|CFM_BOLD|CFM_BACKCOLOR;base.yHeight=200;base.crTextColor=pylite_ui::Theme::TextPrimary;base.crBackColor=pylite_ui::Theme::PanelBackground;base.dwEffects=0;wcscpy_s(base.szFaceName,L"Segoe UI");SendMessage(gStatement,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&base);
  PARAFORMAT2 paragraph{sizeof(paragraph)};paragraph.dwMask=PFM_LINESPACING|PFM_SPACEAFTER;paragraph.bLineSpacingRule=5;paragraph.dyLineSpacing=22;paragraph.dySpaceAfter=70;SendMessage(gStatement,EM_SETPARAFORMAT,0,(LPARAM)&paragraph);
  for(const auto&span:spans){SendMessage(gStatement,EM_SETSEL,span.start,span.start+span.length);CHARFORMAT2W format{sizeof(format)};format.dwMask=CFM_FACE|CFM_SIZE|CFM_BOLD|CFM_COLOR|CFM_BACKCOLOR;format.yHeight=span.points*20;format.dwEffects=span.bold?CFE_BOLD:0;format.crTextColor=span.code?RGB(5,80,174):pylite_ui::Theme::TextPrimary;format.crBackColor=span.code?pylite_ui::Theme::OutputBackground:pylite_ui::Theme::PanelBackground;wcscpy_s(format.szFaceName,span.code?L"Cascadia Mono":L"Segoe UI");SendMessage(gStatement,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&format);}
  SendMessage(gStatement,EM_SETSEL,0,0);SendMessage(gStatement,EM_SETMODIFY,FALSE,0);
}
fs::path DescriptionFor(const fs::path&dir){for(auto name:{L"description.md",L"description.html",L"description.htm"}){auto p=dir/name;if(fs::is_regular_file(p))return p;}return{};}
void LoadProblemForFile(const std::wstring&path){
  fs::path p(path),dir=fs::is_directory(p)?p:p.parent_path();auto description=DescriptionFor(dir);
  if(description.empty()){gProblemDir.clear();SetWindowTextW(gStatement,L"从左侧打开题目目录中的 solution.py，即可在这里阅读题面。");InvalidateRect(gMain,nullptr,FALSE);return;}
  gProblemDir=dir.wstring();RenderStatement(description);if(gRight<300)gRight=480;Layout();InvalidateRect(gMain,nullptr,FALSE);
}
void OpenFile(const std::wstring&p){if(!ConfirmDiscard())return;auto ext=fs::path(p).extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),towlower);if(ext!=L".py"&&ext!=L".pyw"){SetStatus(L"当前仅支持 .py 和 .pyw");return;}std::wstring s;if(!DecodeFile(p,s))return;gFile=p;SetEditorText(s);LoadProblemForFile(p);SetStatus(L"已打开文件");if(gRoot.empty())SetRoot(fs::path(p).parent_path().wstring());SetFocus(gEdit);}
void OpenTreePath(const std::wstring&p){auto path=fs::path(p);auto name=path.filename().wstring();std::transform(name.begin(),name.end(),name.begin(),towlower);if(name==L"description.md"||name==L"description.html"||name==L"description.htm"){auto solution=path.parent_path()/L"solution.py";if(fs::exists(solution))OpenFile(solution.wstring());else LoadProblemForFile(p);return;}OpenFile(p);}

int S(int value){return pylite_ui::Scale(value,gDpi);}

void ApplyParagraphSpacing(HWND edit){
  if(!edit)return;bool wasFormatting=gFormatting;gFormatting=true;CHARRANGE saved{};SendMessage(edit,EM_EXGETSEL,0,(LPARAM)&saved);BOOL modified=(BOOL)SendMessage(edit,EM_GETMODIFY,0,0);SendMessage(edit,EM_SETSEL,0,-1);PARAFORMAT2 format{sizeof(format)};format.dwMask=PFM_LINESPACING;format.bLineSpacingRule=5;format.dyLineSpacing=28;SendMessage(edit,EM_SETPARAFORMAT,0,(LPARAM)&format);SendMessage(edit,EM_EXSETSEL,0,(LPARAM)&saved);SendMessage(edit,EM_SETMODIFY,modified,0);gFormatting=wasFormatting;
}

std::string JoinWords(const std::vector<std::string>&words){std::string out;for(const auto&word:words){if(!out.empty())out+=' ';out+=word;}return out;}
void SetEditorStyle(int style,COLORREF fore,COLORREF back,bool bold=false){Sci(SCI_STYLESETFORE,style,fore);Sci(SCI_STYLESETBACK,style,back);Sci(SCI_STYLESETBOLD,style,bold);}
void UpdateLineNumberMargin(){if(!gEdit)return;const int lines=std::max(1,(int)Sci(SCI_GETLINECOUNT));int digits=3;for(int value=lines;value>=1000;value/=10)digits++;std::string sample((size_t)digits,'9');const int width=(int)Sci(SCI_TEXTWIDTH,STYLE_LINENUMBER,(sptr_t)sample.c_str())+S(14);Sci(SCI_SETMARGINWIDTHN,0,width);}
void ConfigureEditor(){
  if(!gEdit)return;const char*font=pylite_ui::FontExists(L"Cascadia Mono")?"Cascadia Mono":"Consolas";const auto bg=pylite_ui::Theme::EditorBackground,fg=pylite_ui::Theme::TextPrimary;
  Sci(SCI_SETCODEPAGE,SC_CP_UTF8);Sci(SCI_SETTECHNOLOGY,SC_TECHNOLOGY_DIRECTWRITE);Sci(SCI_SETBUFFEREDDRAW,TRUE);Sci(SCI_SETPHASESDRAW,SC_PHASES_MULTIPLE);Sci(SCI_SETFONTQUALITY,SC_EFF_QUALITY_LCD_OPTIMIZED);Sci(SCI_SETIDLESTYLING,SC_IDLESTYLING_AFTERVISIBLE);
  Sci(SCI_STYLESETFONT,STYLE_DEFAULT,(sptr_t)font);Sci(SCI_STYLESETSIZEFRACTIONAL,STYLE_DEFAULT,1100);Sci(SCI_STYLESETFORE,STYLE_DEFAULT,fg);Sci(SCI_STYLESETBACK,STYLE_DEFAULT,bg);Sci(SCI_STYLECLEARALL);
  SetEditorStyle(SCE_P_DEFAULT,fg,bg);SetEditorStyle(SCE_P_COMMENTLINE,RGB(110,119,129),bg);SetEditorStyle(SCE_P_COMMENTBLOCK,RGB(110,119,129),bg);SetEditorStyle(SCE_P_NUMBER,RGB(5,80,174),bg);
  for(int style:{SCE_P_STRING,SCE_P_CHARACTER,SCE_P_TRIPLE,SCE_P_TRIPLEDOUBLE,SCE_P_STRINGEOL,SCE_P_FSTRING,SCE_P_FCHARACTER,SCE_P_FTRIPLE,SCE_P_FTRIPLEDOUBLE})SetEditorStyle(style,RGB(180,35,24),bg);
  SetEditorStyle(SCE_P_WORD,RGB(130,80,223),bg);SetEditorStyle(SCE_P_WORD2,RGB(5,80,174),bg);SetEditorStyle(SCE_P_CLASSNAME,RGB(9,105,218),bg,true);SetEditorStyle(SCE_P_DEFNAME,RGB(9,105,218),bg,true);SetEditorStyle(SCE_P_OPERATOR,fg,bg);SetEditorStyle(SCE_P_IDENTIFIER,fg,bg);SetEditorStyle(SCE_P_DECORATOR,RGB(149,56,0),bg);SetEditorStyle(SCE_P_ATTRIBUTE,RGB(5,120,120),bg);
  Sci(SCI_SETMARGINS,1);Sci(SCI_SETMARGINTYPEN,0,SC_MARGIN_NUMBER);Sci(SCI_STYLESETFONT,STYLE_LINENUMBER,(sptr_t)font);Sci(SCI_STYLESETSIZEFRACTIONAL,STYLE_LINENUMBER,950);Sci(SCI_STYLESETFORE,STYLE_LINENUMBER,pylite_ui::Theme::TextMuted);Sci(SCI_STYLESETBACK,STYLE_LINENUMBER,pylite_ui::Theme::OutputBackground);Sci(SCI_SETMARGINLEFT,0,S(5));Sci(SCI_SETMARGINRIGHT,0,S(8));
  Sci(SCI_SETCARETFORE,pylite_ui::Theme::TextPrimary);Sci(SCI_SETCARETWIDTH,S(2));Sci(SCI_SETCARETLINEVISIBLE,TRUE);Sci(SCI_SETCARETLINEVISIBLEALWAYS,FALSE);Sci(SCI_SETCARETLINEBACK,pylite_ui::Theme::CurrentLine);Sci(SCI_SETSELFORE,TRUE,pylite_ui::Theme::SelectedText);Sci(SCI_SETSELBACK,TRUE,pylite_ui::Theme::SelectedBackground);
  Sci(SCI_SETTABWIDTH,4);Sci(SCI_SETINDENT,4);Sci(SCI_SETUSETABS,FALSE);Sci(SCI_SETWRAPMODE,SC_WRAP_NONE);Sci(SCI_SETSCROLLWIDTHTRACKING,TRUE);Sci(SCI_SETENDATLASTLINE,TRUE);Sci(SCI_SETEXTRAASCENT,S(2));Sci(SCI_SETEXTRADESCENT,S(2));
  Sci(SCI_AUTOCSETIGNORECASE,FALSE);Sci(SCI_AUTOCSETCASEINSENSITIVEBEHAVIOUR,SC_CASEINSENSITIVEBEHAVIOUR_IGNORECASE);Sci(SCI_AUTOCSETORDER,SC_ORDER_CUSTOM);Sci(SCI_AUTOCSETMAXHEIGHT,10);Sci(SCI_AUTOCSETMAXWIDTH,40);Sci(SCI_AUTOCSETDROPRESTOFWORD,TRUE);
  UpdateLineNumberMargin();
}

void DeleteUiResources(){
  for(auto font:{gUiFont,gUiMediumFont,gCodeFont,gIconFont})if(font)DeleteObject(font);
  for(auto brush:{gAppBrush,gPanelBrush,gOutputBrush,gHoverBrush,gSelectedBrush,gPrimaryBrush,gPrimaryHoverBrush,gSuccessBrush,gErrorBrush,gFocusBrush,gMutedBrush})if(brush)DeleteObject(brush);
  for(auto pen:{gBorderPen,gDividerPen})if(pen)DeleteObject(pen);
  gUiFont=gUiMediumFont=gCodeFont=gIconFont=nullptr;
  gAppBrush=gPanelBrush=gOutputBrush=gHoverBrush=gSelectedBrush=gPrimaryBrush=gPrimaryHoverBrush=gSuccessBrush=gErrorBrush=gFocusBrush=gMutedBrush=nullptr;
  gBorderPen=gDividerPen=nullptr;
}

void CreateUiResources(){
  DeleteUiResources();
  gUiFont=pylite_ui::CreateUiFont(gDpi,10,FW_NORMAL);
  gUiMediumFont=pylite_ui::CreateUiFont(gDpi,10,FW_SEMIBOLD);
  gCodeFont=pylite_ui::CreateCodeFont(gDpi,11);
  gIconFont=pylite_ui::CreateIconFont(gDpi,11);
  gAppBrush=CreateSolidBrush(pylite_ui::Theme::AppBackground);
  gPanelBrush=CreateSolidBrush(pylite_ui::Theme::PanelBackground);
  gOutputBrush=CreateSolidBrush(pylite_ui::Theme::OutputBackground);
  gHoverBrush=CreateSolidBrush(pylite_ui::Theme::HoverBackground);
  gSelectedBrush=CreateSolidBrush(pylite_ui::Theme::SelectedBackground);
  gPrimaryBrush=CreateSolidBrush(pylite_ui::Theme::PrimaryButton);
  gPrimaryHoverBrush=CreateSolidBrush(pylite_ui::Theme::PrimaryButtonHover);
  gSuccessBrush=CreateSolidBrush(pylite_ui::Theme::Success);
  gErrorBrush=CreateSolidBrush(pylite_ui::Theme::Error);
  gFocusBrush=CreateSolidBrush(pylite_ui::Theme::FocusAccent);
  gMutedBrush=CreateSolidBrush(pylite_ui::Theme::TextMuted);
  gBorderPen=CreatePen(PS_SOLID,1,pylite_ui::Theme::Border);
  gDividerPen=CreatePen(PS_SOLID,1,pylite_ui::Theme::Divider);
}

void SetControlFonts(){
  for(HWND x:{gTree,gPy,gRun,gTest,gSave,gFolder,gClear,gToggle,gInputTab,gOutputTab,gStatement})if(x)SendMessage(x,WM_SETFONT,(WPARAM)gUiFont,TRUE);
  for(HWND x:{gOutput,gInput})if(x)SendMessage(x,WM_SETFONT,(WPARAM)gCodeFont,TRUE);
}

void AddTooltip(HWND target,const wchar_t*text){
  if(!gTooltip||!target)return;TOOLINFOW info{sizeof(info)};info.uFlags=TTF_IDISHWND|TTF_SUBCLASS;info.hwnd=gMain;info.uId=(UINT_PTR)target;info.lpszText=(LPWSTR)text;SendMessage(gTooltip,TTM_ADDTOOLW,0,(LPARAM)&info);
}

void PaintWithBrush(HDC dc,const RECT&r,HBRUSH brush){FillRect(dc,&r,brush);}
void PaintRounded(HDC dc,const RECT&r,HBRUSH brush,int radius){auto oldBrush=SelectObject(dc,brush);auto oldPen=SelectObject(dc,GetStockObject(NULL_PEN));RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,oldPen);SelectObject(dc,oldBrush);}
void StrokeRounded(HDC dc,const RECT&r,HPEN pen,int radius){auto oldPen=SelectObject(dc,pen);auto oldBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,oldBrush);SelectObject(dc,oldPen);}

struct UiLayout{
  RECT client{},command{},leftPanel{},rightPanel{},editorCard{},outputCard{},status{},leftHeader{},rightHeader{},outputHeader{};
  int leftSplit{},rightSplit{},outputSplit{};
};

UiLayout MeasureLayout(){
  UiLayout u;GetClientRect(gMain,&u.client);
  const int tool=S(46),status=S(24),split=S(5),panelPad=S(8),header=S(38),outputHeader=S(34);
  const int clientRight=(int)u.client.right,bottom=(int)u.client.bottom-status;
  const int left=std::clamp(S(gLeft),S(160),std::max(S(160),clientRight-S(gRight)-S(405)));
  const int right=std::clamp(S(gRight),S(160),std::max(S(160),clientRight-left-S(405)));
  const int rightX=clientRight-right;
  const int midL=left+split,midR=rightX-split;
  const int output=gOutputCollapsed?outputHeader+split+panelPad:std::clamp(S(gSavedOutput),S(90),std::max(S(90),bottom-tool-S(150)));
  const int splitY=bottom-output-split;
  u.command={0,0,u.client.right,tool};
  u.status={0,bottom,u.client.right,u.client.bottom};
  u.leftPanel={0,tool,left,bottom};
  u.rightPanel={rightX,tool,u.client.right,bottom};
  u.leftHeader={0,tool,left,tool+header};
  u.rightHeader={rightX,tool,u.client.right,tool+header};
  u.leftSplit=left;u.rightSplit=rightX;u.outputSplit=splitY;
  u.editorCard={midL+panelPad,tool+panelPad,midR-panelPad,splitY};
  u.outputCard={midL+panelPad,splitY+split,midR-panelPad,bottom-panelPad};
  u.outputHeader={u.outputCard.left,u.outputCard.top,u.outputCard.right,std::min(u.outputCard.bottom,u.outputCard.top+outputHeader)};
  return u;
}

RECT MenuRect(int index){
  const int widths[]={S(52),S(52),S(52),S(52)};int x=S(10);
  for(int i=0;i<index;i++)x+=widths[i];
  return{x,S(6),x+widths[index],S(40)};
}

int HitMenu(int x,int y){if(y<0||y>=S(46))return-1;for(int i=0;i<4;i++){RECT r=MenuRect(i);if(PtInRect(&r,{x,y}))return i;}return-1;}

void ShowTopMenu(int index){
  if(!gMenuBar||index<0||index>3)return;RECT r=MenuRect(index);POINT p{r.left,r.bottom};ClientToScreen(gMain,&p);
  TrackPopupMenuEx(GetSubMenu(gMenuBar,index),TPM_LEFTALIGN|TPM_TOPALIGN|TPM_RIGHTBUTTON,p.x,p.y,gMain,nullptr);
}

LRESULT CALLBACK ButtonProc(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
  if(m==WM_MOUSEMOVE&&!GetWindowLongPtrW(h,GWLP_USERDATA)){SetWindowLongPtrW(h,GWLP_USERDATA,1);TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);InvalidateRect(h,nullptr,FALSE);}
  if(m==WM_MOUSELEAVE){SetWindowLongPtrW(h,GWLP_USERDATA,0);InvalidateRect(h,nullptr,FALSE);}
  if(m==WM_SETFOCUS||m==WM_KILLFOCUS)InvalidateRect(h,nullptr,FALSE);
  return DefSubclassProc(h,m,w,l);
}

void DrawOwnerButton(const DRAWITEMSTRUCT&d){
  RECT r=d.rcItem;const bool hot=GetWindowLongPtrW(d.hwndItem,GWLP_USERDATA)!=0,pressed=(d.itemState&ODS_SELECTED)!=0,disabled=(d.itemState&ODS_DISABLED)!=0;
  const int radius=S(5),id=(int)d.CtlID;HBRUSH fill=gPanelBrush;COLORREF text=pylite_ui::Theme::TextPrimary;
  const bool selectedTab=(id==ID_TAB_INPUT&&gShowInput)||(id==ID_TAB_OUTPUT&&!gShowInput);
  if(id==ID_RUN){fill=(hot||pressed)?gPrimaryHoverBrush:gPrimaryBrush;text=pylite_ui::Theme::PrimaryButtonText;}
  else if(id==ID_TEST){fill=(hot||pressed)?gHoverBrush:gPanelBrush;}
  else if(id==ID_TAB_INPUT||id==ID_TAB_OUTPUT){fill=selectedTab?gSelectedBrush:gPanelBrush;text=selectedTab?pylite_ui::Theme::SelectedText:pylite_ui::Theme::TextSecondary;}
  else if(id==ID_PY){fill=(hot||pressed)?gHoverBrush:gAppBrush;}
  else if(hot||pressed)fill=gHoverBrush;
  PaintWithBrush(d.hDC,r,gPanelBrush);PaintRounded(d.hDC,r,fill,radius);
  if(id==ID_PY||id==ID_TEST)StrokeRounded(d.hDC,r,gBorderPen,radius);
  if((d.itemState&ODS_FOCUS)&&id!=ID_RUN){RECT f=r;InflateRect(&f,-S(2),-S(2));StrokeRounded(d.hDC,f,gBorderPen,radius);}
  if(id==ID_PY){
    RECT dot{r.left+S(12),r.top+(r.bottom-r.top-S(7))/2,r.left+S(19),r.top+(r.bottom-r.top+S(7))/2};
    HBRUSH b=gInterpreter.empty()?gMutedBrush:gSuccessBrush;auto old=SelectObject(d.hDC,b);auto oldPen=SelectObject(d.hDC,GetStockObject(NULL_PEN));Ellipse(d.hDC,dot.left,dot.top,dot.right,dot.bottom);SelectObject(d.hDC,oldPen);SelectObject(d.hDC,old);
    wchar_t value[128]{};GetWindowTextW(d.hwndItem,value,128);RECT tx=r;tx.left+=S(27);tx.right-=S(22);pylite_ui::Text(d.hDC,value,tx,gUiFont,disabled?pylite_ui::Theme::TextMuted:text,DT_SINGLELINE|DT_VCENTER|DT_LEFT|DT_END_ELLIPSIS);
    RECT arrow{r.right-S(22),r.top,r.right-S(7),r.bottom};pylite_ui::Text(d.hDC,L"\xE70D",arrow,gIconFont,pylite_ui::Theme::TextSecondary,DT_SINGLELINE|DT_VCENTER|DT_CENTER);return;
  }
  if(id==ID_RUN||id==ID_TEST||id==ID_TAB_INPUT||id==ID_TAB_OUTPUT){wchar_t value[64]{};GetWindowTextW(d.hwndItem,value,64);pylite_ui::Text(d.hDC,value,r,(id==ID_RUN||id==ID_TEST||selectedTab)?gUiMediumFont:gUiFont,disabled?pylite_ui::Theme::TextMuted:text,DT_SINGLELINE|DT_VCENTER|DT_CENTER);return;}
  std::wstring glyph;
  if(id==ID_SAVE)glyph=L"\xE74E";else if(id==CMD_FOLDER)glyph=L"\xE838";else if(id==ID_CLEAR)glyph=L"\xE74D";else if(id==ID_TOGGLE)glyph=gOutputCollapsed?L"\xE70D":L"\xE70E";
  pylite_ui::Text(d.hDC,glyph,r,gIconFont,disabled?pylite_ui::Theme::TextMuted:pylite_ui::Theme::TextSecondary,DT_SINGLELINE|DT_VCENTER|DT_CENTER);
}

void Layout(){
  if(!gMain)return;auto u=MeasureLayout();const int cardInset=S(1);
  HDWP d=BeginDeferWindowPos(14);
  auto place=[&](HWND h,const RECT&r){if(h)d=DeferWindowPos(d,h,nullptr,r.left,r.top,(int)std::max<LONG>(0,r.right-r.left),(int)std::max<LONG>(0,r.bottom-r.top),SWP_NOZORDER|SWP_NOACTIVATE);};
  RECT tree{u.leftPanel.left+S(8),u.leftHeader.bottom,u.leftPanel.right-S(8),u.leftPanel.bottom-S(8)};place(gTree,tree);
  RECT edit{u.editorCard.left+cardInset,u.editorCard.top+cardInset,u.editorCard.right-cardInset,u.editorCard.bottom-cardInset};place(gEdit,edit);
  RECT bottomContent{u.outputCard.left+cardInset,u.outputHeader.bottom,u.outputCard.right-cardInset,u.outputCard.bottom-cardInset};place(gOutput,bottomContent);place(gInput,bottomContent);
  RECT statement{u.rightPanel.left+S(8),u.rightHeader.bottom,u.rightPanel.right-S(8),u.rightPanel.bottom-S(8)};place(gStatement,statement);
  RECT save{u.client.right-S(374),S(7),u.client.right-S(342),S(39)};place(gSave,save);
  RECT py{u.client.right-S(334),S(7),u.client.right-S(188),S(39)};place(gPy,py);
  RECT test{u.client.right-S(180),S(7),u.client.right-S(92),S(39)};place(gTest,test);
  RECT run{u.client.right-S(84),S(7),u.client.right-S(12),S(39)};place(gRun,run);
  RECT folder{u.leftPanel.right-S(40),u.leftHeader.top+S(5),u.leftPanel.right-S(8),u.leftHeader.top+S(37)};place(gFolder,folder);
  RECT clear{u.outputHeader.right-S(72),u.outputHeader.top+S(1),u.outputHeader.right-S(40),u.outputHeader.bottom-S(1)};place(gClear,clear);
  RECT toggle{u.outputHeader.right-S(36),u.outputHeader.top+S(1),u.outputHeader.right-S(4),u.outputHeader.bottom-S(1)};place(gToggle,toggle);
  RECT inputTab{u.outputHeader.left+S(8),u.outputHeader.top+S(3),u.outputHeader.left+S(66),u.outputHeader.bottom-S(3)};place(gInputTab,inputTab);
  RECT outputTab{u.outputHeader.left+S(70),u.outputHeader.top+S(3),u.outputHeader.left+S(128),u.outputHeader.bottom-S(3)};place(gOutputTab,outputTab);
  if(d)EndDeferWindowPos(d);
  ShowWindow(gOutput,gOutputCollapsed||gShowInput?SW_HIDE:SW_SHOW);ShowWindow(gInput,gOutputCollapsed||!gShowInput?SW_HIDE:SW_SHOW);
}

void PaintMain(HDC dc){
  auto u=MeasureLayout();PaintWithBrush(dc,u.client,gAppBrush);PaintWithBrush(dc,u.command,gPanelBrush);PaintWithBrush(dc,u.leftPanel,gPanelBrush);PaintWithBrush(dc,u.rightPanel,gPanelBrush);PaintWithBrush(dc,u.status,gPanelBrush);
  RECT editor=u.editorCard,output=u.outputCard;PaintRounded(dc,editor,gPanelBrush,S(6));StrokeRounded(dc,editor,gBorderPen,S(6));PaintRounded(dc,output,gPanelBrush,S(6));StrokeRounded(dc,output,gBorderPen,S(6));
  auto old=SelectObject(dc,gDividerPen);MoveToEx(dc,0,u.command.bottom-1,nullptr);LineTo(dc,u.client.right,u.command.bottom-1);MoveToEx(dc,0,u.status.top,nullptr);LineTo(dc,u.client.right,u.status.top);MoveToEx(dc,u.leftSplit+S(2),u.command.bottom,nullptr);LineTo(dc,u.leftSplit+S(2),u.status.top);MoveToEx(dc,u.rightSplit-S(2),u.command.bottom,nullptr);LineTo(dc,u.rightSplit-S(2),u.status.top);SelectObject(dc,old);
  const wchar_t*menus[]={L"文件",L"编辑",L"运行",L"工具"};for(int i=0;i<4;i++){RECT r=MenuRect(i);if(gHotMenu==i)PaintRounded(dc,r,gHoverBrush,S(4));pylite_ui::Text(dc,menus[i],r,gUiFont,pylite_ui::Theme::TextPrimary,DT_SINGLELINE|DT_VCENTER|DT_CENTER);}
  RECT leftTitle{u.leftHeader.left+S(12),u.leftHeader.top,u.leftHeader.right-S(44),u.leftHeader.bottom};pylite_ui::Text(dc,L"资源管理器",leftTitle,gUiMediumFont,pylite_ui::Theme::TextPrimary,DT_SINGLELINE|DT_VCENTER|DT_LEFT|DT_END_ELLIPSIS);
  RECT rightTitle{u.rightHeader.left+S(12),u.rightHeader.top,u.rightHeader.right-S(12),u.rightHeader.bottom};pylite_ui::Text(dc,L"题目描述",rightTitle,gUiMediumFont,pylite_ui::Theme::TextPrimary,DT_SINGLELINE|DT_VCENTER|DT_LEFT);
  HBRUSH stateBrush=gRunning?gFocusBrush:(gLastExit==0?gSuccessBrush:(gLastExit>0?gErrorBrush:gMutedBrush));auto oldBrush=SelectObject(dc,stateBrush);auto oldPen=SelectObject(dc,GetStockObject(NULL_PEN));int dotX=u.outputHeader.left+S(146),dotY=(u.outputHeader.top+u.outputHeader.bottom)/2;Ellipse(dc,dotX-S(3),dotY-S(3),dotX+S(3),dotY+S(3));SelectObject(dc,oldPen);SelectObject(dc,oldBrush);
  RECT state{dotX+S(8),u.outputHeader.top,u.outputHeader.left+S(260),u.outputHeader.bottom};std::wstring stateText=gRunning?(gRunningTests?L"正在测试":L"正在运行"):(gLastExit==0?L"已完成":(gLastExit>0?L"运行失败":L"尚未运行"));pylite_ui::Text(dc,stateText,state,gUiFont,pylite_ui::Theme::TextSecondary,DT_SINGLELINE|DT_VCENTER|DT_LEFT);
  RECT statusLeft{S(10),u.status.top,u.status.right/2,u.status.bottom};pylite_ui::Text(dc,gStatusText,statusLeft,gUiFont,pylite_ui::Theme::TextSecondary,DT_SINGLELINE|DT_VCENTER|DT_LEFT|DT_END_ELLIPSIS);
  RECT statusRight{u.status.right/2,u.status.top,u.status.right-S(10),u.status.bottom};pylite_ui::Text(dc,gPositionText,statusRight,gUiFont,pylite_ui::Theme::TextSecondary,DT_SINGLELINE|DT_VCENTER|DT_RIGHT|DT_END_ELLIPSIS);
}

std::wstring Capture(const std::wstring&exe,const std::vector<std::wstring>&args,DWORD timeout,DWORD*code=nullptr){SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE rd,wr;if(!CreatePipe(&rd,&wr,&sa,0))return{};SetHandleInformation(rd,HANDLE_FLAG_INHERIT,0);STARTUPINFOW si{sizeof(si)};si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=si.hStdError=wr;si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);PROCESS_INFORMATION pi{};std::wstring cmd=QuoteWindowsArg(exe);for(auto&a:args)cmd+=L" "+QuoteWindowsArg(a);BOOL ok=CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi);CloseHandle(wr);if(!ok){CloseHandle(rd);return{};}DWORD start=GetTickCount(),avail=0;std::string out;char buf[4096];while(true){while(PeekNamedPipe(rd,nullptr,0,nullptr,&avail,nullptr)&&avail){DWORD got;ReadFile(rd,buf,std::min<DWORD>(avail,sizeof(buf)),&got,nullptr);out.append(buf,got);}if(WaitForSingleObject(pi.hProcess,20)==WAIT_OBJECT_0)break;if(GetTickCount()-start>timeout){TerminateProcess(pi.hProcess,1);break;}}DWORD got;while(ReadFile(rd,buf,sizeof(buf),&got,nullptr)&&got)out.append(buf,got);DWORD ec;GetExitCodeProcess(pi.hProcess,&ec);if(code)*code=ec;CloseHandle(rd);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return W(out);}
void ValidatePython(std::wstring p){if(p.empty()){wchar_t q[MAX_PATH];if(SearchPathW(nullptr,L"python.exe",nullptr,MAX_PATH,q,nullptr))p=q;else if(SearchPathW(nullptr,L"python3.exe",nullptr,MAX_PATH,q,nullptr))p=q;}auto o=Capture(p,{L"-c",L"import sys; print(sys.version.split()[0])"},5000);auto v=W(ParsePythonVersion(U8(o)));auto*msg=new std::pair<std::wstring,std::wstring>(p,v);PostMessage(gMain,WM_INTERPRETER,0,(LPARAM)msg);}
void DetectPython(){std::thread(ValidatePython,gInterpreter).detach();}
void ChoosePython(){wchar_t p[32768]{};OPENFILENAMEW o{sizeof(o)};o.hwndOwner=gMain;o.lpstrFile=p;o.nMaxFile=32768;o.lpstrFilter=L"Python (python.exe)\0python.exe\0可执行文件\0*.exe\0";o.Flags=OFN_FILEMUSTEXIST|OFN_EXPLORER;if(GetOpenFileNameW(&o))std::thread(ValidatePython,std::wstring(p)).detach();}

void AppendOutput(const std::wstring&s){int n=GetWindowTextLengthW(gOutput);if(n>4*1024*1024){SendMessage(gOutput,EM_SETSEL,0,n-3*1024*1024);SendMessage(gOutput,EM_REPLACESEL,FALSE,(LPARAM)L"");}SendMessage(gOutput,EM_SETSEL,-1,-1);SendMessage(gOutput,EM_REPLACESEL,FALSE,(LPARAM)s.c_str());SendMessage(gOutput,EM_SCROLLCARET,0,0);}
void RunWorker(std::wstring exe,std::wstring file,std::wstring input){
  SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE outRead{},outWrite{},inRead{},inWrite{};
  if(!CreatePipe(&outRead,&outWrite,&sa,0)||!CreatePipe(&inRead,&inWrite,&sa,0)){if(outRead)CloseHandle(outRead);if(outWrite)CloseHandle(outWrite);if(inRead)CloseHandle(inRead);if(inWrite)CloseHandle(inWrite);PostMessage(gMain,WM_RUNEND,0,1);return;}
  SetHandleInformation(outRead,HANDLE_FLAG_INHERIT,0);SetHandleInformation(inWrite,HANDLE_FLAG_INHERIT,0);
  STARTUPINFOW si{sizeof(si)};si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=si.hStdError=outWrite;si.hStdInput=inRead;PROCESS_INFORMATION pi{};
  std::wstring cmd=QuoteWindowsArg(exe)+L" -u "+QuoteWindowsArg(file);auto cwd=fs::path(file).parent_path().wstring();SetEnvironmentVariableW(L"PYTHONIOENCODING",L"utf-8");
  BOOL ok=CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_NEW_PROCESS_GROUP,nullptr,cwd.c_str(),&si,&pi);CloseHandle(outWrite);CloseHandle(inRead);
  if(!ok){CloseHandle(outRead);CloseHandle(inWrite);PostMessage(gMain,WM_RUNEND,0,1);return;}
  gProcess=pi.hProcess;gJob=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION ji{};ji.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;SetInformationJobObject(gJob,JobObjectExtendedLimitInformation,&ji,sizeof(ji));AssignProcessToJobObject(gJob,pi.hProcess);CloseHandle(pi.hThread);
  auto bytes=U8(input);size_t offset=0;while(offset<bytes.size()){DWORD written=0;DWORD chunk=(DWORD)std::min<size_t>(bytes.size()-offset,65536);if(!WriteFile(inWrite,bytes.data()+offset,chunk,&written,nullptr)||!written)break;offset+=written;}CloseHandle(inWrite);
  char b[4096];DWORD n;while(ReadFile(outRead,b,sizeof(b),&n,nullptr)&&n)PostMessage(gMain,WM_APPEND,0,(LPARAM)new std::wstring(W(std::string(b,n))));CloseHandle(outRead);DWORD ec;WaitForSingleObject(pi.hProcess,INFINITE);GetExitCodeProcess(pi.hProcess,&ec);CloseHandle(pi.hProcess);gProcess=nullptr;if(gJob){CloseHandle(gJob);gJob=nullptr;}PostMessage(gMain,WM_RUNEND,0,ec);
}
void StartRun(bool tests=false){
  if(gRunning)return;if(gFile.empty()||gDirty){SendMessage(gMain,WM_COMMAND,CMD_SAVE,0);if(gFile.empty()||gDirty)return;}if(gInterpreter.empty()){SetStatus(L"请先选择 Python 解释器");ChoosePython();return;}
  if(gProblemDir.empty())LoadProblemForFile(gFile);fs::path target=gFile;if(tests){if(gProblemDir.empty()){MessageBoxW(gMain,L"当前文件不属于题目目录。",L"无法运行测试",MB_ICONINFORMATION);return;}target=fs::path(gProblemDir)/L"test_solution.py";if(!fs::is_regular_file(target)){MessageBoxW(gMain,L"题目目录中缺少 test_solution.py。",L"无法运行测试",MB_ICONERROR);return;}}
  auto input=tests?L"":WindowText(gInput);SetWindowTextW(gOutput,L"");gOutputCollapsed=false;gShowInput=false;gRunning=true;gRunningTests=tests;gLastExit=-1;
  SetWindowTextW(tests?gTest:gRun,L"■ 停止");EnableWindow(tests?gRun:gTest,FALSE);SetStatus(tests?L"正在运行题目测试":L"正在运行代码");Layout();InvalidateRect(gMain,nullptr,FALSE);std::thread(RunWorker,gInterpreter,target.wstring(),input).detach();
}
void StopRun(){if(gJob)TerminateJobObject(gJob,1);else if(gProcess)TerminateProcess(gProcess,1);SetStatus(L"正在停止");}

void ShowCompletion(const std::vector<std::wstring>&v,int prefixBytes=0){
  if(v.empty()){Sci(SCI_AUTOCCANCEL);return;}std::string list;for(size_t i=0;i<v.size()&&i<500;i++){if(!list.empty())list+=' ';list+=U8(v[i]);}Sci(SCI_AUTOCSHOW,prefixBytes,(sptr_t)list.c_str());
}
void CompleteStatic(){auto text=EditorBytes();size_t pos=std::min<size_t>((size_t)Sci(SCI_GETCURRENTPOS),text.size()),start=pos;while(start&&((unsigned char)text[start-1]<128)&&(isalnum((unsigned char)text[start-1])||text[start-1]=='_'))start--;auto c=StaticCompletions(text,text.substr(start,pos-start));std::vector<std::wstring> w;for(auto&s:c)w.push_back(W(s));ShowCompletion(w,(int)(pos-start));}
bool CurrentDynamicCompletion(std::wstring&key,int&prefixBytes,std::string*moduleName=nullptr){auto text=EditorBytes();size_t pos=std::min<size_t>((size_t)Sci(SCI_GETCURRENTPOS),text.size());auto context=AttributeCompletionContext(text.substr(0,pos));if(!context)return false;auto module=ResolveModule(ParseImports(text),context.expression);if(module.empty())return false;key=gInterpreter+L"|"+W(module);prefixBytes=(int)context.prefix.size();if(moduleName)*moduleName=module;return true;}
bool DynamicComplete(){std::wstring key;int prefixBytes=0;std::string module;if(!CurrentDynamicCompletion(key,prefixBytes,&module))return false;{std::lock_guard lk(gCacheMutex);auto i=gModuleCache.find(key);if(i!=gModuleCache.end()){ShowCompletion(i->second,prefixBytes);return true;}}if(gCompletionKey==key)return true;gCompletionKey=key;unsigned req=++gCompletionRequest;SetStatus(L"正在加载模块补全");std::thread([key,module,req]{std::wstring code=L"import importlib,json; m=importlib.import_module('"+W(module)+L"'); print('__PYLITE_JSON__'+json.dumps([x for x in dir(m) if isinstance(x,str)]))";auto o=Capture(gInterpreter,{L"-c",code},7000);std::vector<std::wstring> v;auto pos=o.find(L"__PYLITE_JSON__[");if(pos!=std::wstring::npos){auto s=o.substr(pos+16);std::wregex r(L"\"([A-Za-z_]\\w*)\"");for(auto i=std::wsregex_iterator(s.begin(),s.end(),r);i!=std::wsregex_iterator();++i)v.push_back((*i)[1]);std::sort(v.begin(),v.end(),[](auto&a,auto&b){return PublicFirst(U8(a),U8(b));});if(v.size()>500)v.resize(500);}{std::lock_guard lk(gCacheMutex);gModuleCache[key]=v;}PostMessage(gMain,WM_COMPLETIONS,req,(LPARAM)new std::vector<std::wstring>(v));}).detach();return true;}
void RegisterOpen(bool remove){wchar_t exe[MAX_PATH];GetModuleFileNameW(nullptr,exe,MAX_PATH);if(remove){SHDeleteKeyW(HKEY_CURRENT_USER,L"Software\\Classes\\Applications\\PyLite.exe");SHDeleteKeyW(HKEY_CURRENT_USER,L"Software\\Classes\\PyLite.File");}else{HKEY k;RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Classes\\Applications\\PyLite.exe\\shell\\open\\command",0,nullptr,0,KEY_WRITE,nullptr,&k,nullptr);auto cmd=QuoteWindowsArg(exe)+L" \"%1\"";RegSetValueExW(k,nullptr,0,REG_SZ,(BYTE*)cmd.c_str(),(DWORD)((cmd.size()+1)*2));RegCloseKey(k);RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Classes\\Applications\\PyLite.exe\\SupportedTypes",0,nullptr,0,KEY_WRITE,nullptr,&k,nullptr);RegSetValueExW(k,L".py",0,REG_NONE,nullptr,0);RegSetValueExW(k,L".pyw",0,REG_NONE,nullptr,0);RegCloseKey(k);RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Classes\\PyLite.File\\shell\\open\\command",0,nullptr,0,KEY_WRITE,nullptr,&k,nullptr);RegSetValueExW(k,nullptr,0,REG_SZ,(BYTE*)cmd.c_str(),(DWORD)((cmd.size()+1)*2));RegCloseKey(k);for(auto e:{L".py",L".pyw"}){std::wstring p=L"Software\\Classes\\"+std::wstring(e)+L"\\OpenWithProgids";RegCreateKeyExW(HKEY_CURRENT_USER,p.c_str(),0,nullptr,0,KEY_WRITE,nullptr,&k,nullptr);RegSetValueExW(k,L"PyLite.File",0,REG_NONE,nullptr,0);RegCloseKey(k);}}SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,nullptr,nullptr);SetStatus(remove?L"已从“打开方式”移除":L"已注册到“打开方式”");}

HMENU Menus(){HMENU bar=CreateMenu(),f=CreatePopupMenu(),e=CreatePopupMenu(),r=CreatePopupMenu(),t=CreatePopupMenu();AppendMenuW(f,MF_STRING,CMD_NEW,L"新建\tCtrl+N");AppendMenuW(f,MF_STRING,CMD_OPEN,L"打开文件…\tCtrl+O");AppendMenuW(f,MF_STRING,CMD_FOLDER,L"打开文件夹…\tCtrl+Shift+O");AppendMenuW(f,MF_SEPARATOR,0,nullptr);AppendMenuW(f,MF_STRING,CMD_SAVE,L"保存\tCtrl+S");AppendMenuW(f,MF_STRING,CMD_SAVEAS,L"另存为…\tCtrl+Shift+S");AppendMenuW(f,MF_SEPARATOR,0,nullptr);AppendMenuW(f,MF_STRING,CMD_EXIT,L"退出");AppendMenuW(e,MF_STRING,CMD_UNDO,L"撤销\tCtrl+Z");AppendMenuW(e,MF_STRING,CMD_REDO,L"重做\tCtrl+Y");AppendMenuW(e,MF_SEPARATOR,0,nullptr);AppendMenuW(e,MF_STRING,CMD_CUT,L"剪切");AppendMenuW(e,MF_STRING,CMD_COPY,L"复制");AppendMenuW(e,MF_STRING,CMD_PASTE,L"粘贴");AppendMenuW(e,MF_STRING,CMD_ALL,L"全选\tCtrl+A");AppendMenuW(r,MF_STRING,CMD_RUN,L"运行代码\tF5");AppendMenuW(r,MF_STRING,CMD_TEST,L"运行测试\tCtrl+F5");AppendMenuW(r,MF_STRING,CMD_STOP,L"停止\tShift+F5");AppendMenuW(r,MF_STRING,CMD_TOGGLE,L"折叠/展开底栏\tCtrl+J");AppendMenuW(t,MF_STRING,CMD_SELECTPY,L"选择 Python…");AppendMenuW(t,MF_SEPARATOR,0,nullptr);AppendMenuW(t,MF_STRING,CMD_REGISTER,L"注册到“打开方式”");AppendMenuW(t,MF_STRING,CMD_UNREGISTER,L"从“打开方式”移除");AppendMenuW(t,MF_STRING,CMD_ABOUT,L"关于");AppendMenuW(bar,MF_POPUP,(UINT_PTR)f,L"文件");AppendMenuW(bar,MF_POPUP,(UINT_PTR)e,L"编辑");AppendMenuW(bar,MF_POPUP,(UINT_PTR)r,L"运行");AppendMenuW(bar,MF_POPUP,(UINT_PTR)t,L"工具");return bar;}

LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l){
  switch(m){
  case WM_CREATE:{
    gMain=h;gDpi=pylite_ui::WindowDpi(h);LoadLibraryW(L"Msftedit.dll");CreateUiResources();gMenuBar=Menus();
    gTree=CreateWindowExW(0,WC_TREEVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TVS_HASBUTTONS|TVS_SHOWSELALWAYS|TVS_FULLROWSELECT,0,0,0,0,h,(HMENU)ID_TREE,gInst,nullptr);
    gEdit=CreateWindowExW(0,L"Scintilla",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|WS_HSCROLL|WS_CLIPCHILDREN,0,0,0,0,h,(HMENU)ID_EDIT,gInst,nullptr);
    gOutput=CreateWindowExW(0,MSFTEDIT_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,0,0,0,0,h,(HMENU)ID_OUTPUT,gInst,nullptr);
    gInput=CreateWindowExW(0,MSFTEDIT_CLASS,L"",WS_CHILD|WS_VSCROLL|WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,0,0,0,0,h,(HMENU)ID_INPUT,gInst,nullptr);
    gStatement=CreateWindowExW(0,MSFTEDIT_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,0,0,0,0,h,(HMENU)ID_STATEMENT,gInst,nullptr);
    gPy=CreateWindowExW(0,L"BUTTON",L"选择 Python",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,h,(HMENU)ID_PY,gInst,nullptr);
    gRun=CreateWindowExW(0,L"BUTTON",L"▶ 运行",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,h,(HMENU)ID_RUN,gInst,nullptr);
    gTest=CreateWindowExW(0,L"BUTTON",L"✓ 测试",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,h,(HMENU)ID_TEST,gInst,nullptr);
    gSave=CreateWindowExW(0,L"BUTTON",L"保存",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,h,(HMENU)ID_SAVE,gInst,nullptr);
    gFolder=CreateWindowExW(0,L"BUTTON",L"打开文件夹",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,h,(HMENU)CMD_FOLDER,gInst,nullptr);
    gClear=CreateWindowExW(0,L"BUTTON",L"清空",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,h,(HMENU)ID_CLEAR,gInst,nullptr);
    gToggle=CreateWindowExW(0,L"BUTTON",L"折叠",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,h,(HMENU)ID_TOGGLE,gInst,nullptr);
    gInputTab=CreateWindowExW(0,L"BUTTON",L"输入",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,h,(HMENU)ID_TAB_INPUT,gInst,nullptr);
    gOutputTab=CreateWindowExW(0,L"BUTTON",L"输出",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,h,(HMENU)ID_TAB_OUTPUT,gInst,nullptr);
    gTooltip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,h,nullptr,gInst,nullptr);
    AddTooltip(gSave,L"保存  Ctrl+S");AddTooltip(gFolder,L"打开文件夹  Ctrl+Shift+O");AddTooltip(gClear,L"清空当前输入或输出");AddTooltip(gToggle,L"折叠或展开底栏  Ctrl+J");AddTooltip(gPy,L"选择 Python 解释器");AddTooltip(gRun,L"运行当前代码  F5");AddTooltip(gTest,L"运行题目测试  Ctrl+F5");SendMessage(gTooltip,TTM_SETMAXTIPWIDTH,0,S(300));
    SetWindowTheme(gTree,L"Explorer",nullptr);TreeView_SetBkColor(gTree,pylite_ui::Theme::PanelBackground);TreeView_SetTextColor(gTree,pylite_ui::Theme::TextPrimary);TreeView_SetIndent(gTree,S(18));TreeView_SetItemHeight(gTree,S(28));
    for(HWND edit:{gOutput,gInput}){SendMessage(edit,EM_SETBKGNDCOLOR,0,pylite_ui::Theme::OutputBackground);SendMessage(edit,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(S(12),S(10)));}SendMessage(gStatement,EM_SETBKGNDCOLOR,0,pylite_ui::Theme::PanelBackground);SendMessage(gStatement,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(S(14),S(12)));SetWindowTextW(gStatement,L"从左侧打开题目目录中的 solution.py，即可在这里阅读题面。");
    SetControlFonts();Sci(SCI_SETILEXER,0,(sptr_t)CreatePythonLexer());auto keywords=JoinWords(Keywords()),builtins=JoinWords(Builtins());Sci(SCI_SETKEYWORDS,0,(sptr_t)keywords.c_str());Sci(SCI_SETKEYWORDS,1,(sptr_t)builtins.c_str());ConfigureEditor();ApplyParagraphSpacing(gOutput);ApplyParagraphSpacing(gInput);
    for(HWND button:{gPy,gRun,gTest,gSave,gFolder,gClear,gToggle,gInputTab,gOutputTab})SetWindowSubclass(button,ButtonProc,1,0);
    LoadSettings();{auto bundled=BundledProblems();std::wstring leaf;if(!gRoot.empty())leaf=fs::path(gRoot).filename().wstring();std::transform(leaf.begin(),leaf.end(),leaf.begin(),towlower);if(!bundled.empty()&&(gRoot.empty()||!fs::exists(gRoot)||leaf==L"examples"))gRoot=bundled;}Layout();if(!gRoot.empty()&&fs::exists(gRoot))SetRoot(gRoot);DetectPython();UpdateTitle();return 0;
  }
  case WM_SIZE:Layout();InvalidateRect(h,nullptr,FALSE);return 0;
  case WM_DPICHANGED:{
    gDpi=HIWORD(w);auto suggested=(RECT*)l;SetWindowPos(h,nullptr,suggested->left,suggested->top,suggested->right-suggested->left,suggested->bottom-suggested->top,SWP_NOZORDER|SWP_NOACTIVATE);CreateUiResources();SetControlFonts();ConfigureEditor();ApplyParagraphSpacing(gOutput);ApplyParagraphSpacing(gInput);for(HWND edit:{gOutput,gInput})SendMessage(edit,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(S(12),S(10)));SendMessage(gStatement,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(S(14),S(12)));if(!gProblemDir.empty())RenderStatement(DescriptionFor(gProblemDir));TreeView_SetIndent(gTree,S(18));TreeView_SetItemHeight(gTree,S(28));Layout();InvalidateRect(h,nullptr,FALSE);return 0;
  }
  case WM_PAINT:{PAINTSTRUCT p;auto dc=BeginPaint(h,&p);PaintMain(dc);EndPaint(h,&p);return 0;}
  case WM_ERASEBKGND:return 1;
  case WM_GETMINMAXINFO:{auto*x=(MINMAXINFO*)l;x->ptMinTrackSize={S(900),S(560)};return 0;}
  case WM_SETCURSOR:{
    if(LOWORD(l)==HTCLIENT){POINT p;GetCursorPos(&p);ScreenToClient(h,&p);auto u=MeasureLayout();if((abs(p.x-u.leftSplit)<=S(3)||abs(p.x-u.rightSplit)<=S(3))&&p.y>u.command.bottom&&p.y<u.status.top){SetCursor(LoadCursor(nullptr,IDC_SIZEWE));return TRUE;}if(abs(p.y-u.outputSplit)<=S(3)&&p.x>u.leftSplit&&p.x<u.rightSplit){SetCursor(LoadCursor(nullptr,IDC_SIZENS));return TRUE;}}
    break;
  }
  case WM_LBUTTONDOWN:{
    int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);auto u=MeasureLayout();
    if(abs(x-u.leftSplit)<=S(4)&&y>u.command.bottom&&y<u.status.top)gDrag=1;
    else if(abs(x-u.rightSplit)<=S(4)&&y>u.command.bottom&&y<u.status.top)gDrag=2;
    else if(abs(y-u.outputSplit)<=S(4)&&x>u.leftSplit&&x<u.rightSplit)gDrag=3;
    if(gDrag){SetCapture(h);gDragStart={x,y};gDragValue=gDrag==1?gLeft:gDrag==2?gRight:gSavedOutput;}return 0;
  }
  case WM_MOUSEMOVE:{
    int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);
    if(gDrag){RECT r;GetClientRect(h,&r);int dx=MulDiv(x-gDragStart.x,96,(int)gDpi),dy=MulDiv(y-gDragStart.y,96,(int)gDpi),logicalWidth=MulDiv(r.right,96,(int)gDpi),logicalHeight=MulDiv(r.bottom,96,(int)gDpi);
      if(gDrag==1)gLeft=std::clamp(gDragValue+dx,160,std::max(160,logicalWidth-gRight-405));
      if(gDrag==2)gRight=std::clamp(gDragValue-dx,160,std::max(160,logicalWidth-gLeft-405));
      if(gDrag==3){gSavedOutput=std::clamp(gDragValue-dy,90,std::max(90,logicalHeight-150));gOutputCollapsed=false;}
      Layout();InvalidateRect(h,nullptr,FALSE);return 0;
    }
    int hot=HitMenu(x,y);if(hot!=gHotMenu){gHotMenu=hot;auto u=MeasureLayout();InvalidateRect(h,&u.command,FALSE);}TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);return 0;
  }
  case WM_MOUSELEAVE:if(gHotMenu!=-1){gHotMenu=-1;auto u=MeasureLayout();InvalidateRect(h,&u.command,FALSE);}return 0;
  case WM_LBUTTONUP:{
    if(gDrag){gDrag=0;ReleaseCapture();SaveSettings();return 0;}
    int menu=HitMenu(GET_X_LPARAM(l),GET_Y_LPARAM(l));if(menu>=0)ShowTopMenu(menu);return 0;
  }
  case WM_SYSKEYDOWN:if((GetKeyState(VK_MENU)&0x8000)!=0){int menu=w=='F'?0:w=='E'?1:w=='R'?2:w=='T'?3:-1;if(menu>=0){ShowTopMenu(menu);return 0;}}break;
  case WM_DRAWITEM:{auto*d=(DRAWITEMSTRUCT*)l;if(d->CtlType==ODT_BUTTON){DrawOwnerButton(*d);return TRUE;}break;}
  case WM_NOTIFY:{
    auto*n=(NMHDR*)l;
    if(n->idFrom==ID_TREE&&n->code==NM_CUSTOMDRAW){auto*cd=(NMTVCUSTOMDRAW*)l;if(cd->nmcd.dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;if(cd->nmcd.dwDrawStage==CDDS_ITEMPREPAINT){bool selected=(cd->nmcd.uItemState&CDIS_SELECTED)!=0;cd->clrText=selected?pylite_ui::Theme::SelectedText:pylite_ui::Theme::TextPrimary;cd->clrTextBk=selected?pylite_ui::Theme::SelectedBackground:pylite_ui::Theme::PanelBackground;return CDRF_NEWFONT;}}
    if(n->idFrom==ID_TREE&&n->code==TVN_DELETEITEMW){auto*t=(NMTREEVIEWW*)l;if(t->itemOld.lParam)delete (std::wstring*)t->itemOld.lParam;}
    if(n->idFrom==ID_TREE&&n->code==TVN_ITEMEXPANDINGW){auto*t=(NMTREEVIEWW*)l;if(t->action==TVE_EXPAND){TVITEMW i{};i.mask=TVIF_PARAM;i.hItem=t->itemNew.hItem;TreeView_GetItem(gTree,&i);if(i.lParam)LoadChildren(i.hItem,*(std::wstring*)i.lParam);}}
    if(n->idFrom==ID_TREE&&n->code==NM_DBLCLK){auto item=TreeView_GetSelection(gTree);TVITEMW i{};i.mask=TVIF_PARAM;i.hItem=item;TreeView_GetItem(gTree,&i);if(i.lParam&&!fs::is_directory(*(std::wstring*)i.lParam))OpenTreePath(*(std::wstring*)i.lParam);}
    if(n->idFrom==ID_EDIT){auto*sc=(SCNotification*)l;
      if(sc->nmhdr.code==SCN_SAVEPOINTLEFT&&!gFormatting){gDirty=true;UpdateTitle();}
      else if(sc->nmhdr.code==SCN_SAVEPOINTREACHED&&!gFormatting){gDirty=false;UpdateTitle();}
      else if(sc->nmhdr.code==SCN_UPDATEUI){auto pos=Sci(SCI_GETCURRENTPOS);int line=(int)Sci(SCI_LINEFROMPOSITION,pos),column=(int)Sci(SCI_GETCOLUMN,pos);gPositionText=L"Ln "+std::to_wstring(line+1)+L", Col "+std::to_wstring(column+1)+L"    "+gEncoding+L"    Python";auto u=MeasureLayout();InvalidateRect(h,&u.status,FALSE);}
      else if(sc->nmhdr.code==SCN_CHARADDED){if(sc->ch=='.')DynamicComplete();else if(sc->ch=='_'||(sc->ch>0&&sc->ch<128&&isalnum((unsigned char)sc->ch))){if(!DynamicComplete())CompleteStatic();}}
      else if(sc->nmhdr.code==SCN_MODIFIED&&(sc->modificationType&(SC_MOD_INSERTTEXT|SC_MOD_DELETETEXT)))UpdateLineNumberMargin();
    }
    return 0;
  }
  case WM_COMMAND:{
    int id=LOWORD(w);
    if(id==ID_PY||id==CMD_SELECTPY)ChoosePython();else if(id==ID_RUN||id==CMD_RUN)(gRunning?StopRun():StartRun(false));else if(id==ID_TEST||id==CMD_TEST)(gRunning?StopRun():StartRun(true));else if(id==CMD_STOP)StopRun();
    else if(id==CMD_NEW){if(ConfirmDiscard()){gFile.clear();gProblemDir.clear();gEncoding=L"UTF-8";SetEditorText(L"");SetWindowTextW(gStatement,L"从左侧打开题目目录中的 solution.py，即可在这里阅读题面。");}}
    else if(id==CMD_OPEN){auto p=FileDialog(false);if(!p.empty())OpenFile(p);}else if(id==CMD_FOLDER){auto p=FolderDialog();if(!p.empty())SetRoot(p);}
    else if(id==CMD_SAVE||id==ID_SAVE){if(gFile.empty()){auto p=FileDialog(true);if(!p.empty())SaveTo(p);}else SaveTo(gFile);}else if(id==CMD_SAVEAS){auto p=FileDialog(true);if(!p.empty()){gEncoding=L"UTF-8";SaveTo(p);}}
    else if(id==CMD_EXIT)SendMessage(h,WM_CLOSE,0,0);else if(id==CMD_UNDO)Sci(SCI_UNDO);else if(id==CMD_REDO)Sci(SCI_REDO);else if(id==CMD_CUT)Sci(SCI_CUT);else if(id==CMD_COPY)Sci(SCI_COPY);else if(id==CMD_PASTE)Sci(SCI_PASTE);else if(id==CMD_ALL)Sci(SCI_SELECTALL);
    else if(id==ID_TAB_INPUT||id==ID_TAB_OUTPUT){gShowInput=id==ID_TAB_INPUT;gOutputCollapsed=false;Layout();InvalidateRect(gInputTab,nullptr,FALSE);InvalidateRect(gOutputTab,nullptr,FALSE);InvalidateRect(h,nullptr,FALSE);}
    else if(id==CMD_TOGGLE||id==ID_TOGGLE){gOutputCollapsed=!gOutputCollapsed;Layout();InvalidateRect(gToggle,nullptr,FALSE);InvalidateRect(h,nullptr,FALSE);SaveSettings();}else if(id==ID_CLEAR){SetWindowTextW(gShowInput?gInput:gOutput,L"");if(!gShowInput)gLastExit=-1;InvalidateRect(h,nullptr,FALSE);}
    else if(id==CMD_REGISTER)RegisterOpen(false);else if(id==CMD_UNREGISTER)RegisterOpen(true);else if(id==CMD_ABOUT)MessageBoxW(h,L"PyLite 1.0\n轻量级 Python 编辑器\n原生 Win32 便携版",L"关于",MB_OK);return 0;
  }
  case WM_APPEND:{auto*s=(std::wstring*)l;AppendOutput(*s);delete s;return 0;}
  case WM_RUNEND:{bool tests=gRunningTests;gRunning=false;gRunningTests=false;SetWindowTextW(gRun,L"▶ 运行");SetWindowTextW(gTest,L"✓ 测试");EnableWindow(gRun,TRUE);EnableWindow(gTest,TRUE);gLastExit=(int)(DWORD)l;AppendOutput(L"\r\n● 已完成 · 退出码 "+std::to_wstring((DWORD)l)+L"\r\n");SetStatus(tests?(gLastExit==0?L"全部测试通过":L"测试未通过"):(gLastExit==0?L"运行成功":L"运行失败"));InvalidateRect(gRun,nullptr,FALSE);InvalidateRect(gTest,nullptr,FALSE);InvalidateRect(h,nullptr,FALSE);return 0;}
  case WM_INTERPRETER:{auto*p=(std::pair<std::wstring,std::wstring>*)l;if(!p->second.empty()){gInterpreter=p->first;SetWindowTextW(gPy,p->second.c_str());SetStatus(L"Python 解释器有效");{std::lock_guard lk(gCacheMutex);gModuleCache.clear();}gCompletionKey.clear();++gCompletionRequest;SaveSettings();}else{SetWindowTextW(gPy,L"选择 Python");SetStatus(L"Python 解释器不可用");}InvalidateRect(gPy,nullptr,FALSE);delete p;return 0;}
  case WM_COMPLETIONS:{auto*v=(std::vector<std::wstring>*)l;if((unsigned)w==gCompletionRequest){std::wstring key;int prefixBytes=0;if(CurrentDynamicCompletion(key,prefixBytes)&&key==gCompletionKey){ShowCompletion(*v,prefixBytes);SetStatus(v->empty()?L"模块补全不可用":L"模块补全已加载");}}delete v;return 0;}
  case WM_CLOSE:if(!ConfirmDiscard())return 0;SaveSettings();if(gJob)TerminateJobObject(gJob,1);DestroyWindow(h);return 0;
  case WM_DESTROY:if(gMenuBar){DestroyMenu(gMenuBar);gMenuBar=nullptr;}DeleteUiResources();PostQuitMessage(0);return 0;
  }
  return DefWindowProcW(h,m,w,l);
}

int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,PWSTR,int show){
  gInst=hi;CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_TREEVIEW_CLASSES|ICC_BAR_CLASSES|ICC_STANDARD_CLASSES};InitCommonControlsEx(&ic);
  int argc;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(argc>1&&wcscmp(argv[1],L"--register-open-with")==0){gMain=nullptr;RegisterOpen(false);return 0;}if(argc>1&&wcscmp(argv[1],L"--unregister-open-with")==0){gMain=nullptr;RegisterOpen(true);return 0;}
  if(!Scintilla_RegisterClasses(hi)){MessageBoxW(nullptr,L"无法初始化代码编辑器。",L"PyLite",MB_ICONERROR);LocalFree(argv);CoUninitialize();return 1;}
  WNDCLASSEXW wc{sizeof(wc),0,Proc,0,0,hi,LoadIconW(hi,MAKEINTRESOURCEW(1)),LoadCursor(nullptr,IDC_ARROW),nullptr,nullptr,L"PyLiteWindow",LoadIconW(hi,MAKEINTRESOURCEW(1))};RegisterClassExW(&wc);
  auto h=CreateWindowExW(0,wc.lpszClassName,L"未命名.py — PyLite",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1200,800,nullptr,nullptr,hi,nullptr);ShowWindow(h,show);UpdateWindow(h);if(argc>1&&argv[1][0]!=L'-')OpenFile(argv[1]);LocalFree(argv);
  ACCEL a[]={{FVIRTKEY|FCONTROL,'N',CMD_NEW},{FVIRTKEY|FCONTROL,'O',CMD_OPEN},{FVIRTKEY|FCONTROL|FSHIFT,'O',CMD_FOLDER},{FVIRTKEY|FCONTROL,'S',CMD_SAVE},{FVIRTKEY|FCONTROL|FSHIFT,'S',CMD_SAVEAS},{FVIRTKEY|FCONTROL,'Z',CMD_UNDO},{FVIRTKEY|FCONTROL,'Y',CMD_REDO},{FVIRTKEY|FCONTROL,'A',CMD_ALL},{FVIRTKEY,VK_F5,CMD_RUN},{FVIRTKEY|FCONTROL,VK_F5,CMD_TEST},{FVIRTKEY|FSHIFT,VK_F5,CMD_STOP},{FVIRTKEY|FCONTROL,'J',CMD_TOGGLE}};auto ac=CreateAcceleratorTableW(a,ARRAYSIZE(a));MSG msg;while(GetMessageW(&msg,nullptr,0,0)){if(!TranslateAcceleratorW(h,ac,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}DestroyAcceleratorTable(ac);Scintilla_ReleaseResources();CoUninitialize();return(int)msg.wParam;
}
