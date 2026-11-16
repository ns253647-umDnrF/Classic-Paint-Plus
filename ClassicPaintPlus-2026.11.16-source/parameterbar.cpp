#include <cmath>
#include "precomp.h"

namespace {
const int editIds[]={IDC_OBJECT_WIDTH,IDC_OBJECT_HEIGHT,IDC_SELECTION_ANGLE,IDC_SKEW_HORIZONTAL,IDC_SKEW_VERTICAL,IDC_ROUNDING_EDIT,IDC_STAR_BEND_EDIT};
int SliderId(int p){return p==PARAM_ROUNDING?IDC_FIGURE_ROUNDING:IDC_STAR_BEND_SLIDER;}
int SliderValue(int p){return p==PARAM_ROUNDING?FigureRounding():toolsModel.StarBend();}
CStringW SliderText(int p)
{
    CStringW value;if(ParameterHasValue(p))
        value.Format(p==PARAM_ROUNDING?L"%d px":SliderValue(p)>0?L"+%d%%":L"%d%%",SliderValue(p));
    return value;
}
class ParameterBar : public CWindowImpl<ParameterBar>
{
public:
    DECLARE_WND_CLASS_EX(L"PaintParameterBar",0,COLOR_BTNFACE)
    HFONT font=NULL;int fontScale=0,scrollX=0,contentWidth=0;bool updating=false;CStringW labels[PARAM_COUNT];
    BEGIN_MSG_MAP(ParameterBar)
        MESSAGE_HANDLER(WM_CREATE,OnCreate)
        MESSAGE_HANDLER(WM_SIZE,OnSize)
        MESSAGE_HANDLER(WM_COMMAND,OnCommand)
        MESSAGE_HANDLER(WM_HSCROLL,OnScroll)
        MESSAGE_HANDLER(WM_DRAWITEM,OnDrawItem)
        MESSAGE_HANDLER(WM_DESTROY,OnDestroy)
    END_MSG_MAP()
    void Font()
    {
        if(font && fontScale==PanelPercent())return;
        LOGFONTW lf={};GetObjectW(GetStockObject(DEFAULT_GUI_FONT),sizeof(lf),&lf);lf.lfHeight=-PanelPx(11);
        HFONT next=CreateFontIndirectW(&lf);
        for(HWND c=::GetWindow(m_hWnd,GW_CHILD);c;c=::GetWindow(c,GW_HWNDNEXT))::SendMessage(c,WM_SETFONT,WPARAM(next),TRUE);
        if(font)DeleteObject(font);font=next;fontScale=PanelPercent();
    }
    int Layout(int width,bool move)
    {
        Font();HDC dc=::GetDC(m_hWnd);HGDIOBJ old=SelectObject(dc,font);
        const int columns[]={0,0,1,2,2,3,3},rows[]={0,1,0,0,1,0,1};
        int labelWidths[PARAM_COUNT]={},columnLabels[4]={},columnWidths[4]={};
        for(int p=0;p<PARAM_COUNT;++p)
        {
            SIZE extent={};GetTextExtentPoint32W(dc,labels[p],labels[p].GetLength(),&extent);
            labelWidths[p]=PanelPx(p<PARAM_ROUNDING?17:3)+extent.cx+PanelPx(6);
            columnLabels[columns[p]]=max(columnLabels[columns[p]],labelWidths[p]);
        }
        // Share the label column between both rows, independently of language.
        // The extra trailing space separates every caption from its edit box.
        for(int p=0;p<PARAM_COUNT;++p)
        {
            labelWidths[p]=columnLabels[columns[p]];
            int whole=labelWidths[p]+PanelPx(p>=PARAM_ROUNDING?48+85+3:62)+PanelPx(8);
            columnWidths[columns[p]]=max(columnWidths[columns[p]],whole);
        }
        contentWidth=PanelPx(6);for(int col:columnWidths)contentWidth+=col;
        if(move)
        {
            SCROLLINFO si={sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS,0,contentWidth-1,UINT(max(1,width)),scrollX,0};
            SetScrollInfo(SB_HORZ,&si,TRUE);scrollX=GetScrollPos(SB_HORZ);
            for(int p=0;p<PARAM_COUNT;++p)
            {
                int x=PanelPx(3)-scrollX,y=PanelPx(2+rows[p]*23),edit=PanelPx(p>=PARAM_ROUNDING?48:62);
                for(int col=0;col<columns[p];++col)x+=columnWidths[col];
                ::MoveWindow(GetDlgItem(IDC_PARAMETER_FIXED_FIRST+p),x,y,labelWidths[p],PanelPx(21),TRUE);
                int height=PanelEditHeight(GetDlgItem(editIds[p]));
                ::MoveWindow(GetDlgItem(editIds[p]),x+labelWidths[p],y+(PanelPx(21)-height)/2,edit,height,TRUE);
                if(p>=PARAM_ROUNDING)::MoveWindow(GetDlgItem(SliderId(p)),x+labelWidths[p]+edit+PanelPx(3),y,PanelPx(85),PanelPx(21),TRUE);
            }
        }
        SelectObject(dc,old);::ReleaseDC(m_hWnd,dc);return PanelPx(CY_PALETTE);
    }
    void Reveal(HWND control)
    {
        RECT child,client;::GetWindowRect(control,&child);ScreenToClient(&child);GetClientRect(&client);
        if(child.left<0)scrollX+=child.left;else if(child.right>client.right)scrollX+=child.right-client.right;
        Layout(client.right,true);Invalidate(FALSE);
    }
    void Reload()
    {
        if(!IsWindow())return;
        for(int p=0;p<PARAM_COUNT;++p){labels[p].LoadString(g_hinstExe,IDS_PARAMETER_FIRST+p);::SetWindowTextW(GetDlgItem(IDC_PARAMETER_FIXED_FIRST+p),labels[p]);}
        RECT r;GetClientRect(&r);Layout(r.right,true);
    }
    void Update()
    {
        if(!IsWindow() || updating)return;updating=true;
        for(int p=0;p<PARAM_COUNT;++p)
        {
            bool visible=ParameterAvailable(p),enabled=visible && (!toolsModel.IsSelection() || ParameterObjectActive());
            HWND check=GetDlgItem(IDC_PARAMETER_FIXED_FIRST+p),edit=GetDlgItem(editIds[p]);
            ::ShowWindow(check,visible?SW_SHOWNA:SW_HIDE);::EnableWindow(check,enabled);
            if(p<PARAM_ROUNDING)::SendMessage(check,BM_SETCHECK,ParameterIsFixed(p)?BST_CHECKED:BST_UNCHECKED,0);
            if(p>=PARAM_ROUNDING)
            {
                ::ShowWindow(edit,visible?SW_SHOWNA:SW_HIDE);::EnableWindow(edit,enabled);
                ::ShowWindow(GetDlgItem(SliderId(p)),visible?SW_SHOWNA:SW_HIDE);
                ::SendMessage(GetDlgItem(SliderId(p)),TBM_SETPOS,TRUE,SliderValue(p));
                if(::GetFocus()!=edit){CStringW value=SliderText(p);::SetWindowTextW(edit,value);}
            }
        }
        updating=false;
    }
    bool ApplySliderValue(int p)
    {
        CStringW value;::GetWindowTextW(GetDlgItem(editIds[p]),value.GetBuffer(64),64);value.ReleaseBuffer();value.Trim();
        if(value.IsEmpty()){ClearParameter(p);Update();return true;}
        int n=0;
        if(p==PARAM_ROUNDING){if(!ParsePixelValue(value,n,0,MAX_FIGURE_RADIUS))return false;}
        else
        {
            value.TrimRight(L"% ");WCHAR* end=NULL;long parsed=wcstol(value,&end,10);
            if(end==LPCWSTR(value) || *end || parsed< -100 || parsed>100)return false;n=int(parsed);
        }
        if(n!=SliderValue(p) || !ParameterHasValue(p))ParameterEntered(p);
        if(p==PARAM_ROUNDING)SetFigureRounding(int(n));else toolsModel.SetStarBend(int(n));
        ::SendMessage(GetDlgItem(SliderId(p)),TBM_SETPOS,TRUE,n);Update();return true;
    }
    static LRESULT CALLBACK RoundingEditProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR context)
    {
        auto self=reinterpret_cast<ParameterBar*>(context);int p=::GetDlgCtrlID(hwnd)==IDC_ROUNDING_EDIT?PARAM_ROUNDING:PARAM_STAR_BEND;
        if(msg==WM_GETDLGCODE)return DLGC_WANTALLKEYS;
        if(msg==WM_KEYDOWN && (wp==VK_RETURN || wp==VK_ESCAPE))
        {
            if(wp==VK_RETURN && !self->ApplySliderValue(p)){MessageBeep(MB_OK);return 0;}
            if(wp==VK_ESCAPE){CStringW value=SliderText(p);::SetWindowTextW(hwnd,value);}
            canvasWindow.SetFocus();self->Update();return 0;
        }
        if(msg==WM_CHAR && (wp==VK_RETURN || wp==VK_ESCAPE))return 0;
        if(msg==WM_KILLFOCUS){if(!self->ApplySliderValue(p))self->Update();}
        return DefSubclassProc(hwnd,msg,wp,lp);
    }
    static LRESULT CALLBACK EditTrackingProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR context)
    {
        auto self=reinterpret_cast<ParameterBar*>(context);
        int parameter=0;while(parameter<PARAM_COUNT && editIds[parameter]!=::GetDlgCtrlID(hwnd))++parameter;
        if(msg==WM_SETFOCUS)
        {
            RemovePropW(hwnd,L"ParameterTyped");
            // Reparented EDIT controls can notify their original owner. Handle
            // focus on the control itself so the new strip always reveals it.
            self->Reveal(hwnd);
        }
        if(msg==WM_KEYDOWN && wp==VK_ESCAPE)RemovePropW(hwnd,L"ParameterTyped");
        if((msg==WM_CHAR && wp!=VK_RETURN && wp!=VK_ESCAPE) || msg==WM_PASTE || msg==WM_CUT || msg==WM_CLEAR || (msg==WM_KEYDOWN && wp==VK_DELETE))SetPropW(hwnd,L"ParameterTyped",HANDLE(1));
        bool apply=msg==WM_KILLFOCUS || (msg==WM_KEYDOWN && wp==VK_RETURN);
        bool typed=GetPropW(hwnd,L"ParameterTyped")!=NULL;
        bool valid=false;
        if(apply)
        {
            WCHAR raw[64];::GetWindowTextW(hwnd,raw,_countof(raw));CStringW input(raw);input.Trim();
            if(parameter<=PARAM_HEIGHT || parameter==PARAM_ROUNDING){int value;valid=ParsePixelValue(input,value,parameter==PARAM_ROUNDING?0:1,parameter==PARAM_ROUNDING?MAX_FIGURE_RADIUS:32767);}
            else
            {
                input.TrimRight(L"%\u00b0 ");input.Replace(L',',L'.');WCHAR* end=NULL;double value=wcstod(input,&end);
                valid=!input.IsEmpty() && end!=LPCWSTR(input) && !*end && std::isfinite(value);
                if(parameter==PARAM_ANGLE)valid=valid && fabs(value)<=360000;
                else if(parameter>=PARAM_ROUNDING)valid=valid && value>=(parameter==PARAM_STAR_BEND?-100:0) && value<=100 && value==floor(value);
                else valid=valid && fabs(value)<90 && fabs(tan(value*M_PI/180))<=8;
            }
        }
        LRESULT result=DefSubclassProc(hwnd,msg,wp,lp);
        if(apply && valid && (typed || (msg==WM_KEYDOWN && ::GetFocus()!=hwnd)))
        {
            WCHAR text[64];::GetWindowTextW(hwnd,text,_countof(text));
            if(text[0])ParameterEntered(int(parameter));
            UpdateParameterBar();RemovePropW(hwnd,L"ParameterTyped");
        }
        return result;
    }
    static LRESULT CALLBACK SliderProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR)
    {
        bool star=::GetDlgCtrlID(hwnd)==IDC_STAR_BEND_SLIDER;int minimum=star?-100:0,maximum=star?100:MAX_FIGURE_RADIUS,range=maximum-minimum;
        if(msg==WM_ERASEBKGND)return 1;
        if(msg==WM_PAINT || msg==WM_PRINTCLIENT)
        {
            PAINTSTRUCT ps={};HDC dc=msg==WM_PAINT?::BeginPaint(hwnd,&ps):HDC(wp);RECT r;::GetClientRect(hwnd,&r);FillRect(dc,&r,GetSysColorBrush(COLOR_BTNFACE));
            int l=PanelPx(7),rr=max(l+1,r.right-PanelPx(7)),y=PanelPx(9);HBRUSH ink=GetSysColorBrush(registrySettings.DarkTheme?COLOR_BTNTEXT:COLOR_3DSHADOW);
            RECT groove={l,y,rr,y+max(1,PanelPx(2))};FillRect(dc,&groove,ink);
            for(int i=0;i<=4;++i){int x=l+MulDiv(rr-l,i,4);RECT tick={x,y+PanelPx(7),x+max(1,PanelPx(1)),y+PanelPx(10)};FillRect(dc,&tick,ink);}
            int x=l+MulDiv(rr-l,int(::SendMessage(hwnd,TBM_GETPOS,0,0))-minimum,range);RECT thumb={x-PanelPx(3),y-PanelPx(5),x+PanelPx(4),y+PanelPx(6)};
            FillRect(dc,&thumb,GetSysColorBrush(registrySettings.DarkTheme?COLOR_BTNTEXT:COLOR_HIGHLIGHT));
            if(::GetFocus()==hwnd)DrawFocusRect(dc,&r);if(msg==WM_PAINT)::EndPaint(hwnd,&ps);return 0;
        }
        if(msg==WM_LBUTTONDOWN || (msg==WM_MOUSEMOVE && ::GetCapture()==hwnd) || (msg==WM_LBUTTONUP && ::GetCapture()==hwnd))
        {
            if(msg==WM_LBUTTONDOWN){::SetFocus(hwnd);::SetCapture(hwnd);}
            RECT r;::GetClientRect(hwnd,&r);int l=PanelPx(7),span=max(1,r.right-2*l),n=max(minimum,min(maximum,minimum+MulDiv(GET_X_LPARAM(lp)-l,range,span)));
            ::SendMessage(hwnd,TBM_SETPOS,TRUE,n);::SendMessage(::GetParent(hwnd),WM_HSCROLL,MAKEWPARAM(TB_THUMBTRACK,n),LPARAM(hwnd));
            if(msg==WM_LBUTTONUP)::ReleaseCapture();return 0;
        }
        LRESULT result=DefSubclassProc(hwnd,msg,wp,lp);
        if(msg==WM_KEYDOWN || msg==WM_KEYUP || msg==TBM_SETPOS || msg==WM_SETFOCUS || msg==WM_KILLFOCUS)::InvalidateRect(hwnd,NULL,FALSE);
        return result;
    }
    LRESULT OnCreate(UINT,WPARAM,LPARAM,BOOL&)
    {
        toolSettingsWindow.MoveGeometryControls(m_hWnd);
        for(int p=0;p<PARAM_COUNT;++p)::CreateWindowW(p<PARAM_ROUNDING?L"BUTTON":L"STATIC",L"",
            WS_CHILD|(p<PARAM_ROUNDING?WS_TABSTOP|BS_AUTOCHECKBOX:SS_CENTERIMAGE),0,0,1,1,m_hWnd,HMENU(INT_PTR(IDC_PARAMETER_FIXED_FIRST+p)),g_hinstExe,NULL);
        HWND star=::CreateWindowW(TRACKBAR_CLASS,NULL,WS_CHILD|WS_TABSTOP|TBS_AUTOTICKS,0,0,1,1,m_hWnd,HMENU(IDC_STAR_BEND_SLIDER),g_hinstExe,NULL);
        ::SendMessage(star,TBM_SETRANGE,TRUE,MAKELPARAM(-100,100));::SendMessage(star,TBM_SETPAGESIZE,0,10);
        for(int p=PARAM_ROUNDING;p<PARAM_COUNT;++p)
        {
        HWND round=::CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",NULL,WS_CHILD|WS_TABSTOP|ES_CENTER|ES_AUTOHSCROLL,0,0,1,1,m_hWnd,HMENU(INT_PTR(editIds[p])),g_hinstExe,NULL);
        InstallNumericSuffix(round,p==PARAM_ROUNDING?L" px":L"%");SetWindowSubclass(round,RoundingEditProc,1,DWORD_PTR(this));
        SetWindowSubclass(GetDlgItem(SliderId(p)),SliderProc,31,0);
        }
        for(int p=0;p<PARAM_COUNT;++p)SetWindowSubclass(GetDlgItem(editIds[p]),EditTrackingProc,31,DWORD_PTR(this));
        Reload();return 0;
    }
    LRESULT OnSize(UINT,WPARAM,LPARAM,BOOL&){RECT r;GetClientRect(&r);Layout(r.right,true);return 0;}
    LRESULT OnCommand(UINT msg,WPARAM wp,LPARAM lp,BOOL&)
    {
        int p=LOWORD(wp)-IDC_PARAMETER_FIXED_FIRST;
        if(HIWORD(wp)==BN_CLICKED && p>=0 && p<PARAM_ROUNDING)
        {
            bool fixed=::SendMessage(HWND(lp),BM_GETCHECK,0,0)==BST_CHECKED;
            if(fixed && !ParameterHasValue(p)){::SendMessage(HWND(lp),BM_SETCHECK,BST_UNCHECKED,0);::SetFocus(GetDlgItem(editIds[p]));return 0;}
            toolSettingsWindow.ParameterCheckChanged(p,fixed);Update();return 0;
        }
        if(HIWORD(wp)==EN_SETFOCUS && lp)Reveal(HWND(lp));
        LRESULT result=toolSettingsWindow.SendMessage(msg,wp,lp);Update();return result;
    }
    LRESULT OnScroll(UINT,WPARAM wp,LPARAM lp,BOOL&)
    {
        if(!lp)
        {
            SCROLLINFO si={sizeof(si),SIF_ALL};GetScrollInfo(SB_HORZ,&si);int position=scrollX;
            switch(LOWORD(wp))
            {
            case SB_LEFT:position=0;break;case SB_RIGHT:position=si.nMax;break;
            case SB_LINELEFT:position-=PanelPx(20);break;case SB_LINERIGHT:position+=PanelPx(20);break;
            case SB_PAGELEFT:position-=si.nPage;break;case SB_PAGERIGHT:position+=si.nPage;break;
            case SB_THUMBTRACK:case SB_THUMBPOSITION:position=si.nTrackPos;break;default:return 0;
            }
            scrollX=position;RECT r;GetClientRect(&r);Layout(r.right,true);Invalidate(FALSE);return 0;
        }
        if(HWND(lp)==GetDlgItem(IDC_STAR_BEND_SLIDER))
        { ParameterEntered(PARAM_STAR_BEND);toolsModel.SetStarBend(int(::SendMessage(HWND(lp),TBM_GETPOS,0,0)));Update();return 0; }
        return toolSettingsWindow.SendMessage(WM_VSCROLL,wp,lp);
    }
    LRESULT OnDrawItem(UINT msg,WPARAM wp,LPARAM lp,BOOL&){return toolSettingsWindow.SendMessage(msg,wp,lp);}
    LRESULT OnDestroy(UINT,WPARAM,LPARAM,BOOL&){if(font)DeleteObject(font);font=NULL;return 0;}
} parameterBar;
class ObjectActions : public CWindowImpl<ObjectActions>
{
public:
    DECLARE_WND_CLASS_EX(L"PaintObjectActions",0,COLOR_BTNFACE)
    BEGIN_MSG_MAP(ObjectActions)
        MESSAGE_HANDLER(WM_SIZE,OnSize)
        MESSAGE_HANDLER(WM_COMMAND,Forward)
        MESSAGE_HANDLER(WM_DRAWITEM,Forward)
    END_MSG_MAP()
    LRESULT OnSize(UINT,WPARAM,LPARAM,BOOL&)
    {
        RECT r;GetClientRect(&r);int side=PanelPx(25),x=max(0,(r.right-2*side)/2),y=max(0,(r.bottom-side)/2);
        for(int i=0;i<2;++i)::MoveWindow(GetDlgItem(IDC_SAVE_OBJECT_SIZE+i),x+i*side,y,side,side,TRUE);return 0;
    }
    LRESULT Forward(UINT msg,WPARAM wp,LPARAM lp,BOOL&){return toolSettingsWindow.SendMessage(msg,wp,lp);}
} objectActions;
}
void CreateParameterBar(HWND owner)
{
    parameterBar.Create(owner,CRect(0,0,1,1),NULL,WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,WS_EX_CONTROLPARENT);
    objectActions.Create(owner,CRect(0,0,1,1),NULL,WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,WS_EX_CONTROLPARENT);
    for(int i=0;i<2;++i)::SetParent(parameterBar.GetDlgItem(IDC_SAVE_OBJECT_SIZE+i),objectActions);
    toolSettingsWindow.UpdateObjectDimensions(true);toolSettingsWindow.UpdateSelectionAngle(true);UpdateParameterBar();
}
HWND ParameterBarWindow(){return parameterBar.m_hWnd;}
HWND ObjectActionsWindow(){return objectActions.m_hWnd;}
HWND ParameterControl(int id){HWND control=parameterBar.IsWindow()?parameterBar.GetDlgItem(id):NULL;return control?control:objectActions.IsWindow()?objectActions.GetDlgItem(id):NULL;}
int ParameterBarHeight(int width){return parameterBar.IsWindow()?parameterBar.Layout(width,false):0;}
void LayoutParameterBar(){if(parameterBar.IsWindow()){RECT r;parameterBar.GetClientRect(&r);parameterBar.Layout(r.right,true);}}
void UpdateParameterBar(){parameterBar.Update();}
void ReloadParameterBarLanguage(){parameterBar.Reload();}
