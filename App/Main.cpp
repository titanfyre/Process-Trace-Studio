#include "../SDK/ProcessTrace.h"
#include "Sample.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <windowsx.h>
#include <algorithm>
#include <thread>
#include <deque>
#include <memory>
#include <sstream>
#include <iomanip>
namespace {
constexpr COLORREF Back=RGB(14,20,31),Panel=RGB(24,33,49),White=RGB(235,242,254),Muted=RGB(153,174,206),Blue=RGB(47,119,249);
constexpr UINT Update=WM_APP+1,Finished=WM_APP+2;
HWND window,list,images,paths,start,stop,save,setup;
HWND heading,subtitle,notice,totals,privacy;
HFONT font,title;HBRUSH background,field;int dpi=96;
std::unique_ptr<ProcessTrace::Client> client;std::thread worker;HANDLE cancelEvent;
std::mutex incomingLock;std::deque<PT_BATCH> incoming;
std::wstring pendingError;WPARAM epoch=0;
struct Row {PT_STATS stats;PT_EVENT event;};std::deque<Row> rows;
PT_STATS latest{};unsigned long long uiDrops=0;bool preview=false,running=false;
std::wstring message=L"Ready. Capture starts only when you choose Start.";
int S(int n){return MulDiv(n,dpi,96);}
std::wstring Widen(const char* text){int n=MultiByteToWideChar(CP_UTF8,0,text,-1,nullptr,0);std::wstring w(static_cast<size_t>(n),0);if(n)MultiByteToWideChar(CP_UTF8,0,text,-1,w.data(),n);if(!w.empty())w.pop_back();return w;}
std::wstring Hex(PT_U64 n){std::wostringstream s;s<<std::hex<<std::uppercase<<n;return s.str();}
void Buttons(){EnableWindow(start,!running&&!preview);EnableWindow(stop,running);EnableWindow(images,!running);EnableWindow(paths,!running);EnableWindow(save,!rows.empty());if(notice)SetWindowTextW(notice,message.c_str());}
void Fonts(){if(font)DeleteObject(font);if(title)DeleteObject(title);font=CreateFontW(-S(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");title=CreateFontW(-S(27),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");for(HWND h:{list,images,paths,start,stop,save,setup,subtitle,notice,totals,privacy})if(h)SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);if(heading)SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(title),TRUE);}
void Layout(){RECT r{};GetClientRect(window,&r);int w=r.right;MoveWindow(heading,S(24),S(20),w-S(48),S(42),TRUE);MoveWindow(subtitle,S(24),S(66),w-S(48),S(25),TRUE);MoveWindow(notice,S(24),S(108),w-S(48),S(27),TRUE);MoveWindow(totals,S(24),r.bottom-S(53),w-S(48),S(24),TRUE);MoveWindow(privacy,S(24),r.bottom-S(29),w-S(48),S(22),TRUE);MoveWindow(start,S(24),S(151),S(110),S(38),TRUE);MoveWindow(stop,S(144),S(151),S(100),S(38),TRUE);MoveWindow(save,w-S(302),S(151),S(126),S(38),TRUE);MoveWindow(setup,w-S(166),S(151),S(142),S(38),TRUE);MoveWindow(images,S(262),S(148),S(150),S(26),TRUE);MoveWindow(paths,S(262),S(174),S(250),S(26),TRUE);MoveWindow(list,S(24),S(220),w-S(48),r.bottom-S(284),TRUE);int widths[]{98,82,82,164,110,122};for(int i=0;i<6;++i)ListView_SetColumnWidth(list,i,S(widths[i]));ListView_SetColumnWidth(list,6,(std::max)(S(170),w-S(820)));InvalidateRect(window,nullptr,FALSE);}
void Add(const PT_BATCH& batch){
    latest=batch.stats;SendMessageW(list,WM_SETREDRAW,FALSE,0);
    for(PT_U32 i=0;i<batch.count;++i){const auto& e=batch.events[i];rows.push_back({batch.stats,e});
        if(rows.size()>500){rows.pop_front();ListView_DeleteItem(list,0);}
        double ms=batch.stats.frequency&&e.qpc>=batch.stats.start_qpc?static_cast<double>(e.qpc-batch.stats.start_qpc)*1000.0/static_cast<double>(batch.stats.frequency):0;
        std::wostringstream t;t<<std::fixed<<std::setprecision(3)<<ms;
        std::wstring cells[]{t.str(),e.type==PT_EVENT_CREATE?L"CREATE":e.type==PT_EVENT_EXIT?L"EXIT":L"IMAGE",std::to_wstring(e.pid),L"0x"+Hex(e.process_key),e.type==PT_EVENT_CREATE?std::to_wstring(e.parent_pid):L"-",e.type==PT_EVENT_EXIT?L"0x"+Hex(e.exit_status):e.flags&PT_FLAG_KERNEL_IMAGE?L"Kernel image":e.flags&PT_FLAG_TRUNCATED?L"Name truncated":L"-",ProcessTrace::Name(e)};
        LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=ListView_GetItemCount(list);item.pszText=cells[0].data();int row=ListView_InsertItem(list,&item);for(int j=1;j<7;++j)ListView_SetItemText(list,row,j,cells[j].data());
    }
    SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,FALSE);Buttons();InvalidateRect(window,nullptr,FALSE);
}
void Stop(){
    if(cancelEvent)SetEvent(cancelEvent);if(client)client->Cancel();if(worker.joinable())worker.join();
    if(client){try{latest=client->Status();client->Stop();}catch(const std::exception& e){message=Widen(e.what());}client.reset();}
    {std::lock_guard<std::mutex> guard(incomingLock);incoming.clear();pendingError.clear();}++epoch;
    if(cancelEvent){CloseHandle(cancelEvent);cancelEvent=nullptr;}running=false;Buttons();InvalidateRect(window,nullptr,FALSE);
}
void Start(){
    try{
        Stop();client=std::make_unique<ProcessTrace::Client>();client->Start(Button_GetCheck(images)==BST_CHECKED,Button_GetCheck(paths)==BST_CHECKED);
        latest=client->Status();rows.clear();ListView_DeleteAllItems(list);uiDrops=0;cancelEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!cancelEvent)throw std::runtime_error("Could not create cancellation event.");
        running=true;message=L"Capturing. Closing this window stops capture and releases the device.";Buttons();
        const WPARAM token=epoch;
        worker=std::thread([token]{
            try{while(WaitForSingleObject(cancelEvent,0)!=WAIT_OBJECT_0){auto batch=client->Receive(INFINITE,cancelEvent);if(!batch.stats.version)break;{std::lock_guard<std::mutex> guard(incomingLock);if(incoming.size()==64){uiDrops+=incoming.front().count;incoming.pop_front();}incoming.push_back(batch);}PostMessageW(window,Update,token,0);}}
            catch(const std::exception& e){{std::lock_guard<std::mutex> guard(incomingLock);pendingError=Widen(e.what());}PostMessageW(window,Finished,token,0);}
        });
    }catch(const std::exception& e){message=Widen(e.what());Stop();}
    InvalidateRect(window,nullptr,FALSE);
}
void Export(){
    wchar_t path[MAX_PATH]=L"Process-Trace.csv";OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;dialog.lpstrFile=path;dialog.nMaxFile=MAX_PATH;dialog.lpstrFilter=L"CSV report (*.csv)\0*.csv\0\0";dialog.lpstrDefExt=L"csv";dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    std::string data="session,sequence,elapsed_ms,type,pid,process_key,create_time_100ns,parent_pid,parent_key,exit_status,flags,image\r\n";
    for(const auto& r:rows){PT_BATCH b{};b.stats=r.stats;b.count=1;b.events[0]=r.event;data+=ProcessTrace::Csv(b);}
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){MessageBoxW(window,L"The report could not be created. Choose a new filename; existing files are preserved.",L"Export",MB_ICONERROR);return;}
    DWORD wrote=0;bool ok=WriteFile(file,data.data(),static_cast<DWORD>(data.size()),&wrote,nullptr)&&wrote==data.size();CloseHandle(file);
    if(!ok){DeleteFileW(path);MessageBoxW(window,L"Export failed; the incomplete report was removed.",L"Export",MB_ICONERROR);}else{message=L"CSV exported. Only the latest 500 visible events are included.";InvalidateRect(window,nullptr,FALSE);}
}
LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE:{window=h;dpi=static_cast<int>(GetDpiForWindow(h));background=CreateSolidBrush(Back);field=CreateSolidBrush(Panel);INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&controls);
        auto label=[&](const wchar_t* text){return CreateWindowW(L"STATIC",text,WS_CHILD|WS_VISIBLE|SS_LEFT|SS_ENDELLIPSIS,0,0,0,0,h,nullptr,nullptr,nullptr);};heading=label(L"Process Trace Studio");subtitle=label(L"Read-only kernel telemetry for Windows software integration and QA");notice=label(message.c_str());totals=label(L"");privacy=label(L"No command-line, environment, file-content or memory capture. Names only by default. Export is local and explicit.");
        auto button=[&](const wchar_t* text,int id){return CreateWindowW(L"BUTTON",text,WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,0,0,0,0,h,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);};
        start=button(L"Start capture",1);stop=button(L"Stop",2);save=button(L"Export CSV",3);setup=button(L"Driver setup",4);
        images=CreateWindowW(L"BUTTON",L"Image loads",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,0,0,0,0,h,nullptr,nullptr,nullptr);Button_SetCheck(images,BST_CHECKED);
        paths=CreateWindowW(L"BUTTON",L"Include full paths (sensitive)",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,0,0,0,0,h,nullptr,nullptr,nullptr);
        list=CreateWindowW(WC_LISTVIEW,L"",WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SHOWSELALWAYS,0,0,0,0,h,nullptr,nullptr,nullptr);ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);ListView_SetBkColor(list,Panel);ListView_SetTextBkColor(list,Panel);ListView_SetTextColor(list,White);
        const wchar_t* columns[]{L"Time (ms)",L"Event",L"PID",L"Process key",L"Parent PID",L"Detail / exit",L"Image name"};for(int i=0;i<7;++i){LVCOLUMNW c{};c.mask=LVCF_TEXT|LVCF_WIDTH;c.pszText=const_cast<wchar_t*>(columns[i]);c.cx=S(120);ListView_InsertColumn(list,i,&c);}Fonts();Buttons();if(preview){message=L"SAMPLE DATA - offline preview; no driver opened and no capture started.";Add(Sample());}return 0;}
    case WM_SIZE:Layout();return 0;
    case WM_GETMINMAXINFO:{auto p=reinterpret_cast<MINMAXINFO*>(l);p->ptMinTrackSize={S(1040),S(600)};return 0;}
    case WM_DPICHANGED:{dpi=HIWORD(w);auto r=reinterpret_cast<RECT*>(l);Fonts();SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);Layout();return 0;}
    case WM_COMMAND:if(LOWORD(w)==1)Start();else if(LOWORD(w)==2){message=L"Capture stopped. The driver is inactive.";Stop();}else if(LOWORD(w)==3)Export();else if(LOWORD(w)==4){wchar_t path[MAX_PATH]{};GetModuleFileNameW(nullptr,path,MAX_PATH);std::wstring location=path;auto p=location.find_last_of(L"\\/");location=location.substr(0,p+1)+L"Dependencies\\ProcessTraceSetup.exe";auto r=ShellExecuteW(h,L"runas",location.c_str(),nullptr,nullptr,SW_SHOWNORMAL);if(reinterpret_cast<INT_PTR>(r)<=32)MessageBoxW(h,L"The signed driver installer is not present in this preview. Download it from the project release after signing and VM validation.",L"Driver setup",MB_ICONINFORMATION);}return 0;
    case Update:{if(w!=epoch)return 0;std::deque<PT_BATCH> received;{std::lock_guard<std::mutex> guard(incomingLock);received.swap(incoming);}for(const auto& b:received)Add(b);return 0;}
    case Finished:{if(w!=epoch)return 0;{std::lock_guard<std::mutex> guard(incomingLock);message=pendingError;}Stop();return 0;}
    case WM_DRAWITEM:{auto d=reinterpret_cast<DRAWITEMSTRUCT*>(l);if(d->CtlType!=ODT_BUTTON)break;bool disabled=(d->itemState&ODS_DISABLED)!=0;HBRUSH brush=CreateSolidBrush(d->CtlID==1&&!disabled?Blue:Panel);HPEN pen=CreatePen(PS_SOLID,1,disabled?Panel:Blue);auto oldBrush=SelectObject(d->hDC,brush);auto oldPen=SelectObject(d->hDC,pen);RoundRect(d->hDC,d->rcItem.left,d->rcItem.top,d->rcItem.right,d->rcItem.bottom,S(10),S(10));SelectObject(d->hDC,oldBrush);SelectObject(d->hDC,oldPen);DeleteObject(brush);DeleteObject(pen);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,disabled?Muted:White);SelectObject(d->hDC,font);wchar_t label[80]{};GetWindowTextW(d->hwndItem,label,80);DrawTextW(d->hDC,label,-1,&d->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);if(d->itemState&ODS_FOCUS)DrawFocusRect(d->hDC,&d->rcItem);return TRUE;}
    case WM_SETCURSOR:if(LOWORD(l)==HTCLIENT){HWND target=reinterpret_cast<HWND>(w);if((target==start||target==stop||target==save||target==setup||target==images||target==paths)&&IsWindowEnabled(target)){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}}break;
    case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:SetBkColor(reinterpret_cast<HDC>(w),Back);SetTextColor(reinterpret_cast<HDC>(w),(reinterpret_cast<HWND>(l)==subtitle||reinterpret_cast<HWND>(l)==totals||reinterpret_cast<HWND>(l)==privacy)?Muted:White);return reinterpret_cast<LRESULT>(background);
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:case WM_PRINTCLIENT:{PAINTSTRUCT p{};HDC dc=m==WM_PAINT?BeginPaint(h,&p):reinterpret_cast<HDC>(w);RECT r{};GetClientRect(h,&r);FillRect(dc,&r,background);SetBkMode(dc,TRANSPARENT);SetWindowTextW(notice,message.c_str());
        std::wstring summary=L"Observed "+std::to_wstring(latest.received)+L"   |   Driver dropped "+std::to_wstring(latest.dropped)+L"   |   Truncated names "+std::to_wstring(latest.truncated)+L"   |   Visible "+std::to_wstring(rows.size())+L" / 500";
        {std::lock_guard<std::mutex> guard(incomingLock);if(uiDrops)summary+=L"   |   UI queue skipped "+std::to_wstring(uiDrops);}
        SetWindowTextW(totals,summary.c_str());if(m==WM_PAINT)EndPaint(h,&p);return 0;}
    case WM_DESTROY:Stop();DeleteObject(font);DeleteObject(title);DeleteObject(background);DeleteObject(field);PostQuitMessage(0);return 0;
    }return DefWindowProcW(h,m,w,l);
}
bool Render(HWND h,const wchar_t* path){
    RECT r{};GetClientRect(h,&r);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    HDC dc=GetDC(h),memory=CreateCompatibleDC(dc);void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);ReleaseDC(h,dc);if(!bitmap||!memory){if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);return false;}
    auto old=SelectObject(memory,bitmap);SendMessageW(h,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(memory),PRF_CLIENT|PRF_ERASEBKGND);
    // Print each native child into its own DC. A hidden-window preview must not
    // depend on desktop visibility or a control preserving its parent's origin.
    for(HWND child=GetWindow(h,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
        if(!(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE))continue;RECT bounds{};GetWindowRect(child,&bounds);MapWindowPoints(nullptr,h,reinterpret_cast<POINT*>(&bounds),2);int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;if(width<=0||height<=0)continue;
        HDC target=CreateCompatibleDC(memory);HBITMAP surface=CreateCompatibleBitmap(memory,width,height);if(!target||!surface){if(target)DeleteDC(target);if(surface)DeleteObject(surface);continue;}auto previous=SelectObject(target,surface);SendMessageW(child,WM_PRINT,reinterpret_cast<WPARAM>(target),PRF_CLIENT|PRF_NONCLIENT|PRF_CHILDREN|PRF_ERASEBKGND);BitBlt(memory,bounds.left,bounds.top,width,height,target,0,0,SRCCOPY);SelectObject(target,previous);DeleteObject(surface);DeleteDC(target);
    }GdiFlush();
    DWORD bytes=static_cast<DWORD>(r.right*r.bottom*4);BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+bytes;
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);bool ok=false;
    if(file!=INVALID_HANDLE_VALUE){DWORD n=0;ok=WriteFile(file,&header,sizeof(header),&n,nullptr)&&n==sizeof(header)&&WriteFile(file,&info.bmiHeader,sizeof(BITMAPINFOHEADER),&n,nullptr)&&n==sizeof(BITMAPINFOHEADER)&&WriteFile(file,pixels,bytes,&n,nullptr)&&n==bytes;CloseHandle(file);if(!ok)DeleteFileW(path);}
    SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);return ok;
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int show){
    UNREFERENCED_PARAMETER(command);int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);std::wstring image;
    if(args){preview=count==2&&std::wstring(args[1])==L"--preview";if(count==3&&std::wstring(args[1])==L"--render-preview"){preview=true;image=args[2];}LocalFree(args);}
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=Proc;wc.lpszClassName=L"TitanProcessTraceStudio";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&wc);
    HWND h=CreateWindowW(wc.lpszClassName,L"Process Trace Studio",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1180,720,nullptr,nullptr,instance,nullptr);if(!h)return 1;Layout();if(!image.empty()){bool ok=Render(h,image.c_str());DestroyWindow(h);return ok?0:1;}ShowWindow(h,show);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return static_cast<int>(msg.wParam);
}
