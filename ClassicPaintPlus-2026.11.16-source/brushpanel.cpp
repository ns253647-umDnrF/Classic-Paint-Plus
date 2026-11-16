#include "precomp.h"

void DrawSoftBrushStroke(HDC dc,const POINT* points,size_t count,int shape,int size,int hardness,COLORREF color,BYTE opacity,COLORREF replaceColor)
{
    if(!count || size<1 || size>500) return;
    DIBSECTION dib={};GetObject(GetCurrentObject(dc,OBJ_BITMAP),sizeof(dib),&dib);GdiFlush();
    if(!dib.dsBm.bmBits || dib.dsBm.bmBitsPixel!=32) return;
    CRect area(points[0].x,points[0].y,points[0].x+1,points[0].y+1);
    for(size_t i=1;i<count;++i) { area.left=min(area.left,points[i].x);area.top=min(area.top,points[i].y);area.right=max(area.right,points[i].x+1);area.bottom=max(area.bottom,points[i].y+1); }
    area.InflateRect(size/2+2,size/2+2);area.IntersectRect(area,CRect(0,0,dib.dsBm.bmWidth,dib.dsBm.bmHeight));if(area.IsRectEmpty()) return;
    std::vector<BYTE> coverage(size_t(area.Width())*area.Height(),0);
    int side=size+2,half=side/2;double radius=max(.5,size/2.0),hard=max(0,min(100,hardness))/100.0;
    std::vector<BYTE> kernel(size_t(side)*side,0);
    for(int y=0;y<side;++y) for(int x=0;x<side;++x)
    {
        // Centre even round/square tips between pixels so their diameter is exact.
        // Slanted tips stay centred on the cursor's pixel, even at 1-2 px widths.
        double offset=(size%2==0 && shape<=BrushStyleSquare)?.5:0;
        double dx=x-half+offset,dy=y-half+offset,d;
        if(shape==BrushStyleSquare) d=max(fabs(dx),fabs(dy))/radius;
        else if(shape==BrushStyleRound) d=sqrt(dx*dx+dy*dy)/radius;
        else
        { double u=(dx+(shape==BrushStyleForeSlash?-dy:dy))*.7071067811865476,v=(dx+(shape==BrushStyleForeSlash?dy:-dy))*.7071067811865476;d=max(fabs(u)/radius,fabs(v)/max(.55,radius*.14)); }
        // The nearest pixels are the tip's centre, also at zero hardness.
        if(offset)
        {
            double centre=(shape==BrushStyleRound?sqrt(.5):.5)/radius;
            d=max(0.0,(d-centre)/(1-centre));
        }
        // 100% hardness has binary coverage, including the edge. Softer kernels
        // use the same footprint and increase monotonically with hardness.
        double a=hard==1?(d<1?1:0):(d<=hard?1:d>=1?0:(1-d)/(1-hard));
        if(hard<1) a=a*a*(3-2*a);
        kernel[size_t(y)*side+x]=BYTE(a*255+.5);
    }
    auto stamp=[&](double cx,double cy)
    {
        int left=int(floor(cx+.5))-half,top=int(floor(cy+.5))-half;
        for(int y=max(area.top,top);y<min(area.bottom,top+side);++y)
            for(int x=max(area.left,left);x<min(area.right,left+side);++x)
            { BYTE& a=coverage[size_t(y-area.top)*area.Width()+x-area.left];a=max(a,kernel[size_t(y-top)*side+x-left]); }
    };
    stamp(points[0].x,points[0].y);double spacing=max(1.0,size*.08),distanceToNext=spacing;
    for(size_t i=1;i<count;++i)
    {
        double dx=points[i].x-points[i-1].x,dy=points[i].y-points[i-1].y,length=sqrt(dx*dx+dy*dy);
        if(length==0) continue;
        for(;distanceToNext<=length;distanceToNext+=spacing) stamp(points[i-1].x+dx*distanceToNext/length,points[i-1].y+dy*distanceToNext/length);
        distanceToNext-=length;
    }
    stamp(points[count-1].x,points[count-1].y);
    DWORD rgb=RGB(GetBValue(color),GetGValue(color),GetRValue(color));
    for(int y=area.top;y<area.bottom;++y)
    {
        DWORD* row=reinterpret_cast<DWORD*>(static_cast<BYTE*>(dib.dsBm.bmBits)+size_t(dib.dsBm.bmHeight-1-y)*dib.dsBm.bmWidthBytes);
        for(int x=area.left;x<area.right;++x)
        {
            unsigned cov=coverage[size_t(y-area.top)*area.Width()+x-area.left];if(!cov) continue;
            DWORD& d=row[x];unsigned da=255-(d>>24);
            if(replaceColor!=CLR_INVALID && (d&0xffffff)!=RGB(GetBValue(replaceColor),GetGValue(replaceColor),GetRValue(replaceColor))) continue;
            if(!opacity) { unsigned a=(da*(255-cov)+127)/255;d=(d&0xffffff)|((255-a)<<24);continue; }
            unsigned sa=(cov*opacity+127)/255,weight=sa*255+da*(255-sa);if(!weight) continue;
            DWORD result=(255-(weight+127)/255)<<24;
            for(int c=0;c<3;++c) result|=(((rgb>>(c*8)&255)*sa*255+(d>>(c*8)&255)*da*(255-sa)+weight/2)/weight)<<(c*8);
            d=result;
        }
    }
}
namespace {
bool Eraser() { return toolsModel.GetActiveTool()==TOOL_RUBBER; }
int BrushSize() { return Eraser()?toolsModel.GetEraserSize():toolsModel.GetBrushWidth(); }
int BrushShape() { return Eraser()?int(registrySettings.EraserShape):int(toolsModel.GetBrushStyle()); }
DWORD& BrushHardness() { return Eraser()?registrySettings.EraserHardness:registrySettings.BrushHardness; }
void SetSize(int value) { if(Eraser()) toolsModel.SetEraserSize(value);else toolsModel.SetBrushWidth(value); }
void SetShape(int value) { if(Eraser()) toolsModel.SetEraserShape(value);else toolsModel.SetBrushStyle(BrushStyle(value)); }
CStringW Label(UINT id) { CStringW s;s.LoadString(g_hinstExe,id);return s; }
int Preset(int shape,int index) { const int sizes[2][3]={{7,4,1},{8,5,2}};return sizes[shape==0?0:1][index]; }
class BrushPanel:public CWindowImpl<BrushPanel>
{
    HWND presets=NULL,sizeEdit=NULL,hardnessSlider=NULL,hardnessLabel=NULL,tip=NULL;
    int listedShape=-1;
    HFONT font=NULL;bool changing=false;
    CStringW labels[6];
public:
    DECLARE_WND_CLASS_EX(L"PaintBrushSettings",0,COLOR_BTNFACE)
    void ReloadLanguage()
    {
        ::SendMessage(tip,TTM_POP,0,0);
        for(int i=0;i<6;++i)
        {
            labels[i]=Label(i<4?IDS_BRUSH_ROUND+i:i==4?IDS_BRUSH_SIZE:IDS_BRUSH_HARDNESS);
            HWND target=i<4?HWND(GetDlgItem(IDC_BRUSH_SHAPE_FIRST+i)):i==4?sizeEdit:hardnessSlider;
            TOOLINFOW info={sizeof(info)};info.hwnd=m_hWnd;info.uFlags=TTF_IDISHWND;info.uId=UINT_PTR(target);
            info.lpszText=labels[i].GetBuffer();::SendMessage(tip,TTM_UPDATETIPTEXTW,0,LPARAM(&info));labels[i].ReleaseBuffer();
            if(i<4 || i==5) ::SetWindowTextW(target,labels[i]);
        }
    }
    BEGIN_MSG_MAP(BrushPanel)
        MESSAGE_HANDLER(WM_CREATE,OnCreate)
        MESSAGE_HANDLER(WM_COMMAND,OnCommand)
        MESSAGE_HANDLER(WM_VSCROLL,OnScroll)
        MESSAGE_HANDLER(WM_DRAWITEM,OnDraw)
        MESSAGE_HANDLER(WM_PAINT,OnPaint)
        MESSAGE_HANDLER(WM_DESTROY,OnDestroy)
    END_MSG_MAP()
    void Sync()
    {
        if(!IsWindow()) return;changing=true;
        ShowWindow((toolsModel.GetActiveTool()==TOOL_BRUSH || Eraser())?SW_SHOWNA:SW_HIDE);
        SetWindowPos(NULL,0,0,PanelPx(50),PanelPx(242),SWP_NOZORDER|SWP_NOACTIVATE);
        for(int i=0;i<4;++i) ::MoveWindow(GetDlgItem(IDC_BRUSH_SHAPE_FIRST+i),PanelPx(3+i%2*22),PanelPx(3+i/2*22),PanelPx(22),PanelPx(22),TRUE);
        ::MoveWindow(presets,PanelPx(3),PanelPx(51),PanelPx(44),PanelPx(19),TRUE);
        RECT listRect;::GetClientRect(presets,&listRect);
        ::SendMessage(presets,LB_SETITEMHEIGHT,0,listRect.bottom);
        ::SendMessage(presets,LB_SETCOLUMNWIDTH,listRect.right/3,0);
        if(listedShape!=BrushShape())
        {
            ::SendMessage(presets,WM_SETREDRAW,FALSE,0);::SendMessage(presets,LB_RESETCONTENT,0,0);
            listedShape=BrushShape();for(int i=0;i<3;++i) { CStringW value;value.Format(L"%d",Preset(listedShape,i));::SendMessageW(presets,LB_ADDSTRING,0,LPARAM(LPCWSTR(value))); }
            ::SendMessage(presets,WM_SETREDRAW,TRUE,0);
        }
        int selected=-1;for(int i=0;i<3;++i) if(Preset(listedShape,i)==BrushSize()) selected=i;
        ::SendMessage(presets,LB_SETCURSEL,selected,0);
        ::MoveWindow(sizeEdit,PanelPx(3),PanelPx(76),PanelPx(44),PanelPx(21),TRUE);
        ::MoveWindow(hardnessSlider,PanelPx(3),PanelPx(103),PanelPx(44),PanelPx(70),TRUE);
        ::MoveWindow(hardnessLabel,PanelPx(3),PanelPx(176),PanelPx(44),PanelPx(16),TRUE);
        ::MoveWindow(GetDlgItem(IDC_BRUSH_PREVIEW),PanelPx(3),PanelPx(195),PanelPx(44),PanelPx(44),TRUE);
        LOGFONTW lf={};GetObject(GetStockObject(DEFAULT_GUI_FONT),sizeof(lf),&lf);lf.lfHeight=-PanelPx(11);
        HFONT next=CreateFontIndirectW(&lf);for(HWND control:{sizeEdit,hardnessLabel}) ::SendMessage(control,WM_SETFONT,reinterpret_cast<WPARAM>(next),TRUE);
        CompactPanelEdit(sizeEdit);
        if(font) DeleteObject(font);font=next;
        ::SendMessage(hardnessSlider,TBM_SETPOS,TRUE,100-BrushHardness());
        CStringW number;number.Format(L"%d",BrushSize());if(::GetFocus()!=sizeEdit) ::SetWindowTextW(sizeEdit,number);
        number.Format(L"%u%%",BrushHardness());::SetWindowTextW(hardnessLabel,number);
        RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);changing=false;
    }
    bool ApplySize()
    {
        WCHAR text[32];::GetWindowTextW(sizeEdit,text,_countof(text));int value;
        if(!ParsePixelValue(text,value,1,500)) return false;SetSize(value);return true;
    }
    static LRESULT CALLBACK EditProc(HWND window,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR context)
    {
        auto self=reinterpret_cast<BrushPanel*>(context);
        if(msg==WM_GETDLGCODE) return DLGC_WANTALLKEYS;
        if(msg==WM_KEYDOWN && (wp==VK_RETURN || wp==VK_ESCAPE))
        {
            if(wp==VK_RETURN && !self->ApplySize()) { MessageBeep(MB_OK);::SendMessage(window,EM_SETSEL,0,-1);return 0; }
            canvasWindow.SetFocus();self->Sync();return 0;
        }
        if(msg==WM_CHAR && (wp==VK_RETURN || wp==VK_ESCAPE)) return 0;
        if(msg==WM_KILLFOCUS) { if(!self->ApplySize()) { CStringW s;s.Format(L"%d",BrushSize());::SetWindowTextW(window,s); } }
        return DefSubclassProc(window,msg,wp,lp);
    }
    LRESULT OnCreate(UINT,WPARAM,LPARAM,BOOL&)
    {
        for(int i=0;i<4;++i) CreateWindowW(L"BUTTON",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,1,1,m_hWnd,
            reinterpret_cast<HMENU>(INT_PTR(IDC_BRUSH_SHAPE_FIRST+i)),g_hinstExe,NULL);
        presets=CreateWindowExW(WS_EX_CLIENTEDGE,L"LISTBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_MULTICOLUMN|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,
            0,0,1,1,m_hWnd,(HMENU)IDC_BRUSH_PRESET_FIRST,g_hinstExe,NULL);
        hardnessSlider=CreateWindowW(TRACKBAR_CLASSW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_VERT|TBS_NOTICKS,0,0,1,1,m_hWnd,(HMENU)IDC_BRUSH_HARDNESS,g_hinstExe,NULL);
        ::SendMessage(hardnessSlider,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
        InstallPanelTrackbar(hardnessSlider);
        sizeEdit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"4",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_CENTER|ES_NUMBER,0,0,1,1,m_hWnd,(HMENU)IDC_BRUSH_SIZE_EDIT,g_hinstExe,NULL);
        InstallPixelUnits(sizeEdit);SetWindowSubclass(sizeEdit,EditProc,1,reinterpret_cast<DWORD_PTR>(this));
        hardnessLabel=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_CENTER,0,0,1,1,m_hWnd,NULL,g_hinstExe,NULL);
        CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_OWNERDRAW,0,0,1,1,m_hWnd,(HMENU)IDC_BRUSH_PREVIEW,g_hinstExe,NULL);
        tip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,NULL,WS_POPUP|TTS_ALWAYSTIP,0,0,0,0,m_hWnd,NULL,g_hinstExe,NULL);
        ::SendMessage(tip,TTM_SETMAXTIPWIDTH,0,250);
        for(int i=0;i<6;++i)
        {
            labels[i]=Label(i<4?IDS_BRUSH_ROUND+i:i==4?IDS_BRUSH_SIZE:IDS_BRUSH_HARDNESS);
            TOOLINFOW info={sizeof(info)};info.hwnd=m_hWnd;info.uFlags=TTF_IDISHWND|TTF_SUBCLASS;
            HWND target=i<4?HWND(GetDlgItem(IDC_BRUSH_SHAPE_FIRST+i)):i==4?sizeEdit:hardnessSlider;
            info.uId=reinterpret_cast<UINT_PTR>(target);
            info.lpszText=const_cast<LPWSTR>(LPCWSTR(labels[i]));::SendMessage(tip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&info));
        }
        return 0;
    }
    LRESULT OnCommand(UINT,WPARAM wp,LPARAM,BOOL&)
    {
        if(changing) return 0;int id=LOWORD(wp);
        if(id>=IDC_BRUSH_SHAPE_FIRST && id<IDC_BRUSH_SHAPE_FIRST+4) SetShape(id-IDC_BRUSH_SHAPE_FIRST);
        if(id==IDC_BRUSH_PRESET_FIRST && HIWORD(wp)==LBN_SELCHANGE)
        { int choice=int(::SendMessage(presets,LB_GETCURSEL,0,0));if(choice>=0 && choice<3) SetSize(Preset(BrushShape(),choice)); }
        return 0;
    }
    LRESULT OnScroll(UINT,WPARAM,LPARAM lp,BOOL&)
    {
        if(HWND(lp)==hardnessSlider) { BrushHardness()=100-DWORD(::SendMessage(hardnessSlider,TBM_GETPOS,0,0));toolsModel.NotifyToolSettingsChanged(); }
        Sync();return 0;
    }
    LRESULT OnDraw(UINT,WPARAM,LPARAM lp,BOOL&)
    {
        const auto& item=*reinterpret_cast<DRAWITEMSTRUCT*>(lp);RECT r=item.rcItem;int shape=BrushShape(),size=BrushSize();
        if(item.CtlID==IDC_BRUSH_PREVIEW)
        {
            FillRect(item.hDC,&r,GetSysColorBrush(COLOR_BTNFACE));if(size>r.right-r.left-2) return TRUE;
            HBITMAP bitmap=CreateColorDIB(r.right,r.bottom,GetSysColor(COLOR_BTNFACE));HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,bitmap);
            POINT p={(r.right-r.left)/2,(r.bottom-r.top)/2};DrawSoftBrushStroke(dc,&p,1,shape,size,BrushHardness(),paletteModel.GetFgColor(),paletteModel.GetFgAlpha());
            FlattenCanvasSurface(dc);
            BitBlt(item.hDC,0,0,r.right,r.bottom,dc,0,0,SRCCOPY);SelectObject(dc,old);DeleteDC(dc);DeleteObject(bitmap);return TRUE;
        }
        bool preset=item.CtlID==IDC_BRUSH_PRESET_FIRST;
        if(preset && item.itemID>=3) return TRUE;
        bool selected;
        if(preset) { size=Preset(shape,item.itemID);selected=size==BrushSize(); }
        else { shape=item.CtlID-IDC_BRUSH_SHAPE_FIRST;selected=shape==BrushShape();size=11; }
        FillRect(item.hDC,&r,GetSysColorBrush(selected?COLOR_HIGHLIGHT:COLOR_BTNFACE));
        if(!preset) DrawFrameControl(item.hDC,&r,DFC_BUTTON,DFCS_BUTTONPUSH|(selected?DFCS_PUSHED:0));
        int x=(r.left+r.right)/2,y=(r.top+r.bottom)/2;
        COLORREF ink=GetSysColor(preset && selected?COLOR_HIGHLIGHTTEXT:COLOR_BTNTEXT);
        if(preset && shape==BrushStyleRound && size==1)
        {
            // GDI's round brush path loses a zero-length stroke at scaled 2/3 px.
            // A 1 px sample stays one physical pixel at every panel scale.
            int side=1;RECT dot={x-side/2,y-side/2,x-side/2+side,y-side/2+side};
            HBRUSH brush=CreateSolidBrush(ink);FillRect(item.hDC,&dot,brush);DeleteObject(brush);
        }
        else Brush(item.hDC,x,y,x,y,ink,shape,PanelPx(size));
        if(selected && !preset) { InflateRect(&r,-1,-1);FrameRect(item.hDC,&r,GetSysColorBrush(COLOR_HIGHLIGHT)); }
        if((item.itemState&ODS_FOCUS) && preset) { InflateRect(&r,-1,-1);DrawFocusRect(item.hDC,&r); }
        return TRUE;
    }
    LRESULT OnPaint(UINT,WPARAM,LPARAM,BOOL&)
    {
        PAINTSTRUCT ps;HDC dc=BeginPaint(&ps);RECT r;GetClientRect(&r);FillRect(dc,&r,GetSysColorBrush(COLOR_BTNFACE));
        EndPaint(&ps);return 0;
    }
    LRESULT OnDestroy(UINT,WPARAM,LPARAM,BOOL&) { if(font) DeleteObject(font);if(tip) ::DestroyWindow(tip);return 0; }
};
BrushPanel panel;
}
void CreateBrushPanel(HWND owner)
{
    int size=registrySettings.BrushSize,shape=registrySettings.BrushShape;
    toolsModel.SetBrushWidth(size);toolsModel.SetBrushStyle(BrushStyle(shape));
    panel.Create(owner,CRect(0,0,PanelPx(50),PanelPx(242)),NULL,WS_CHILD|WS_CLIPCHILDREN,WS_EX_CONTROLPARENT,IDC_BRUSH_PANEL);panel.Sync();
}
void UpdateBrushPanel() { panel.Sync(); }
void ReloadBrushLanguage() { if(panel.IsWindow()) panel.ReloadLanguage(); }
