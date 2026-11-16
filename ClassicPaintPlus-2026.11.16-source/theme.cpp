#include <list>
#include <set>
#include <windows.h>
#include "precomp.h"
#include <uxtheme.h>
#include <richedit.h>
#undef GetSysColor
#undef GetSysColorBrush
#undef DrawEdge
#undef DrawFrameControl

COLORREF PaintSysColor(int index)
{
    if(!registrySettings.DarkTheme) return ::GetSysColor(index);
    switch(index)
    {
    case COLOR_APPWORKSPACE:return RGB(38,38,38);
    case COLOR_WINDOW:return RGB(45,45,45);
    case COLOR_MENU:case COLOR_MENUBAR:return RGB(66,66,66);
    case COLOR_BTNFACE:return RGB(83,83,83);
    case COLOR_3DSHADOW:case COLOR_WINDOWFRAME:return RGB(48,48,48);
    case COLOR_3DDKSHADOW:return RGB(32,32,32);
    case COLOR_3DHILIGHT:case COLOR_3DLIGHT:return RGB(110,110,110);
    case COLOR_HIGHLIGHT:return RGB(110,110,110);
    case COLOR_GRAYTEXT:return RGB(150,150,150);
    case COLOR_BTNTEXT:case COLOR_WINDOWTEXT:case COLOR_MENUTEXT:case COLOR_HIGHLIGHTTEXT:return RGB(230,230,230);
    case COLOR_INFOBK:return RGB(55,55,55);
    case COLOR_INFOTEXT:return RGB(235,235,235);
    case COLOR_SCROLLBAR:return RGB(58,58,58);
    default:return ::GetSysColor(index);
    }
}
HBRUSH PaintSysColorBrush(int index)
{
    if(!registrySettings.DarkTheme) return ::GetSysColorBrush(index);
    static HBRUSH brushes[32]={};if(index<0 || index>=32) return ::GetSysColorBrush(index);
    if(!brushes[index]) brushes[index]=CreateSolidBrush(PaintSysColor(index));return brushes[index];
}
BOOL PaintDrawEdge(HDC dc,LPRECT r,UINT edge,UINT flags)
{
    if(!registrySettings.DarkTheme) return ::DrawEdge(dc,r,edge,flags);
    FrameRect(dc,r,PaintSysColorBrush(edge&BDR_SUNKENOUTER?COLOR_3DDKSHADOW:COLOR_3DHILIGHT));
    if(flags&BF_MIDDLE) { RECT inside=*r;InflateRect(&inside,-1,-1);FillRect(dc,&inside,PaintSysColorBrush(COLOR_BTNFACE)); }
    if(flags&BF_ADJUST) InflateRect(r,-1,-1);return TRUE;
}
BOOL PaintDrawFrameControl(HDC dc,LPRECT r,UINT type,UINT state)
{
    if(!registrySettings.DarkTheme || type!=DFC_BUTTON) return ::DrawFrameControl(dc,r,type,state);
    FillRect(dc,r,PaintSysColorBrush(state&DFCS_PUSHED?COLOR_3DSHADOW:COLOR_BTNFACE));
    FrameRect(dc,r,PaintSysColorBrush(state&DFCS_PUSHED?COLOR_3DHILIGHT:COLOR_3DDKSHADOW));return TRUE;
}
namespace {
const UINT WM_THEME_APPLY=WM_APP+117;
struct MenuItem { HMENU menu;UINT position,id;CStringW label;bool bar,separator,submenu; };
std::list<MenuItem> menuItems;std::set<DWORD_PTR> menuPointers;
void ApplyWindow(HWND window);
HFONT CreateMenuFont()
{
    NONCLIENTMETRICSW metrics={sizeof(metrics)};
    if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0)) return CreateFontIndirectW(&metrics.lfMenuFont);
    LOGFONTW fallback={};GetObjectW(GetStockObject(DEFAULT_GUI_FONT),sizeof(fallback),&fallback);return CreateFontIndirectW(&fallback);
}
void DrawButton(HWND window,HDC dc)
{
    RECT r;GetClientRect(window,&r);DWORD style=GetWindowLongPtrW(window,GWL_STYLE),type=style&BS_TYPEMASK;
    FillRect(dc,&r,PaintSysColorBrush(COLOR_BTNFACE));CStringW label;int length=GetWindowTextLengthW(window);GetWindowTextW(window,label.GetBuffer(length+1),length+1);label.ReleaseBuffer();
    HFONT font=reinterpret_cast<HFONT>(SendMessageW(window,WM_GETFONT,0,0));HGDIOBJ old=SelectObject(dc,font?font:GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,PaintSysColor(IsWindowEnabled(window)?COLOR_BTNTEXT:COLOR_GRAYTEXT));
    LRESULT state=SendMessageW(window,BM_GETSTATE,0,0);UINT flags=DT_SINGLELINE|DT_VCENTER;
    if(type==BS_GROUPBOX)
    { RECT frame=r;frame.top+=7;FrameRect(dc,&frame,PaintSysColorBrush(COLOR_3DHILIGHT));r.left+=8;r.bottom=r.top+16;SIZE size;GetTextExtentPoint32W(dc,label,label.GetLength(),&size);r.right=r.left+size.cx+4;FillRect(dc,&r,PaintSysColorBrush(COLOR_BTNFACE));DrawTextW(dc,label,-1,&r,DT_LEFT|DT_SINGLELINE); }
    else if(type==BS_CHECKBOX || type==BS_AUTOCHECKBOX || type==BS_RADIOBUTTON || type==BS_AUTORADIOBUTTON || type==BS_AUTO3STATE || type==BS_3STATE)
    {
        int side=min(14,r.bottom-r.top);RECT mark={r.left,(r.bottom-side)/2,r.left+side,(r.bottom+side)/2};
        HPEN pen=CreatePen(PS_SOLID,1,PaintSysColor(COLOR_BTNTEXT));HGDIOBJ prev=SelectObject(dc,pen),brush=SelectObject(dc,PaintSysColorBrush(COLOR_WINDOW));
        bool radio=type==BS_RADIOBUTTON || type==BS_AUTORADIOBUTTON;
        if(radio) Ellipse(dc,mark.left,mark.top,mark.right,mark.bottom);else Rectangle(dc,mark.left,mark.top,mark.right,mark.bottom);
        if(SendMessageW(window,BM_GETCHECK,0,0))
        { if(radio) { SelectObject(dc,PaintSysColorBrush(COLOR_BTNTEXT));Ellipse(dc,mark.left+4,mark.top+4,mark.right-4,mark.bottom-4); }
          else { MoveToEx(dc,mark.left+3,mark.top+7,NULL);LineTo(dc,mark.left+6,mark.top+10);LineTo(dc,mark.right-2,mark.top+3); } }
        SelectObject(dc,brush);SelectObject(dc,prev);DeleteObject(pen);r.left+=side+5;DrawTextW(dc,label,-1,&r,flags|DT_LEFT);
    }
    else
    { PaintDrawFrameControl(dc,&r,DFC_BUTTON,DFCS_BUTTONPUSH|(state&BST_PUSHED?DFCS_PUSHED:0));DrawTextW(dc,label,-1,&r,flags|DT_CENTER); }
    if(GetFocus()==window && type!=BS_GROUPBOX) { InflateRect(&r,-3,-3);DrawFocusRect(dc,&r); }
    SelectObject(dc,old);
}
LRESULT CALLBACK ThemeProc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR)
{
    if(message==WM_THEME_APPLY) { ApplyWindow(window);return 0; }
    if(message==WM_NCDESTROY) { RemoveWindowSubclass(window,ThemeProc,817);return DefSubclassProc(window,message,wp,lp); }
    // Once themed, menus keep their renderer in both themes. Switching their
    // native/owner-drawn type can leave Windows' cached popup text invisible.
    if(!registrySettings.DarkTheme && message!=WM_INITMENUPOPUP && message!=WM_MEASUREITEM && message!=WM_DRAWITEM)
        return DefSubclassProc(window,message,wp,lp);
    if(message==WM_INITMENUPOPUP)
    {
        // The main handler rebuilds Recent Files. Theme the finished menu so
        // freshly inserted entries use the same text colour as the other items.
        LRESULT result=DefSubclassProc(window,message,wp,lp);
        ThemeMenu(reinterpret_cast<HMENU>(wp));return result;
    }
    if(message>=WM_CTLCOLORMSGBOX && message<=WM_CTLCOLORSTATIC)
    {
        HDC dc=reinterpret_cast<HDC>(wp);bool edit=message==WM_CTLCOLOREDIT || message==WM_CTLCOLORLISTBOX;
        if(HWND(lp)==textEditWindow.m_hWnd) return DefSubclassProc(window,message,wp,lp);
        SetTextColor(dc,PaintSysColor(COLOR_WINDOWTEXT));SetBkColor(dc,PaintSysColor(edit?COLOR_WINDOW:COLOR_BTNFACE));
        return reinterpret_cast<LRESULT>(PaintSysColorBrush(edit?COLOR_WINDOW:COLOR_BTNFACE));
    }
    if(message==WM_MEASUREITEM)
    {
        auto item=reinterpret_cast<MEASUREITEMSTRUCT*>(lp);
        if(item && item->CtlType==ODT_MENU && menuPointers.count(item->itemData))
        {
            auto data=reinterpret_cast<MenuItem*>(item->itemData);HDC dc=GetDC(window);
            HFONT font=CreateMenuFont();HGDIOBJ old=SelectObject(dc,font);RECT text={};
            DrawTextW(dc,data->label,-1,&text,DT_CALCRECT|DT_SINGLELINE);
            // Windows adds the menu-bar gutters itself; adding them here again
            // shifts each following caption by another 12 pixels.
            item->itemWidth=text.right+(data->bar?0:56);item->itemHeight=data->separator?7:data->bar?GetSystemMetrics(SM_CYMENU):max(22,text.bottom+6);
            SelectObject(dc,old);DeleteObject(font);ReleaseDC(window,dc);return TRUE;
        }
    }
    if(message==WM_DRAWITEM)
    {
        auto item=reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if(item && item->CtlType==ODT_MENU && menuPointers.count(item->itemData))
        {
            auto data=reinterpret_cast<MenuItem*>(item->itemData);RECT r=item->rcItem;
            FillRect(item->hDC,&r,PaintSysColorBrush(item->itemState&ODS_SELECTED?COLOR_HIGHLIGHT:COLOR_MENU));
            if(data->separator) { r.top=(r.top+r.bottom)/2;r.bottom=r.top+1;r.left+=22;FillRect(item->hDC,&r,PaintSysColorBrush(COLOR_3DHILIGHT));return TRUE; }
            HFONT font=CreateMenuFont();HGDIOBJ old=SelectObject(item->hDC,font);
            SetTextColor(item->hDC,PaintSysColor(item->itemState&ODS_DISABLED?COLOR_GRAYTEXT:COLOR_MENUTEXT));SetBkMode(item->hDC,TRANSPARENT);
            if(item->itemState&ODS_CHECKED) { RECT check=r;check.right=check.left+22;DrawTextW(item->hDC,L"✓",-1,&check,DT_CENTER|DT_VCENTER|DT_SINGLELINE); }
            r.left+=data->bar?6:27;r.right-=data->bar?6:22;
            int tab=data->label.Find(L'\t');CStringW left=tab<0?data->label:data->label.Left(tab);
            DrawTextW(item->hDC,left,-1,&r,DT_LEFT|DT_SINGLELINE|DT_VCENTER);
            if(tab>=0) DrawTextW(item->hDC,data->label.Mid(tab+1),-1,&r,DT_RIGHT|DT_SINGLELINE|DT_VCENTER);
            if(data->submenu && !data->bar) { r=item->rcItem;r.left=r.right-18;DrawTextW(item->hDC,L"›",-1,&r,DT_CENTER|DT_SINGLELINE|DT_VCENTER); }
            SelectObject(item->hDC,old);DeleteObject(font);return TRUE;
        }
    }
    WCHAR cls[80];GetClassNameW(window,cls,_countof(cls));
    if(message==WM_ERASEBKGND && (GetClassLongPtrW(window,GCLP_HBRBACKGROUND)==COLOR_BTNFACE+1 || !lstrcmpW(cls,L"#32770") || !lstrcmpW(cls,L"ToolBox") || !lstrcmpW(cls,L"MSPaintApp") || !lstrcmpW(cls,TOOLBARCLASSNAMEW)))
    { RECT r;GetClientRect(window,&r);FillRect(HDC(wp),&r,PaintSysColorBrush(COLOR_BTNFACE));return TRUE; }
    if(!lstrcmpiW(cls,L"Button") && (GetWindowLongPtrW(window,GWL_STYLE)&BS_TYPEMASK)!=BS_OWNERDRAW && (message==WM_PAINT || message==WM_PRINTCLIENT))
    { PAINTSTRUCT ps;HDC dc=message==WM_PAINT?BeginPaint(window,&ps):HDC(wp);DrawButton(window,dc);if(message==WM_PAINT) EndPaint(window,&ps);return 0; }
    if((!lstrcmpiW(cls,L"ComboBox") || !lstrcmpiW(cls,UPDOWN_CLASSW)) && (message==WM_PAINT || message==WM_PRINTCLIENT))
    {
        PAINTSTRUCT ps;HDC dc=message==WM_PAINT?BeginPaint(window,&ps):HDC(wp);RECT r;GetClientRect(window,&r);
        FillRect(dc,&r,PaintSysColorBrush(COLOR_WINDOW));FrameRect(dc,&r,PaintSysColorBrush(COLOR_3DHILIGHT));
        HPEN pen=CreatePen(PS_SOLID,1,PaintSysColor(COLOR_BTNTEXT));HGDIOBJ oldPen=SelectObject(dc,pen),oldBrush=SelectObject(dc,PaintSysColorBrush(COLOR_BTNTEXT));
        if(!lstrcmpiW(cls,L"ComboBox"))
        {
            COMBOBOXINFO info={sizeof(info)};GetComboBoxInfo(window,&info);RECT arrow=info.rcButton;FillRect(dc,&arrow,PaintSysColorBrush(COLOR_BTNFACE));
            int x=(arrow.left+arrow.right)/2,y=(arrow.top+arrow.bottom)/2;POINT points[]={{x-3,y-1},{x+3,y-1},{x,y+2}};Polygon(dc,points,3);
            if((GetWindowLongPtrW(window,GWL_STYLE)&3)==CBS_DROPDOWNLIST)
            {
                WCHAR label[512]={};GetWindowTextW(window,label,_countof(label));RECT text=info.rcItem;text.left+=3;
                HFONT font=HFONT(SendMessageW(window,WM_GETFONT,0,0));HGDIOBJ old=SelectObject(dc,font?font:GetStockObject(DEFAULT_GUI_FONT));
                SetBkMode(dc,TRANSPARENT);SetTextColor(dc,PaintSysColor(IsWindowEnabled(window)?COLOR_WINDOWTEXT:COLOR_GRAYTEXT));DrawTextW(dc,label,-1,&text,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
                SelectObject(dc,old);if(GetFocus()==window) { InflateRect(&text,-1,-1);DrawFocusRect(dc,&text); }
            }
        }
        else
        {
            int cx=(r.left+r.right)/2,cy=(r.top+r.bottom)/2;MoveToEx(dc,r.left,cy,NULL);LineTo(dc,r.right,cy);
            POINT up[]={{cx-3,cy/2+1},{cx+3,cy/2+1},{cx,cy/2-2}},down[]={{cx-3,cy+cy/2-1},{cx+3,cy+cy/2-1},{cx,cy+cy/2+2}};
            Polygon(dc,up,3);Polygon(dc,down,3);
        }
        SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(pen);if(message==WM_PAINT) EndPaint(window,&ps);return 0;
    }
    if((message==WM_PAINT || message==WM_PRINTCLIENT) && !lstrcmpW(cls,TOOLBARCLASSNAMEW))
    {
        PAINTSTRUCT ps;HDC dc=message==WM_PAINT?BeginPaint(window,&ps):HDC(wp);RECT client;GetClientRect(window,&client);FillRect(dc,&client,PaintSysColorBrush(COLOR_BTNFACE));
        HIMAGELIST images=HIMAGELIST(SendMessageW(window,TB_GETIMAGELIST,0,0));int iw=16,ih=16;ImageList_GetIconSize(images,&iw,&ih);
        int hot=int(SendMessageW(window,TB_GETHOTITEM,0,0));for(int i=0;i<SendMessageW(window,TB_BUTTONCOUNT,0,0);++i)
        { TBBUTTON button={};SendMessageW(window,TB_GETBUTTON,i,LPARAM(&button));RECT r;SendMessageW(window,TB_GETITEMRECT,i,LPARAM(&r));if(button.fsStyle&TBSTYLE_SEP) continue;
          if(button.fsState&(TBSTATE_CHECKED|TBSTATE_PRESSED) || hot==i) { FillRect(dc,&r,PaintSysColorBrush(button.fsState&TBSTATE_CHECKED?COLOR_3DSHADOW:COLOR_HIGHLIGHT));FrameRect(dc,&r,PaintSysColorBrush(COLOR_3DHILIGHT)); }
          ImageList_Draw(images,button.iBitmap,dc,(r.left+r.right-iw)/2,(r.top+r.bottom-ih)/2,ILD_TRANSPARENT); }
        if(message==WM_PAINT) EndPaint(window,&ps);return 0;
    }
    if((message==WM_PAINT || message==WM_PRINTCLIENT) && !lstrcmpW(cls,STATUSCLASSNAMEW))
    {
        PAINTSTRUCT ps;HDC dc=message==WM_PAINT?BeginPaint(window,&ps):HDC(wp);RECT r;GetClientRect(window,&r);FillRect(dc,&r,PaintSysColorBrush(COLOR_BTNFACE));
        HGDIOBJ old=SelectObject(dc,GetStockObject(DEFAULT_GUI_FONT));SetTextColor(dc,PaintSysColor(COLOR_BTNTEXT));SetBkMode(dc,TRANSPARENT);
        int count=int(SendMessageW(window,SB_GETPARTS,0,0));for(int i=0;i<count;++i) { SendMessageW(window,SB_GETRECT,i,LPARAM(&r));WCHAR text[512]={};int length=LOWORD(SendMessageW(window,SB_GETTEXTLENGTH,i,0));if(length<512) SendMessageW(window,SB_GETTEXT,i,LPARAM(text));r.left+=4;DrawTextW(dc,text,-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX); }
        SelectObject(dc,old);if(message==WM_PAINT) EndPaint(window,&ps);return 0;
    }
    return DefSubclassProc(window,message,wp,lp);
}
void ApplyWindow(HWND window)
{
    static bool applying=false;if(applying || !IsWindow(window)) return;applying=true;
    WCHAR cls[80];GetClassNameW(window,cls,_countof(cls));
    // Text pixels belong to the document, and are never recolored by the UI theme.
    if(!lstrcmpiW(cls,MSFTEDIT_CLASS)) { applying=false;return; }
    SetWindowSubclass(window,ThemeProc,817,0);
    bool dark=registrySettings.DarkTheme!=0;
    SetWindowTheme(window,dark?L"DarkMode_Explorer":NULL,NULL);
    if(!(GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD))
    {
        static HMODULE module=LoadLibraryExW(L"dwmapi.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
        typedef HRESULT(WINAPI *DwmAttribute)(HWND,DWORD,LPCVOID,DWORD);
        auto set=module?reinterpret_cast<DwmAttribute>(GetProcAddress(module,"DwmSetWindowAttribute")):NULL;
        BOOL value=dark;if(set) { if(FAILED(set(window,20,&value,sizeof(value)))) set(window,19,&value,sizeof(value)); }
        if(HMENU menu=GetMenu(window)) ThemeMenu(menu,true);
    }
    if(!lstrcmpW(cls,TOOLTIPS_CLASSW)) { SendMessageW(window,TTM_SETTIPBKCOLOR,PaintSysColor(COLOR_INFOBK),0);SendMessageW(window,TTM_SETTIPTEXTCOLOR,PaintSysColor(COLOR_INFOTEXT),0); }
    RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME);applying=false;
}
BOOL CALLBACK ApplyChild(HWND window,LPARAM) { ApplyWindow(window);return TRUE; }
BOOL CALLBACK ApplyTop(HWND window,LPARAM) { ApplyWindow(window);EnumChildWindows(window,ApplyChild,0);return TRUE; }
LRESULT CALLBACK CreationHook(int code,WPARAM wp,LPARAM lp)
{
    if(code>=0) { const auto* msg=reinterpret_cast<CWPRETSTRUCT*>(lp);if(msg->message==WM_CREATE || msg->message==WM_INITDIALOG) ApplyWindow(msg->hwnd); }
    return CallNextHookEx(NULL,code,wp,lp);
}
}
void ThemeMenu(HMENU menu,bool bar)
{
    if(!IsMenu(menu)) return;MENUINFO info={sizeof(info)};info.fMask=MIM_BACKGROUND;info.hbrBack=PaintSysColorBrush(COLOR_MENU);SetMenuInfo(menu,&info);
    // Dynamic entries may have been removed since the last popup. Retain only
    // records still attached to this menu, rather than leaking one set per open.
    std::set<DWORD_PTR> attached;
    for(int i=0;i<GetMenuItemCount(menu);++i)
    {
        MENUITEMINFOW item={sizeof(item)};item.fMask=MIIM_DATA;
        if(GetMenuItemInfoW(menu,i,TRUE,&item))attached.insert(item.dwItemData);
    }
    for(auto it=menuItems.begin();it!=menuItems.end();)
        if(it->menu==menu && !attached.count(DWORD_PTR(&*it)))
        {menuPointers.erase(DWORD_PTR(&*it));it=menuItems.erase(it);}else ++it;
    for(int i=0;i<GetMenuItemCount(menu);++i)
    {
        WCHAR label[512]={};MENUITEMINFOW item={sizeof(item)};item.fMask=MIIM_FTYPE|MIIM_STRING|MIIM_ID|MIIM_DATA|MIIM_SUBMENU;item.dwTypeData=label;item.cch=_countof(label);GetMenuItemInfoW(menu,i,TRUE,&item);
        if(item.hSubMenu) ThemeMenu(item.hSubMenu);
        bool ours=menuPointers.count(item.dwItemData)!=0;
        if((item.fType&MFT_OWNERDRAW) && !ours) continue; // The editable numeric popup owns its item data.
        if(registrySettings.DarkTheme || ours || bar)
        {
            MenuItem* data;if(ours) data=reinterpret_cast<MenuItem*>(item.dwItemData);
            else { menuItems.push_back({menu,UINT(i),item.wID,label,bar,(item.fType&MFT_SEPARATOR)!=0,item.hSubMenu!=NULL});data=&menuItems.back();menuPointers.insert(reinterpret_cast<DWORD_PTR>(data)); }
            data->bar=bar;if(label[0]) data->label=label;item.fType|=MFT_OWNERDRAW;item.dwItemData=reinterpret_cast<ULONG_PTR>(data);
            // Retain accessible/native captions as well as our rendering copy.
            item.dwTypeData=const_cast<LPWSTR>(LPCWSTR(data->label));item.cch=data->label.GetLength();
            item.fMask=MIIM_FTYPE|MIIM_DATA|MIIM_STRING;
            SetMenuItemInfoW(menu,i,TRUE,&item);
        }
    }
}
void InitializeTheme() { static HHOOK hook=SetWindowsHookExW(WH_CALLWNDPROCRET,CreationHook,NULL,GetCurrentThreadId());(void)hook; }
void ForgetThemedMenu(HMENU menu)
{
    for(int i=0;i<GetMenuItemCount(menu);++i) if(HMENU sub=GetSubMenu(menu,i)) ForgetThemedMenu(sub);
    for(auto it=menuItems.begin();it!=menuItems.end();)
        if(it->menu==menu) { menuPointers.erase(DWORD_PTR(&*it));it=menuItems.erase(it); } else ++it;
}
void ApplyTheme()
{
    InitializeTheme();EnumThreadWindows(GetCurrentThreadId(),ApplyTop,0);
    if(toolBoxContainer.IsWindow()) toolBoxContainer.ApplyScale();
    if(fontsDialog.IsWindow()) fontsDialog.InitToolbar();
    if(mainWindow.IsWindow()) { DrawMenuBar(mainWindow);RedrawWindow(mainWindow,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_ALLCHILDREN); }
}
