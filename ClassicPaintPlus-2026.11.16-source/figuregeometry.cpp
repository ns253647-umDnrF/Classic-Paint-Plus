#include "precomp.h"
#include "figuregeometry.h"

int FigureStarPoints(int kind)
{
    switch(kind) { case FIG_STAR4:return 4;case FIG_STAR5:return 5;case FIG_STAR6:return 6;
    case FIG_STAR8:return 8;case FIG_STAR16:return 16;case FIG_STAR24:return 24;case FIG_STAR32:return 32;default:return 0; }
}
namespace {
void Turn(FigureContour& p,int quarter)
{ for(auto& v:p)for(int i=0;i<quarter;++i) { double x=v.x;v.x=1-v.y;v.y=x; } }
FigureContour CircularArrow(bool full)
{
    FigureContour p;double start=full?-M_PI*.43:-M_PI*.86,end=full?M_PI*1.22:M_PI*.5;
    double bodyEnd=end-.35,outer=.38,inner=.22;
    auto add=[&](double radius,double a){p.push_back({.5+radius*cos(a),.5+radius*sin(a)});};
    for(int i=0;i<=64;++i)add(outer,start+(bodyEnd-start)*i/64);
    add(.49,bodyEnd);add(.30,end+.18);add(.10,bodyEnd);add(inner,bodyEnd);
    for(int i=63;i>=0;--i)add(inner,start+(bodyEnd-start)*i/64);
    return p;
}
}
FigureContours FigureGeometry(int kind,int ellipseSteps,int starBend)
{
    FigureContours result(1);auto& p=result[0];
    auto add=[&](double x,double y){p.push_back({x,y});};
    int star=FigureStarPoints(kind);
    if(star)
    {
        // A regular pentagram: each inward corner is the intersection of two
        // diagonals of the outer pentagon, so adjoining ray edges stay collinear.
        double standard=star==5 ? .5*cos(2*M_PI/5)/cos(M_PI/5) : star==4?.16:star<8?.23:.38;
        starBend=max(-100,min(100,starBend));
        // At +100 the inner vertices lie on the polygon's straight edges.
        double inner=starBend<0?standard*(1+starBend/100.0):standard+(.5*cos(M_PI/star)-standard)*starBend/100.0;
        for(int i=0;i<2*star;++i)
        { double a=-M_PI/2+M_PI*i/star,r=(i&1)?inner:.5;add(.5+r*cos(a),.5+r*sin(a)); }
        return result;
    }
    switch(kind)
    {
    case FIG_RECT:case FIG_RRECT:p={{0,0},{1,0},{1,1},{0,1}};break;
    case FIG_ELLIPSE:
        for(int i=0;i<ellipseSteps;++i) { double a=2*M_PI*i/ellipseSteps;add(.5+.5*cos(a),.5+.5*sin(a)); }break;
    case FIG_TRIANGLE:p={{.5,0},{1,1},{0,1}};break;
    case FIG_RIGHT_TRIANGLE:p={{0,0},{1,1},{0,1}};break;
    case FIG_DIAMOND:p={{.5,0},{1,.5},{.5,1},{0,.5}};break;
    case FIG_PENTAGON:case FIG_HEXAGON:
        for(int i=0,n=kind==FIG_PENTAGON?5:6;i<n;++i) { double a=(n==5?-M_PI/2:0)+2*M_PI*i/n;add(.5+.5*cos(a),.5+.5*sin(a)); }break;
    case FIG_RIGHT_ARROW:case FIG_LEFT_ARROW:case FIG_UP_ARROW:case FIG_DOWN_ARROW:
    case FIG_STRIPED_RIGHT_ARROW:case FIG_NOTCHED_RIGHT_ARROW:
        p={{0,.28},{.58,.28},{.58,0},{1,.5},{.58,1},{.58,.72},{0,.72}};
        if(kind==FIG_LEFT_ARROW)Turn(p,2);if(kind==FIG_UP_ARROW)Turn(p,3);if(kind==FIG_DOWN_ARROW)Turn(p,1);
        if(kind==FIG_NOTCHED_RIGHT_ARROW)p.push_back({.18,.5});
        if(kind==FIG_STRIPED_RIGHT_ARROW)
        {
            for(auto& v:p)v.x=.24+v.x*.76;
            result.push_back({{0,.28},{.045,.28},{.045,.72},{0,.72}});
            result.push_back({{.09,.28},{.17,.28},{.17,.72},{.09,.72}});
        }
        break;
    case FIG_LEFT_RIGHT_ARROW:case FIG_UP_DOWN_ARROW:
        p={{0,.5},{.3,0},{.3,.3},{.7,.3},{.7,0},{1,.5},{.7,1},{.7,.7},{.3,.7},{.3,1}};
        if(kind==FIG_UP_DOWN_ARROW)Turn(p,1);break;
    case FIG_QUAD_ARROW:
        p={{.5,0},{.72,.25},{.60,.25},{.60,.40},{.75,.40},{.75,.28},{1,.5},{.75,.72},{.75,.60},{.60,.60},{.60,.75},{.72,.75},{.5,1},{.28,.75},{.40,.75},{.40,.60},{.25,.60},{.25,.72},{0,.5},{.25,.28},{.25,.40},{.40,.40},{.40,.25},{.28,.25}};break;
    case FIG_LEFT_RIGHT_UP_ARROW:
        p={{.5,0},{.75,.3},{.62,.3},{.62,.63},{.75,.63},{.75,.45},{1,.73},{.75,1},{.75,.84},{.25,.84},{.25,1},{0,.73},{.25,.45},{.25,.63},{.38,.63},{.38,.3},{.25,.3}};break;
    case FIG_BENT_ARROW:
        p={{0,1},{0,.3},{.03,.18},{.13,.10},{.25,.07},{.63,.07},{.63,0},{1,.22},{.63,.45},{.63,.30},{.28,.30},{.23,.34},{.23,1}};break;
    case FIG_UTURN_ARROW:
        p={{0,1},{0,.32}};
        for(int i=0;i<=24;++i){double a=M_PI+M_PI*i/24;add(.4+.4*cos(a),.32+.32*sin(a));}
        add(.8,.65);add(1,.65);add(.68,1);add(.38,.65);add(.56,.65);add(.56,.32);
        for(int i=24;i>=0;--i){double a=M_PI+M_PI*i/24;add(.4+.16*cos(a),.32+.12*sin(a));}add(.24,1);break;
    case FIG_LEFT_UP_ARROW:
        p={{.7,0},{1,.32},{.82,.32},{.82,.82},{.32,.82},{.32,1},{0,.7},{.32,.4},{.32,.58},{.58,.58},{.58,.32},{.4,.32}};break;
    case FIG_BENT_UP_ARROW:
        p={{0,.76},{.60,.76},{.60,.3},{.42,.3},{.71,0},{1,.3},{.82,.3},{.82,1},{0,1}};break;
    case FIG_CURVED_LEFT_ARROW:case FIG_CURVED_RIGHT_ARROW:case FIG_CURVED_UP_ARROW:case FIG_CURVED_DOWN_ARROW:
        p=CircularArrow(false);
        if(kind==FIG_CURVED_RIGHT_ARROW)Turn(p,2);if(kind==FIG_CURVED_UP_ARROW)Turn(p,1);if(kind==FIG_CURVED_DOWN_ARROW)Turn(p,3);break;
    case FIG_PENTAGON_ARROW:p={{0,0},{.65,0},{1,.5},{.65,1},{0,1}};break;
    case FIG_CHEVRON:p={{0,0},{.55,0},{1,.5},{.55,1},{0,1},{.45,.5}};break;
    case FIG_RIGHT_ARROW_CALLOUT:case FIG_LEFT_ARROW_CALLOUT:case FIG_UP_ARROW_CALLOUT:case FIG_DOWN_ARROW_CALLOUT:
        p={{0,.12},{.58,.12},{.58,.36},{.75,.36},{.75,0},{1,.5},{.75,1},{.75,.64},{.58,.64},{.58,.88},{0,.88}};
        if(kind==FIG_LEFT_ARROW_CALLOUT)Turn(p,2);if(kind==FIG_UP_ARROW_CALLOUT)Turn(p,3);if(kind==FIG_DOWN_ARROW_CALLOUT)Turn(p,1);break;
    case FIG_LEFT_RIGHT_ARROW_CALLOUT:
        p={{0,.5},{.2,.15},{.2,.38},{.32,.38},{.32,0},{.68,0},{.68,.38},{.8,.38},{.8,.15},{1,.5},{.8,.85},{.8,.62},{.68,.62},{.68,1},{.32,1},{.32,.62},{.2,.62},{.2,.85}};break;
    case FIG_QUAD_ARROW_CALLOUT:
        p={{.5,0},{.67,.18},{.57,.18},{.57,.29},{.71,.29},{.71,.43},{.82,.43},{.82,.33},{1,.5},{.82,.67},{.82,.57},{.71,.57},{.71,.71},{.57,.71},{.57,.82},{.67,.82},{.5,1},{.33,.82},{.43,.82},{.43,.71},{.29,.71},{.29,.57},{.18,.57},{.18,.67},{0,.5},{.18,.33},{.18,.43},{.29,.43},{.29,.29},{.43,.29},{.43,.18},{.33,.18}};break;
    case FIG_CIRCULAR_ARROW:case FIG_LEFT_CIRCULAR_ARROW:
        p=CircularArrow(true);if(kind==FIG_LEFT_CIRCULAR_ARROW)for(auto& v:p)v.x=1-v.x;break;
    case FIG_RECT_CALLOUT:p={{0,0},{1,0},{1,.75},{.45,.75},{.18,1},{.23,.75},{0,.75}};break;
    case FIG_OVAL_CALLOUT:case FIG_CLOUD_CALLOUT:
        for(int i=0;i<=120;++i)
        {
            double a=2*M_PI*i/120;
            if(i==42)add(.2,1);if(i>40 && i<48)continue;
            double r=kind==FIG_CLOUD_CALLOUT?(.9+.1*cos(10*a)):1;add(.5+.5*r*cos(a),.4+.4*r*sin(a));
        }break;
    default:return {};
    }
    return result;
}

FigureContour RoundFigureContour(const FigureContour& vertices,int radius,double width,double height,bool closed)
{
    if(radius<=0 || width<=0 || height<=0)return vertices;
    // The free-polygon tool keeps a duplicate endpoint while constructing the
    // next edge. Zero-length edges must not prevent either neighbour rounding.
    FigureContour points;points.reserve(vertices.size());
    for(auto v:vertices)if(points.empty() || v.x!=points.back().x || v.y!=points.back().y)points.push_back(v);
    if(closed && points.size()>1 && points.front().x==points.back().x && points.front().y==points.back().y)points.pop_back();
    if(points.size()<3)return points;
    for(auto& p:points){p.x*=width;p.y*=height;}
    struct Corner { FigureVertex before={},after={};double tangent=0; };
    std::vector<Corner> corners(points.size());
    for(size_t i=0;i<points.size();++i)
    {
        if(!closed && (i==0 || i+1==points.size()))continue;
        auto v=points[i],prev=points[(i+points.size()-1)%points.size()],next=points[(i+1)%points.size()];
        double a=hypot(prev.x-v.x,prev.y-v.y),b=hypot(next.x-v.x,next.y-v.y);
        if(a<1e-8 || b<1e-8)continue;
        auto& c=corners[i];c.before={(prev.x-v.x)/a,(prev.y-v.y)/a};c.after={(next.x-v.x)/b,(next.y-v.y)/b};
        double dot=max(-1.0,min(1.0,c.before.x*c.after.x+c.before.y*c.after.y));
        // A circular fillet touches each incident edge at r*cot(angle/2).
        // Straight and reversing edges have no well-defined corner to round.
        if(fabs(c.before.x*c.after.y-c.before.y*c.after.x)>1e-8)
            c.tangent=sqrt((1+dot)/(1-dot));
    }
    double effective=radius;
    for(size_t i=0;i<(closed?points.size():points.size()-1);++i)
    {
        size_t j=(i+1)%points.size();double sum=corners[i].tangent+corners[j].tangent;
        if(sum>1e-8)effective=min(effective,hypot(points[j].x-points[i].x,points[j].y-points[i].y)/sum);
    }
    // Limit all corners together to keep the same actual radius when a short
    // edge prevents the requested radius. Sampling error stays below 0.1 px.
    FigureContour rounded;
    for(size_t i=0;i<points.size();++i)
    {
        const auto& c=corners[i];auto v=points[i];
        if(c.tangent<=1e-8 || effective<=1e-8){rounded.push_back(v);continue;}
        double distance=effective*c.tangent;
        FigureVertex a={v.x+c.before.x*distance,v.y+c.before.y*distance},b={v.x+c.after.x*distance,v.y+c.after.y*distance};
        double bx=c.before.x+c.after.x,by=c.before.y+c.after.y;
        double centreDistance=hypot(effective,distance)/hypot(bx,by);
        FigureVertex centre={v.x+bx*centreDistance,v.y+by*centreDistance};
        double start=atan2(a.y-centre.y,a.x-centre.x);
        double sweep=atan2((a.x-centre.x)*(b.y-centre.y)-(a.y-centre.y)*(b.x-centre.x),
                           (a.x-centre.x)*(b.x-centre.x)+(a.y-centre.y)*(b.y-centre.y));
        double step=2*acos(max(-1.0,1-.1/effective));
        int steps=max(2,min(512,int(ceil(fabs(sweep)/step))));
        for(int j=0;j<=steps;++j)
        { double angle=start+sweep*j/steps;rounded.push_back({centre.x+effective*cos(angle),centre.y+effective*sin(angle)}); }
    }
    for(auto& p:rounded){p.x/=width;p.y/=height;}
    return rounded;
}
