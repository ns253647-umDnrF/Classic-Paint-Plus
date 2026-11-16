#pragma once
#include "raster.h"
enum class SaveFormat { Bmp24, Bmp1, Bmp4, Bmp8, Jpeg, Png, Gif, Tiff, WebP, Ico, Tga, Dds };
#include <string>
#include "paintmetadata.h"
struct PngOptions
{
    bool interlaced=false,optimize=false;
    static PngOptions LoadDefaults();
    void SaveDefaults() const;
};
struct WebpOptions
{
    bool lossless=true;int quality=90;
    static WebpOptions LoadDefaults();
    void SaveDefaults() const;
};
struct IcoOptions { int size=0;bool keepOtherImages=true; };
struct DdsOptions
{
    int compression=0; // 0 uncompressed RGBA, 1 DXT1, 2 DXT3, 3 DXT5
    static DdsOptions LoadDefaults();
    void SaveDefaults() const;
};
struct IconFrame
{
    int width=0,height=0,depth=32;
    std::vector<BYTE> encoded;
};
struct ImageFileInfo
{
    SaveFormat format=SaveFormat::Png;
    float xDpi=96,yDpi=96;
    bool animated=false,multipleImages=false;
    int ddsCompression=0;
    std::vector<IconFrame> icons;
    int selectedIcon=0;
    COLORREF pngBackdrop=CLR_INVALID; // Standard PNG bKGD, independent of private metadata.
    PaintServiceData paintData;
};
struct QuantizedImage { std::vector<DWORD> palette;std::vector<BYTE> indices;double rms=0; };
QuantizedImage QuantizeRaster(const RasterImage& image,int colors);
bool DecodePng(const std::vector<BYTE>& bytes,RasterImage& image,ImageFileInfo* info=NULL);
bool EncodePng(const RasterImage& image,const PngOptions& options,std::vector<BYTE>& bytes,float xDpi=96,float yDpi=96,bool forceRgba=false,COLORREF backdrop=CLR_INVALID);
bool EncodeBmp(const RasterImage& image,int depth,std::vector<BYTE>& bytes,float xDpi,float yDpi);
bool DecodeWebp(const std::vector<BYTE>& bytes,RasterImage& image,ImageFileInfo* info=NULL);
bool EncodeWebp(const RasterImage& image,const WebpOptions& options,std::vector<BYTE>& bytes);
bool DecodeIco(const std::vector<BYTE>& bytes,RasterImage& image,ImageFileInfo& info,int frame=-1);
bool DecodeIconFrame(const IconFrame& frame,RasterImage& image);
bool EncodeIco(const RasterImage& image,const IcoOptions& options,const ImageFileInfo* info,std::vector<BYTE>& bytes,const PaintServiceData* data=NULL);
bool DecodeTga(const std::vector<BYTE>& bytes,RasterImage& image,ImageFileInfo* info=NULL);
bool EncodeTga(const RasterImage& image,std::vector<BYTE>& bytes);
bool DecodeDds(const std::vector<BYTE>& bytes,RasterImage& image,ImageFileInfo* info=NULL);
bool EncodeDds(const RasterImage& image,std::vector<BYTE>& bytes,const DdsOptions& options=DdsOptions{});
bool DecodeTiff(const std::vector<BYTE>& bytes,RasterImage& image,ImageFileInfo* info=NULL);
bool EncodeTiff(const RasterImage& image,std::vector<BYTE>& bytes,float xDpi=96,float yDpi=96,const PaintServiceData* data=NULL);
SaveFormat FormatFromPath(LPCWSTR path);
bool IsCustomImageFile(LPCWSTR path);
bool LoadCustomImage(LPCWSTR path,RasterImage& image,ImageFileInfo& info);
bool SaveCustomImage(HBITMAP bitmap,LPCWSTR path,SaveFormat format,bool mainFile);
const PngOptions& CurrentPngOptions();
const WebpOptions& CurrentWebpOptions();
const IcoOptions& CurrentIcoOptions();
const DdsOptions& CurrentDdsOptions();
void SetCurrentPngOptions(const PngOptions& options);
void SetCurrentWebpOptions(const WebpOptions& options);
void SetCurrentIcoOptions(const IcoOptions& options);
void SetCurrentDdsOptions(const DdsOptions& options);
void SetImageDocumentInfo(const ImageFileInfo& info);
ImageFileInfo& ImageDocumentInfo();
SaveFormat CurrentSaveFormat();
void SetCurrentSaveFormat(SaveFormat format);
void SetPendingImageFormat(LPCWSTR path,SaveFormat format);
SaveFormat ResolveSaveFormat(LPCWSTR path,bool mainFile);
CStringW BuildSaveImageFilters();
SaveFormat SaveFormatFromFilterIndex(UINT index);
UINT SaveFormatFilterIndex(SaveFormat format);
CStringW BuildOpenImageFilters();
bool ShowPngOptions(HWND owner,PngOptions& options);
bool ShowWebpOptions(HWND owner,WebpOptions& options);
bool ShowIcoOptions(HWND owner,IcoOptions& options);
bool ShowDdsOptions(HWND owner,DdsOptions& options);
int SelectIconFrame(HWND owner,const ImageFileInfo& info);
