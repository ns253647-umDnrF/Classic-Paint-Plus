// JPEG encoding and bounded EXIF thumbnail updates. LGPL-2.0-or-later.
#include <vector>
#include <memory>
#include <new>
#include <stdio.h>
#include <setjmp.h>
#include "precomp.h"
#include "jpeg.h"
extern "C" {
#include <jpeglib.h>
}

namespace {
JpegMetadata documentMetadata;
bool JfifThumbnail(const JpegMarker& marker)
{
    const auto& d=marker.data;if(marker.code!=0xe0) return false;
    if(d.size()>=14 && !memcmp(d.data(),"JFIF\0",5))
        return d[12] && d[13] && d.size()>=14+size_t(d[12])*d[13]*3;
    if(d.size()<8 || memcmp(d.data(),"JFXX\0",5)) return false;
    if(d[5]==0x10) return d[6]==0xff && d[7]==0xd8;
    if(!d[6] || !d[7]) return false;
    return (d[5]==0x11 && d.size()>=8+768+size_t(d[6])*d[7]) ||
        (d[5]==0x13 && d.size()>=8+size_t(d[6])*d[7]*3);
}
struct Codec
{
    jpeg_compress_struct c={};
    struct Error { jpeg_error_mgr base; jmp_buf jump; } error;
    unsigned char* output=nullptr;
    unsigned long size=0;
    ~Codec() { if(c.mem) jpeg_destroy_compress(&c);free(output); }
};
void JpegErrorExit(j_common_ptr c)
{ longjmp(reinterpret_cast<Codec::Error*>(c->err)->jump,1); }

bool Encode(const BYTE* rgb,int width,int height,const JpegOptions& o,
            const JpegMetadata& markers,float xDpi,float yDpi,std::vector<BYTE>& result)
{
    std::unique_ptr<Codec> state(new Codec);
    auto& c=state->c;c.err=jpeg_std_error(&state->error.base);
    c.err->error_exit=JpegErrorExit;
    if(setjmp(state->error.jump)) return false;
    jpeg_create_compress(&c);jpeg_mem_dest(&c,&state->output,&state->size);
    c.image_width=width;c.image_height=height;c.input_components=3;c.in_color_space=JCS_RGB;
    jpeg_set_defaults(&c);jpeg_set_quality(&c,max(0,min(100,o.quality)),TRUE);
    c.comp_info[0].h_samp_factor=o.horizontal?2:1;
    c.comp_info[0].v_samp_factor=o.vertical?2:1;
    c.optimize_coding=o.optimize?TRUE:FALSE;
    if(o.progressive) jpeg_simple_progression(&c);
    c.density_unit=1;c.X_density=WORD(max(1,min(65535,xDpi+.5f)));
    c.Y_density=WORD(max(1,min(65535,yDpi+.5f)));
    for(const auto& marker:markers)
        if(marker.code==0xe0 && marker.data.size()>=14 && !memcmp(marker.data.data(),"JFIF\0",5))
            c.write_JFIF_header=FALSE;
    jpeg_start_compress(&c,TRUE);
    for(const auto& marker:markers)
    {
        if(marker.data.size()>65533) continue;
        jpeg_write_marker(&c,marker.code,marker.data.data(),unsigned(marker.data.size()));
    }
    while(c.next_scanline<c.image_height)
    {
        JSAMPROW row=const_cast<BYTE*>(rgb+size_t(c.next_scanline)*width*3);
        jpeg_write_scanlines(&c,&row,1);
    }
    jpeg_finish_compress(&c);result.assign(state->output,state->output+state->size);return true;
}

bool ReadBitmap(HBITMAP bitmap,std::vector<BYTE>& rgb,int& width,int& height)
{
    RasterImage image;if(!ReadRaster(bitmap,image)) return false;
    width=image.width;height=image.height;FlattenRaster(image,DocumentBackdrop());
    if(width>JPEG_MAX_DIMENSION || height>JPEG_MAX_DIMENSION) return false;
    rgb.resize(image.pixels.size()*3);
    for(size_t i=0;i<image.pixels.size();++i)
    { DWORD p=image.pixels[i];rgb[i*3]=BYTE(p>>16);rgb[i*3+1]=BYTE(p>>8);rgb[i*3+2]=BYTE(p); }
    return true;
}

// All offsets are relative to the TIFF header, not the APP1 signature.
struct Exif
{
    std::vector<BYTE>& bytes;
    bool little=false,valid=false;
    size_t first=0,next=0;
    bool fits(size_t p,size_t n) const { return p<=bytes.size() && n<=bytes.size()-p; }
    unsigned read(size_t p,int n) const
    {
        if(!fits(p,n)) return 0;
        unsigned value=0;
        for(int i=0;i<n;++i) value|=unsigned(bytes[p+i])<<(8*(little?i:n-1-i));
        return value;
    }
    void put(size_t p,unsigned value,int n)
    { if(fits(p,n)) for(int i=0;i<n;++i) bytes[p+i]=BYTE(value>>(8*(little?i:n-1-i))); }
    size_t ifdEnd(size_t p) const
    {
        if(p<14 || !fits(p,2)) return 0;
        unsigned count=read(p,2);size_t end=p+2+size_t(count)*12;
        return count<=4096 && fits(end,4)?end:0;
    }
    size_t find(size_t ifd,unsigned tag) const
    {
        size_t end=ifdEnd(ifd);if(!end) return 0;
        for(size_t p=ifd+2;p<end;p+=12) if(read(p,2)==tag) return p;
        return 0;
    }
    unsigned scalar(size_t ifd,unsigned tag,unsigned fallback=0) const
    {
        size_t p=find(ifd,tag);if(!p || read(p+4,4)!=1) return fallback;
        unsigned type=read(p+2,2);return type==3?read(p+8,2):type==4?read(p+8,4):fallback;
    }
    void setScalar(size_t ifd,unsigned tag,unsigned value)
    {
        size_t p=find(ifd,tag);if(!p || read(p+4,4)!=1) return;
        unsigned type=read(p+2,2);if(type==3) put(p+8,min(value,65535u),2);else if(type==4) put(p+8,value,4);
    }
    explicit Exif(std::vector<BYTE>& data):bytes(data)
    {
        if(!fits(0,14) || memcmp(bytes.data(),"Exif\0\0",6)) return;
        little=bytes[6]=='I' && bytes[7]=='I';
        if(!little && !(bytes[6]=='M' && bytes[7]=='M')) return;
        if(read(8,2)!=42) return;
        first=size_t(read(10,4))+6;next=ifdEnd(first);valid=next!=0;
    }
    bool hasThumbnail() const
    {
        if(!valid || !read(next,4)) return false;
        size_t ifd=size_t(read(next,4))+6;
        // JPEG-compressed and uncompressed TIFF thumbnails both count.
        return ifdEnd(ifd) && (scalar(ifd,0x201)!=0 || find(ifd,0x111)!=0);
    }
    void dimensions(int width,int height)
    {
        if(!valid) return;
        setScalar(first,0x100,width);setScalar(first,0x101,height);
        size_t sub=size_t(scalar(first,0x8769))+6;
        setScalar(sub,0xA002,width);setScalar(sub,0xA003,height);
    }
    bool thumbnail(const std::vector<BYTE>& jpeg,int width,int height)
    {
        if(!valid) return false;
        // Old JPEG bytes are no longer useful. Remove a terminal thumbnail or
        // zero its range without moving any MakerNote / GPS / other TIFF offsets.
        if(read(next,4))
        {
            size_t oldIfd=size_t(read(next,4))+6;
            size_t pos=size_t(scalar(oldIfd,0x201))+6,len=scalar(oldIfd,0x202);
            if(pos>=14 && len && fits(pos,len) && pos>next+4 && pos>ifdEnd(oldIfd))
            {
                if(pos+len==bytes.size())
                {
                    // Reuse the compact IFD1 layout we generated previously.
                    // Repeated Save must not grow APP1 until it hits 64 KiB.
                    size_t x=find(oldIfd,0x11A),y=find(oldIfd,0x11B);
                    bool ours=read(oldIfd,2)==10 && pos==oldIfd+142 && x && y &&
                        read(x+8,4)+6==oldIfd+126 && read(y+8,4)+6==oldIfd+134;
                    bytes.resize(ours?oldIfd:pos);
                }
                else memset(bytes.data()+pos,0,len);
            }
        }
        if(bytes.size()&1) bytes.push_back(0);
        size_t ifd=bytes.size(),rational=ifd+2+10*12+4,data=rational+16;
        if(data+jpeg.size()>65533) return false;
        bytes.resize(data+jpeg.size(),0);put(next,unsigned(ifd-6),4);put(ifd,10,2);
        auto entry=[&](int i,unsigned tag,unsigned type,unsigned count,unsigned value)
        {
            size_t p=ifd+2+i*12;put(p,tag,2);put(p+2,type,2);put(p+4,count,4);
            put(p+8,value,type==3 && count==1?2:4);
        };
        entry(0,0x100,4,1,width);entry(1,0x101,4,1,height);entry(2,0x103,3,1,6);
        entry(3,0x112,3,1,scalar(first,0x112,1));
        entry(4,0x11A,5,1,unsigned(rational-6));entry(5,0x11B,5,1,unsigned(rational+8-6));
        entry(6,0x128,3,1,2);entry(7,0x201,4,1,unsigned(data-6));
        entry(8,0x202,4,1,unsigned(jpeg.size()));entry(9,0x213,3,1,1);
        put(rational,72,4);put(rational+4,1,4);put(rational+8,72,4);put(rational+12,1,4);
        memcpy(bytes.data()+data,jpeg.data(),jpeg.size());return true;
    }
};

std::vector<BYTE> NewExif(int width,int height,float xDpi,float yDpi)
{
    std::vector<BYTE> data(186,0);
    memcpy(data.data(),"Exif\0\0II\x2a\0\x08\0\0\0",14);
    Exif t(data);t.put(14,6,2);
    auto entry=[&](size_t p,unsigned tag,unsigned type,unsigned count,unsigned value)
    { t.put(p,tag,2);t.put(p+2,type,2);t.put(p+4,count,4);t.put(p+8,value,type==3 && count==1?2:4); };
    entry(16,0x112,3,1,1);entry(28,0x11A,5,1,164);entry(40,0x11B,5,1,172);
    entry(52,0x128,3,1,2);entry(64,0x213,3,1,1);entry(76,0x8769,4,1,86);
    t.put(92,6,2);
    entry(94,0x9000,7,4,0x31333230); // Exif 0231
    entry(106,0x9101,7,4,0x00030201); // Y, Cb, Cr
    entry(118,0xA000,7,4,0x30303130); // Flashpix 0100
    entry(130,0xA001,3,1,1);entry(142,0xA002,4,1,width);entry(154,0xA003,4,1,height);
    t.put(170,unsigned(max(1,min(65535,xDpi+.5f))),4);t.put(174,1,4);
    t.put(178,unsigned(max(1,min(65535,yDpi+.5f))),4);t.put(182,1,4);
    return data;
}

bool MakeThumbnail(const std::vector<BYTE>& rgb,int width,int height,bool dcf,
                   std::vector<BYTE>& jpeg,int& tw,int& th)
{
    double scale=min(160.0/width,120.0/height);
    int w=max(1,int(width*scale+.5)),h=max(1,int(height*scale+.5));
    tw=dcf?160:w;th=dcf?120:h;
    std::vector<BYTE> pixels(size_t(tw)*th*3,0);
    // Area filtering is stable for both photographs and fine drawing lines.
    // Each thumbnail pixel integrates a rectangular region in the full image.
    for(int y=0;y<h;++y) for(int x=0;x<w;++x)
    {
        double left=double(x)*width/w,right=double(x+1)*width/w;
        double top=double(y)*height/h,bottom=double(y+1)*height/h;
        double sum[3]={},area=(right-left)*(bottom-top);
        for(int sy=int(top);sy<min(height,int(ceil(bottom)));++sy)
            for(int sx=int(left);sx<min(width,int(ceil(right)));++sx)
            {
                double weight=(min(right,sx+1.0)-max(left,double(sx)))*(min(bottom,sy+1.0)-max(top,double(sy)));
                const BYTE* p=&rgb[(size_t(sy)*width+sx)*3];for(int c=0;c<3;++c) sum[c]+=p[c]*weight;
            }
        BYTE* p=&pixels[(size_t(y+(th-h)/2)*tw+x+(tw-w)/2)*3];
        for(int c=0;c<3;++c) p[c]=BYTE(max(0,min(255,int(sum[c]/area+.5))));
    }
    JpegOptions options;options.quality=80;options.horizontal=true;options.vertical=false;
    return Encode(pixels.data(),tw,th,options,{},72,72,jpeg);
}

bool WriteAtomic(LPCWSTR path,const std::vector<BYTE>& data)
{
    CStringW temporary;HANDLE file=INVALID_HANDLE_VALUE;
    for(unsigned attempt=0;attempt<32 && file==INVALID_HANDLE_VALUE;++attempt)
    {
        temporary.Format(L"%s.paint-%lu-%lu-%u.tmp",path,GetCurrentProcessId(),GetTickCount(),attempt);
        file=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(file==INVALID_HANDLE_VALUE && GetLastError()!=ERROR_FILE_EXISTS) return false;
    }
    if(file==INVALID_HANDLE_VALUE) return false;
    size_t pos=0;bool ok=true;
    while(pos<data.size())
    {
        DWORD written=0,chunk=DWORD(min(size_t(1<<20),data.size()-pos));
        if(!WriteFile(file,data.data()+pos,chunk,&written,NULL) || written!=chunk) { ok=false;break; }
        pos+=written;
    }
    if(ok) ok=FlushFileBuffers(file)!=FALSE;
    if(!CloseHandle(file)) ok=false;
    if(ok) ok=MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
    if(!ok) DeleteFileW(temporary);
    return ok;
}
}

bool IsJpegFile(LPCWSTR name,REFGUID format)
{
    if(format!=GUID_NULL) return format==Gdiplus::ImageFormatJPEG;
    LPCWSTR ext=PathFindExtensionW(name);
    return !lstrcmpiW(ext,L".jpg") || !lstrcmpiW(ext,L".jpeg") || !lstrcmpiW(ext,L".jpe") || !lstrcmpiW(ext,L".jfif");
}

JpegMetadata ReadJpegMetadata(LPCWSTR name)
{
    JpegMetadata markers;if(!name || !*name) return markers;
    FILE* raw=_wfopen(name,L"rb");if(!raw) return markers;
    std::unique_ptr<FILE,decltype(&fclose)> file(raw,fclose);
    if(fgetc(raw)!=0xff || fgetc(raw)!=0xd8) return markers;
    size_t total=0;
    while(total<16*1024*1024)
    {
        if(fgetc(raw)!=0xff) break;
        int code;do { code=fgetc(raw); } while(code==0xff);
        if(code<0 || code==0xda || code==0xd9) break;
        if(code==0x01 || (code>=0xd0 && code<=0xd7)) continue;
        int hi=fgetc(raw),lo=fgetc(raw);if(hi<0 || lo<0) break;
        int size=(hi<<8)+lo-2;if(size<0) break;
        std::vector<BYTE> payload(size);
        if(fread(payload.data(),1,size,raw)!=size) break;
        total+=size+4;
        // APP14/Adobe describes the source color encoding. Only keep APP0 if
        // it actually carries a thumbnail (JFIF or one of the JFXX formats).
        if((code>=0xe1 && code<=0xef && code!=0xee) || code==0xfe)
            markers.push_back({code,std::move(payload)});
        else if(code==0xe0)
        { JpegMarker marker={code,std::move(payload)};if(JfifThumbnail(marker)) markers.push_back(std::move(marker)); }
    }
    return markers;
}
void SetDocumentJpegMetadata(LPCWSTR name) { documentMetadata=ReadJpegMetadata(name); }

bool SaveJpegBitmap(HBITMAP bitmap,LPCWSTR path,const JpegOptions& options,
                    const JpegMetadata& metadata,float xDpi,float yDpi)
{
    try
    {
        std::vector<BYTE> rgb;int width,height;
        if(!ReadBitmap(bitmap,rgb,width,height)) return false;
        JpegMetadata markers=options.preserveMetadata?metadata:JpegMetadata{};
        // The canvas is RGB. A CMYK/gray source ICC profile cannot describe
        // the converted pixels; copying it would give other viewers wrong colors.
        bool incompatibleProfile=false;
        for(const auto& marker:markers)
            if(marker.code==0xe2 && marker.data.size()>=34 && !memcmp(marker.data.data(),"ICC_PROFILE\0",12) &&
                marker.data[12]==1 && memcmp(marker.data.data()+30,"RGB ",4)) incompatibleProfile=true;
        if(incompatibleProfile)
            for(auto it=markers.begin();it!=markers.end();)
            {
                if(it->code==0xe2 && it->data.size()>=12 && !memcmp(it->data.data(),"ICC_PROFILE\0",12)) it=markers.erase(it);
                else ++it;
            }
        bool existing=false;for(auto marker:metadata)
        { Exif exif(marker.data);if(exif.hasThumbnail() || JfifThumbnail(marker)) existing=true; }
        bool thumbnail=options.thumbnails==1 || (options.thumbnails==0 && existing);
        for(auto it=markers.begin();it!=markers.end();)
        {
            if(it->code==0xe0 && thumbnail) { it=markers.erase(it);continue; }
            if(it->code==0xe0 && it->data.size()>=14 && !memcmp(it->data.data(),"JFIF\0",5))
            {
                auto& d=it->data;WORD x=WORD(max(1,min(65535,xDpi+.5f))),y=WORD(max(1,min(65535,yDpi+.5f)));
                d[7]=1;d[8]=BYTE(x>>8);d[9]=BYTE(x);d[10]=BYTE(y>>8);d[11]=BYTE(y);
            }
            ++it;
        }
        std::vector<BYTE> thumb;int tw=0,th=0;
        if(thumbnail && !MakeThumbnail(rgb,width,height,options.dcf,thumb,tw,th)) return false;
        bool attached=false;
        for(auto& marker:markers)
        {
            if(marker.code!=0xe1) continue;
            Exif exif(marker.data);if(!exif.valid) continue;
            exif.dimensions(width,height);
            if(thumbnail && !attached)
            { if(!exif.thumbnail(thumb,tw,th)) return false;attached=true; }
        }
        if(thumbnail && !attached)
        {
            JpegMarker marker={0xe1,NewExif(width,height,xDpi,yDpi)};Exif exif(marker.data);
            if(!exif.thumbnail(thumb,tw,th)) return false;markers.insert(markers.begin(),std::move(marker));
        }
        std::vector<BYTE> encoded;
        return Encode(rgb.data(),width,height,options,markers,xDpi,yDpi,encoded) && WriteAtomic(path,encoded);
    }
    catch(const std::bad_alloc&) { return false; }
}
bool SaveDocumentJpeg(HBITMAP bitmap,LPCWSTR path,bool mainFile)
{
    return SaveJpegBitmap(bitmap,path,CurrentJpegOptions(),mainFile?documentMetadata:JpegMetadata{},g_xDpi,g_yDpi);
}
