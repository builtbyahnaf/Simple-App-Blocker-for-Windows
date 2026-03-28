#include <windows.h>
#include <commdlg.h>
#include <vector>
#include <fstream>
#include <ctime>
#include <string>
#include <uxtheme.h>
#include <commctrl.h>
#include <shellapi.h>

#pragma comment(lib,"uxtheme.lib")
#pragma comment(linker,\
"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' \
version='6.0.0.0' \
processorArchitecture='*' \
publicKeyToken='6595b64144ccf1df' \
language='*'\"")
#pragma comment(lib, "shell32.lib")

#define BTN_SELECT 1
#define BTN_BLOCK 2
#define BTN_UNBLOCK 3
#define LIST_BOX 4


/*

Simple App Blocker for Windows

Version 3.42

Used: pure C++
Author: Muhammad Tahmid Ahnaf + ChatGPT

*/

HWND listBox;
HWND daysInput;
HWND chooseButton;
HWND blockButton;
HWND unblockButton;
HWND daysText;
HWND instrBox;


std::string selectedExe="";

std::string baseKey="SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\";

struct BlockItem{
    std::string exe;
    long long expire;
};

std::vector<BlockItem> blocks;

long long now(){
    return time(NULL);
}

bool IsRunningAsAdmin()
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;

    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(
        &NtAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0,0,0,0,0,0,
        &adminGroup))
    {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin == TRUE;
}
void ShowAdminWarning()
{
    MessageBoxW(
        NULL,
        L"The Blocker App is not running with administrator privileges.\n\n"
        L"Without administrator access, blocking and unblocking actions may fail.\n\n"
        L"To fix this:\n"
        L"Right-click the application file and select Run as administrator.\n",
        L"Administrator Privileges Required",
        MB_OK | MB_ICONWARNING | MB_SYSTEMMODAL
    );
}

bool blockRegistry(std::string exe){
    HKEY hKey;
    std::string path=baseKey+exe;

    if(RegCreateKeyExA(HKEY_LOCAL_MACHINE,path.c_str(),0,NULL,0,KEY_SET_VALUE,NULL,&hKey,NULL)!=ERROR_SUCCESS)
        return false;

    const char* debugger="blocked";
    RegSetValueExA(hKey,"Debugger",0,REG_SZ,(BYTE*)debugger,strlen(debugger)+1);

    RegCloseKey(hKey);
    return true;
}

/*
void unblockRegistry(std::string exe){
    std::string path=baseKey+exe;
    RegDeleteTreeA(HKEY_LOCAL_MACHINE,path.c_str());
}
*/

void unblockRegistry(std::string exe)
{
    std::string path = baseKey + exe;

    // 1️⃣ Try deleting entire key
    //LONG res = RegDeleteTreeA(HKEY_LOCAL_MACHINE, path.c_str());

    HKEY hKey;

    LONG res = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        path.c_str(),
        0,
        KEY_ALL_ACCESS,
        &hKey
    );

    if(res == ERROR_SUCCESS)
    {
        RegDeleteValueA(hKey,"Debugger");   // remove debugger
        RegCloseKey(hKey);
    }

    // 2️⃣ If deletion failed, try clearing Debugger value
    
    res = RegOpenKeyExA(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_SET_VALUE, &hKey);

    if(res == ERROR_SUCCESS)
    {
        const char* val = "";
        RegSetValueExA(
            hKey,
            "Debugger",
            0,
            REG_SZ,
            (const BYTE*)val,
            /* strlen(val) + 1 */ 0
        );

        RegCloseKey(hKey);
        return;
    }

    // 3️⃣ If everything fails
    std::string msg = "Error unblocking " + exe +
    ".\nPlease unblock it manually as per instructions.";

    MessageBoxA(NULL, msg.c_str(), "Unblock Failed", MB_ICONERROR);
}

/* Saving logic from v1: save in that specific exe's dir (user won't find)
void save(){
    std::ofstream f("blocks.txt");
    for(auto &b:blocks)
        f<<b.exe<<" "<<b.expire<<"\n";
}

void load(){
    blocks.clear();
    std::ifstream f("blocks.txt");

    std::string exe;
    long long exp;

    while(f>>exe>>exp){
        if(exp>now())
            blocks.push_back({exe,exp});
        else
            unblockRegistry(exe);
    }
}
*/
std::string getExeDir()
{
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);

    std::string path(buffer);
    return path.substr(0, path.find_last_of("\\/"));
}
void save()
{
    std::ofstream f(getExeDir() + "\\blocks.txt");

    for(auto &b:blocks)
        f << b.exe << " " << b.expire << "\n";
}
void load()
{
    blocks.clear();

    std::ifstream f(getExeDir() + "\\blocks.txt");

    std::string exe;
    long long exp;

    while(f >> exe >> exp)
    {
        if(exp > now())
            blocks.push_back({exe,exp});
        else
            unblockRegistry(exe);
    }
}


void refreshList(){
    SendMessage(listBox, LB_RESETCONTENT, 0, 0);

    for(auto &b : blocks){
        long remaining = b.expire - now();
        if(remaining < 0) remaining = 0;

        int days = remaining / 86400;
        remaining %= 86400;

        int hours = remaining / 3600;
        remaining %= 3600;

        int minutes = remaining / 60;

        int seconds = remaining % 60;

        std::string timeStr = "";

        if(days > 0)
            timeStr += std::to_string(days) + " day" + (days > 1 ? "s" : "");

        if(hours > 0){
            if(!timeStr.empty()) timeStr += ", ";
            timeStr += std::to_string(hours) + " hour" + (hours > 1 ? "s" : "");
        }

        if(minutes > 0){
            if(!timeStr.empty()) timeStr += ", ";
            timeStr += std::to_string(minutes) + " minute" + (minutes > 1 ? "s" : "");
        }
        if(seconds > 0){
            if(!timeStr.empty()) timeStr += ", ";
            timeStr += std::to_string(seconds) + " second" + (seconds > 1 ? "s" : "");
        }

        if(timeStr.empty())
            timeStr = "less than a second, will be unblocked right away";

        std::string s = b.exe + "  (expires in " + timeStr + ")";

        SendMessageA(listBox, LB_ADDSTRING, 0, (LPARAM)s.c_str());
    }
}

std::string filename(std::string path){
    size_t p=path.find_last_of("\\/");
    return path.substr(p+1);
}

void chooseExe(HWND hwnd){

    char file[MAX_PATH]="";

    OPENFILENAMEA ofn={0};
    ofn.lStructSize=sizeof(ofn);
    ofn.lpstrFilter="Exe\0*.exe\0";
    ofn.lpstrFile=file;
    ofn.nMaxFile=MAX_PATH;
    ofn.Flags=OFN_FILEMUSTEXIST;

    if(GetOpenFileNameA(&ofn)){
        selectedExe=filename(file);
        MessageBoxA(hwnd,selectedExe.c_str(),"Selected",0);
    }
}

void blockDays(){

    if(selectedExe=="") return;

    char buf[10];
    GetWindowTextA(daysInput,buf,10);

    int days=atoi(buf);
    if(days<=0) return;

    long long exp=now()+days*86400;

    blockRegistry(selectedExe);

    blocks.push_back({selectedExe,exp});

    save();
}

void checkExpired(){

    for(int i=0;i<blocks.size();){

        if(blocks[i].expire<=now()){
            unblockRegistry(blocks[i].exe);
            blocks.erase(blocks.begin()+i);
        }
        else i++;
    }

    save();
}

void unblockSelected(){

    int sel=SendMessage(listBox,LB_GETCURSEL,0,0);
    if(sel==LB_ERR) return;

    unblockRegistry(blocks[sel].exe);

    blocks.erase(blocks.begin()+sel);

    save();
}

HFONT LoadCustomFont(const char* fontName, int sizePx)
{
    return CreateFontA(
        sizePx, 0, 0,0, FW_NORMAL,
        FALSE,FALSE,FALSE,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        fontName
    );
}
bool isSystemDarkMode()
{
    DWORD value=0, size=sizeof(value);
    if(RegGetValueA(HKEY_CURRENT_USER,
                    "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                    "AppsUseLightTheme",
                    RRF_RT_REG_DWORD,
                    NULL, &value, &size) == ERROR_SUCCESS)
        return value == 0; // 0 = dark, 1 = light
    return false; // fallback
}
COLORREF getAccentColor()
{
    DWORD color=0;
    DWORD size=sizeof(color);

    if(RegGetValueA(
        HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\DWM",
        "ColorizationColor",
        RRF_RT_REG_DWORD,
        NULL,
        &color,
        &size)==ERROR_SUCCESS)
    {
        return RGB(
            (color>>16)&0xff,
            (color>>8)&0xff,
            color&0xff
        );
    }

    return RGB(0,120,215); // fallback
}


LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam){
    
    const char* unblockText =
"To unblock manually:\r\n"
"1) Go to registry editor.\r\n"
"2) Navigate to:\r\n"
"Computer\\HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\\r\n"
"3) Select your .exe file (be careful selecting the correct one).\r\n"
"4) Select 'Debugger' and delete the value 'blocked'. Or, open that and clear 'blocked' to ''.\r\n"
"5) Refresh. If it still fails, something else is blocking the app.";
COLORREF bg = isSystemDarkMode() ? RGB(30,30,30) : RGB(245,245,245);
COLORREF fg = isSystemDarkMode() ? RGB(220,220,220) : RGB(0,0,0);

        
        int hoveredButton = -1;
    switch(msg){

        case WM_CREATE:
        {
            HFONT font = LoadCustomFont("Outfit", 18);

            
        chooseButton = CreateWindowA("BUTTON","Choose EXE",WS_VISIBLE|WS_CHILD|BS_OWNERDRAW,
        20,20,120,30,hwnd,(HMENU)BTN_SELECT,NULL,NULL);

        daysText = CreateWindowA("STATIC","Days:",WS_VISIBLE|WS_CHILD,
        160,25,40,20,hwnd,NULL,NULL,NULL);

        daysInput=CreateWindowA("EDIT","1",
        WS_VISIBLE|WS_CHILD|WS_BORDER,
        200,20,40,25,hwnd,NULL,NULL,NULL);

        blockButton = CreateWindowA("BUTTON","Block",
        WS_VISIBLE|WS_CHILD|BS_OWNERDRAW,
        260,20,80,30,hwnd,(HMENU)BTN_BLOCK,NULL,NULL);

        unblockButton = CreateWindowA("BUTTON","Unblock",
        WS_VISIBLE|WS_CHILD|BS_OWNERDRAW,
        350,20,80,30,hwnd,(HMENU)BTN_UNBLOCK,NULL,NULL);

        listBox=CreateWindowA("LISTBOX","",
        WS_VISIBLE|WS_CHILD|WS_BORDER|LBS_STANDARD,
        20,70,480,220,hwnd,(HMENU)LIST_BOX,NULL,NULL);


        SendMessage(listBox, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(daysInput, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(chooseButton, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(blockButton, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(unblockButton, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(daysText, WM_SETFONT, (WPARAM)font, TRUE);

        //CreateWindowA("STATIC", unblockText, WS_VISIBLE | WS_CHILD | SS_LEFT, 20,400,410,140, hwnd,NULL,NULL,NULL);
        instrBox = CreateWindowA(
        "EDIT",
        unblockText,
        WS_VISIBLE | WS_CHILD | WS_BORDER |
        ES_MULTILINE | ES_READONLY
        /* WS_VSCROLL | ES_AUTOVSCROLL */,
        20,300,480,250,
        hwnd,NULL,NULL,NULL
        );
        SendMessage(instrBox, WM_SETFONT, (WPARAM)font, TRUE);

        load();
        refreshList();

        SetTimer(hwnd,1,1000,NULL);

        break;

        }
        case WM_SIZE:{

             int winWidth  = LOWORD(lParam); // new width
    int winHeight = HIWORD(lParam); // new height

    // reposition your controls based on window size
    MoveWindow(listBox, 20, 70, winWidth - 40, winHeight - 380, TRUE);
    MoveWindow(daysInput, 200, 20, 50, 25, TRUE); // static width OK
    MoveWindow(instrBox, 20, winHeight - 260, winWidth - 40, 250, TRUE);

    // Buttons can move proportionally or stay fixed
    MoveWindow(chooseButton, 20, 20, 120, 30, TRUE);
    MoveWindow(blockButton, 260, 20, 80, 30, TRUE);
    MoveWindow(unblockButton, 350, 20, 80, 30, TRUE);
            InvalidateRect(hwnd, NULL, TRUE); // repaint the entire window
    UpdateWindow(hwnd);
    break;
        }
    
        case WM_MOUSEMOVE:
{
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(hwnd,&p);
    

    HWND ctrl = ChildWindowFromPoint(hwnd,p);

    if(ctrl)
    {
        int id = GetDlgCtrlID(ctrl);
        if(id==BTN_BLOCK || id==BTN_UNBLOCK)
        {
            hoveredButton = id;
            InvalidateRect(ctrl,NULL,TRUE);
        }
        else hoveredButton = -1;
    }

    break;
}

        
        case WM_TIMER:{
            if(wParam == 1)  // our timer ID
    {
        checkExpired();   // remove expired blocks
        refreshList();    // update countdown display
    }
    break;
        }
        case WM_CTLCOLOREDIT:
case WM_CTLCOLORSTATIC:
{
    /*
    HDC hdc = (HDC)wParam;
    SetTextColor(hdc, fg);
    SetBkColor(hdc, bg);
    static HBRUSH hbr = CreateSolidBrush(bg);
    return (INT_PTR)hbr;
    */
    HDC hdcCommon = (HDC)wParam;
    SetBkMode(hdcCommon, TRANSPARENT);
    SetBkColor(hdcCommon, RGB(255,255,255));
    SetTextColor(hdcCommon, RGB(0,0,0));
    return (INT_PTR)GetStockObject(WHITE_BRUSH);
    
}
case WM_ERASEBKGND:
    return 1;
case WM_DRAWITEM:
{
    LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;

    if(dis->CtlType == ODT_BUTTON)
    {
        COLORREF accent = getAccentColor();
        COLORREF bg = RGB(240,240,240);

        if(dis->CtlID == hoveredButton)
            bg = accent;

        HBRUSH brush = CreateSolidBrush(bg);

        HPEN pen = CreatePen(PS_SOLID,1,accent);
        SelectObject(dis->hDC,pen);
        SelectObject(dis->hDC,brush);

        RoundRect(
            dis->hDC,
            dis->rcItem.left,
            dis->rcItem.top,
            dis->rcItem.right,
            dis->rcItem.bottom,
            10,10
        );

        SetBkMode(dis->hDC,TRANSPARENT);
        SetTextColor(dis->hDC,RGB(0,0,0));

        char text[64];
        GetWindowTextA(dis->hwndItem,text,64);

        DrawTextA(
            dis->hDC,
            text,
            -1,
            &dis->rcItem,
            DT_CENTER|DT_VCENTER|DT_SINGLELINE
        );

        DeleteObject(brush);
        DeleteObject(pen);

        return TRUE;
    }
    if(dis->itemState & ODS_SELECTED)
{
    OffsetRect(&dis->rcItem,1,1);
}

    break;
}

        case WM_COMMAND:

        switch(LOWORD(wParam)){

            case BTN_SELECT:
            chooseExe(hwnd);
            break;

            case BTN_BLOCK:
            blockDays();
            refreshList();
            break;

            case BTN_UNBLOCK:
            unblockSelected();
            refreshList();
            break;
        }

        break;

        case WM_DESTROY:
        save();
    PostQuitMessage(1);
    break;
    }

    return DefWindowProc(hwnd,msg,wParam,lParam);
}

int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE,LPSTR,int nCmdShow){

    if (!IsRunningAsAdmin())
    {
        ShowAdminWarning();
        ShellExecuteW(
    NULL,
    L"runas",
    L"b.exe",
    NULL,
    NULL,
    SW_SHOWNORMAL
);
        return 0;
    }

    WNDCLASSA wc={0};
    wc.lpfnWndProc=WndProc;
    wc.hInstance=hInstance;
    wc.lpszClassName="Blocker";

    RegisterClassA(&wc);

    INITCOMMONCONTROLSEX icc{};
icc.dwSize = sizeof(icc);
icc.dwICC = ICC_STANDARD_CLASSES;
InitCommonControlsEx(&icc);

    HWND hwnd=CreateWindowA("Blocker","App Blocker",
    WS_OVERLAPPEDWINDOW,
    520,80, //x, y pos
    540,600, //width, height
    NULL,NULL,hInstance,NULL);

    ShowWindow(hwnd,nCmdShow);

    MSG msg;

    while(GetMessage(&msg,NULL,0,0)){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}