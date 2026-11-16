#include <vector>
#include <new>
#include "precomp.h"
#include "imageformats.h"
#include "third_party/lodepng/lodepng.h"
#include <webp/decode.h>
#include <webp/encode.h>
#include <webp/demux.h>

namespace {
COLORREF ReadPngBackdrop(const LodePNGInfo& info)
{
    if(!info.background_defined) return CLR_INVALID;
    if(info.color.colortype==LCT_PALETTE)
    {
        if(!info.color.palette || info.background_r>=info.color.palettesize) return CLR_INVALID;
        const BYTE* color=info.color.palette+4*info.background_r;
        return RGB(color[0],color[1],color[2]);
    }
    unsigned depth=info.color.bitdepth;
    if(depth!=1 && depth!=2 && depth!=4 && depth!=8 && depth!=16) return CLR_INVALID;
    unsigned limit=(1u<<depth)-1,r=info.background_r,g=info.background_g,b=info.background_b;
    if(info.color.colortype==LCT_GREY || info.color.colortype==LCT_GREY_ALPHA) g=b=r;
    if(r>limit || g>limit || b>limit) return CLR_INVALID;
    // Match the decoder's conversion to 8-bit document pixels.
    auto channel=[depth,limit](unsigned value) { return depth==16?value>>8:value*255/limit; };
    return RGB(channel(r),channel(g),channel(b));
}
void SetPngDensity(lodepng::State& state,float xDpi,float yDpi)
{
    state.info_png.phys_defined=1;state.info_png.phys_unit=1;
    state.info_png.phys_x=unsigned(max(1,min(1000000,xDpi/0.0254f+.5f)));
    state.info_png.phys_y=unsigned(max(1,min(1000000,yDpi/0.0254f+.5f)));
}
std::vector<BYTE> RgbaBytes(const RasterImage& image)
{
    std::vector<BYTE> bytes(image.pixels.size()*4);
    for(size_t i=0;i<image.pixels.size();++i)
    {
        DWORD p=image.pixels[i];bytes[i*4]=BYTE(p>>16);bytes[i*4+1]=BYTE(p>>8);
        bytes[i*4+2]=BYTE(p);bytes[i*4+3]=BYTE(p>>24);
    }
    return bytes;
}
void Put16(std::vector<BYTE>& b,size_t p,unsigned value) { b[p]=BYTE(value);b[p+1]=BYTE(value>>8); }
void Put32(std::vector<BYTE>& b,size_t p,unsigned value) { Put16(b,p,value);Put16(b,p+2,value>>16); }
}
bool DecodePng(const std::vector<BYTE>& bytes,RasterImage& image,ImageFileInfo* info)
{
    lodepng::State state;unsigned w=0,h=0;
    if(lodepng_inspect(&w,&h,&state,bytes.data(),bytes.size()) || !w || !h || w>32767 || h>32767 || size_t(w)*h>INT_MAX/4) return false;
    state.decoder.zlibsettings.max_output_size=size_t(w)*h*8+size_t(h)*8+64;
    std::vector<BYTE> rgba;
    if(lodepng::decode(rgba,w,h,state,bytes)) return false;
    image.width=int(w);image.height=int(h);image.pixels.resize(size_t(w)*h);
    for(size_t i=0;i<image.pixels.size();++i)
        image.pixels[i]=DWORD(rgba[i*4+3])<<24|DWORD(rgba[i*4])<<16|DWORD(rgba[i*4+1])<<8|rgba[i*4+2];
    if(info)
    {
        info->format=SaveFormat::Png;info->paintData=ReadPngPaintData(bytes);
        info->pngBackdrop=ReadPngBackdrop(state.info_png);
        if(state.info_png.phys_defined && state.info_png.phys_unit==1)
        { info->xDpi=state.info_png.phys_x*.0254f;info->yDpi=state.info_png.phys_y*.0254f; }
    }
    return true;
}
bool EncodePng(const RasterImage& image,const PngOptions& options,std::vector<BYTE>& bytes,float xDpi,float yDpi,bool forceRgba,COLORREF backdrop)
{
    if(!image.Valid()) return false;
    lodepng::State state;state.info_png.interlace_method=options.interlaced?1:0;SetPngDensity(state,xDpi,yDpi);
    if(forceRgba) { state.encoder.auto_convert=0;state.info_png.color.colortype=LCT_RGBA;state.info_png.color.bitdepth=8; }
    if(backdrop!=CLR_INVALID)
    { state.info_png.background_defined=1;state.info_png.background_r=GetRValue(backdrop);state.info_png.background_g=GetGValue(backdrop);state.info_png.background_b=GetBValue(backdrop); }
    state.encoder.zlibsettings.windowsize=32768;state.encoder.zlibsettings.nicematch=258;
    auto rgba=RgbaBytes(image);std::vector<BYTE> lossless;
    if(lodepng::encode(lossless,rgba,image.width,image.height,state)) return false;
    if(options.optimize && !forceRgba)
    {
        QuantizedImage quant=QuantizeRaster(image,256);
        int backgroundIndex=-1;
        if(backdrop!=CLR_INVALID)
        {
            DWORD rgb=RGB(GetBValue(backdrop),GetGValue(backdrop),GetRValue(backdrop));
            for(size_t i=0;i<quant.palette.size();++i)
            { DWORD& color=quant.palette[i];if(!(color>>24)) color=rgb;if((color&0xffffff)==rgb) backgroundIndex=int(i); }
            if(backgroundIndex<0 && quant.palette.size()<256) { backgroundIndex=int(quant.palette.size());quant.palette.push_back(0xff000000|rgb); }
        }
        // A difficult photograph falls back to lossless instead of forcing
        // visibly poor colors into a 256-entry palette.
        if(!quant.palette.empty() && quant.rms<=5.0 && (backdrop==CLR_INVALID || backgroundIndex>=0))
        {
            lodepng::State indexed;indexed.info_png.interlace_method=options.interlaced?1:0;
            indexed.info_raw.colortype=indexed.info_png.color.colortype=LCT_PALETTE;
            indexed.info_raw.bitdepth=indexed.info_png.color.bitdepth=8;indexed.encoder.auto_convert=0;
            indexed.encoder.filter_palette_zero=0;indexed.encoder.filter_strategy=LFS_MINSUM;
            indexed.encoder.zlibsettings.windowsize=32768;indexed.encoder.zlibsettings.nicematch=258;
            SetPngDensity(indexed,xDpi,yDpi);
            if(backdrop!=CLR_INVALID) { indexed.info_png.background_defined=1;indexed.info_png.background_r=unsigned(backgroundIndex); }
            for(DWORD color:quant.palette)
            {
                lodepng_palette_add(&indexed.info_raw,BYTE(color>>16),BYTE(color>>8),BYTE(color),BYTE(color>>24));
                lodepng_palette_add(&indexed.info_png.color,BYTE(color>>16),BYTE(color>>8),BYTE(color),BYTE(color>>24));
            }
            std::vector<BYTE> compressed;
            if(!lodepng::encode(compressed,quant.indices,image.width,image.height,indexed) && compressed.size()<lossless.size())
                lossless.swap(compressed);
        }
    }
    bytes.swap(lossless);return true;
}
bool EncodeBmp(const RasterImage& source,int depth,std::vector<BYTE>& bytes,float xDpi,float yDpi)
{
    if(!source.Valid() || (depth!=1 && depth!=4 && depth!=8 && depth!=24)) return false;
    RasterImage image=source;FlattenRaster(image,DocumentBackdrop());
    QuantizedImage quant;
    if(depth==1)
    {
        quant.palette={0xff000000,0xffffffff};quant.indices.resize(image.pixels.size());
        // Floyd-Steinberg grayscale diffusion for usable monochrome photographs.
        std::vector<double> current(image.width+2,0),next(image.width+2,0);
        for(int y=0;y<image.height;++y)
        {
            for(int x=0;x<image.width;++x)
            {
                DWORD p=image.pixels[size_t(y)*image.width+x];
                double value=.299*(p>>16&255)+.587*(p>>8&255)+.114*(p&255)+current[x+1];
                bool white=value>=128;quant.indices[size_t(y)*image.width+x]=white?1:0;
                double error=max(-255.0,min(255.0,value-(white?255:0)));
                current[x+2]+=error*7/16;next[x]+=error*3/16;next[x+1]+=error*5/16;next[x+2]+=error/16;
            }
            current.swap(next);std::fill(next.begin(),next.end(),0);
        }
    }
    else if(depth<24) quant=QuantizeRaster(image,1<<depth);
    size_t stride=(size_t(image.width)*depth+31)/32*4;
    unsigned colors=depth<24?1u<<depth:0,offset=14+40+colors*4;
    size_t total=offset+stride*image.height;if(total>MAXDWORD) return false;
    bytes.assign(total,0);bytes[0]='B';bytes[1]='M';Put32(bytes,2,unsigned(total));Put32(bytes,10,offset);
    Put32(bytes,14,40);Put32(bytes,18,image.width);Put32(bytes,22,image.height);Put16(bytes,26,1);Put16(bytes,28,depth);
    Put32(bytes,34,unsigned(stride*image.height));Put32(bytes,38,unsigned(xDpi/.0254f+.5f));Put32(bytes,42,unsigned(yDpi/.0254f+.5f));Put32(bytes,46,colors);
    for(size_t i=0;i<quant.palette.size();++i) Put32(bytes,54+i*4,quant.palette[i]&0xffffff);
    for(int y=0;y<image.height;++y)
    {
        BYTE* row=bytes.data()+offset+size_t(image.height-1-y)*stride;
        for(int x=0;x<image.width;++x)
        {
            size_t index=size_t(y)*image.width+x;DWORD p=image.pixels[index];
            if(depth==24) { row[x*3]=BYTE(p);row[x*3+1]=BYTE(p>>8);row[x*3+2]=BYTE(p>>16); }
            else if(depth==8) row[x]=quant.indices[index];
            else if(depth==4) row[x/2]|=quant.indices[index]<<((1-x%2)*4);
            else row[x/8]|=quant.indices[index]<<(7-x%8);
        }
    }
    return true;
}
bool DecodeWebp(const std::vector<BYTE>& bytes,RasterImage& image,ImageFileInfo* info)
{
    WebPBitstreamFeatures features={};
    if(WebPGetFeatures(bytes.data(),bytes.size(),&features)!=VP8_STATUS_OK || features.width<=0 || features.height<=0 ||
        size_t(features.width)*features.height>INT_MAX/4) return false;
    image.width=features.width;image.height=features.height;image.pixels.resize(size_t(image.width)*image.height);
    if(features.has_animation)
    {
        WebPData data={bytes.data(),bytes.size()};WebPAnimDecoderOptions options;WebPAnimDecoderOptionsInit(&options);options.color_mode=MODE_BGRA;
        WebPAnimDecoder* decoder=WebPAnimDecoderNew(&data,&options);if(!decoder) return false;
        BYTE* pixels=NULL;int timestamp=0;bool ok=WebPAnimDecoderGetNext(decoder,&pixels,&timestamp)!=0;
        if(ok) memcpy(image.pixels.data(),pixels,image.pixels.size()*4);
        WebPAnimDecoderDelete(decoder);if(!ok) return false;
    }
    else if(!WebPDecodeBGRAInto(bytes.data(),bytes.size(),reinterpret_cast<BYTE*>(image.pixels.data()),image.pixels.size()*4,image.width*4)) return false;
    if(info) { info->format=SaveFormat::WebP;info->paintData=ReadWebpPaintData(bytes);info->animated=features.has_animation!=0; }
    return true;
}
bool EncodeWebp(const RasterImage& image,const WebpOptions& options,std::vector<BYTE>& bytes)
{
    if(!image.Valid() || image.width>WEBP_MAX_DIMENSION || image.height>WEBP_MAX_DIMENSION) return false;
    WebPConfig config;if(!WebPConfigInit(&config)) return false;
    config.lossless=options.lossless;config.quality=float(max(0,min(100,options.quality)));config.method=6;config.exact=1;
    WebPPicture picture;if(!WebPPictureInit(&picture)) return false;
    picture.use_argb=1;picture.width=image.width;picture.height=image.height;
    if(!WebPPictureImportBGRA(&picture,reinterpret_cast<const BYTE*>(image.pixels.data()),image.width*4)) { WebPPictureFree(&picture);return false; }
    WebPMemoryWriter writer;WebPMemoryWriterInit(&writer);picture.writer=WebPMemoryWrite;picture.custom_ptr=&writer;
    bool ok=WebPEncode(&config,&picture)!=0;
    if(ok) bytes.assign(writer.mem,writer.mem+writer.size);
    WebPMemoryWriterClear(&writer);WebPPictureFree(&picture);return ok;
}
SaveFormat FormatFromPath(LPCWSTR path)
{
    LPCWSTR ext=PathFindExtensionW(path);
    if(!lstrcmpiW(ext,L".bmp") || !lstrcmpiW(ext,L".dib")) return SaveFormat::Bmp24;
    if(!lstrcmpiW(ext,L".jpg") || !lstrcmpiW(ext,L".jpeg") || !lstrcmpiW(ext,L".jpe") || !lstrcmpiW(ext,L".jfif")) return SaveFormat::Jpeg;
    if(!lstrcmpiW(ext,L".gif")) return SaveFormat::Gif;
    if(!lstrcmpiW(ext,L".tif") || !lstrcmpiW(ext,L".tiff")) return SaveFormat::Tiff;
    if(!lstrcmpiW(ext,L".webp")) return SaveFormat::WebP;
    if(!lstrcmpiW(ext,L".ico")) return SaveFormat::Ico;
    if(!lstrcmpiW(ext,L".tga") || !lstrcmpiW(ext,L".targa")) return SaveFormat::Tga;
    if(!lstrcmpiW(ext,L".dds")) return SaveFormat::Dds;
    return SaveFormat::Png;
}
bool IsCustomImageFile(LPCWSTR path)
{
    auto format=FormatFromPath(path);LPCWSTR ext=PathFindExtensionW(path);
    if(!lstrcmpiW(ext,L".png") || format==SaveFormat::WebP || format==SaveFormat::Ico || format==SaveFormat::Tiff || format==SaveFormat::Tga || format==SaveFormat::Dds) return true;
    BYTE signature[12]={};DWORD count=0;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(file==INVALID_HANDLE_VALUE) return false;
    ReadFile(file,signature,sizeof(signature),&count,NULL);CloseHandle(file);
    return (count>=4 && (!memcmp(signature,"DDS ",4) || !memcmp(signature,"II\x2a\0",4) || !memcmp(signature,"MM\0\x2a",4))) || (count>=8 && !memcmp(signature,"\x89PNG\r\n\x1a\n",8)) ||
        (count>=12 && !memcmp(signature,"RIFF",4) && !memcmp(signature+8,"WEBP",4)) ||
        (count>=6 && signature[0]==0 && signature[1]==0 && signature[2]==1 && signature[3]==0);
}
bool LoadCustomImage(LPCWSTR path,RasterImage& image,ImageFileInfo& info)
{
    try
    {
        std::vector<BYTE> data;if(!ReadImageBytes(path,data)) return false;
        if(data.size()>=8 && !memcmp(data.data(),"\x89PNG\r\n\x1a\n",8)) return DecodePng(data,image,&info);
        if(data.size()>=12 && !memcmp(data.data(),"RIFF",4) && !memcmp(data.data()+8,"WEBP",4)) return DecodeWebp(data,image,&info);
        if(data.size()>=6 && data[0]==0 && data[1]==0 && data[2]==1 && data[3]==0) return DecodeIco(data,image,info);
        if(data.size()>=4 && !memcmp(data.data(),"DDS ",4)) return DecodeDds(data,image,&info);
        if(data.size()>=4 && (!memcmp(data.data(),"II\x2a\0",4) || !memcmp(data.data(),"MM\0\x2a",4))) return DecodeTiff(data,image,&info);
        if(FormatFromPath(path)==SaveFormat::Tga) return DecodeTga(data,image,&info);
        return false;
    }
    catch(const std::bad_alloc&) { return false; }
}
bool SaveCustomImage(HBITMAP bitmap,LPCWSTR path,SaveFormat format,bool mainFile)
{
    try
    {
        RasterImage image;if(!ReadRaster(bitmap,image)) return false;
        std::vector<BYTE> bytes;bool ok=false;
        PaintServiceData data=CurrentPaintData();const PaintServiceData* service=PreservePaintData() && data.present && SupportsPaintData(format)?&data:NULL;
        if(format==SaveFormat::Png)
        {
            COLORREF backdrop=DocumentAlpha().enabled?DocumentBackdrop():CLR_INVALID;
            if(backdrop!=CLR_INVALID) for(DWORD& p:image.pixels) if(!(p>>24)) p=RGB(GetBValue(backdrop),GetGValue(backdrop),GetRValue(backdrop));
            ok=EncodePng(image,CurrentPngOptions(),bytes,g_xDpi,g_yDpi,false,backdrop);
        }
        else if(format==SaveFormat::WebP) ok=EncodeWebp(image,CurrentWebpOptions(),bytes);
        else if(format==SaveFormat::Ico) ok=EncodeIco(image,CurrentIcoOptions(),mainFile?&ImageDocumentInfo():NULL,bytes,service);
        else if(format==SaveFormat::Tga) ok=EncodeTga(image,bytes);
        else if(format==SaveFormat::Dds) ok=EncodeDds(image,bytes,CurrentDdsOptions());
        else if(format==SaveFormat::Tiff) ok=EncodeTiff(image,bytes,g_xDpi,g_yDpi,service);
        else if(int(format)<=int(SaveFormat::Bmp8))
        { const int depths[]={24,1,4,8};ok=EncodeBmp(image,depths[int(format)],bytes,g_xDpi,g_yDpi); }
        if(ok && service)
        {
            if(format==SaveFormat::Png) ok=SetPngPaintData(bytes,service);
            else if(format==SaveFormat::WebP) { bool alpha=false;for(DWORD p:image.pixels) if((p>>24)!=255) { alpha=true;break; }ok=SetWebpPaintData(bytes,*service,image.width,image.height,alpha); }
            else if(format==SaveFormat::Tga) ok=SetTgaPaintData(bytes,*service);
        }
        if(!ok || !WriteImageBytes(path,bytes)) return false;
        if(mainFile)
        {
            auto& info=ImageDocumentInfo();info.paintData=service?*service:PaintServiceData{};info.format=format;info.animated=false;info.multipleImages=false;
            info.pngBackdrop=format==SaveFormat::Png && DocumentAlpha().enabled?DocumentBackdrop():CLR_INVALID;
            if(format==SaveFormat::Dds) info.ddsCompression=CurrentDdsOptions().compression;
            if(format==SaveFormat::Ico)
            {
                ImageFileInfo updated;RasterImage decoded;
                if(DecodeIco(bytes,decoded,updated))
                {
                    int selected=CurrentIcoOptions().keepOtherImages && !info.icons.empty()?info.selectedIcon:0;
                    updated.selectedIcon=selected;info=std::move(updated);
                }
            }
        }
        return true;
    }
    catch(const std::bad_alloc&) { return false; }
}
