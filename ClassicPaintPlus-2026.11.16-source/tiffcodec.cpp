// Use the Windows TIFF codec, keeping straight alpha and lossless compression.
#include "precomp.h"
#include "imageformats.h"
#include <wincodec.h>
namespace {
struct ComScope { HRESULT status=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);~ComScope(){if(SUCCEEDED(status)) CoUninitialize();} };
bool Factory(CComPtr<IWICImagingFactory>& factory)
{ return SUCCEEDED(factory.CoCreateInstance(CLSID_WICImagingFactory)); }
}
bool DecodeTiff(const std::vector<BYTE>& bytes,RasterImage& image,ImageFileInfo* info)
{
    if(bytes.empty() || bytes.size()>0xffffffffu) return false;
    ComScope com;CComPtr<IWICImagingFactory> factory;if(!Factory(factory)) return false;
    CComPtr<IWICStream> stream;CComPtr<IWICBitmapDecoder> decoder;CComPtr<IWICBitmapFrameDecode> frame;
    if(FAILED(factory->CreateStream(&stream)) || FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()),DWORD(bytes.size()))) ||
       FAILED(factory->CreateDecoderFromStream(stream,NULL,WICDecodeMetadataCacheOnDemand,&decoder)) || FAILED(decoder->GetFrame(0,&frame))) return false;
    GUID container;if(FAILED(decoder->GetContainerFormat(&container)) || container!=GUID_ContainerFormatTiff) return false;
    UINT w=0,h=0,frames=0;double xDpi=96,yDpi=96;
    if(FAILED(frame->GetSize(&w,&h)) || !w || !h || size_t(w)*h>100000000 || w>INT_MAX || h>INT_MAX) return false;
    CComPtr<IWICFormatConverter> converter;
    if(FAILED(factory->CreateFormatConverter(&converter)) || FAILED(converter->Initialize(frame,GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,NULL,0,WICBitmapPaletteTypeCustom))) return false;
    RasterImage out;out.width=w;out.height=h;out.pixels.resize(size_t(w)*h);
    if(FAILED(converter->CopyPixels(NULL,w*4,UINT(out.pixels.size()*4),reinterpret_cast<BYTE*>(out.pixels.data())))) return false;
    // WIC exposes TIFF orientation as metadata rather than rotating its pixels.
    CComPtr<IWICMetadataQueryReader> metadata;unsigned orientation=1;
    if(SUCCEEDED(frame->GetMetadataQueryReader(&metadata)))
    { PROPVARIANT value;PropVariantInit(&value);if(SUCCEEDED(metadata->GetMetadataByName(L"/ifd/{ushort=274}",&value)) && value.vt==VT_UI2) orientation=value.uiVal;PropVariantClear(&value); }
    if(orientation>=2 && orientation<=8)
    {
        RasterImage rotated;rotated.width=orientation>=5?h:w;rotated.height=orientation>=5?w:h;rotated.pixels.resize(out.pixels.size());
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x)
        {
            unsigned dx=x,dy=y;
            switch(orientation)
            {
                case 2:dx=w-1-x;break;case 3:dx=w-1-x;dy=h-1-y;break;case 4:dy=h-1-y;break;
                case 5:dx=y;dy=x;break;case 6:dx=h-1-y;dy=x;break;case 7:dx=h-1-y;dy=w-1-x;break;case 8:dx=y;dy=w-1-x;break;
            }
            rotated.pixels[size_t(dy)*rotated.width+dx]=out.pixels[size_t(y)*w+x];
        }
        out=std::move(rotated);
    }
    frame->GetResolution(&xDpi,&yDpi);decoder->GetFrameCount(&frames);
    if(info)
    {
        info->format=SaveFormat::Tiff;info->multipleImages=frames>1;info->paintData={};
        if(metadata)
        {
            PROPVARIANT value;PropVariantInit(&value);
            if(SUCCEEDED(metadata->GetMetadataByName(L"/ifd/{ushort=270}",&value)) && value.vt==VT_LPSTR && value.pszVal)
            { size_t n=strnlen(value.pszVal,128);info->paintData=ParsePaintData(reinterpret_cast<const BYTE*>(value.pszVal),n); }
            PropVariantClear(&value);
        }
        if(orientation>=5 && orientation<=8) Swap(xDpi,yDpi);
        info->xDpi=float(xDpi>0?xDpi:96);info->yDpi=float(yDpi>0?yDpi:96);
    }
    image=std::move(out);return true;
}
bool EncodeTiff(const RasterImage& image,std::vector<BYTE>& bytes,float xDpi,float yDpi,const PaintServiceData* data)
{
    if(!image.Valid() || image.pixels.size()>0x3fffffff) return false;
    ComScope com;CComPtr<IWICImagingFactory> factory;if(!Factory(factory)) return false;
    CComPtr<IStream> stream;CComPtr<IWICBitmapEncoder> encoder;CComPtr<IWICBitmapFrameEncode> frame;CComPtr<IPropertyBag2> properties;
    if(FAILED(CreateStreamOnHGlobal(NULL,TRUE,&stream)) || FAILED(factory->CreateEncoder(GUID_ContainerFormatTiff,NULL,&encoder)) ||
       FAILED(encoder->Initialize(stream,WICBitmapEncoderNoCache)) || FAILED(encoder->CreateNewFrame(&frame,&properties))) return false;
    PROPBAG2 property={};property.pstrName=const_cast<LPWSTR>(L"TiffCompressionMethod");VARIANT value;VariantInit(&value);value.vt=VT_UI1;value.bVal=WICTiffCompressionLZW;
    if(FAILED(properties->Write(1,&property,&value)) || FAILED(frame->Initialize(properties)) || FAILED(frame->SetSize(image.width,image.height)) ||
       FAILED(frame->SetResolution(xDpi>0?xDpi:96,yDpi>0?yDpi:96))) return false;
    if(data && data->present)
    {
        CComPtr<IWICMetadataQueryWriter> metadata;if(FAILED(frame->GetMetadataQueryWriter(&metadata))) return false;
        std::string text=SerializePaintData(*data);PROPVARIANT value;PropVariantInit(&value);value.vt=VT_LPSTR;value.pszVal=const_cast<char*>(text.c_str());
        if(FAILED(metadata->SetMetadataByName(L"/ifd/{ushort=270}",&value))) return false;
    }
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
    if(FAILED(frame->SetPixelFormat(&format)) || format!=GUID_WICPixelFormat32bppBGRA ||
       FAILED(frame->WritePixels(image.height,image.width*4,UINT(image.pixels.size()*4),reinterpret_cast<BYTE*>(const_cast<DWORD*>(image.pixels.data())))) ||
       FAILED(frame->Commit()) || FAILED(encoder->Commit())) return false;
    STATSTG stat={};if(FAILED(stream->Stat(&stat,STATFLAG_NONAME)) || stat.cbSize.QuadPart>0xffffffffu) return false;
    LARGE_INTEGER zero={};if(FAILED(stream->Seek(zero,STREAM_SEEK_SET,NULL))) return false;
    bytes.resize(size_t(stat.cbSize.QuadPart));ULONG count=0;
    return SUCCEEDED(stream->Read(bytes.data(),ULONG(bytes.size()),&count)) && count==bytes.size();
}
