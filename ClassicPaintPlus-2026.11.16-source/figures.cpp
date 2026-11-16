#include <vector>
#include <initializer_list>
#include "precomp.h"
#include "figures.h"
#include "figuregeometry.h"

bool IsBoundedFigureTool(int tool)
{
    return tool==TOOL_RECT || tool==TOOL_ELLIPSE || tool==TOOL_RRECT || tool==TOOL_FIGURE;
}

bool IsRoundedFigureTool()
{ return toolsModel.GetActiveTool()==TOOL_SHAPE || (IsBoundedFigureTool(toolsModel.GetActiveTool()) && CanRoundFigure(toolsModel.ObjectSizeKind(false))); }

double FigureRotationAngle(double radians,bool snap)
{
    double a=atan2(sin(radians),cos(radians));
    if(snap) a=(a<0?-1:1)*floor(fabs(a)/(M_PI/12)+.5)*(M_PI/12);
    return a;
}

CRect FigureBounds(POINT start, POINT end, bool center, bool constrain)
{
    LONG dx=end.x-start.x,dy=end.y-start.y;
    if(constrain) { LONG side=max(labs(dx),labs(dy)); dx=dx<0?-side:side; dy=dy<0?-side:side; }
    CRect rc;
    if(center) rc.SetRect(start.x-labs(dx),start.y-labs(dy),start.x+labs(dx),start.y+labs(dy));
    else { rc=CRect(start,CPoint(start.x+dx,start.y+dy)); rc.NormalizeRect(); }
    return rc;
}

void DrawFigure(HDC dc,int kind,const RECT& bounds,HBRUSH fg,HBRUSH bg,int thickness,int style,double angle,double skewX,double skewY,int rounding,int starBend)
{
    CRect rc(bounds);if(rc.Width()<=0 || rc.Height()<=0)return;
    if(rounding<0)rounding=FigureRounding(kind);
    if(fabs(angle)<1e-8 && !skewX && !skewY)
    {
        if(kind==FIG_RECT && !rounding) { Rect(dc,rc.left,rc.top,rc.right,rc.bottom,fg,bg,thickness,style);return; }
        if(kind==FIG_ELLIPSE) { Ellp(dc,rc.left,rc.top,rc.right,rc.bottom,fg,bg,thickness,style);return; }
    }
    int steps=max(32,int(ceil(M_PI*sqrt(double(max(rc.Width(),rc.Height()))))));
    auto contours=FigureGeometry(kind,steps,starBend);
    double cx=(rc.left+rc.right-1)*.5,cy=(rc.top+rc.bottom-1)*.5,c=cos(angle),s=sin(angle);
    for(const auto& contour:contours)
    {
        FigureContour pixels;pixels.reserve(contour.size());
        for(auto p:contour)
        {
            double x=(p.x-.5)*(rc.Width()-1),y=(p.y-.5)*(rc.Height()-1);
            x+=skewX*y;y+=skewY*x;pixels.push_back({x,y});
        }
        // Round in image pixels after stretching/skewing; rotation then keeps
        // the circular radius unchanged instead of turning it into an ellipse.
        auto rounded=CanRoundFigure(kind)?RoundFigureContour(pixels,rounding,1,1):pixels;
        std::vector<POINT> points;points.reserve(rounded.size());
        for(auto p:rounded)
        {
            double x=p.x,y=p.y;
            POINT point={LONG(floor(cx+c*x-s*y+.5)),LONG(floor(cy+s*x+c*y+.5))};
            if(points.empty() || point.x!=points.back().x || point.y!=points.back().y)points.push_back(point);
        }
        if(points.size()>1)Poly(dc,points.data(),int(points.size()),fg,bg,thickness,style,TRUE,FALSE);
    }
}

void DrawFigureIcon(HDC dc,int kind,RECT r,COLORREF color)
{
    struct Runtime { ULONG_PTR token=0;Runtime(){Gdiplus::GdiplusStartupInput i;Gdiplus::GdiplusStartup(&token,&i,NULL);}~Runtime(){if(token)Gdiplus::GdiplusShutdown(token);} };
    static Runtime runtime;
    using namespace Gdiplus;
    Graphics graphics(dc);graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    graphics.TranslateTransform(float(r.left),float(r.top));
    graphics.ScaleTransform((r.right-r.left)/20.f,(r.bottom-r.top)/20.f);
    GraphicsPath path;
    auto polygon=[&](std::initializer_list<PointF> vertices)
    { std::vector<PointF> p(vertices);path.AddPolygon(p.data(),int(p.size())); };
    auto rounded=[&](float left,float top,float width,float height,float radius)
    {
        float d=2*radius;
        path.AddArc(left,top,d,d,180,90);path.AddArc(left+width-d,top,d,d,270,90);
        path.AddArc(left+width-d,top+height-d,d,d,0,90);path.AddArc(left,top+height-d,d,d,90,90);path.CloseFigure();
    };
    switch(kind)
    {
    case -TOOL_LINE:path.AddLine(2,2,18,18);break;
    case -TOOL_BEZIER:path.AddBezier(4,18,22,15,-3,7,16,2);break;
    case -TOOL_SHAPE:polygon({{2,3},{13,3},{8,14},{17,14},{17,18},{3,18},{8,7},{2,7}});break;
    case FIG_ELLIPSE: path.AddEllipse(1.f,4.f,18.f,12.f);break;
    case FIG_RECT: path.AddRectangle(RectF(1,3,18,14));break;
    case FIG_RRECT: rounded(1,3,18,14,3);break;
    case FIG_TRIANGLE: polygon({{10,1},{19,18},{1,18}});break;
    case FIG_RIGHT_TRIANGLE: polygon({{2,2},{18,18},{2,18}});break;
    case FIG_DIAMOND: polygon({{10,1},{19,10},{10,19},{1,10}});break;
    case FIG_PENTAGON: case FIG_HEXAGON:
    {
        int n=kind==FIG_PENTAGON?5:6;float phase=kind==FIG_PENTAGON?-float(M_PI)/2:0;
        std::vector<PointF> points;
        for(int i=0;i<n;++i) { float a=phase+float(2*M_PI*i/n);points.push_back(PointF(10+9*cosf(a),10+9*sinf(a))); }
        path.AddPolygon(points.data(),n);break;
    }
    case FIG_RIGHT_ARROW: case FIG_LEFT_ARROW: case FIG_UP_ARROW: case FIG_DOWN_ARROW:
    {
        PointF points[]={{1,6},{11,6},{11,1},{19,10},{11,19},{11,14},{1,14}};
        for(auto& p:points)
        {
            if(kind==FIG_LEFT_ARROW) p.X=20-p.X;
            else if(kind==FIG_UP_ARROW) { float x=p.X;p.X=p.Y;p.Y=20-x; }
            else if(kind==FIG_DOWN_ARROW) { float x=p.X;p.X=20-p.Y;p.Y=x; }
        }
        path.AddPolygon(points,_countof(points));break;
    }
    case FIG_STAR5:
    {
        std::vector<PointF> points;
        const auto geometry=FigureGeometry(FIG_STAR5);
        for(auto p:geometry[0])points.push_back(PointF(float(1+18*p.x),float(1+18*p.y)));
        path.AddPolygon(points.data(),int(points.size()));break;
    }
    case FIG_STAR4: case FIG_STAR6:
    {
        int n=kind-FIG_STAR4+4;std::vector<PointF> points;
        for(int i=0;i<2*n;++i)
        {
            float a=float(-M_PI/2+M_PI*i/n),radius=(i&1)?(n==4?3.f:4.5f):9.f;
            points.push_back(PointF(10+radius*cosf(a),10+radius*sinf(a)));
        }
        path.AddPolygon(points.data(),int(points.size()));break;
    }
    case FIG_RECT_CALLOUT:
        polygon({{1,2},{19,2},{19,14},{10,14},{5,19},{6,14},{1,14}});break;
    case FIG_OVAL_CALLOUT:
        path.AddBezier(6,15,1,14,-1,7,4,4);path.AddBezier(4,4,10,0,19,3,19,9);
        path.AddBezier(19,9,19,14,15,16,10,16);path.AddLine(10,16,4,19);path.AddLine(4,19,6,15);path.CloseFigure();break;
    case FIG_CLOUD_CALLOUT:
        path.AddBezier(4,14,0,14,0,9,2,8);path.AddBezier(2,8,0,4,4,2,6,4);
        path.AddBezier(6,4,7,0,12,0,13,3);path.AddBezier(13,3,17,1,20,5,18,8);
        path.AddBezier(18,8,21,11,18,15,15,14);path.AddBezier(15,14,14,17,10,17,9,15);
        path.AddBezier(9,15,7,17,4,16,4,14);path.CloseFigure();
        path.AddEllipse(3.f,17.f,3.f,2.f);path.AddEllipse(1.f,19.f,1.f,1.f);break;
    default:
        for(const auto& contour:FigureGeometry(kind))
        {
            std::vector<PointF> p;for(auto v:contour)p.push_back(PointF(float(1+18*v.x),float(1+18*v.y)));
            if(p.size()>2)path.AddPolygon(p.data(),int(p.size()));
        }
        if(!path.GetPointCount())return;
    
    }
    Color outline(255,GetRValue(color),GetGValue(color),GetBValue(color));
    Pen pen(outline,1.1f);pen.SetLineJoin(LineJoinRound);
    LinearGradientBrush fill(PointF(0,1),PointF(0,19),
        registrySettings.DarkTheme?Color(255,76,80,84):Color(255,255,255,255),
        registrySettings.DarkTheme?Color(255,48,52,56):Color(255,220,236,247));
    if(kind>=0 || kind==-TOOL_SHAPE)graphics.FillPath(&fill,&path);
    graphics.DrawPath(&pen,&path);
    int points=FigureStarPoints(kind);
    if(points>=8)
    {
        WCHAR label[8];StringCchPrintfW(label,_countof(label),L"%d",points);
        Font font(FontFamily::GenericSansSerif(),points<10?8.f:7.f,FontStyleBold,UnitPixel);
        StringFormat format;format.SetAlignment(StringAlignmentCenter);format.SetLineAlignment(StringAlignmentCenter);
        SolidBrush ink(registrySettings.DarkTheme?Color(255,240,240,240):Color(255,0,0,0));graphics.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        graphics.DrawString(label,-1,&font,RectF(2,2,16,16),&format,&ink);
    }
}
