#include "precomp.h"

namespace {
CStringW GroupText(int group) { CStringW text;text.LoadString(g_hinstExe,IDS_SHAPE_GROUP_FIRST+group);return text; }
CStringW FigureText(UINT id) { CStringW text;text.LoadString(g_hinstExe,id);return text; }
int ChoiceSlot(int choice) { return choice>=0?choice:FIG_COUNT+(choice==-TOOL_LINE?0:choice==-TOOL_BEZIER?1:2); }
int ChoiceId(int choice) { return choice>=0?IDC_FIGURE_FIRST+choice:IDC_LINE_CHOICE_FIRST+ChoiceSlot(choice)-FIG_COUNT; }
int ActiveChoice()
{
    switch(toolsModel.GetActiveTool())
    {
    case TOOL_LINE:case TOOL_BEZIER:case TOOL_SHAPE:return -toolsModel.GetActiveTool();
    case TOOL_RECT:return FIG_RECT;case TOOL_RRECT:return FIG_RRECT;case TOOL_ELLIPSE:return FIG_ELLIPSE;
    case TOOL_FIGURE:return registrySettings.FigureKind;default:return INT_MIN;
    }
}
class FigurePopup : public CWindowImpl<FigurePopup>
{
public:
    DECLARE_WND_CLASS_EX(L"PaintFigurePopup",0,COLOR_BTNFACE)
    HWND tip=NULL;CStringW labels[FIG_COUNT+3];int group=GROUP_POLYGON,columns=6;
    BEGIN_MSG_MAP(FigurePopup)
        MESSAGE_HANDLER(WM_CREATE,OnCreate)
        MESSAGE_HANDLER(WM_COMMAND,OnCommand)
        MESSAGE_HANDLER(WM_DRAWITEM,OnDrawItem)
        MESSAGE_HANDLER(WM_ACTIVATE,OnActivate)
        MESSAGE_HANDLER(WM_CLOSE,OnClose)
        MESSAGE_HANDLER(WM_DESTROY,OnDestroy)
    END_MSG_MAP()
    static LRESULT CALLBACK KeyProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR context)
    {
        auto self=reinterpret_cast<FigurePopup*>(context);
        if(msg==WM_KEYDOWN)
        {
            if(wp==VK_ESCAPE) { self->ShowWindow(SW_HIDE);canvasWindow.SetFocus();return 0; }
            if(wp==VK_RETURN || wp==VK_SPACE) { ::SendMessage(hwnd,BM_CLICK,0,0);return 0; }
            const auto& items=FigureGroupItems(self->group);int index=0,n=int(items.size());
            for(int i=0;i<n;++i)if(ChoiceId(items[i])==::GetDlgCtrlID(hwnd))index=i;
            int next=index;
            if(wp==VK_RIGHT || wp==VK_TAB)next=(index+((wp==VK_TAB && GetKeyState(VK_SHIFT)<0)?n-1:1))%n;
            if(wp==VK_LEFT)next=(index+n-1)%n;
            if(wp==VK_DOWN)next=(index+self->columns)%n;
            if(wp==VK_UP)next=(index+n-self->columns)%n;
            if(next!=index) { ::SetFocus(self->GetDlgItem(ChoiceId(items[next])));return 0; }
        }
        return DefSubclassProc(hwnd,msg,wp,lp);
    }
    void Reload()
    {
        if(!IsWindow())return;SetWindowText(GroupText(group));::SendMessage(tip,TTM_POP,0,0);
        for(int g=0;g<GROUP_COUNT;++g)for(int choice:FigureGroupItems(g))
        {
            int slot=ChoiceSlot(choice);labels[slot]=FigureLabel(choice);HWND button=GetDlgItem(ChoiceId(choice));
            ::SetWindowTextW(button,labels[slot]);TOOLINFOW info={sizeof(info)};info.hwnd=m_hWnd;info.uFlags=TTF_IDISHWND;
            info.uId=UINT_PTR(button);info.lpszText=labels[slot].GetBuffer();::SendMessage(tip,TTM_UPDATETIPTEXTW,0,LPARAM(&info));labels[slot].ReleaseBuffer();
        }
    }
    LRESULT OnCreate(UINT,WPARAM,LPARAM,BOOL&)
    {
        tip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,NULL,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,0,0,0,0,m_hWnd,NULL,g_hinstExe,NULL);
        ::SendMessage(tip,TTM_SETMAXTIPWIDTH,0,320);
        for(int g=0;g<GROUP_COUNT;++g)for(int choice:FigureGroupItems(g))
        {
            int slot=ChoiceSlot(choice);labels[slot]=FigureLabel(choice);
            HWND button=CreateWindowW(L"BUTTON",labels[slot],WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,0,0,28,28,m_hWnd,HMENU(INT_PTR(ChoiceId(choice))),g_hinstExe,NULL);
            SetWindowSubclass(button,KeyProc,1,DWORD_PTR(this));
            TOOLINFOW info={sizeof(info)};info.hwnd=m_hWnd;info.uFlags=TTF_IDISHWND|TTF_SUBCLASS;
            info.uId=UINT_PTR(button);info.lpszText=labels[slot].GetBuffer();::SendMessage(tip,TTM_ADDTOOLW,0,LPARAM(&info));labels[slot].ReleaseBuffer();
        }
        return 0;
    }
    LRESULT OnCommand(UINT,WPARAM wp,LPARAM,BOOL&)
    {
        if(HIWORD(wp)==BN_CLICKED)for(int choice:FigureGroupItems(group))if(ChoiceId(choice)==LOWORD(wp))
        { ShowWindow(SW_HIDE);ActivateFigureChoice(choice);break; }
        return 0;
    }
    LRESULT OnDrawItem(UINT,WPARAM,LPARAM lp,BOOL&)
    {
        auto& d=*reinterpret_cast<DRAWITEMSTRUCT*>(lp);int choice=INT_MIN;
        for(int value:FigureGroupItems(group))if(ChoiceId(value)==d.CtlID)choice=value;
        if(choice==INT_MIN)return FALSE;
        RECT r=d.rcItem;DrawFrameControl(d.hDC,&r,DFC_BUTTON,DFCS_BUTTONPUSH|((d.itemState&ODS_SELECTED)?DFCS_PUSHED:0));
        InflateRect(&r,-PanelPx(4),-PanelPx(4));DrawFigureIcon(d.hDC,choice,r,GetSysColor(COLOR_BTNTEXT));
        if(FigureGroupSelection(group)==choice) { r=d.rcItem;InflateRect(&r,-PanelPx(2),-PanelPx(2));FrameRect(d.hDC,&r,GetSysColorBrush(COLOR_HIGHLIGHT)); }
        if(d.itemState&ODS_FOCUS) { r=d.rcItem;InflateRect(&r,-PanelPx(3),-PanelPx(3));DrawFocusRect(d.hDC,&r); }
        return TRUE;
    }
    LRESULT OnActivate(UINT,WPARAM wp,LPARAM,BOOL&) { if(LOWORD(wp)==WA_INACTIVE)ShowWindow(SW_HIDE);return 0; }
    LRESULT OnClose(UINT,WPARAM,LPARAM,BOOL&) { ShowWindow(SW_HIDE);return 0; }
    LRESULT OnDestroy(UINT,WPARAM,LPARAM,BOOL&) { if(tip)::DestroyWindow(tip);return 0; }
} popup;

class FigureControls : public CWindowImpl<FigureControls>
{
public:
    bool origin=false;HWND pressed=NULL,tip=NULL;CStringW labels[GROUP_COUNT];
    DECLARE_WND_CLASS_EX(L"PaintFigureControls",0,COLOR_BTNFACE)
    BEGIN_MSG_MAP(FigureControls)
        MESSAGE_HANDLER(WM_CREATE,OnCreate)
        MESSAGE_HANDLER(WM_COMMAND,OnCommand)
        MESSAGE_HANDLER(WM_DRAWITEM,OnDrawItem)
        MESSAGE_HANDLER(WM_DESTROY,OnDestroy)
    END_MSG_MAP()
    static LRESULT CALLBACK ButtonProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR context)
    {
        auto self=reinterpret_cast<FigureControls*>(context);int group=::GetDlgCtrlID(hwnd)-IDC_SHAPE_GROUP_FIRST;
        if(msg==WM_LBUTTONDOWN || msg==WM_LBUTTONDBLCLK)
        {
            self->pressed=hwnd;::SetFocus(hwnd);::SetCapture(hwnd);::SendMessage(hwnd,BM_SETSTATE,TRUE,0);
            if(group!=GROUP_ELLIPSE)::SetTimer(hwnd,1,400,NULL);return 0;
        }
        if(msg==WM_TIMER && wp==1)
        {
            ::KillTimer(hwnd,1);
            if(self->pressed==hwnd && ::GetCapture()==hwnd)
            { self->pressed=NULL;::SendMessage(hwnd,BM_SETSTATE,FALSE,0);::ReleaseCapture();ShowFigureGallery(group); }
            return 0;
        }
        if(msg==WM_MOUSEMOVE && self->pressed==hwnd)
        {
            RECT r;::GetClientRect(hwnd,&r);POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
            if(!PtInRect(&r,p))::KillTimer(hwnd,1);::SendMessage(hwnd,BM_SETSTATE,PtInRect(&r,p),0);return 0;
        }
        if(msg==WM_LBUTTONUP)
        {
            bool click=self->pressed==hwnd;self->pressed=NULL;::KillTimer(hwnd,1);::SendMessage(hwnd,BM_SETSTATE,FALSE,0);
            if(::GetCapture()==hwnd)::ReleaseCapture();RECT r;::GetClientRect(hwnd,&r);POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
            if(click && PtInRect(&r,p))self->SendMessage(WM_COMMAND,MAKEWPARAM(::GetDlgCtrlID(hwnd),BN_CLICKED),LPARAM(hwnd));return 0;
        }
        if(msg==WM_CAPTURECHANGED || msg==WM_CANCELMODE || msg==WM_DESTROY || (msg==WM_KEYDOWN && wp==VK_ESCAPE))
        {
            if(self->pressed==hwnd)self->pressed=NULL;::KillTimer(hwnd,1);::SendMessage(hwnd,BM_SETSTATE,FALSE,0);
            if(msg==WM_KEYDOWN && ::GetCapture()==hwnd)::ReleaseCapture();
        }
        if(msg==WM_KEYDOWN && (wp==VK_DOWN || wp==VK_F4) && group!=GROUP_ELLIPSE) { ShowFigureGallery(group);return 0; }
        return DefSubclassProc(hwnd,msg,wp,lp);
    }
    LRESULT OnCreate(UINT,WPARAM,LPARAM,BOOL&)
    {
        tip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,NULL,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,0,0,0,0,m_hWnd,NULL,g_hinstExe,NULL);
        ::SendMessage(tip,TTM_SETMAXTIPWIDTH,0,320);
        for(int i=0;i<(origin?2:GROUP_COUNT);++i)
        {
            int id=origin?IDC_FIGURE_CORNER+i:IDC_SHAPE_GROUP_FIRST+i;
            labels[i]=origin?FigureText(IDS_FIGURE_CORNER+i):GroupText(i);
            HWND button=CreateWindowW(L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,25,25,m_hWnd,HMENU(INT_PTR(id)),g_hinstExe,NULL);
            if(!origin)SetWindowSubclass(button,ButtonProc,1,DWORD_PTR(this));
            TOOLINFOW info={sizeof(info)};info.hwnd=m_hWnd;info.uFlags=TTF_IDISHWND|TTF_SUBCLASS;
            info.uId=UINT_PTR(button);info.lpszText=labels[i].GetBuffer();::SendMessage(tip,TTM_ADDTOOLW,0,LPARAM(&info));labels[i].ReleaseBuffer();
        }
        return 0;
    }
    LRESULT OnCommand(UINT,WPARAM wp,LPARAM,BOOL&)
    {
        int id=LOWORD(wp);if(HIWORD(wp)!=BN_CLICKED)return 0;
        if(!origin && id>=IDC_SHAPE_GROUP_FIRST && id<IDC_SHAPE_GROUP_FIRST+GROUP_COUNT)ActivateFigureChoice(FigureGroupSelection(id-IDC_SHAPE_GROUP_FIRST));
        if(origin && (id==IDC_FIGURE_CORNER || id==IDC_FIGURE_CENTER))
        { registrySettings.ShapeFromCenter=id==IDC_FIGURE_CENTER;registrySettings.StorePreferences();RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);canvasWindow.Invalidate(FALSE); }
        return 0;
    }
    LRESULT OnDrawItem(UINT,WPARAM,LPARAM lp,BOOL&)
    {
        auto& d=*reinterpret_cast<DRAWITEMSTRUCT*>(lp);RECT r=d.rcItem;
        int group=int(d.CtlID)-IDC_SHAPE_GROUP_FIRST;
        bool selected=origin?registrySettings.ShapeFromCenter==(d.CtlID==IDC_FIGURE_CENTER):FigureGroupForKind(ActiveChoice())==group;
        DrawFrameControl(d.hDC,&r,DFC_BUTTON,DFCS_BUTTONPUSH|((selected || (d.itemState&ODS_SELECTED))?DFCS_PUSHED:0));InflateRect(&r,-PanelPx(4),-PanelPx(4));
        if(origin)
        {
            HPEN pen=CreatePen(PS_DOT,1,GetSysColor(COLOR_BTNTEXT));HGDIOBJ oldPen=SelectObject(d.hDC,pen),oldBrush=SelectObject(d.hDC,GetStockObject(NULL_BRUSH));
            Rectangle(d.hDC,r.left,r.top,r.right,r.bottom);SelectObject(d.hDC,oldPen);SelectObject(d.hDC,oldBrush);DeleteObject(pen);
            int x=d.CtlID==IDC_FIGURE_CENTER?(r.left+r.right)/2:r.left,y=d.CtlID==IDC_FIGURE_CENTER?(r.top+r.bottom)/2:r.top;
            RECT point={x-PanelPx(2),y-PanelPx(2),x+PanelPx(3),y+PanelPx(3)};FillRect(d.hDC,&point,GetSysColorBrush(COLOR_HIGHLIGHT));
        }
        else if(group>=0 && group<GROUP_COUNT)
        {
            DrawFigureIcon(d.hDC,FigureGroupSelection(group),r,GetSysColor(COLOR_BTNTEXT));
            if(group!=GROUP_ELLIPSE)
            {
                int x=d.rcItem.right-PanelPx(3),y=d.rcItem.bottom-PanelPx(3);
                POINT arrow[]={{x-PanelPx(3),y},{x,y},{x,y-PanelPx(3)}};
                Poly(d.hDC,arrow,3,GetSysColor(COLOR_BTNTEXT),GetSysColor(COLOR_BTNTEXT),1,1,TRUE,FALSE);
            }
        }
        if(selected) { r=d.rcItem;InflateRect(&r,-PanelPx(2),-PanelPx(2));FrameRect(d.hDC,&r,GetSysColorBrush(COLOR_HIGHLIGHT)); }
        if(d.itemState&ODS_FOCUS) { r=d.rcItem;InflateRect(&r,-PanelPx(3),-PanelPx(3));DrawFocusRect(d.hDC,&r); }return TRUE;
    }
    LRESULT OnDestroy(UINT,WPARAM,LPARAM,BOOL&) { if(tip)::DestroyWindow(tip);return 0; }
} galleryControls,originControls;
}

void CreateFigureControls(HWND toolbox)
{ galleryControls.Create(toolbox,CRect(0,0,50,75),NULL,WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,WS_EX_CONTROLPARENT); }
void CreateFigureOriginControls(HWND settings)
{
    originControls.origin=true;originControls.Create(settings,CRect(0,0,50,25),NULL,WS_CHILD|WS_CLIPCHILDREN,WS_EX_CONTROLPARENT,IDC_FIGURE_ORIGIN);
    LayoutFigureControls();UpdateFigureControls();
}
void UpdateFigureControls()
{
    int active=ActiveChoice();if(active!=INT_MIN)RememberFigureGroupChoice(active);
    if(galleryControls.IsWindow())for(int group=0;group<GROUP_COUNT;++group)
    {
        CStringW title=GroupText(group);if(group!=GROUP_ELLIPSE)title+=L": "+FigureLabel(FigureGroupSelection(group));
        galleryControls.labels[group]=title;if(group!=GROUP_ELLIPSE)galleryControls.labels[group]+=L"\n"+FigureText(IDS_FIGURE_HOLD);
        HWND button=galleryControls.GetDlgItem(IDC_SHAPE_GROUP_FIRST+group);::SetWindowTextW(button,title);
        TOOLINFOW info={sizeof(info)};info.hwnd=galleryControls;info.uFlags=TTF_IDISHWND;info.uId=UINT_PTR(button);
        info.lpszText=galleryControls.labels[group].GetBuffer();::SendMessage(galleryControls.tip,TTM_UPDATETIPTEXTW,0,LPARAM(&info));galleryControls.labels[group].ReleaseBuffer();
        ::InvalidateRect(button,NULL,FALSE);
    }
    if(originControls.IsWindow())originControls.ShowWindow(IsBoundedFigureTool(toolsModel.GetActiveTool())?SW_SHOWNA:SW_HIDE);
}
void ShowFigureGallery(int group)
{
    if(group<0 || group>=GROUP_COUNT || group==GROUP_ELLIPSE)return;
    if(popup.IsWindowVisible() && popup.group==group) { popup.ShowWindow(SW_HIDE);return; }
    popup.group=group;const auto& items=FigureGroupItems(group);
    RECT anchor;::GetWindowRect(galleryControls.GetDlgItem(IDC_SHAPE_GROUP_FIRST+group),&anchor);
    MONITORINFO monitor={sizeof(monitor)};GetMonitorInfo(MonitorFromRect(&anchor,MONITOR_DEFAULTTONEAREST),&monitor);
    popup.columns=min(group==GROUP_ARROWS?4:int(items.size()),max(1,(monitor.rcWork.right-monitor.rcWork.left-PanelPx(8))/PanelPx(28)));
    int width=PanelPx(popup.columns*28+6),height=PanelPx(((int(items.size())+popup.columns-1)/popup.columns)*28+6);
    int x=anchor.right,y=anchor.top;if(x+width>monitor.rcWork.right)x=max(monitor.rcWork.left,anchor.left-width);
    y=max(monitor.rcWork.top,min(y,monitor.rcWork.bottom-height));
    if(!popup.IsWindow())popup.Create(mainWindow,CRect(x,y,x+width,y+height),GroupText(group),WS_POPUP|WS_BORDER|WS_CLIPCHILDREN,WS_EX_TOOLWINDOW);
    for(HWND child=::GetWindow(popup,GW_CHILD);child;child=::GetWindow(child,GW_HWNDNEXT))::ShowWindow(child,SW_HIDE);
    for(size_t i=0;i<items.size();++i)
        ::SetWindowPos(popup.GetDlgItem(ChoiceId(items[i])),NULL,PanelPx(2+int(i)%popup.columns*28),PanelPx(2+int(i)/popup.columns*28),PanelPx(28),PanelPx(28),SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    popup.SetWindowText(GroupText(group));popup.SetWindowPos(HWND_TOP,x,y,width,height,SWP_SHOWWINDOW);
    ::SetFocus(popup.GetDlgItem(ChoiceId(FigureGroupSelection(group))));popup.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
void ReloadFigureLanguage()
{
    UpdateFigureControls();popup.Reload();
    if(originControls.IsWindow())for(int i=0;i<2;++i)
    {
        originControls.labels[i]=FigureText(IDS_FIGURE_CORNER+i);HWND button=originControls.GetDlgItem(IDC_FIGURE_CORNER+i);::SetWindowTextW(button,originControls.labels[i]);
        TOOLINFOW info={sizeof(info)};info.hwnd=originControls;info.uFlags=TTF_IDISHWND;info.uId=UINT_PTR(button);
        info.lpszText=originControls.labels[i].GetBuffer();::SendMessage(originControls.tip,TTM_UPDATETIPTEXTW,0,LPARAM(&info));originControls.labels[i].ReleaseBuffer();
    }
}
void LayoutFigureControls(int scrollOffset)
{
    if(galleryControls.IsWindow())
    {
        int size=PanelPx(25),width=PanelPx(50)/2;
        galleryControls.SetWindowPos(HWND_TOP,0,PanelPx(ToolboxTopMargin())+5*size-scrollOffset,2*width,3*size,SWP_NOACTIVATE);
        for(int i=0;i<GROUP_COUNT;++i)::MoveWindow(galleryControls.GetDlgItem(IDC_SHAPE_GROUP_FIRST+i),(i%2)*width,(i/2)*size,width,size,TRUE);
    }
    if(originControls.IsWindow())
    {
        originControls.MoveWindow(0,PanelPx(168),PanelPx(50),PanelPx(25));
        for(int i=0;i<2;++i)::MoveWindow(originControls.GetDlgItem(IDC_FIGURE_CORNER+i),PanelPx(i*25),0,PanelPx(25),PanelPx(25),TRUE);
    }
    if(popup.IsWindow())popup.ShowWindow(SW_HIDE);
}
