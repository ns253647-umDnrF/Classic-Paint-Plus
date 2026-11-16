// Targa 1.0/2.0: indexed, greyscale and truecolour; raw and RLE packets.
#include "precomp.h"
#include "imageformats.h"
namespace {
unsigned U16(const BYTE* p) { return p[0]|unsigned(p[1])<<8; }
DWORD U32(const BYTE* p) { return p[0]|DWORD(p[1])<<8|DWORD(p[2])<<16|DWORD(p[3])<<24; }
void Put32(std::vector<BYTE>& b,DWORD v) { for(int i=0;i<4;++i) b.push_back(BYTE(v>>(i*8))); }
DWORD Colour(const BYTE* p,int bits,bool alpha)
{
    if(bits==15 || bits==16)
    { unsigned v=U16(p),r=(v>>10)&31,g=(v>>5)&31,b=v&31;return (alpha?(v&0x8000?0xff000000:0):0xff000000)|((r*255+15)/31<<16)|((g*255+15)/31<<8)|(b*255+15)/31; }
    return DWORD(alpha && bits==32?p[3]:255)<<24|DWORD(p[2])<<16|DWORD(p[1])<<8|p[0];
}
}
bool DecodeTga(const std::vector<BYTE>& bytes,RasterImage& image,ImageFileInfo* info)
{
    if(bytes.size()<18) return false;
    const BYTE* h=bytes.data();int type=h[2],base=type&7,bits=h[16],descriptor=h[17];
    bool rle=type>=9 && type<=11,pal=base==1,grey=base==3;
    if((!rle && (type<1 || type>3)) || (base<1 || base>3) || h[1]>1 || (descriptor&0xc0)) return false;
    if((pal && (h[1]!=1 || (bits!=8 && bits!=16))) ||
       (grey && bits!=8 && bits!=16) || (!pal && !grey && bits!=15 && bits!=16 && bits!=24 && bits!=32)) return false;
    unsigned width=U16(h+12),height=U16(h+14);
    if(!width || !height || size_t(width)*height>100000000) return false;
    size_t cursor=18+h[0];if(cursor>bytes.size()) return false;
    bool alpha=(descriptor&15)!=0 || (pal && (h[7]==16 || h[7]==32)),premult=false;
    // A 2.0 extension can explicitly mark the stored alpha as meaningful,
    // ignored, or premultiplied. Never mistake padding for transparency.
    if(bytes.size()>=26 && !memcmp(bytes.data()+bytes.size()-18,"TRUEVISION-XFILE.\0",18))
    {
        size_t ext=U32(bytes.data()+bytes.size()-26);
        if(ext && ext<=bytes.size()-26 && bytes.size()-26-ext>=495 && U16(bytes.data()+ext)>=495)
        { BYTE mode=bytes[ext+494];if(mode<=1) alpha=false;else if(mode<=4) { alpha=true;premult=mode==4; } }
    }
    std::vector<DWORD> palette;
    unsigned first=U16(h+3),colours=U16(h+5),mapBits=h[7];
    if(h[1])
    {
        if(!colours || first+colours>65536 || (mapBits!=15 && mapBits!=16 && mapBits!=24 && mapBits!=32)) return false;
        unsigned stride=(mapBits+7)/8;if(size_t(colours)*stride>bytes.size()-cursor) return false;
        palette.resize(first+colours);
        for(unsigned i=0;i<colours;++i,cursor+=stride) palette[first+i]=Colour(bytes.data()+cursor,mapBits,alpha);
    }
    unsigned stride=(bits+7)/8;
    auto pixel=[&](DWORD& value)->bool
    {
        if(bytes.size()-cursor<stride) return false;
        const BYTE* p=bytes.data()+cursor;cursor+=stride;
        if(pal) { unsigned index=bits==8?p[0]:U16(p);if(index<first || index>=palette.size()) return false;value=palette[index]; }
        else if(grey) value=DWORD(bits==16 && alpha?p[1]:255)<<24|DWORD(p[0])*0x010101;
        else value=Colour(p,bits,alpha);
        if(premult)
        {
            unsigned a=value>>24;DWORD straight=a<<24;
            if(a) for(int c=0;c<3;++c) straight|=min(255u,((value>>(c*8)&255)*255+a/2)/a)<<(c*8);
            value=straight;
        }
        return true;
    };
    RasterImage out;out.width=width;out.height=height;out.pixels.resize(size_t(width)*height);
    size_t count=out.pixels.size(),at=0;
    while(at<count)
    {
        unsigned length=1;bool repeat=false;
        if(rle) { if(cursor==bytes.size()) return false;BYTE code=bytes[cursor++];length=(code&127)+1;repeat=(code&128)!=0; }
        if(length>count-at) return false;
        DWORD value=0;if(repeat && !pixel(value)) return false;
        for(unsigned i=0;i<length;++i,++at)
        {
            if(!repeat && !pixel(value)) return false;
            size_t x=at%width,y=at/width;
            if(descriptor&16) x=width-1-x;if(!(descriptor&32)) y=height-1-y;
            out.pixels[y*width+x]=value;
        }
    }
    image=std::move(out);if(info) { info->format=SaveFormat::Tga;info->paintData=ReadTgaPaintData(bytes); }return true;
}
bool EncodeTga(const RasterImage& image,std::vector<BYTE>& bytes)
{
    if(!image.Valid() || image.width>65535 || image.height>65535) return false;
    bytes.assign(18,0);bytes[2]=10;bytes[12]=BYTE(image.width);bytes[13]=BYTE(image.width>>8);
    bytes[14]=BYTE(image.height);bytes[15]=BYTE(image.height>>8);bytes[16]=32;bytes[17]=0x28;
    // Keep packets within scanlines for compatibility with older TGA readers.
    for(int y=0;y<image.height;++y)
    {
        const DWORD* row=image.pixels.data()+size_t(y)*image.width;int x=0;
        while(x<image.width)
        {
            int run=1;while(run<128 && x+run<image.width && row[x+run]==row[x]) ++run;
            if(run>1) { bytes.push_back(BYTE(0x80|(run-1)));Put32(bytes,row[x]);x+=run; }
            else
            {
                int start=x++;
                while(x<image.width && x-start<128 && !(x+1<image.width && row[x]==row[x+1])) ++x;
                bytes.push_back(BYTE(x-start-1));for(int i=start;i<x;++i) Put32(bytes,row[i]);
            }
        }
    }
    // Specify straight alpha in a TGA 2.0 extension.
    if(bytes.size()>0xffffffffu-521) return false;
    DWORD extension=DWORD(bytes.size());bytes.resize(bytes.size()+495,0);
    bytes[extension]=239;bytes[extension+1]=1;bytes[extension+494]=3;
    Put32(bytes,extension);Put32(bytes,0);const char signature[]="TRUEVISION-XFILE.";
    bytes.insert(bytes.end(),signature,signature+sizeof(signature));return true;
}
