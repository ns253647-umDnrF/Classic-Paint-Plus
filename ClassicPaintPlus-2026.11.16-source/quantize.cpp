// Adaptive palette quantization: weighted variance splitting followed by Lloyd
// refinement. Common flat colors are fixed so diagrams/text keep their exact ink.
// This is a local implementation; it does not call TinyPNG or use its code.
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <cmath>
#include "precomp.h"
#include "imageformats.h"

namespace {
struct ColorPoint { DWORD color;double weight;float c[4];int index=0; };
struct Center { float c[4];bool fixed=false; };
int AlphaClass(float alpha) { return alpha<.5f?0:alpha>254.5f?2:1; }
double Distance(const float* a,const float* b)
{
    if(AlphaClass(a[3])!=AlphaClass(b[3])) return 1e12;
    double r=a[0]-b[0],g=a[1]-b[1],blue=a[2]-b[2],alpha=a[3]-b[3];
    return .3*r*r+.59*g*g+.11*blue*blue+alpha*alpha;
}
void Components(DWORD p,float* out)
{
    out[3]=float(p>>24);float a=out[3]/255.f;
    out[0]=(p>>16&255)*a;out[1]=(p>>8&255)*a;out[2]=(p&255)*a;
}
DWORD Pack(const Center& center)
{
    unsigned a=unsigned(max(0,min(255,int(center.c[3]+.5f))));
    if(!a) return 0;
    unsigned p=a<<24;
    for(int c=0;c<3;++c) p|=unsigned(max(0,min(255,int(center.c[c]*255/a+.5f))))<<((2-c)*8);
    return p;
}
struct Box
{
    std::vector<int> points;double variance=0;int axis=0;
    void Measure(const std::vector<ColorPoint>& colors)
    {
        double sum[4]={},square[4]={},weight=0;
        for(int i:points)
        { const auto& p=colors[i];weight+=p.weight;for(int c=0;c<4;++c) { sum[c]+=p.weight*p.c[c];square[c]+=p.weight*p.c[c]*p.c[c]; } }
        variance=0;double biggest=0;const double channelWeight[4]={.3,.59,.11,1};
        for(int c=0;c<4;++c)
        {
            double v=max(0.0,square[c]-sum[c]*sum[c]/max(1.0,weight))*channelWeight[c];
            variance+=v;if(v>biggest) { biggest=v;axis=c; }
        }
    }
};
}
QuantizedImage QuantizeRaster(const RasterImage& image,int maxColors)
{
    QuantizedImage output;if(!image.Valid() || maxColors<2 || maxColors>256) return output;
    // Exact histogram for drawings; cap the histogram for photographic images.
    std::unordered_map<DWORD,size_t> map;std::vector<ColorPoint> colors;
    bool reduced=false;
    auto key=[&](DWORD p) { if(!(p>>24)) return DWORD(0);return reduced?(p&0xfff8f8f8):p; };
    for(int pass=0;pass<2;++pass)
    {
        map.clear();colors.clear();
        for(DWORD p:image.pixels)
        {
            DWORD k=key(p);auto found=map.find(k);
            if(found!=map.end()) { colors[found->second].weight+=1;continue; }
            ColorPoint item={k,1,{}};Components(k,item.c);map.emplace(k,colors.size());colors.push_back(item);
            if(!reduced && colors.size()>131072) break;
        }
        if(colors.size()<=131072 || reduced) break;reduced=true;
    }
    if(colors.size()<=size_t(maxColors))
    {
        for(size_t i=0;i<colors.size();++i) { colors[i].index=int(i);output.palette.push_back(colors[i].color); }
    }
    else
    {
        std::vector<int> ordered(colors.size());std::iota(ordered.begin(),ordered.end(),0);
        std::stable_sort(ordered.begin(),ordered.end(),[&](int a,int b) { return colors[a].weight>colors[b].weight; });
        std::vector<Center> centers;std::vector<bool> fixed(colors.size(),false);
        for(int i:ordered)
        {
            if(centers.size()>=size_t(maxColors/4) || colors[i].weight<image.pixels.size()*.001) break;
            Center center={};std::copy(colors[i].c,colors[i].c+4,center.c);center.fixed=true;centers.push_back(center);fixed[i]=true;
        }
        std::vector<Box> boxes(3);
        for(size_t i=0;i<colors.size();++i) if(!fixed[i]) boxes[AlphaClass(colors[i].c[3])].points.push_back(int(i));
        boxes.erase(std::remove_if(boxes.begin(),boxes.end(),[](const Box& box) { return box.points.empty(); }),boxes.end());
        for(auto& box:boxes) box.Measure(colors);
        while(centers.size()+boxes.size()<size_t(maxColors))
        {
            size_t choice=0;double score=-1;
            for(size_t i=0;i<boxes.size();++i) if(boxes[i].points.size()>1 && boxes[i].variance>score)
            { choice=i;score=boxes[i].variance; }
            if(score<0) break;
            Box& box=boxes[choice];int axis=box.axis;
            std::stable_sort(box.points.begin(),box.points.end(),[&](int a,int b) { return colors[a].c[axis]<colors[b].c[axis]; });
            double total=0;for(int i:box.points) total+=colors[i].weight;
            double cumulative=0;size_t split=0;
            while(split+1<box.points.size() && cumulative<total/2) cumulative+=colors[box.points[split++]].weight;
            split=max(size_t(1),min(split,box.points.size()-1));
            Box other;other.points.assign(box.points.begin()+split,box.points.end());box.points.resize(split);
            box.Measure(colors);other.Measure(colors);boxes.push_back(std::move(other));
        }
        for(const auto& box:boxes)
        {
            Center center={};double total=0;double sum[4]={};
            for(int i:box.points) { total+=colors[i].weight;for(int c=0;c<4;++c) sum[c]+=colors[i].weight*colors[i].c[c]; }
            for(int c=0;c<4;++c) center.c[c]=float(sum[c]/total);centers.push_back(center);
        }
        for(int pass=0;pass<7;++pass)
        {
            struct Sum { double c[4]={},weight=0; };std::vector<Sum> sums(centers.size());
            for(auto& p:colors)
            {
                double best=1e30;int index=0;
                for(size_t i=0;i<centers.size();++i) { double d=Distance(p.c,centers[i].c);if(d<best) { best=d;index=int(i); } }
                p.index=index;auto& sum=sums[index];sum.weight+=p.weight;for(int c=0;c<4;++c) sum.c[c]+=p.weight*p.c[c];
            }
            if(pass==6) break;
            for(size_t i=0;i<centers.size();++i) if(!centers[i].fixed && sums[i].weight)
                for(int c=0;c<4;++c) centers[i].c[c]=float(sums[i].c[c]/sums[i].weight);
        }
        for(const auto& center:centers) output.palette.push_back(Pack(center));
    }
    output.indices.reserve(image.pixels.size());double error=0;
    for(DWORD p:image.pixels)
    {
        int index=colors[map.at(key(p))].index;output.indices.push_back(BYTE(index));
        float before[4],after[4];Components(p,before);Components(output.palette[index],after);
        for(int c=0;c<4;++c) { double d=before[c]-after[c];error+=d*d; }
    }
    output.rms=sqrt(error/max(1.0,double(image.pixels.size())*4));return output;
}
