#include "precomp.h"

AffinePointD TransformPoint(double x,double y,const RECT& r,double degrees,double kx,double ky,bool inverse)
{
    double cx=(double(r.left)+r.right)/2,cy=(double(r.top)+r.bottom)/2;
    x-=cx;y-=cy;double a=degrees*M_PI/180,c=cos(a),s=sin(a);
    if(inverse) { double xx=c*x+s*y;y=-s*x+c*y;x=xx;y-=ky*x;x-=kx*y; }
    else { x+=kx*y;y+=ky*x;double xx=c*x-s*y;y=s*x+c*y;x=xx; }
    return {cx+x,cy+y};
}
CRect TransformedBounds(const RECT& r,double a,double x,double y)
{
    if(!a && !x && !y) return CRect(r);
    AffinePointD p[]={TransformPoint(r.left,r.top,r,a,x,y),TransformPoint(r.right,r.top,r,a,x,y),
        TransformPoint(r.right,r.bottom,r,a,x,y),TransformPoint(r.left,r.bottom,r,a,x,y)};
    double l=p[0].x,t=p[0].y,rr=l,b=t;
    for(auto q:p) { l=min(l,q.x);t=min(t,q.y);rr=max(rr,q.x);b=max(b,q.y); }
    return CRect(LONG(floor(l+1e-8)),LONG(floor(t+1e-8)),LONG(ceil(rr-1e-8)),LONG(ceil(b-1e-8)));
}

POINT EditFramePoint(POINT point,const RECT& r,double degrees)
{ return RotateSelectionPoint(point,r,degrees); }

static POINT FrameHandle(const RECT& r,int hit)
{
    const int x[]={0,1,2,0,2,0,1,2},y[]={0,0,0,1,1,2,2,2};int i=hit-HIT_UPPER_LEFT;
    return {r.left+(r.right-r.left)*x[i]/2,r.top+(r.bottom-r.top)*y[i]/2};
}
HITTEST HitEditFrame(const RECT& bounds,double degrees,POINT point)
{
    CRect r=bounds;canvasWindow.ImageToCanvas(r);point=EditFramePoint(point,r,-degrees);
    int radius=max(4,CanvasGripSize()/2+2);
    for(int hit=HIT_UPPER_LEFT;hit<=HIT_LOWER_RIGHT;++hit)
    { POINT p=FrameHandle(r,hit);if(abs(point.x-p.x)<=radius && abs(point.y-p.y)<=radius) return HITTEST(hit); }
    CRect outer=r,inner=r;outer.InflateRect(3,3);inner.DeflateRect(3,3);
    if(!outer.PtInRect(point)) return HIT_NONE;
    return inner.PtInRect(point)?HIT_INNER:HIT_BORDER;
}
void DrawEditFrame(HDC dc,const RECT& bounds,double degrees)
{
    CRect r=bounds;canvasWindow.ImageToCanvas(r);
    POINT corners[]={{r.left,r.top},{r.right,r.top},{r.right,r.bottom},{r.left,r.bottom}};
    for(auto& p:corners) p=EditFramePoint(p,r,degrees);
    DrawDashedPolyline(dc,corners,4,true);
    int size=max(3,CanvasGripSize());HBRUSH brush=CreateSolidBrush(registrySettings.CanvasMarkerColor);
    for(int hit=HIT_UPPER_LEFT;hit<=HIT_LOWER_RIGHT;++hit)
    { POINT p=EditFramePoint(FrameHandle(r,hit),r,degrees);CRect grip(p.x-size/2,p.y-size/2,p.x-size/2+size,p.y-size/2+size);FillRect(dc,&grip,brush); }
    DeleteObject(brush);
}
void SetEditFrameCursor(HITTEST hit,double degrees)
{
    if(hit==HIT_BORDER || hit==HIT_INNER) { SetCursor(LoadCursorW(NULL,IDC_SIZEALL));return; }
    const double angles[]={45,90,135,0,0,135,90,45};
    if(hit<HIT_UPPER_LEFT || hit>HIT_LOWER_RIGHT) return;
    int direction=int(lround((angles[hit-HIT_UPPER_LEFT]+degrees)/45.0));direction=(direction%4+4)%4;
    LPCWSTR cursors[]={IDC_SIZEWE,IDC_SIZENWSE,IDC_SIZENS,IDC_SIZENESW};SetCursor(LoadCursorW(NULL,cursors[direction]));
}
CRect DragEditFrame(const RECT& start,HITTEST hit,POINT origin,POINT point,double degrees,int minWidth,int minHeight,bool fixedWidth,bool fixedHeight)
{
    CRect r=start;double dx=point.x-origin.x,dy=point.y-origin.y;
    if(hit==HIT_BORDER || hit==HIT_INNER) { r.OffsetRect(LONG(dx),LONG(dy));return r; }
    double a=degrees*M_PI/180,c=cos(a),s=sin(a),x=dx*c+dy*s,y=-dx*s+dy*c;
    bool left=hit==HIT_UPPER_LEFT || hit==HIT_MIDDLE_LEFT || hit==HIT_LOWER_LEFT;
    bool right=hit==HIT_UPPER_RIGHT || hit==HIT_MIDDLE_RIGHT || hit==HIT_LOWER_RIGHT;
    bool top=hit>=HIT_UPPER_LEFT && hit<=HIT_UPPER_RIGHT;
    bool bottom=hit>=HIT_LOWER_LEFT && hit<=HIT_LOWER_RIGHT;
    if(fixedWidth)x=0;if(fixedHeight)y=0;
    if(left) r.left=min(r.right-minWidth,r.left+LONG(lround(x)));
    if(right) r.right=max(r.left+minWidth,r.right+LONG(lround(x)));
    if(top) r.top=min(r.bottom-minHeight,r.top+LONG(lround(y)));
    if(bottom) r.bottom=max(r.top+minHeight,r.bottom+LONG(lround(y)));
    double cx=(r.left+r.right-start.left-start.right)*.5,cy=(r.top+r.bottom-start.top-start.bottom)*.5;
    r.OffsetRect(LONG(lround(cx*c-cy*s-cx)),LONG(lround(cx*s+cy*c-cy)));
    return r;
}

CRect SizeEditFrame(const RECT& r,double degrees,int width,int height)
{
    double dx=(width-(r.right-r.left))/2.0,dy=(height-(r.bottom-r.top))/2.0,angle=degrees*M_PI/180;
    int left=r.left+int(lround(cos(angle)*dx-sin(angle)*dy-dx));
    int top=r.top+int(lround(sin(angle)*dx+cos(angle)*dy-dy));
    return CRect(left,top,left+width,top+height);
}

bool IsCornerHandle(HITTEST hit)
{ return hit==HIT_UPPER_LEFT || hit==HIT_UPPER_RIGHT || hit==HIT_LOWER_LEFT || hit==HIT_LOWER_RIGHT; }
POINT TransformHandle(const RECT& r,int hit,double a,double x,double y)
{
    POINT p=FrameHandle(r,hit);auto q=TransformPoint(p.x,p.y,r,a,x,y);
    return {LONG(lround(q.x)),LONG(lround(q.y))};
}
CRect DragTransformFrame(const RECT& start,HITTEST hit,POINT origin,POINT p,double a,double x,double y,int minSize,bool fixedWidth,bool fixedHeight)
{
    if(hit==HIT_INNER || hit==HIT_BORDER) { CRect r=start;r.OffsetRect(p.x-origin.x,p.y-origin.y);return r; }
    auto q=TransformPoint(p.x,p.y,start,a,x,y,true),o=TransformPoint(origin.x,origin.y,start,a,x,y,true);
    POINT delta={LONG(lround(q.x-o.x)),LONG(lround(q.y-o.y))};
    CRect r=DragEditFrame(start,hit,{0,0},delta,0,minSize,minSize,fixedWidth,fixedHeight);
    double cx=(r.left+r.right-start.left-start.right)*.5,cy=(r.top+r.bottom-start.top-start.bottom)*.5;
    auto c=TransformPoint((start.left+start.right)*.5+cx,(start.top+start.bottom)*.5+cy,start,a,x,y);
    r.OffsetRect(LONG(lround(c.x-(start.left+start.right)*.5-cx)),LONG(lround(c.y-(start.top+start.bottom)*.5-cy)));
    return r;
}
void DragFrameSkew(const RECT& r,HITTEST hit,POINT origin,POINT p,double a,double sx,double sy,bool snap,double& x,double& y)
{
    double angle=a*M_PI/180,dx=p.x-origin.x,dy=p.y-origin.y;
    double ux=dx*cos(angle)+dy*sin(angle),uy=-dx*sin(angle)+dy*cos(angle);
    x=sx;y=sy;
    bool horizontal=hit==HIT_UPPER_CENTER || hit==HIT_LOWER_CENTER;
    if(horizontal) x+=(hit==HIT_UPPER_CENTER?-2.0:2.0)*ux/max(1L,r.bottom-r.top);
    else y+=(hit==HIT_MIDDLE_LEFT?-2.0:2.0)*uy/max(1L,r.right-r.left);
    double& value=horizontal?x:y;
    if(snap) value=tan(round(atan(value)*180/M_PI/15)*15*M_PI/180);
    value=max(-8.0,min(8.0,value));
}
