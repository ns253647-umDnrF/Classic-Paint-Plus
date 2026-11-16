#include <vector>
#include <new>
#include "precomp.h"
#include "raster.h"

namespace {
struct Pixels
{
    DIBSECTION dib={};
    bool topDown=false;
    explicit Pixels(HBITMAP bitmap,bool top=false):topDown(top) { GetObject(bitmap,sizeof(dib),&dib);GdiFlush(); }
    bool direct() const { return dib.dsBm.bmBits && dib.dsBm.bmBitsPixel==32; }
    int width() const { return dib.dsBm.bmWidth; }
    int height() const { return dib.dsBm.bmHeight; }
    DWORD* row(int y) const
    {
        // GetObject normalizes biHeight on Windows, even for a top-down DIB.
        // Editor DIBs are bottom-up; the resampler explicitly supplies top-down.
        int index=topDown?y:height()-1-y;
        return reinterpret_cast<DWORD*>(static_cast<BYTE*>(dib.dsBm.bmBits)+size_t(index)*dib.dsBm.bmWidthBytes);
    }
};
struct Runtime
{
    ULONG_PTR token=0;
    Runtime() { Gdiplus::GdiplusStartupInput in;Gdiplus::GdiplusStartup(&token,&in,NULL); }
    ~Runtime() { if(token) Gdiplus::GdiplusShutdown(token); }
};
}
bool ReadRaster(HBITMAP bitmap,RasterImage& image,bool opaque)
{
    Pixels source(bitmap);
    int w=source.width(),h=source.height();
    if(w<=0 || h<=0 || w>32767 || h>32767 || size_t(w)*h>INT_MAX/4) return false;
    image.width=w;image.height=h;image.pixels.resize(size_t(w)*h);
    if(source.direct() && !opaque)
    {
        for(int y=0;y<h;++y) for(int x=0;x<w;++x)
            image.pixels[size_t(y)*w+x]=(source.row(y)[x]&0xffffff) |
                (opaque?0xff000000:(source.row(y)[x]^0xff000000)&0xff000000);
        return true;
    }
    BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(NULL);int rows=GetDIBits(dc,bitmap,0,h,image.pixels.data(),&info,DIB_RGB_COLORS);DeleteDC(dc);
    for(auto& pixel:image.pixels) pixel|=0xff000000;
    return rows==h;
}
HBITMAP MakeRasterBitmap(const RasterImage& image)
{
    if(!image.Valid()) return NULL;
    HBITMAP bitmap=CreateDIBWithProperties(image.width,image.height);Pixels target(bitmap);
    if(!target.direct()) { DeleteObject(bitmap);return NULL; }
    for(int y=0;y<image.height;++y) for(int x=0;x<image.width;++x)
        target.row(y)[x]=image.pixels[size_t(y)*image.width+x]^0xff000000;
    return bitmap;
}
bool BitmapHasAlpha(HBITMAP bitmap)
{
    Pixels source(bitmap);if(!source.direct()) return false;
    for(int y=0;y<source.height();++y) for(int x=0;x<source.width();++x)
        if(source.row(y)[x]&0xff000000) return true;
    return false;
}
HBITMAP MakeTransparentBitmap(int width,int height)
{
    HBITMAP bitmap=CreateDIBWithProperties(width,height);Pixels pixels(bitmap);
    if(!pixels.direct()) return bitmap;
    for(int y=0;y<height;++y) for(int x=0;x<width;++x) pixels.row(y)[x]=0xff000000;
    return bitmap;
}
RasterImage ResizeRaster(const RasterImage& source,int width,int height,int interpolation)
{
    RasterImage out;if(!source.Valid() || width<=0 || height<=0 || size_t(width)*height>INT_MAX/4) return out;
    if(width==source.width && height==source.height) return source;
    out.width=width;out.height=height;out.pixels.resize(size_t(width)*height);
    if(interpolation==0)
    {
        for(int y=0;y<height;++y) for(int x=0;x<width;++x)
            out.pixels[size_t(y)*width+x]=source.pixels[size_t(LONGLONG(y)*source.height/height)*source.width+LONGLONG(x)*source.width/width];
        return out;
    }
    static Runtime runtime;std::vector<DWORD> premult=source.pixels;
    for(auto& p:premult)
    {
        unsigned a=p>>24;p=(a<<24)|(((p>>16&255)*a+127)/255<<16)|(((p>>8&255)*a+127)/255<<8)|((p&255)*a+127)/255;
    }
    Gdiplus::Bitmap from(source.width,source.height,source.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(premult.data()));
    Gdiplus::Bitmap to(width,height,width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(out.pixels.data()));
    Gdiplus::Graphics g(&to);g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    g.SetInterpolationMode(interpolation==1?Gdiplus::InterpolationModeHighQualityBilinear:Gdiplus::InterpolationModeHighQualityBicubic);
    Gdiplus::ImageAttributes attributes;attributes.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
    if(g.DrawImage(&from,Gdiplus::Rect(0,0,width,height),0,0,source.width,source.height,Gdiplus::UnitPixel,&attributes)!=Gdiplus::Ok) return {};
    g.Flush(Gdiplus::FlushIntentionSync);
    for(auto& p:out.pixels)
    {
        unsigned a=p>>24;if(!a) { p=0;continue; }
        p=(a<<24)|(min(255u,((p>>16&255)*255+a/2)/a)<<16)|
            (min(255u,((p>>8&255)*255+a/2)/a)<<8)|min(255u,((p&255)*255+a/2)/a);
    }
    return out;
}
HBITMAP CopyRasterBitmap(HBITMAP source,int width,int height,int interpolation)
{
    RasterImage image;if(!ReadRaster(source,image)) return NULL;
    if(!width || !height) { width=image.width;height=image.height; }
    return MakeRasterBitmap(ResizeRaster(image,width,height,interpolation));
}
void FlattenRaster(RasterImage& image,COLORREF background)
{
    unsigned back[3]={GetBValue(background),GetGValue(background),GetRValue(background)};
    for(auto& p:image.pixels)
    {
        unsigned a=p>>24,result=0xff000000;
        for(int c=0;c<3;++c) result|=(((p>>(c*8)&255)*a+back[c]*(255-a)+127)/255)<<(c*8);
        p=result;
    }
}
void FlattenCanvasSurface(HDC dc,const RECT* area,bool checker)
{
    Pixels image(reinterpret_cast<HBITMAP>(GetCurrentObject(dc,OBJ_BITMAP)));if(!image.direct()) return;
    CRect r(0,0,image.width(),image.height());if(area) r.IntersectRect(r,area);
    for(int y=r.top;y<r.bottom;++y) for(int x=r.left;x<r.right;++x)
    {
        DWORD& p=image.row(y)[x];unsigned t=p>>24;if(!t) continue;
        unsigned gray=checker?(((x/8+y/8)&1)?224:255):255,result=0;
        COLORREF matte=DocumentAlpha().enabled?DocumentBackdrop():RGB(gray,gray,gray);
        unsigned back[]={GetBValue(matte),GetGValue(matte),GetRValue(matte)};
        for(int c=0;c<3;++c) result|=(((p>>(c*8)&255)*(255-t)+back[c]*t+127)/255)<<(c*8);
        p=result;
    }
}
void DrawRasterPreview(HDC dc,const RECT& target,HBITMAP bitmap,bool checker)
{
    HBITMAP copy=CopyRasterBitmap(bitmap);if(!copy) return;
    HDC memory=CreateCompatibleDC(dc);HGDIOBJ old=SelectObject(memory,copy);FlattenCanvasSurface(memory,NULL,checker);
    StretchBlt(dc,target.left,target.top,target.right-target.left,target.bottom-target.top,memory,0,0,GetDIBWidth(copy),GetDIBHeight(copy),SRCCOPY);
    SelectObject(memory,old);DeleteDC(memory);DeleteObject(copy);
}
bool BlendRasterSelection(HDC dc,const RECT& target,HBITMAP premultiplied,BYTE opacity)
{
    Pixels destination(reinterpret_cast<HBITMAP>(GetCurrentObject(dc,OBJ_BITMAP))),source(premultiplied,true);
    if(!destination.direct() || !source.direct() || GetMapMode(dc)!=MM_TEXT) return false;
    POINT origin={target.left,target.top};LPtoDP(dc,&origin,1);
    RECT logicalClip;int region=GetClipBox(dc,&logicalClip);
    CRect clip=target;clip.IntersectRect(clip,&logicalClip);
    for(int y=max(0,clip.top-target.top);y<min(source.height(),clip.bottom-target.top);++y)
    {
        int dy=y+origin.y;if(dy<0 || dy>=destination.height()) continue;
        for(int x=max(0,clip.left-target.left);x<min(source.width(),clip.right-target.left);++x)
        {
            int dx=x+origin.x;if(dx<0 || dx>=destination.width()) continue;
            if(region==COMPLEXREGION && !PtVisible(dc,x+target.left,y+target.top)) continue;
            DWORD s=source.row(y)[x],&d=destination.row(dy)[dx];unsigned sa=((s>>24)*opacity+127)/255;
            if(!sa) continue;
            unsigned da=255-(d>>24),weight=sa*255+da*(255-sa),a=(weight+127)/255;
            DWORD out=(255-a)<<24;
            for(int c=0;c<3;++c)
            {
                unsigned value=((s>>(c*8)&255)*opacity*255+(d>>(c*8)&255)*da*(255-sa)+weight/2)/weight;
                out|=min(255u,value)<<(c*8);
            }
            d=out;
        }
    }
    return true;
}
void InvertRasterColors(HDC dc,const RECT& rect)
{
    Pixels image(reinterpret_cast<HBITMAP>(GetCurrentObject(dc,OBJ_BITMAP)));
    if(!image.direct()) { InvertRect(dc,&rect);return; }
    CRect r(0,0,image.width(),image.height());r.IntersectRect(r,&rect);
    for(int y=r.top;y<r.bottom;++y) for(int x=r.left;x<r.right;++x) image.row(y)[x]^=0xffffff;
}
void ClearRasterArea(HDC dc,const RECT& rect,HBITMAP mask)
{
    Pixels image(reinterpret_cast<HBITMAP>(GetCurrentObject(dc,OBJ_BITMAP)));if(!image.direct()) return;
    HDC maskDC=NULL;HGDIOBJ old=NULL;
    if(mask) { maskDC=CreateCompatibleDC(NULL);old=SelectObject(maskDC,mask); }
    CRect r(0,0,image.width(),image.height());r.IntersectRect(r,&rect);
    for(int y=r.top;y<r.bottom;++y) for(int x=r.left;x<r.right;++x)
        if(!mask || GetPixel(maskDC,x-rect.left,y-rect.top)!=0) image.row(y)[x]=0xff000000;
    if(maskDC) { SelectObject(maskDC,old);DeleteDC(maskDC); }
}
bool ReadImageBytes(LPCWSTR path,std::vector<BYTE>& data)
{
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size={};bool ok=GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<=512*1024*1024;
    if(ok)
    {
        data.resize(size_t(size.QuadPart));DWORD count=0;
        ok=ReadFile(file,data.data(),DWORD(data.size()),&count,NULL) && count==data.size();
    }
    CloseHandle(file);return ok;
}
bool WriteImageBytes(LPCWSTR path,const std::vector<BYTE>& data)
{
    CStringW temporary;HANDLE file=INVALID_HANDLE_VALUE;
    for(unsigned attempt=0;attempt<32 && file==INVALID_HANDLE_VALUE;++attempt)
    {
        temporary.Format(L"%s.paint-%lu-%lu-%u.tmp",path,GetCurrentProcessId(),GetTickCount(),attempt);
        file=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(file==INVALID_HANDLE_VALUE && GetLastError()!=ERROR_FILE_EXISTS) return false;
    }
    if(file==INVALID_HANDLE_VALUE) return false;
    size_t pos=0;bool ok=true;
    while(pos<data.size())
    {
        DWORD written=0,chunk=DWORD(min(size_t(1<<20),data.size()-pos));
        if(!WriteFile(file,data.data()+pos,chunk,&written,NULL) || written!=chunk) { ok=false;break; }
        pos+=written;
    }
    if(ok) ok=FlushFileBuffers(file)!=FALSE;
    if(!CloseHandle(file)) ok=false;
    if(ok) ok=MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
    if(!ok) DeleteFileW(temporary);return ok;
}
