/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Providing the canvas window class
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 *             Copyright 2026 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
 */

#include "precomp.h"

CCanvasWindow canvasWindow;

/* FUNCTIONS ********************************************************/

HCURSOR SelectionCombineCursor(bool subtract)
{
    struct Cache
    {
        struct Entry { int width,height;bool subtract;HCURSOR cursor; };
        std::vector<Entry> entries;
        ~Cache(){for(const auto& e:entries)DestroyCursor(e.cursor);}
    };
    static Cache cache;
    const int width=max(32,GetSystemMetrics(SM_CXCURSOR)),height=max(32,GetSystemMetrics(SM_CYCURSOR));
    for(const auto& e:cache.entries)if(e.width==width && e.height==height && e.subtract==subtract)return e.cursor;
    HBITMAP bitmap=CreateColorDIB(width,height,RGB(255,0,255));HDC dc=CreateCompatibleDC(NULL);
    if(!bitmap || !dc){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return LoadCursorW(NULL,IDC_ARROW);}
    HGDIOBJ old=SelectObject(dc,bitmap),brush=SelectObject(dc,GetStockObject(WHITE_BRUSH));
    const int stroke=max(1,MulDiv(width,1,32));HPEN pen=CreatePen(PS_SOLID,stroke,RGB(0,0,0));HGDIOBJ oldPen=SelectObject(dc,pen);
    auto point=[&](int x,int y){return POINT{MulDiv(x,width,32),MulDiv(y,height,32)};};
    POINT arrow[]={point(2,2),point(2,21),point(7,16),point(11,24),point(14,22),point(10,14),point(17,14)};
    Polygon(dc,arrow,_countof(arrow));
    // Opaque black sign with a white rim: readable over light/dark image pixels.
    auto bar=[&](int x,int y,int right,int bottom)
    {
        POINT a=point(x,y),b=point(right,bottom);RECT r={a.x-stroke,a.y-stroke,b.x+stroke,b.y+stroke};
        FillRect(dc,&r,(HBRUSH)GetStockObject(WHITE_BRUSH));
    };
    bar(19,23,29,25);if(!subtract)bar(23,19,25,29);
    auto ink=[&](int x,int y,int right,int bottom)
    { POINT a=point(x,y),b=point(right,bottom);RECT r={a.x,a.y,b.x,b.y};FillRect(dc,&r,(HBRUSH)GetStockObject(BLACK_BRUSH)); };
    ink(19,23,29,25);if(!subtract)ink(23,19,25,29);
    int stride=((width+15)/16)*2;std::vector<BYTE> andBits(size_t(stride)*height,255),xorBits(size_t(stride)*height,0);
    GdiFlush();
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)
    {
        COLORREF pixel=GetPixel(dc,x,y);if(pixel==RGB(255,0,255))continue;
        int offset=y*stride+x/8;BYTE bit=BYTE(0x80>>(x&7));andBits[offset]&=BYTE(~bit);
        if(pixel==RGB(255,255,255))xorBits[offset]|=bit;
    }
    SelectObject(dc,oldPen);DeleteObject(pen);SelectObject(dc,brush);SelectObject(dc,old);DeleteDC(dc);DeleteObject(bitmap);
    POINT hot=point(2,2);HCURSOR cursor=CreateCursor(g_hinstExe,hot.x,hot.y,width,height,andBits.data(),xorBits.data());
    if(!cursor)return LoadCursorW(NULL,IDC_ARROW);
    cache.entries.push_back({width,height,subtract,cursor});return cursor;
}

HCURSOR
CStyledCursor::CreateStyledCursor(BrushStyle style, INT zoom, INT radius, COLORREF color, BOOL is_rubber)
{
    const INT diameter = 2 * radius;
    if (diameter * zoom / DEFAULT_ZOOM <= 2)
    {
        HCURSOR hCursor = ::LoadCursor(NULL, IDC_CROSS);
        return hCursor ? CopyCursor(hCursor) : NULL;
    }

    const INT crosshair1 = 6, crosshair2 = crosshair1 - 2;
    const INT width = diameter + 2 * crosshair1, height = diameter + 2 * crosshair1;
    DWORD hotX = width / 2, hotY = height / 2;

    HDC hdcScreen = ::GetDC(NULL);
    if (!hdcScreen)
        return NULL;
    HDC hdcMem = ::CreateCompatibleDC(hdcScreen);
    if (!hdcMem)
    {
        ::ReleaseDC(NULL, hdcScreen);
        return NULL;
    }

    RECT rc = { 0, 0, width, height };

    // Create the AND mask bitmap. This must be monochrome (1bpp):
    // white bits are transparent, black bits are opaque.
    HBITMAP hbmMask = ::CreateBitmap(width, height, 1, 1, NULL);
    if (hbmMask)
    {
        HBITMAP hbmOld = (HBITMAP)::SelectObject(hdcMem, hbmMask);

        // Fill with white brush
        ::FillRect(hdcMem, &rc, (HBRUSH)::GetStockObject(WHITE_BRUSH));

        if (!is_rubber)
        {
            // Draw crosshair with white pen
            ::SelectObject(hdcMem, (HPEN)::GetStockObject(WHITE_PEN));
            ::MoveToEx(hdcMem, 0, hotY, NULL);
            ::LineTo(hdcMem, crosshair2, hotY);
            ::MoveToEx(hdcMem, width - crosshair2, hotY, NULL);
            ::LineTo(hdcMem, width, hotY);
            ::MoveToEx(hdcMem, hotX, 0, NULL);
            ::LineTo(hdcMem, hotX, crosshair2);
            ::MoveToEx(hdcMem, hotX, height - crosshair2, NULL);
            ::LineTo(hdcMem, hotX, height);
        }

        // Draw brush or erase with black color
        if (is_rubber)
            Erase(hdcMem, hotX, hotY, hotX, hotY, RGB(0, 0, 0), radius + 1);
        else
            Brush(hdcMem, hotX, hotY, hotX, hotY, RGB(0, 0, 0), style, diameter);

        if (is_rubber)
            InflateRect(&rc, -1, -1);

        ::SelectObject(hdcMem, hbmOld);
    }

    // Create the color (XOR) bitmap
    HBITMAP hbmColor = ::CreateCompatibleBitmap(hdcScreen, width, height);
    if (hbmColor)
    {
        HBITMAP hbmOld = (HBITMAP)::SelectObject(hdcMem, hbmColor);

        // Fill with black brush
        ::FillRect(hdcMem, &rc, (HBRUSH)::GetStockObject(BLACK_BRUSH));

        if (is_rubber)
        {
            // Draw for rubber border
            INT avg = (GetRValue(color) + GetGValue(color) + GetBValue(color)) / 3;
            COLORREF color2 = (avg > 255 / 2) ? RGB(0, 0, 0) : RGB(255, 255, 255);
            Erase(hdcMem, hotX, hotY, hotX, hotY, color2, radius + 1);
        }
        else
        {
            // Draw crosshair with white pen
            ::SelectObject(hdcMem, (HPEN)::GetStockObject(WHITE_PEN));
            ::MoveToEx(hdcMem, 0, hotY, NULL);
            ::LineTo(hdcMem, crosshair2, hotY);
            ::MoveToEx(hdcMem, width - crosshair2, hotY, NULL);
            ::LineTo(hdcMem, width, hotY);
            ::MoveToEx(hdcMem, hotX, 0, NULL);
            ::LineTo(hdcMem, hotX, crosshair2);
            ::MoveToEx(hdcMem, hotX, height - crosshair2, NULL);
            ::LineTo(hdcMem, hotX, height);
        }

        // Draw brush or erase with color
        if (is_rubber)
            Erase(hdcMem, hotX, hotY, hotX, hotY, color, radius);
        else
            Brush(hdcMem, hotX, hotY, hotX, hotY, color, style, diameter);

        ::SelectObject(hdcMem, hbmOld);
    }

    ::ReleaseDC(NULL, hdcScreen);
    ::DeleteDC(hdcMem);

    if (zoom != DEFAULT_ZOOM)
    {
        INT newWidth = width * zoom / DEFAULT_ZOOM;
        INT newHeight = height * zoom / DEFAULT_ZOOM;
        HBITMAP hbmMaskNew = CopyMonoImage(hbmMask, newWidth, newHeight, STRETCH_DELETESCANS);
        HBITMAP hbmColorNew = CopyDIBImage(hbmColor, newWidth, newHeight, STRETCH_DELETESCANS);
        ::DeleteObject(hbmMask);
        ::DeleteObject(hbmColor);
        hbmMask = hbmMaskNew;
        hbmColor = hbmColorNew;
        hotX = width * zoom / (2 * DEFAULT_ZOOM);
        hotY = height * zoom / (2 * DEFAULT_ZOOM);
    }

    ICONINFO ii = { FALSE, hotX, hotY, hbmMask, hbmColor };
    HCURSOR hCursor = (HCURSOR)::CreateIconIndirect(&ii);

    ::DeleteObject(hbmMask);
    ::DeleteObject(hbmColor);

    return hCursor;
}

void CStyledCursor::SetStyle(BrushStyle style, INT zoom, INT radius, COLORREF color, BOOL is_rubber)
{
    if (m_hCursor &&
        m_style == style &&
        m_zoom == zoom &&
        m_radius == radius &&
        m_color == color &&
        m_is_rubber == is_rubber)
    {
        return;
    }

    if (m_hCursor)
        DestroyCursor(m_hCursor);

    m_hCursor = CreateStyledCursor(style, zoom, radius, color, is_rubber);
    m_style = style;
    m_zoom = zoom;
    m_radius = radius;
    m_color = color;
    m_is_rubber = is_rubber;
}

CCanvasWindow::CCanvasWindow()
    : m_drawing(FALSE)
    , m_hitCanvasSizeBox(HIT_NONE)
    , m_ptOrig { -1, -1 }
{
    m_rcResizing.SetRectEmpty();
}

CCanvasWindow::~CCanvasWindow()
{
}

RECT CCanvasWindow::GetBaseRect()
{
    CRect rcBase;
    GetImageRect(rcBase);
    ImageToCanvas(rcBase);
    rcBase.InflateRect(CanvasGripSize(), CanvasGripSize());
    return rcBase;
}

VOID CCanvasWindow::ImageToCanvas(POINT& pt)
{
    Zoomed(pt);
    pt.x += CanvasGripSize() - GetScrollPos(SB_HORZ);
    pt.y += CanvasGripSize() - GetScrollPos(SB_VERT);
}

VOID CCanvasWindow::ImageToCanvas(RECT& rc)
{
    Zoomed(rc);
    ::OffsetRect(&rc, CanvasGripSize() - GetScrollPos(SB_HORZ), CanvasGripSize() - GetScrollPos(SB_VERT));
}

VOID CCanvasWindow::CanvasToImage(POINT& pt, BOOL bRound)
{
    pt.x -= CanvasGripSize() - GetScrollPos(SB_HORZ);
    pt.y -= CanvasGripSize() - GetScrollPos(SB_VERT);
    UnZoomed(pt, bRound);
}

VOID CCanvasWindow::CanvasToImage(RECT& rc, BOOL bRound)
{
    ::OffsetRect(&rc, GetScrollPos(SB_HORZ) - CanvasGripSize(), GetScrollPos(SB_VERT) - CanvasGripSize());
    UnZoomed(rc, bRound);
}

VOID CCanvasWindow::GetImageRect(RECT& rc)
{
    rc = { 0, 0, imageModel.GetWidth(), imageModel.GetHeight() };
}

HITTEST CCanvasWindow::CanvasHitTest(POINT pt)
{
    if (selectionModel.m_bShow || ::IsWindowVisible(textEditWindow))
        return HIT_INNER;
    RECT rcBase = GetBaseRect();
    return getSizeBoxHitTest(pt, &rcBase, CanvasGripSize());
}

VOID CCanvasWindow::getNewZoomRect(CRect& rcView, INT newZoom, CPoint ptTarget)
{
    CRect rcImage;
    GetImageRect(rcImage);
    ImageToCanvas(rcImage);

    // Calculate the zoom rectangle
    INT oldZoom = toolsModel.GetZoom();
    GetClientRect(rcView);
    LONG cxView = rcView.right * oldZoom / newZoom, cyView = rcView.bottom * oldZoom / newZoom;
    rcView.SetRect(ptTarget.x - cxView / 2, ptTarget.y - cyView / 2,
                   ptTarget.x + cxView / 2, ptTarget.y + cyView / 2);

    // Shift the rectangle if necessary
    INT dx = 0, dy = 0;
    if (rcView.left < rcImage.left)
        dx = rcImage.left - rcView.left;
    else if (rcImage.right < rcView.right)
        dx = rcImage.right - rcView.right;
    if (rcView.top < rcImage.top)
        dy = rcImage.top - rcView.top;
    else if (rcImage.bottom < rcView.bottom)
        dy = rcImage.bottom - rcView.bottom;
    rcView.OffsetRect(dx, dy);

    rcView.IntersectRect(&rcView, &rcImage);
}

VOID CCanvasWindow::zoomTo(INT newZoom, LONG left, LONG top)
{
    POINT pt = { left, top };
    CanvasToImage(pt);


    toolsModel.SetZoom(newZoom);
    ImageToCanvas(pt);
    pt.x += GetScrollPos(SB_HORZ);
    pt.y += GetScrollPos(SB_VERT);

    updateScrollRange();
    updateScrollPos(pt.x, pt.y);
    // The text control follows image coordinates after the final scroll origin
    // is known; zoom notifications arrive before these scroll positions change.
    if(textEditWindow.IsWindowVisible())textEditWindow.ValidateEditRect(NULL);
    Invalidate(TRUE);
}

VOID CCanvasWindow::zoomAt(INT newZoom, POINT clientPoint)
{
    newZoom=max(MIN_ZOOM,min(MAX_ZOOM,newZoom));
    const int oldZoom=toolsModel.GetZoom();
    if(newZoom==oldZoom)return;
    const int grip=CanvasGripSize();
    // Keep subpixel image coordinates until the final screen-pixel rounding.
    auto anchor=[&](LONG point,int scroll) {
        return LONG(lround(double(LONGLONG(point)+scroll-grip)*newZoom/oldZoom+grip-point));
    };
    const POINT scroll={anchor(clientPoint.x,GetScrollPos(SB_HORZ)),anchor(clientPoint.y,GetScrollPos(SB_VERT))};
    toolsModel.SetZoom(newZoom);
    updateScrollRange();
    updateScrollPos(scroll.x,scroll.y);
    if(textEditWindow.IsWindowVisible())textEditWindow.ValidateEditRect(NULL);
    Invalidate(FALSE);
}

BOOL CCanvasWindow::DoDraw(HDC hDC, RECT& rcClient, RECT& rcPaint)
{
    // This is the target area we have to draw on
    CRect rcCanvasDraw;
    rcCanvasDraw.IntersectRect(&rcClient, &rcPaint);

    // Calculate image size
    CRect rcImage;
    GetImageRect(rcImage);
    SIZE sizeImage = { imageModel.GetWidth(), imageModel.GetHeight() };

    // We use a memory bitmap to reduce flickering
    HBITMAP hbmCache1 = CreateDIBWithProperties(rcClient.right, rcClient.bottom);
    if (!hbmCache1)
        return FALSE; // Out of memory
    HBITMAP hbmCache2 = CreateDIBWithProperties(sizeImage.cx, sizeImage.cy);
    if (!hbmCache2)
    {
        ::DeleteObject(hbmCache1);
        return FALSE; // Out of memory
    }

    HDC hdcMem0 = ::CreateCompatibleDC(hDC);
    HGDIOBJ hbm0Old = ::SelectObject(hdcMem0, hbmCache1);

    // Fill the background on hdcMem0
    ::FillRect(hdcMem0, &rcCanvasDraw, GetSysColorBrush(COLOR_APPWORKSPACE));

    // Draw the sizeboxes if necessary
    RECT rcBase = GetBaseRect();
    if (!selectionModel.m_bShow && !::IsWindowVisible(textEditWindow) && !toolsModel.HasEditableFigure())
        drawSizeBoxes(hdcMem0, &rcBase, FALSE, &rcCanvasDraw, CanvasGripSize(), registrySettings.CanvasMarkerColor);

    // Calculate the target area on the image
    CRect rcImageDraw = rcCanvasDraw;
    CanvasToImage(rcImageDraw);
    rcImageDraw.IntersectRect(&rcImageDraw, &rcImage);

    // Consider rounding down by zooming
    rcImageDraw.right += 1;
    rcImageDraw.bottom += 1;

    // hdcMem1 <-- imageModel
    HDC hdcMem1 = ::CreateCompatibleDC(hDC);
    HGDIOBJ hbm1Old = ::SelectObject(hdcMem1, hbmCache2);
    ::BitBlt(hdcMem1, rcImageDraw.left, rcImageDraw.top, rcImageDraw.Width(), rcImageDraw.Height(),
             imageModel.GetDC(), rcImageDraw.left, rcImageDraw.top, SRCCOPY);

    // Draw overlay #1 on hdcMem1
    toolsModel.OnDrawOverlayOnImage(hdcMem1);
    FlattenCanvasSurface(hdcMem1,&rcImageDraw);

    // Transfer the bits with stretch (hdcMem0 <-- hdcMem1)
    ImageToCanvas(rcImage);
    ::StretchBlt(hdcMem0, rcImage.left, rcImage.top, rcImage.Width(), rcImage.Height(),
                 hdcMem1, 0, 0, sizeImage.cx, sizeImage.cy, SRCCOPY);

    // Clean up hdcMem1
    ::SelectObject(hdcMem1, hbm1Old);
    ::DeleteDC(hdcMem1);

    // Draw the grid on hdcMem0
    if (g_showGrid && toolsModel.GetZoom() >= 4000)
    {
        HPEN oldPen = (HPEN) ::SelectObject(hdcMem0, ::CreatePen(PS_SOLID, 1, RGB(160, 160, 160)));
        for (INT counter = 0; counter < sizeImage.cy; counter++)
        {
            POINT pt0 = { 0, counter }, pt1 = { sizeImage.cx, counter };
            ImageToCanvas(pt0);
            ImageToCanvas(pt1);
            ::MoveToEx(hdcMem0, pt0.x, pt0.y, NULL);
            ::LineTo(hdcMem0, pt1.x, pt1.y);
        }
        for (INT counter = 0; counter < sizeImage.cx; counter++)
        {
            POINT pt0 = { counter, 0 }, pt1 = { counter, sizeImage.cy };
            ImageToCanvas(pt0);
            ImageToCanvas(pt1);
            ::MoveToEx(hdcMem0, pt0.x, pt0.y, NULL);
            ::LineTo(hdcMem0, pt1.x, pt1.y);
        }
        ::DeleteObject(::SelectObject(hdcMem0, oldPen));
    }

    // Draw overlay #2 on hdcMem0
    toolsModel.OnDrawOverlayOnCanvas(hdcMem0);

    // Draw new frame on hdcMem0 if any
    if (m_hitCanvasSizeBox != HIT_NONE && !m_rcResizing.IsRectEmpty())
        ::DrawFocusRect(hdcMem0, &m_rcResizing);

    // Transfer the bits (hDC <-- hdcMem0)
    ::BitBlt(hDC, rcCanvasDraw.left, rcCanvasDraw.top, rcCanvasDraw.Width(), rcCanvasDraw.Height(),
             hdcMem0, rcCanvasDraw.left, rcCanvasDraw.top, SRCCOPY);

    // Clean up hdcMem0
    ::SelectObject(hdcMem0, hbm0Old);
    ::DeleteDC(hdcMem0);
    ::DeleteObject(hbmCache2);
    ::DeleteObject(hbmCache1);

    return TRUE;
}

VOID CCanvasWindow::updateScrollRange()
{
    if(m_updatingScrollRange)return;
    m_updatingScrollRange=true;
    CRect rcClient;
    GetClientRect(&rcClient);
    CSize sizeZoomed = { Zoomed(imageModel.GetWidth()), Zoomed(imageModel.GetHeight()) };
    CSize sizeWhole = { sizeZoomed.cx + (CanvasGripSize() * 2) + 20, sizeZoomed.cy + (CanvasGripSize() * 2) + 20 };
    // Anchor zoom only within the document's natural scroll bounds. Negative
    // positions would detach the canvas from the top/left of the workspace.
    DWORD style=GetStyle();
    CSize available(rcClient.Width()+((style&WS_VSCROLL)?GetSystemMetrics(SM_CXVSCROLL):0),
                    rcClient.Height()+((style&WS_HSCROLL)?GetSystemMetrics(SM_CYHSCROLL):0));
    bool horizontal=false,vertical=false;
    for(int pass=0;pass<3;++pass)
    {
        int width=max(1,available.cx-(vertical?GetSystemMetrics(SM_CXVSCROLL):0));
        int height=max(1,available.cy-(horizontal?GetSystemMetrics(SM_CYHSCROLL):0));
        horizontal=sizeWhole.cx+1>width;
        vertical=sizeWhole.cy+1>height;
    }
    ShowScrollBar(SB_HORZ,horizontal);ShowScrollBar(SB_VERT,vertical);
    GetClientRect(&rcClient);
    SCROLLINFO si = { sizeof(si), SIF_PAGE | SIF_RANGE };
    si.nMin   = 0;
    si.nMax   = sizeWhole.cx;
    si.nPage  = max(1,rcClient.Width());
    SetScrollInfo(SB_HORZ, &si);
    si.nMin   = 0;
    si.nMax   = sizeWhole.cy;
    si.nPage  = max(1,rcClient.Height());
    SetScrollInfo(SB_VERT, &si);
    m_updatingScrollRange=false;
}

VOID CCanvasWindow::updateScrollPos(INT x, INT y)
{
    SetScrollPos(SB_HORZ, x);
    SetScrollPos(SB_VERT, y);
}

POINT CCanvasWindow::VisibleImageOrigin()
{
    POINT origin={};ImageToCanvas(origin);
    int zoom=max(1,toolsModel.GetZoom());
    // A hidden edge gets the classic Paint inset: three screen pixels, not
    // three image pixels. Visible edges stay at image coordinate zero.
    origin.x=origin.x<0?LONG((LONGLONG(3-origin.x)*1000+zoom-1)/zoom):0;
    origin.y=origin.y<0?LONG((LONGLONG(3-origin.y)*1000+zoom-1)/zoom):0;
    origin.x=min(origin.x,max(0,imageModel.GetWidth()-1));
    origin.y=min(origin.y,max(0,imageModel.GetHeight()-1));
    return origin;
}

void CCanvasWindow::ScrollToCorner(int corner)
{
    if(corner<0 || corner>3 || m_drawing || ::GetCapture()) return;
    updateScrollRange();
    SCROLLINFO x={sizeof(x),SIF_RANGE|SIF_PAGE},y={sizeof(y),SIF_RANGE|SIF_PAGE};
    GetScrollInfo(SB_HORZ,&x);GetScrollInfo(SB_VERT,&y);
    updateScrollPos(corner&2?max(0,x.nMax-int(x.nPage)+1):0,
                    corner&1?max(0,y.nMax-int(y.nPage)+1):0);
    if(textEditWindow.IsWindowVisible()) textEditWindow.ValidateEditRect(NULL);
    Invalidate(FALSE);
}

LRESULT CCanvasWindow::OnSize(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (m_hWnd)
        updateScrollRange();

    return 0;
}

VOID CCanvasWindow::OnHVScroll(WPARAM wParam, INT fnBar)
{
    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(fnBar, &si);

    switch (LOWORD(wParam))
    {
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            si.nPos = (SHORT)HIWORD(wParam);
            break;
        case SB_LINELEFT: // SB_LINEUP
            si.nPos -= 15;
            break;
        case SB_LINERIGHT: // SB_LINEDOWN
            si.nPos += 15;
            break;
        case SB_PAGELEFT: // SB_PAGEUP
            si.nPos -= si.nPage;
            break;
        case SB_PAGERIGHT: // SB_PAGEDOWN
            si.nPos += si.nPage;
            break;
    }
    si.nPos = max(min(si.nPos, si.nMax), si.nMin);
    SetScrollInfo(fnBar, &si);
    Invalidate();
}

LRESULT CCanvasWindow::OnHScroll(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    OnHVScroll(wParam, SB_HORZ);
    return 0;
}

LRESULT CCanvasWindow::OnVScroll(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    OnHVScroll(wParam, SB_VERT);
    return 0;
}

LRESULT CCanvasWindow::OnButtonDown(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if(nMsg==WM_LBUTTONDOWN || nMsg==WM_RBUTTONDOWN) SetFocus();
    if(textEditWindow.IsWindow() && textEditWindow.CanvasMouse(nMsg,wParam,POINT{GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)})) return 0;
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    if(toolsModel.EditFigureMouse(nMsg,wParam,pt)) return 0;
    if(selectionModel.EditMouse(nMsg,wParam,pt)) return 0;

    if(selectionModel.IsRotating()) return 0;
    // Commit pending numeric controls before hit testing the selection's new angle.
    if(nMsg==WM_LBUTTONDOWN || nMsg==WM_RBUTTONDOWN) SetFocus();
    if(nMsg==WM_RBUTTONDOWN && !m_drawing && selectionModel.StartRotation(pt))
    { SetFocus();m_nMouseDownMsg=WM_RBUTTONDOWN;SetCapture();Invalidate(FALSE);return 0; }
    if(nMsg==WM_RBUTTONDOWN && m_drawing && m_nMouseDownMsg==WM_LBUTTONDOWN)
    {
        POINT imagePoint=pt;CanvasToImage(imagePoint,TRUE);
        if(toolsModel.OnRotateFigure(TRUE,imagePoint.x,imagePoint.y))
        {
            Invalidate(FALSE);
            return 0; // Keep the initiating left-button gesture and its mouse capture.
        }
    }
    // A popup is not a drawing gesture. TrackPopupMenu sends WM_CANCELMODE;
    // marking it as drawing would cancel the selection before the menu opens.
    if(nMsg==WM_RBUTTONDOWN && !m_drawing && toolsModel.IsSelection())
    { ClientToScreen(&pt);mainWindow.TrackPopupMenu(pt,0);return 0; }
    m_nMouseDownMsg = nMsg;
    BOOL bLeftButton = (nMsg == WM_LBUTTONDOWN);

    if (nMsg == WM_MBUTTONDOWN)
    {
        m_ptOrig = pt;
        SetCapture();
        ::SetCursor(::LoadCursorW(g_hinstExe, MAKEINTRESOURCEW(IDC_HANDDRAG)));
        return 0;
    }

    HITTEST hitSelection = selectionModel.hitTest(pt);
    if (hitSelection != HIT_NONE)
    {
        m_drawing = TRUE;
        CanvasToImage(pt, TRUE);
        SetCapture();
        toolsModel.OnButtonDown(bLeftButton, pt.x, pt.y, FALSE);
        Invalidate();
        return 0;
    }

    HITTEST hit = CanvasHitTest(pt);
    if (hit == HIT_NONE || hit == HIT_BORDER)
    {
        switch (toolsModel.GetActiveTool())
        {
            case TOOL_BEZIER:
            case TOOL_SHAPE:
                toolsModel.OnEndDraw(TRUE);
                Invalidate();
                break;

            case TOOL_FREESEL:
            case TOOL_RECTSEL:
                toolsModel.OnEndDraw(FALSE);
                Invalidate();
                break;

            default:
                break;
        }

        toolsModel.resetTool(); // resets the point-buffer of the polygon and bezier functions
        return 0;
    }

    CanvasToImage(pt, TRUE);

    if (hit == HIT_INNER)
    {
        SetFocus();
        m_drawing = TRUE;
        SetCapture();
        toolsModel.OnButtonDown(bLeftButton, pt.x, pt.y, FALSE);
        Invalidate();
        return 0;
    }

    if (bLeftButton)
    {
        m_hitCanvasSizeBox = hit;
        m_ptOrig = pt;
        GetImageRect(m_rcResizing);
        ImageToCanvas(m_rcResizing);
        SetCapture();
        Invalidate(FALSE);
    }

    return 0;
}

LRESULT CCanvasWindow::OnButtonDblClk(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if(IsBoundedFigureTool(toolsModel.GetActiveTool()) || toolsModel.IsSelection() || ::IsWindowVisible(textEditWindow))
        return OnButtonDown(nMsg==WM_LBUTTONDBLCLK?WM_LBUTTONDOWN:WM_RBUTTONDOWN,wParam,lParam,bHandled);
    if(nMsg==WM_RBUTTONDBLCLK && toolsModel.IsSelection() && selectionModel.m_bShow)
        return OnButtonDown(WM_RBUTTONDOWN,wParam,lParam,bHandled);
    if(nMsg==WM_RBUTTONDBLCLK && m_drawing && m_nMouseDownMsg==WM_LBUTTONDOWN &&
       (IsBoundedFigureTool(toolsModel.GetActiveTool()) || toolsModel.GetActiveTool()==TOOL_SHAPE))
        return OnButtonDown(WM_RBUTTONDOWN,wParam,lParam,bHandled);
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    CanvasToImage(pt, TRUE);

    m_drawing = FALSE;
    ::ReleaseCapture();
    m_nMouseDownMsg = 0;

    toolsModel.OnButtonDown(nMsg == WM_LBUTTONDBLCLK, pt.x, pt.y, TRUE);
    toolsModel.resetTool();
    Invalidate();
    return 0;
}

LRESULT CCanvasWindow::OnMouseMove(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    // Captured drawing gestures do not receive WM_SETCURSOR automatically.
    if(toolsModel.IsSelection() && m_drawing && m_hitCanvasSizeBox==HIT_NONE && m_nMouseDownMsg!=WM_MBUTTONDOWN &&
       !toolsModel.SetFigureCursor(pt) && !selectionModel.SetFrameCursor(pt))
        ::SetCursor(::LoadCursorW(NULL,IDC_CROSS));
    if(toolsModel.EditFigureMouse(nMsg,wParam,pt)) return 0;
    if(selectionModel.EditMouse(nMsg,wParam,pt)) return 0;
    if(selectionModel.IsRotating()) { selectionModel.UpdateRotation(pt,(wParam&MK_SHIFT)!=0);return 0; }

    if (m_nMouseDownMsg == WM_MBUTTONDOWN)
    {
        INT x = GetScrollPos(SB_HORZ) - (pt.x - m_ptOrig.x);
        INT y = GetScrollPos(SB_VERT) - (pt.y - m_ptOrig.y);
        SendMessage(WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, x), 0);
        SendMessage(WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, y), 0);
        m_ptOrig = pt;
        return 0;
    }

    CanvasToImage(pt, TRUE);

    if (toolsModel.GetActiveTool() == TOOL_ZOOM)
        Invalidate();

    if (!m_drawing || toolsModel.GetActiveTool() <= TOOL_AIRBRUSH)
    {
        TRACKMOUSEEVENT tme = { sizeof(tme) };
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = m_hWnd;
        tme.dwHoverTime = 0;
        ::TrackMouseEvent(&tme);

        if (!m_drawing)
        {
            CRect rcImage;
            GetImageRect(rcImage);

            CStringW strCoord;
            if (rcImage.PtInRect(pt))
                strCoord.Format(L"%ld, %ld", pt.x, pt.y);
            ::SendMessageW(g_hStatusBar, SB_SETTEXT, 1, (LPARAM)(LPCWSTR)strCoord);
        }
    }

    if (m_drawing || (toolsModel.IsSelection() && m_hitCanvasSizeBox == HIT_NONE))
    {
        toolsModel.DrawWithMouseTool(pt, wParam);
        return 0;
    }

    if (m_hitCanvasSizeBox == HIT_NONE || ::GetCapture() != m_hWnd)
        return 0;

    // Dragging now... Calculate the new size
    INT cxImage = imageModel.GetWidth(), cyImage = imageModel.GetHeight();
    INT cxDelta = pt.x - m_ptOrig.x;
    INT cyDelta = pt.y - m_ptOrig.y;
    switch (m_hitCanvasSizeBox)
    {
        case HIT_UPPER_LEFT:
            cxImage -= cxDelta;
            cyImage -= cyDelta;
            break;
        case HIT_UPPER_CENTER:
            cyImage -= cyDelta;
            break;
        case HIT_UPPER_RIGHT:
            cxImage += cxDelta;
            cyImage -= cyDelta;
            break;
        case HIT_MIDDLE_LEFT:
            cxImage -= cxDelta;
            break;
        case HIT_MIDDLE_RIGHT:
            cxImage += cxDelta;
            break;
        case HIT_LOWER_LEFT:
            cxImage -= cxDelta;
            cyImage += cyDelta;
            break;
        case HIT_LOWER_CENTER:
            cyImage += cyDelta;
            break;
        case HIT_LOWER_RIGHT:
            cxImage += cxDelta;
            cyImage += cyDelta;
            break;
        default:
            return 0;
    }

    // Limit bitmap size
    cxImage = max(1, cxImage);
    cyImage = max(1, cyImage);
    cxImage = min(MAXWORD, cxImage);
    cyImage = min(MAXWORD, cyImage);

    // Display new size
    CStringW strSize;
    strSize.Format(L"%d x %d", cxImage, cyImage);
    ::SendMessageW(g_hStatusBar, SB_SETTEXT, 2, (LPARAM)(LPCWSTR)strSize);

    // Dragging now... Fix the position...
    CRect rcResizing = { 0, 0, cxImage, cyImage };
    switch (m_hitCanvasSizeBox)
    {
        case HIT_UPPER_LEFT:
            rcResizing.OffsetRect(cxDelta, cyDelta);
            break;
        case HIT_UPPER_CENTER:
            rcResizing.OffsetRect(0, cyDelta);
            break;
        case HIT_UPPER_RIGHT:
            rcResizing.OffsetRect(0, cyDelta);
            break;
        case HIT_MIDDLE_LEFT:
            rcResizing.OffsetRect(cxDelta, 0);
            break;
        case HIT_LOWER_LEFT:
            rcResizing.OffsetRect(cxDelta, 0);
            break;
        default:
            break;
    }
    ImageToCanvas(rcResizing);
    m_rcResizing = rcResizing;
    Invalidate(TRUE);

    return 0;
}

LRESULT CCanvasWindow::OnButtonUp(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    if(toolsModel.EditFigureMouse(nMsg,wParam,pt)) return 0;
    if(selectionModel.EditMouse(nMsg,wParam,pt)) return 0;
    if(selectionModel.IsRotating())
    {
        if(nMsg==WM_RBUTTONUP)
        { selectionModel.UpdateRotation(pt,(wParam&MK_SHIFT)!=0);selectionModel.EndRotation(false);m_nMouseDownMsg=0;::ReleaseCapture(); }
        return 0;
    }
    CanvasToImage(pt, TRUE);

    if(nMsg==WM_RBUTTONUP && m_drawing && m_nMouseDownMsg==WM_LBUTTONDOWN &&
       toolsModel.OnRotateFigure(FALSE,pt.x,pt.y))
    {
        Invalidate(FALSE);
        return 0;
    }
    BOOL wasDrawing=m_drawing;
    m_drawing=FALSE; // ReleaseCapture must not treat a normal commit as capture loss.
    HITTEST resizeHit = m_hitCanvasSizeBox;
    m_hitCanvasSizeBox = HIT_NONE;
    ::ReleaseCapture();

    BOOL bLeftButton = (m_nMouseDownMsg == WM_LBUTTONDOWN);
    m_nMouseDownMsg = 0;

    if (wasDrawing)
    {
        m_drawing = FALSE;
        toolsModel.OnButtonUp(bLeftButton, pt.x, pt.y);
        if(toolsModel.IsSelection())SendMessage(WM_SETCURSOR,WPARAM(m_hWnd),MAKELPARAM(HTCLIENT,WM_MOUSEMOVE));
        Invalidate(FALSE);
        ::SendMessageW(g_hStatusBar, SB_SETTEXT, 2, (LPARAM)L"");
        return 0;
    }

    if (resizeHit == HIT_NONE || !bLeftButton)
        return 0;

    // Resize the image
    INT cxImage = imageModel.GetWidth(), cyImage = imageModel.GetHeight();
    INT cxDelta = pt.x - m_ptOrig.x;
    INT cyDelta = pt.y - m_ptOrig.y;
    switch (resizeHit)
    {
        case HIT_UPPER_LEFT:
            imageModel.Crop(cxImage - cxDelta, cyImage - cyDelta, cxDelta, cyDelta);
            break;
        case HIT_UPPER_CENTER:
            imageModel.Crop(cxImage, cyImage - cyDelta, 0, cyDelta);
            break;
        case HIT_UPPER_RIGHT:
            imageModel.Crop(cxImage + cxDelta, cyImage - cyDelta, 0, cyDelta);
            break;
        case HIT_MIDDLE_LEFT:
            imageModel.Crop(cxImage - cxDelta, cyImage, cxDelta, 0);
            break;
        case HIT_MIDDLE_RIGHT:
            imageModel.Crop(cxImage + cxDelta, cyImage, 0, 0);
            break;
        case HIT_LOWER_LEFT:
            imageModel.Crop(cxImage - cxDelta, cyImage + cyDelta, cxDelta, 0);
            break;
        case HIT_LOWER_CENTER:
            imageModel.Crop(cxImage, cyImage + cyDelta, 0, 0);
            break;
        case HIT_LOWER_RIGHT:
            imageModel.Crop(cxImage + cxDelta, cyImage + cyDelta, 0, 0);
            break;
        default:
            break;
    }
    m_rcResizing.SetRectEmpty();

    g_imageSaved = FALSE;

    m_hitCanvasSizeBox = HIT_NONE;
    toolsModel.resetTool(); // resets the point-buffer of the polygon and bezier functions
    updateScrollRange();
    Invalidate(TRUE);
    return 0;
}

LRESULT CCanvasWindow::OnSetCursor(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (CWaitCursor::IsWaiting())
    {
        bHandled = FALSE;
        return 0;
    }

    if (m_nMouseDownMsg == WM_MBUTTONDOWN)
    {
        ::SetCursor(::LoadCursorW(g_hinstExe, MAKEINTRESOURCEW(IDC_HANDDRAG)));
        return 0;
    }

    POINT pt;
    ::GetCursorPos(&pt);
    ScreenToClient(&pt);

    CRect rcClient;
    GetClientRect(&rcClient);

    if (!rcClient.PtInRect(pt))
    {
        bHandled = FALSE;
        return 0;
    }

    if(textEditWindow.IsWindow() && textEditWindow.SetCanvasCursor(pt)) return 0;
    if(toolsModel.GetActiveTool()!=TOOL_ZOOM)
    {
        if(toolsModel.SetFigureCursor(pt)) return 0;
        if(selectionModel.SetFrameCursor(pt)) return 0;
    }

    CRect rcImage;
    GetImageRect(rcImage);
    ImageToCanvas(rcImage);

    if (rcImage.PtInRect(pt))
    {
        switch (toolsModel.GetActiveTool())
        {
            case TOOL_FILL:
                ::SetCursor(::LoadCursorW(g_hinstExe, MAKEINTRESOURCEW(IDC_FILL)));
                break;
            case TOOL_COLOR:
                ::SetCursor(::LoadCursorW(g_hinstExe, MAKEINTRESOURCEW(IDC_COLOR)));
                break;
            case TOOL_ZOOM:
                ::SetCursor(::LoadCursorW(g_hinstExe, MAKEINTRESOURCEW(IDC_ZOOM)));
                break;
            case TOOL_PEN:
                ::SetCursor(::LoadCursorW(g_hinstExe, MAKEINTRESOURCEW(IDC_PEN)));
                break;
            case TOOL_AIRBRUSH:
                ::SetCursor(::LoadCursorW(g_hinstExe, MAKEINTRESOURCEW(IDC_AIRBRUSH)));
                break;
            case TOOL_RUBBER:
            {
                m_hRubberCursor.SetStyle(BrushStyle(registrySettings.EraserShape),
                                         toolsModel.GetZoom(),
                                         toolsModel.GetRubberRadius(),
                                         paletteModel.GetBgColor(), TRUE);
                m_hRubberCursor.SetCursor();
                break;
            }
            case TOOL_BRUSH:
            {
                m_hBrushCursor.SetStyle(toolsModel.GetBrushStyle(),
                                        toolsModel.GetZoom(),
                                        toolsModel.GetBrushWidth() / 2,
                                        paletteModel.GetFgColor(), FALSE);
                m_hBrushCursor.SetCursor();
                break;
            }
            default:
                ::SetCursor(::LoadCursorW(NULL, (LPCWSTR)IDC_CROSS));
        }
        return 0;
    }

    if (selectionModel.m_bShow || !setCursorOnSizeBox(CanvasHitTest(pt)))
        bHandled = FALSE;

    return 0;
}

LRESULT CCanvasWindow::OnKeyDown(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if(nMsg==WM_SYSKEYDOWN || nMsg==WM_SYSKEYUP)
    {
        if(wParam!=VK_MENU || !toolsModel.IsSelection()){bHandled=FALSE;return 0;}
    }
    if(toolsModel.IsSelection() && (wParam==VK_SHIFT || wParam==VK_MENU))
        SendMessage(WM_SETCURSOR,WPARAM(m_hWnd),MAKELPARAM(HTCLIENT,WM_MOUSEMOVE));
    if(nMsg==WM_KEYDOWN && GetKeyState(VK_CONTROL)>=0 && GetKeyState(VK_MENU)>=0 && GetKeyState(VK_SHIFT)>=0)
    {
        int corner=wParam==VK_HOME?0:wParam==VK_END?1:wParam==VK_PRIOR?2:wParam==VK_NEXT?3:-1;
        if(corner>=0) { ScrollToCorner(corner);return 0; }
    }
    if(DocumentAlphaPicking() && nMsg==WM_KEYDOWN && wParam==VK_ESCAPE) { CancelDocumentAlphaPick();return 0; }
    if(toolsModel.EditFigureMouse(nMsg,wParam,POINT{})) return 0;
    if(selectionModel.EditMouse(nMsg,wParam,POINT{})) return 0;
    if(selectionModel.IsRotating())
    {
        if(wParam==VK_SHIFT) selectionModel.UpdateRotationModifiers(nMsg==WM_KEYDOWN);
        if(wParam==VK_ESCAPE && nMsg==WM_KEYDOWN)
        { selectionModel.EndRotation(true);m_nMouseDownMsg=0;::ReleaseCapture(); }
        return 0;
    }
    if(wParam==VK_SHIFT) toolsModel.OnModifiersChanged();
    if (wParam == VK_ESCAPE && nMsg==WM_KEYDOWN)
    {
        OnEndDraw(TRUE);
        ::ReleaseCapture();
        m_nMouseDownMsg = 0;
        m_hitCanvasSizeBox = HIT_NONE;
        m_rcResizing.SetRectEmpty();
        Invalidate(TRUE);
    }

    return 0;
}

LRESULT CCanvasWindow::OnCancelMode(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    toolsModel.EditFigureMouse(nMsg,wParam,POINT{});
    selectionModel.EditMouse(nMsg,wParam,POINT{});
    if(m_drawing && toolsModel.IsSelection())
    { m_drawing=FALSE;m_nMouseDownMsg=0;toolsModel.OnEndDraw(TRUE);if(::GetCapture()==m_hWnd)::ReleaseCapture(); }
    if(selectionModel.IsRotating()) { selectionModel.EndRotation(true);m_nMouseDownMsg=0;if(::GetCapture()==m_hWnd) ::ReleaseCapture(); }
    // Cancel dragging
    if (m_hitCanvasSizeBox != HIT_NONE)
    {
        m_nMouseDownMsg = 0;
        if (::GetCapture() == m_hWnd) ::ReleaseCapture();
    }
    m_hitCanvasSizeBox = HIT_NONE;
    m_rcResizing.SetRectEmpty();
    Invalidate(TRUE);
    return 0;
}

LRESULT CCanvasWindow::OnMouseWheel(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    return ::SendMessageW(GetParent(), nMsg, wParam, lParam);
}

LRESULT CCanvasWindow::OnCaptureChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if(reinterpret_cast<HWND>(lParam)!=m_hWnd) { toolsModel.EditFigureMouse(nMsg,wParam,POINT{});selectionModel.EditMouse(nMsg,wParam,POINT{}); }
    if(selectionModel.IsRotating() && reinterpret_cast<HWND>(lParam)!=m_hWnd)
    { selectionModel.EndRotation(true);m_nMouseDownMsg=0; }
    if(m_drawing && reinterpret_cast<HWND>(lParam)!=m_hWnd &&
       (IsBoundedFigureTool(toolsModel.GetActiveTool()) || toolsModel.GetActiveTool()==TOOL_SHAPE || toolsModel.IsSelection()))
    {
        m_nMouseDownMsg=0;
        OnEndDraw(TRUE);
    }
    if (m_hitCanvasSizeBox != HIT_NONE && reinterpret_cast<HWND>(lParam) != m_hWnd)
    {
        m_hitCanvasSizeBox = HIT_NONE;
        m_nMouseDownMsg = 0;
        m_rcResizing.SetRectEmpty();
        Invalidate(FALSE);
    }
    ::SendMessageW(g_hStatusBar, SB_SETTEXT, 2, (LPARAM)L"");
    return 0;
}

LRESULT CCanvasWindow::OnEraseBkgnd(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    return TRUE; // do nothing => transparent background
}

LRESULT CCanvasWindow::OnPaint(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    RECT rcClient;
    GetClientRect(&rcClient);

    static BOOL s_bShowedOutOfMemory = FALSE; // Don't show "Out Of Memory" message multiple time

    PAINTSTRUCT ps;
    HDC hDC = BeginPaint(&ps);

    if (DoDraw(hDC, rcClient, ps.rcPaint))
    {
        s_bShowedOutOfMemory = FALSE;
    }
    else if (!s_bShowedOutOfMemory)
    {
        ShowOutOfMemory();
        s_bShowedOutOfMemory = TRUE;
        imageModel.ClearHistory(); // Reduce memory usage
    }

    EndPaint(&ps);
    return 0;
}

VOID CCanvasWindow::OnEndDraw(BOOL bCancel)
{
    if(selectionModel.IsRotating()) { selectionModel.EndRotation(bCancel);m_nMouseDownMsg=0;if(::GetCapture()==m_hWnd) ::ReleaseCapture(); }
    m_drawing = FALSE;
    toolsModel.OnEndDraw(bCancel);
    if(bCancel && (IsBoundedFigureTool(toolsModel.GetActiveTool()) || toolsModel.GetActiveTool()==TOOL_SHAPE))
    {
        m_nMouseDownMsg=0;
        if(::GetCapture()==m_hWnd) ::ReleaseCapture();
    }
    Invalidate(FALSE);
}

LRESULT CCanvasWindow::OnCtlColorEdit(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    SetTextColor((HDC)wParam, paletteModel.GetFgColor());
    SetBkMode((HDC)wParam, TRANSPARENT);
    return (LRESULT)GetStockObject(NULL_BRUSH);
}

LRESULT CCanvasWindow::OnPaletteModelColorChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    imageModel.NotifyImageChanged();
    return 0;
}
