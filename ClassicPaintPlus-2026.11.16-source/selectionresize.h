// Local selection interpolation, LGPL-2.0-or-later.
#pragma once
#include "selectionresize_resource.h"
#define IDC_RESAMPLE_PANEL 4810
#define IDC_RESAMPLE_FIRST 4811

class SelectionResampler
{
    HBITMAP scaled = NULL, source = NULL, mask = NULL;
    int width = 0, height = 0, mode = -1;
    COLORREF key = CLR_INVALID;
    double angle = 0, shearX = 0, shearY = 0;
public:
    ~SelectionResampler() { Reset(); }
    void Reset() { if (scaled) DeleteObject(scaled); scaled = NULL; }
    bool Draw(HDC dest, const RECT& rc, HBITMAP bitmap, HBITMAP maskBitmap, COLORREF colorKey, int algorithm, BYTE opacity=255,double rotation=0,double skewX=0,double skewY=0);
};
// Sequential shears have determinant one, including when both axes are skewed.
struct AffinePointD { double x,y; };
AffinePointD TransformPoint(double x,double y,const RECT& rect,double angle,double skewX=0,double skewY=0,bool inverse=false);
CRect TransformedBounds(const RECT& rect,double angle,double skewX=0,double skewY=0);
CRect RotatedSelectionBounds(const RECT& rect,double angle);
POINT RotateSelectionPoint(POINT point,const RECT& rect,double angle);

void CreateSelectionResizePanel(HWND owner, const RECT& bounds);
void UpdateSelectionResizePanel();
void LayoutSelectionResizePanel(const RECT& bounds);
