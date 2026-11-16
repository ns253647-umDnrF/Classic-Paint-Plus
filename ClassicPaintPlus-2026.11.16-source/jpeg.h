// Local JPEG options and metadata handling. LGPL-2.0-or-later.
#pragma once
#include <vector>
#include "imageformats.h"

struct JpegOptions
{
    int quality=90;
    bool progressive=false, optimize=true, horizontal=true, vertical=false;
    int thumbnails=0; // 0 update existing, 1 always create/update, 2 leave unchanged
    bool dcf=false, preserveMetadata=true;
    static JpegOptions LoadDefaults();
    void SaveDefaults() const;
};
struct JpegMarker { int code; std::vector<BYTE> data; };
struct SaveImageOptions
{
    JpegOptions jpeg;
    PngOptions png;
    WebpOptions webp;
    IcoOptions ico;
    DdsOptions dds;
    bool preservePaintData=true;
};
using JpegMetadata=std::vector<JpegMarker>;
const JpegOptions& CurrentJpegOptions();
void SetCurrentJpegOptions(const JpegOptions& options);
bool ShowJpegOptions(HWND owner,JpegOptions& options);
bool IsJpegFile(LPCWSTR name,REFGUID format=GUID_NULL);
JpegMetadata ReadJpegMetadata(LPCWSTR name);
void SetDocumentJpegMetadata(LPCWSTR name);
bool SaveJpegBitmap(HBITMAP bitmap,LPCWSTR path,const JpegOptions& options,
                    const JpegMetadata& metadata,float xDpi,float yDpi);
bool SaveDocumentJpeg(HBITMAP bitmap,LPCWSTR path,bool mainFile);
UINT_PTR JpegSaveHook(HWND hwnd,UINT message,WPARAM wp,LPARAM lp);
bool JpegFilterSelected(const OPENFILENAMEW& ofn);
