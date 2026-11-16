#include "precomp.h"
#include "imageformats.h"
#include "third_party/lodepng/lodepng.h"
namespace {
bool preserve=true;
DWORD U32(const BYTE* p) { return DWORD(p[0])|(DWORD(p[1])<<8)|(DWORD(p[2])<<16)|(DWORD(p[3])<<24); }
DWORD Big32(const BYTE* p) { return DWORD(p[3])|(DWORD(p[2])<<8)|(DWORD(p[1])<<16)|(DWORD(p[0])<<24); }
void Put32(std::vector<BYTE>& b,size_t p,DWORD value) { for(int i=0;i<4;++i) b[p+i]=BYTE(value>>(i*8)); }
void PutBig(std::vector<BYTE>& b,size_t p,DWORD value) { for(int i=0;i<4;++i) b[p+i]=BYTE(value>>((3-i)*8)); }
unsigned Rgb(COLORREF color) { return unsigned(GetRValue(color))<<16|unsigned(GetGValue(color))<<8|GetBValue(color); }
COLORREF Color(unsigned value) { return RGB(value>>16&255,value>>8&255,value&255); }
}
std::string SerializePaintData(const PaintServiceData& data)
{
    if(!data.present) return {};
    char value[80];sprintf_s(value,"ClassicPaintPlus/1;key=%d;rgb=%06X;matte=%06X",data.hasColor?1:0,Rgb(data.color),Rgb(data.backdrop));
    return value;
}
PaintServiceData ParsePaintData(const BYTE* bytes,size_t size)
{
    PaintServiceData result;
    const char* prefix="ClassicPaintPlus/1;key=";const size_t n=strlen(prefix);
    if(size!=n+1+5+6+7+6 || memcmp(bytes,prefix,n) || (bytes[n]!='0' && bytes[n]!='1') ||
       memcmp(bytes+n+1,";rgb=",5) || memcmp(bytes+n+12,";matte=",7)) return result;
    auto hex=[](const BYTE* p,unsigned& out) {
        out=0;for(int i=0;i<6;++i) { int c=p[i],v=c>='0' && c<='9'?c-'0':c>='A' && c<='F'?c-'A'+10:-1;if(v<0) return false;out=out*16+v; }return true;
    };
    unsigned color,matte;if(!hex(bytes+n+6,color) || !hex(bytes+n+19,matte)) return result;
    result.present=true;result.hasColor=bytes[n]=='1';result.color=Color(color);result.backdrop=Color(matte);return result;
}
PaintServiceData ReadPngPaintData(const std::vector<BYTE>& b)
{
    PaintServiceData result;if(b.size()<8 || memcmp(b.data(),"\x89PNG\r\n\x1a\n",8)) return result;
    for(size_t p=8;p+12<=b.size();)
    {
        size_t n=Big32(b.data()+p);if(n>b.size()-p-12) return {};
        if(!memcmp(b.data()+p+4,"cpPs",4))
        { if(!lodepng_chunk_check_crc(b.data()+p)) result=ParsePaintData(b.data()+p+8,n); }
        if(!memcmp(b.data()+p+4,"IEND",4)) break;p+=n+12;
    }
    return result;
}
bool SetPngPaintData(std::vector<BYTE>& b,const PaintServiceData* data)
{
    if(b.size()<8 || memcmp(b.data(),"\x89PNG\r\n\x1a\n",8)) return false;
    for(size_t p=8;p+12<=b.size();)
    {
        size_t n=Big32(b.data()+p);if(n>b.size()-p-12) return false;
        if(!memcmp(b.data()+p+4,"cpPs",4)) { b.erase(b.begin()+p,b.begin()+p+n+12);continue; }
        if(!memcmp(b.data()+p+4,"IEND",4))
        {
            if(data && data->present)
            {
                std::string value=SerializePaintData(*data);std::vector<BYTE> chunk(value.size()+12);
                PutBig(chunk,0,DWORD(value.size()));memcpy(chunk.data()+4,"cpPs",4);memcpy(chunk.data()+8,value.data(),value.size());
                lodepng_chunk_generate_crc(chunk.data());b.insert(b.begin()+p,chunk.begin(),chunk.end());
            }
            return true;
        }
        p+=n+12;
    }
    return false;
}
PaintServiceData ReadWebpPaintData(const std::vector<BYTE>& b)
{
    if(b.size()<12 || memcmp(b.data(),"RIFF",4) || memcmp(b.data()+8,"WEBP",4)) return {};
    size_t limit=size_t(U32(b.data()+4))+8;if(limit>b.size() || limit<12) return {};
    for(size_t p=12;p+8<=limit;)
    {
        size_t n=U32(b.data()+p+4);if(n>limit-p-8 || n+(n&1)>limit-p-8) return {};
        if(!memcmp(b.data()+p,"CPPA",4)) return ParsePaintData(b.data()+p+8,n);
        p+=8+n+(n&1);
    }
    return {};
}
bool SetWebpPaintData(std::vector<BYTE>& b,const PaintServiceData& data,int width,int height,bool alpha)
{
    if(b.size()<20 || b.size()>MAXDWORD-256 || memcmp(b.data(),"RIFF",4) || memcmp(b.data()+8,"WEBP",4)) return false;
    // Application chunks belong to the extended container. VP8X dimensions are 24-bit minus one.
    if(memcmp(b.data()+12,"VP8X",4))
    {
        std::vector<BYTE> header(18,0);memcpy(header.data(),"VP8X",4);Put32(header,4,10);header[8]=alpha?0x10:0;
        for(int i=0;i<3;++i) { header[12+i]=BYTE((width-1)>>(8*i));header[15+i]=BYTE((height-1)>>(8*i)); }
        b.insert(b.begin()+12,header.begin(),header.end());
    }
    std::string value=SerializePaintData(data);size_t p=b.size();b.resize(p+8+value.size()+(value.size()&1),0);
    memcpy(b.data()+p,"CPPA",4);Put32(b,p+4,DWORD(value.size()));memcpy(b.data()+p+8,value.data(),value.size());Put32(b,4,DWORD(b.size()-8));return true;
}
PaintServiceData ReadTgaPaintData(const std::vector<BYTE>& b)
{ return b.size()>=18 && b[0]<=b.size()-18?ParsePaintData(b.data()+18,b[0]):PaintServiceData{}; }
bool SetTgaPaintData(std::vector<BYTE>& b,const PaintServiceData& data)
{
    if(b.size()<44 || b[0] || memcmp(b.data()+b.size()-18,"TRUEVISION-XFILE.\0",18)) return false;
    std::string value=SerializePaintData(data);if(value.size()>255 || b.size()>MAXDWORD-value.size()) return false;
    b.insert(b.begin()+18,value.begin(),value.end());b[0]=BYTE(value.size());
    for(size_t p=b.size()-26;p<b.size()-18;p+=4) { DWORD offset=U32(b.data()+p);if(offset) Put32(b,p,offset+DWORD(value.size())); }
    return true;
}
bool SupportsPaintData(SaveFormat f)
{ return f==SaveFormat::Png || f==SaveFormat::WebP || f==SaveFormat::Tiff || f==SaveFormat::Tga || f==SaveFormat::Ico; }
bool CanSavePaintData() { return DocumentAlpha().enabled && DocumentAlpha().customized; }
PaintServiceData CurrentPaintData()
{
    PaintServiceData data;if(CanSavePaintData()) { data.present=true;data.hasColor=DocumentAlpha().hasColor;data.color=DocumentAlpha().color;data.backdrop=DocumentAlpha().backdrop; }return data;
}
bool PreservePaintData() { return preserve; }
void SetPreservePaintData(bool value) { preserve=value; }
