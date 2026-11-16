#include <vector>
#include <initializer_list>
#include "precomp.h"
#include "exactspray_resource.h"

namespace
{
struct IconRuntime
{
    ULONG_PTR token=0;
    IconRuntime() { Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&token,&input,NULL); }
    ~IconRuntime() { if(token) Gdiplus::GdiplusShutdown(token); }
};
void ToolIcon(Gdiplus::Graphics& g,int tool)
{
    using namespace Gdiplus;
    bool whiteInk=registrySettings.DarkTheme && (tool==0 || tool==1 || tool==9 || tool==10 || tool==11);
    Color ink=whiteInk?Color::White:Color(255,25,25,25);
    Color yellow(255,255,220,38),teal(255,36,190,190),blue(255,45,99,230);
    Pen pen(ink,.85f),fine(ink,.65f),dash(ink,.8f);dash.SetDashStyle(DashStyleDash);
    SolidBrush white(Color(255,255,255,255)),gray(Color(255,195,195,195)),black(ink),cyan(teal),gold(yellow),azure(blue);
    auto poly=[&](std::initializer_list<PointF> points,Gdiplus::Brush* fill=nullptr,Pen* outline=nullptr)
    {
        std::vector<PointF> p(points);
        if(fill) g.FillPolygon(fill,p.data(),int(p.size()));
        g.DrawPolygon(outline?outline:&pen,p.data(),int(p.size()));
    };
    switch(tool)
    {
    case 10: g.DrawLine(&pen,2.f,2.f,14.f,14.f);break;
    case 11: g.DrawBezier(&pen,9.f,1.f,18.f,7.f,0.f,8.f,8.f,15.f);break;
    case 12: g.FillRectangle(&gray,1.f,3.f,14.f,10.f);g.DrawRectangle(&pen,1.f,3.f,14.f,10.f);break;
    case 13: poly({{3,2},{13,2},{7,11},{14,11},{12,14},{1,14},{6,5}},&gray);break;
    case 14: g.FillEllipse(&gray,1.f,4.f,14.f,8.f);g.DrawEllipse(&pen,1.f,4.f,14.f,8.f);break;
    case 15:
        { GraphicsPath path;path.AddArc(1,3,5,5,180,90);path.AddArc(10,3,5,5,270,90);
          path.AddArc(10,8,5,5,0,90);path.AddArc(1,8,5,5,90,90);path.CloseFigure();g.FillPath(&gray,&path);g.DrawPath(&pen,&path); }break;
    }
}
void CrispDarkSymbol(Gdiplus::Graphics& g,int tool,int side)
{
    using namespace Gdiplus;
    g.SetPixelOffsetMode(PixelOffsetModeNone);
    Color color=registrySettings.DarkTheme?Color::White:Color::Black;
    float stroke=float(max(2,MulDiv(2,side,24)));Pen pen(color,stroke);
    pen.SetStartCap(LineCapRound);pen.SetEndCap(LineCapRound);pen.SetLineJoin(LineJoinRound);
    auto p=[&](float x,float y) { return PointF(float(lround(x*side/16)),float(lround(y*side/16))); };
    if(tool==10) g.DrawLine(&pen,p(2,2),p(14,14));
    else if(tool==11) g.DrawBezier(&pen,p(9,1),p(18,7),p(0,8),p(8,15));
}
// These are downsampled crops of the supplied artwork, not a reconstruction.
// Load them at their final pixel size so the image list does not resample them.
void CopySuppliedTools(void* destination,int stride,int percent,int side)
{
    UINT id=percent==150?IDR_EXACT_TOOLS_150:percent==200?IDR_EXACT_TOOLS_200:IDR_EXACT_TOOLS_250;
    HRSRC resource=FindResourceW(g_hinstExe,MAKEINTRESOURCEW(id),RT_RCDATA);if(!resource) return;
    DWORD bytes=SizeofResource(g_hinstExe,resource);
    HGLOBAL loaded=LoadResource(g_hinstExe,resource);const void* sourceBytes=loaded?LockResource(loaded):NULL;
    if(!sourceBytes || !bytes || !destination) return;
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!memory) return;
    void* data=GlobalLock(memory);if(!data) { GlobalFree(memory);return; }
    CopyMemory(data,sourceBytes,bytes);GlobalUnlock(memory);
    IStream* stream=NULL;if(FAILED(CreateStreamOnHGlobal(memory,TRUE,&stream))) { GlobalFree(memory);return; }
    {
        Gdiplus::Bitmap artwork(stream);
        Gdiplus::Rect rect(0,0,10*side,side);Gdiplus::BitmapData data={};
        if(artwork.GetWidth()==UINT(rect.Width) && artwork.GetHeight()==UINT(side) &&
           artwork.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&data)==Gdiplus::Ok)
        {
            for(int y=0;y<side;++y) CopyMemory(static_cast<BYTE*>(destination)+size_t(y)*stride,
                static_cast<BYTE*>(data.Scan0)+y*data.Stride,rect.Width*4);
            artwork.UnlockBits(&data);
        }
    }
    stream->Release();
    if(registrySettings.DarkTheme)
    {
        // Tint only the three monochrome glyphs. Keep every original alpha
        // value, so their contours and antialiasing are not reconstructed.
        for(int tool:{0,1,9}) for(int y=0;y<side;++y)
        {
            DWORD* row=reinterpret_cast<DWORD*>(static_cast<BYTE*>(destination)+size_t(y)*stride);
            for(int x=tool*side;x<(tool+1)*side;++x)
                if(row[x]>>24) row[x]=(row[x]&0xff000000)|0x00ffffff;
        }
        // Whiten only the pouring paint at the left of the supplied bucket.
        // Its original coverage is retained; the bucket, handle and cyan stay intact.
        for(int y=0;y<side;++y)
        {
            DWORD* row=reinterpret_cast<DWORD*>(static_cast<BYTE*>(destination)+size_t(y)*stride);
            float py=(y+.5f)*40/side;
            float edge=py<11?14.f:py<14?25.f-py:py<16?24.f-py:8.f;
            for(int x=0;(x+.5f)*40/side<edge;++x)
            {
                DWORD& p=row[3*side+x];int r=(p>>16)&255,g=(p>>8)&255,b=p&255;
                if((p>>24) && max(r,max(g,b))-min(r,min(g,b))<24)
                    p=(p&0xff000000)|0x00ffffff;
            }
        }
    }
}
}

HIMAGELIST CreatePanelToolIcons(int percent)
{
    static IconRuntime runtime;
    if(percent==100)
    {
        HIMAGELIST list=ImageList_Create(16,16,ILC_COLOR24|ILC_MASK,16,0);
        HBITMAP bitmap=(HBITMAP)LoadImageW(g_hinstExe,MAKEINTRESOURCEW(IDB_TOOLBARICONS),IMAGE_BITMAP,256,16,LR_CREATEDIBSECTION);
        if(!list || !bitmap) { if(list) ImageList_Destroy(list);if(bitmap) DeleteObject(bitmap);return NULL; }
        if(bitmap && registrySettings.DarkTheme)
        {
            HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,bitmap);
            for(int tool:{0,1,9,10,11}) for(int y=0;y<16;++y) for(int x=tool*16;x<(tool+1)*16;++x)
                if(GetPixel(dc,x,y)==RGB(0,0,0)) SetPixelV(dc,x,y,RGB(255,255,255));
            for(int y=3;y<15;++y)for(int x=0;x<(y==3?5:y==4?4:3);++x)
                if(GetPixel(dc,3*16+x,y)==RGB(0,0,0))SetPixelV(dc,3*16+x,y,RGB(255,255,255));
            SelectObject(dc,old);DeleteDC(dc);
        }
        ImageList_AddMasked(list,bitmap,RGB(255,0,255));DeleteObject(bitmap);
        return list;
    }
    int side=MulDiv(16,percent,100),width=side*16;
    BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-side;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,&bits,NULL,0);
    if(!bitmap) return NULL;
    {
        Gdiplus::Bitmap target(width,side,width*4,PixelFormat32bppPARGB,static_cast<BYTE*>(bits));
        Gdiplus::Graphics g(&target);g.Clear(Gdiplus::Color(0,0,0,0));g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        for(int i=10;i<16;++i)
        {
            auto state=g.Save();g.TranslateTransform(float(i*side),0);
            if(registrySettings.DarkTheme && (i==10 || i==11))
                CrispDarkSymbol(g,i,side);
            else { g.ScaleTransform(percent/100.f,percent/100.f);ToolIcon(g,i); }
            g.Restore(state);
        }
    }
    CopySuppliedTools(bits,width*4,percent,side);
    HIMAGELIST list=ImageList_Create(side,side,ILC_COLOR32,16,0);
    ImageList_Add(list,bitmap,NULL);DeleteObject(bitmap);return list;
}

// A single symmetric vector arrow, rotated around its centre for each direction.
// Render at the destination resolution rather than scaling rounded GDI endpoints.
void DrawPanelCopyArrow(HDC dc,const RECT& r,int direction,COLORREF color,bool pressed)
{
    using namespace Gdiplus;
    static IconRuntime runtime;
    Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetPixelOffsetMode(PixelOffsetModeHalf);
    const float side=float(min(r.right-r.left,r.bottom-r.top)),scale=side/14.f;
    const float shift=pressed?float(PanelPx(1)):0.f;
    g.TranslateTransform((r.left+r.right)*.5f+shift,(r.top+r.bottom)*.5f+shift);
    g.RotateTransform(direction==2?-90.f:direction==1?-45.f:0.f);
    g.ScaleTransform(scale,scale);
    Pen ink(Color(255,GetRValue(color),GetGValue(color),GetBValue(color)),1.15f);
    ink.SetStartCap(LineCapRound);ink.SetEndCap(LineCapRound);ink.SetLineJoin(LineJoinRound);
    g.DrawLine(&ink,3.2f,0.f,-3.2f,0.f);
    const PointF head[]={{-.4f,-2.8f},{-3.2f,0.f},{-.4f,2.8f}};
    g.DrawLines(&ink,head,_countof(head));
}

// Render supplied raster artwork without reconstructing its contours.
static void DrawPanelRaster(HDC dc,const RECT& r,UINT id,bool themedMonochrome=false)
{
    using namespace Gdiplus;
    static IconRuntime runtime;
    HRSRC resource=FindResourceW(g_hinstExe,MAKEINTRESOURCEW(id),RT_RCDATA);
    if(!resource) return;
    DWORD bytes=SizeofResource(g_hinstExe,resource);
    const void* source=LockResource(LoadResource(g_hinstExe,resource));
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!source || !memory) { if(memory) GlobalFree(memory);return; }
    void* data=GlobalLock(memory);if(!data) { GlobalFree(memory);return; }
    CopyMemory(data,source,bytes);GlobalUnlock(memory);
    IStream* stream=NULL;if(FAILED(CreateStreamOnHGlobal(memory,TRUE,&stream))) { GlobalFree(memory);return; }
    {
        Bitmap artwork(stream);
        Graphics g(dc);
        double scale=min(double(r.right-r.left)/artwork.GetWidth(),double(r.bottom-r.top)/artwork.GetHeight());
        int width=max(1,int(lround(artwork.GetWidth()*scale))),height=max(1,int(lround(artwork.GetHeight()*scale)));
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);g.SetPixelOffsetMode(PixelOffsetModeHalf);
        ImageAttributes attributes;attributes.SetWrapMode(WrapModeTileFlipXY);
        if(themedMonochrome && registrySettings.DarkTheme)
        {
            // Recolour only this monochrome arrow for contrast, preserving its
            // original coverage and contours (including antialiased pixels).
            Color background;artwork.GetPixel(0,0,&background);
            COLORREF face=GetSysColor(COLOR_BTNFACE),ink=GetSysColor(COLOR_BTNTEXT);
            ColorMatrix matrix={};matrix.m[3][3]=matrix.m[4][4]=1;
            const BYTE original[]={background.GetR(),background.GetG(),background.GetB()};
            const BYTE fg[]={GetRValue(ink),GetGValue(ink),GetBValue(ink)};
            const BYTE bg[]={GetRValue(face),GetGValue(face),GetBValue(face)};
            for(int i=0;i<3;++i){matrix.m[i][i]=float(int(bg[i])-fg[i])/max(1,int(original[i]));matrix.m[4][i]=fg[i]/255.f;}
            attributes.SetColorMatrix(&matrix);
        }
        g.DrawImage(&artwork,Gdiplus::Rect(r.left+(r.right-r.left-width)/2,r.top+(r.bottom-r.top-height)/2,width,height),
                    0,0,artwork.GetWidth(),artwork.GetHeight(),UnitPixel,&attributes);
    }
    stream->Release();
}

void DrawPanelTransparency(HDC dc,const RECT& r,bool transparent)
{
    DrawPanelRaster(dc,r,transparent?IDR_TRANSPARENCY_CLEAR:IDR_TRANSPARENCY_OPAQUE);
}

void DrawPanelSwapArrow(HDC dc,const RECT& r)
{
    // The intermediate sizes use the same supplied contours, downsampled
    // directly from the larger original rather than enlarging the small icon.
    DrawPanelRaster(dc,r,registrySettings.PanelMagnification==100?IDR_SWAP_100:IDR_SWAP_250,true);
    if(!registrySettings.DarkTheme)FrameRect(dc,&r,(HBRUSH)GetStockObject(BLACK_BRUSH));
}
