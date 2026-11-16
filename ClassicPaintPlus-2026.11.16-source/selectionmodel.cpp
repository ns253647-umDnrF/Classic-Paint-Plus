/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Keep track of selection parameters, notify listeners
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 *             Copyright 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
 */

#include "precomp.h"

SelectionModel selectionModel;

/* FUNCTIONS ********************************************************/

SelectionModel::SelectionModel()
    : m_hbmColor(NULL)
    , m_hbmMask(NULL)
    , m_bShow(FALSE)
    , m_bContentChanged(FALSE)
{
    m_rc.SetRectEmpty();
    m_rcOld.SetRectEmpty();
    m_ptHit = { -1, -1 };
}

SelectionModel::~SelectionModel()
{
    DeleteObject(m_hbmSourceMask);
    ClearColorImage();
    ClearMaskImage();
}

void SelectionModel::DrawBackgroundPoly(HDC hDCImage, COLORREF crBg)
{
    if (m_rcOld.IsRectEmpty())
        return;

    HGDIOBJ hbrOld = ::SelectObject(hDCImage, ::GetStockObject(DC_BRUSH));
    ::SetDCBrushColor(hDCImage, crBg);
    ::MaskBlt(hDCImage, m_rcOld.left, m_rcOld.top, m_rcOld.Width(), m_rcOld.Height(),
              hDCImage, m_rcOld.left, m_rcOld.top, m_hbmSourceMask, 0, 0, MAKEROP4(PATCOPY, SRCCOPY));
    ::SelectObject(hDCImage, hbrOld);
}

void SelectionModel::DrawBackgroundRect(HDC hDCImage, COLORREF crBg)
{
    if (m_rcOld.IsRectEmpty())
        return;

    Rect(hDCImage, m_rcOld.left, m_rcOld.top, m_rcOld.right, m_rcOld.bottom, crBg, crBg, 0, 1);
}

void SelectionModel::DrawBackground(HDC hDCImage, COLORREF crBg)
{
    if(m_rcOld.IsRectEmpty()) return;
    if(registrySettings.ReplaceDeletedWithAlpha && (DocumentAlpha().enabled || (m_hbmColor && BitmapHasAlpha(m_hbmColor))))
    {
        ClearRasterArea(hDCImage,m_rcOld,m_hbmSourceMask);
        return;
    }
    if (m_hbmSourceMask)
        DrawBackgroundPoly(hDCImage, crBg);
    else
        DrawBackgroundRect(hDCImage, crBg);
}

void
SelectionModel::DrawSelection(HDC hDCImage, COLORREF crBg, BOOL bBgTransparent, const CRect& rc,
                              HBITMAP hbm)
{
    if (rc.IsRectEmpty() || m_transparency==100)
        return;

    BITMAP bm;
    if (!GetObjectW(hbm, sizeof(BITMAP), &bm))
        return;

    COLORREF keyColor = (bBgTransparent ? crBg : CLR_INVALID);

    if (m_hbmMask || m_skewX || m_skewY || m_angle!=0 || m_transparency>0 || BitmapHasAlpha(hbm) ||
        BitmapHasAlpha(reinterpret_cast<HBITMAP>(GetCurrentObject(hDCImage,OBJ_BITMAP))) ||
        (registrySettings.SelectionInterpolation!=0 &&
        (rc.Width()!=bm.bmWidth || rc.Height()!=bm.bmHeight)))
    {
        bool drawn=m_resampler.Draw(hDCImage,rc,hbm,m_hbmMask,keyColor,
            registrySettings.SelectionInterpolation,BYTE(MulDiv(100-m_transparency,255,100)),m_angle,m_skewX,m_skewY);
        if(drawn || m_hbmMask || m_skewX || m_skewY || m_transparency>0 || m_angle!=0) return;
    }

    HDC hMemDC = CreateCompatibleDC(hDCImage);
    HGDIOBJ hbmOld = SelectObject(hMemDC, hbm);
    int oldMode = SetStretchBltMode(hDCImage, COLORONCOLOR);
    ColorKeyedMaskBlt(hDCImage, rc.left, rc.top, rc.Width(), rc.Height(),
                      hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, m_hbmMask, keyColor);
    SetStretchBltMode(hDCImage, oldMode);
    SelectObject(hMemDC, hbmOld);
    DeleteDC(hMemDC);
}

void SelectionModel::setMask(const CRect& rc, HBITMAP hbmMask)
{
    DeleteObject(m_hbmSourceMask);m_hbmSourceMask=NULL;
    m_resampler.Reset();
    if (m_hbmMask)
        ::DeleteObject(m_hbmMask);

    m_hbmMask = hbmMask;
    m_rc = m_rcOld = rc;
    m_angle=0;m_rotating=false;m_skewX=m_skewY=0;m_rotateMode=false;m_editing=false;m_contourValid=false;
    m_transparency=0;
}

HBITMAP SelectionModel::GetSelectionContents()
{
    HBITMAP hbmPart = m_hbmColor;
    if (!hbmPart)
    {
        HBITMAP hbmWhole = imageModel.LockBitmap();
        hbmPart = getSubImage(hbmWhole, m_rc);
        imageModel.UnlockBitmap(hbmWhole);
    }
    if (!hbmPart)
        return NULL;

    if(!m_angle && !m_skewX && !m_skewY && !m_hbmMask && !m_transparency && !toolsModel.IsBackgroundTransparent() &&
        GetDIBWidth(hbmPart)==m_rc.Width() && GetDIBHeight(hbmPart)==m_rc.Height())
    {
        if(hbmPart!=m_hbmColor) return hbmPart;
        return CopyDIBImage(hbmPart);
    }

    CRect bounds=DrawingBounds(),rc=m_rc;
    rc.OffsetRect(-bounds.left,-bounds.top);

    HDC hdcMem = ::CreateCompatibleDC(NULL);
    HBITMAP hbmNew = (DocumentAlpha().enabled || m_hbmMask || toolsModel.IsBackgroundTransparent() || m_skewX || m_skewY || m_angle || BitmapHasAlpha(hbmPart))
        ? MakeTransparentBitmap(bounds.Width(),bounds.Height())
        : CreateColorDIB(bounds.Width(), bounds.Height(), paletteModel.GetBgColor());
    if (!hbmNew || !hdcMem)
    {
        if (hbmNew) DeleteObject(hbmNew);
        if (hdcMem) DeleteDC(hdcMem);
        if (hbmPart != m_hbmColor) DeleteObject(hbmPart);
        return NULL;
    }
    HGDIOBJ hbmOld = ::SelectObject(hdcMem, hbmNew);
    DrawSelection(hdcMem, paletteModel.GetBgColor(), toolsModel.IsBackgroundTransparent(), rc, hbmPart);
    ::SelectObject(hdcMem, hbmOld);
    ::DeleteDC(hdcMem);

    if (hbmPart != m_hbmColor)
    {
        m_resampler.Reset();
        ::DeleteObject(hbmPart);
    }
    return hbmNew;
}

BOOL SelectionModel::IsLanded() const
{
    return !m_hbmColor;
}

bool SelectionModel::SetTransparency(int percent)
{
    if(percent<0 || percent>100 || !m_bShow || m_rc.IsRectEmpty()) return false;
    if(percent==m_transparency) return true;
    // Lift the original pixels before changing alpha; repeated slider changes
    // must never re-sample already blended pixels or push intermediate undo states.
    if(IsLanded() && !TakeOff()) return false;
    if(!m_hbmColor) return false;
    m_transparency=percent;m_bContentChanged=TRUE;
    imageModel.NotifyImageChanged();return true;
}

BOOL SelectionModel::TakeOff()
{
    if (!IsLanded() || m_rc.IsRectEmpty())
        return FALSE;

    // Get the contents of the selection area
    ClearColorImage();
    // Keep the original RGBA pixels. The tool's color key, shape mask and
    // opacity are combined only when drawing/exporting the floating selection.
    // Baking the key here would prevent toggling its background mode later.
    HBITMAP whole=imageModel.LockBitmap();
    m_hbmColor=getSubImage(whole,m_rc);
    imageModel.UnlockBitmap(whole);
    if(!m_hbmColor) return FALSE;

    // Geometry masks belong to the selection, not to the active selection tool.
    if(m_hbmMask)
    {
        HBITMAP original=CopyMonoImage(m_hbmMask);
        if(!original) { ClearColorImage();return FALSE; }
        DeleteObject(m_hbmSourceMask);m_hbmSourceMask=original;
    }

    // Save the selection area
    m_rcOld = m_rc;

    imageModel.NotifyImageChanged();
    return TRUE;
}

void SelectionModel::Landing()
{
    if (IsLanded() && !m_bShow)
    {
        imageModel.NotifyImageChanged();
        return;
    }

    if (m_bContentChanged ||
        (!m_rc.EqualRect(m_rcOld) && !m_rc.IsRectEmpty() && !m_rcOld.IsRectEmpty()))
    {
        CRect rc;
        CRect bounds=DrawingBounds();rc.UnionRect(bounds, m_rcOld);
        imageModel.PushImageForUndo(rc);

        canvasWindow.m_drawing = FALSE;
        toolsModel.OnDrawOverlayOnImage(imageModel.GetDC());
    }

    HideSelection();
}

void SelectionModel::InsertFromHBITMAP(HBITMAP hbmColor, INT x, INT y, HBITMAP hbmMask)
{
    DeleteObject(m_hbmSourceMask);m_hbmSourceMask=NULL;
    ::DeleteObject(m_hbmColor);
    m_hbmColor = hbmColor;
    m_angle=0;m_rotating=false;m_skewX=m_skewY=0;m_rotateMode=false;m_editing=false;m_contourValid=false;
    m_transparency=0;

    m_rc.left = x;
    m_rc.top = y;
    m_rc.right = x + GetDIBWidth(hbmColor);
    m_rc.bottom = y + GetDIBHeight(hbmColor);

    if (hbmMask)
    {
        ::DeleteObject(m_hbmMask);
        m_hbmMask = hbmMask;
    }
    else
    {
        ClearMaskImage();
    }

    NotifyContentChanged();
}

void SelectionModel::FlipHorizontally()
{
    TakeOff();

    HDC hdcMem = ::CreateCompatibleDC(NULL);
    if (m_hbmMask)
    {
        int width=GetDIBWidth(m_hbmMask),height=GetDIBHeight(m_hbmMask);
        ::SelectObject(hdcMem, m_hbmMask);
        ::StretchBlt(hdcMem, width - 1, 0, -width, height,
                     hdcMem, 0, 0, width, height, SRCCOPY);
    }
    if (m_hbmColor)
    {
        int width=GetDIBWidth(m_hbmColor),height=GetDIBHeight(m_hbmColor);
        ::SelectObject(hdcMem, m_hbmColor);
        ::StretchBlt(hdcMem, width - 1, 0, -width, height,
                     hdcMem, 0, 0, width, height, SRCCOPY);
    }
    ::DeleteDC(hdcMem);

    NotifyContentChanged();
}

void SelectionModel::FlipVertically()
{
    TakeOff();

    HDC hdcMem = ::CreateCompatibleDC(NULL);
    if (m_hbmMask)
    {
        int width=GetDIBWidth(m_hbmMask),height=GetDIBHeight(m_hbmMask);
        ::SelectObject(hdcMem, m_hbmMask);
        ::StretchBlt(hdcMem, 0, height - 1, width, -height,
                     hdcMem, 0, 0, width, height, SRCCOPY);
    }
    if (m_hbmColor)
    {
        int width=GetDIBWidth(m_hbmColor),height=GetDIBHeight(m_hbmColor);
        ::SelectObject(hdcMem, m_hbmColor);
        ::StretchBlt(hdcMem, 0, height - 1, width, -height,
                     hdcMem, 0, 0, width, height, SRCCOPY);
    }
    ::DeleteDC(hdcMem);

    NotifyContentChanged();
}

void SelectionModel::RotateNTimes90Degrees(int iN)
{
    if(iN<1 || iN>3) return;
    TakeOff();if(!m_hbmColor) return;
    if(m_angle) { m_angle=fmod(m_angle+iN*90,360.0);NotifyContentChanged();return; }
    if(iN==2) { FlipHorizontally();FlipVertically();return; }
    HDC dc=CreateCompatibleDC(NULL);if(!dc) return;
    auto rotate=[&](HBITMAP bitmap,BOOL mono)->HBITMAP {
        if(!bitmap) return NULL;
        HGDIOBJ old=SelectObject(dc,bitmap);
        HBITMAP result=Rotate90DegreeBlt(dc,GetDIBWidth(bitmap),GetDIBHeight(bitmap),iN==1,mono);
        SelectObject(dc,old);return result;
    };
    HBITMAP color=rotate(m_hbmColor,FALSE),mask=rotate(m_hbmMask,TRUE);
    DeleteDC(dc);
    if(!color || (m_hbmMask && !mask)) { DeleteObject(color);DeleteObject(mask);ShowOutOfMemory();return; }
    DeleteObject(m_hbmColor);DeleteObject(m_hbmMask);m_hbmColor=color;m_hbmMask=mask;
    SwapWidthAndHeight();NotifyContentChanged();
}

void SelectionModel::StretchSkew(int nStretchPercentX, int nStretchPercentY, int nSkewDegX, int nSkewDegY)
{
    if(nStretchPercentX<1 || nStretchPercentX>500 || nStretchPercentY<1 || nStretchPercentY>500 ||
        nSkewDegX < -89 || nSkewDegX>89 || nSkewDegY < -89 || nSkewDegY>89) return;
    if(nStretchPercentX==100 && nStretchPercentY==100 && !nSkewDegX && !nSkewDegY) return;
    TakeOff();if(!m_hbmColor) return;
    LONGLONG width=LONGLONG(m_rc.Width())*nStretchPercentX/100;
    LONGLONG height=LONGLONG(m_rc.Height())*nStretchPercentY/100;
    if(width>32767 || height>32767) return;
    int newWidth=max(1,int(width)),newHeight=max(1,int(height));
    // Keep the original source for ordinary percentage/handle resizing.
    if(!nSkewDegX && !nSkewDegY)
    {
        m_rc.right=m_rc.left+newWidth;m_rc.bottom=m_rc.top+newHeight;
        NotifyContentChanged();return;
    }
    // Materialize the displayed size before skewing, even when percentages are
    // 100: dragging handles may already have changed the selection dimensions.
    HBITMAP color=CopyRasterBitmap(m_hbmColor,newWidth,newHeight,registrySettings.SelectionInterpolation);
    HBITMAP mask=m_hbmMask?CopyMonoImage(m_hbmMask,newWidth,newHeight):CreateMonoBitmap(newWidth,newHeight,TRUE);
    HDC dc=CreateCompatibleDC(NULL);
    for(int axis=0;axis<2 && dc && color && mask;++axis)
    {
        int angle=axis?nSkewDegY:nSkewDegX;if(!angle) continue;
        auto skew=[&](HBITMAP& bitmap,BOOL mono) {
            HGDIOBJ old=SelectObject(dc,bitmap);
            HBITMAP next=SkewDIB(dc,bitmap,angle,axis!=0,mono);
            SelectObject(dc,old); // DeleteObject fails while selected into a DC.
            DeleteObject(bitmap);bitmap=next;
        };
        skew(color,FALSE);skew(mask,TRUE);
    }
    if(dc) DeleteDC(dc);
    if(!dc || !color || !mask) { DeleteObject(color);DeleteObject(mask);ShowOutOfMemory();return; }
    DeleteObject(m_hbmColor);DeleteObject(m_hbmMask);m_hbmColor=color;m_hbmMask=mask;
    m_rc.right=m_rc.left+GetDIBWidth(color);m_rc.bottom=m_rc.top+GetDIBHeight(color);
    m_bShow=TRUE;NotifyContentChanged();
}

void SelectionModel::SetRectFromPoints(const POINT& ptFrom, const POINT& ptTo)
{
    m_rc = CRect(ptFrom, ptTo);
    m_rc.NormalizeRect();
}

void SelectionModel::Dragging(HITTEST hit, POINT pt)
{
    POINT actualPoint=pt;double cx=(m_rc.left+m_rc.right)/2.0,cy=(m_rc.top+m_rc.bottom)/2.0;
    bool resize=m_angle && hit>=HIT_UPPER_LEFT && hit<=HIT_LOWER_RIGHT;
    if(resize)
    {
        double a=m_angle*M_PI/180,dx=pt.x-m_ptHit.x,dy=pt.y-m_ptHit.y;
        pt.x=m_ptHit.x+LONG(lround(dx*cos(a)+dy*sin(a)));
        pt.y=m_ptHit.y+LONG(lround(-dx*sin(a)+dy*cos(a)));
    }
    switch (hit)
    {
        case HIT_NONE:
            break;
        case HIT_UPPER_LEFT:
            m_rc.left += pt.x - m_ptHit.x;
            m_rc.top += pt.y - m_ptHit.y;
            break;
        case HIT_UPPER_CENTER:
            m_rc.top += pt.y - m_ptHit.y;
            break;
        case HIT_UPPER_RIGHT:
            m_rc.right += pt.x - m_ptHit.x;
            m_rc.top += pt.y - m_ptHit.y;
            break;
        case HIT_MIDDLE_LEFT:
            m_rc.left += pt.x - m_ptHit.x;
            break;
        case HIT_MIDDLE_RIGHT:
            m_rc.right += pt.x - m_ptHit.x;
            break;
        case HIT_LOWER_LEFT:
            m_rc.left += pt.x - m_ptHit.x;
            m_rc.bottom += pt.y - m_ptHit.y;
            break;
        case HIT_LOWER_CENTER:
            m_rc.bottom += pt.y - m_ptHit.y;
            break;
        case HIT_LOWER_RIGHT:
            m_rc.right += pt.x - m_ptHit.x;
            m_rc.bottom += pt.y - m_ptHit.y;
            break;
        case HIT_BORDER:
        case HIT_INNER:
            m_rc.OffsetRect(pt.x - m_ptHit.x, pt.y - m_ptHit.y);
            break;
    }
    if(resize)
    {
        double dx=(m_rc.left+m_rc.right)/2.0-cx,dy=(m_rc.top+m_rc.bottom)/2.0-cy,a=m_angle*M_PI/180;
        m_rc.OffsetRect(LONG(lround(dx*cos(a)-dy*sin(a)-dx)),LONG(lround(dx*sin(a)+dy*cos(a)-dy)));
    }
    m_ptHit = actualPoint;
}

void SelectionModel::ClearMaskImage()
{
    m_contourValid=false;m_contour.clear();
    m_resampler.Reset();
    if (m_hbmMask)
    {
        ::DeleteObject(m_hbmMask);
        m_hbmMask = NULL;
    }
}

void SelectionModel::ClearColorImage()
{
    m_resampler.Reset();
    if (m_hbmColor)
    {
        ::DeleteObject(m_hbmColor);
        m_hbmColor = NULL;
    }
}

void SelectionModel::HideSelection()
{
    ResetSelectionParameterLocks();
    DeleteObject(m_hbmSourceMask);m_hbmSourceMask=NULL;
    m_angle=0;m_rotating=false;m_skewX=m_skewY=0;m_rotateMode=false;m_editing=false;m_contourValid=false;
    m_bShow = m_bContentChanged = FALSE;
    m_transparency=0;
    ClearColorImage();
    ClearMaskImage();
    m_rc.SetRectEmpty();
    m_rcOld.SetRectEmpty();
    imageModel.NotifyImageChanged();
}

void SelectionModel::DeleteSelection()
{
    if (!m_bShow)
        return;

    if(IsLanded() && !TakeOff())return;
    imageModel.PushImageForUndo();
    DrawBackground(imageModel.GetDC(), paletteModel.GetBgColor());

    HideSelection();
}

void SelectionModel::InvertColors()
{
    TakeOff();
    if(!m_hbmColor)return;

    BITMAP bm;
    ::GetObjectW(m_hbmColor, sizeof(bm), &bm);

    HDC hdc = ::CreateCompatibleDC(NULL);
    HGDIOBJ hbmOld = ::SelectObject(hdc, m_hbmColor);
    RECT rc = { 0, 0, bm.bmWidth, bm.bmHeight };
    InvertRasterColors(hdc,rc);
    ::SelectObject(hdc, hbmOld);
    ::DeleteDC(hdc);

    NotifyContentChanged();
}

void SelectionModel::NotifyContentChanged()
{
    m_contourValid=false;
    m_resampler.Reset();
    m_bContentChanged = TRUE;
    imageModel.NotifyImageChanged();
}

void SelectionModel::SwapWidthAndHeight()
{
    INT cx = m_rc.Width(), cy = m_rc.Height();
    m_rc.right = m_rc.left + cy;
    m_rc.bottom = m_rc.top + cx;
}

void SelectionModel::moveSelection(INT xDelta, INT yDelta)
{
    if (!m_bShow)
        return;

    TakeOff();
    m_rc.OffsetRect(xDelta, yDelta);
    canvasWindow.Invalidate();
}

void SelectionModel::StretchSelection(BOOL bShrink)
{
    if (!m_bShow)
        return;

    TakeOff();

    INT cx = m_rc.Width(), cy = m_rc.Height();

    if (bShrink)
        m_rc.InflateRect(-cx / 4, -cy / 4);
    else
        m_rc.InflateRect(+cx / 2, +cy / 2);

    // The selection area must exist there
    if (m_rc.Width() <= 0)
        m_rc.right = m_rc.left + 1;
    if (m_rc.Height() <= 0)
        m_rc.bottom = m_rc.top + 1;

    imageModel.NotifyImageChanged();
}
