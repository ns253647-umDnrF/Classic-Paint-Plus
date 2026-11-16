#pragma once
enum TextWarpStyle { WARP_NONE,WARP_ARC,WARP_ARC_LOWER,WARP_ARC_UPPER,WARP_ARCH,WARP_BULGE,
    WARP_SHELL_LOWER,WARP_SHELL_UPPER,WARP_FLAG,WARP_WAVE,WARP_FISH,WARP_RISE,WARP_FISHEYE,
    WARP_INFLATE,WARP_SQUEEZE,WARP_TWIST,WARP_COUNT };
struct TextWarpOptions { int style=WARP_NONE,bend=50,horizontal=0,vertical=0;bool verticalAxis=false; };
struct WarpPoint { double x,y; };
class TextWarpMesh
{
    static const int Columns=40,Rows=32;
    std::vector<WarpPoint> points;
public:
    explicit TextWarpMesh(const TextWarpOptions& options,double aspect=1);
    WarpPoint Forward(WarpPoint p) const;
    WarpPoint Inverse(WarpPoint p) const;
    void ApplyMask(RasterImage& image,int outputHeight=0) const;
};
bool TextWarpActive(const TextWarpOptions& options);
bool ShowTextWarpDialog(HWND owner);
void DrawTextWarpIcon(HDC dc,const RECT& rect,COLORREF color);
#include "textwarp_resource.h"
