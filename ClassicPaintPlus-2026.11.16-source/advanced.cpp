#include <algorithm>
#include "precomp.h"

std::vector<CStringW> RecentFonts()
{
    std::vector<CStringW> names;WCHAR name[LF_FACESIZE];
    for(unsigned i=0;i<min(30u,registrySettings.RecentFontsCount);++i)
    {
        CStringW key;key.Format(L"Font%u",i);
        GetPrivateProfileStringW(L"RecentFonts",key,L"",name,_countof(name),RegistrySettings::ConfigPath());
        if(name[0]) names.emplace_back(name);
    }
    return names;
}
static void StoreRecentFonts(const std::vector<CStringW>& names)
{
    for(unsigned i=0;i<30;++i)
    {
        CStringW key;key.Format(L"Font%u",i);
        WritePrivateProfileStringW(L"RecentFonts",key,i<names.size()?LPCWSTR(names[i]):NULL,RegistrySettings::ConfigPath());
    }
}
void RememberFont(LPCWSTR name)
{
    if(!name || !*name || !registrySettings.RecentFontsCount) return;
    auto names=RecentFonts();
    names.erase(std::remove_if(names.begin(),names.end(),[&](const CStringW& old){return old.CompareNoCase(name)==0;}),names.end());
    names.insert(names.begin(),CStringW(name));if(names.size()>registrySettings.RecentFontsCount) names.resize(registrySettings.RecentFontsCount);
    StoreRecentFonts(names);
}
void TrimRecentFonts() { StoreRecentFonts(RecentFonts()); }
static void ContrastFramePixel(HDC dc,int x,int y)
{
    COLORREF bg=GetPixel(dc,x,y);if(bg==CLR_INVALID) return;
    int red=GetRValue(bg),green=GetGValue(bg),blue=GetBValue(bg);
    COLORREF ink=bg^0x00ffffff;
    if(abs(255-2*red)+abs(255-2*green)+abs(255-2*blue)<180)
        ink=(red*299+green*587+blue*114>=128000)?RGB(0,0,0):RGB(255,255,255);
    SetPixelV(dc,x,y,ink);
}
void DrawDashedPolyline(HDC dc,const POINT* points,int count,bool closed)
{
    if(!points || count<2) return;RECT clip;if(GetClipBox(dc,&clip)==ERROR) return;
    double phase=0;
    for(int edge=0;edge<count-(closed?0:1);++edge)
    {
        POINT a=points[edge],b=points[(edge+1)%count];double dx=double(b.x)-a.x,dy=double(b.y)-a.y,length=hypot(dx,dy);
        int steps=max(abs(b.x-a.x),abs(b.y-a.y));if(!steps) continue;
        double first=0,last=1;
        auto limit=[&](double origin,double delta,double low,double high)
        { if(!delta) return origin>=low && origin<=high;double t0=(low-origin)/delta,t1=(high-origin)/delta;if(t0>t1) std::swap(t0,t1);first=max(first,t0);last=min(last,t1);return first<=last; };
        if(limit(a.x,dx,clip.left-1,clip.right) && limit(a.y,dy,clip.top-1,clip.bottom))
            for(int i=max(0,int(floor(first*steps)));i<min(steps,int(ceil(last*steps))+1);++i)
            {
                if(fmod(phase+i*length/steps,8.0)>=4) continue;
                int x=int(lround(a.x+dx*i/steps)),y=int(lround(a.y+dy*i/steps));
                if(x>=clip.left && x<clip.right && y>=clip.top && y<clip.bottom) ContrastFramePixel(dc,x,y);
            }
        phase=fmod(phase+length,8.0);
    }
}
void DrawDashedRect(HDC dc,const RECT& r)
{
    if(r.right<=r.left || r.bottom<=r.top) return;
    RECT clip;if(GetClipBox(dc,&clip)==ERROR) return;
    auto contrast=[&](int x,int y) { ContrastFramePixel(dc,x,y); };
    for(int x=max(r.left,clip.left);x<min(r.right,clip.right);++x)
        if((x-r.left)%8<4)
        {
            if(r.top>=clip.top && r.top<clip.bottom) contrast(x,r.top);
            if(r.bottom-1!=r.top && r.bottom-1>=clip.top && r.bottom-1<clip.bottom) contrast(x,r.bottom-1);
        }
    for(int y=max(r.top+1,clip.top);y<min(r.bottom-1,clip.bottom);++y)
        if((y-r.top)%8<4)
        {
            if(r.left>=clip.left && r.left<clip.right) contrast(r.left,y);
            if(r.right-1!=r.left && r.right-1>=clip.left && r.right-1<clip.right) contrast(r.right-1,y);
        }
}
void DrawColorSample(HDC dc,const RECT& r,COLORREF color,BYTE alpha)
{
    for(int y=r.top;y<r.bottom;y+=4) for(int x=r.left;x<r.right;x+=4)
    {
        unsigned bg=((x-r.left)/4+(y-r.top)/4)%2?200:255;
        HBRUSH brush=CreateSolidBrush(RGB((GetRValue(color)*alpha+bg*(255-alpha)+127)/255,
            (GetGValue(color)*alpha+bg*(255-alpha)+127)/255,(GetBValue(color)*alpha+bg*(255-alpha)+127)/255));
        RECT cell={x,y,min(x+4,r.right),min(y+4,r.bottom)};FillRect(dc,&cell,brush);DeleteObject(brush);
    }
}
DWORD RasterPixel(HDC dc,int x,int y)
{
    DIBSECTION dib={};GetObject(GetCurrentObject(dc,OBJ_BITMAP),sizeof(dib),&dib);GdiFlush();
    if(x<0 || y<0 || x>=dib.dsBm.bmWidth || y>=dib.dsBm.bmHeight) return 0xffffffff;
    if(dib.dsBm.bmBitsPixel==32 && dib.dsBm.bmBits)
        return reinterpret_cast<DWORD*>(static_cast<BYTE*>(dib.dsBm.bmBits)+size_t(dib.dsBm.bmHeight-1-y)*dib.dsBm.bmWidthBytes)[x]^0xff000000;
    COLORREF c=GetPixel(dc,x,y);return 0xff000000|RGB(GetBValue(c),GetGValue(c),GetRValue(c));
}
void FloodRaster(HDC dc,int x,int y,DWORD rgba)
{
    DIBSECTION d={};GetObject(GetCurrentObject(dc,OBJ_BITMAP),sizeof(d),&d);GdiFlush();
    if(!d.dsBm.bmBits || d.dsBm.bmBitsPixel!=32 || x<0 || y<0 || x>=d.dsBm.bmWidth || y>=d.dsBm.bmHeight) return;
    auto row=[&](int yy){return reinterpret_cast<DWORD*>(static_cast<BYTE*>(d.dsBm.bmBits)+size_t(d.dsBm.bmHeight-1-yy)*d.dsBm.bmWidthBytes);};
    DWORD original=row(y)[x],replacement=rgba^0xff000000;if(original==replacement) return;
    // Scanline fill compares RGBA, so transparent and opaque equal-RGB areas are distinct.
    std::vector<POINT> pending={{x,y}};
    while(!pending.empty())
    {
        POINT p=pending.back();pending.pop_back();if(row(p.y)[p.x]!=original) continue;
        int left=p.x,right=p.x;while(left>0 && row(p.y)[left-1]==original) --left;
        while(right+1<d.dsBm.bmWidth && row(p.y)[right+1]==original) ++right;
        for(int xx=left;xx<=right;++xx) row(p.y)[xx]=replacement;
        for(int yy:{p.y-1,p.y+1}) if(yy>=0 && yy<d.dsBm.bmHeight)
        {
            bool run=false;for(int xx=left;xx<=right;++xx)
            { bool match=row(yy)[xx]==original;if(match && !run) pending.push_back({xx,yy});run=match; }
        }
    }
}
