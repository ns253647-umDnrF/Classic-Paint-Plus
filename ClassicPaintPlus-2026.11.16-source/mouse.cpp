/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Things which should not be in the mouse event handler itself
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 *             Copyright 2021-2023 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
 */

#include <vector>
#include "precomp.h"
#include "figuregeometry.h"
#include <atlalloc.h>

static SIZE_T s_cPoints = 0;
static CHeapPtr<POINT, CLocalAllocator> s_dynamicPoints;
static POINT s_staticPoints[512]; // 512 is enough
static SIZE_T s_maxPoints = _countof(s_staticPoints);
static LPPOINT s_pPoints = s_staticPoints;
static POINT g_ptStart, g_ptEnd;

/* FUNCTIONS ********************************************************/

void
regularize(LONG x0, LONG y0, LONG& x1, LONG& y1)
{
    if (labs(x1 - x0) >= labs(y1 - y0))
        y1 = y0 + (y1 > y0 ? labs(x1 - x0) : -labs(x1 - x0));
    else
        x1 = x0 + (x1 > x0 ? labs(y1 - y0) : -labs(y1 - y0));
}

void
roundTo8Directions(LONG x0, LONG y0, LONG& x1, LONG& y1)
{
    if (labs(x1 - x0) >= labs(y1 - y0))
    {
        if (labs(y1 - y0) * 5 < labs(x1 - x0) * 2)
            y1 = y0;
        else
            y1 = y0 + (y1 > y0 ? labs(x1 - x0) : -labs(x1 - x0));
    }
    else
    {
        if (labs(x1 - x0) * 5 < labs(y1 - y0) * 2)
            x1 = x0;
        else
            x1 = x0 + (x1 > x0 ? labs(y1 - y0) : -labs(y1 - y0));
    }
}

BOOL nearlyEqualPoints(INT x0, INT y0, INT x1, INT y1)
{
    INT cxThreshold = toolsModel.GetLineWidth() + UnZoomed(GetSystemMetrics(SM_CXDRAG));
    INT cyThreshold = toolsModel.GetLineWidth() + UnZoomed(GetSystemMetrics(SM_CYDRAG));
    return (abs(x1 - x0) <= cxThreshold) && (abs(y1 - y0) <= cyThreshold);
}

void getBoundaryOfPoints(RECT& rcBoundary, SIZE_T cPoints, const POINT *pPoints)
{
    POINT ptMin = { MAXLONG, MAXLONG }, ptMax = { (LONG)MINLONG, (LONG)MINLONG };
    while (cPoints-- > 0)
    {
        LONG x = pPoints->x, y = pPoints->y;
        ptMin = { min(x, ptMin.x), min(y, ptMin.y) };
        ptMax = { max(x, ptMax.x), max(y, ptMax.y) };
        ++pPoints;
    }

    ptMax.x += 1;
    ptMax.y += 1;

    CRect rc(ptMin, ptMax);
    rcBoundary = rc;
}

void ShiftPoints(INT dx, INT dy)
{
    for (SIZE_T i = 0; i < s_cPoints; ++i)
    {
        POINT& pt = s_pPoints[i];
        pt.x += dx;
        pt.y += dy;
    }
}

void BuildMaskFromPoints()
{
    CRect rc;
    getBoundaryOfPoints(rc, s_cPoints, s_pPoints);

    ShiftPoints(-rc.left, -rc.top);

    HDC hdcMem = ::CreateCompatibleDC(NULL);
    HBITMAP hbmMask = ::CreateBitmap(rc.Width(), rc.Height(), 1, 1, NULL);
    HGDIOBJ hbmOld = ::SelectObject(hdcMem, hbmMask);
    CRect local(0,0,rc.Width(),rc.Height());
    ::FillRect(hdcMem, &local, (HBRUSH)::GetStockObject(BLACK_BRUSH));
    HGDIOBJ hPenOld = ::SelectObject(hdcMem, GetStockObject(NULL_PEN));
    HGDIOBJ hbrOld = ::SelectObject(hdcMem, GetStockObject(WHITE_BRUSH));
    ::Polygon(hdcMem, s_pPoints, (INT)s_cPoints);
    ::SelectObject(hdcMem, hbrOld);
    ::SelectObject(hdcMem, hPenOld);
    ::SelectObject(hdcMem, hbmOld);
    ::DeleteDC(hdcMem);

    selectionModel.setMask(rc, hbmMask);
}

void ToolBase::reset()
{
    if (s_pPoints != s_staticPoints)
    {
        s_dynamicPoints.Free();
        s_pPoints = s_staticPoints;
        s_maxPoints = _countof(s_staticPoints);
    }

    s_cPoints = 0;
    g_ptEnd = g_ptStart = { -1, -1 };

    if (selectionModel.m_bShow)
    {
        selectionModel.Landing();
        selectionModel.HideSelection();
    }
}

void ToolBase::OnEndDraw(BOOL bCancel)
{
    reset();
    imageModel.NotifyImageChanged();
}

void ToolBase::beginEvent()
{
    m_hdc = imageModel.GetDC();
    m_fg = paletteModel.GetFgColor();
    m_bg = paletteModel.GetBgColor();
}

void ToolBase::endEvent()
{
    m_hdc = NULL;
}

static void pushToPoints(LONG x, LONG y)
{
    if (s_cPoints + 1 >= s_maxPoints)
    {
        SIZE_T newMax = s_maxPoints + 512;
        SIZE_T cbNew = newMax * sizeof(POINT);
        if (!s_dynamicPoints.ReallocateBytes(cbNew))
        {
            ATLTRACE("%d, %d, %d\n", (INT)s_cPoints, (INT)s_maxPoints, (INT)cbNew);
            return;
        }

        if (s_pPoints == s_staticPoints)
            CopyMemory(s_dynamicPoints, s_staticPoints, s_cPoints * sizeof(POINT));

        s_pPoints = s_dynamicPoints;
        s_maxPoints = newMax;
    }

    s_pPoints[s_cPoints++] = { x, y };
}

/* TOOLS ********************************************************/

struct TwoPointDrawTool : ToolBase
{
    BOOL m_bLeftButton = FALSE;
    BOOL m_bDrawing = FALSE;

    void OnButtonDown(BOOL bLeftButton, LONG x, LONG y, BOOL bDoubleClick) override
    {
        m_bLeftButton = bLeftButton;
        m_bDrawing = TRUE;
        imageModel.NotifyImageChanged();
    }

    BOOL OnMouseMove(BOOL bLeftButton, LONG& x, LONG& y) override
    {
        imageModel.NotifyImageChanged();
        return TRUE;
    }

    BOOL OnButtonUp(BOOL bLeftButton, LONG& x, LONG& y) override
    {
        CRect rcPartial(g_ptStart, g_ptEnd);
        if (IsBoundedFigureTool(toolsModel.GetActiveTool()))
        {
            g_ptEnd = { x, y };
            rcPartial = FigureBounds(g_ptStart, g_ptEnd, !!registrySettings.ShapeFromCenter, GetAsyncKeyState(VK_SHIFT)<0);
        }
        rcPartial.NormalizeRect();
        SIZE size = toolsModel.GetToolSize();
        rcPartial.InflateRect((size.cx + 1) / 2, (size.cy + 1) / 2);
        imageModel.PushImageForUndo(rcPartial);

        OnDrawOverlayOnImage(m_hdc);
        m_bDrawing = FALSE;
        imageModel.NotifyImageChanged();
        return TRUE;
    }

    void OnEndDraw(BOOL bCancel) override
    {
        m_bDrawing = FALSE;
        ToolBase::OnEndDraw(bCancel);
    }

    void OnSpecialTweak(BOOL bMinus) override
    {
        toolsModel.MakeLineThickerOrThinner(bMinus);
    }
};

typedef enum DIRECTION
{
    NO_DIRECTION = -1,
    DIRECTION_HORIZONTAL,
    DIRECTION_VERTICAL,
    DIRECTION_DIAGONAL_RIGHT_DOWN,
    DIRECTION_DIAGONAL_RIGHT_UP,
} DIRECTION;

#define THRESHOULD_DEG 15

static DIRECTION
GetDirection(LONG x0, LONG y0, LONG x1, LONG y1)
{
    LONG dx = x1 - x0, dy = y1 - y0;

    if (labs(dx) <= 8 && labs(dy) <= 8)
        return NO_DIRECTION;

    double radian = atan2((double)dy, (double)dx);
    if (radian < DEG2RAD(-180 + THRESHOULD_DEG))
    {
        ATLTRACE("DIRECTION_HORIZONTAL: %ld\n", RAD2DEG(radian));
        return DIRECTION_HORIZONTAL;
    }
    if (radian < DEG2RAD(-90 - THRESHOULD_DEG))
    {
        ATLTRACE("DIRECTION_DIAGONAL_RIGHT_DOWN: %ld\n", RAD2DEG(radian));
        return DIRECTION_DIAGONAL_RIGHT_DOWN;
    }
    if (radian < DEG2RAD(-90 + THRESHOULD_DEG))
    {
        ATLTRACE("DIRECTION_VERTICAL: %ld\n", RAD2DEG(radian));
        return DIRECTION_VERTICAL;
    }
    if (radian < DEG2RAD(-THRESHOULD_DEG))
    {
        ATLTRACE("DIRECTION_DIAGONAL_RIGHT_UP: %ld\n", RAD2DEG(radian));
        return DIRECTION_DIAGONAL_RIGHT_UP;
    }
    if (radian < DEG2RAD(+THRESHOULD_DEG))
    {
        ATLTRACE("DIRECTION_HORIZONTAL: %ld\n", RAD2DEG(radian));
        return DIRECTION_HORIZONTAL;
    }
    if (radian < DEG2RAD(+90 - THRESHOULD_DEG))
    {
        ATLTRACE("DIRECTION_DIAGONAL_RIGHT_DOWN: %ld\n", RAD2DEG(radian));
        return DIRECTION_DIAGONAL_RIGHT_DOWN;
    }
    if (radian < DEG2RAD(+90 + THRESHOULD_DEG))
    {
        ATLTRACE("DIRECTION_VERTICAL: %ld\n", RAD2DEG(radian));
        return DIRECTION_VERTICAL;
    }
    if (radian < DEG2RAD(+180 - THRESHOULD_DEG))
    {
        ATLTRACE("DIRECTION_DIAGONAL_RIGHT_UP: %ld\n", RAD2DEG(radian));
        return DIRECTION_DIAGONAL_RIGHT_UP;
    }
    ATLTRACE("DIRECTION_HORIZONTAL: %ld\n", RAD2DEG(radian));
    return DIRECTION_HORIZONTAL;
}

static void
RestrictDrawDirection(DIRECTION dir, LONG x0, LONG y0, LONG& x1, LONG& y1)
{
    switch (dir)
    {
        case NO_DIRECTION:
        default:
            return;

        case DIRECTION_HORIZONTAL:
            y1 = y0;
            break;

        case DIRECTION_VERTICAL:
            x1 = x0;
            break;

        case DIRECTION_DIAGONAL_RIGHT_DOWN:
            y1 = y0 + (x1 - x0);
            break;

        case DIRECTION_DIAGONAL_RIGHT_UP:
            x1 = x0 - (y1 - y0);
            break;
    }
}

struct SmoothDrawTool : ToolBase
{
    DIRECTION m_direction = NO_DIRECTION;
    BOOL m_bShiftDown = FALSE;
    BOOL m_bLeftButton = FALSE;

    virtual void OnDraw(HDC hdc, BOOL bLeftButton, POINT pt0, POINT pt1) = 0;

    void OnButtonDown(BOOL bLeftButton, LONG x, LONG y, BOOL bDoubleClick) override
    {
        m_direction = NO_DIRECTION;
        m_bShiftDown = (::GetKeyState(VK_SHIFT) & 0x8000); // Is Shift key pressed?
        m_bLeftButton = bLeftButton;
        s_cPoints = 0;
        pushToPoints(x, y);
        pushToPoints(x, y); // We have to draw the first point
        imageModel.NotifyImageChanged();
    }

    BOOL OnMouseMove(BOOL bLeftButton, LONG& x, LONG& y) override
    {
        if (!m_bShiftDown)
        {
            pushToPoints(x, y);
            imageModel.NotifyImageChanged();
            return TRUE;
        }

        if (m_direction == NO_DIRECTION)
        {
            m_direction = GetDirection(g_ptStart.x, g_ptStart.y, x, y);
            if (m_direction == NO_DIRECTION)
                return FALSE;
        }

        RestrictDrawDirection(m_direction, g_ptStart.x, g_ptStart.y, x, y);
        pushToPoints(x, y);
        imageModel.NotifyImageChanged();
        return TRUE;
    }

    BOOL OnButtonUp(BOOL bLeftButton, LONG& x, LONG& y) override
    {
        if (m_bShiftDown && m_direction != NO_DIRECTION)
            RestrictDrawDirection(m_direction, g_ptStart.x, g_ptStart.y, x, y);

        pushToPoints(x, y);

        CRect rcPartial;
        getBoundaryOfPoints(rcPartial, s_cPoints, s_pPoints);

        SIZE size = toolsModel.GetToolSize();
        rcPartial.InflateRect((size.cx + 1) / 2, (size.cy + 1) / 2);

        imageModel.PushImageForUndo(rcPartial);

        OnDrawOverlayOnImage(m_hdc);
        imageModel.NotifyImageChanged();
        OnEndDraw(FALSE);
        return TRUE;
    }

    void OnDrawOverlayOnImage(HDC hdc) override
    {
        for (SIZE_T i = 1; i < s_cPoints; ++i)
        {
            OnDraw(hdc, m_bLeftButton, s_pPoints[i - 1], s_pPoints[i]);
        }
    }
};

struct SelectionBaseTool : ToolBase
{
    BOOL m_bLeftButton=FALSE,m_bDrawing=FALSE;
    int m_combine=0;
    CRect m_newRect;
    bool isRectSelect() const { return toolsModel.GetActiveTool()==TOOL_RECTSEL; }
    bool SetFrameCursor(POINT point) override
    {
        if(selectionModel.IsEditing() || selectionModel.IsRotating())return false;
        int mode=m_bDrawing?m_combine:GetKeyState(VK_MENU)<0?-1:GetKeyState(VK_SHIFT)<0?1:0;
        if(!mode)return false;
        // Shift on a rotation/shear handle keeps its existing snapping function.
        if(!m_bDrawing && mode>0 && selectionModel.m_bShow && selectionModel.RotateMode() &&
           HitTransformArrow(selectionModel.m_rc,selectionModel.RotationAngle(),selectionModel.SkewX(),selectionModel.SkewY(),point)!=HIT_NONE)return false;
        CRect image;canvasWindow.GetImageRect(image);canvasWindow.ImageToCanvas(image);
        if(!m_bDrawing && !image.PtInRect(point))return false;
        ::SetCursor(SelectionCombineCursor(mode<0));return true;
    }
    void OnDrawOverlayOnImage(HDC dc) override
    {
        if(selectionModel.IsLanded() || !selectionModel.m_bShow)return;
        selectionModel.DrawBackground(dc,paletteModel.GetBgColor());
        selectionModel.DrawSelection(dc,paletteModel.GetBgColor(),toolsModel.IsBackgroundTransparent());
    }
    void OnDrawOverlayOnCanvas(HDC dc) override
    {
        selectionModel.drawFrameOnCanvas(dc);
        if(!m_bDrawing)return;
        if(isRectSelect())
        { CRect r=m_newRect;canvasWindow.ImageToCanvas(r);if(!r.IsRectEmpty())RectSel(dc,r.left,r.top,r.right,r.bottom); }
        else
        {
            std::vector<POINT> points(s_pPoints,s_pPoints+s_cPoints);
            for(auto& p:points)canvasWindow.ImageToCanvas(p);
            DrawDashedPolyline(dc,points.data(),int(points.size()),false);
        }
    }
    void OnButtonDown(BOOL left,LONG x,LONG y,BOOL) override
    {
        m_bLeftButton=left;m_bDrawing=FALSE;POINT p={x,y};
        if(!left)
        { canvasWindow.ImageToCanvas(p);canvasWindow.ClientToScreen(&p);mainWindow.TrackPopupMenu(p,0);return; }
        m_combine=GetKeyState(VK_MENU)<0?-1:GetKeyState(VK_SHIFT)<0?1:0;
        // Direct tool clients use the same frame interaction as canvas messages.
        POINT c=p;canvasWindow.ImageToCanvas(c);
        if(!m_combine && selectionModel.EditMouse(WM_LBUTTONDOWN,GetKeyState(VK_CONTROL)<0?MK_CONTROL:0,c)) return;
        if(!m_combine)selectionModel.Landing();
        imageModel.Clamp(p);g_ptStart=p;m_newRect=CRect(p,p);s_cPoints=0;
        if(!isRectSelect())pushToPoints(p.x,p.y);
        m_bDrawing=TRUE;SetFrameCursor(c);imageModel.NotifyImageChanged();
    }
    BOOL OnMouseMove(BOOL,LONG& x,LONG& y) override
    {
        if(!m_bLeftButton)return TRUE;POINT p={x,y};
        if(!m_bDrawing) { canvasWindow.ImageToCanvas(p);selectionModel.EditMouse(WM_MOUSEMOVE,MK_LBUTTON,p);return TRUE; }
        imageModel.Clamp(p);
        if(isRectSelect()) { m_newRect=CRect(g_ptStart,p);m_newRect.NormalizeRect(); }
        else pushToPoints(p.x,p.y);
        imageModel.NotifyImageChanged();return TRUE;
    }
    BOOL OnButtonUp(BOOL,LONG& x,LONG& y) override
    {
        if(!m_bLeftButton)return TRUE;POINT p={x,y};
        if(!m_bDrawing) { canvasWindow.ImageToCanvas(p);selectionModel.EditMouse(WM_LBUTTONUP,0,p);return TRUE; }
        OnMouseMove(TRUE,x,y);m_bDrawing=FALSE;
        HBITMAP mask=NULL;CRect bounds=m_newRect;
        if(!isRectSelect())
        {
            if(s_cPoints<3) { imageModel.NotifyImageChanged();return TRUE; }
            getBoundaryOfPoints(bounds,s_cPoints,s_pPoints);
            mask=CreateMonoBitmap(bounds.Width(),bounds.Height(),FALSE);
            if(!mask) { ShowOutOfMemory();return TRUE; }
            HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,mask);
            std::vector<POINT> points(s_pPoints,s_pPoints+s_cPoints);
            for(auto& q:points) { q.x-=bounds.left;q.y-=bounds.top; }
            HGDIOBJ pen=SelectObject(dc,GetStockObject(NULL_PEN)),brush=SelectObject(dc,GetStockObject(WHITE_BRUSH));
            Polygon(dc,points.data(),int(points.size()));SelectObject(dc,brush);SelectObject(dc,pen);SelectObject(dc,old);DeleteDC(dc);
        }
        if(!bounds.IsRectEmpty() && !selectionModel.CombineSelection(bounds,mask,m_combine))ShowOutOfMemory();
        else if(bounds.IsRectEmpty())DeleteObject(mask);
        s_cPoints=0;imageModel.NotifyImageChanged();return TRUE;
    }
    void OnEndDraw(BOOL cancel) override
    {
        if(cancel && m_bDrawing && m_combine)
        { m_bDrawing=FALSE;s_cPoints=0;imageModel.NotifyImageChanged();return; }
        m_bDrawing=FALSE;s_cPoints=0;
        if(cancel)selectionModel.HideSelection();else selectionModel.Landing();
        ToolBase::OnEndDraw(cancel);
    }
    void OnSpecialTweak(BOOL minus) override { selectionModel.StretchSelection(minus); }
};
struct FreeSelTool : SelectionBaseTool {};
struct RectSelTool : SelectionBaseTool {};


// TOOL_RUBBER
struct RubberTool : SmoothDrawTool
{
    bool m_eraseAlpha=false;
    void OnButtonDown(BOOL left,LONG x,LONG y,BOOL doubleClick) override
    {
        m_eraseAlpha=ReplaceWithDocumentAlpha();
        SmoothDrawTool::OnButtonDown(left,x,y,doubleClick);
    }
    void OnDrawOverlayOnImage(HDC hdc) override
    {
        DrawSoftBrushStroke(hdc,s_pPoints,s_cPoints,int(registrySettings.EraserShape),toolsModel.GetEraserSize(),
            int(registrySettings.EraserHardness),paletteModel.GetBgColor(),m_eraseAlpha?0:255,
            m_bLeftButton?CLR_INVALID:m_fg);
    }
    void OnDraw(HDC,BOOL,POINT,POINT) override { }

    void OnSpecialTweak(BOOL bMinus) override
    {
        toolsModel.MakeRubberThickerOrThinner(bMinus);
    }
};

// TOOL_FILL
struct FillTool : ToolBase
{
    void OnButtonDown(BOOL bLeftButton, LONG x, LONG y, BOOL bDoubleClick) override
    {
        imageModel.PushImageForUndo();
        HBRUSH hBrush = (bLeftButton ? toolsModel.GetFgBrush() : toolsModel.GetBgBrush());
        BYTE alpha=bLeftButton?paletteModel.GetFgAlpha():paletteModel.GetBgAlpha();
        if(alpha!=255 || BitmapHasAlpha(reinterpret_cast<HBITMAP>(GetCurrentObject(m_hdc,OBJ_BITMAP))))
        {
            COLORREF color=bLeftButton?paletteModel.GetFgColor():paletteModel.GetBgColor();
            FloodRaster(m_hdc,x,y,(DWORD(alpha)<<24)|RGB(GetBValue(color),GetGValue(color),GetRValue(color)));
        }
        else Fill(m_hdc, x, y, hBrush);
    }
};

// TOOL_COLOR
struct ColorTool : ToolBase
{
    COLORREF fetchColor(LONG x, LONG y)
    {
        if (0 <= x && x < imageModel.GetWidth() && 0 <= y && y < imageModel.GetHeight())
            return GetPixel(m_hdc, x, y);
        return RGB(255, 255, 255); // Outside is white
    }

    void OnButtonDown(BOOL bLeftButton, LONG x, LONG y, BOOL bDoubleClick) override
    {
        COLORREF rgbColor = fetchColor(x, y);
        toolSettingsWindow.SendMessage(WM_TOOLSMODELCOLORPICKED, rgbColor, 0);
    }

    BOOL OnMouseMove(BOOL bLeftButton, LONG& x, LONG& y) override
    {
        COLORREF rgbColor = fetchColor(x, y);
        toolSettingsWindow.SendMessage(WM_TOOLSMODELCOLORPICKED, rgbColor, 0);
        return TRUE;
    }

    BOOL OnButtonUp(BOOL bLeftButton, LONG& x, LONG& y) override
    {
        COLORREF rgbColor = fetchColor(x, y);
        if (bLeftButton)
            paletteModel.SetFgColor(rgbColor,BYTE(RasterPixel(m_hdc,x,y)>>24));
        else
            paletteModel.SetBgColor(rgbColor,BYTE(RasterPixel(m_hdc,x,y)>>24));

        if (registrySettings.ReturnAfterColorPick)
            toolsModel.SetActiveTool(toolsModel.GetOldActiveTool());
        return TRUE;
    }
};

// TOOL_ZOOM
struct ZoomTool : ToolBase
{
    BOOL m_bZoomed = FALSE;

    BOOL getNewZoomRect(CRect& rcView, INT newZoom);

    void OnDrawOverlayOnCanvas(HDC hdc) override
    {
        CRect rcView;
        INT oldZoom = toolsModel.GetZoom();
        if (oldZoom < MAX_ZOOM && getNewZoomRect(rcView, oldZoom * 2))
            DrawXorRect(hdc, &rcView);
    }

    void OnButtonDown(BOOL bLeftButton, LONG x, LONG y, BOOL bDoubleClick) override
    {
        INT newZoom, oldZoom = toolsModel.GetZoom();
        if (bLeftButton)
            newZoom = (oldZoom < MAX_ZOOM) ? min(MAX_ZOOM,oldZoom * 2) : MIN_ZOOM;
        else
            newZoom = (oldZoom > MIN_ZOOM) ? max(MIN_ZOOM,oldZoom / 2) : MAX_ZOOM;

        m_bZoomed = FALSE;

        if (oldZoom != newZoom)
        {
            CRect rcView;
            if (x>=0 && y>=0 && x<imageModel.GetWidth() && y<imageModel.GetHeight())
            {
                CPoint point(x,y);canvasWindow.ImageToCanvas(point);
                canvasWindow.getNewZoomRect(rcView,newZoom,point);
                canvasWindow.zoomTo(newZoom, rcView.left, rcView.top);
                m_bZoomed = TRUE;
            }
        }
    }

    BOOL OnButtonUp(BOOL bLeftButton, LONG& x, LONG& y) override
    {
        if (m_bZoomed)
            toolsModel.ReturnFromZoom();

        return TRUE;
    }
};

BOOL ZoomTool::getNewZoomRect(CRect& rcView, INT newZoom)
{
    CPoint pt;
    ::GetCursorPos(&pt);
    canvasWindow.ScreenToClient(&pt);

    canvasWindow.getNewZoomRect(rcView, newZoom, pt);

    CRect rc;
    canvasWindow.GetImageRect(rc);
    canvasWindow.ImageToCanvas(rc);

    return rc.PtInRect(pt);
}

// TOOL_PEN
struct PenTool : SmoothDrawTool
{
    void OnDraw(HDC hdc, BOOL bLeftButton, POINT pt0, POINT pt1) override
    {
        HBRUSH hBrush = (bLeftButton ? toolsModel.GetFgBrush() : toolsModel.GetBgBrush());
        Line(hdc, pt0.x, pt0.y, pt1.x, pt1.y, hBrush, toolsModel.GetPenWidth());
    }

    void OnSpecialTweak(BOOL bMinus) override
    {
        toolsModel.MakePenThickerOrThinner(bMinus);
    }
};

// TOOL_BRUSH
struct BrushTool : SmoothDrawTool
{
    void OnDrawOverlayOnImage(HDC hdc) override
    {
        BYTE alpha=m_bLeftButton?paletteModel.GetFgAlpha():paletteModel.GetBgAlpha();
        int size=toolsModel.GetBrushWidth();bool preset=toolsModel.GetBrushStyle()==BrushStyleRound?(size==1 || size==4 || size==7):(size==2 || size==5 || size==8);
        if(registrySettings.BrushHardness==100 && alpha==255 && preset)
        { SmoothDrawTool::OnDrawOverlayOnImage(hdc);return; }
        DrawSoftBrushStroke(hdc,s_pPoints,s_cPoints,toolsModel.GetBrushStyle(),toolsModel.GetBrushWidth(),
            registrySettings.BrushHardness,m_bLeftButton?paletteModel.GetFgColor():paletteModel.GetBgColor(),alpha);
    }
    void OnDraw(HDC hdc, BOOL bLeftButton, POINT pt0, POINT pt1) override
    {
        HBRUSH hBrush = (bLeftButton ? toolsModel.GetFgBrush() : toolsModel.GetBgBrush());
        Brush(hdc, pt0.x, pt0.y, pt1.x, pt1.y, hBrush, toolsModel.GetBrushStyle(),
              toolsModel.GetBrushWidth());
    }

    void OnSpecialTweak(BOOL bMinus) override
    {
        toolsModel.MakeBrushThickerOrThinner(bMinus);
    }
};

// TOOL_AIRBRUSH
struct AirBrushTool : SmoothDrawTool
{
    DWORD m_dwTick = 0;

    void OnButtonDown(BOOL bLeftButton, LONG x, LONG y, BOOL bDoubleClick) override
    {
        m_dwTick = GetTickCount();
        SmoothDrawTool::OnButtonDown(bLeftButton, x, y, bDoubleClick);
    }

    void OnDrawOverlayOnImage(HDC hdc) override
    {
        srand(m_dwTick);
        SmoothDrawTool::OnDrawOverlayOnImage(hdc);
    }

    void OnDraw(HDC hdc, BOOL bLeftButton, POINT pt0, POINT pt1) override
    {
        HBRUSH hBrush = (bLeftButton ? toolsModel.GetFgBrush() : toolsModel.GetBgBrush());
        BYTE alpha=bLeftButton?paletteModel.GetFgAlpha():paletteModel.GetBgAlpha();
        if(registrySettings.SpraySmoothing || alpha!=255)
            SmoothSpray(hdc,pt1.x,pt1.y,bLeftButton?paletteModel.GetFgColor():paletteModel.GetBgColor(),toolsModel.GetAirBrushRadius(),registrySettings.SprayStyle,registrySettings.SpraySmoothing,alpha);
        else if(registrySettings.SprayStyle)
            SprayPattern(hdc,pt1.x,pt1.y,bLeftButton?paletteModel.GetFgColor():paletteModel.GetBgColor(),toolsModel.GetAirBrushRadius(),registrySettings.SprayStyle,bLeftButton?paletteModel.GetFgAlpha():paletteModel.GetBgAlpha());
        else Airbrush(hdc, pt1.x, pt1.y, hBrush, toolsModel.GetAirBrushRadius());
    }

    void OnSpecialTweak(BOOL bMinus) override
    {
        toolsModel.MakeAirBrushThickerOrThinner(bMinus);
    }
};

// TOOL_TEXT
struct TextTool : ToolBase
{
    void OnDrawOverlayOnImage(HDC hdc) override
    {
        if(::IsWindowVisible(textEditWindow)) textEditWindow.RenderDocument(hdc);
        if (canvasWindow.m_drawing && toolsModel.GetActiveTool()==TOOL_TEXT)
        {
            CRect& rc = selectionModel.m_rc;
            if (!rc.IsRectEmpty())
                RectSel(hdc, rc.left, rc.top, rc.right, rc.bottom);
        }
    }

    void OnDrawOverlayOnCanvas(HDC hdc) override
    { if(::IsWindowVisible(textEditWindow)) textEditWindow.DrawRotatedFrame(hdc); }

    void UpdatePoint(LONG x, LONG y)
    {
        POINT pt = { x, y };
        imageModel.Clamp(pt);
        selectionModel.SetRectFromPoints(g_ptStart, pt);
        imageModel.NotifyImageChanged();
    }

    void OnButtonDown(BOOL bLeftButton, LONG x, LONG y, BOOL bDoubleClick) override
    {
        if (!textEditWindow.IsWindow())
            textEditWindow.Create(canvasWindow);

        UpdatePoint(x, y);
    }

    BOOL OnMouseMove(BOOL bLeftButton, LONG& x, LONG& y) override
    {
        UpdatePoint(x, y);
        return TRUE;
    }

    void draw(HDC hdc)
    {
        textEditWindow.InvalidateEditRect();
        textEditWindow.RenderDocument(hdc);
        RememberFont(registrySettings.strFontName);
    }

    void quit()
    {
        if (textEditWindow.IsWindow())
        {
            bool wasVisible=textEditWindow.IsWindowVisible()!=FALSE;
            textEditWindow.ShowWindow(SW_HIDE);
            if(wasVisible) textEditWindow.RestoreSavedRotation();
        }
        selectionModel.HideSelection();
    }

    BOOL OnButtonUp(BOOL bLeftButton, LONG& x, LONG& y) override
    {
        POINT pt = { x, y };
        imageModel.Clamp(pt);
        selectionModel.SetRectFromPoints(g_ptStart, pt);

        BOOL bTextBoxShown = ::IsWindowVisible(textEditWindow);
        if (bTextBoxShown)
        {
            if (textEditWindow.GetWindowTextLength() > 0)
            {
                imageModel.PushImageForUndo();
                draw(m_hdc);
            }
            if (selectionModel.m_rc.IsRectEmpty())
            {
                quit();
                return TRUE;
            }
        }

        if (registrySettings.ShowTextTool)
        {
            if (!fontsDialog.IsWindow())
                fontsDialog.Create(mainWindow);

            fontsDialog.ShowWindow(SW_SHOWNOACTIVATE);
        }

        CRect rc = selectionModel.m_rc;

        // Enlarge if tool small
        INT cxMin = CX_MINTEXTEDIT, cyMin = CY_MINTEXTEDIT;
        if (selectionModel.m_rc.IsRectEmpty())
        {
            rc.SetRect(x, y, x + cxMin, y + cyMin);
        }
        else
        {
            if (rc.right - rc.left < cxMin)
                rc.right = rc.left + cxMin;
            if (rc.bottom - rc.top < cyMin)
                rc.bottom = rc.top + cyMin;
        }

        if (!textEditWindow.IsWindow())
            textEditWindow.Create(canvasWindow);

        SIZE preset=toolsModel.ObjectSizePreset(true);
        if(preset.cx) rc.right=rc.left+preset.cx;
        if(preset.cy) rc.bottom=rc.top+preset.cy;
        textEditWindow.BeginTextBox(preset.cy!=0 && ParameterIsFixed(PARAM_HEIGHT));
        textEditWindow.SetWindowText(NULL);
        textEditWindow.ValidateEditRect(&rc);
        textEditWindow.FixEditPos(NULL);
        textEditWindow.ShowWindow(SW_SHOWNOACTIVATE);
        toolsModel.RestoreSavedObjectSize(true);
        textEditWindow.SetFocus();
        toolSettingsWindow.UpdateObjectDimensions(true);
        toolSettingsWindow.UpdateSelectionAngle(true);
        return TRUE;
    }

    void OnEndDraw(BOOL bCancel) override
    {
        if (!bCancel)
        {
            if (::IsWindowVisible(textEditWindow) &&
                textEditWindow.GetWindowTextLength() > 0)
            {
                imageModel.PushImageForUndo();
                draw(m_hdc);
            }
        }
        quit();
        ToolBase::OnEndDraw(bCancel);
    }
};

// TOOL_LINE
struct LineTool : TwoPointDrawTool
{
    void OnDrawOverlayOnImage(HDC hdc) override
    {
        if (!m_bDrawing)
            return;
        if (GetAsyncKeyState(VK_SHIFT) < 0)
            roundTo8Directions(g_ptStart.x, g_ptStart.y, g_ptEnd.x, g_ptEnd.y);
        HBRUSH hBrush = (m_bLeftButton ? toolsModel.GetFgBrush() : toolsModel.GetBgBrush());
        Line(hdc, g_ptStart.x, g_ptStart.y, g_ptEnd.x, g_ptEnd.y, hBrush, toolsModel.GetLineWidth());
    }
};

// TOOL_BEZIER
struct BezierTool : ToolBase
{
    BOOL m_bLeftButton = FALSE;

    void OnDrawOverlayOnImage(HDC hdc)
    {
        HBRUSH hBrush = (m_bLeftButton ? toolsModel.GetFgBrush() : toolsModel.GetBgBrush());
        switch (s_cPoints)
        {
            case 2:
                Line(hdc, s_pPoints[0].x, s_pPoints[0].y, s_pPoints[1].x, s_pPoints[1].y, hBrush, toolsModel.GetLineWidth());
                break;
            case 3:
                Bezier(hdc, s_pPoints[0], s_pPoints[2], s_pPoints[2], s_pPoints[1], hBrush, toolsModel.GetLineWidth());
                break;
            case 4:
                Bezier(hdc, s_pPoints[0], s_pPoints[2], s_pPoints[3], s_pPoints[1], hBrush, toolsModel.GetLineWidth());
                break;
        }
    }

    void OnButtonDown(BOOL bLeftButton, LONG x, LONG y, BOOL bDoubleClick) override
    {
        m_bLeftButton = bLeftButton;

        if (s_cPoints == 0)
        {
            pushToPoints(x, y);
            pushToPoints(x, y);
        }
        else
        {
            s_pPoints[s_cPoints - 1] = { x, y };
        }

        imageModel.NotifyImageChanged();
    }

    BOOL OnMouseMove(BOOL bLeftButton, LONG& x, LONG& y) override
    {
        if (s_cPoints > 0)
            s_pPoints[s_cPoints - 1] = { x, y };
        imageModel.NotifyImageChanged();
        return TRUE;
    }

    BOOL OnButtonUp(BOOL bLeftButton, LONG& x, LONG& y) override
    {
        if (s_cPoints >= 4)
        {
            OnEndDraw(FALSE);
            return TRUE;
        }
        pushToPoints(x, y);
        imageModel.NotifyImageChanged();
        return TRUE;
    }

    void OnEndDraw(BOOL bCancel) override
    {
        if (!bCancel && s_cPoints > 1)
        {
            // FIXME: I couldn't calculate boundary rectangle from Bezier curve
            imageModel.PushImageForUndo();
            OnDrawOverlayOnImage(m_hdc);
        }
        ToolBase::OnEndDraw(bCancel);
    }

    void OnSpecialTweak(BOOL bMinus) override
    {
        toolsModel.MakeLineThickerOrThinner(bMinus);
    }
};

// Ready-made shapes keep their geometry separate from the rotation gesture.
// This also ensures that preview, final pixels and the Undo bounds use one state.
struct BoundedFigureTool : TwoPointDrawTool
{
    CRect m_bounds,m_editStart;
    bool m_editing=false,m_editRotating=false;
    HITTEST m_editHit=HIT_NONE;
    POINT m_editPointer={};
    double m_editAngle=0,m_skewX=0,m_skewY=0,m_editSkewX=0,m_editSkewY=0;
    bool m_rotateMode=false,m_editMoved=false,m_editLeftRotation=false,m_editArrow=false;
    POINT m_editCanvas={},m_editLastCanvas={};
    int m_kind=FIG_RECT,m_starBend=0;
    bool m_rotating=false, m_resumeResize=false;
    double m_angle=0, m_baseAngle=0, m_mouseAngle=0;
    double m_endX=0,m_endY=0;
    POINT m_lastMouse={};
    POINT m_rotationMouse={};
    virtual int Kind() const=0;

    void UpdateSize(LONG x,LONG y)
    {
        if(m_resumeResize)
        {
            if(x==m_lastMouse.x && y==m_lastMouse.y) return;
            double dx=x-m_lastMouse.x,dy=y-m_lastMouse.y;
            m_endX+=cos(m_angle)*dx+sin(m_angle)*dy;
            m_endY+=-sin(m_angle)*dx+cos(m_angle)*dy;
            g_ptEnd={LONG(floor(m_endX+.5)),LONG(floor(m_endY+.5))};
        }
        else g_ptEnd={x,y};
        m_lastMouse={x,y};
        m_bounds=FigureBounds(g_ptStart,g_ptEnd,!!registrySettings.ShapeFromCenter,GetAsyncKeyState(VK_SHIFT)<0);
        SIZE preset=toolsModel.ObjectSizePreset(false);
        if(preset.cx)
        {
            m_bounds.left=registrySettings.ShapeFromCenter?g_ptStart.x-(preset.cx-1)/2:
                g_ptEnd.x<g_ptStart.x?g_ptStart.x-preset.cx+1:g_ptStart.x;
            m_bounds.right=m_bounds.left+preset.cx;
        }
        if(preset.cy)
        {
            m_bounds.top=registrySettings.ShapeFromCenter?g_ptStart.y-(preset.cy-1)/2:
                g_ptEnd.y<g_ptStart.y?g_ptStart.y-preset.cy+1:g_ptStart.y;
            m_bounds.bottom=m_bounds.top+preset.cy;
        }
    }
    void UpdateRotation(LONG x,LONG y)
    {
        m_rotationMouse={x,y};
        if(ParameterIsFixed(PARAM_ANGLE))return;
        double dx=x-(m_bounds.left+m_bounds.right-1)*.5;
        double dy=y-(m_bounds.top+m_bounds.bottom-1)*.5;
        if(dx*dx+dy*dy<4) return;
        double angle=m_baseAngle+atan2(dy,dx)-m_mouseAngle;
        m_angle=FigureRotationAngle(angle,GetKeyState(VK_SHIFT)<0);
    }
    BOOL GetRotationAngle(double& angle) const override { angle=m_angle;return m_bDrawing || m_editing; }
    bool SetRotationAngle(double degrees) override
    {
        if(m_rotating || m_editRotating || m_editHit!=HIT_NONE) return false;
        m_angle=degrees*M_PI/180;imageModel.NotifyImageChanged();return true;
    }
    bool GetSkew(double& x,double& y) const override
    { if(!m_bDrawing && !m_editing)return false;x=m_skewX;y=m_skewY;return true; }
    bool SetSkew(double x,double y) override
    {
        if(m_rotating || m_editRotating || m_editHit!=HIT_NONE)return false;
        m_skewX=x;m_skewY=y;imageModel.NotifyImageChanged();return true;
    }
    bool GetStarBend(int& value) const override
    { if(!m_bDrawing && !m_editing)return false;value=m_starBend;return true; }
    bool SetStarBend(int value) override
    { m_starBend=value;imageModel.NotifyImageChanged();return true; }
    bool GetEditableBounds(RECT& bounds) const override { bounds=m_bounds;return m_editing; }
    bool SetFrameCursor(POINT point) override
    {
        if(!m_editing)return false;
        HITTEST hit=m_editHit!=HIT_NONE?m_editHit:HitTransformFrame(FrameBounds(),m_angle*180/M_PI,m_skewX,m_skewY,point);
        if(m_editRotating) { ::SetCursor(TransformFrameCursor(IsCornerHandle(hit)?hit:HIT_UPPER_RIGHT,m_angle*180/M_PI,0,0,true));return true; }
        if(hit==HIT_NONE)return false;
        bool arrow=m_editHit!=HIT_NONE?m_editArrow:HitTransformArrow(FrameBounds(),m_angle*180/M_PI,m_skewX,m_skewY,point)==hit;
        ::SetCursor(TransformFrameCursor(hit,m_angle*180/M_PI,m_skewX,m_skewY,m_rotateMode && arrow));return true;
    }
    bool SetDimensions(int width,int height) override
    {
        if(!m_editing || m_editRotating || m_editHit!=HIT_NONE || width<1 || height<1 || width>32767 || height>32767) return false;
        m_bounds=SizeEditFrame(m_bounds,m_angle*180/M_PI,width,height);imageModel.NotifyImageChanged();return true;
    }
    CRect FrameBounds() const { CRect r=m_bounds;--r.right;--r.bottom;return r; }
    bool ExportFigure(FigureClipboardData& d) const override
    {
        if(!m_editing)return false;
        d={0x46505043,3,sizeof(d),DWORD(m_kind),DWORD(m_bounds.Width()),DWORD(m_bounds.Height()),
            DWORD(toolsModel.GetLineWidth()),DWORD(toolsModel.GetShapeStyle()),DWORD(FigureRounding(m_kind)),
            m_bLeftButton?paletteModel.GetFgColor():paletteModel.GetBgColor(),
            m_bLeftButton?paletteModel.GetBgColor():paletteModel.GetFgColor(),DWORD(paletteModel.SelectedPalette()),
            m_angle*180/M_PI,m_skewX,m_skewY,m_starBend};
        return ValidFigureClipboard(d);
    }
    bool ImportFigure(const FigureClipboardData& d,POINT p) override
    {
        if(!ValidFigureClipboard(d))return false;
        BeginParameterEdit(false);
        m_bounds=CRect(0,0,d.width,d.height);m_angle=d.angle*M_PI/180;m_skewX=d.skewX;m_skewY=d.skewY;m_kind=d.kind;m_starBend=d.starBend;
        CRect visible=TransformedBounds(m_bounds,d.angle,d.skewX,d.skewY);int margin=(d.lineWidth+1)/2+2;
        m_bounds.OffsetRect(p.x-visible.left+margin,p.y-visible.top+margin);
        m_bDrawing=FALSE;m_bLeftButton=TRUE;m_editing=true;m_editHit=HIT_NONE;m_editRotating=m_rotating=m_resumeResize=m_rotateMode=false;
        toolsModel.RememberFigureAngle(d.angle);toolsModel.RememberFigureSkew(m_skewX,m_skewY);imageModel.NotifyImageChanged();return true;
    }
    void EndEditGesture(bool cancel)
    {
        if(cancel) { m_bounds=m_editStart;m_angle=m_editAngle;m_skewX=m_editSkewX;m_skewY=m_editSkewY; }
        m_editHit=HIT_NONE;m_editRotating=false;m_editLeftRotation=false;
        toolsModel.RememberFigureAngle(m_angle*180/M_PI);toolsModel.RememberFigureSkew(m_skewX,m_skewY);
        if(::GetCapture()==canvasWindow.m_hWnd)::ReleaseCapture();imageModel.NotifyImageChanged();
    }
    bool EditMouse(UINT message,WPARAM keys,POINT point) override
    {
        if(!m_editing)return false;
        bool gesture=m_editHit!=HIT_NONE || m_editRotating;
        if(message==WM_CAPTURECHANGED || message==WM_CANCELMODE)
        { if(gesture)EndEditGesture(true);return gesture; }
        if(message==WM_KEYDOWN && keys==VK_ESCAPE && gesture) { EndEditGesture(true);return true; }
        if(message==WM_KEYDOWN && keys==VK_RETURN) { toolsModel.OnEndDraw(FALSE);return true; }
        bool modifier=gesture && (message==WM_KEYDOWN || message==WM_KEYUP) && keys==VK_SHIFT;
        if(modifier)point=m_editLastCanvas;
        if(gesture)
        {
            bool finish=message==(m_editRotating && !m_editLeftRotation?WM_RBUTTONUP:WM_LBUTTONUP);
            if(message==WM_MOUSEMOVE || finish || modifier)
            {
                m_editLastCanvas=point;
                if(abs(point.x-m_editCanvas.x)>=GetSystemMetrics(SM_CXDRAG) || abs(point.y-m_editCanvas.y)>=GetSystemMetrics(SM_CYDRAG))m_editMoved=true;
                canvasWindow.CanvasToImage(point,TRUE);
                if(m_editMoved)
                {
                    if(m_editRotating)UpdateRotation(point.x,point.y);
                    else if(m_rotateMode && m_editArrow && m_editHit>=HIT_UPPER_LEFT && m_editHit<=HIT_LOWER_RIGHT)
                    {
                        DragFrameSkew(m_editStart,m_editHit,m_editPointer,point,m_editAngle*180/M_PI,m_editSkewX,m_editSkewY,GetKeyState(VK_SHIFT)<0,m_skewX,m_skewY);
                        if(ParameterIsFixed(PARAM_SKEW_X))m_skewX=m_editSkewX;
                        if(ParameterIsFixed(PARAM_SKEW_Y))m_skewY=m_editSkewY;
                    }
                    else m_bounds=DragTransformFrame(m_editStart,m_editHit,m_editPointer,point,m_editAngle*180/M_PI,m_editSkewX,m_editSkewY,2,ParameterIsFixed(PARAM_WIDTH),ParameterIsFixed(PARAM_HEIGHT));
                    SetFrameCursor(m_editLastCanvas);imageModel.NotifyImageChanged();toolsModel.ShowRotationAngle();
                }
                if(finish)
                { if(!m_editMoved && m_editHit==HIT_INNER)m_rotateMode=!m_rotateMode;EndEditGesture(false); }
            }
            return true;
        }
        if(message!=WM_LBUTTONDOWN && message!=WM_RBUTTONDOWN)return false;
        HITTEST hit=HitTransformFrame(FrameBounds(),m_angle*180/M_PI,m_skewX,m_skewY,point);
        if(hit==HIT_NONE) { toolsModel.OnEndDraw(FALSE);return message==WM_LBUTTONDOWN; }
        if(message==WM_RBUTTONDOWN && hit==HIT_INNER)
        { canvasWindow.ClientToScreen(&point);ShowFigureContextMenu(point);return true; }
        m_editArrow=HitTransformArrow(FrameBounds(),m_angle*180/M_PI,m_skewX,m_skewY,point)==hit;
        m_editCanvas=m_editLastCanvas=point;m_editMoved=false;
        m_editStart=m_bounds;m_editAngle=m_angle;m_editSkewX=m_skewX;m_editSkewY=m_skewY;
        canvasWindow.CanvasToImage(point,TRUE);m_editPointer=point;
        m_editLeftRotation=message==WM_LBUTTONDOWN && m_rotateMode && m_editArrow && IsCornerHandle(hit);
        if(message==WM_RBUTTONDOWN || m_editLeftRotation)
        {
            m_editRotating=true;m_baseAngle=m_angle;m_rotationMouse=point;
            m_mouseAngle=atan2(point.y-(m_bounds.top+m_bounds.bottom-1)*.5,point.x-(m_bounds.left+m_bounds.right-1)*.5);
        }
        m_editHit=hit;
        canvasWindow.SetFocus();canvasWindow.SetCapture();SetFrameCursor(m_editCanvas);return true;
    }
    void OnDrawOverlayOnCanvas(HDC dc) override
    { if(m_editing)DrawTransformFrame(dc,FrameBounds(),m_angle*180/M_PI,m_skewX,m_skewY,m_rotateMode); }
    void OnModifiersChanged() override
    {
        if(m_rotating) { UpdateRotation(m_rotationMouse.x,m_rotationMouse.y);imageModel.NotifyImageChanged(); }
    }
    void OnButtonDown(BOOL left,LONG x,LONG y,BOOL twice) override
    {
        if(m_editing) OnEndDraw(FALSE);
        BeginParameterEdit(false);
        m_rotating=m_resumeResize=false;m_angle=toolsModel.FigureAngle()*M_PI/180;m_kind=Kind();
        toolsModel.FigureSkew(m_skewX,m_skewY);m_starBend=PreparedFigureTransform().starBend;m_rotateMode=false;
        UpdateSize(x,y);
        TwoPointDrawTool::OnButtonDown(left,x,y,twice);
    }
    BOOL OnMouseMove(BOOL,LONG& x,LONG& y) override
    {
        if(m_rotating) UpdateRotation(x,y);else UpdateSize(x,y);
        imageModel.NotifyImageChanged();
        return FALSE; // g_ptEnd is the unrotated size handle, not the rotation pointer.
    }
    BOOL OnRotate(BOOL begin,LONG x,LONG y) override
    {
        if(!m_bDrawing || !m_bLeftButton) return FALSE;
        if(begin)
        {
            if(m_rotating) return TRUE;
            if(x!=m_lastMouse.x || y!=m_lastMouse.y) UpdateSize(x,y);
            m_baseAngle=m_angle;
            m_mouseAngle=atan2(y-(m_bounds.top+m_bounds.bottom-1)*.5,x-(m_bounds.left+m_bounds.right-1)*.5);
            m_rotating=true;
            m_rotationMouse={x,y};
        }
        else if(m_rotating)
        {
            UpdateRotation(x,y);
            m_rotating=false;m_resumeResize=true;
            m_lastMouse={x,y};m_endX=g_ptEnd.x;m_endY=g_ptEnd.y;
        }
        imageModel.NotifyImageChanged();
        return TRUE;
    }
    void OnDrawOverlayOnImage(HDC dc) override
    {
        if(!m_bDrawing && !m_editing) return;
        DrawFigure(dc,m_kind,m_bounds,m_bLeftButton?toolsModel.GetFgBrush():toolsModel.GetBgBrush(),
            m_bLeftButton?toolsModel.GetBgBrush():toolsModel.GetFgBrush(),toolsModel.GetLineWidth(),toolsModel.GetShapeStyle(),m_angle,m_skewX,m_skewY,-1,m_starBend);
    }
    BOOL OnButtonUp(BOOL,LONG& x,LONG& y) override
    {
        if(!m_bDrawing) return FALSE;
        if(m_rotating) UpdateRotation(x,y);else UpdateSize(x,y);
        m_bDrawing=FALSE;m_editing=!m_bounds.IsRectEmpty();m_rotating=m_resumeResize=false;
        toolsModel.RestoreSavedObjectSize(false);
        toolsModel.RememberFigureAngle(m_angle*180/M_PI);toolsModel.RememberFigureSkew(m_skewX,m_skewY);
        imageModel.NotifyImageChanged();return FALSE;
    }
    void OnEndDraw(BOOL cancel) override
    {
        bool hadFigure=m_bDrawing || m_editing;
        if(!cancel && (m_bDrawing || m_editing))
        {
            CRect dirty=TransformedBounds(m_bounds,m_angle*180/M_PI,m_skewX,m_skewY);
            int margin=(toolsModel.GetLineWidth()+1)/2+2;dirty.InflateRect(margin,margin);
            imageModel.PushImageForUndo(dirty);OnDrawOverlayOnImage(imageModel.GetDC());
        }
        m_editing=false;m_editHit=HIT_NONE;m_editRotating=false;m_rotating=m_resumeResize=false;
        TwoPointDrawTool::OnEndDraw(cancel);
        if(hadFigure)RestoreFigureRoundingPreset();
    }
};

// TOOL_RECT
struct RectTool : BoundedFigureTool
{
    int Kind() const override { return FIG_RECT; }
};

// TOOL_SHAPE
struct ShapeTool : ToolBase
{
    BOOL m_bLeftButton = FALSE;
    BOOL m_bClosed = FALSE;
    bool m_rotating=false,m_resumePoint=false;
    std::vector<POINT> m_originalPoints;
    double m_centerX=0,m_centerY=0,m_startAngle=0;
    double m_totalAngle=0,m_rotationBase=0;
    POINT m_lastMouse={};
    POINT m_rotationMouse={};
    BOOL GetRotationAngle(double& angle) const override { angle=m_totalAngle;return m_rotating; }
    void OnModifiersChanged() override
    {
        if(m_rotating) { LONG x=m_rotationMouse.x,y=m_rotationMouse.y;OnMouseMove(TRUE,x,y); }
    }

    BOOL OnRotate(BOOL begin,LONG x,LONG y) override
    {
        if(!m_bLeftButton || s_cPoints<2) return FALSE;
        if(begin && !m_rotating)
        {
            m_originalPoints.assign(s_pPoints,s_pPoints+s_cPoints);
            RECT rc;getBoundaryOfPoints(rc,s_cPoints,s_pPoints);
            m_centerX=(rc.left+rc.right-1)*.5;m_centerY=(rc.top+rc.bottom-1)*.5;
            m_startAngle=atan2(y-m_centerY,x-m_centerX);
            m_rotationBase=m_totalAngle;m_rotationMouse={x,y};
            m_rotating=true;
        }
        else if(!begin && m_rotating)
        {
            OnMouseMove(TRUE,x,y);
            m_rotating=false;m_resumePoint=true;m_lastMouse={x,y};
        }
        return TRUE;
    }

    void OnDrawOverlayOnImage(HDC hdc)
    {
        if (s_cPoints <= 0)
            return;

        std::vector<POINT> points;
        if(FigureRounding(-TOOL_SHAPE)>0 && s_cPoints>=3)
        {
            FigureContour source;for(size_t i=0;i<s_cPoints;++i)source.push_back({double(s_pPoints[i].x),double(s_pPoints[i].y)});
            for(auto p:RoundFigureContour(source,FigureRounding(-TOOL_SHAPE),1,1,!!m_bClosed))points.push_back({LONG(lround(p.x)),LONG(lround(p.y))});
        }
        POINT* vertices=points.empty()?s_pPoints:points.data();int count=points.empty()?int(s_cPoints):int(points.size());
        Poly(hdc,vertices,count,m_bLeftButton?toolsModel.GetFgBrush():toolsModel.GetBgBrush(),
            m_bLeftButton?toolsModel.GetBgBrush():toolsModel.GetFgBrush(),toolsModel.GetLineWidth(),toolsModel.GetShapeStyle(),m_bClosed,FALSE);
    }

    void OnButtonDown(BOOL bLeftButton, LONG x, LONG y, BOOL bDoubleClick) override
    {
        m_rotating=m_resumePoint=false;m_originalPoints.clear();
        m_bLeftButton = bLeftButton;
        m_bClosed = FALSE;

        if ((s_cPoints > 0) && (GetAsyncKeyState(VK_SHIFT) < 0))
            roundTo8Directions(s_pPoints[s_cPoints - 1].x, s_pPoints[s_cPoints - 1].y, x, y);

        pushToPoints(x, y);

        if (s_cPoints > 1 && bDoubleClick)
        {
            OnEndDraw(FALSE);
            return;
        }

        if (s_cPoints == 1)
            pushToPoints(x, y); // We have to draw the first point

        imageModel.NotifyImageChanged();
    }

    BOOL OnMouseMove(BOOL bLeftButton, LONG& x, LONG& y) override
    {
        if(m_rotating)
        {
            m_rotationMouse={x,y};
            double dx=x-m_centerX,dy=y-m_centerY;
            if(dx*dx+dy*dy>=4)
            {
                m_totalAngle=FigureRotationAngle(m_rotationBase+atan2(dy,dx)-m_startAngle,GetKeyState(VK_SHIFT)<0);
                double a=m_totalAngle-m_rotationBase,c=cos(a),s=sin(a);
                for(SIZE_T i=0;i<s_cPoints;++i)
                {
                    double px=m_originalPoints[i].x-m_centerX,py=m_originalPoints[i].y-m_centerY;
                    s_pPoints[i]={LONG(floor(m_centerX+c*px-s*py+.5)),LONG(floor(m_centerY+s*px+c*py+.5))};
                }
            }
            imageModel.NotifyImageChanged();return FALSE;
        }
        if(m_resumePoint && s_cPoints>1)
        {
            if(x==m_lastMouse.x && y==m_lastMouse.y) return FALSE;
            POINT mouse={x,y};
            x=s_pPoints[s_cPoints-1].x+x-m_lastMouse.x;
            y=s_pPoints[s_cPoints-1].y+y-m_lastMouse.y;
            m_lastMouse=mouse;
        }
        if (s_cPoints > 1)
        {
            if (GetAsyncKeyState(VK_SHIFT) < 0)
                roundTo8Directions(s_pPoints[s_cPoints - 2].x, s_pPoints[s_cPoints - 2].y, x, y);

            s_pPoints[s_cPoints - 1] = { x, y };
        }

        imageModel.NotifyImageChanged();
        return TRUE;
    }

    BOOL OnButtonUp(BOOL bLeftButton, LONG& x, LONG& y) override
    {
        bool rotated=m_rotating || m_resumePoint;
        if(m_rotating || m_resumePoint)
        {
            OnMouseMove(bLeftButton,x,y);
            x=s_pPoints[s_cPoints-1].x;y=s_pPoints[s_cPoints-1].y;
            m_rotating=m_resumePoint=false;m_originalPoints.clear();
        }
        if (!rotated && (s_cPoints > 1) && (GetAsyncKeyState(VK_SHIFT) < 0))
            roundTo8Directions(s_pPoints[s_cPoints - 2].x, s_pPoints[s_cPoints - 2].y, x, y);

        m_bClosed = FALSE;
        if (nearlyEqualPoints(x, y, s_pPoints[0].x, s_pPoints[0].y))
        {
            OnEndDraw(FALSE);
            return TRUE;
        }

        pushToPoints(x, y);
        imageModel.NotifyImageChanged();
        return TRUE;
    }

    void OnEndDraw(BOOL bCancel) override
    {
        if (!bCancel && s_cPoints > 1)
        {
            CRect rcPartial;
            getBoundaryOfPoints(rcPartial, s_cPoints, s_pPoints);

            SIZE size = toolsModel.GetToolSize();
            rcPartial.InflateRect((size.cx + 1) / 2, (size.cy + 1) / 2);

            imageModel.PushImageForUndo(rcPartial);

            m_bClosed = TRUE;
            OnDrawOverlayOnImage(m_hdc);
        }
        m_bClosed = FALSE;
        m_rotating=m_resumePoint=false;m_originalPoints.clear();
        m_totalAngle=0;
        ToolBase::OnEndDraw(bCancel);
    }

    void OnSpecialTweak(BOOL bMinus) override
    {
        toolsModel.MakeLineThickerOrThinner(bMinus);
    }
};

// TOOL_ELLIPSE
struct EllipseTool : BoundedFigureTool
{
    int Kind() const override { return FIG_ELLIPSE; }
};

// TOOL_RRECT
struct RRectTool : BoundedFigureTool
{
    int Kind() const override { return FIG_RRECT; }
};

struct GalleryFigureTool : BoundedFigureTool
{
    int Kind() const override { return registrySettings.FigureKind; }
};

/*static*/ ToolBase*
ToolBase::createToolObject(TOOLTYPE type)
{
    switch (type)
    {
        case TOOL_FREESEL:  return new FreeSelTool();
        case TOOL_RECTSEL:  return new RectSelTool();
        case TOOL_RUBBER:   return new RubberTool();
        case TOOL_FILL:     return new FillTool();
        case TOOL_COLOR:    return new ColorTool();
        case TOOL_ZOOM:     return new ZoomTool();
        case TOOL_PEN:      return new PenTool();
        case TOOL_BRUSH:    return new BrushTool();
        case TOOL_AIRBRUSH: return new AirBrushTool();
        case TOOL_TEXT:     return new TextTool();
        case TOOL_LINE:     return new LineTool();
        case TOOL_BEZIER:   return new BezierTool();
        case TOOL_RECT:     return new RectTool();
        case TOOL_SHAPE:    return new ShapeTool();
        case TOOL_ELLIPSE:  return new EllipseTool();
        case TOOL_RRECT:    return new RRectTool();
        case TOOL_FIGURE:   return new GalleryFigureTool();
    }
    UNREACHABLE;
    return NULL;
}

void ToolsModel::OnButtonDown(BOOL bLeftButton, LONG x, LONG y, BOOL bDoubleClick)
{
    if(m_activeTool==TOOL_ZOOM)
    { m_pZoomTool->beginEvent();m_pZoomTool->OnButtonDown(bLeftButton,x,y,bDoubleClick);m_pZoomTool->endEvent();return; }
    m_pToolObject->beginEvent();
    g_ptEnd = g_ptStart = { x, y };
    m_pToolObject->OnButtonDown(bLeftButton, x, y, bDoubleClick);
    m_pToolObject->endEvent();
}

void ToolsModel::OnMouseMove(BOOL bLeftButton, LONG x, LONG y)
{
    if(m_activeTool==TOOL_ZOOM)
    { m_pZoomTool->beginEvent();m_pZoomTool->OnMouseMove(bLeftButton,x,y);m_pZoomTool->endEvent();return; }
    m_pToolObject->beginEvent();
    if (m_pToolObject->OnMouseMove(bLeftButton, x, y))
        g_ptEnd = { x, y };

    m_pToolObject->endEvent();
}

void ToolsModel::OnButtonUp(BOOL bLeftButton, LONG x, LONG y)
{
    if(m_activeTool==TOOL_ZOOM)
    { m_pZoomTool->beginEvent();m_pZoomTool->OnButtonUp(bLeftButton,x,y);m_pZoomTool->endEvent();return; }
    if(DocumentAlphaPicking()) { if(bLeftButton) PickDocumentAlpha(x,y);else CancelDocumentAlphaPick();return; }
    m_pToolObject->beginEvent();
    if (m_pToolObject->OnButtonUp(bLeftButton, x, y))
        g_ptEnd = { x, y };

    m_pToolObject->endEvent();
}

void ToolsModel::OnEndDraw(BOOL bCancel)
{
    ATLTRACE("ToolsModel::OnEndDraw(%d)\n", bCancel);
    TOOLTYPE active=m_activeTool;m_activeTool=GetEditingTool();
    m_pToolObject->beginEvent();
    m_pToolObject->OnEndDraw(bCancel);
    m_pToolObject->endEvent();
    m_activeTool=active;
}

BOOL ToolsModel::OnRotateFigure(BOOL begin,LONG x,LONG y)
{
    if(m_activeTool==TOOL_ZOOM)return FALSE;
    m_pToolObject->beginEvent();
    BOOL handled=m_pToolObject->OnRotate(begin,x,y);
    m_pToolObject->endEvent();
    ShowRotationAngle();
    return handled;
}

double ToolsModel::FigureAngle() const
{
    double angle=PreparedFigureTransform().angle,radians=0;
    if(m_pToolObject && m_pToolObject->GetRotationAngle(radians)) angle=radians*180/M_PI;
    angle=fmod(angle,360.0);return angle<0?angle+360:angle;
}
bool ToolsModel::SetFigureAngle(double degrees)
{
    if(!IsBoundedFigureTool(m_activeTool) || !_finite(degrees)) return false;
    degrees=fmod(degrees,360.0);if(degrees<0) degrees+=360;
    if(!m_pToolObject->SetRotationAngle(degrees)) return false;
    if(!HasEditableFigure() && !canvasWindow.m_drawing)PreparedFigureTransform().angle=degrees;
    toolSettingsWindow.UpdateSelectionAngle(true);return true;
}
void ToolsModel::FigureSkew(double& x,double& y) const
{ const auto& p=PreparedFigureTransform();x=p.skewX;y=p.skewY;if(m_pToolObject)m_pToolObject->GetSkew(x,y); }
bool ToolsModel::SetFigureSkew(double x,double y)
{
    if(!IsBoundedFigureTool(m_activeTool) || !_finite(x) || !_finite(y) || fabs(x)>8 || fabs(y)>8)return false;
    if(!m_pToolObject->SetSkew(x,y))return false;
    if(!HasEditableFigure() && !canvasWindow.m_drawing){auto& p=PreparedFigureTransform();p.skewX=x;p.skewY=y;}
    toolSettingsWindow.UpdateSelectionAngle(true);return true;
}
int ToolsModel::StarBend() const
{ int value=PreparedFigureTransform().starBend;if(m_pToolObject)m_pToolObject->GetStarBend(value);return value; }
bool ToolsModel::SetStarBend(int percent)
{
    if(!IsBoundedFigureTool(m_activeTool) || percent<-100 || percent>100 || !m_pToolObject->SetStarBend(percent))return false;
    if(!HasEditableFigure() && !canvasWindow.m_drawing)PreparedFigureTransform().starBend=percent;
    UpdateParameterBar();return true;
}
bool ToolsModel::SetFigureDimensions(int width,int height) { return m_pToolObject && m_pToolObject->SetDimensions(width,height); }
bool ToolsModel::HasEditableFigure() const { RECT r;return EditableFigureBounds(r); }
bool ToolsModel::EditableFigureBounds(RECT& bounds) const
{ return m_pToolObject && m_pToolObject->GetEditableBounds(bounds); }
bool ToolsModel::EditFigureMouse(UINT message,WPARAM keys,POINT point)
{ return m_activeTool!=TOOL_ZOOM && m_pToolObject && m_pToolObject->EditMouse(message,keys,point); }
bool ToolsModel::SetFigureCursor(POINT point)
{
    return m_pToolObject && m_pToolObject->SetFrameCursor(point);
}

void ToolsModel::ShowRotationAngle()
{
    toolSettingsWindow.UpdateSelectionAngle();
    double angle;
    if(m_pToolObject->GetRotationAngle(angle))
    {
        CStringW text;text.Format(L"%.1f\x00b0",angle*180/M_PI);
        ::SendMessageW(g_hStatusBar,SB_SETTEXT,1,reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(text)));
    }
}
void ToolsModel::OnModifiersChanged()
{
    m_pToolObject->OnModifiersChanged();ShowRotationAngle();
}

void ToolsModel::OnDrawOverlayOnImage(HDC hdc)
{
    m_pToolObject->OnDrawOverlayOnImage(hdc);
}

void ToolsModel::OnDrawOverlayOnCanvas(HDC hdc)
{
    m_pToolObject->OnDrawOverlayOnCanvas(hdc);
    if(m_activeTool==TOOL_ZOOM && m_pZoomTool)m_pZoomTool->OnDrawOverlayOnCanvas(hdc);
}

void ToolsModel::SpecialTweak(BOOL bMinus)
{
    m_pToolObject->OnSpecialTweak(bMinus);
}

void ToolsModel::DrawWithMouseTool(POINT pt, WPARAM wParam)
{
    LONG xRel = pt.x - g_ptStart.x, yRel = pt.y - g_ptStart.y;

    switch (m_activeTool)
    {
        // freesel, rectsel and text tools always show numbers limited to fit into image area
        case TOOL_FREESEL:
        case TOOL_RECTSEL:
        case TOOL_TEXT:
            if (xRel < 0)
                xRel = (pt.x < 0) ? -g_ptStart.x : xRel;
            else if (pt.x > imageModel.GetWidth())
                xRel = imageModel.GetWidth() - g_ptStart.x;
            if (yRel < 0)
                yRel = (pt.y < 0) ? -g_ptStart.y : yRel;
            else if (pt.y > imageModel.GetHeight())
                yRel = imageModel.GetHeight() - g_ptStart.y;
            break;

        // while drawing, update cursor coordinates only for tools 3, 7, 8, 9, 14
        case TOOL_RUBBER:
        case TOOL_PEN:
        case TOOL_BRUSH:
        case TOOL_AIRBRUSH:
        case TOOL_SHAPE:
        {
            CStringW strCoord;
            strCoord.Format(L"%ld, %ld", pt.x, pt.y);
            ::SendMessageW(g_hStatusBar, SB_SETTEXT, 1, (LPARAM)(LPCWSTR)strCoord);
            break;
        }
        default:
            break;
    }

    // rectsel and shape tools always show non-negative numbers when drawing
    if (IsBoundedFigureTool(m_activeTool))
    {
        CRect rc = FigureBounds(g_ptStart, g_ptEnd, !!registrySettings.ShapeFromCenter, GetAsyncKeyState(VK_SHIFT)<0);
        xRel = rc.Width(); yRel = rc.Height();
    }
    if (m_activeTool == TOOL_RECTSEL || m_activeTool == TOOL_SHAPE)
    {
        xRel = labs(xRel);
        yRel = labs(yRel);
    }

    if (wParam & MK_LBUTTON)
    {
        OnMouseMove(TRUE, pt.x, pt.y);
        canvasWindow.Invalidate(FALSE);
        if(IsBoundedFigureTool(m_activeTool))
        {
            CRect rc=FigureBounds(g_ptStart,g_ptEnd,!!registrySettings.ShapeFromCenter,GetAsyncKeyState(VK_SHIFT)<0);
            xRel=rc.Width();yRel=rc.Height();
        }
        if ((m_activeTool >= TOOL_TEXT) || IsSelection())
        {
            CStringW strSize;
            if ((m_activeTool >= TOOL_LINE) && (GetAsyncKeyState(VK_SHIFT) < 0))
                yRel = xRel;
            strSize.Format(L"%ld x %ld", xRel, yRel);
            ::SendMessageW(g_hStatusBar, SB_SETTEXT, 2, (LPARAM)(LPCWSTR)strSize);
        }
    }

    if ((wParam & MK_RBUTTON) && !(wParam & MK_LBUTTON))
    {
        OnMouseMove(FALSE, pt.x, pt.y);
        canvasWindow.Invalidate(FALSE);
        if (m_activeTool >= TOOL_TEXT)
        {
            CStringW strSize;
            if ((m_activeTool >= TOOL_LINE) && (GetAsyncKeyState(VK_SHIFT) < 0))
                yRel = xRel;
            strSize.Format(L"%ld x %ld", xRel, yRel);
            ::SendMessageW(g_hStatusBar, SB_SETTEXT, 2, (LPARAM)(LPCWSTR)strSize);
        }
    }
    ShowRotationAngle();
}

bool ToolsModel::ExportFigure(FigureClipboardData& data) const
{ return m_pToolObject && m_pToolObject->ExportFigure(data); }
bool ToolsModel::ImportFigure(const FigureClipboardData& data)
{
    if(!ValidFigureClipboard(data))return false;
    SetActiveTool(TOOL_FIGURE);registrySettings.FigureKind=data.kind;
    SetFigureRounding(data.rounding,data.kind);
    paletteModel.SelectPalette(PAL_TYPE(data.palette));
    paletteModel.SetFgColor(data.foreground);paletteModel.SetBgColor(data.background);
    SetLineWidth(data.lineWidth);SetShapeStyle(data.style);
    bool result=m_pToolObject->ImportFigure(data,canvasWindow.VisibleImageOrigin());
    NotifyToolChanged();NotifyToolSettingsChanged();return result;
}
