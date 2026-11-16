// DDS surfaces are expanded into the editor's unrestricted RGBA image.
// Saving supports lossless BGRA and BC1/BC2/BC3 (DXT1/DXT3/DXT5).
#include "precomp.h"
#include "imageformats.h"
#include "third_party/libsquish-1.15/squish.h"
#define BCDEC_BC4BC5_PRECISE
#define BCDEC_IMPLEMENTATION
#include "third_party/bcdec/bcdec.h"
namespace {
DWORD U32(const BYTE* p) { return p[0]|DWORD(p[1])<<8|DWORD(p[2])<<16|DWORD(p[3])<<24; }
constexpr DWORD Four(char a,char b,char c,char d) { return DWORD(a)|DWORD(b)<<8|DWORD(c)<<16|DWORD(d)<<24; }
void Put(std::vector<BYTE>& b,size_t at,DWORD value) { for(int i=0;i<4;++i) b[at+i]=BYTE(value>>(8*i)); }
unsigned Channel(DWORD value,DWORD mask,unsigned absent)
{
    if(!mask) return absent;
    unsigned shift=0;while(!(mask&1)) { mask>>=1;++shift; }
    return unsigned((ULONGLONG((value>>shift)&mask)*255+mask/2)/mask);
}
bool ValidMask(DWORD mask,unsigned bits)
{
    if(!mask) return true;if(bits<32 && (mask>>bits)) return false;
    while(!(mask&1)) mask>>=1;return (mask&(mask+1))==0;
}
BYTE Normal(float f,bool signedValue=false)
{
    if(signedValue) f=(f+1)*.5f;
    if(!_finite(f) || f<=0) return 0;if(f>=1) return 255;return BYTE(f*255+.5f);
}
bool EncodeCompressedDds(const RasterImage& image,int compression,std::vector<BYTE>& bytes)
{
    unsigned columns=(image.width+3)/4,rows=(image.height+3)/4,blockSize=compression==1?8:16;
    size_t dataSize=size_t(columns)*rows*blockSize;
    if(dataSize>0xffffffffu-128) return false;
    bytes.assign(128+dataSize,0);memcpy(bytes.data(),"DDS ",4);
    Put(bytes,4,124);Put(bytes,8,0x81007);Put(bytes,12,image.height);Put(bytes,16,image.width);Put(bytes,20,DWORD(dataSize));
    Put(bytes,76,32);Put(bytes,80,4);Put(bytes,84,Four('D','X','T',compression==1?'1':compression==2?'3':'5'));Put(bytes,108,0x1000);
    int flags=(compression==1?squish::kDxt1:compression==2?squish::kDxt3:squish::kDxt5)|squish::kColourClusterFit|squish::kWeightColourByAlpha;
    for(unsigned by=0;by<rows;++by) for(unsigned bx=0;bx<columns;++bx)
    {
        BYTE rgba[64]={};int mask=0;
        // Mask partial edge blocks instead of padding the document or changing its size.
        for(unsigned y=0;y<4 && by*4+y<image.height;++y) for(unsigned x=0;x<4 && bx*4+x<image.width;++x)
        {
            unsigned index=y*4+x;DWORD pixel=image.pixels[size_t(by*4+y)*image.width+bx*4+x];
            rgba[index*4]=BYTE(pixel>>16);rgba[index*4+1]=BYTE(pixel>>8);rgba[index*4+2]=BYTE(pixel);rgba[index*4+3]=BYTE(pixel>>24);
            mask|=1<<index;
        }
        squish::CompressMasked(rgba,mask,bytes.data()+128+(size_t(by)*columns+bx)*blockSize,flags);
    }
    return true;
}
}
bool DecodeDds(const std::vector<BYTE>& bytes,RasterImage& image,ImageFileInfo* info)
{
    if(bytes.size()<128 || memcmp(bytes.data(),"DDS ",4) || U32(bytes.data()+4)!=124 || U32(bytes.data()+76)!=32) return false;
    auto read=[&](size_t offset){return U32(bytes.data()+offset);};
    DWORD w=read(16),h=read(12),flags=read(8),pitch=read(20),pf=read(80),code=read(84),bits=read(88);
    if(!w || !h || w>INT_MAX || h>INT_MAX || ULONGLONG(w)*h>100000000) return false;
    DWORD rm=read(92),gm=read(96),bm=read(100),am=(pf&1)?read(104):0;
    size_t offset=128;int bc=0;bool signedBC=false,premult=false,opaque=false,luminance=(pf&0x20000)!=0;
    bool multiple=read(28)>1 || read(24)>1 || (read(112)&0x200)!=0;
    if(pf&4)
    {
        switch(code)
        {
            case Four('D','X','T','1'):bc=1;break;
            case Four('D','X','T','2'):bc=2;premult=true;break;
            case Four('D','X','T','3'):bc=2;break;
            case Four('D','X','T','4'):bc=3;premult=true;break;
            case Four('D','X','T','5'):bc=3;break;
            case Four('A','T','I','1'):case Four('B','C','4','U'):bc=4;break;
            case Four('B','C','4','S'):bc=4;signedBC=true;break;
            case Four('A','T','I','2'):case Four('B','C','5','U'):bc=5;break;
            case Four('B','C','5','S'):bc=5;signedBC=true;break;
            case Four('D','X','1','0'):
            {
                // Some DDS writers use arraySize=0 for an ordinary surface.
                // Treat it as one; the pixel buffer is still bounds-checked.
                if(bytes.size()<148) return false;
                DWORD format=read(128),dimension=read(132),alpha=read(144)&7;
                if(dimension<2 || dimension>4 || (dimension==2 && h!=1)) return false;
                multiple=multiple || read(140)>1 || (read(136)&4)!=0 || dimension==4;
                offset=148;premult=alpha==2;opaque=alpha==3;rm=gm=bm=am=0;luminance=false;
                switch(format)
                {
                    case 70:case 71:case 72:bc=1;break;
                    case 73:case 74:case 75:bc=2;break;
                    case 76:case 77:case 78:bc=3;break;
                    case 79:case 80:case 81:bc=4;signedBC=format==81;break;
                    case 82:case 83:case 84:bc=5;signedBC=format==84;break;
                    case 94:case 95:case 96:bc=6;signedBC=format==96;break;
                    case 97:case 98:case 99:bc=7;break;
                    case 27:case 28:case 29:bits=32;rm=0xff;gm=0xff00;bm=0xff0000;am=0xff000000;break;
                    case 87:case 90:case 91:bits=32;rm=0xff0000;gm=0xff00;bm=0xff;am=0xff000000;break;
                    case 88:case 92:case 93:bits=32;rm=0xff0000;gm=0xff00;bm=0xff;break;
                    case 24:bits=32;rm=0x3ff;gm=0xffc00;bm=0x3ff00000;am=0xc0000000;break;
                    case 85:bits=16;rm=0xf800;gm=0x7e0;bm=0x1f;break;
                    case 86:bits=16;rm=0x7c00;gm=0x3e0;bm=0x1f;am=0x8000;break;
                    case 115:bits=16;rm=0xf00;gm=0xf0;bm=0xf;am=0xf000;break;
                    case 49:bits=16;rm=0xff;gm=0xff00;break;
                    case 61:bits=8;rm=0xff;break;
                    case 65:bits=8;am=0xff;break;
                    default:return false;
                }
                break;
            }
            default:return false;
        }
    }
    else if(!(pf&(0x40|0x20000|2))) return false;
    if(pf==2 && !(pf&4)) am=bits==8?255:am;
    RasterImage out;out.width=w;out.height=h;
    if(bc)
    {
        unsigned blockSize=(bc==1 || bc==4)?8:16,columns=(w+3)/4,rows=(h+3)/4;
        if(size_t(columns)*rows*blockSize>bytes.size()-offset) return false;
        out.pixels.resize(size_t(w)*h);
        for(unsigned by=0;by<rows;++by) for(unsigned bx=0;bx<columns;++bx)
        {
            ULONGLONG block[2]={};memcpy(block,bytes.data()+offset+(size_t(by)*columns+bx)*blockSize,blockSize);
            BYTE rgba[64]={};float values[48]={};
            switch(bc)
            {
                case 1:bcdec_bc1(block,rgba,16);break;case 2:bcdec_bc2(block,rgba,16);break;
                case 3:bcdec_bc3(block,rgba,16);break;case 4:bcdec_bc4_float(block,values,4,signedBC);break;
                case 5:bcdec_bc5_float(block,values,8,signedBC);break;case 6:bcdec_bc6h_float(block,values,12,signedBC);break;
                case 7:bcdec_bc7(block,rgba,16);break;
            }
            for(unsigned y=0;y<4 && by*4+y<h;++y) for(unsigned x=0;x<4 && bx*4+x<w;++x)
            {
                unsigned p=y*4+x;DWORD value;
                if(bc==4) value=0xff000000|DWORD(Normal(values[p],signedBC))<<16;
                else if(bc==5) value=0xff000000|DWORD(Normal(values[p*2],signedBC))<<16|DWORD(Normal(values[p*2+1],signedBC))<<8;
                else if(bc==6) value=0xff000000|DWORD(Normal(values[p*3]))<<16|DWORD(Normal(values[p*3+1]))<<8|Normal(values[p*3+2]);
                else value=DWORD(rgba[p*4+3])<<24|DWORD(rgba[p*4])<<16|DWORD(rgba[p*4+1])<<8|rgba[p*4+2];
                out.pixels[size_t(by*4+y)*w+bx*4+x]=value;
            }
        }
    }
    else
    {
        if(bits!=8 && bits!=16 && bits!=24 && bits!=32) return false;
        if(luminance) gm=bm=0;
        if(!ValidMask(rm,bits) || !ValidMask(gm,bits) || !ValidMask(bm,bits) || !ValidMask(am,bits) ||
           (rm&gm) || (rm&bm) || (rm&am) || (gm&bm) || (gm&am) || (bm&am)) return false;
        size_t stride=size_t(w)*(bits/8);if((flags&8) && pitch) { if(pitch<stride) return false;stride=pitch; }
        if(stride> (bytes.size()-offset)/h) return false;
        out.pixels.resize(size_t(w)*h);
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x)
        {
            const BYTE* p=bytes.data()+offset+y*stride+x*(bits/8);DWORD value=0;
            for(unsigned c=0;c<bits/8;++c) value|=DWORD(p[c])<<(c*8);
            unsigned r=Channel(value,rm,0),g=luminance?r:Channel(value,gm,0),b=luminance?r:Channel(value,bm,0);
            out.pixels[size_t(y)*w+x]=Channel(value,am,255)<<24|r<<16|g<<8|b;
        }
    }
    if(premult || opaque) for(DWORD& value:out.pixels)
    {
        if(opaque) value|=0xff000000;
        else { unsigned a=value>>24;DWORD straight=a<<24;if(a) for(int c=0;c<3;++c) straight|=min(255u,((value>>(c*8)&255)*255+a/2)/a)<<(c*8);value=straight; }
    }
    image=std::move(out);if(info) { info->format=SaveFormat::Dds;info->multipleImages=multiple;info->ddsCompression=bc>=1 && bc<=3?bc:0; }return true;
}
bool EncodeDds(const RasterImage& image,std::vector<BYTE>& bytes,const DdsOptions& options)
{
    if(!image.Valid() || image.pixels.size()>0x3fffffdf || options.compression<0 || options.compression>3) return false;
    if(options.compression) return EncodeCompressedDds(image,options.compression,bytes);
    bytes.assign(128+image.pixels.size()*4,0);memcpy(bytes.data(),"DDS ",4);
    Put(bytes,4,124);Put(bytes,8,0x100f);Put(bytes,12,image.height);Put(bytes,16,image.width);Put(bytes,20,image.width*4);
    Put(bytes,76,32);Put(bytes,80,0x41);Put(bytes,88,32);
    Put(bytes,92,0xff0000);Put(bytes,96,0xff00);Put(bytes,100,0xff);Put(bytes,104,0xff000000);Put(bytes,108,0x1000);
    memcpy(bytes.data()+128,image.pixels.data(),image.pixels.size()*4);return true;
}
