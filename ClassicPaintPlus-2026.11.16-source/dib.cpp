/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Some DIB related functions
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 */

#include "precomp.h"
#include "shellintegration.h"
#include "jpeg.h"
#include "imageformats_resource.h"

INT g_fileSize = 0;
float g_xDpi = 96;
float g_yDpi = 96;
SYSTEMTIME g_fileTime;

#define WIDTHBYTES(i) (((i) + 31) / 32 * 4)

struct BITMAPINFOEX : BITMAPINFO
{
    RGBQUAD bmiColorsExtra[256 - 1];
};

/* FUNCTIONS ********************************************************/

// Convert DPI (dots per inch) into PPCM (pixels per centimeter)
float PpcmFromDpi(float dpi)
{
    // 1 DPI is 0.0254 meter. 1 centimeter is 1/100 meter.
    return dpi / (0.0254f * 100.0f);
}

HBITMAP
CreateDIBWithProperties(int width, int height)
{
    if(width<=0 || height<=0 || width>32767 || height>32767 || size_t(width)*height>INT_MAX/4)
        return NULL;
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32; // BGRT: zero reserved byte = opaque GDI drawing
    void* bits=NULL;
    return CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
}

HBITMAP
CreateMonoBitmap(int width, int height, BOOL bWhite)
{
    HBITMAP hbm = CreateBitmap(width, height, 1, 1, NULL);
    if (hbm == NULL)
        return NULL;

    if (bWhite)
    {
        HDC hdc = CreateCompatibleDC(NULL);
        HGDIOBJ hbmOld = SelectObject(hdc, hbm);
        RECT rc = { 0, 0, width, height };
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
        SelectObject(hdc, hbmOld);
        DeleteDC(hdc);
    }

    return hbm;
}

HBITMAP
CreateColorDIB(int width, int height, COLORREF rgb)
{
    HBITMAP ret = CreateDIBWithProperties(width, height);
    if (!ret)
        return NULL;

    if (rgb)
    {
        HDC hdc = CreateCompatibleDC(NULL);
        HGDIOBJ hbmOld = SelectObject(hdc, ret);
        RECT rc;
        SetRect(&rc, 0, 0, width, height);
        HBRUSH hbr = CreateSolidBrush(rgb);
        FillRect(hdc, &rc, hbr);
        DeleteObject(hbr);
        SelectObject(hdc, hbmOld);
        DeleteDC(hdc);
    }

    return ret;
}

HBITMAP CopyMonoImage(HBITMAP hbm, INT cx, INT cy, INT stretchMode)
{
    BITMAP bm;
    if (!::GetObjectW(hbm, sizeof(bm), &bm))
        return NULL;

    if (cx == 0 || cy == 0)
    {
        cx = bm.bmWidth;
        cy = bm.bmHeight;
    }

    HDC hdc1 = ::CreateCompatibleDC(NULL);
    HDC hdc2 = ::CreateCompatibleDC(NULL);
    HBITMAP hbmNew = ::CreateBitmap(cx, cy, 1, 1, NULL);
    HGDIOBJ hbm1Old = ::SelectObject(hdc1, hbm);
    HGDIOBJ hbm2Old = ::SelectObject(hdc2, hbmNew);
    ::SetStretchBltMode(hdc2, stretchMode);
    ::StretchBlt(hdc2, 0, 0, cx, cy, hdc1, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
    ::SelectObject(hdc1, hbm1Old);
    ::SelectObject(hdc2, hbm2Old);
    ::DeleteDC(hdc1);
    ::DeleteDC(hdc2);
    return hbmNew;
}

HBITMAP CopyDIBImage(HBITMAP hbm, INT cx, INT cy, INT stretchMode)
{
    BITMAP format={};GetObject(hbm,sizeof(format),&format);
    if(format.bmBitsPixel==32) return CopyRasterBitmap(hbm,cx,cy,stretchMode==STRETCH_HALFTONE?2:0);
    if (stretchMode == STRETCH_HALFTONE)
        return (HBITMAP)CopyImage(hbm, IMAGE_BITMAP, cx, cy, LR_CREATEDIBSECTION);

    BITMAP bm;
    if (!::GetObjectW(hbm, sizeof(bm), &bm))
        return NULL;

    if (cx == 0 || cy == 0)
    {
        cx = bm.bmWidth;
        cy = bm.bmHeight;
    }

    HDC hdc1 = ::CreateCompatibleDC(NULL);
    HDC hdc2 = ::CreateCompatibleDC(NULL);
    HBITMAP hbmNew = CreateDIBWithProperties(cx, cy);
    HGDIOBJ hbm1Old = ::SelectObject(hdc1, hbm);
    HGDIOBJ hbm2Old = ::SelectObject(hdc2, hbmNew);
    ::SetStretchBltMode(hdc2, stretchMode);
    ::StretchBlt(hdc2, 0, 0, cx, cy, hdc1, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
    ::SelectObject(hdc1, hbm1Old);
    ::SelectObject(hdc2, hbm2Old);
    ::DeleteDC(hdc1);
    ::DeleteDC(hdc2);
    return hbmNew;
}

HBITMAP CachedBufferDIB(HBITMAP hbm, int minimalWidth, int minimalHeight)
{
    if (minimalWidth <= 0)
        minimalWidth = 1;
    if (minimalHeight <= 0)
        minimalHeight = 1;

    BITMAP bm;
    if (!GetObjectW(hbm, sizeof(bm), &bm))
        hbm = NULL;

    if (hbm && minimalWidth <= bm.bmWidth && minimalHeight <= bm.bmHeight)
        return hbm;

    if (hbm)
        DeleteObject(hbm);

    return CreateDIBWithProperties((minimalWidth * 3) / 2, (minimalHeight * 3) / 2);
}

int
GetDIBWidth(HBITMAP hBitmap)
{
    BITMAP bm={};
    ::GetObjectW(hBitmap, sizeof(BITMAP), &bm);
    return bm.bmWidth;
}

int
GetDIBHeight(HBITMAP hBitmap)
{
    BITMAP bm={};
    ::GetObjectW(hBitmap, sizeof(BITMAP), &bm);
    return bm.bmHeight;
}

BOOL SaveDIBToFile(HBITMAP hBitmap, LPCWSTR FileName, BOOL fIsMainFile, REFGUID guidFileType)
{
    CWaitCursor waitCursor;

    SaveFormat format=ResolveSaveFormat(FileName,fIsMainFile!=FALSE);
    if(guidFileType==Gdiplus::ImageFormatPNG) format=SaveFormat::Png;
    if(guidFileType==Gdiplus::ImageFormatJPEG) format=SaveFormat::Jpeg;
    bool alphaFormat=format==SaveFormat::Png || format==SaveFormat::WebP || format==SaveFormat::Ico || format==SaveFormat::Tga || format==SaveFormat::Dds || format==SaveFormat::Tiff;
    if(!ConfirmAlphaLoss(hBitmap,alphaFormat)) return FALSE;
    if(format==SaveFormat::Ico && CurrentIcoOptions().size==0 && (GetDIBWidth(hBitmap)>256 || GetDIBHeight(hBitmap)>256))
    {
        IcoOptions options=CurrentIcoOptions();
        if(!ShowIcoOptions(mainWindow,options)) return FALSE;
        SetCurrentIcoOptions(options);
    }
    HRESULT hr=E_FAIL;
    if(format==SaveFormat::Png || format==SaveFormat::WebP || format==SaveFormat::Ico || format==SaveFormat::Tga || format==SaveFormat::Dds || format==SaveFormat::Tiff || int(format)<=int(SaveFormat::Bmp8))
        hr=SaveCustomImage(hBitmap,FileName,format,fIsMainFile!=FALSE)?S_OK:E_FAIL;
    else if(format==SaveFormat::Jpeg)
        hr=SaveDocumentJpeg(hBitmap,FileName,fIsMainFile!=FALSE)?S_OK:E_FAIL;
    else
    {
        // Legacy formats without alpha use an explicit white composite.
        RasterImage pixels;
        if(ReadRaster(hBitmap,pixels))
        {
            FlattenRaster(pixels,DocumentBackdrop());CImageDx img;img.Attach(MakeRasterBitmap(pixels));
            hr=img.SaveDx(FileName,guidFileType,g_xDpi,g_yDpi);
        }
    }

    if (FAILED(hr))
    {
        ShowError(IDS_SAVEERROR, FileName);
        return FALSE;
    }

    if(format==SaveFormat::Png) SetCurrentPngOptions(PngOptions::LoadDefaults());
    if (!fIsMainFile)
        return TRUE;

    WIN32_FIND_DATAW find;
    HANDLE hFind = ::FindFirstFileW(FileName, &find);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        ShowError(IDS_SAVEERROR, FileName);
        return FALSE;
    }
    ::FindClose(hFind);

    ImageDocumentInfo().format=format;SetCurrentSaveFormat(format);
    SetFileInfo(FileName, &find, TRUE);
    if(IsJpegFile(FileName,guidFileType)) SetDocumentJpegMetadata(FileName);
    imageModel.MarkSaved();
    return TRUE;
}

BOOL PrintBitmapDocument(HDC printer,HBITMAP bitmap,LPCWSTR title,LPCWSTR output)
{
    RasterImage image;
    if(!printer || !ReadRaster(bitmap,image)) return FALSE;
    FlattenRaster(image,DocumentBackdrop());
    int pageWidth=GetDeviceCaps(printer,HORZRES),pageHeight=GetDeviceCaps(printer,VERTRES);
    if(pageWidth<=0 || pageHeight<=0) return FALSE;
    double width=image.width*double(GetDeviceCaps(printer,LOGPIXELSX))/(g_xDpi>0?g_xDpi:96);
    double height=image.height*double(GetDeviceCaps(printer,LOGPIXELSY))/(g_yDpi>0?g_yDpi:96);
    if(width<=0 || height<=0) return FALSE;
    double scale=min(1.0,min(pageWidth/width,pageHeight/height));
    int targetWidth=max(1,int(width*scale)),targetHeight=max(1,int(height*scale));
    DOCINFOW doc={sizeof(doc)};doc.lpszDocName=title;doc.lpszOutput=output;
    if(StartDocW(printer,&doc)<=0) return FALSE;
    BOOL ok=StartPage(printer)>0;
    if(ok)
    {
        BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=image.width;info.bmiHeader.biHeight=-image.height;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        // DIB transfer works across display/printer devices; StretchBlt does not.
        SetStretchBltMode(printer,HALFTONE);SetBrushOrgEx(printer,0,0,NULL);
        int lines=StretchDIBits(printer,0,0,targetWidth,targetHeight,0,0,image.width,image.height,
            image.pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);
        ok=lines!=0 && lines!=GDI_ERROR && EndPage(printer)>0;
    }
    if(ok) ok=EndDoc(printer)>0;
    if(!ok) AbortDoc(printer);
    return ok;
}

void SetFileInfo(LPCWSTR name, LPWIN32_FIND_DATAW pFound, BOOL isAFile)
{
    // update file time and size
    if (pFound)
    {
        FILETIME ft;
        ::FileTimeToLocalFileTime(&pFound->ftLastWriteTime, &ft);
        ::FileTimeToSystemTime(&ft, &g_fileTime);

        g_fileSize = pFound->nFileSizeLow;
    }
    else
    {
        ZeroMemory(&g_fileTime, sizeof(g_fileTime));
        g_fileSize = 0;
    }

    // update g_szFileName
    if (name && name[0])
    {
        CStringW strName = name;
        ::GetFullPathNameW(strName, _countof(g_szFileName), g_szFileName, NULL);
        // The following code won't work correctly when (name == g_szFileName):
        //   ::GetFullPathNameW(name, _countof(g_szFileName), g_szFileName, NULL);
    }
    else
    {
        ::LoadStringW(g_hinstExe, IDS_DEFAULTFILENAME, g_szFileName, _countof(g_szFileName));
    }

    // set title
    CStringW strTitle;
    strTitle.Format(IDS_WINDOWTITLE, PathFindFileNameW(g_szFileName));
    mainWindow.SetWindowText(strTitle);

    // update file info and recent
    g_isAFile = isAFile;
    if (g_isAFile)
        registrySettings.SetMostRecentFile(g_szFileName);

    imageModel.MarkSaved();
}

HBITMAP InitializeImage(LPCWSTR name, LPWIN32_FIND_DATAW pFound, BOOL isFile)
{
    COLORREF white = RGB(255, 255, 255);
    HBITMAP hBitmap = CreateColorDIB(registrySettings.BMPWidth, registrySettings.BMPHeight, white);
    if (hBitmap == NULL)
    {
        ShowOutOfMemory();
        return NULL;
    }

    HDC hScreenDC = ::GetDC(NULL);
    g_xDpi = (float)::GetDeviceCaps(hScreenDC, LOGPIXELSX);
    g_yDpi = (float)::GetDeviceCaps(hScreenDC, LOGPIXELSY);
    ::ReleaseDC(NULL, hScreenDC);

    SetDocumentJpegMetadata(NULL);SetImageDocumentInfo(ImageFileInfo{});
    return SetBitmapAndInfo(hBitmap, name, pFound, isFile);
}

HBITMAP SetBitmapAndInfo(HBITMAP hBitmap, LPCWSTR name, LPWIN32_FIND_DATAW pFound, BOOL isFile)
{
    ResetDocumentAlpha(hBitmap);
    // update image
    canvasWindow.updateScrollPos();
    imageModel.PushImageForUndo(hBitmap);
    imageModel.ClearHistory();

    SetFileInfo(name, pFound, isFile);
    g_imageSaved = TRUE;
    return hBitmap;
}

HBITMAP DoLoadImageFile(HWND hwnd, LPCWSTR name, BOOL fIsMainFile)
{
    CWaitCursor waitCursor;

    // find the file
    WIN32_FIND_DATAW find;
    HANDLE hFind = ::FindFirstFileW(name, &find);
    if (hFind == INVALID_HANDLE_VALUE) // does not exist
    {
        ShowError(IDS_LOADERRORTEXT, name);
        return NULL;
    }
    ::FindClose(hFind);

    // is file empty?
    if (find.nFileSizeLow == 0 && find.nFileSizeHigh == 0)
    {
        if (fIsMainFile)
            return InitializeImage(name, &find, TRUE);
    }

    HBITMAP hBitmap=NULL;ImageFileInfo info;info.format=FormatFromPath(name);
    float xDpi=0,yDpi=0;
    if(IsCustomImageFile(name))
    {
        RasterImage pixels;
        if(!LoadCustomImage(name,pixels,info)) { ShowError(IDS_LOADERRORTEXT,name);return NULL; }
        if(fIsMainFile && info.format==SaveFormat::Ico)
        {
            int selected=SelectIconFrame(hwnd,info);if(selected<0) return NULL;
            if(selected!=info.selectedIcon && !DecodeIconFrame(info.icons[selected],pixels))
            { ShowError(IDS_LOADERRORTEXT,name);return NULL; }
            info.selectedIcon=selected;info.paintData=ReadPngPaintData(info.icons[selected].encoded);
        }
        hBitmap=MakeRasterBitmap(pixels);xDpi=info.xDpi;yDpi=info.yDpi;
    }
    else
    {
        CImageDx img;HRESULT hr=img.LoadDx(name,&xDpi,&yDpi);
        if(FAILED(hr)) { ShowError(IDS_LOADERRORTEXT,name);return NULL; }
        HBITMAP original=img.Detach();RasterImage pixels;
        if(ReadRaster(original,pixels,true)) hBitmap=MakeRasterBitmap(pixels);
        DeleteObject(original);
        if(info.format==SaveFormat::Bmp24)
        {
            HANDLE file=CreateFileW(name,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
            if(file!=INVALID_HANDLE_VALUE)
            {
                BYTE header[54]={};DWORD read=0;ReadFile(file,header,sizeof(header),&read,NULL);CloseHandle(file);
                if(read>=30 && header[0]=='B' && header[1]=='M')
                { if(header[28]==1) info.format=SaveFormat::Bmp1;else if(header[28]==4) info.format=SaveFormat::Bmp4;else if(header[28]==8) info.format=SaveFormat::Bmp8; }
            }
        }
    }
    if(!hBitmap) { ShowOutOfMemory();return NULL; }
    if(!fIsMainFile) return hBitmap;

    if (xDpi <= 0 || yDpi <= 0)
    {
        HDC hDC = ::GetDC(NULL);
        xDpi = (float)::GetDeviceCaps(hDC, LOGPIXELSX);
        yDpi = (float)::GetDeviceCaps(hDC, LOGPIXELSY);
        ::ReleaseDC(NULL, hDC);
    }

    g_xDpi = xDpi;
    g_yDpi = yDpi;

    SetImageDocumentInfo(info);
    SetDocumentJpegMetadata(name);
    SetBitmapAndInfo(hBitmap, name, &find, TRUE);
    RegisterPaintWithWindows();
    SetPreservePaintData(true);
    if(info.paintData.present)
    {
        auto& state=DocumentAlpha();state.enabled=state.customized=true;state.hasColor=info.paintData.hasColor;
        state.color=info.paintData.color;state.backdrop=info.paintData.backdrop;NotifyDocumentAlpha();
    }
    else if(info.format==SaveFormat::Png && info.pngBackdrop!=CLR_INVALID && DocumentAlpha().enabled)
    {
        // bKGD suggests a display matte, not a transparent colour key or an edit.
        DocumentAlpha().backdrop=info.pngBackdrop;NotifyDocumentAlpha();
    }
    if(info.animated)
    {
        CStringW message;message.LoadString(g_hinstExe,IDS_ANIMATED_FIRST);
        CStringW title;title.LoadString(g_hinstExe,IDS_PROGRAMNAME);
        MessageBoxW(hwnd,message,title,MB_OK|MB_ICONINFORMATION);
        CStringW frameName(name);int dot=frameName.ReverseFind(L'.');
        if(dot>=0) frameName=frameName.Left(dot);frameName+=L"-frame.webp";
        SetFileInfo(frameName,NULL,FALSE);g_imageSaved=FALSE;
    }
    else if(info.multipleImages)
    {
        CStringW message;message.LoadString(g_hinstExe,IDS_MULTI_IMAGE_FIRST);
        CStringW title;title.LoadString(g_hinstExe,IDS_PROGRAMNAME);
        MessageBoxW(hwnd,message,title,MB_OK|MB_ICONINFORMATION);
        CStringW singleName(name);int dot=singleName.ReverseFind(L'.');
        if(dot>=0) singleName=singleName.Left(dot);
        singleName+=info.format==SaveFormat::Tiff?L"-page.tif":L"-surface.dds";
        SetFileInfo(singleName,NULL,FALSE);g_imageSaved=FALSE;
    }
    return hBitmap;
}

HBITMAP Rotate90DegreeBlt(HDC hDC1, INT cx, INT cy, BOOL bRight, BOOL bMono)
{
    if(cx<=0 || cy<=0) return NULL;
    if(!bMono)
    {
        RasterImage source,out;
        if(ReadRaster(reinterpret_cast<HBITMAP>(GetCurrentObject(hDC1,OBJ_BITMAP)),source))
        {
            if(cx>source.width || cy>source.height) return NULL;
            out.width=cy;out.height=cx;out.pixels.resize(size_t(cx)*cy);
            for(int y=0;y<cy;++y) for(int x=0;x<cx;++x)
                out.pixels[bRight?size_t(x)*cy+(cy-1-y):size_t(cx-1-x)*cy+y]=source.pixels[size_t(y)*source.width+x];
            return MakeRasterBitmap(out);
        }
    }
    HBITMAP hbm2;
    if (bMono)
        hbm2 = ::CreateBitmap(cy, cx, 1, 1, NULL);
    else
        hbm2 = CreateDIBWithProperties(cy, cx);
    if (!hbm2)
        return NULL;

    HDC hDC2 = CreateCompatibleDC(NULL);
    HGDIOBJ hbm2Old = SelectObject(hDC2, hbm2);
    if (bRight)
    {
        for (INT y = 0; y < cy; ++y)
        {
            for (INT x = 0; x < cx; ++x)
            {
                COLORREF rgb = GetPixel(hDC1, x, y);
                SetPixelV(hDC2, cy - (y + 1), x, rgb);
            }
        }
    }
    else
    {
        for (INT y = 0; y < cy; ++y)
        {
            for (INT x = 0; x < cx; ++x)
            {
                COLORREF rgb = GetPixel(hDC1, x, y);
                SetPixelV(hDC2, y, cx - (x + 1), rgb);
            }
        }
    }
    SelectObject(hDC2, hbm2Old);
    DeleteDC(hDC2);
    return hbm2;
}

HBITMAP SkewDIB(HDC hDC1, HBITMAP hbm, INT nDegree, BOOL bVertical, BOOL bMono)
{
    CWaitCursor waitCursor;

    if(nDegree < -89 || nDegree>89) return NULL;

    if (nDegree == 0)
        return CopyDIBImage(hbm);

    const double eTan = tan(abs(nDegree) * M_PI / 180);

    BITMAP bm={};
    if(!::GetObjectW(hbm, sizeof(bm), &bm)) return NULL;
    INT cx = bm.bmWidth, cy = bm.bmHeight, dx = 0, dy = 0;
    if (bVertical)
        dy = INT(cx * eTan);
    else
        dx = INT(cy * eTan);

    if (dx == 0 && dy == 0)
        return bMono?CopyMonoImage(hbm):CopyDIBImage(hbm);

    if(cx+dx>32767 || cy+dy>32767 || size_t(cx+dx)*(cy+dy)>INT_MAX/4) return NULL;

    HBITMAP hbmNew;
    if (bMono)
        hbmNew = CreateMonoBitmap(cx + dx, cy + dy, FALSE);
    else
        hbmNew = CreateColorDIB(cx + dx, cy + dy, RGB(255, 255, 255));
    if (!hbmNew)
        return NULL;

    HDC hDC2 = CreateCompatibleDC(NULL);
    HGDIOBJ hbm2Old = SelectObject(hDC2, hbmNew);
    if (bVertical)
    {
        for (INT x = 0; x < cx; ++x)
        {
            INT delta = INT(x * eTan);
            if (nDegree > 0)
                ::BitBlt(hDC2, x, (dy - delta), 1, cy, hDC1, x, 0, SRCCOPY);
            else
                ::BitBlt(hDC2, x, delta, 1, cy, hDC1, x, 0, SRCCOPY);
        }
    }
    else
    {
        for (INT y = 0; y < cy; ++y)
        {
            INT delta = INT(y * eTan);
            if (nDegree > 0)
                ::BitBlt(hDC2, (dx - delta), y, cx, 1, hDC1, 0, y, SRCCOPY);
            else
                ::BitBlt(hDC2, delta, y, cx, 1, hDC1, 0, y, SRCCOPY);
        }
    }

    SelectObject(hDC2, hbm2Old);
    DeleteDC(hDC2);
    return hbmNew;
}

HBITMAP getSubImage(HBITMAP hbmWhole, const RECT& rcPartial)
{
    CRect rc = rcPartial;
    HBITMAP hbmPart = CreateDIBWithProperties(rc.Width(), rc.Height());
    if (!hbmPart)
        return NULL;

    HDC hDC1 = ::CreateCompatibleDC(NULL);
    HDC hDC2 = ::CreateCompatibleDC(NULL);
    HGDIOBJ hbm1Old = ::SelectObject(hDC1, hbmWhole);
    HGDIOBJ hbm2Old = ::SelectObject(hDC2, hbmPart);
    ::BitBlt(hDC2, 0, 0, rc.Width(), rc.Height(), hDC1, rc.left, rc.top, SRCCOPY);
    ::SelectObject(hDC1, hbm1Old);
    ::SelectObject(hDC2, hbm2Old);
    ::DeleteDC(hDC1);
    ::DeleteDC(hDC2);
    return hbmPart;
}

void putSubImage(HBITMAP hbmWhole, const RECT& rcPartial, HBITMAP hbmPart)
{
    CRect rc = rcPartial;
    HDC hDC1 = ::CreateCompatibleDC(NULL);
    HDC hDC2 = ::CreateCompatibleDC(NULL);
    HGDIOBJ hbm1Old = ::SelectObject(hDC1, hbmWhole);
    HGDIOBJ hbm2Old = ::SelectObject(hDC2, hbmPart);
    ::BitBlt(hDC1, rc.left, rc.top, rc.Width(), rc.Height(), hDC2, 0, 0, SRCCOPY);
    ::SelectObject(hDC1, hbm1Old);
    ::SelectObject(hDC2, hbm2Old);
    ::DeleteDC(hDC1);
    ::DeleteDC(hDC2);
}

struct BITMAPINFODX : BITMAPINFO
{
    RGBQUAD bmiColorsAdditional[256 - 1];
};

static HGLOBAL ClipboardMemory(const void* bytes,size_t size)
{
    HGLOBAL memory=GlobalAlloc(GHND|GMEM_SHARE,size);if(!memory) return NULL;
    void* data=GlobalLock(memory);if(!data) { GlobalFree(memory);return NULL; }
    memcpy(data,bytes,size);GlobalUnlock(memory);return memory;
}
HGLOBAL BitmapToClipboardDIB(HBITMAP bitmap)
{
    // Legacy consumers get ordinary, opaque BGR. Never expose internal BGRT.
    RasterImage image;std::vector<BYTE> bmp;
    if(!ReadRaster(bitmap,image) || !EncodeBmp(image,24,bmp,96,96)) return NULL;
    return ClipboardMemory(bmp.data()+14,bmp.size()-14);
}
HGLOBAL BitmapToClipboardDIBV5(HBITMAP bitmap)
{
    RasterImage image;if(!ReadRaster(bitmap,image)) return NULL;
    BITMAPV5HEADER h={};h.bV5Size=sizeof(h);h.bV5Width=image.width;h.bV5Height=-image.height;
    h.bV5Planes=1;h.bV5BitCount=32;h.bV5Compression=BI_BITFIELDS;
    h.bV5SizeImage=DWORD(image.pixels.size()*4);
    h.bV5RedMask=0xff0000;h.bV5GreenMask=0xff00;h.bV5BlueMask=0xff;h.bV5AlphaMask=0xff000000;
    h.bV5CSType=LCS_sRGB;h.bV5Intent=LCS_GM_IMAGES;
    std::vector<BYTE> data(sizeof(h)+h.bV5SizeImage);
    memcpy(data.data(),&h,sizeof(h));memcpy(data.data()+sizeof(h),image.pixels.data(),h.bV5SizeImage);
    return ClipboardMemory(data.data(),data.size());
}
HBITMAP BitmapFromClipboardDIB(HGLOBAL memory)
{
    const BYTE* data=static_cast<const BYTE*>(GlobalLock(memory));size_t length=GlobalSize(memory);
    if(!data) return NULL;
    HBITMAP result=NULL;
    do
    {
        if(length<sizeof(BITMAPCOREHEADER)) break;
        DWORD header=*reinterpret_cast<const DWORD*>(data);
        if(header>length || (header!=12 && header<40)) break;
        int w,h,depth;DWORD compression=0,colors=0;size_t offset=header;
        auto* info=reinterpret_cast<const BITMAPINFO*>(data);
        if(header==12)
        {
            auto* core=reinterpret_cast<const BITMAPCOREHEADER*>(data);
            w=core->bcWidth;h=core->bcHeight;depth=core->bcBitCount;
            if(core->bcPlanes!=1) break;
            if(depth<=8) offset+=(1u<<depth)*3;
        }
        else
        {
            w=info->bmiHeader.biWidth;h=info->bmiHeader.biHeight;depth=info->bmiHeader.biBitCount;
            compression=info->bmiHeader.biCompression;colors=info->bmiHeader.biClrUsed;
            if(info->bmiHeader.biPlanes!=1) break;
            if(depth<=8) { if(!colors) colors=1u<<depth;if(colors>(1u<<depth)) break; }
            if(colors>256) break;
            offset+=size_t(colors)*4;
            if(header==40 && compression==BI_BITFIELDS) offset+=12;
        }
        if(w<=0 || w>32767 || !h || h < -32767 || h>32767 ||
           (depth!=1 && depth!=4 && depth!=8 && depth!=16 && depth!=24 && depth!=32) ||
           (compression!=BI_RGB && compression!=BI_BITFIELDS)) break;
        int height=abs(h);size_t stride=(size_t(w)*depth+31)/32*4;
        if(offset>length || stride*height>length-offset || size_t(w)*height>INT_MAX/4) break;
        HDC dc=CreateCompatibleDC(NULL);HBITMAP temp=CreateDIBWithProperties(w,height);
        int rows=temp?SetDIBits(dc,temp,0,height,data+offset,info,DIB_RGB_COLORS):0;DeleteDC(dc);
        RasterImage image;
        bool ok=rows==height && ReadRaster(temp,image,true);DeleteObject(temp);if(!ok) break;
        // V4/V5 explicitly describes opacity; CF_DIB's reserved byte does not.
        if(depth==32 && header>=108)
        {
            DWORD mask=reinterpret_cast<const BITMAPV4HEADER*>(data)->bV4AlphaMask;
            if(mask)
            {
                unsigned shift=0;while(!(mask&1)) { mask>>=1;++shift; }
                for(int y=0;y<height;++y) for(int x=0;x<w;++x)
                {
                    DWORD pixel=*reinterpret_cast<const DWORD*>(data+offset+size_t(h<0?y:height-1-y)*stride+x*4);
                    unsigned alpha=unsigned((ULONGLONG((pixel>>shift)&mask)*255+mask/2)/mask);
                    image.pixels[size_t(y)*w+x]=(image.pixels[size_t(y)*w+x]&0xffffff)|(alpha<<24);
                }
            }
        }
        result=MakeRasterBitmap(image);
    } while(false);
    GlobalUnlock(memory);return result;
}

HBITMAP BitmapFromHEMF(HENHMETAFILE hEMF)
{
    CWaitCursor waitCursor;

    ENHMETAHEADER header;
    if (!GetEnhMetaFileHeader(hEMF, sizeof(header), &header))
        return NULL;

    // EMF bounds can start away from (0,0); their right/bottom are inclusive.
    LONGLONG width=LONGLONG(header.rclBounds.right)-header.rclBounds.left+1;
    LONGLONG height=LONGLONG(header.rclBounds.bottom)-header.rclBounds.top+1;
    if(width<=0 || height<=0 || width>32767 || height>32767) return NULL;
    INT cx=INT(width),cy=INT(height);CRect rc(0,0,cx,cy);
    HBITMAP hbm = CreateColorDIB(cx, cy, RGB(255, 255, 255));
    if (!hbm)
        return NULL;

    HDC hDC = CreateCompatibleDC(NULL);
    HGDIOBJ hbmOld = SelectObject(hDC, hbm);
    BOOL played=PlayEnhMetaFile(hDC, hEMF, &rc);
    SelectObject(hDC, hbmOld);
    DeleteDC(hDC);
    if(!played) { DeleteObject(hbm);return NULL; }
    return hbm;
}

BOOL IsBitmapBlackAndWhite(HBITMAP hbm)
{
    CWaitCursor waitCursor;

    BITMAP bm;
    if (!::GetObjectW(hbm, sizeof(bm), &bm))
        return FALSE;

    if (bm.bmBitsPixel == 1)
        return TRUE;

    BITMAPINFOEX bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = bm.bmWidth;
    bmi.bmiHeader.biHeight = bm.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;

    DWORD widthbytes = WIDTHBYTES(24 * bm.bmWidth);
    DWORD cbBits = widthbytes * bm.bmHeight;
    LPBYTE pbBits = new BYTE[cbBits];

    HDC hdc = ::CreateCompatibleDC(NULL);
    ::GetDIBits(hdc, hbm, 0, bm.bmHeight, pbBits, &bmi, DIB_RGB_COLORS);
    ::DeleteDC(hdc);

    BOOL bBlackAndWhite = TRUE;
    for (LONG y = 0; y < bm.bmHeight; ++y)
    {
        LPBYTE pbLine = &pbBits[widthbytes * y];
        for (LONG x = 0; x < bm.bmWidth; ++x)
        {
            BYTE Blue = *pbLine++;
            BYTE Green = *pbLine++;
            BYTE Red = *pbLine++;
            COLORREF rgbColor = RGB(Red, Green, Blue);
            if (rgbColor != RGB(0, 0, 0) && rgbColor != RGB(255, 255, 255))
            {
                bBlackAndWhite = FALSE;
                goto Finish;
            }
        }
    }

Finish:
    delete[] pbBits;

    return bBlackAndWhite;
}

HBITMAP ConvertToBlackAndWhite(HBITMAP hbm)
{
    if(BitmapHasAlpha(hbm))
    {
        RasterImage image;if(!ReadRaster(hbm,image)) return NULL;
        for(auto& p:image.pixels)
        {
            unsigned gray=299*(p>>16&255)+587*(p>>8&255)+114*(p&255);
            p=(p&0xff000000)|(gray>=128000?0xffffff:0);
        }
        return MakeRasterBitmap(image);
    }
    CWaitCursor waitCursor;

    BITMAP bm;
    if (!::GetObjectW(hbm, sizeof(bm), &bm))
        return NULL;

    BITMAPINFOEX bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bm.bmWidth;
    bmi.bmiHeader.biHeight = bm.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 1;
    bmi.bmiColorsExtra[0].rgbBlue = 255;
    bmi.bmiColorsExtra[0].rgbGreen = 255;
    bmi.bmiColorsExtra[0].rgbRed = 255;
    HDC hdc = ::CreateCompatibleDC(NULL);
    LPVOID pvMonoBits;
    HBITMAP hMonoBitmap = ::CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pvMonoBits, NULL, 0);
    if (!hMonoBitmap)
    {
        ::DeleteDC(hdc);
        return NULL;
    }

    HBITMAP hNewBitmap = CreateDIBWithProperties(bm.bmWidth, bm.bmHeight);
    if (hNewBitmap)
    {
        ::GetDIBits(hdc, hbm, 0, bm.bmHeight, pvMonoBits, &bmi, DIB_RGB_COLORS);
        ::SetDIBits(hdc, hNewBitmap, 0, bm.bmHeight, pvMonoBits, &bmi, DIB_RGB_COLORS);
    }
    ::DeleteObject(hMonoBitmap);
    ::DeleteDC(hdc);

    return hNewBitmap;
}
