#include "precomp.h"

int PanelPercent() { return int(registrySettings.PanelMagnification); }
int PanelPx(int value) { return MulDiv(value,PanelPercent(),100); }
int PanelEditHeight(HWND edit)
{
    HDC dc=GetDC(edit);
    HFONT font=HFONT(SendMessage(edit,WM_GETFONT,0,0));
    HGDIOBJ old=SelectObject(dc,font?font:GetStockObject(DEFAULT_GUI_FONT));
    TEXTMETRICW metric={};GetTextMetricsW(dc,&metric);
    SelectObject(dc,old);ReleaseDC(edit,dc);
    // A single-line edit needs one text line, the native (unscaled) border,
    // and the same three client pixels of padding as the original dialogs.
    return metric.tmHeight+2*GetSystemMetrics(SM_CYEDGE)+3;
}
void CompactPanelEdit(HWND edit)
{
    if(!IsWindow(edit))return;
    RECT r;GetWindowRect(edit,&r);
    SetWindowPos(edit,NULL,0,0,r.right-r.left,PanelEditHeight(edit),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
}
int PanelScale() { return PanelPx(1); }
int ToolboxTopMargin() { return int(registrySettings.ToolboxTopMargin); }
int CanvasGripSize() { return max(3,min(15,int(registrySettings.CanvasMarkerSize))); }
int ToolbarHeight() { return 8*PanelPx(25); }
int ToolSettingsHeight() { return (toolsModel.GetActiveTool()==TOOL_BRUSH || toolsModel.GetActiveTool()==TOOL_RUBBER || toolsModel.GetActiveTool()==TOOL_AIRBRUSH)?242:toolsModel.IsSelection()?238:IsBoundedFigureTool(toolsModel.GetActiveTool())?200:toolsModel.GetActiveTool()==TOOL_TEXT?75:170; }
int ToolboxContentHeight() { return PanelPx(ToolboxTopMargin()+3+ToolSettingsHeight()+4)+ToolbarHeight(); }
int ToolboxWidth(int height)
{
    // Reserve the scrollbar gutter for the tallest tool. Switching to a short
    // tool disables scrolling instead of moving and repainting the canvas.
    int maximum=PanelPx(ToolboxTopMargin()+3+242+4)+ToolbarHeight();
    return PanelPx(50)+(height<maximum?GetSystemMetrics(SM_CXVSCROLL):0);
}

void ApplyPanelScale()
{
    // Changing interface metrics must preserve completed editable objects.
    if(canvasWindow.m_drawing || (!toolsModel.IsSelection() && !toolsModel.HasEditableFigure() &&
       !(toolsModel.GetActiveTool()==TOOL_TEXT && ::IsWindowVisible(textEditWindow)))) canvasWindow.OnEndDraw(TRUE);
    toolBoxContainer.ApplyScale();
    mainWindow.SendMessage(WM_SIZE);
    paletteWindow.Invalidate(FALSE);
    mainWindow.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
}

namespace
{
CStringW Text(UINT id) { CStringW s;s.LoadString(g_hinstExe,id);return s; }
const UINT panelCommands[]={IDM_PANEL_100,IDM_PANEL_150,IDM_PANEL_200,IDM_PANEL_250};

// Keep the native menu loop and its complete parent chain alive. The menu owns
// a real EDIT child; a thread-local menu filter routes input without activating
// another popup window (which would dismiss Windows' menu chain).
struct InlineNumber
{
    HMENU menu=NULL;
    HWND owner=NULL,window=NULL,edit=NULL;
    HHOOK hook=NULL;
    UINT command=0;
    bool cancel=false;
    int minimum=0,maximum=500;
    int Current() const { return command==IDM_MARKER_SIZE?CanvasGripSize():command==IDM_RECENT_FONTS?int(registrySettings.RecentFontsCount):ToolboxTopMargin(); }
    bool Value(int& value)
    {
        WCHAR text[16]={};GetWindowTextW(edit,text,_countof(text));
        return ParsePixelValue(text,value,minimum,maximum);
    }
    void Set(int value)
    {
        WCHAR text[16];StringCchPrintfW(text,_countof(text),L"%d",value);SetWindowTextW(edit,text);
        SendMessage(edit,EM_SETSEL,0,-1);
    }
    bool Apply()
    {
        int value;if(!edit || !Value(value)) return false;
        if(value==Current()) return true;
        if(command==IDM_MARKER_SIZE)
        {
            registrySettings.CanvasMarkerSize=value;
            canvasWindow.updateScrollRange();canvasWindow.Invalidate(FALSE);
        }
        else if(command==IDM_RECENT_FONTS) { registrySettings.RecentFontsCount=value;TrimRecentFonts();if(fontsDialog.IsWindow()) fontsDialog.InitFontNames(); }
        else { registrySettings.ToolboxTopMargin=value;ApplyPanelScale(); }
        registrySettings.StorePreferences();return true;
    }
    void Step(int delta)
    {
        int value=Current();Value(value);Set(max(minimum,min(maximum,value+delta)));Apply();
    }
    void End()
    {
        if(!cancel) Apply();
        if(hook) UnhookWindowsHookEx(hook);
        hook=NULL;
        if(edit && IsWindow(edit)) { SendMessage(edit,WM_KILLFOCUS,0,0);DestroyWindow(edit); }
        edit=NULL;window=NULL;menu=NULL;
    }
    static LRESULT CALLBACK EditProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR context)
    {
        auto self=reinterpret_cast<InlineNumber*>(context);
        if(msg==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
        if(msg==WM_KEYDOWN && wp==VK_RETURN)
        { if(!self->Apply()) MessageBeep(MB_OK);else self->Set(self->Current());SendMessage(hwnd,EM_SETSEL,0,-1);return 0; }
        if(msg==WM_CHAR && (wp==VK_RETURN || wp==VK_ESCAPE)) return 0;
        return DefSubclassProc(hwnd,msg,wp,lp);
    }
    static LRESULT CALLBACK Filter(int code,WPARAM wp,LPARAM lp);
    void Begin(HWND parent,HMENU popup,UINT id)
    {
        End();owner=parent;menu=popup;command=id;cancel=false;
        minimum=id==IDM_MARKER_SIZE?3:0;maximum=id==IDM_MARKER_SIZE?15:id==IDM_RECENT_FONTS?30:500;
        hook=SetWindowsHookExW(WH_MSGFILTER,Filter,NULL,GetCurrentThreadId());
    }
    void Draw(const DRAWITEMSTRUCT& item)
    {
        RECT r=item.rcItem;FillRect(item.hDC,&r,GetSysColorBrush(COLOR_MENU));
        RECT spin={r.left+80,r.top+5,r.left+98,r.bottom-5};
        RECT up=spin;up.bottom=(spin.top+spin.bottom)/2;RECT down=spin;down.top=up.bottom;
        DrawFrameControl(item.hDC,&up,DFC_SCROLL,DFCS_SCROLLUP);
        DrawFrameControl(item.hDC,&down,DFC_SCROLL,DFCS_SCROLLDOWN);
        RECT units={r.left+103,r.top,r.right,r.bottom};SetBkMode(item.hDC,TRANSPARENT);
        SetTextColor(item.hDC,GetSysColor(COLOR_MENUTEXT));DrawTextW(item.hDC,L"",-1,&units,DT_SINGLELINE|DT_VCENTER);
        HWND menuWindow=WindowFromDC(item.hDC);
        // The themed menu renderer may supply a memory DC. Resolve the popup
        // by its item rectangle in that case instead of depending on its DC.
        WCHAR className[32]={};GetClassNameW(menuWindow,className,_countof(className));
        if(lstrcmpW(className,L"#32768"))
        {
            RECT screen={};GetMenuItemRect(NULL,menu,0,&screen);menuWindow=NULL;
            HWND candidate=NULL;
            while((candidate=FindWindowExW(NULL,candidate,L"#32768",NULL))!=NULL)
            {
                if(GetWindowThreadProcessId(candidate,NULL)!=GetCurrentThreadId()) continue;
                RECT bounds;GetWindowRect(candidate,&bounds);
                POINT center={(screen.left+screen.right)/2,(screen.top+screen.bottom)/2};
                if(PtInRect(&bounds,center)) { menuWindow=candidate;break; }
            }
        }
        if(!menu || !menuWindow) return;
        if(!edit)
        {
            window=menuWindow;
            edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",NULL,WS_CHILD|WS_VISIBLE|ES_NUMBER|ES_CENTER|ES_AUTOHSCROLL|ES_NOHIDESEL,
                r.left+6,r.top+5,74,r.bottom-r.top-10,window,reinterpret_cast<HMENU>(IDC_MARKER_EDIT),g_hinstExe,NULL);
            SendMessage(edit,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
            SendMessage(edit,EM_SETLIMITTEXT,3,0);if(command!=IDM_RECENT_FONTS) InstallPixelUnits(edit);Set(Current());
            SetWindowSubclass(edit,EditProc,1,reinterpret_cast<DWORD_PTR>(this));
            SendMessage(edit,WM_SETFOCUS,0,0);
        }
        SetWindowPos(edit,HWND_TOP,r.left+6,r.top+5,74,r.bottom-r.top-10,SWP_NOACTIVATE|SWP_SHOWWINDOW);
        InvalidateRect(edit,NULL,TRUE);
    }
};
InlineNumber numberInput;
LRESULT CALLBACK InlineNumber::Filter(int code,WPARAM wp,LPARAM lp)
{
    InlineNumber& self=numberInput;
    if(code!=MSGF_MENU || !self.edit) return CallNextHookEx(self.hook,code,wp,lp);
    MSG& msg=*reinterpret_cast<MSG*>(lp);
    if(msg.message==WM_KEYDOWN)
    {
        if(msg.wParam==VK_ESCAPE) { self.cancel=true;EndMenu();return 1; }
        if(msg.wParam==VK_UP || msg.wParam==VK_DOWN) { self.Step(msg.wParam==VK_UP?1:-1);return 1; }
        if(msg.wParam==VK_TAB) { self.Apply();EndMenu();return 1; }
        if((GetKeyState(VK_CONTROL)&0x8000) && msg.wParam=='A') { SendMessage(self.edit,EM_SETSEL,0,-1);return 1; }
        if((GetKeyState(VK_CONTROL)&0x8000) && (msg.wParam=='V' || msg.wParam=='C' || msg.wParam=='X'))
        { SendMessage(self.edit,msg.wParam=='V'?WM_PASTE:msg.wParam=='C'?WM_COPY:WM_CUT,0,0);return 1; }
        // TranslateMessage is normally called by the native menu loop. Consumed
        // key messages must explicitly generate WM_CHAR for typing/backspace.
        TranslateMessage(&msg);SendMessage(self.edit,msg.message,msg.wParam,msg.lParam);return 1;
    }
    if(msg.message==WM_CHAR || msg.message==WM_KEYUP)
    { SendMessage(self.edit,msg.message,msg.wParam,msg.lParam);return 1; }
    if(msg.message>=WM_MOUSEFIRST && msg.message<=WM_MOUSELAST)
    {
        RECT r;if(GetMenuItemRect(NULL,self.menu,0,&r) && PtInRect(&r,msg.pt))
        {
            if(msg.message==WM_LBUTTONDOWN || msg.message==WM_LBUTTONDBLCLK)
            {
                if(msg.pt.x>=r.left+80 && msg.pt.x<r.left+98) self.Step(msg.pt.y<(r.top+r.bottom)/2?1:-1);
                else SendMessage(self.edit,EM_SETSEL,0,-1);
            }
            if(msg.message==WM_MOUSEWHEEL) self.Step(GET_WHEEL_DELTA_WPARAM(msg.wParam)>0?1:-1);
            SetCursor(LoadCursor(NULL,msg.pt.x<r.left+80?IDC_IBEAM:IDC_ARROW));return 1;
        }
    }
    return CallNextHookEx(self.hook,code,wp,lp);
}
}

void BuildPanelSettingsMenu(HMENU settings)
{
    HMENU sizes=CreatePopupMenu();
    for(int i=0;i<4;++i) { CStringW s;s.Format(L"%d%%",100+i*50);AppendMenuW(sizes,MF_STRING,panelCommands[i],s); }
    AppendMenuW(settings,MF_POPUP,reinterpret_cast<UINT_PTR>(sizes),Text(IDS_PANEL_SIZE));
    HMENU margin=CreatePopupMenu();AppendMenuW(margin,MF_OWNERDRAW,IDM_TOOLBOX_MARGIN,NULL);
    AppendMenuW(settings,MF_POPUP,reinterpret_cast<UINT_PTR>(margin),Text(IDS_TOOLBOX_MARGIN));
    HMENU markers=CreatePopupMenu(),size=CreatePopupMenu();
    AppendMenuW(size,MF_OWNERDRAW,IDM_MARKER_SIZE,NULL);
    AppendMenuW(markers,MF_POPUP,reinterpret_cast<UINT_PTR>(size),Text(IDS_MARKER_SIZE));
    AppendMenuW(markers,MF_STRING,IDM_MARKER_COLOR,Text(IDS_MARKER_COLOR));
    AppendMenuW(settings,MF_POPUP,reinterpret_cast<UINT_PTR>(markers),Text(IDS_CANVAS_MARKERS));
}

bool InitMarkerSizeMenu(HWND owner,HMENU popup)
{
    UINT id=GetMenuItemID(popup,0);
    if(GetMenuItemCount(popup)!=1 || (id!=IDM_MARKER_SIZE && id!=IDM_TOOLBOX_MARGIN && id!=IDM_RECENT_FONTS)) return false;
    numberInput.Begin(owner,popup,id);return true;
}
LRESULT HandlePanelMenuMessage(HWND,UINT msg,WPARAM wp,LPARAM lp,BOOL& handled)
{
    if(msg==WM_MEASUREITEM)
    {
        auto item=reinterpret_cast<MEASUREITEMSTRUCT*>(lp);
        if(item->CtlType==ODT_MENU && (item->itemID==IDM_MARKER_SIZE || item->itemID==IDM_TOOLBOX_MARGIN || item->itemID==IDM_RECENT_FONTS))
        { item->itemWidth=124;item->itemHeight=36;return TRUE; }
    }
    if(msg==WM_DRAWITEM)
    {
        auto item=reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if(item->CtlType==ODT_MENU && (item->itemID==IDM_MARKER_SIZE || item->itemID==IDM_TOOLBOX_MARGIN || item->itemID==IDM_RECENT_FONTS))
        { numberInput.Draw(*item);return TRUE; }
    }
    if(msg==WM_UNINITMENUPOPUP && reinterpret_cast<HMENU>(wp)==numberInput.menu) numberInput.End();
    if(msg==WM_EXITMENULOOP) numberInput.End();
    handled=FALSE;return 0;
}
void ShowMarkerSizeInput(HWND owner)
{
    HMENU settings=GetSubMenu(GetMenu(owner),4),markers=GetSubMenu(settings,1);
    POINT p;GetCursorPos(&p);TrackPopupMenu(GetSubMenu(markers,0),TPM_LEFTALIGN,p.x,p.y,0,owner,NULL);
}
bool HandlePanelSettingsCommand(HWND owner,UINT command)
{
    for(int i=0;i<4;++i) if(command==panelCommands[i])
    {
        registrySettings.PanelMagnification=100+i*50;
        registrySettings.StorePreferences();ApplyPanelScale();return true;
    }
    if(command==IDM_MARKER_SIZE) { ShowMarkerSizeInput(owner);return true; }
    if(command==IDM_MARKER_COLOR)
    {
        COLORREF color=registrySettings.CanvasMarkerColor;
        if(mainWindow.ChooseColor(&color))
        {
            registrySettings.CanvasMarkerColor=color;registrySettings.StorePreferences();canvasWindow.Invalidate(FALSE);
        }
        return true;
    }
    return false;
}
void UpdatePanelSettingsChecks(HMENU menu)
{
    for(UINT i=0;i<4;++i) CheckMenuItem(menu,panelCommands[i],CHECKED_IF(PanelPercent()==int(100+i*50)));
}

namespace
{
LRESULT CALLBACK ScaledTrackbarProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR)
{
    if(PanelPercent()==100 && !registrySettings.DarkTheme) return DefSubclassProc(hwnd,message,wp,lp);
    // Native trackbar invalidation covers its own thumb, not our larger one.
    // Repaint the entire custom control on position/range and keyboard changes.
    if(message==TBM_SETPOS || message==TBM_SETRANGE || message==TBM_SETRANGEMIN ||
       message==TBM_SETRANGEMAX || message==WM_KEYDOWN || message==WM_KEYUP ||
       message==WM_MOUSEWHEEL || message==WM_ENABLE || message==WM_SIZE)
    {
        LRESULT result=DefSubclassProc(hwnd,message,wp,lp);
        InvalidateRect(hwnd,NULL,FALSE);
        return result;
    }
    if(message!=WM_PAINT && message!=WM_PRINTCLIENT && message!=WM_ERASEBKGND && message!=WM_LBUTTONDOWN &&
       message!=WM_MOUSEMOVE && message!=WM_LBUTTONUP && message!=WM_CAPTURECHANGED &&
       message!=WM_SETFOCUS && message!=WM_KILLFOCUS)
        return DefSubclassProc(hwnd,message,wp,lp);
    CRect rc;GetClientRect(hwnd,&rc);
    int top=PanelPx(8),bottom=max(top+1,rc.bottom-PanelPx(8)),x=min(rc.right-PanelPx(9),PanelPx(12));
    int minimum=(int)SendMessage(hwnd,TBM_GETRANGEMIN,0,0),maximum=(int)SendMessage(hwnd,TBM_GETRANGEMAX,0,0);
    if(message==WM_ERASEBKGND) return 1;
    if(message==WM_PAINT || message==WM_PRINTCLIENT)
    {
        PAINTSTRUCT ps={};HDC dc=message==WM_PAINT?BeginPaint(hwnd,&ps):reinterpret_cast<HDC>(wp);
        FillRect(dc,&rc,GetSysColorBrush(COLOR_BTNFACE));
        bool dark=registrySettings.DarkTheme!=0,enabled=IsWindowEnabled(hwnd)!=FALSE;
        HBRUSH scaleBrush=GetSysColorBrush(dark?(enabled?COLOR_BTNTEXT:COLOR_GRAYTEXT):COLOR_3DSHADOW);
        HBRUSH thumbBrush=GetSysColorBrush(dark?(enabled?COLOR_BTNTEXT:COLOR_GRAYTEXT):COLOR_HIGHLIGHT);
        RECT groove={x-PanelScale(),top,x+PanelScale(),bottom};
        FillRect(dc,&groove,scaleBrush);
        int ticks=maximum-minimum<=6?maximum-minimum:4;
        for(int i=0;i<=ticks;++i)
        {
            int y=top+MulDiv(i,bottom-top,max(1,ticks));RECT tick={x+PanelPx(8),y,x+PanelPx(11),y+PanelScale()};
            FillRect(dc,&tick,scaleBrush);
        }
        int value=(int)SendMessage(hwnd,TBM_GETPOS,0,0),y=top+MulDiv(value-minimum,bottom-top,max(1,maximum-minimum));
        POINT thumb[]={{x-PanelPx(6),y-PanelPx(4)},{x+PanelPx(4),y-PanelPx(4)},
            {x+PanelPx(8),y},{x+PanelPx(4),y+PanelPx(4)},{x-PanelPx(6),y+PanelPx(4)}};
        HGDIOBJ oldPen=SelectObject(dc,GetStockObject(NULL_PEN)),oldBrush=SelectObject(dc,thumbBrush);
        Polygon(dc,thumb,_countof(thumb));SelectObject(dc,oldBrush);SelectObject(dc,oldPen);
        if(GetFocus()==hwnd) { rc.DeflateRect(PanelScale(),PanelScale());DrawFocusRect(dc,&rc); }
        if(message==WM_PAINT) EndPaint(hwnd,&ps);return 0;
    }
    auto drag=[&](int y,UINT notification)
    {
        int value=minimum+MulDiv(max(top,min(bottom,y))-top,maximum-minimum,bottom-top);
        SendMessage(hwnd,TBM_SETPOS,TRUE,value);
        SendMessage(GetParent(hwnd),WM_VSCROLL,MAKEWPARAM(notification,value),reinterpret_cast<LPARAM>(hwnd));
    };
    if(message==WM_LBUTTONDOWN)
    {
        SetFocus(hwnd);SetPropW(hwnd,L"PanelTrackDrag",reinterpret_cast<HANDLE>(1));SetCapture(hwnd);
        drag(GET_Y_LPARAM(lp),TB_THUMBTRACK);return 0;
    }
    if(message==WM_MOUSEMOVE && GetPropW(hwnd,L"PanelTrackDrag")) { drag(GET_Y_LPARAM(lp),TB_THUMBTRACK);return 0; }
    if(message==WM_LBUTTONUP && GetPropW(hwnd,L"PanelTrackDrag"))
    {
        drag(GET_Y_LPARAM(lp),TB_THUMBPOSITION);RemovePropW(hwnd,L"PanelTrackDrag");ReleaseCapture();
        SendMessage(GetParent(hwnd),WM_VSCROLL,TB_ENDTRACK,reinterpret_cast<LPARAM>(hwnd));return 0;
    }
    if(message==WM_CAPTURECHANGED && RemovePropW(hwnd,L"PanelTrackDrag"))
        SendMessage(GetParent(hwnd),WM_VSCROLL,TB_ENDTRACK,reinterpret_cast<LPARAM>(hwnd));
    if(message==WM_SETFOCUS || message==WM_KILLFOCUS) InvalidateRect(hwnd,NULL,TRUE);
    return DefSubclassProc(hwnd,message,wp,lp);
}
}
void InstallPanelTrackbar(HWND trackbar) { SetWindowSubclass(trackbar,ScaledTrackbarProc,19,0); }
