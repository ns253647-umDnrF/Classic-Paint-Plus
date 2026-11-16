/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Undo and redo functionality
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 *             Copyright 2023 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
 */

#include "precomp.h"

ImageModel imageModel;

/* FUNCTIONS ********************************************************/

void IMAGE_PART::clear()
{
    ::DeleteObject(m_hbmImage);
    m_hbmImage = NULL;
    m_rcPart.SetRectEmpty();
    m_bPartial = FALSE;
}

void ImageModel::NotifyImageChanged()
{
    NotifyDocumentAlpha();
    if(toolSettingsWindow.IsWindow()) { toolSettingsWindow.UpdateSelectionTransparency();toolSettingsWindow.UpdateSelectionAngle();toolSettingsWindow.UpdateObjectDimensions(); }
    if (canvasWindow.IsWindow())
    {
        canvasWindow.updateScrollRange();
        canvasWindow.Invalidate();
    }

    if (miniature.IsWindow())
        miniature.Invalidate();
}

ImageModel::ImageModel()
    : m_hDrawingDC(::CreateCompatibleDC(NULL))
    , m_currInd(0)
    , m_undoSteps(0)
    , m_redoSteps(0)
{
    m_hbmMaster = CreateColorDIB(1, 1, RGB(255, 255, 255));
    m_hbmOld = ::SelectObject(m_hDrawingDC, m_hbmMaster);

    g_imageSaved = TRUE;
}

ImageModel::~ImageModel()
{
    ::SelectObject(m_hDrawingDC, m_hbmOld); // De-select
    ::DeleteDC(m_hDrawingDC);
    ::DeleteObject(m_hbmMaster);
    ClearHistory();
}

BOOL ImageModel::SwapPart()
{
    IMAGE_PART& part = m_historyItems[m_currInd];
    if (!part.m_bPartial)
    {
        Swap(m_hbmMaster, part.m_hbmImage);
    }
    else
    {
        HBITMAP hbmMaster = LockBitmap();
        HBITMAP hbmPart = getSubImage(hbmMaster, part.m_rcPart);
        if(!hbmPart) { UnlockBitmap(hbmMaster);ShowOutOfMemory();return FALSE; }
        putSubImage(hbmMaster, part.m_rcPart, part.m_hbmImage);
        ::DeleteObject(part.m_hbmImage);
        part.m_hbmImage = hbmPart;
        UnlockBitmap(hbmMaster);
    }
    Swap(DocumentAlpha(),part.m_alpha);
    Swap(m_revision,part.m_revision);
    g_imageSaved=(m_revision==m_savedRevision);
    return TRUE;
}

void ImageModel::Undo(BOOL bClearRedo)
{
    ATLTRACE("%s: %d\n", __FUNCTION__, m_undoSteps);
    if (!CanUndo())
        return;

    selectionModel.HideSelection();

    m_currInd = (m_currInd + HISTORYSIZE - 1) % HISTORYSIZE; // Go previous
    ATLASSERT(m_hbmMaster != NULL);
    if(!SwapPart()) { m_currInd=(m_currInd+1)%HISTORYSIZE;return; }
    ::SelectObject(m_hDrawingDC, m_hbmMaster); // Re-select

    m_undoSteps--;
    if (bClearRedo)
        m_redoSteps = 0;
    else if (m_redoSteps < HISTORYSIZE - 1)
        m_redoSteps++;

    NotifyImageChanged();
}

void ImageModel::Redo()
{
    ATLTRACE("%s: %d\n", __FUNCTION__, m_redoSteps);
    if (!CanRedo())
        return;

    selectionModel.HideSelection();

    ATLASSERT(m_hbmMaster != NULL);
    if(!SwapPart()) return;
    m_currInd = (m_currInd + 1) % HISTORYSIZE; // Go next
    ::SelectObject(m_hDrawingDC, m_hbmMaster); // Re-select

    m_redoSteps--;
    if (m_undoSteps < HISTORYSIZE - 1)
        m_undoSteps++;

    NotifyImageChanged();
}

void ImageModel::ClearHistory()
{
    for (int i = 0; i < HISTORYSIZE; ++i)
    {
        m_historyItems[i].clear();
    }

    m_undoSteps = 0;
    m_redoSteps = 0;
}

void ImageModel::PushImageForUndo()
{
    HBITMAP hbm = CopyBitmap();
    if (hbm == NULL)
    {
        ShowOutOfMemory();
        return;
    }

    PushImageForUndo(hbm);
}

void ImageModel::PushImageForUndo(HBITMAP hbm)
{
    ATLTRACE("%s: %d\n", __FUNCTION__, m_currInd);

    if (hbm == NULL)
    {
        ShowOutOfMemory();
        return;
    }

    IMAGE_PART& part = m_historyItems[m_currInd];
    part.clear();
    part.m_alpha=DocumentAlpha();
    part.m_revision=m_revision;
    part.m_hbmImage = m_hbmMaster;
    m_hbmMaster = hbm;
    ::SelectObject(m_hDrawingDC, m_hbmMaster); // Re-select

    PushDone();
}

void ImageModel::PushImageForUndo(const RECT& rcPartial)
{
    ATLTRACE("%s: %d\n", __FUNCTION__, m_currInd);

    CRect rcImage = { 0, 0, GetWidth(), GetHeight() };
    CRect rc = rcPartial;
    if (!rc.IntersectRect(rc, rcImage))
        rc.SetRect(-1, -1, 0, 0);

    HBITMAP hbmMaster = LockBitmap();
    HBITMAP snapshot = getSubImage(hbmMaster, rc);
    UnlockBitmap(hbmMaster);
    if(!snapshot) { ShowOutOfMemory();return; }

    IMAGE_PART& part = m_historyItems[m_currInd];
    part.clear();
    part.m_alpha=DocumentAlpha();
    part.m_revision=m_revision;
    part.m_bPartial=TRUE;
    part.m_rcPart=rc;
    part.m_hbmImage=snapshot;

    PushDone();
}

void ImageModel::PushDone()
{
    m_revision=++m_nextRevision;
    m_currInd = (m_currInd + 1) % HISTORYSIZE; // Go next

    if (m_undoSteps < HISTORYSIZE - 1)
        m_undoSteps++;
    m_redoSteps = 0;

    g_imageSaved = FALSE;
    NotifyImageChanged();
}

void ImageModel::Crop(int nWidth, int nHeight, int nOffsetX, int nOffsetY)
{
    // We cannot create bitmaps of size zero
    if (nWidth <= 0)
        nWidth = 1;
    if (nHeight <= 0)
        nHeight = 1;

    // Canvas growth uses the same background replacement policy as erasing.
    HBITMAP hbmNew = ReplaceWithDocumentAlpha() ? MakeTransparentBitmap(nWidth, nHeight) :
        CreateColorDIB(nWidth, nHeight, paletteModel.GetBgColor());
    if (!hbmNew)
    {
        ShowOutOfMemory();
        return;
    }

    // Put the master image as a sub-image
    RECT rcPart = { -nOffsetX, -nOffsetY, GetWidth() - nOffsetX, GetHeight() - nOffsetY };
    HBITMAP hbmOld = imageModel.LockBitmap();
    putSubImage(hbmNew, rcPart, hbmOld);
    imageModel.UnlockBitmap(hbmOld);

    // Push it
    PushImageForUndo(hbmNew);

    NotifyImageChanged();
}

void ImageModel::SaveImage(LPCWSTR lpFileName)
{
    SaveDIBToFile(m_hbmMaster, lpFileName, TRUE);
}

BOOL ImageModel::IsImageSaved() const
{
    return g_imageSaved;
}

void ImageModel::MarkSaved()
{
    m_savedRevision=m_revision;
    g_imageSaved=TRUE;
}

void ImageModel::StretchSkew(int nStretchPercentX, int nStretchPercentY, int nSkewDegX, int nSkewDegY)
{
    if(nStretchPercentX<1 || nStretchPercentX>500 || nStretchPercentY<1 || nStretchPercentY>500 ||
        nSkewDegX < -89 || nSkewDegX>89 || nSkewDegY < -89 || nSkewDegY>89) return;
    int newWidth=max(1,GetWidth()*nStretchPercentX/100),newHeight=max(1,GetHeight()*nStretchPercentY/100);
    if(newWidth==GetWidth() && newHeight==GetHeight() && !nSkewDegX && !nSkewDegY) return;
    HBITMAP result=CopyDIBImage(m_hbmMaster,newWidth,newHeight);
    HDC dc=CreateCompatibleDC(NULL);
    for(int axis=0;axis<2 && result && dc;++axis)
    {
        int angle=axis?nSkewDegY:nSkewDegX;if(!angle) continue;
        HGDIOBJ old=SelectObject(dc,result);
        HBITMAP next=SkewDIB(dc,result,angle,axis!=0);
        SelectObject(dc,old);DeleteObject(result);result=next;
    }
    if(dc) DeleteDC(dc);
    if(!result || !dc) { DeleteObject(result);ShowOutOfMemory();return; }
    // Publish the result only after every stage succeeded, as one undo operation.
    PushImageForUndo(result);
    NotifyImageChanged();
}

int ImageModel::GetWidth() const
{
    return GetDIBWidth(m_hbmMaster);
}

int ImageModel::GetHeight() const
{
    return GetDIBHeight(m_hbmMaster);
}

void ImageModel::InvertColors()
{
    RECT rect = {0, 0, GetWidth(), GetHeight()};
    PushImageForUndo();
    InvertRasterColors(m_hDrawingDC, rect);
    NotifyImageChanged();
}

HDC ImageModel::GetDC()
{
    return m_hDrawingDC;
}

void ImageModel::FlipHorizontally()
{
    PushImageForUndo();
    StretchBlt(m_hDrawingDC, GetWidth() - 1, 0, -GetWidth(), GetHeight(), GetDC(), 0, 0,
               GetWidth(), GetHeight(), SRCCOPY);
    NotifyImageChanged();
}

void ImageModel::FlipVertically()
{
    PushImageForUndo();
    StretchBlt(m_hDrawingDC, 0, GetHeight() - 1, GetWidth(), -GetHeight(), GetDC(), 0, 0,
               GetWidth(), GetHeight(), SRCCOPY);
    NotifyImageChanged();
}

void ImageModel::RotateNTimes90Degrees(int iN)
{
    switch (iN)
    {
        case 1:
        case 3:
        {
            HBITMAP hbm = Rotate90DegreeBlt(m_hDrawingDC, GetWidth(), GetHeight(), iN == 1, FALSE);
            PushImageForUndo(hbm);
            break;
        }
        case 2:
        {
            PushImageForUndo();
            ::StretchBlt(m_hDrawingDC, GetWidth() - 1, GetHeight() - 1, -GetWidth(), -GetHeight(),
                         m_hDrawingDC, 0, 0, GetWidth(), GetHeight(), SRCCOPY);
            break;
        }
    }
    NotifyImageChanged();
}

void ImageModel::Clamp(POINT& pt) const
{
    pt.x = max(0, min(pt.x, GetWidth()));
    pt.y = max(0, min(pt.y, GetHeight()));
}

HBITMAP ImageModel::CopyBitmap()
{
    HBITMAP hBitmap = LockBitmap();
    HBITMAP ret = CopyDIBImage(hBitmap);
    UnlockBitmap(hBitmap);
    return ret;
}

BOOL ImageModel::IsBlackAndWhite()
{
    HBITMAP hBitmap = LockBitmap();
    BOOL bBlackAndWhite = IsBitmapBlackAndWhite(hBitmap);
    UnlockBitmap(hBitmap);
    return bBlackAndWhite;
}

void ImageModel::PushBlackAndWhite()
{
    HBITMAP hBitmap = LockBitmap();
    HBITMAP hNewBitmap = ConvertToBlackAndWhite(hBitmap);
    UnlockBitmap(hBitmap);

    PushImageForUndo(hNewBitmap);
}

HBITMAP ImageModel::LockBitmap()
{
    // NOTE: An app cannot select a bitmap into more than one device context at a time.
    ::SelectObject(m_hDrawingDC, m_hbmOld); // De-select
    HBITMAP hbmLocked = m_hbmMaster;
    m_hbmMaster = NULL;
    return hbmLocked;
}

void ImageModel::UnlockBitmap(HBITMAP hbmLocked)
{
    m_hbmMaster = hbmLocked;
    m_hbmOld = ::SelectObject(m_hDrawingDC, m_hbmMaster); // Re-select
}
