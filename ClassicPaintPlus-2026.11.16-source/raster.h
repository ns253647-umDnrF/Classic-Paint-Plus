// Internal image surfaces use BGRT: T = 255 - opacity. Win32 GDI writes a zero
// reserved byte when drawing, which correctly makes newly painted pixels opaque.
// File/codec buffers use ordinary straight-alpha ARGB instead.
#pragma once
#include <vector>
struct RasterImage
{
    int width=0,height=0;
    std::vector<DWORD> pixels;
    bool Valid() const { return width>0 && height>0 && pixels.size()==size_t(width)*height; }
};
bool ReadRaster(HBITMAP bitmap,RasterImage& image,bool opaque=false);
HBITMAP MakeRasterBitmap(const RasterImage& image);
bool BitmapHasAlpha(HBITMAP bitmap);
HBITMAP MakeTransparentBitmap(int width,int height);
RasterImage ResizeRaster(const RasterImage& source,int width,int height,int interpolation=2);
HBITMAP CopyRasterBitmap(HBITMAP source,int width=0,int height=0,int interpolation=2);
void FlattenRaster(RasterImage& image,COLORREF background=RGB(255,255,255));
void FlattenCanvasSurface(HDC dc,const RECT* area=NULL,bool checker=true);
void DrawRasterPreview(HDC dc,const RECT& target,HBITMAP bitmap,bool checker=true);
bool BlendRasterSelection(HDC dc,const RECT& target,HBITMAP premultiplied,BYTE opacity);
void InvertRasterColors(HDC dc,const RECT& rect);
void ClearRasterArea(HDC dc,const RECT& rect,HBITMAP mask=NULL);
bool ReadImageBytes(LPCWSTR path,std::vector<BYTE>& data);
bool WriteImageBytes(LPCWSTR path,const std::vector<BYTE>& data);
