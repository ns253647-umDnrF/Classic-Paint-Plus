/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Keep track of selection parameters, notify listeners
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 *             Copyright 2019-2023 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
 */

#pragma once
#include "selectionresize.h"

class SelectionModel
{
private:
    HBITMAP m_hbmColor;
    HBITMAP m_hbmMask;
    HBITMAP m_hbmSourceMask=NULL; // Original cutout; independent of transformed selection.
    SelectionResampler m_resampler;
    int m_transparency = 0; // Per-selection amount: 0 opaque, 100 invisible.
    double m_angle=0,m_startAngle=0,m_pointerAngle=0;
    bool m_rotating=false,m_rotationWasLanded=false;
    BOOL m_rotationChanged=FALSE;
    POINT m_rotationPointer={};
    double m_skewX=0,m_skewY=0;
    bool m_rotateMode=false,m_editing=false,m_editMoved=false,m_editWasLanded=false,m_editCtrl=false,m_editArrow=false;
    BOOL m_editChanged=FALSE;
    HITTEST m_editHit=HIT_NONE;
    CRect m_editStart,m_editOldRect;
    bool m_editDupUndo=false;
    POINT m_editOrigin={},m_editCanvas={},m_editLast={};
    double m_editAngle=0,m_editSkewX=0,m_editSkewY=0;
    std::vector<POINT> m_contour;
    bool m_contourValid=false;
    void EndEdit(bool cancel);
    void BuildContour();

public:
    BOOL m_bShow;
    BOOL m_bContentChanged;
    CRect m_rc;    // in image pixel coordinates
    POINT m_ptHit; // in image pixel coordinates
    CRect m_rcOld; // in image pixel coordinates

    SelectionModel();
    ~SelectionModel();

    void SetRectFromPoints(const POINT& ptFrom, const POINT& ptTo);
    void setMask(const CRect& rc, HBITMAP hbmMask);

    int GetTransparency() const { return m_transparency; }
    bool SetTransparency(int percent);
    BOOL TakeOff();
    void Landing();
    BOOL IsLanded() const;
    void HideSelection();
    void DeleteSelection();
    HITTEST hitTest(POINT ptCanvas);
    void drawFrameOnCanvas(HDC hCanvasDC);
    void moveSelection(INT xDelta, INT yDelta);

    HBITMAP GetSelectionContents();
    void DrawBackground(HDC hDCImage, COLORREF crBg);
    void DrawBackgroundPoly(HDC hDCImage, COLORREF crBg);
    void DrawBackgroundRect(HDC hDCImage, COLORREF crBg);

    void DrawSelection(HDC hDCImage, COLORREF crBg, BOOL bBgTransparent, const CRect& rc, HBITMAP hbm);

    void DrawSelection(HDC hDCImage, COLORREF crBg, BOOL bBgTransparent)
    {
        return DrawSelection(hDCImage, crBg, bBgTransparent, m_rc);
    }

    void DrawSelection(HDC hDCImage, COLORREF crBg, BOOL bBgTransparent, const CRect& rc)
    {
        return DrawSelection(hDCImage, crBg, bBgTransparent, rc, m_hbmColor);
    }

    void InsertFromHBITMAP(HBITMAP hbmColor, INT x = 0, INT y = 0, HBITMAP hbmMask = NULL);

    // operation
    void FlipHorizontally();
    void FlipVertically();
    void RotateNTimes90Degrees(int iN);
    void StretchSkew(int nStretchPercentX, int nStretchPercentY, int nSkewDegX, int nSkewDegY);
    void InvertSelection();
    void InvertColors();
    // Takes ownership of mask (NULL means a full rectangle). 0 replace, 1 add, -1 subtract.
    bool CombineSelection(const CRect& bounds,HBITMAP mask,int operation);
    bool ReadCanvasMask(std::vector<BYTE>& pixels) const;
    bool ContainsImagePoint(POINT point) const;
    bool EditMouse(UINT message,WPARAM keys,POINT canvasPoint);
    bool SetFrameCursor(POINT canvasPoint);
    bool RotateMode() const { return m_rotateMode; }
    bool IsEditing() const { return m_editing; }
    bool SetSkew(double x,double y);
    double SkewX() const { return m_skewX; }
    double SkewY() const { return m_skewY; }

    void Dragging(HITTEST hit, POINT pt);
    void ClearMaskImage();
    void ClearColorImage();
    void NotifyContentChanged();

    void StretchSelection(BOOL bShrink);
    bool StartRotation(POINT canvasPoint);
    void UpdateRotation(POINT canvasPoint,bool snap);
    void UpdateRotationModifiers(bool snap) { UpdateRotation(m_rotationPointer,snap); }
    void EndRotation(bool cancel);
    bool IsRotating() const { return m_rotating; }
    double RotationAngle() const { return m_angle; }
    bool SetRotationAngle(double degrees);
    CRect DrawingBounds() const { return TransformedBounds(m_rc,m_angle,m_skewX,m_skewY); }

private:
    SelectionModel(const SelectionModel&);
    SelectionModel& operator=(const SelectionModel&);

    void ShiftPtStack(INT dx, INT dy);
    void SwapWidthAndHeight();
};
