// Device-space transform arrows, separate resize grips and matching native cursors.
#include "precomp.h"
#include <array>

POINT TransformArrowHandle(const RECT& r,int hit,double a,double x,double y)
{
    POINT grip=TransformHandle(r,hit,a,x,y);
    // Normals to the transformed sides always point out of the parallelogram.
    double t=a*M_PI/180,c=cos(t),s=sin(t);
    double tx=c-s*y,ty=s+c*y;
    double vx=c*x-s*(1+y*x),vy=s*x+c*(1+y*x);
    double topX=ty,topY=-tx,rightX=vy,rightY=-vx;
    double len=hypot(topX,topY);topX/=len;topY/=len;
    len=hypot(rightX,rightY);rightX/=len;rightY/=len;
    int horizontal=(hit==HIT_UPPER_LEFT || hit==HIT_MIDDLE_LEFT || hit==HIT_LOWER_LEFT)?-1:
        (hit==HIT_UPPER_RIGHT || hit==HIT_MIDDLE_RIGHT || hit==HIT_LOWER_RIGHT)?1:0;
    int vertical=hit<=HIT_UPPER_RIGHT?1:hit>=HIT_LOWER_LEFT?-1:0;
    double nx=horizontal*rightX+vertical*topX,ny=horizontal*rightY+vertical*topY;
    double distance=13+(max(3,CanvasGripSize())+1)/2.0;
    len=max(fabs(nx),fabs(ny));if(len<1e-8) { nx=horizontal;ny=-vertical;len=max(fabs(nx),fabs(ny)); }
    grip.x+=LONG(lround(distance*nx/len));grip.y+=LONG(lround(distance*ny/len));return grip;
}
HITTEST HitTransformArrow(const RECT& bounds,double a,double x,double y,POINT point)
{
    CRect r=bounds;canvasWindow.ImageToCanvas(r);HITTEST result=HIT_NONE;int best=INT_MAX;
    for(int hit=HIT_UPPER_LEFT;hit<=HIT_LOWER_RIGHT;++hit)
    {
        POINT p=TransformArrowHandle(r,hit,a,x,y);int dx=abs(p.x-point.x),dy=abs(p.y-point.y);
        if(dx<=10 && dy<=10 && dx*dx+dy*dy<best) { result=HITTEST(hit);best=dx*dx+dy*dy; }
    }
    return result;
}
HITTEST HitTransformFrame(const RECT& bounds,double a,double x,double y,POINT point)
{
    CRect r=bounds;canvasWindow.ImageToCanvas(r);int radius=max(4,CanvasGripSize()/2+2);
    for(int hit=HIT_UPPER_LEFT;hit<=HIT_LOWER_RIGHT;++hit)
    { POINT p=TransformHandle(r,hit,a,x,y);if(abs(p.x-point.x)<=radius && abs(p.y-point.y)<=radius)return HITTEST(hit); }
    HITTEST arrow=HitTransformArrow(bounds,a,x,y,point);if(arrow!=HIT_NONE)return arrow;
    auto p=TransformPoint(point.x,point.y,r,a,x,y,true);
    if(p.x<r.left-3 || p.x>r.right+3 || p.y<r.top-3 || p.y>r.bottom+3)return HIT_NONE;
    return p.x<r.left+3 || p.x>r.right-3 || p.y<r.top+3 || p.y>r.bottom-3?HIT_BORDER:HIT_INNER;
}
namespace {
double ArrowAngle(int hit,double a,double x,double y,bool rotate)
{
    if(rotate && IsCornerHandle(HITTEST(hit)))return a;
    const double resize[]={45,90,135,0,0,135,90,45};
    double angle=rotate?((hit==HIT_UPPER_CENTER || hit==HIT_LOWER_CENTER)?0:90):resize[hit-HIT_UPPER_LEFT];
    double dx=cos(angle*M_PI/180),dy=sin(angle*M_PI/180);
    if(!rotate) { dx+=x*dy;dy+=y*dx; }
    else if(hit==HIT_UPPER_CENTER || hit==HIT_LOWER_CENTER)dy=y;
    return a+atan2(dy,dx)*180/M_PI;
}
void DrawArrow(HDC dc,POINT origin,int hit,double angle,bool rotate)
{
    std::vector<POINT> arrow;
    auto add=[&](double x,double y) { arrow.push_back({origin.x+LONG(lround(x)),origin.y+LONG(lround(y))}); };
    if(rotate && IsCornerHandle(HITTEST(hit)))
    {
        const double angles[]={-135,0,-45,0,0,135,0,45};double base=(angles[hit-HIT_UPPER_LEFT]+angle)*M_PI/180;
        double t0=base-M_PI/4,t1=base+M_PI/4;
        auto end=[&](double t,double tangent,double radial){add(radial*cos(t)-tangent*sin(t),radial*sin(t)+tangent*cos(t));};
        end(t0,-5,5);end(t0,1,10);end(t0,1,7);
        for(int i=1;i<8;++i)end(t0+i*(t1-t0)/8,0,7);
        end(t1,-1,7);end(t1,-1,10);end(t1,5,5);end(t1,-1,0);end(t1,-1,3);
        for(int i=7;i>0;--i)end(t0+i*(t1-t0)/8,0,3);
        end(t0,1,3);end(t0,1,0);
    }
    else
    {
        double t=angle*M_PI/180,c=cos(t),s=sin(t);
        const POINT polygon[]={{-8,0},{-2,-5},{-2,-2},{2,-2},{2,-5},{8,0},{2,5},{2,2},{-2,2},{-2,5}};
        for(auto v:polygon)add(c*v.x-s*v.y,s*v.x+c*v.y);
    }
    Polygon(dc,arrow.data(),int(arrow.size()));
}
struct Cursors
{
    struct Entry { int kind=-1,angle=0;HCURSOR handle=NULL; };
    std::array<Entry,64> entries={};size_t next=0;
    ~Cursors() { for(auto& entry:entries)if(entry.handle)DestroyCursor(entry.handle); }
} cursors;
}
HCURSOR TransformFrameCursor(HITTEST hit,double a,double x,double y,bool rotate)
{
    if(hit<HIT_UPPER_LEFT || hit>HIT_LOWER_RIGHT)return LoadCursorW(NULL,IDC_SIZEALL);
    double degrees=ArrowAngle(hit,a,x,y,rotate);int angle=int(lround(degrees/2))*2;angle=(angle%360+360)%360;
    int kind=rotate && IsCornerHandle(hit)?int(hit):0;
    for(auto& entry:cursors.entries)if(entry.handle && entry.kind==kind && entry.angle==angle)return entry.handle;
    // A monochrome AND/XOR cursor has a transparent exterior and opaque black/
    // white arrow pixels; its hotspot is the same center used for hit testing.
    const int size=32,stride=4;BYTE andBits[size*stride],xorBits[size*stride];
    memset(andBits,255,sizeof(andBits));memset(xorBits,0,sizeof(xorBits));
    HBITMAP bitmap=CreateColorDIB(size,size,RGB(255,0,255));HDC dc=CreateCompatibleDC(NULL);
    if(!bitmap || !dc) { if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return LoadCursorW(NULL,IDC_CROSS); }
    HGDIOBJ old=SelectObject(dc,bitmap),oldBrush=SelectObject(dc,GetStockObject(BLACK_BRUSH));
    HPEN pen=CreatePen(PS_SOLID,1,RGB(255,255,255));HGDIOBJ oldPen=SelectObject(dc,pen);
    DrawArrow(dc,{size/2,size/2},hit,angle,rotate);GdiFlush();
    for(int row=0;row<size;++row)for(int col=0;col<size;++col)
    {
        COLORREF pixel=GetPixel(dc,col,row);if(pixel==RGB(255,0,255))continue;
        int offset=row*stride+col/8;BYTE bit=BYTE(0x80>>(col&7));andBits[offset]&=BYTE(~bit);
        if(pixel==RGB(255,255,255))xorBits[offset]|=bit;
    }
    SelectObject(dc,oldPen);DeleteObject(pen);SelectObject(dc,oldBrush);SelectObject(dc,old);DeleteDC(dc);DeleteObject(bitmap);
    HCURSOR cursor=CreateCursor(g_hinstExe,size/2,size/2,size,size,andBits,xorBits);if(!cursor)return LoadCursorW(NULL,IDC_CROSS);
    auto* slot=&cursors.entries[cursors.next++%cursors.entries.size()];
    if(slot->handle==GetCursor())slot=&cursors.entries[cursors.next++%cursors.entries.size()];
    if(slot->handle)DestroyCursor(slot->handle);slot->handle=cursor;slot->kind=kind;slot->angle=angle;return cursor;
}
void DrawTransformFrame(HDC dc,const RECT& bounds,double a,double x,double y,bool rotating,bool muted)
{
    CRect r=bounds;canvasWindow.ImageToCanvas(r);
    const int corners[]={HIT_UPPER_LEFT,HIT_UPPER_RIGHT,HIT_LOWER_RIGHT,HIT_LOWER_LEFT};
    POINT p[5];for(int i=0;i<4;++i)p[i]=TransformHandle(r,corners[i],a,x,y);p[4]=p[0];
    if(muted)
    { int saved=SaveDC(dc);SetBkMode(dc,TRANSPARENT);HPEN pen=CreatePen(PS_DOT,1,RGB(140,140,140));SelectObject(dc,pen);Polyline(dc,p,5);RestoreDC(dc,saved);DeleteObject(pen); }
    else DrawDashedPolyline(dc,p,4,true);
    int saved=SaveDC(dc);SetBkMode(dc,TRANSPARENT);SetPolyFillMode(dc,WINDING);
    HPEN pen=CreatePen(PS_SOLID,1,RGB(255,255,255));SelectObject(dc,pen);SelectObject(dc,GetStockObject(BLACK_BRUSH));
    int size=max(3,CanvasGripSize());HBRUSH grip=CreateSolidBrush(registrySettings.CanvasMarkerColor);
    for(int hit=HIT_UPPER_LEFT;hit<=HIT_LOWER_RIGHT;++hit)
    {
        POINT point=TransformHandle(r,hit,a,x,y);CRect dot(point.x-size/2,point.y-size/2,point.x-size/2+size,point.y-size/2+size);FillRect(dc,&dot,grip);
        point=TransformArrowHandle(r,hit,a,x,y);DrawArrow(dc,point,hit,ArrowAngle(hit,a,x,y,rotating),rotating);
    }
    if(rotating)
    {
        POINT c={(r.left+r.right)/2,(r.top+r.bottom)/2};
        const POINT cross[]={{c.x-1,c.y-5},{c.x+1,c.y-5},{c.x+1,c.y-1},{c.x+5,c.y-1},{c.x+5,c.y+1},{c.x+1,c.y+1},
            {c.x+1,c.y+5},{c.x-1,c.y+5},{c.x-1,c.y+1},{c.x-5,c.y+1},{c.x-5,c.y-1},{c.x-1,c.y-1}};Polygon(dc,cross,_countof(cross));
    }
    RestoreDC(dc,saved);DeleteObject(pen);DeleteObject(grip);
}
