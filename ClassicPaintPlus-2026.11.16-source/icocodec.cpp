#include <vector>
#include "precomp.h"
#include "imageformats.h"

namespace {
unsigned U16(const BYTE* p) { return unsigned(p[0])|unsigned(p[1])<<8; }
unsigned U32(const BYTE* p) { return U16(p)|(U16(p+2)<<16); }
void W16(std::vector<BYTE>& b,size_t p,unsigned v) { b[p]=BYTE(v);b[p+1]=BYTE(v>>8); }
void W32(std::vector<BYTE>& b,size_t p,unsigned v) { W16(b,p,v);W16(b,p+2,v>>16); }
unsigned Channel(unsigned pixel,unsigned mask)
{
    if(!mask) return 0;unsigned shift=0;
    while(!(mask&1)) { mask>>=1;++shift; }
    return unsigned((static_cast<ULONGLONG>((pixel>>shift)&mask)*255+mask/2)/mask);
}
bool DecodeIconDib(const IconFrame& frame,RasterImage& image)
{
    const auto& b=frame.encoded;if(b.size()<40) return false;
    unsigned header=U32(b.data()),w=U32(b.data()+4),fullHeight=U32(b.data()+8);
    if(header<40 || header>b.size() || !w || w>256 || !fullHeight || fullHeight>512 || (fullHeight&1)) return false;
    unsigned h=fullHeight/2,depth=U16(b.data()+14),compression=U32(b.data()+16),colors=U32(b.data()+32);
    if(U16(b.data()+12)!=1 || (depth!=1 && depth!=4 && depth!=8 && depth!=16 && depth!=24 && depth!=32) ||
        (compression!=0 && compression!=3)) return false;
    unsigned red=depth==16?0x7c00:0xff0000,green=depth==16?0x3e0:0xff00,blue=depth==16?0x1f:0xff,alpha=depth==32?0xff000000:0;
    size_t pos=header;
    if(compression==3)
    {
        size_t masks=header>=52?40:pos;
        if(masks+12>b.size()) return false;
        red=U32(b.data()+masks);green=U32(b.data()+masks+4);blue=U32(b.data()+masks+8);
        if(header>=56) alpha=U32(b.data()+52);else if(header==40) pos+=12;
        if(!red || !green || !blue || (red&green) || (red&blue) || (green&blue)) return false;
    }
    if(depth<=8) { if(!colors) colors=1u<<depth;if(colors>(1u<<depth)) return false; }else colors=0;
    if(pos+size_t(colors)*4>b.size()) return false;
    const BYTE* palette=b.data()+pos;pos+=size_t(colors)*4;
    size_t stride=(size_t(w)*depth+31)/32*4,maskStride=(w+31)/32*4;
    if(stride*h>b.size()-pos) return false;
    const BYTE* pixels=b.data()+pos;pos+=stride*h;
    bool hasMask=maskStride*h<=b.size()-pos,anyAlpha=false;
    if(depth==32 && alpha)
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) if(U32(pixels+y*stride+x*4)&alpha) anyAlpha=true;
    if(depth!=32 && !hasMask) return false;
    image.width=w;image.height=h;image.pixels.resize(size_t(w)*h);
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x)
    {
        const BYTE* row=pixels+(h-1-y)*stride;unsigned r,g,blueValue,a=255;
        if(depth<=8)
        {
            unsigned index=depth==8?row[x]:depth==4?(row[x/2]>>((1-x%2)*4))&15:(row[x/8]>>(7-x%8))&1;
            if(index>=colors) return false;
            blueValue=palette[index*4];g=palette[index*4+1];r=palette[index*4+2];
        }
        else if(depth==24) { blueValue=row[x*3];g=row[x*3+1];r=row[x*3+2]; }
        else
        {
            unsigned p=depth==16?U16(row+x*2):U32(row+x*4);
            r=Channel(p,red);g=Channel(p,green);blueValue=Channel(p,blue);
            if(anyAlpha) a=Channel(p,alpha);
        }
        if(hasMask && (b[pos+(h-1-y)*maskStride+x/8]&(0x80>>(x%8)))) a=0;
        image.pixels[size_t(y)*w+x]=(a<<24)|(r<<16)|(g<<8)|blueValue;
    }
    return true;
}
bool EncodeIconDib(const RasterImage& image,std::vector<BYTE>& b)
{
    size_t stride=size_t(image.width)*4,maskStride=(image.width+31)/32*4;
    b.assign(40+(stride+maskStride)*image.height,0);
    W32(b,0,40);W32(b,4,image.width);W32(b,8,image.height*2);W16(b,12,1);W16(b,14,32);
    W32(b,20,unsigned((stride+maskStride)*image.height));
    for(int y=0;y<image.height;++y) for(int x=0;x<image.width;++x)
    {
        DWORD pixel=image.pixels[size_t(y)*image.width+x];
        W32(b,40+size_t(image.height-1-y)*stride+x*4,pixel);
        if(!(pixel>>24)) b[40+stride*image.height+size_t(image.height-1-y)*maskStride+x/8]|=0x80>>(x%8);
    }
    return true;
}
}
bool DecodeIconFrame(const IconFrame& frame,RasterImage& image)
{
    bool ok=frame.encoded.size()>=8 && !memcmp(frame.encoded.data(),"\x89PNG\r\n\x1a\n",8)
        ? DecodePng(frame.encoded,image):DecodeIconDib(frame,image);
    return ok && image.width==frame.width && image.height==frame.height;
}
bool DecodeIco(const std::vector<BYTE>& bytes,RasterImage& image,ImageFileInfo& info,int selected)
{
    if(bytes.size()<6 || U16(bytes.data())!=0 || U16(bytes.data()+2)!=1) return false;
    unsigned count=U16(bytes.data()+4);if(!count || count>256 || 6+size_t(count)*16>bytes.size()) return false;
    info.icons.clear();int largest=0,best=-1;
    for(unsigned i=0;i<count;++i)
    {
        const BYTE* entry=bytes.data()+6+i*16;unsigned size=U32(entry+8),offset=U32(entry+12);
        if(offset<6+count*16 || offset>bytes.size() || size>bytes.size()-offset || !size) return false;
        IconFrame frame;frame.width=entry[0]?entry[0]:256;frame.height=entry[1]?entry[1]:256;
        frame.depth=U16(entry+6);if(!frame.depth) frame.depth=32;
        frame.encoded.assign(bytes.begin()+offset,bytes.begin()+offset+size);info.icons.push_back(std::move(frame));
        int score=info.icons.back().width*info.icons.back().height*64+min(32,info.icons.back().depth);
        if(score>best) { best=score;largest=int(i); }
    }
    info.selectedIcon=selected>=0?selected:largest;
    if(info.selectedIcon>=int(info.icons.size()) || !DecodeIconFrame(info.icons[info.selectedIcon],image)) return false;
    info.format=SaveFormat::Ico;info.paintData=ReadPngPaintData(info.icons[info.selectedIcon].encoded);return true;
}
bool EncodeIco(const RasterImage& source,const IcoOptions& options,const ImageFileInfo* info,std::vector<BYTE>& bytes,const PaintServiceData* data)
{
    if(!source.Valid()) return false;
    RasterImage image=source;
    int size=options.size;
    if(!size && (source.width>256 || source.height>256)) size=256;
    if(size)
    {
        if(size<1 || size>256) return false;
        double scale=min(double(size)/source.width,double(size)/source.height);
        RasterImage resized=ResizeRaster(source,max(1,int(source.width*scale+.5)),max(1,int(source.height*scale+.5)),2);
        if(!resized.Valid()) return false;
        image.width=image.height=size;image.pixels.assign(size_t(size)*size,0);
        for(int y=0;y<resized.height;++y) for(int x=0;x<resized.width;++x)
            image.pixels[size_t(y+(size-resized.height)/2)*size+x+(size-resized.width)/2]=resized.pixels[size_t(y)*resized.width+x];
    }
    IconFrame replacement;replacement.width=image.width;replacement.height=image.height;replacement.depth=32;
    if(image.width==256 || image.height==256 || (data && data->present))
    { if(!EncodePng(image,PngOptions{},replacement.encoded,96,96,true) || !SetPngPaintData(replacement.encoded,data)) return false; }
    else if(!EncodeIconDib(image,replacement.encoded)) return false;
    std::vector<IconFrame> frames;
    int selected=0;
    if(info && options.keepOtherImages && !info->icons.empty()) { frames=info->icons;selected=info->selectedIcon; }
    if(frames.empty()) frames.push_back(std::move(replacement));
    else
    {
        if(selected<0 || selected>=int(frames.size())) return false;
        frames[selected]=std::move(replacement);
    }
    // Retained frames keep their pixels, but an unchecked checkbox strips our metadata everywhere.
    if(!data) for(auto& frame:frames) if(frame.encoded.size()>=8 && !memcmp(frame.encoded.data(),"\x89PNG\r\n\x1a\n",8))
        if(!SetPngPaintData(frame.encoded,NULL)) return false;
    size_t offset=6+frames.size()*16,total=offset;for(const auto& frame:frames) total+=frame.encoded.size();
    if(total>MAXDWORD) return false;
    bytes.assign(total,0);W16(bytes,2,1);W16(bytes,4,unsigned(frames.size()));
    for(size_t i=0;i<frames.size();++i)
    {
        const auto& frame=frames[i];size_t entry=6+i*16;
        bytes[entry]=BYTE(frame.width);bytes[entry+1]=BYTE(frame.height);
        W16(bytes,entry+4,1);W16(bytes,entry+6,frame.depth);W32(bytes,entry+8,unsigned(frame.encoded.size()));W32(bytes,entry+12,unsigned(offset));
        memcpy(bytes.data()+offset,frame.encoded.data(),frame.encoded.size());offset+=frame.encoded.size();
    }
    return true;
}
