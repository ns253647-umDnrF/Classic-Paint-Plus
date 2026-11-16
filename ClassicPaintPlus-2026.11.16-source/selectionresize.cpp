#include <vector>
#include <new>
#include "precomp.h"
#include "selectionresize.h"

namespace
{
struct ImagingRuntime
{
    ULONG_PTR token = 0;
    ImagingRuntime() { Gdiplus::GdiplusStartupInput input; Gdiplus::GdiplusStartup(&token, &input, NULL); }
    ~ImagingRuntime() { if (token) Gdiplus::GdiplusShutdown(token); }
};

bool ReadPixels(HBITMAP bitmap, int w, int h, std::vector<DWORD>& pixels)
{
    BITMAP bm = {};
    if (!GetObjectW(bitmap,sizeof(bm),&bm) || bm.bmWidth!=w || bm.bmHeight!=h) return false;
    pixels.resize(static_cast<size_t>(w)*h);
    BITMAPINFO info = {};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=w; info.bmiHeader.biHeight=-h;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(NULL);
    int rows=GetDIBits(dc,bitmap,0,h,pixels.data(),&info,DIB_RGB_COLORS);
    DeleteDC(dc);
    return rows==h;
}

HBITMAP RenderScaled(HBITMAP source,HBITMAP mask,COLORREF key,int w,int h,int mode,double rotation,double skewX,double skewY)
{
    static ImagingRuntime runtime;
    if (!runtime.token || w<=0 || h<=0 || static_cast<LONGLONG>(w)*h>INT_MAX/4) return NULL;
    BITMAP bm={};
    if(!GetObjectW(source,sizeof(bm),&bm)) return NULL;
    try
    {
        std::vector<DWORD> pixels,coverage;
        RasterImage originalPixels;if(!ReadRaster(source,originalPixels)) return NULL;
        pixels=std::move(originalPixels.pixels);
        if(mask && !ReadPixels(mask,bm.bmWidth,bm.bmHeight,coverage)) return NULL;
        DWORD keyRGB=RGB(GetBValue(key),GetGValue(key),GetRValue(key));
        for(size_t i=0;i<pixels.size();++i)
        {
            DWORD rgb=pixels[i]&0xffffff;
            bool visible=(!mask || (coverage[i]&0xffffff)) && (key==CLR_INVALID || rgb!=keyRGB);
            // Premultiplication before interpolation prevents color-key fringes.
            unsigned a=visible?pixels[i]>>24:0;
            pixels[i]=(a<<24)|(((rgb>>16&255)*a+127)/255<<16)|(((rgb>>8&255)*a+127)/255<<8)|((rgb&255)*a+127)/255;
        }
        CRect bounds=TransformedBounds(CRect(0,0,w,h),rotation,skewX,skewY);
        int outputWidth=bounds.Width(),outputHeight=bounds.Height();
        if(outputWidth<=0 || outputHeight<=0 || LONGLONG(outputWidth)*outputHeight>100000000) return NULL;
        BITMAPINFO info={};
        info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=outputWidth; info.bmiHeader.biHeight=-outputHeight;
        info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
        void* output=NULL;
        HBITMAP result=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,&output,NULL,0);
        if(!result) return NULL;
        if(mode==0 && rotation==0 && !skewX && !skewY)
        {
            DWORD* target=static_cast<DWORD*>(output);
            for(int y=0;y<h;++y) for(int x=0;x<w;++x)
                target[size_t(y)*w+x]=pixels[size_t(LONGLONG(y)*bm.bmHeight/h)*bm.bmWidth+LONGLONG(x)*bm.bmWidth/w];
            return result;
        }
        Gdiplus::Status status;
        {
            Gdiplus::Bitmap original(bm.bmWidth,bm.bmHeight,bm.bmWidth*4,PixelFormat32bppPARGB,
                                    reinterpret_cast<BYTE*>(pixels.data()));
            Gdiplus::Bitmap target(outputWidth,outputHeight,outputWidth*4,PixelFormat32bppPARGB,static_cast<BYTE*>(output));
            Gdiplus::Graphics graphics(&target);
            graphics.Clear(Gdiplus::Color(0,0,0,0));
            graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
            graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            graphics.SetInterpolationMode(mode==0 ? Gdiplus::InterpolationModeNearestNeighbor : mode==1 ? Gdiplus::InterpolationModeHighQualityBilinear :
                                                    Gdiplus::InterpolationModeHighQualityBicubic);
            Gdiplus::ImageAttributes attributes;
            attributes.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
            if(rotation==0 && !skewX && !skewY)
                status=graphics.DrawImage(&original,Gdiplus::Rect(0,0,w,h),0,0,bm.bmWidth,bm.bmHeight,Gdiplus::UnitPixel,&attributes);
            else
            {
                auto point=[&](double x,double y) { auto p=TransformPoint(x,y,CRect(0,0,w,h),rotation,skewX,skewY);return Gdiplus::PointF(float(p.x-bounds.left),float(p.y-bounds.top)); };
                Gdiplus::PointF corners[]={point(0,0),point(w,0),point(0,h)};
                status=graphics.DrawImage(&original,corners,3,0,0,bm.bmWidth,bm.bmHeight,Gdiplus::UnitPixel,&attributes);
            }
        }
        if(status!=Gdiplus::Ok) { DeleteObject(result); return NULL; }
        return result;
    }
    catch(const std::bad_alloc&) { return NULL; }
}

// Original generated PNG assets are embedded in the EXE. Render directly at
// the current panel resolution and preserve their alpha against the system face.
HBITMAP CreateResampleIcon(int width,int height,int index)
{
    static ImagingRuntime runtime;
    HRSRC resource=FindResourceW(g_hinstExe,MAKEINTRESOURCEW(5300+index),RT_RCDATA);
    if(!resource) return NULL;
    DWORD bytes=SizeofResource(g_hinstExe,resource);
    HGLOBAL loaded=LoadResource(g_hinstExe,resource);const void* sourceBytes=loaded?LockResource(loaded):NULL;
    if(!sourceBytes || !bytes) return NULL;
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!memory) return NULL;
    void* dest=GlobalLock(memory);if(!dest) { GlobalFree(memory);return NULL; }
    CopyMemory(dest,sourceBytes,bytes);GlobalUnlock(memory);
    IStream* stream=NULL;
    if(FAILED(CreateStreamOnHGlobal(memory,TRUE,&stream))) { GlobalFree(memory);return NULL; }
    HBITMAP result=CreateColorDIB(width,height,GetSysColor(COLOR_BTNFACE));
    {
        Gdiplus::Bitmap source(stream);
        int w=source.GetWidth(),h=source.GetHeight();
        // Bound the nontransparent symbol once per scale change, so generator
        // margins do not make some symbols appear smaller than their neighbors.
        int left=w,top=h,right=0,bottom=0;
        Gdiplus::BitmapData pixels={};Gdiplus::Rect rect(0,0,w,h);
        if(source.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&pixels)==Gdiplus::Ok)
        {
            for(int y=0;pixels.Scan0 && y<h;++y)
            {
                const DWORD* row=reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(pixels.Scan0)+y*pixels.Stride);
                for(int x=0;x<w;++x) if((row[x]>>24)>32)
                { left=min(left,x);right=max(right,x+1);top=min(top,y);bottom=max(bottom,y+1); }
            }
            source.UnlockBits(&pixels);
        }
        if(right>left && bottom>top && result)
        {
            HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,result);
            {
                Gdiplus::Graphics g(dc);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
                const float outline=registrySettings.DarkTheme ? PanelPercent()/100.f : 0.f;
                float scale=min(max(1.f,width-2*outline)/(right-left),max(1.f,height-2*outline)/(bottom-top));
                float sw=(right-left)*scale,sh=(bottom-top)*scale;
                Gdiplus::RectF target((width-sw)/2,(height-sh)/2,sw,sh);
                if(registrySettings.DarkTheme)
                {
                    // A light outline follows the artwork's alpha silhouette, not
                    // the rectangular button. Work at 2x for smooth 150/250% edges.
                    const int ow=width*2,oh=height*2,radius=max(1,int(lround(outline*2)));
                    Gdiplus::Bitmap mask(ow,oh,PixelFormat32bppARGB);
                    {
                        Gdiplus::Graphics mg(&mask);mg.Clear(Gdiplus::Color(0,0,0,0));
                        mg.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);mg.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
                        mg.DrawImage(&source,Gdiplus::RectF(target.X*2,target.Y*2,sw*2,sh*2),float(left),float(top),float(right-left),float(bottom-top),Gdiplus::UnitPixel);
                    }
                    Gdiplus::BitmapData data={};Gdiplus::Rect outlineRect(0,0,ow,oh);
                    if(mask.LockBits(&outlineRect,Gdiplus::ImageLockModeRead|Gdiplus::ImageLockModeWrite,PixelFormat32bppARGB,&data)==Gdiplus::Ok)
                    {
                        std::vector<BYTE> alpha(size_t(ow)*oh);
                        for(int y=0;y<oh;++y)
                        {
                            const DWORD* row=reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(data.Scan0)+y*data.Stride);
                            for(int x=0;x<ow;++x)alpha[size_t(y)*ow+x]=BYTE(row[x]>>24);
                        }
                        for(int y=0;y<oh;++y)
                        {
                            DWORD* row=reinterpret_cast<DWORD*>(static_cast<BYTE*>(data.Scan0)+y*data.Stride);
                            for(int x=0;x<ow;++x)
                            {
                                int cover=0;
                                for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx)
                                    if(dx*dx+dy*dy<=radius*radius && x+dx>=0 && x+dx<ow && y+dy>=0 && y+dy<oh)
                                        cover=max(cover,int(alpha[size_t(y+dy)*ow+x+dx]));
                                // Leave the interior unchanged; only add the outer contour.
                                row[x]=(DWORD(max(0,cover-alpha[size_t(y)*ow+x]))<<24)|0x00f4f8ff;
                            }
                        }
                        mask.UnlockBits(&data);
                        g.DrawImage(&mask,Gdiplus::Rect(0,0,width,height),0,0,ow,oh,Gdiplus::UnitPixel);
                    }
                }
                g.DrawImage(&source,target,float(left),float(top),float(right-left),float(bottom-top),Gdiplus::UnitPixel);
            }
            SelectObject(dc,old);DeleteDC(dc);
        }
    }
    stream->Release();return result;
}

class ResizePanel : public CWindowImpl<ResizePanel>
{
    HWND tooltip = NULL;
    HBITMAP icons[3] = {};
    CStringW labels[3];
public:
    DECLARE_WND_CLASS_EX(L"SelectionResizePanel",0,COLOR_BTNFACE)
    void ReloadLanguage()
    {
        ::SendMessage(tooltip,TTM_POP,0,0);
        for(int i=0;i<3;++i)
        {
            labels[i].LoadString(g_hinstExe,IDS_RESAMPLE_NEAREST+i);HWND button=GetDlgItem(IDC_RESAMPLE_FIRST+i);
            ::SetWindowTextW(button,labels[i]);TOOLINFOW info={sizeof(info)};info.hwnd=m_hWnd;info.uFlags=TTF_IDISHWND;
            info.uId=UINT_PTR(button);info.lpszText=labels[i].GetBuffer();::SendMessage(tooltip,TTM_UPDATETIPTEXTW,0,LPARAM(&info));labels[i].ReleaseBuffer();
        }
    }
    BEGIN_MSG_MAP(ResizePanel)
        MESSAGE_HANDLER(WM_CREATE,OnCreate)
        MESSAGE_HANDLER(WM_COMMAND,OnCommand)
        MESSAGE_HANDLER(WM_DRAWITEM,OnDrawItem)
        MESSAGE_HANDLER(WM_DESTROY,OnDestroy)
    END_MSG_MAP()
    void Layout(const RECT& r)
    {
        MoveWindow(&r);
        CRect bounds;GetClientRect(&bounds);
        int width=max(1,bounds.Width()-PanelPx(10)),height=max(1,bounds.Height()/3-PanelPx(8));
        for(int i=0;i<3;++i)
        {
            if(icons[i]) DeleteObject(icons[i]);
            icons[i]=CreateResampleIcon(width,height,i);
            int top=i*bounds.Height()/3,bottom=(i+1)*bounds.Height()/3;
            ::MoveWindow(GetDlgItem(IDC_RESAMPLE_FIRST+i),0,top,bounds.Width(),bottom-top,TRUE);
        }
        RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
    }
    LRESULT OnCreate(UINT,WPARAM,LPARAM,BOOL&)
    {
        CRect bounds; GetClientRect(&bounds);
        tooltip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,NULL,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,
                               CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,m_hWnd,NULL,g_hinstExe,NULL);
        SetPropW(m_hWnd,L"SelectionResizeTooltip",tooltip);
        ::SendMessageW(tooltip,TTM_SETMAXTIPWIDTH,0,300);
        ::SendMessageW(tooltip,TTM_SETDELAYTIME,TTDT_INITIAL,350);
        for(int i=0;i<3;++i)
        {
            labels[i].LoadString(g_hinstExe,IDS_RESAMPLE_NEAREST+i);
            int top=i*bounds.Height()/3, bottom=(i+1)*bounds.Height()/3;
            HWND child=CreateWindowW(L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
                0,top,bounds.Width(),bottom-top,m_hWnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RESAMPLE_FIRST+i)),g_hinstExe,NULL);
            TOOLINFOW info={sizeof(info)};
            info.uFlags=TTF_IDISHWND|TTF_SUBCLASS;
            info.hwnd=m_hWnd; info.uId=reinterpret_cast<UINT_PTR>(child);
            info.lpszText=const_cast<LPWSTR>(static_cast<LPCWSTR>(labels[i]));
            ::SendMessageW(tooltip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&info));
        }
        CRect r;GetWindowRect(&r);::MapWindowPoints(NULL,GetParent(),reinterpret_cast<POINT*>(&r),2);Layout(r);
        return 0;
    }
    LRESULT OnDrawItem(UINT,WPARAM,LPARAM lp,BOOL&)
    {
        const DRAWITEMSTRUCT& draw=*reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        int index=draw.CtlID-IDC_RESAMPLE_FIRST;
        if(index<0 || index>2) return FALSE;
        RECT r=draw.rcItem;
        bool selected=registrySettings.SelectionInterpolation==index;
        DrawFrameControl(draw.hDC,&r,DFC_BUTTON,DFCS_BUTTONPUSH|
            ((selected || (draw.itemState&ODS_SELECTED))?DFCS_PUSHED:0));
        if(registrySettings.DarkTheme)
            FrameRect(draw.hDC,&r,GetSysColorBrush(COLOR_BTNTEXT));
        if(selected)
        {
            InflateRect(&r,-2,-2);
            FrameRect(draw.hDC,&r,GetSysColorBrush(registrySettings.DarkTheme?COLOR_BTNTEXT:COLOR_HIGHLIGHT));
        }
        if(icons[index])
        {
            BITMAP bm={}; GetObjectW(icons[index],sizeof(bm),&bm);
            int x=(draw.rcItem.right-bm.bmWidth)/2,y=(draw.rcItem.bottom-bm.bmHeight)/2;
            HDC dc=CreateCompatibleDC(draw.hDC);
            HGDIOBJ old=SelectObject(dc,icons[index]);
            BitBlt(draw.hDC,x,y,bm.bmWidth,bm.bmHeight,dc,0,0,SRCCOPY);
            SelectObject(dc,old); DeleteDC(dc);
        }
        if(draw.itemState&ODS_FOCUS)
        {
            r=draw.rcItem; InflateRect(&r,-3,-3); DrawFocusRect(draw.hDC,&r);
        }
        return TRUE;
    }
    LRESULT OnDestroy(UINT,WPARAM,LPARAM,BOOL&)
    {
        RemovePropW(m_hWnd,L"SelectionResizeTooltip");
        if(tooltip) ::DestroyWindow(tooltip);
        for(HBITMAP icon:icons) if(icon) DeleteObject(icon);
        return 0;
    }
    LRESULT OnCommand(UINT,WPARAM wp,LPARAM,BOOL&)
    {
        int index=LOWORD(wp)-IDC_RESAMPLE_FIRST;
        if(HIWORD(wp)==BN_CLICKED && index>=0 && index<3)
        {
            registrySettings.SelectionInterpolation=index;
            registrySettings.StorePreferences();
            RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
            canvasWindow.Invalidate(FALSE);
            if(miniature.IsWindow()) miniature.Invalidate(FALSE);
        }
        return 0;
    }
};
ResizePanel panel;
}

bool SelectionResampler::Draw(HDC dest,const RECT& rc,HBITMAP bitmap,HBITMAP maskBitmap,COLORREF colorKey,int algorithm,BYTE opacity,double rotation,double skewX,double skewY)
{
    int w=rc.right-rc.left,h=rc.bottom-rc.top;
    if(!scaled || source!=bitmap || mask!=maskBitmap || width!=w || height!=h || key!=colorKey || mode!=algorithm || angle!=rotation || shearX!=skewX || shearY!=skewY)
    {
        Reset();
        scaled=RenderScaled(bitmap,maskBitmap,colorKey,w,h,algorithm,rotation,skewX,skewY);
        if(!scaled) return false;
        source=bitmap; mask=maskBitmap; width=w; height=h; key=colorKey; mode=algorithm;angle=rotation;shearX=skewX;shearY=skewY;
    }
    CRect bounds=TransformedBounds(rc,rotation,skewX,skewY);w=bounds.Width();h=bounds.Height();
    if(BlendRasterSelection(dest,bounds,scaled,opacity)) return true;
    HDC dc=CreateCompatibleDC(dest);
    HGDIOBJ old=SelectObject(dc,scaled);
    BLENDFUNCTION blend={AC_SRC_OVER,0,opacity,AC_SRC_ALPHA};
    BOOL ok=AlphaBlend(dest,bounds.left,bounds.top,w,h,dc,0,0,w,h,blend);
    SelectObject(dc,old); DeleteDC(dc);
    return !!ok;
}

void CreateSelectionResizePanel(HWND owner, const RECT& bounds)
{
    CRect position(bounds);
    panel.Create(owner,position,NULL,WS_CHILD|WS_CLIPCHILDREN,WS_EX_CONTROLPARENT,IDC_RESAMPLE_PANEL);
    UpdateSelectionResizePanel();
}

void UpdateSelectionResizePanel()
{
    if(panel.IsWindow()) panel.ShowWindow(toolsModel.IsSelection()?SW_SHOWNA:SW_HIDE);
}

void LayoutSelectionResizePanel(const RECT& bounds) { if(panel.IsWindow()) panel.Layout(bounds); }
void ReloadSelectionResizeLanguage() { if(panel.IsWindow()) panel.ReloadLanguage(); }
