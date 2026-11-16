/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Window procedure of the tool settings window
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 *             Copyright 2018 Stanislav Motylkov <x86corez@gmail.com>
 *             Copyright 2021-2023 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
 */

#include <cmath>
#include "precomp.h"

#define X_TOOLSETTINGS  0
#define Y_TOOLSETTINGS  (40 + CY_TOOLBAR + 31)
#define CX_TOOLSETTINGS CX_TOOLBAR
#define CY_TOOLSETTINGS 170

#define CX_TRANS_ICON 35
#define CY_TRANS_ICON 23
#define MARGIN1 3
#define MARGIN2 2

#define MAX_ZOOM_TRACK 7
#define MIN_ZOOM_TRACK 0
#define DEFAULT_ZOOM_TRACK 4

static const BYTE s_AirRadius[8] = { 5, 8, 3, 12, 10, 14, 16, 16 };
static const BYTE s_AirStyle[8] = { 0,0,0,0,1,2,3,4 };
static const int s_ZoomPresets[4] = { 1000,2000,6000,8000 };
static const int s_ZoomSteps[8] = { 125,250,500,1000,2000,4000,6000,8000 };

CToolSettingsWindow toolSettingsWindow;

/* FUNCTIONS ********************************************************/

BOOL CToolSettingsWindow::DoCreate(HWND hwndParent)
{
    RECT toolSettingsWindowPos =
    {
        X_TOOLSETTINGS, Y_TOOLSETTINGS,
        X_TOOLSETTINGS + CX_TOOLSETTINGS, Y_TOOLSETTINGS + CY_TOOLSETTINGS
    };
    return !!Create(toolBoxContainer, toolSettingsWindowPos, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);
}

static INT
getSplitRects(RECT *rects, INT cColumns, INT cRows, LPCRECT prc, LPPOINT ppt)
{
    INT cx = prc->right - prc->left, cy = prc->bottom - prc->top;
    for (INT i = 0, iRow = 0; iRow < cRows; ++iRow)
    {
        for (INT iColumn = 0; iColumn < cColumns; ++iColumn)
        {
            RECT& rc = rects[i];
            rc.left = prc->left + (iColumn * cx / cColumns);
            rc.top = prc->top + (iRow * cy / cRows);
            rc.right = prc->left + ((iColumn + 1) * cx / cColumns);
            rc.bottom = prc->top + ((iRow + 1) * cy / cRows);
            if (ppt && ::PtInRect(&rc, *ppt))
                return i;
            ++i;
        }
    }
    return -1;
}

static inline INT getTransRects(RECT rects[2], LPCRECT prc, LPPOINT ppt = NULL)
{
    return getSplitRects(rects, 1, 2, prc, ppt);
}

VOID CToolSettingsWindow::drawTrans(HDC hdc, LPCRECT prc)
{
    RECT rc[2];
    getTransRects(rc, prc);

    ::FillRect(hdc, &rc[toolsModel.IsBackgroundTransparent()], GetSysColorBrush(COLOR_HIGHLIGHT));
    for(int i=0;i<2;++i)
    {
        int width=PanelPx(CX_TRANS_ICON),height=PanelPx(PanelPercent()==100?CY_TRANS_ICON:24);
        int x=(rc[i].left+rc[i].right-width)/2,y=(rc[i].top+rc[i].bottom-height)/2;
        if(PanelPercent()==100)
            DrawIconEx(hdc,x,y,i?m_hTranspIcon:m_hNontranspIcon,width,height,0,NULL,DI_NORMAL);
        else
        {
            RECT icon={x,y,x+width,y+height};
            DrawPanelTransparency(hdc,icon,i!=0);
        }
    }
}

static inline INT getRubberRects(RECT rects[4], LPCRECT prc, LPPOINT ppt = NULL)
{
    return getSplitRects(rects, 1, 4, prc, ppt);
}

VOID CToolSettingsWindow::drawRubber(HDC hdc, LPCRECT prc)
{
    RECT rects[4], rcRubber;
    getRubberRects(rects, prc);
    INT xCenter = (prc->left + prc->right) / 2;
    for (INT i = 0; i < 4; i++)
    {
        INT iColor, radius = i + 2;
        if (toolsModel.GetRubberRadius() == radius)
        {
            ::FillRect(hdc, &rects[i], ::GetSysColorBrush(COLOR_HIGHLIGHT));
            iColor = COLOR_HIGHLIGHTTEXT;
        }
        else
        {
            iColor = COLOR_WINDOWTEXT;
        }

        INT yCenter = (rects[i].top + rects[i].bottom) / 2;
        radius=PanelPx(radius);
        rcRubber.left = xCenter - radius;
        rcRubber.top = yCenter - radius;
        rcRubber.right = rcRubber.left + radius * 2;
        rcRubber.bottom = rcRubber.top + radius * 2;
        ::FillRect(hdc, &rcRubber, GetSysColorBrush(iColor));
    }
}

static inline INT getBrushRects(RECT rects[12], LPCRECT prc, LPPOINT ppt = NULL)
{
    return getSplitRects(rects, 3, 4, prc, ppt);
}

struct BrushStyleAndWidth
{
    BrushStyle style;
    INT width;
};

static const BrushStyleAndWidth c_BrushPresets[] =
{
    { BrushStyleRound,     7 }, { BrushStyleRound,     4 }, { BrushStyleRound,     1 },
    { BrushStyleSquare,    8 }, { BrushStyleSquare,    5 }, { BrushStyleSquare,    2 },
    { BrushStyleForeSlash, 8 }, { BrushStyleForeSlash, 5 }, { BrushStyleForeSlash, 2 },
    { BrushStyleBackSlash, 8 }, { BrushStyleBackSlash, 5 }, { BrushStyleBackSlash, 2 },
};

VOID CToolSettingsWindow::drawBrush(HDC hdc, LPCRECT prc)
{
    RECT rects[12];
    getBrushRects(rects, prc);

    for (INT i = 0; i < 12; i++)
    {
        RECT rcItem = rects[i];
        INT x = (rcItem.left + rcItem.right) / 2, y = (rcItem.top + rcItem.bottom) / 2;

        INT iColor;
        const BrushStyleAndWidth& data = c_BrushPresets[i];
        if (data.width == toolsModel.GetBrushWidth() && data.style == toolsModel.GetBrushStyle())
        {
            iColor = COLOR_HIGHLIGHTTEXT;
            ::FillRect(hdc, &rcItem, GetSysColorBrush(COLOR_HIGHLIGHT));
        }
        else
        {
            iColor = COLOR_WINDOWTEXT;
        }

        Brush(hdc, x, y, x, y, ::GetSysColor(iColor), data.style, PanelPx(data.width));
    }
}

static inline INT getLineRects(RECT rects[5], LPCRECT prc, LPPOINT ppt = NULL)
{
    return getSplitRects(rects, 1, 5, prc, ppt);
}

VOID CToolSettingsWindow::drawLine(HDC hdc, LPCRECT prc)
{
    RECT rects[5];
    getLineRects(rects, prc);

    for (INT i = 0; i < 5; i++)
    {
        INT penWidth = i + 1;
        CRect rcLine = rects[i];
        rcLine.InflateRect(-PanelPx(2), 0);
        rcLine.top = (rcLine.top + rcLine.bottom - PanelPx(penWidth)) / 2;
        rcLine.bottom = rcLine.top + PanelPx(penWidth);
        if (toolsModel.GetLineWidth() == penWidth)
        {
            ::FillRect(hdc, &rects[i], ::GetSysColorBrush(COLOR_HIGHLIGHT));
            ::FillRect(hdc, &rcLine, ::GetSysColorBrush(COLOR_HIGHLIGHTTEXT));
        }
        else
        {
            ::FillRect(hdc, &rcLine, ::GetSysColorBrush(COLOR_WINDOWTEXT));
        }
    }
}

static INT getAirBrushRects(RECT rects[8],LPCRECT prc,LPPOINT ppt=NULL)
{
    // Preserve the classic unequal cells: the small and large nozzles need
    // different widths. The additional four patterns occupy a separate box.
    for(int i=0;i<4;++i) rects[i]=*prc;
    int cx=prc->right-prc->left,cy=prc->bottom-prc->top;
    rects[0].right=rects[1].left=prc->left+cx*3/8;
    rects[0].bottom=rects[1].bottom=prc->top+cy/2;
    rects[2].top=rects[3].top=prc->top+cy/2;
    rects[2].right=rects[3].left=prc->left+cx*2/8;
    RECT extra=*prc;OffsetRect(&extra,0,PanelPx(67));getSplitRects(rects+4,2,2,&extra,NULL);
    if(ppt) for(int i=0;i<8;++i) if(PtInRect(rects+i,*ppt)) return i;
    return -1;
}
VOID CToolSettingsWindow::drawAirBrush(HDC dc,LPCRECT prc)
{
    RECT rects[8];getAirBrushRects(rects,prc);unsigned seed=2387,classicSeed=0;
    for(int i=0;i<8;++i)
    {
        RECT r=rects[i];bool selected=registrySettings.SprayStyle==s_AirStyle[i] && toolsModel.GetAirBrushRadius()==s_AirRadius[i];
        FillRect(dc,&r,GetSysColorBrush(selected?COLOR_HIGHLIGHT:COLOR_BTNFACE));
        COLORREF ink=GetSysColor(selected?COLOR_HIGHLIGHTTEXT:COLOR_BTNTEXT);
        if(i<4)
        {
            // Exactly the original three-pass preview, using a private copy of
            // the MSVC random sequence so repainting never alters a live stroke.
            int radius=PanelPx(s_AirRadius[i]),cx=(r.left+r.right)/2,cy=(r.top+r.bottom)/2;
            for(int pass=0;pass<3;++pass) for(int y=-radius;y<=radius;++y) for(int x=-radius;x<=radius;++x)
                if(x*x+y*y<=radius*radius)
                { classicSeed=classicSeed*214013u+2531011u;if(((classicSeed>>16)&32767)%radius==0) SetPixelV(dc,cx+x,cy+y,ink); }
            continue;
        }
        int radius=PanelPx(8),rx=i==7?max(1,radius/3):radius,ry=i==6?max(1,radius/3):radius;
        int count=i==4?90:i==5?9:40;
        for(int n=0;n<count;++n)
        {
            seed=seed*1664525+1013904223;double angle=seed*2*M_PI/4294967296.;seed=seed*1664525+1013904223;double distance=sqrt(seed/4294967296.);
            int x=(r.left+r.right)/2+int(lround(cos(angle)*distance*rx)),y=(r.top+r.bottom)/2+int(lround(sin(angle)*distance*ry));
            int size=i==5?PanelPx(2):max(1,PanelPx(1));RECT dot={x,y,min(r.right,x+size),min(r.bottom,y+size)};FillRect(dc,&dot,GetSysColorBrush(selected?COLOR_HIGHLIGHTTEXT:COLOR_BTNTEXT));
        }
    }
}

static inline INT getBoxRects(RECT rects[3], LPCRECT prc, LPPOINT ppt = NULL)
{
    return getSplitRects(rects, 1, 3, prc, ppt);
}

VOID CToolSettingsWindow::drawBox(HDC hdc, LPCRECT prc)
{
    CRect rects[3];
    getBoxRects(rects, prc);

    for (INT iItem = 0; iItem < 3; ++iItem)
    {
        CRect& rcItem = rects[iItem];

        if (toolsModel.GetShapeStyle() == iItem)
            ::FillRect(hdc, &rcItem, ::GetSysColorBrush(COLOR_HIGHLIGHT));

        rcItem.InflateRect(-PanelPx(5), -PanelPx(5));

        if (iItem <= 1)
        {
            COLORREF rgbPen;
            if (toolsModel.GetShapeStyle() == iItem)
                rgbPen = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
            else
                rgbPen = ::GetSysColor(COLOR_WINDOWTEXT);
            HGDIOBJ hOldBrush;
            if (iItem == 0)
                hOldBrush = ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
            else
                hOldBrush = ::SelectObject(hdc, ::GetSysColorBrush(COLOR_APPWORKSPACE));
            HGDIOBJ hOldPen = ::SelectObject(hdc, ::CreatePen(PS_SOLID, PanelScale(), rgbPen));
            ::Rectangle(hdc, rcItem.left, rcItem.top, rcItem.right, rcItem.bottom);
            ::DeleteObject(::SelectObject(hdc, hOldPen));
            ::SelectObject(hdc, hOldBrush);
        }
        else
        {
            if (toolsModel.GetShapeStyle() == iItem)
                ::FillRect(hdc, &rcItem, ::GetSysColorBrush(COLOR_HIGHLIGHTTEXT));
            else
                ::FillRect(hdc, &rcItem, ::GetSysColorBrush(COLOR_WINDOWTEXT));
        }
    }
}

LRESULT CToolSettingsWindow::OnCreate(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    /* preloading the draw transparent/nontransparent icons for later use */
    m_hNontranspIcon = (HICON)LoadImageW(g_hinstExe, MAKEINTRESOURCEW(IDI_NONTRANSPARENT),
                                         IMAGE_ICON, CX_TRANS_ICON, CY_TRANS_ICON, LR_DEFAULTCOLOR);
    m_hTranspIcon = (HICON)LoadImageW(g_hinstExe, MAKEINTRESOURCEW(IDI_TRANSPARENT),
                                      IMAGE_ICON, CX_TRANS_ICON, CY_TRANS_ICON, LR_DEFAULTCOLOR);

    CRect trackbarZoomPos, rect2;
    calculateTwoBoxes(trackbarZoomPos, rect2);
    trackbarZoomPos.InflateRect(-1, -1);

    trackbarZoom.Create(TRACKBAR_CLASS, m_hWnd, trackbarZoomPos, NULL, WS_CHILD | WS_TABSTOP | TBS_VERT | TBS_AUTOTICKS | TBS_FIXEDLENGTH);
    trackbarZoom.SendMessage(TBM_SETRANGE, TRUE, MAKELPARAM(MIN_ZOOM_TRACK, MAX_ZOOM_TRACK));
    trackbarZoom.SendMessage(TBM_SETPOS, TRUE, DEFAULT_ZOOM_TRACK);
    zoomPresets.Create(L"LISTBOX",m_hWnd,CRect(5,5,45,68),NULL,
        WS_CHILD|WS_TABSTOP|LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOINTEGRALHEIGHT,
        WS_EX_CLIENTEDGE,IDC_ZOOM_PRESETS);
    for(int value:s_ZoomPresets)
    { CStringW label;label.Format(L"%dx",value/1000);zoomPresets.SendMessage(LB_ADDSTRING,0,LPARAM(LPCWSTR(label))); }
    trackbarRounding.Create(TRACKBAR_CLASS, m_hWnd, CRect(26,72,49,137), NULL,
        WS_CHILD | WS_TABSTOP | TBS_AUTOTICKS | TBS_FIXEDLENGTH, 0, IDC_FIGURE_ROUNDING);
    trackbarRounding.SendMessage(TBM_SETRANGE, TRUE, MAKELPARAM(0,MAX_FIGURE_RADIUS));
    trackbarRounding.SendMessage(TBM_SETTICFREQ,100,0);
    trackbarRounding.SendMessage(TBM_SETPAGESIZE,0,10);
    trackbarRounding.SendMessage(TBM_SETPOS,TRUE,FigureRounding());
    InstallPanelTrackbar(trackbarZoom);
    roundingTip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,NULL,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,
        0,0,0,0,m_hWnd,NULL,g_hinstExe,NULL);
    TOOLINFOW info={sizeof(info)};info.hwnd=m_hWnd;info.uFlags=TTF_IDISHWND|TTF_SUBCLASS;
    info.uId=reinterpret_cast<UINT_PTR>(trackbarRounding.m_hWnd);info.lpszText=LPSTR_TEXTCALLBACKW;
    ::SendMessage(roundingTip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&info));
    ::SendMessage(roundingTip,TTM_SETMAXTIPWIDTH,0,300);
    UpdateRoundingLabel();
    CreateSelectionResizePanel(m_hWnd, rect2);
    CreateFigureOriginControls(m_hWnd);
    zoomLabel.LoadString(g_hinstExe,IDS_ZOOM_PERCENT);
    zoomEdit.Create(L"EDIT",m_hWnd,CRect(3,75,47,96),NULL,WS_CHILD|WS_TABSTOP|ES_CENTER|ES_AUTOHSCROLL,
        WS_EX_CLIENTEDGE,IDC_ZOOM_EDIT);
    zoomEdit.SendMessage(EM_SETLIMITTEXT,12,0);
    InstallNumericSuffix(zoomEdit,L"%");
    SetWindowSubclass(zoomEdit,ZoomEditProc,1,reinterpret_cast<DWORD_PTR>(this));
    info.uId=reinterpret_cast<UINT_PTR>(zoomEdit.m_hWnd);
    ::SendMessage(roundingTip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&info));
    transparencyLabel.LoadString(g_hinstExe,IDS_SELECTION_TRANSPARENCY);
    trackbarTransparency.Create(TRACKBAR_CLASS,m_hWnd,CRect(3,144,47,209),transparencyLabel,
        WS_CHILD|WS_TABSTOP|TBS_VERT|TBS_AUTOTICKS|TBS_FIXEDLENGTH,0,IDC_SELECTION_TRANSPARENCY);
    trackbarTransparency.SendMessage(TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
    trackbarTransparency.SendMessage(TBM_SETTICFREQ,25,0);
    trackbarTransparency.SendMessage(TBM_SETPAGESIZE,0,10);
    InstallPanelTrackbar(trackbarTransparency);
    transparencyEdit.Create(L"EDIT",m_hWnd,CRect(3,212,47,233),NULL,
        WS_CHILD|WS_TABSTOP|ES_CENTER|ES_AUTOHSCROLL,WS_EX_CLIENTEDGE,IDC_TRANSPARENCY_EDIT);
    transparencyEdit.SendMessage(EM_SETLIMITTEXT,8,0);
    InstallNumericSuffix(transparencyEdit,L"%");
    SetWindowSubclass(transparencyEdit,TransparencyEditProc,1,reinterpret_cast<DWORD_PTR>(this));
    info.uId=reinterpret_cast<UINT_PTR>(trackbarTransparency.m_hWnd);
    ::SendMessage(roundingTip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&info));
    info.uId=reinterpret_cast<UINT_PTR>(transparencyEdit.m_hWnd);
    ::SendMessage(roundingTip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&info));
    trackbarSpray.Create(TRACKBAR_CLASS,m_hWnd,CRect(3,144,47,209),NULL,
        WS_CHILD|WS_TABSTOP|TBS_VERT|TBS_AUTOTICKS|TBS_FIXEDLENGTH,0,IDC_SPRAY_SMOOTHING);
    trackbarSpray.SendMessage(TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
    trackbarSpray.SendMessage(TBM_SETTICFREQ,25,0);trackbarSpray.SendMessage(TBM_SETPAGESIZE,0,10);
    InstallPanelTrackbar(trackbarSpray);
    sprayEdit.Create(L"EDIT",m_hWnd,CRect(3,212,47,233),NULL,
        WS_CHILD|WS_TABSTOP|ES_CENTER|ES_AUTOHSCROLL,WS_EX_CLIENTEDGE,IDC_SPRAY_SMOOTHING_EDIT);
    sprayEdit.SendMessage(EM_SETLIMITTEXT,8,0);InstallNumericSuffix(sprayEdit,L"%");
    SetWindowSubclass(sprayEdit,SprayEditProc,1,reinterpret_cast<DWORD_PTR>(this));
    for(HWND control:{trackbarSpray.m_hWnd,sprayEdit.m_hWnd})
    { info.uId=reinterpret_cast<UINT_PTR>(control);::SendMessage(roundingTip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&info)); }
    UpdateSpraySmoothing(true);
    angleLabel.LoadString(g_hinstExe,IDS_SELECTION_ANGLE);
    angleEdit.Create(L"EDIT",m_hWnd,CRect(3,240,47,261),NULL,
        WS_CHILD|WS_TABSTOP|ES_CENTER|ES_AUTOHSCROLL,WS_EX_CLIENTEDGE,IDC_SELECTION_ANGLE);
    angleEdit.SendMessage(EM_SETLIMITTEXT,24,0);
    InstallNumericSuffix(angleEdit,L"\u00b0");
    SetWindowSubclass(angleEdit,AngleEditProc,1,reinterpret_cast<DWORD_PTR>(this));
    info.uId=reinterpret_cast<UINT_PTR>(angleEdit.m_hWnd);
    ::SendMessage(roundingTip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&info));
    CreateSkewControls(info);
    widthLabel.LoadString(g_hinstExe,IDS_LINE_WIDTH);
    widthEdit.Create(L"EDIT",m_hWnd,CRect(3,142,47,163),NULL,
        WS_CHILD|WS_TABSTOP|ES_CENTER|ES_AUTOHSCROLL|ES_NUMBER,WS_EX_CLIENTEDGE,IDC_LINE_WIDTH);
    InstallPixelUnits(widthEdit);
    SetWindowSubclass(widthEdit,WidthEditProc,1,reinterpret_cast<DWORD_PTR>(this));
    info.uId=reinterpret_cast<UINT_PTR>(widthEdit.m_hWnd);
    ::SendMessage(roundingTip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&info));
    for(int i=0;i<2;++i)
    {
        CWindow& edit=i?objectHeight:objectWidth;
        edit.Create(L"EDIT",m_hWnd,CRect(3,0,47,21),NULL,WS_CHILD|WS_TABSTOP|ES_CENTER|ES_AUTOHSCROLL,WS_EX_CLIENTEDGE,i?IDC_OBJECT_HEIGHT:IDC_OBJECT_WIDTH);
        InstallPixelUnits(edit);SetWindowSubclass(edit,DimensionEditProc,1,reinterpret_cast<DWORD_PTR>(this));
        info.uId=reinterpret_cast<UINT_PTR>(edit.m_hWnd);::SendMessage(roundingTip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&info));
    }
    dimensionX.Create(L"STATIC",m_hWnd,CRect(3,0,47,12),L"x",WS_CHILD|SS_CENTER);
    for(int i=0;i<2;++i)
    {
        CWindow& button=i?resetSizeButton:saveSizeButton;
        button.Create(L"BUTTON",m_hWnd,CRect(0,0,25,25),i?L"R":L"S",WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,0,IDC_SAVE_OBJECT_SIZE+i);
        info.uId=reinterpret_cast<UINT_PTR>(button.m_hWnd);
        ::SendMessage(roundingTip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&info));
    }
    CreateBrushPanel(m_hWnd);ApplyScale();UpdateZoomText();UpdateSelectionTransparency(true);UpdateSelectionAngle(true);UpdateLineWidth(true);
    return 0;
}

void CToolSettingsWindow::ApplyScale()
{
    CRect first,second;calculateTwoBoxes(first,second);
    first.InflateRect(-PanelScale(),-PanelScale());
    first.OffsetRect(0,PanelPx(70));
    trackbarZoom.MoveWindow(first);
    zoomPresets.MoveWindow(PanelPx(5),PanelPx(5),PanelPx(40),PanelPx(63));
    RECT presetClient;zoomPresets.GetClientRect(&presetClient);
    zoomPresets.SendMessage(LB_SETITEMHEIGHT,0,max(1,presetClient.bottom/4));

    trackbarSpray.MoveWindow(PanelPx(3),PanelPx(144),PanelPx(44),PanelPx(65));
    sprayEdit.MoveWindow(PanelPx(3),PanelPx(212),PanelPx(44),PanelPx(21));
    trackbarSpray.SendMessage(TBM_SETTHUMBLENGTH,PanelPx(12));UpdateSpraySmoothing();
    trackbarTransparency.MoveWindow(PanelPx(3),PanelPx(144),PanelPx(44),PanelPx(65));
    transparencyEdit.MoveWindow(PanelPx(3),PanelPx(212),PanelPx(44),PanelPx(21));
    UpdateSelectionAngle();UpdateLineWidth();UpdateObjectDimensions();
    trackbarTransparency.SendMessage(TBM_SETTHUMBLENGTH,PanelPx(12));
    trackbarZoom.SendMessage(TBM_SETTHUMBLENGTH,PanelPx(12));
    trackbarRounding.SendMessage(TBM_SETTHUMBLENGTH,PanelPx(12));
    if(zoomEdit.IsWindow())
    {
        zoomEdit.MoveWindow(PanelPx(3),PanelPx(145),PanelPx(44),PanelPx(21));
        LOGFONTW font={};GetObjectW(GetStockObject(DEFAULT_GUI_FONT),sizeof(font),&font);font.lfHeight=-PanelPx(11);
        HFONT next=CreateFontIndirectW(&font);zoomEdit.SendMessage(WM_SETFONT,reinterpret_cast<WPARAM>(next),TRUE);
        zoomPresets.SendMessage(WM_SETFONT,reinterpret_cast<WPARAM>(next),TRUE);
        if(sprayEdit.IsWindow()) sprayEdit.SendMessage(WM_SETFONT,reinterpret_cast<WPARAM>(next),TRUE);
        if(transparencyEdit.IsWindow()) transparencyEdit.SendMessage(WM_SETFONT,reinterpret_cast<WPARAM>(next),TRUE);
        if(angleEdit.IsWindow()) angleEdit.SendMessage(WM_SETFONT,reinterpret_cast<WPARAM>(next),TRUE);
        if(widthEdit.IsWindow()) widthEdit.SendMessage(WM_SETFONT,reinterpret_cast<WPARAM>(next),TRUE);
        for(HWND control:{objectWidth.m_hWnd,objectHeight.m_hWnd,dimensionX.m_hWnd,saveSizeButton.m_hWnd,resetSizeButton.m_hWnd,skewEdit[0].m_hWnd,skewEdit[1].m_hWnd,skewCaption[0].m_hWnd,skewCaption[1].m_hWnd}) ::SendMessage(control,WM_SETFONT,WPARAM(next),TRUE);
        if(panelFont) DeleteObject(panelFont);panelFont=next;
    }
    for(HWND control:{zoomEdit.m_hWnd,sprayEdit.m_hWnd,transparencyEdit.m_hWnd,widthEdit.m_hWnd})CompactPanelEdit(control);
    LayoutParameterBar();
    // Selection uses the full-width second box even when another tool is selected.
    second.right=PanelPx(45);
    LayoutSelectionResizePanel(second);
    if(roundingTip)
    {
        RECT cells[8];CRect area;CRect unused;calculateTwoBoxes(area,unused);area.InflateRect(-PanelPx(MARGIN2),-PanelPx(MARGIN2));getAirBrushRects(cells,&area);
        for(int i=0;i<8;++i)
        {
            TOOLINFOW info={sizeof(info)};info.hwnd=m_hWnd;info.uId=200+i;
            bool exists=::SendMessage(roundingTip,TTM_GETTOOLINFOW,0,LPARAM(&info))!=0;
            info.uFlags=TTF_SUBCLASS;info.rect=cells[i];if(toolsModel.GetActiveTool()!=TOOL_AIRBRUSH) SetRectEmpty(&info.rect);info.lpszText=LPSTR_TEXTCALLBACKW;
            ::SendMessage(roundingTip,exists?TTM_NEWTOOLRECTW:TTM_ADDTOOLW,0,LPARAM(&info));
        }
    }
    UpdateBrushPanel();
    Invalidate();
}

void CToolSettingsWindow::UpdateZoomText()
{
    if(!zoomEdit.IsWindow()) return;
    CStringW text;int zoom=toolsModel.GetZoom();
    int selected=-1;for(int i=0;i<4;++i) if(zoom==s_ZoomPresets[i]) selected=i;
    zoomPresets.SendMessage(LB_SETCURSEL,selected,0);
    zoomPresets.Invalidate(FALSE);
    if(zoom%10) text.Format(L"%.1f%%",zoom/10.0);else text.Format(L"%d%%",zoom/10);
    zoomEdit.SetWindowText(text);
}
bool CToolSettingsWindow::ApplyZoomText(bool finish)
{
    if(m_applyingZoom)return true;
    CStringW text;zoomEdit.GetWindowText(text);text.Trim();text.Replace(L',',L'.');
    if(!text.IsEmpty() && text[text.GetLength()-1]==L'%') { text.Delete(text.GetLength()-1);text.Trim(); }
    wchar_t* end=nullptr;double value=wcstod(text,&end);
    if(text.IsEmpty() || end==static_cast<LPCWSTR>(text) || *end || !(value>=MIN_ZOOM/10.0 && value<=MAX_ZOOM/10.0)) return false;
    int zoom=int(floor(value*10+.5));bool changed=zoom!=toolsModel.GetZoom();m_applyingZoom=true;
    canvasWindow.zoomTo(zoom);UpdateZoomText();
    if(finish || changed)toolsModel.ReturnFromZoom();m_applyingZoom=false;return true;
}
LRESULT CALLBACK CToolSettingsWindow::ZoomEditProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR context)
{
    auto self=reinterpret_cast<CToolSettingsWindow*>(context);
    if(message==WM_GETDLGCODE) return DLGC_WANTALLKEYS;
    if(message==WM_KEYDOWN && (wp==VK_RETURN || wp==VK_ESCAPE))
    {
        if(wp==VK_ESCAPE) self->UpdateZoomText();
        else if(!self->ApplyZoomText(true))
        {
            CStringW range;range.LoadString(g_hinstExe,IDS_ZOOM_RANGE);
            ::SendMessageW(g_hStatusBar,SB_SETTEXT,1,reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(range)));
            MessageBeep(MB_OK);::SendMessage(hwnd,EM_SETSEL,0,-1);return 0;
        }
        if(toolsModel.GetActiveTool()==TOOL_TEXT && ::IsWindowVisible(textEditWindow))textEditWindow.SetFocus();else canvasWindow.SetFocus();return 0;
    }
    if(message==WM_CHAR && (wp==VK_RETURN || wp==VK_ESCAPE)) return 0;
    if(message==WM_KILLFOCUS && !self->ApplyZoomText()) self->UpdateZoomText();
    return DefSubclassProc(hwnd,message,wp,lp);
}

void CToolSettingsWindow::UpdateSpraySmoothing(bool forceText)
{
    if(!sprayEdit.IsWindow()) return;
    bool visible=toolsModel.GetActiveTool()==TOOL_AIRBRUSH;
    trackbarSpray.ShowWindow(visible?SW_SHOWNA:SW_HIDE);sprayEdit.ShowWindow(visible?SW_SHOWNA:SW_HIDE);
    trackbarSpray.SendMessage(TBM_SETPOS,TRUE,100-registrySettings.SpraySmoothing);
    CStringW name;name.LoadString(g_hinstExe,IDS_SPRAY_SMOOTHING);
    spraySmoothingLabel.Format(L"%s: %u%%",LPCWSTR(name),registrySettings.SpraySmoothing);trackbarSpray.SetWindowText(spraySmoothingLabel);
    if(forceText || ::GetFocus()!=sprayEdit.m_hWnd)
    { CStringW value;value.Format(L"%u%%",registrySettings.SpraySmoothing);sprayEdit.SetWindowText(value); }
}
bool CToolSettingsWindow::ApplySprayText()
{
    CStringW text;sprayEdit.GetWindowText(text);text.Trim();text.TrimRight(L"% ");WCHAR* end=NULL;long value=wcstol(text,&end,10);
    if(text.IsEmpty() || end==LPCWSTR(text) || *end || value<0 || value>100) return false;
    if(registrySettings.SpraySmoothing!=DWORD(value))
    { registrySettings.SpraySmoothing=value;registrySettings.StorePreferences();toolsModel.NotifyToolSettingsChanged(); }
    UpdateSpraySmoothing(true);return true;
}
LRESULT CALLBACK CToolSettingsWindow::SprayEditProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR context)
{
    auto self=reinterpret_cast<CToolSettingsWindow*>(context);
    if(message==WM_GETDLGCODE) return DLGC_WANTALLKEYS;
    if(message==WM_KEYDOWN && (wp==VK_RETURN || wp==VK_ESCAPE))
    {
        if(wp==VK_ESCAPE) self->UpdateSpraySmoothing(true);
        else if(!self->ApplySprayText()) { MessageBeep(MB_OK);::SendMessage(hwnd,EM_SETSEL,0,-1);return 0; }
        canvasWindow.SetFocus();return 0;
    }
    if(message==WM_CHAR && (wp==VK_RETURN || wp==VK_ESCAPE)) return 0;
    if(message==WM_KILLFOCUS && !self->ApplySprayText()) self->UpdateSpraySmoothing(true);
    return DefSubclassProc(hwnd,message,wp,lp);
}

void CToolSettingsWindow::UpdateSelectionTransparency(bool forceText)
{
    if(!transparencyEdit.IsWindow()) return;
    bool visible=toolsModel.IsSelection();
    trackbarTransparency.ShowWindow(visible?SW_SHOWNA:SW_HIDE);
    transparencyEdit.ShowWindow(visible?SW_SHOWNA:SW_HIDE);
    bool enabled=visible && selectionModel.m_bShow && !selectionModel.m_rc.IsRectEmpty();
    trackbarTransparency.EnableWindow(enabled);transparencyEdit.EnableWindow(enabled);
    int value=selectionModel.GetTransparency();
    trackbarTransparency.SendMessage(TBM_SETPOS,TRUE,100-value);
    if(forceText || ::GetFocus()!=transparencyEdit.m_hWnd)
    { CStringW text;text.Format(L"%d%%",value);transparencyEdit.SetWindowText(text); }
}
void CToolSettingsWindow::UpdateSelectionAngle(bool forceText)
{
    if(!angleEdit.IsWindow()) return;
    bool selection=toolsModel.IsSelection(),text=toolsModel.GetActiveTool()==TOOL_TEXT,figure=IsBoundedFigureTool(toolsModel.GetActiveTool());
    bool visible=selection || text || figure,enabled=text || figure || (selection && selectionModel.m_bShow && !selectionModel.m_rc.IsRectEmpty());
    if(transformSeparator.IsWindow())transformSeparator.ShowWindow(SW_HIDE);
    UpdateSkew(forceText);
    angleLabel.LoadString(g_hinstExe,selection?IDS_SELECTION_ANGLE:IDS_OBJECT_ANGLE);
    angleEdit.ShowWindow(visible?SW_SHOWNA:SW_HIDE);angleEdit.EnableWindow(enabled);
    if(forceText || selectionModel.IsRotating() || ::GetFocus()!=angleEdit.m_hWnd)
    {
        double angle=selection?selectionModel.RotationAngle():text?textEditWindow.Angle():toolsModel.FigureAngle();
        CStringW number;number.Format(L"%.2f",angle);
        number.TrimRight(L'0');number.TrimRight(L'.');number+=L"°";
        if(!ParameterHasValue(PARAM_ANGLE))number.Empty();
        lastAngleText=number;angleEdit.SetWindowText(number);
    }
    UpdateParameterBar();
}
bool CToolSettingsWindow::ApplyAngleText()
{
    if(!angleEdit.IsWindowEnabled()) return false;
    CStringW text;angleEdit.GetWindowText(text);
    text.Trim();text.Replace(L',',L'.');if(!text.IsEmpty() && text[text.GetLength()-1]==L'°') { text.Delete(text.GetLength()-1);text.Trim(); }
    if(text.IsEmpty()) { ClearParameter(PARAM_ANGLE);UpdateSelectionAngle(true);return true; }
    CStringW displayed(lastAngleText);displayed.TrimRight(L"° ");
    if(text==displayed) return true; // Focus-only suffix changes must not round the stored angle.
    WCHAR* end=NULL;double value=wcstod(text,&end);
    if(text.IsEmpty() || end==LPCWSTR(text) || *end || !std::isfinite(value) || fabs(value)>360000) return false;
    bool applied=toolsModel.IsSelection()?selectionModel.SetRotationAngle(value):
        toolsModel.GetActiveTool()==TOOL_TEXT?textEditWindow.SetRotationAngle(value):toolsModel.SetFigureAngle(value);
    if(!applied) return false;
    ParameterEntered(PARAM_ANGLE);UpdateSelectionAngle(true);return true;
}
LRESULT CALLBACK CToolSettingsWindow::AngleEditProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR context)
{
    auto self=reinterpret_cast<CToolSettingsWindow*>(context);
    if(message==WM_GETDLGCODE) return DLGC_WANTALLKEYS;
    if(message==WM_KEYDOWN && (wp==VK_RETURN || wp==VK_ESCAPE))
    {
        if(wp==VK_ESCAPE) self->UpdateSelectionAngle(true);
        else if(!self->ApplyAngleText()) { MessageBeep(MB_OK);::SendMessage(hwnd,EM_SETSEL,0,-1);return 0; }
        if(toolsModel.GetActiveTool()==TOOL_TEXT && ::IsWindowVisible(textEditWindow)) textEditWindow.SetFocus();else canvasWindow.SetFocus();return 0;
    }
    if(message==WM_CHAR && (wp==VK_RETURN || wp==VK_ESCAPE)) return 0;
    if(message==WM_KILLFOCUS && !self->ApplyAngleText()) self->UpdateSelectionAngle(true);
    return DefSubclassProc(hwnd,message,wp,lp);
}
void CToolSettingsWindow::UpdateLineWidth(bool forceText)
{
    if(!widthEdit.IsWindow()) return;
    int tool=toolsModel.GetActiveTool();bool visible=tool>=TOOL_LINE;
    widthEdit.MoveWindow(PanelPx(3),PanelPx(tool<=TOOL_BEZIER?75:142),PanelPx(44),PanelEditHeight(widthEdit),FALSE);
    widthEdit.ShowWindow(visible?SW_SHOWNA:SW_HIDE);
    if(forceText || ::GetFocus()!=widthEdit.m_hWnd)
    { CStringW text;text.Format(L"%d",toolsModel.GetLineWidth());widthEdit.SetWindowText(text); }
}
bool CToolSettingsWindow::ApplyWidthText()
{
    CStringW text;widthEdit.GetWindowText(text);int value;if(!ParsePixelValue(text,value,1,500)) return false;
    if(toolsModel.GetLineWidth()!=value) toolsModel.SetLineWidth(int(value));
    UpdateLineWidth(true);return true;
}
LRESULT CALLBACK CToolSettingsWindow::WidthEditProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR context)
{
    auto self=reinterpret_cast<CToolSettingsWindow*>(context);
    if(message==WM_GETDLGCODE) return DLGC_WANTALLKEYS;
    if(message==WM_KEYDOWN && (wp==VK_RETURN || wp==VK_ESCAPE))
    {
        if(wp==VK_ESCAPE) self->UpdateLineWidth(true);
        else if(!self->ApplyWidthText()) { MessageBeep(MB_OK);::SendMessage(hwnd,EM_SETSEL,0,-1);return 0; }
        canvasWindow.SetFocus();return 0;
    }
    if(message==WM_CHAR && (wp==VK_RETURN || wp==VK_ESCAPE)) return 0;
    if(message==WM_KILLFOCUS && !self->ApplyWidthText()) self->UpdateLineWidth(true);
    return DefSubclassProc(hwnd,message,wp,lp);
}
void CToolSettingsWindow::ReloadLanguage()
{
    zoomLabel.LoadString(g_hinstExe,IDS_ZOOM_PERCENT);transparencyLabel.LoadString(g_hinstExe,IDS_SELECTION_TRANSPARENCY);
    UpdateObjectDimensions();UpdateSelectionAngle();widthLabel.LoadString(g_hinstExe,IDS_LINE_WIDTH);UpdateRoundingLabel();trackbarTransparency.SetWindowText(transparencyLabel);
    ::SendMessage(roundingTip,TTM_POP,0,0);
    ReloadSelectionResizeLanguage();ReloadBrushLanguage();ReloadFigureLanguage();ReloadParameterBarLanguage();UpdateSpraySmoothing();
}
bool CToolSettingsWindow::ApplyTransparencyText()
{
    if(!transparencyEdit.IsWindowEnabled()) return false;
    CStringW text;transparencyEdit.GetWindowText(text);text.Trim();
    if(!text.IsEmpty() && text[text.GetLength()-1]==L'%') { text.Delete(text.GetLength()-1);text.Trim(); }
    WCHAR* end=NULL;long value=wcstol(text,&end,10);
    if(text.IsEmpty() || end==static_cast<LPCWSTR>(text) || *end || value<0 || value>100) return false;
    if(!selectionModel.SetTransparency(int(value))) return false;
    UpdateSelectionTransparency(true);return true;
}
LRESULT CALLBACK CToolSettingsWindow::TransparencyEditProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR context)
{
    auto self=reinterpret_cast<CToolSettingsWindow*>(context);
    if(message==WM_GETDLGCODE) return DLGC_WANTALLKEYS;
    if(message==WM_KEYDOWN && (wp==VK_RETURN || wp==VK_ESCAPE))
    {
        if(wp==VK_ESCAPE) self->UpdateSelectionTransparency(true);
        else if(!self->ApplyTransparencyText())
        { MessageBeep(MB_OK);::SendMessage(hwnd,EM_SETSEL,0,-1);return 0; }
        canvasWindow.SetFocus();return 0;
    }
    if(message==WM_CHAR && (wp==VK_RETURN || wp==VK_ESCAPE)) return 0;
    if(message==WM_KILLFOCUS && !self->ApplyTransparencyText()) self->UpdateSelectionTransparency(true);
    return DefSubclassProc(hwnd,message,wp,lp);
}

LRESULT CToolSettingsWindow::OnDestroy(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    ::DestroyIcon(m_hNontranspIcon);
    ::DestroyIcon(m_hTranspIcon);
    if(roundingTip) ::DestroyWindow(roundingTip);
    if(panelFont) DeleteObject(panelFont);
    return 0;
}

LRESULT CToolSettingsWindow::OnVScroll(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if(reinterpret_cast<HWND>(lParam)==trackbarSpray.m_hWnd)
    {
        registrySettings.SpraySmoothing=100-(DWORD)trackbarSpray.SendMessage(TBM_GETPOS,0,0);UpdateSpraySmoothing(true);
        toolsModel.NotifyToolSettingsChanged();
        if(LOWORD(wParam)!=TB_THUMBTRACK) registrySettings.StorePreferences();
        ::SendMessageW(g_hStatusBar,SB_SETTEXT,1,LPARAM(LPCWSTR(spraySmoothingLabel)));return 0;
    }
    if(reinterpret_cast<HWND>(lParam)==trackbarTransparency.m_hWnd)
    {
        int value=100-(int)trackbarTransparency.SendMessage(TBM_GETPOS,0,0);
        selectionModel.SetTransparency(value);UpdateSelectionTransparency(true);
        CStringW status;status.Format(L"%s: %d%%",static_cast<LPCWSTR>(transparencyLabel),selectionModel.GetTransparency());
        ::SendMessageW(g_hStatusBar,SB_SETTEXT,1,reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(status)));
        return 0;
    }
    if(reinterpret_cast<HWND>(lParam)==trackbarRounding.m_hWnd)
    {
        ParameterEntered(PARAM_ROUNDING);SetFigureRounding(int(trackbarRounding.SendMessage(TBM_GETPOS,0,0)));
        UpdateRoundingLabel();
        ::SendMessageW(g_hStatusBar,SB_SETTEXT,1,reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(roundingLabel)));
        canvasWindow.Invalidate(FALSE);
        return 0;
    }
    INT trackPos = MAX_ZOOM_TRACK - (INT)trackbarZoom.SendMessage(TBM_GETPOS, 0, 0);
    canvasWindow.zoomTo(s_ZoomSteps[max(0,min(MAX_ZOOM_TRACK,trackPos))]);

    INT zoomRate = toolsModel.GetZoom();

    CStringW strZoom;
    if (zoomRate % 10 == 0)
        strZoom.Format(L"%d%%", zoomRate / 10);
    else
        strZoom.Format(L"%d.%d%%", zoomRate / 10, zoomRate % 10);

    ::SendMessageW(g_hStatusBar, SB_SETTEXT, 1, (LPARAM)(LPCWSTR)strZoom);

    OnToolsModelZoomChanged(nMsg, wParam, lParam, bHandled);
    return 0;
}

LRESULT CToolSettingsWindow::OnNotify(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    NMHDR *pnmhdr = (NMHDR*)lParam;
    if(pnmhdr->hwndFrom==roundingTip && pnmhdr->code==TTN_GETDISPINFOW && pnmhdr->idFrom>=200 && pnmhdr->idFrom<208)
    {
        int i=int(pnmhdr->idFrom)-200;CStringW name;name.LoadString(g_hinstExe,i<4?IDS_TOOLTIP9:IDS_SPRAY_FIRST+i-4);
        sprayLabel.Format(L"%s (%d px)",LPCWSTR(name),s_AirRadius[i]*2);reinterpret_cast<NMTTDISPINFOW*>(lParam)->lpszText=sprayLabel.GetBuffer();sprayLabel.ReleaseBuffer();return 0;
    }
    if(pnmhdr->hwndFrom==roundingTip && pnmhdr->code==TTN_GETDISPINFOW)
        reinterpret_cast<NMTTDISPINFOW*>(lParam)->lpszText=const_cast<LPWSTR>(static_cast<LPCWSTR>(
            pnmhdr->idFrom==reinterpret_cast<UINT_PTR>(zoomEdit.m_hWnd)?zoomLabel:
            pnmhdr->idFrom==reinterpret_cast<UINT_PTR>(angleEdit.m_hWnd)?angleLabel:
            (pnmhdr->idFrom==reinterpret_cast<UINT_PTR>(skewEdit[0].m_hWnd) || pnmhdr->idFrom==reinterpret_cast<UINT_PTR>(skewCaption[0].m_hWnd))?skewLabel[0]:
            (pnmhdr->idFrom==reinterpret_cast<UINT_PTR>(skewEdit[1].m_hWnd) || pnmhdr->idFrom==reinterpret_cast<UINT_PTR>(skewCaption[1].m_hWnd))?skewLabel[1]:
            pnmhdr->idFrom==reinterpret_cast<UINT_PTR>(widthEdit.m_hWnd)?widthLabel:
             pnmhdr->idFrom==reinterpret_cast<UINT_PTR>(objectWidth.m_hWnd)?objectWidthLabel:
             pnmhdr->idFrom==reinterpret_cast<UINT_PTR>(objectHeight.m_hWnd)?objectHeightLabel:
             pnmhdr->idFrom==reinterpret_cast<UINT_PTR>(saveSizeButton.m_hWnd)?saveSizeLabel:
             pnmhdr->idFrom==reinterpret_cast<UINT_PTR>(resetSizeButton.m_hWnd)?resetSizeLabel:
            (pnmhdr->idFrom==reinterpret_cast<UINT_PTR>(sprayEdit.m_hWnd) || pnmhdr->idFrom==reinterpret_cast<UINT_PTR>(trackbarSpray.m_hWnd))?spraySmoothingLabel:
            (pnmhdr->idFrom==reinterpret_cast<UINT_PTR>(transparencyEdit.m_hWnd) ||
             pnmhdr->idFrom==reinterpret_cast<UINT_PTR>(trackbarTransparency.m_hWnd))?transparencyLabel:roundingLabel));
    if (pnmhdr->code == NM_CUSTOMDRAW)
    {
        NMCUSTOMDRAW *pCustomDraw = (NMCUSTOMDRAW*)pnmhdr;
        pCustomDraw->uItemState &= ~CDIS_FOCUS; // Do not draw the focus
    }
    return 0;
}

VOID CToolSettingsWindow::calculateTwoBoxes(CRect& rect1, CRect& rect2)
{
    CRect rcClient;
    GetClientRect(&rcClient);
    rcClient.right=PanelPx(50);
    rcClient.bottom = PanelPx(140); // Origin buttons are below these two boxes.
    rcClient.InflateRect(-PanelPx(MARGIN1), -PanelPx(MARGIN1));

    INT yCenter = (rcClient.top + rcClient.bottom) / 2;
    rect1.SetRect(rcClient.left, rcClient.top, rcClient.right, yCenter);
    rect2.SetRect(rcClient.left, yCenter, rcClient.right, rcClient.bottom);

    rect1.InflateRect(-PanelPx(MARGIN2), -PanelPx(MARGIN2));
    rect2.InflateRect(-PanelPx(MARGIN2), -PanelPx(MARGIN2));

}

void CToolSettingsWindow::UpdateRoundingLabel()
{
    CStringW name;name.LoadString(g_hinstExe,IDS_FIGURE_ROUNDING);
    roundingLabel.Format(L"%s: %u px",static_cast<LPCWSTR>(name),unsigned(FigureRounding()));
    trackbarRounding.SetWindowText(roundingLabel);UpdateParameterBar();
}

LRESULT CToolSettingsWindow::OnPaint(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    CRect rect1, rect2;
    calculateTwoBoxes(rect1, rect2);

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(&ps);
    CRect background;GetClientRect(&background);FillRect(hdc,&background,GetSysColorBrush(COLOR_BTNFACE));
    // Keep the original 40 px interior so the 35 px classic artwork has a
    // visible selection-colour margin. Larger panels preserve that proportion.
    if(toolsModel.IsSelection() || toolsModel.GetActiveTool()==TOOL_TEXT)
    { rect1.left=PanelPx(3);rect1.right=PanelPx(47); }

    if (toolsModel.GetActiveTool() != TOOL_ZOOM)
        ::DrawEdge(hdc, &rect1, BDR_SUNKENOUTER, BF_RECT | BF_MIDDLE);

    if (toolsModel.GetActiveTool() >= TOOL_RECT || toolsModel.GetActiveTool()==TOOL_AIRBRUSH)
        ::DrawEdge(hdc, &rect2, BDR_SUNKENOUTER, BF_RECT | BF_MIDDLE);

    rect1.InflateRect(-PanelPx(MARGIN2), -PanelPx(MARGIN2));
    rect2.InflateRect(-PanelPx(MARGIN2), -PanelPx(MARGIN2));
    switch (toolsModel.GetActiveTool())
    {
        case TOOL_FREESEL:
        case TOOL_RECTSEL:
        case TOOL_TEXT:
            drawTrans(hdc, &rect1);
            break;
        case TOOL_RUBBER:
        case TOOL_BRUSH:
            break;
        case TOOL_AIRBRUSH:
            drawAirBrush(hdc, &rect1);
            break;
        case TOOL_LINE:
        case TOOL_BEZIER:
            drawLine(hdc, &rect1);
            break;
        case TOOL_RECT:
        case TOOL_SHAPE:
        case TOOL_ELLIPSE:
        case TOOL_RRECT:
        case TOOL_FIGURE:
            drawBox(hdc, &rect1);
            drawLine(hdc, &rect2);
            break;
        case TOOL_COLOR:
            if (m_rgbPickColor != CLR_INVALID)
            {
                HBRUSH hbr = CreateSolidBrush(m_rgbPickColor);
                FillRect(hdc, &rect1, hbr);
                DeleteObject(hbr);
            }
            break;
        case TOOL_FILL:
        case TOOL_ZOOM:
        case TOOL_PEN:
            break;
    }
    EndPaint(&ps);
    return 0;
}

LRESULT CToolSettingsWindow::OnLButtonDown(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    SetFocus(); // Apply pending numeric input before selecting a new preset.
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

    CRect rect1, rect2;
    calculateTwoBoxes(rect1, rect2);
    RECT rects[12];

    INT iItem;
    switch (toolsModel.GetActiveTool())
    {
        case TOOL_FREESEL:
        case TOOL_RECTSEL:
        case TOOL_TEXT:
            iItem = getTransRects(rects, &rect1, &pt);
            if (iItem != -1)
                toolsModel.SetBackgroundTransparent(iItem);
            break;
        case TOOL_RUBBER:
            iItem = getRubberRects(rects, &rect1, &pt);
            if (iItem != -1)
                toolsModel.SetRubberRadius(iItem + 2);
            break;
        case TOOL_BRUSH:
            iItem = getBrushRects(rects, &rect1, &pt);
            if (iItem != -1)
            {
                const BrushStyleAndWidth& data = c_BrushPresets[iItem];
                toolsModel.SetBrushStyle(data.style);
                toolsModel.SetBrushWidth(data.width);
            }
            break;
        case TOOL_AIRBRUSH:
            rect1.InflateRect(-PanelPx(MARGIN2),-PanelPx(MARGIN2));
            iItem = getAirBrushRects(rects, &rect1, &pt);
            if (iItem != -1)
                { registrySettings.SprayStyle=s_AirStyle[iItem];toolsModel.SetAirBrushRadius(s_AirRadius[iItem]); }
            break;
        case TOOL_LINE:
        case TOOL_BEZIER:
            iItem = getLineRects(rects, &rect1, &pt);
            if (iItem != -1)
                toolsModel.SetLineWidth(iItem + 1);
            break;
        case TOOL_RECT:
        case TOOL_SHAPE:
        case TOOL_ELLIPSE:
        case TOOL_RRECT:
        case TOOL_FIGURE:
            iItem = getBoxRects(rects, &rect1, &pt);
            if (iItem != -1)
                toolsModel.SetShapeStyle(iItem);

            iItem = getLineRects(rects, &rect2, &pt);
            if (iItem != -1)
                toolsModel.SetLineWidth(iItem + 1);
            break;
        case TOOL_FILL:
        case TOOL_COLOR:
        case TOOL_ZOOM:
        case TOOL_PEN:
            break;
    }

    ::SetCapture(::GetParent(m_hWnd));
    return 0;
}

LRESULT CToolSettingsWindow::OnToolsModelToolChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    m_rgbPickColor = CLR_INVALID;
    Invalidate();
    trackbarZoom.ShowWindow((wParam == TOOL_ZOOM) ? SW_SHOW : SW_HIDE);
    zoomEdit.ShowWindow(wParam==TOOL_ZOOM?SW_SHOWNA:SW_HIDE);
    zoomPresets.ShowWindow(wParam==TOOL_ZOOM?SW_SHOWNA:SW_HIDE);
    trackbarRounding.SendMessage(TBM_SETPOS,TRUE,FigureRounding());
    trackbarRounding.ShowWindow(IsRoundedFigureTool()?SW_SHOWNA:SW_HIDE);
    UpdateRoundingLabel();UpdateSelectionTransparency(true);UpdateSelectionAngle(true);UpdateLineWidth(true);UpdateObjectDimensions(true);UpdateBrushPanel();
    toolBoxContainer.Layout();
    ApplyScale();
    RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
    return 0;
}

LRESULT CToolSettingsWindow::OnToolsModelSettingsChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    UpdateLineWidth();UpdateSpraySmoothing();UpdateRoundingLabel();
    trackbarRounding.SendMessage(TBM_SETPOS,TRUE,FigureRounding());
    if(roundingTip)
    {
        RECT cells[8];CRect area;CRect unused;calculateTwoBoxes(area,unused);area.InflateRect(-PanelPx(MARGIN2),-PanelPx(MARGIN2));getAirBrushRects(cells,&area);
        for(int i=0;i<8;++i)
        {
            TOOLINFOW info={sizeof(info)};info.hwnd=m_hWnd;info.uId=200+i;
            bool exists=::SendMessage(roundingTip,TTM_GETTOOLINFOW,0,LPARAM(&info))!=0;
            info.uFlags=TTF_SUBCLASS;info.rect=cells[i];if(toolsModel.GetActiveTool()!=TOOL_AIRBRUSH) SetRectEmpty(&info.rect);info.lpszText=LPSTR_TEXTCALLBACKW;
            ::SendMessage(roundingTip,exists?TTM_NEWTOOLRECTW:TTM_ADDTOOLW,0,LPARAM(&info));
        }
    }
    UpdateBrushPanel();
    Invalidate();
    return 0;
}

LRESULT CToolSettingsWindow::OnToolsModelColorPicked(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    m_rgbPickColor = (COLORREF)wParam;
    Invalidate();
    return 0;
}

LRESULT CToolSettingsWindow::OnToolsModelZoomChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    int tbPos=0,zoom=toolsModel.GetZoom();
    for(int i=1;i<=MAX_ZOOM_TRACK;++i)
        if(abs(zoom-s_ZoomSteps[i])<abs(zoom-s_ZoomSteps[tbPos])) tbPos=i;

    trackbarZoom.SendMessage(TBM_SETPOS, TRUE, MAX_ZOOM_TRACK - tbPos);
    UpdateZoomText();
    return 0;
}

LRESULT CToolSettingsWindow::OnCommand(UINT,WPARAM wp,LPARAM lp,BOOL& handled)
{
    int id=LOWORD(wp);
    if(id==IDC_ZOOM_PRESETS && HIWORD(wp)==LBN_SELCHANGE && HWND(lp)==zoomPresets.m_hWnd)
    {
        int selected=int(zoomPresets.SendMessage(LB_GETCURSEL));
        if(selected>=0 && selected<4)
        {
            canvasWindow.zoomTo(s_ZoomPresets[selected]);
            if(::GetCapture()==zoomPresets.m_hWnd)::ReleaseCapture();
            toolsModel.ReturnFromZoom();
            if(toolsModel.GetActiveTool()==TOOL_TEXT && ::IsWindowVisible(textEditWindow))textEditWindow.SetFocus();else canvasWindow.SetFocus();
        }
        return 0;
    }
    if(HIWORD(wp)!=BN_CLICKED || (id!=IDC_SAVE_OBJECT_SIZE && id!=IDC_RESET_OBJECT_SIZE)) { handled=FALSE;return 0; }
    HWND button=GetDlgItem(id);
    if(HWND(lp)!=button || !::IsWindowEnabled(button)) return 0;
    // Finish any pending edit before reading bounds or clearing the prepared size.
    ::SetFocus(button);
    bool text=toolsModel.GetActiveTool()==TOOL_TEXT;
    if(!text && !IsBoundedFigureTool(toolsModel.GetActiveTool())) return 0;
    CRect bounds;bool active=text?textEditWindow.IsWindowVisible() && textEditWindow.GetEditRect(&bounds):toolsModel.EditableFigureBounds(bounds);
    if(id==IDC_SAVE_OBJECT_SIZE)
    {
        SIZE size=active?SIZE{m_dimensionOmitted[0]?0:bounds.Width(),m_dimensionOmitted[1]?0:bounds.Height()}:toolsModel.ObjectSizePreset(text);
        toolsModel.SaveObjectSize(text,size.cx,size.cy);
    }
    else if(id==IDC_RESET_OBJECT_SIZE && !active) toolsModel.ResetObjectSize(text);
    UpdateObjectDimensions(true);UpdateSelectionAngle(true);UpdateRoundingLabel();
    if(text && active) textEditWindow.SetFocus();else canvasWindow.SetFocus();
    return 0;
}
LRESULT CToolSettingsWindow::OnDrawItem(UINT,WPARAM,LPARAM lp,BOOL& handled)
{
    auto d=reinterpret_cast<DRAWITEMSTRUCT*>(lp);
    if(d->CtlID==IDC_ZOOM_PRESETS)
    {
        bool selected=(d->itemState&ODS_SELECTED)!=0;
        FillRect(d->hDC,&d->rcItem,GetSysColorBrush(selected?COLOR_HIGHLIGHT:COLOR_BTNFACE));
        if(d->itemID>=4) return TRUE;
        int saved=SaveDC(d->hDC);SelectObject(d->hDC,panelFont?panelFont:GetStockObject(DEFAULT_GUI_FONT));
        COLORREF ink=GetSysColor(selected?COLOR_HIGHLIGHTTEXT:COLOR_BTNTEXT);
        SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,ink);
        RECT text=d->rcItem;text.left+=PanelPx(5);
        CStringW label;label.Format(L"%dx",s_ZoomPresets[d->itemID]/1000);
        DrawTextW(d->hDC,label,-1,&text,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        int size=PanelPx(s_ZoomPresets[d->itemID]/1000),x=d->rcItem.left+PanelPx(25);
        int y=(d->rcItem.top+d->rcItem.bottom-size)/2;RECT sample={x,y,x+size,y+size};
        HBRUSH brush=CreateSolidBrush(ink);FillRect(d->hDC,&sample,brush);DeleteObject(brush);
        if(d->itemState&ODS_FOCUS) DrawFocusRect(d->hDC,&d->rcItem);
        RestoreDC(d->hDC,saved);return TRUE;
    }
    if(d->CtlID!=IDC_SAVE_OBJECT_SIZE && d->CtlID!=IDC_RESET_OBJECT_SIZE) { handled=FALSE;return 0; }
    RECT r=d->rcItem;bool pushed=(d->itemState&ODS_SELECTED)!=0;
    FillRect(d->hDC,&r,GetSysColorBrush(COLOR_BTNFACE));
    DrawEdge(d->hDC,&r,pushed?EDGE_SUNKEN:EDGE_RAISED,BF_RECT);
    if(pushed) OffsetRect(&r,PanelPx(1),PanelPx(1));
    int state=SaveDC(d->hDC);SelectObject(d->hDC,panelFont?panelFont:GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,GetSysColor(d->itemState&ODS_DISABLED?COLOR_GRAYTEXT:COLOR_BTNTEXT));
    DrawTextW(d->hDC,d->CtlID==IDC_SAVE_OBJECT_SIZE?L"S":L"R",1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    if(d->itemState&ODS_FOCUS) { r=d->rcItem;InflateRect(&r,-PanelPx(3),-PanelPx(3));DrawFocusRect(d->hDC,&r); }
    RestoreDC(d->hDC,state);return TRUE;
}

void CToolSettingsWindow::UpdateObjectDimensions(bool forceText)
{
    if(!objectWidth.IsWindow()) return;
    bool text=toolsModel.GetActiveTool()==TOOL_TEXT,visible=text || IsBoundedFigureTool(toolsModel.GetActiveTool());
    CRect bounds;bool active=text?textEditWindow.IsWindow() && textEditWindow.IsWindowVisible() && textEditWindow.GetEditRect(&bounds):toolsModel.EditableFigureBounds(bounds);
    if(active!=m_dimensionsActive)
        m_dimensionOmitted[0]=m_dimensionOmitted[1]=false;
    m_dimensionsActive=active;if(active) m_dimensionDisplaySize={bounds.Width(),bounds.Height()};
    dimensionX.ShowWindow(SW_HIDE);
    saveSizeLabel.LoadString(g_hinstExe,IDS_SAVE_OBJECT_SIZE);resetSizeLabel.LoadString(g_hinstExe,IDS_RESET_OBJECT_SIZE);
    for(int i=0;i<2;++i)
    {
        CWindow& button=i?resetSizeButton:saveSizeButton;

        button.ShowWindow(visible?SW_SHOWNA:SW_HIDE);button.EnableWindow(visible && (i?!active:true));
    }

    objectWidthLabel.LoadString(g_hinstExe,IDS_OBJECT_WIDTH);objectHeightLabel.LoadString(g_hinstExe,IDS_OBJECT_HEIGHT);
    for(int i=0;i<2;++i)
    {
        CWindow& edit=i?objectHeight:objectWidth;edit.ShowWindow(visible?SW_SHOWNA:SW_HIDE);edit.EnableWindow(visible);
        int value=active?(i?bounds.Height():bounds.Width()):(i?toolsModel.ObjectSizePreset(text).cy:toolsModel.ObjectSizePreset(text).cx);
        if(active && m_dimensionOmitted[i]) value=0;
        if(forceText || ::GetFocus()!=edit.m_hWnd) edit.SetWindowText(value?PixelText(value):CStringW(L""));
    }
    UpdateParameterBar();
}
bool CToolSettingsWindow::ApplyDimensionText(HWND edit)
{
    if(!::IsWindowEnabled(edit)) return false;
    WCHAR text[32];::GetWindowTextW(edit,text,_countof(text));int value=0;
    bool isText=toolsModel.GetActiveTool()==TOOL_TEXT;CRect bounds;
    if(!isText && !IsBoundedFigureTool(toolsModel.GetActiveTool())) return false;
    bool active=isText?textEditWindow.IsWindow() && textEditWindow.IsWindowVisible() && textEditWindow.GetEditRect(&bounds):toolsModel.EditableFigureBounds(bounds);
    CStringW input(text);input.Trim();
    if(active && input.IsEmpty()) { m_dimensionOmitted[edit==objectHeight.m_hWnd]=true;ClearParameter(edit==objectHeight.m_hWnd?PARAM_HEIGHT:PARAM_WIDTH);UpdateParameterBar();return true; }
    if((active || !input.IsEmpty()) && !ParsePixelValue(text,value,1,32767)) return false;
    if(!active)
    {
        SIZE& preset=toolsModel.ObjectSizePreset(isText);
        (edit==objectWidth.m_hWnd?preset.cx:preset.cy)=value;
        if(value)ParameterEntered(edit==objectHeight.m_hWnd?PARAM_HEIGHT:PARAM_WIDTH);else ClearParameter(edit==objectHeight.m_hWnd?PARAM_HEIGHT:PARAM_WIDTH);
        UpdateObjectDimensions();return true;
    }
    m_dimensionOmitted[edit==objectHeight.m_hWnd]=false;
    int width=edit==objectWidth.m_hWnd?value:bounds.Width(),height=edit==objectHeight.m_hWnd?value:bounds.Height();
    if(width==bounds.Width() && height==bounds.Height()) return true;
    bool applied=isText?textEditWindow.SetDimensions(width,height):toolsModel.SetFigureDimensions(width,height);
    if(applied) { ParameterEntered(edit==objectHeight.m_hWnd?PARAM_HEIGHT:PARAM_WIDTH);UpdateObjectDimensions(); }return applied;
}
LRESULT CALLBACK CToolSettingsWindow::DimensionEditProc(HWND edit,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR context)
{
    auto self=reinterpret_cast<CToolSettingsWindow*>(context);
    if(msg==WM_GETDLGCODE) return DLGC_WANTALLKEYS;
    if(msg==WM_KEYDOWN && (wp==VK_RETURN || wp==VK_ESCAPE))
    {
        if(wp==VK_ESCAPE) self->UpdateObjectDimensions(true);
        else if(!self->ApplyDimensionText(edit)) { MessageBeep(MB_OK);::SendMessage(edit,EM_SETSEL,0,-1);return 0; }
        if(toolsModel.GetActiveTool()==TOOL_TEXT && ::IsWindowVisible(textEditWindow)) textEditWindow.SetFocus();else canvasWindow.SetFocus();
        self->UpdateObjectDimensions(true);return 0;
    }
    if(msg==WM_CHAR && (wp==VK_RETURN || wp==VK_ESCAPE)) return 0;
    if(msg==WM_KILLFOCUS && !self->ApplyDimensionText(edit)) self->UpdateObjectDimensions(true);
    return DefSubclassProc(edit,msg,wp,lp);
}

void CToolSettingsWindow::MoveGeometryControls(HWND parent)
{
    for(HWND control:{angleEdit.m_hWnd,skewEdit[0].m_hWnd,skewEdit[1].m_hWnd,objectWidth.m_hWnd,objectHeight.m_hWnd,saveSizeButton.m_hWnd,resetSizeButton.m_hWnd,trackbarRounding.m_hWnd})::SetParent(control,parent);
    dimensionX.ShowWindow(SW_HIDE);transformSeparator.ShowWindow(SW_HIDE);
    for(auto& caption:skewCaption)caption.ShowWindow(SW_HIDE);
}
void CToolSettingsWindow::ParameterCheckChanged(int parameter,bool fixed)
{
    if(parameter<=PARAM_HEIGHT)m_dimensionOmitted[parameter]=false;
    SetParameterFixed(parameter,fixed);UpdateObjectDimensions(true);UpdateSelectionAngle(true);UpdateRoundingLabel();
}
