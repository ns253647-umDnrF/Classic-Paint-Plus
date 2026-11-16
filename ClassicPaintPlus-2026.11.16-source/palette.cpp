/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Window procedure of the palette window
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 *             Copyright 2023 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
 */

#include "precomp.h"
#include "colorpicker.h"

/* The private metrics */
#define CXY_SELECTEDBOX     PanelPx(15)
#define X_MARGIN PanelPx(4)
#define COLOR_PITCH PanelPx(16)
#define Y_MARGIN ((rcClient.bottom / 2) - PanelPx(15))
#define X_COLORBOX_OFFSET   (X_MARGIN + CXY_BIGBOX + X_MARGIN + PanelPx(PALETTE_EXTRA_GAP))
#define COLOR_COUNT         28
#define HALF_COLOR_COUNT    (COLOR_COUNT / 2)

CPaletteWindow paletteWindow;

/* FUNCTIONS ********************************************************/

CPaletteWindow::CPaletteWindow()
    : m_hbmCached(NULL), m_swapPressed(FALSE)
{
}

CPaletteWindow::~CPaletteWindow()
{
    if (m_hbmCached)
        ::DeleteObject(m_hbmCached);
}

static VOID drawColorBox(HDC hDC, LPCRECT prc, HBRUSH hbr, UINT nBorder)
{
    RECT rc = *prc;
    ::FillRect(hDC, &rc, GetSysColorBrush(COLOR_3DFACE));
    bool raised=nBorder==BDR_RAISEDINNER;
    int edge=PanelScale();
    RECT top={rc.left,rc.top,rc.right,rc.top+edge},left={rc.left,rc.top,rc.left+edge,rc.bottom};
    RECT bottom={rc.left,rc.bottom-edge,rc.right,rc.bottom},right={rc.right-edge,rc.top,rc.right,rc.bottom};
    HBRUSH light=GetSysColorBrush(COLOR_3DHIGHLIGHT),dark=GetSysColorBrush(COLOR_3DSHADOW);
    FillRect(hDC,&top,raised?light:dark);FillRect(hDC,&left,raised?light:dark);
    FillRect(hDC,&bottom,raised?dark:light);FillRect(hDC,&right,raised?dark:light);
    InflateRect(&rc,-edge,-edge);
    ::FillRect(hDC, &rc, hbr);
}

static VOID getColorBoxRect(LPRECT prc, const RECT& rcClient, INT iColor)
{
    INT dx = (iColor % HALF_COLOR_COUNT) * COLOR_PITCH;
    INT dy = (iColor / HALF_COLOR_COUNT) * COLOR_PITCH;
    prc->left   = X_COLORBOX_OFFSET + dx;
    prc->right  = prc->left + CXY_COLORBOX;
    prc->top    = Y_MARGIN + dy;
    prc->bottom = prc->top + CXY_COLORBOX;
}

// Shared geometry for painting and hit testing, including the overlapping borders.
static RECT getCurrentColorRect(const RECT& rcClient, BOOL foreground)
{
    const INT position = foreground ? 3 : 5;
    RECT rc;
    rc.left = X_MARGIN + CXY_BIGBOX * position / 8 - CXY_SELECTEDBOX / 2;
    rc.top = Y_MARGIN + PanelPx(8) + CXY_BIGBOX * position / 8 - CXY_SELECTEDBOX / 2;
    rc.right = rc.left + CXY_SELECTEDBOX;
    rc.bottom = rc.top + CXY_SELECTEDBOX;
    return rc;
}

static RECT getSwapRect(const RECT& rcClient)
{
    // Keep the supplied 20/43 pixel artwork at its native size at 100/250%.
    int side=registrySettings.PanelMagnification==100?20:
             registrySettings.PanelMagnification==150?28:
             registrySettings.PanelMagnification==200?36:43;
    RECT rc = { X_MARGIN + PanelPx(16), Y_MARGIN - PanelPx(14), 0, 0 };
    rc.right=rc.left+side;rc.bottom=rc.top+side;
    return rc;
}

static RECT documentSwatch(const RECT& rcClient,bool backdrop)
{
    RECT r={PanelPx(272+PALETTE_EXTRA_GAP),Y_MARGIN+(backdrop?COLOR_PITCH:0),PanelPx(272+PALETTE_EXTRA_GAP)+CXY_COLORBOX,Y_MARGIN+(backdrop?COLOR_PITCH:0)+CXY_COLORBOX};return r;
}
static bool editDocumentSwatch(const RECT& rcClient,POINT point)
{
    if(!DocumentAlpha().enabled) return false;
    RECT upper=documentSwatch(rcClient,false),lower=documentSwatch(rcClient,true);
    if(PtInRect(&upper,point)) { BeginDocumentAlphaPick();return true; }
    if(!PtInRect(&lower,point)) return false;
    COLORREF color=DocumentBackdrop();if(ChooseColorFromCanvas(mainWindow,&color)) SetDocumentBackdrop(color);return true;
}

BOOL CPaletteWindow::EditCurrentColor(INT xPos, INT yPos)
{
    xPos+=GetScrollPos(SB_HORZ);
    RECT rcClient;
    GetClientRect(&rcClient);
    POINT pt = { xPos, yPos };
    RECT fg = getCurrentColorRect(rcClient, TRUE);
    RECT bg = getCurrentColorRect(rcClient, FALSE);
    // The foreground box is painted last and owns the overlap.
    BOOL foreground = ::PtInRect(&fg, pt);
    if (!foreground && !::PtInRect(&bg, pt))
        return FALSE;
    COLORREF color = foreground ? paletteModel.GetFgColor() : paletteModel.GetBgColor();
    BYTE alpha=foreground?paletteModel.GetFgAlpha():paletteModel.GetBgAlpha();
    if (ChooseColorFromCanvas(mainWindow,&color,&alpha))
    {
        if (foreground)
            paletteModel.SetFgColor(color,alpha);
        else
            paletteModel.SetBgColor(color,alpha);
    }
    return TRUE;
}

INT CPaletteWindow::DoHitTest(INT xPos, INT yPos) const
{
    xPos+=GetScrollPos(SB_HORZ);
    RECT rcClient;
    GetClientRect(&rcClient);

    /* delta x and y */
    INT dx = (xPos - X_COLORBOX_OFFSET), dy = (yPos - Y_MARGIN);
    // Integer division rounds small negative offsets to zero.
    if (dx < 0 || dy < 0)
        return -1;

    /* horizontal and vertical indexes */
    INT ix = (dx / COLOR_PITCH), iy = (dy / COLOR_PITCH);
    if(dx%COLOR_PITCH>=CXY_COLORBOX || dy%COLOR_PITCH>=CXY_COLORBOX) return -1;

    /* Is it inside of a color box? */
    if (0 <= ix && ix < HALF_COLOR_COUNT && 0 <= iy && iy < 2)
        return ix + (iy * HALF_COLOR_COUNT); /* return the color index */

    return -1; /* Not found */
}

LRESULT CPaletteWindow::OnEraseBkgnd(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    return TRUE; /* Avoid flickering */
}

LRESULT CPaletteWindow::OnPaint(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    RECT rc, rcClient;
    GetClientRect(&rcClient);

    PAINTSTRUCT ps;
    HDC hDC = BeginPaint(&ps);

    /* To avoid flickering, we use a memory bitmap.
       The left and top values are zeros in client rectangle */
    HDC hMemDC = ::CreateCompatibleDC(hDC);
    m_hbmCached = CachedBufferDIB(m_hbmCached, rcClient.right, rcClient.bottom);
    HGDIOBJ hbmOld = ::SelectObject(hMemDC, m_hbmCached);

    /* Fill the background (since WM_ERASEBKGND handling is disabled) */
    ::FillRect(hMemDC, &rcClient, GetSysColorBrush(COLOR_3DFACE));
    SetViewportOrgEx(hMemDC,-GetScrollPos(SB_HORZ),0,NULL);

    /* Draw the big box that contains the black box and the white box */
    rc = { X_MARGIN, Y_MARGIN + PanelPx(8), X_MARGIN + CXY_BIGBOX, Y_MARGIN + PanelPx(8) + CXY_BIGBOX };
    if(PanelPercent()==100) ::DrawEdge(hMemDC, &rc, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
    else { drawColorBox(hMemDC,&rc,GetSysColorBrush(COLOR_BTNFACE),BDR_SUNKENOUTER);InflateRect(&rc,-PanelScale(),-PanelScale()); }
    for (INT y = rc.top; y < rc.bottom; y+=PanelScale())
    {
        BOOL bLight = ((y-rc.top)/PanelScale() & 1);
        for (INT x = rc.left; x < rc.right; x+=PanelScale())
        {
            if (bLight)
            {
                RECT cell={x,y,min(rc.right,x+PanelScale()),min(rc.bottom,y+PanelScale())};
                ::FillRect(hMemDC,&cell,GetSysColorBrush(COLOR_3DHIGHLIGHT));
            }
            bLight = !bLight;
        }
    }

    /* Draw the white box in the big box, at 5/8 position */
    rc = getCurrentColorRect(rcClient, FALSE);
    drawColorBox(hMemDC, &rc, toolsModel.GetBgBrush(), BDR_RAISEDINNER);
    if(paletteModel.GetBgAlpha()!=255) { InflateRect(&rc,-PanelScale(),-PanelScale());DrawColorSample(hMemDC,rc,paletteModel.GetBgColor(),paletteModel.GetBgAlpha()); }

    /* Draw the black box (overlapping the white box), at 3/8 position */
    rc = getCurrentColorRect(rcClient, TRUE);
    drawColorBox(hMemDC, &rc, toolsModel.GetFgBrush(), BDR_RAISEDINNER);
    if(paletteModel.GetFgAlpha()!=255) { InflateRect(&rc,-PanelScale(),-PanelScale());DrawColorSample(hMemDC,rc,paletteModel.GetFgColor(),paletteModel.GetFgAlpha()); }

    // Small overlapping squares illustrate foreground/background; this is not a reset button.
    // Fixed black/white swatches, with more of the rear white square exposed.
    RECT symbol = { X_MARGIN + PanelPx(5), Y_MARGIN - PanelPx(9), X_MARGIN + PanelPx(14), Y_MARGIN };
    ::FillRect(hMemDC, &symbol, (HBRUSH)::GetStockObject(WHITE_BRUSH));
    for(int i=0;i<PanelScale();++i) { ::FrameRect(hMemDC,&symbol,(HBRUSH)::GetStockObject(BLACK_BRUSH));InflateRect(&symbol,-1,-1); }
    symbol = { X_MARGIN, Y_MARGIN - PanelPx(14), X_MARGIN + PanelPx(8), Y_MARGIN - PanelPx(6) };
    ::FillRect(hMemDC, &symbol, (HBRUSH)::GetStockObject(BLACK_BRUSH));
    for(int i=0;i<PanelScale();++i) { ::FrameRect(hMemDC,&symbol,(HBRUSH)::GetStockObject(WHITE_BRUSH));InflateRect(&symbol,-1,-1); }

    // The supplied exchange arrow is scaled as a bitmap at every panel size.
    RECT swapRect = getSwapRect(rcClient);
    DrawPanelSwapArrow(hMemDC,swapRect);
    if (m_swapPressed)
        ::DrawEdge(hMemDC, &swapRect, BDR_SUNKENOUTER, BF_RECT);

    /* Draw the normal color boxes */
    const PAL_TYPE palette = paletteModel.SelectedPalette();
    for (INT i = 0; i < COLOR_COUNT; i++)
    {
        getColorBoxRect(&rc, rcClient, i);
        HBRUSH hbr = toolsModel.CreateBrush(palette, paletteModel.GetColor(i));
        drawColorBox(hMemDC, &rc, hbr, BDR_SUNKENOUTER);
        DeleteObject(hbr);
    }

    if(DocumentAlpha().enabled)
    {
        for(int i=0;i<2;++i)
        {
            RECT sample=documentSwatch(rcClient,i!=0);COLORREF color=i?DocumentBackdrop():DocumentAlpha().color;
            HBRUSH brush=CreateSolidBrush(color);drawColorBox(hMemDC,&sample,brush,BDR_SUNKENOUTER);DeleteObject(brush);
            if(!i && !DocumentAlpha().hasColor) { InflateRect(&sample,-PanelScale(),-PanelScale());DrawColorSample(hMemDC,sample,0,0); }
        }
    }
    /* Transfer bits (hDC <-- hMemDC) */
    SetViewportOrgEx(hMemDC,0,0,NULL);
    ::BitBlt(hDC, 0, 0, rcClient.right, rcClient.bottom, hMemDC, 0, 0, SRCCOPY);

    ::SelectObject(hMemDC, hbmOld);
    ::DeleteDC(hMemDC);
    EndPaint(&ps);
    return 0;
}

LRESULT CPaletteWindow::OnLButtonDown(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    RECT rcClient;
    GetClientRect(&rcClient);
    POINT pt = { GET_X_LPARAM(lParam)+GetScrollPos(SB_HORZ), GET_Y_LPARAM(lParam) };
    RECT swapRect = getSwapRect(rcClient);
    if (::PtInRect(&swapRect, pt))
    {
        m_swapPressed = TRUE;
        SetCapture();
        Invalidate(FALSE);
        return 0;
    }
    RECT upper=documentSwatch(rcClient,false),lower=documentSwatch(rcClient,true);
    if(DocumentAlpha().enabled && (PtInRect(&upper,pt) || PtInRect(&lower,pt))) return 0;
    RECT fg = getCurrentColorRect(rcClient, TRUE);
    RECT bg = getCurrentColorRect(rcClient, FALSE);
    if (::PtInRect(&fg, pt) || ::PtInRect(&bg, pt))
        return 0;
    INT iColor = DoHitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    if (iColor != -1)
        paletteModel.SetFgColor(paletteModel.GetColor(iColor));
    SetCapture();
    return 0;
}

LRESULT CPaletteWindow::OnRButtonDown(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    INT iColor = DoHitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    if (iColor != -1)
        paletteModel.SetBgColor(paletteModel.GetColor(iColor));
    return 0;
}

LRESULT CPaletteWindow::OnLButtonDblClk(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (EditCurrentColor(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
        return 0;
    RECT rcClient;
    GetClientRect(&rcClient);
    RECT swapRect = getSwapRect(rcClient);
    POINT pt = { GET_X_LPARAM(lParam)+GetScrollPos(SB_HORZ), GET_Y_LPARAM(lParam) };
    if(editDocumentSwatch(rcClient,pt)) return 0;
    if (::PtInRect(&swapRect, pt))
        return OnLButtonDown(nMsg, wParam, lParam, bHandled);
    INT iColor = DoHitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    COLORREF rgbColor = paletteModel.GetFgColor();
    if (iColor != -1 && mainWindow.ChooseColor(&rgbColor))
    {
        paletteModel.SetColor(iColor, rgbColor);
        paletteModel.SetFgColor(rgbColor);
    }
    return 0;
}

LRESULT CPaletteWindow::OnRButtonDblClk(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (EditCurrentColor(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
        return 0;
    INT iColor = DoHitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    COLORREF rgbColor = paletteModel.GetBgColor();
    if (iColor != -1 && mainWindow.ChooseColor(&rgbColor))
    {
        paletteModel.SetColor(iColor, rgbColor);
        paletteModel.SetBgColor(rgbColor);
    }
    return 0;
}

LRESULT CPaletteWindow::OnPaletteModelColorChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    Invalidate(FALSE);
    return 0;
}

LRESULT CPaletteWindow::OnMouseMove(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (::GetCapture() != m_hWnd)
        return 0;

    if (m_swapPressed)
        return 0; // Dragging the swap button must not move the palette toolbar.

    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    ClientToScreen(&pt);

    RECT rc;
    mainWindow.GetWindowRect(&rc);

    POINT ptCenter = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };

    DWORD dwExpectedBar1ID = ((pt.y < ptCenter.y) ? BAR1ID_TOP : BAR1ID_BOTTOM);

    if (registrySettings.Bar1ID != dwExpectedBar1ID)
    {
        registrySettings.Bar1ID = dwExpectedBar1ID;
        mainWindow.PostMessage(WM_SIZE, 0, 0);
    }

    return 0;
}

LRESULT CPaletteWindow::OnLButtonUp(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (::GetCapture() != m_hWnd)
        return 0;

    BOOL swapPressed = m_swapPressed;
    m_swapPressed = FALSE;
    ::ReleaseCapture();
    if (swapPressed)
    {
        RECT rcClient;
        GetClientRect(&rcClient);
        RECT swapRect = getSwapRect(rcClient);
        POINT pt = { GET_X_LPARAM(lParam)+GetScrollPos(SB_HORZ), GET_Y_LPARAM(lParam) };
        if (::PtInRect(&swapRect, pt))
            paletteModel.SwapColors();
        Invalidate(FALSE);
    }
    return 0;
}

LRESULT CPaletteWindow::OnCaptureChanged(UINT, WPARAM, LPARAM, BOOL&)
{
    m_swapPressed = FALSE;
    Invalidate(FALSE);
    return 0;
}

LRESULT CPaletteWindow::OnCreate(UINT,WPARAM,LPARAM,BOOL&)
{
    ModifyStyle(0,WS_CLIPCHILDREN);
    const LPCWSTR names[]={L"\u2190",L"\u2199",L"\u2193",L"\u2190",L"\u2199"};
    for(int i=0;i<5;++i)
        m_copyButtons[i].Create(L"BUTTON",m_hWnd,CRect(0,0,14,14),names[i],WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,0,IDC_COPY_COLOR_FIRST+i);
    LayoutCopyButtons();UpdateTransparencyTips();return 0;
}
void CPaletteWindow::LayoutCopyButtons()
{
    RECT rcClient;GetClientRect(&rcClient);bool enabled=DocumentAlpha().enabled;
    for(int i=0;i<5;++i) if(m_copyButtons[i].IsWindow())
    {
        RECT swatch=documentSwatch(rcClient,i>=3);int column=i<3?i:i-3;
        m_copyButtons[i].MoveWindow(PanelPx(288+PALETTE_EXTRA_GAP+column*16)-GetScrollPos(SB_HORZ),swatch.top,CXY_COLORBOX,CXY_COLORBOX);
        m_copyButtons[i].EnableWindow(enabled && (i>=3 || DocumentAlpha().hasColor));
        m_copyButtons[i].ShowWindow(enabled?SW_SHOWNA:SW_HIDE);
    }
}
LRESULT CPaletteWindow::OnCommand(UINT,WPARAM wp,LPARAM lp,BOOL& handled)
{
    int index=int(LOWORD(wp))-IDC_COPY_COLOR_FIRST;
    if(HIWORD(wp)!=BN_CLICKED || index<0 || index>=5) { handled=FALSE;return 0; }
    if(HWND(lp)!=m_copyButtons[index].m_hWnd || !DocumentAlpha().enabled || (index<3 && !DocumentAlpha().hasColor)) return 0;
    COLORREF color=index<3?DocumentAlpha().color:DocumentBackdrop();
    // These shortcuts copy RGB, never the alpha channel of the source swatch.
    if(index==0 || index==3) paletteModel.SetFgColor(color,255);
    else if(index==1 || index==4) paletteModel.SetBgColor(color,255);
    else SetDocumentBackdrop(color);
    if(toolsModel.GetActiveTool()==TOOL_TEXT && ::IsWindowVisible(textEditWindow)) textEditWindow.SetFocus();else canvasWindow.SetFocus();
    return 0;
}
LRESULT CPaletteWindow::OnDrawItem(UINT,WPARAM,LPARAM lp,BOOL& handled)
{
    auto d=reinterpret_cast<DRAWITEMSTRUCT*>(lp);int index=int(d->CtlID)-IDC_COPY_COLOR_FIRST;
    if(index<0 || index>=5) { handled=FALSE;return 0; }
    RECT r=d->rcItem;bool pressed=(d->itemState&ODS_SELECTED)!=0;
    FillRect(d->hDC,&r,GetSysColorBrush(COLOR_BTNFACE));DrawEdge(d->hDC,&r,pressed?EDGE_SUNKEN:EDGE_RAISED,BF_RECT);
    COLORREF ink=GetSysColor(d->itemState&ODS_DISABLED?COLOR_GRAYTEXT:COLOR_BTNTEXT);
    int direction=index==2?2:(index==1 || index==4)?1:0;
    if(PanelPercent()>100) DrawPanelCopyArrow(d->hDC,r,direction,ink,pressed);
    else
    {
        int state=SaveDC(d->hDC),shift=pressed?1:0;SetViewportOrgEx(d->hDC,r.left+shift,r.top+shift,NULL);
        HPEN pen=CreatePen(PS_SOLID,1,ink);SelectObject(d->hDC,pen);
        auto line=[&](int x1,int y1,int x2,int y2) { MoveToEx(d->hDC,x1,y1,NULL);LineTo(d->hDC,x2,y2); };
        if(direction==2) { line(7,3,7,11);line(4,7,7,10);line(7,10,11,6); }
        else if(direction==1) { line(10,4,4,10);line(4,6,4,11);line(4,10,9,10); }
        else { line(10,7,3,7);line(7,4,4,7);line(4,7,8,11); }
        RestoreDC(d->hDC,state);DeleteObject(pen);
    }
    if(d->itemState&ODS_FOCUS) { InflateRect(&r,-2,-2);DrawFocusRect(d->hDC,&r); }
    return TRUE;
}

LRESULT CPaletteWindow::OnSize(UINT,WPARAM,LPARAM,BOOL&)
{
    CRect rc;GetClientRect(&rc);
    SCROLLINFO si={sizeof(si),SIF_RANGE|SIF_PAGE,0,PanelPx((DocumentAlpha().enabled?340:268)+PALETTE_EXTRA_GAP)-1,UINT(max(1,rc.Width()))};
    SetScrollInfo(SB_HORZ,&si,TRUE);LayoutCopyButtons();UpdateTransparencyTips();Invalidate(FALSE);return 0;
}
LRESULT CPaletteWindow::OnHScroll(UINT message,WPARAM wp,LPARAM,BOOL&)
{
    SCROLLINFO si={sizeof(si),SIF_ALL};GetScrollInfo(SB_HORZ,&si);
    int position=si.nPos;
    if(message==WM_MOUSEWHEEL) position-=GET_WHEEL_DELTA_WPARAM(wp)*PanelPx(32)/WHEEL_DELTA;
    else switch(LOWORD(wp))
    {
    case SB_LINELEFT: position-=PanelPx(16);break;
    case SB_LINERIGHT: position+=PanelPx(16);break;
    case SB_PAGELEFT: position-=si.nPage;break;
    case SB_PAGERIGHT: position+=si.nPage;break;
    case SB_THUMBPOSITION: case SB_THUMBTRACK: position=si.nTrackPos;break;
    case SB_LEFT: position=0;break;
    case SB_RIGHT: position=si.nMax;break;
    }
    SetScrollPos(SB_HORZ,position,TRUE);LayoutCopyButtons();UpdateTransparencyTips();Invalidate(FALSE);return 0;
}

void CPaletteWindow::UpdateTransparencyTips()
{
    bool create=!m_tip;
    if(create) m_tip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,NULL,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,
        0,0,0,0,m_hWnd,NULL,g_hinstExe,NULL);
    RECT rcClient;GetClientRect(&rcClient);
    for(int i=0;i<2;++i)
    {
        TOOLINFOW info={sizeof(info)};info.hwnd=m_hWnd;info.uFlags=TTF_SUBCLASS;info.uId=i+1;
        info.rect=documentSwatch(rcClient,i!=0);OffsetRect(&info.rect,-GetScrollPos(SB_HORZ),0);
        if(!DocumentAlpha().enabled) SetRectEmpty(&info.rect);
        info.lpszText=LPSTR_TEXTCALLBACKW;
        ::SendMessage(m_tip,create?TTM_ADDTOOLW:TTM_NEWTOOLRECTW,0,LPARAM(&info));
    }
    for(auto& button:m_copyButtons) if(button.IsWindow())
    {
        TOOLINFOW info={sizeof(info)};info.hwnd=m_hWnd;info.uFlags=TTF_IDISHWND|TTF_SUBCLASS;info.uId=UINT_PTR(button.m_hWnd);
        if(!::SendMessage(m_tip,TTM_GETTOOLINFOW,0,LPARAM(&info)))
        { info.uFlags=TTF_IDISHWND|TTF_SUBCLASS;info.lpszText=LPSTR_TEXTCALLBACKW;::SendMessage(m_tip,TTM_ADDTOOLW,0,LPARAM(&info)); }
    }
    ::SendMessage(m_tip,TTM_SETMAXTIPWIDTH,0,350);
}
LRESULT CPaletteWindow::OnNotify(UINT,WPARAM,LPARAM lp,BOOL& handled)
{
    auto info=reinterpret_cast<NMTTDISPINFOW*>(lp);
    if(info->hdr.hwndFrom==m_tip && info->hdr.code==TTN_GETDISPINFOW)
    {
        UINT id=info->hdr.idFrom==1?IDS_ALPHA_COLOR:IDS_ALPHA_BACKDROP;
        const UINT labels[]={IDS_COPY_FOREGROUND,IDS_COPY_BACKGROUND,IDS_COPY_BACKDROP,IDS_COPY_FOREGROUND,IDS_COPY_BACKGROUND};
        for(int i=0;i<5;++i) if(info->hdr.idFrom==UINT_PTR(m_copyButtons[i].m_hWnd)) id=labels[i];
        m_tipText.LoadString(g_hinstExe,id);info->lpszText=const_cast<LPWSTR>(LPCWSTR(m_tipText));
    }
    else handled=FALSE;return 0;
}
LRESULT CPaletteWindow::OnDestroy(UINT,WPARAM,LPARAM,BOOL&)
{ if(m_tip) ::DestroyWindow(m_tip);m_tip=NULL;return 0; }
