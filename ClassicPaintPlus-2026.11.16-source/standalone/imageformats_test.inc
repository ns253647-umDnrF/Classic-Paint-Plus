#include "imageformats.h"
#include "imageformats_resource.h"

static int formatDialogKind=0,formatDialogAction=0;
static VOID CALLBACK DriveFormatOptions(HWND,UINT,UINT_PTR timer,DWORD)
{
    HWND dialog=FindJpegDialogControl(formatDialogKind==0?IDC_PNG_INTERLACE:formatDialogKind==1?IDC_WEBP_LOSSLESS:IDC_ICO_SIZE);
    if(!dialog) return;KillTimer(NULL,timer);
    if(formatDialogKind==0)
    {
        CaptureCanvasDialog(dialog,L"format-tests/png-options.bmp");
        if(formatDialogAction==0)
        {
            SendDlgItemMessage(dialog,IDC_PNG_INTERLACE,BM_CLICK,0,0);SendDlgItemMessage(dialog,IDC_PNG_OPTIMIZE,BM_CLICK,0,0);
            SendDlgItemMessage(dialog,IDC_FORMAT_DEFAULTS,BM_CLICK,0,0);
        }
        else SendDlgItemMessage(dialog,IDC_FORMAT_RESET,BM_CLICK,0,0);
    }
    else if(formatDialogKind==1)
    {
        RequireCanvas(!IsWindowEnabled(GetDlgItem(dialog,IDC_FORMAT_QUALITY)),"WebP quality disabled for lossless");
        SendDlgItemMessage(dialog,IDC_WEBP_LOSSLESS,BM_CLICK,0,0);SetDlgItemInt(dialog,IDC_FORMAT_QUALITY,83,FALSE);
        RequireCanvas(SendDlgItemMessage(dialog,IDC_FORMAT_SLIDER,TBM_GETPOS,0,0)==83,"WebP edit controls quality slider");
        CaptureCanvasDialog(dialog,L"format-tests/webp-options.bmp");SendDlgItemMessage(dialog,IDC_FORMAT_DEFAULTS,BM_CLICK,0,0);
    }
    else
    {
        HWND combo=GetDlgItem(dialog,IDC_ICO_SIZE);SendMessage(combo,CB_SETCURSEL,1,0);
        SendMessage(dialog,WM_COMMAND,MAKEWPARAM(IDC_ICO_SIZE,CBN_SELCHANGE),reinterpret_cast<LPARAM>(combo));
        CaptureCanvasDialog(dialog,formatDialogKind==2?L"format-tests/ico-options.bmp":L"format-tests/ico-select.bmp");
    }
    PostMessage(dialog,WM_COMMAND,formatDialogAction==1?IDCANCEL:IDOK,0);
}
static int formatSaveStage=0,formatSaveTicks=0;
static VOID CALLBACK DriveFormatSave(HWND,UINT,UINT_PTR timer,DWORD)
{
    if(++formatSaveTicks>200) ExitProcess(86);
    HWND hook=FindSaveHook();if(!hook) return;HWND parent=GetParent(hook);
    if(formatSaveStage<24)
    {
        int index=formatSaveStage/2;
        if(!(formatSaveStage%2))
        {
            HWND combo=GetDlgItem(parent,cmb1);SendMessage(combo,CB_SETCURSEL,index,0);
            SendMessage(parent,WM_COMMAND,MAKEWPARAM(cmb1,CBN_SELCHANGE),reinterpret_cast<LPARAM>(combo));
            SendMessage(parent,WM_COMMAND,MAKEWPARAM(cmb1,CBN_SELENDOK),reinterpret_cast<LPARAM>(combo));
        }
        else
        {
            bool expected=index==4 || index==5 || index==8 || index==9 || index==11;
            RequireCanvas(!!IsWindowEnabled(GetDlgItem(hook,IDC_JPEG_OPTIONS))==expected,"Options state matches all twelve save formats");
            auto* ofn=reinterpret_cast<OPENFILENAMEW*>(GetWindowLongPtr(hook,DWLP_USER));
            RequireCanvas(ofn->nFilterIndex==index+1,"file type index is synchronized with native Save As");
        }
        ++formatSaveStage;return;
    }
    if(formatSaveStage==24)
    {
        HWND combo=GetDlgItem(parent,cmb1);SendMessage(combo,CB_SETCURSEL,5,0);
        SendMessage(parent,WM_COMMAND,MAKEWPARAM(cmb1,CBN_SELCHANGE),reinterpret_cast<LPARAM>(combo));
        SendMessage(parent,WM_COMMAND,MAKEWPARAM(cmb1,CBN_SELENDOK),reinterpret_cast<LPARAM>(combo));++formatSaveStage;return;
    }
    if(formatSaveStage==25)
    { CaptureCanvasDialog(parent,L"format-tests/png-save-as.bmp");++formatSaveStage;PostMessage(GetDlgItem(hook,IDC_JPEG_OPTIONS),BM_CLICK,0,0);return; }
    if(formatSaveStage==26)
    {
        HWND dialog=FindJpegDialogControl(IDC_PNG_INTERLACE);if(!dialog) return;
        SendDlgItemMessage(dialog,IDC_PNG_OPTIMIZE,BM_CLICK,0,0);PostMessage(dialog,WM_COMMAND,IDOK,0);++formatSaveStage;return;
    }
    if(FindJpegDialogControl(IDC_PNG_INTERLACE)) return;
    KillTimer(NULL,timer);PostMessage(parent,WM_COMMAND,IDCANCEL,0);
}
static void TestFormatDialogs()
{
    PngOptions png;
    for(int action:{0,1,2})
    {
        formatDialogKind=0;formatDialogAction=action;SetTimer(NULL,0,100,DriveFormatOptions);
        RequireCanvas(ShowPngOptions(mainWindow,png)==(action!=1),"PNG dialog OK and Cancel");
        RequireCanvas(png.optimize==(action!=2) && png.interlaced==(action!=2),"PNG checkbox/Reset/Cancel state");
        auto saved=PngOptions::LoadDefaults();RequireCanvas(saved.optimize && saved.interlaced,"PNG explicit defaults saved to private INI");
    }
    WebpOptions webp;formatDialogKind=1;formatDialogAction=0;SetTimer(NULL,0,100,DriveFormatOptions);
    RequireCanvas(ShowWebpOptions(mainWindow,webp) && !webp.lossless && webp.quality==83,"WebP dialog options");
    auto defaults=WebpOptions::LoadDefaults();RequireCanvas(!defaults.lossless && defaults.quality==83,"WebP defaults saved to private INI");
    IcoOptions ico;formatDialogKind=2;SetTimer(NULL,0,100,DriveFormatOptions);
    RequireCanvas(ShowIcoOptions(mainWindow,ico) && ico.size==16,"ICO size combo");
    std::vector<BYTE> bytes;RasterImage pixels;ImageFileInfo info;ReadImageBytes(L"format-tests/multi.ico",bytes);DecodeIco(bytes,pixels,info);
    formatDialogKind=3;SetTimer(NULL,0,100,DriveFormatOptions);
    RequireCanvas(SelectIconFrame(mainWindow,info)==1,"ICO frame selection and preview");
    SetCurrentPngOptions(PngOptions{});SetCurrentWebpOptions(WebpOptions{});
    WCHAR file[MAX_PATH]=L"format-tests\\ui.png";formatSaveStage=0;formatSaveTicks=0;SetTimer(NULL,0,50,DriveFormatSave);
    RequireCanvas(!mainWindow.GetSaveFileName(file,_countof(file)) && formatSaveStage==27,"real Save As PNG options and Cancel");
    RequireCanvas(!CurrentPngOptions().optimize,"canceling Save As discards staged PNG settings");
    if(testLanguages.empty()) EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    for(WORD language:testLanguages) for(UINT id=IDS_PNG_TITLE;id<=IDS_ICON_FILE;++id)
        RequireCanvas(!ExactString(language,id).IsEmpty(),"new format strings translated into every language");
    puts("PASS PNG/WebP/ICO dialogs, config.ini defaults, Reset/Cancel, native Save As routing across twelve formats and translations");
}

static RasterImage DocumentRaster()
{
    RasterImage result;RequireCanvas(ReadRaster(reinterpret_cast<HBITMAP>(GetCurrentObject(imageModel.GetDC(),OBJ_BITMAP)),result),"read document RGBA");return result;
}
static RasterImage LoadFixture(LPCWSTR path)
{
    RasterImage image;ImageFileInfo info;RequireCanvas(LoadCustomImage(path,image,info),"decode independent fixture");return image;
}
static void TestTransparentDrawing()
{
    RasterImage empty;empty.width=empty.height=32;empty.pixels.assign(1024,0x000000ff);
    for(int tool=0;tool<7;++tool)
    {
        HBITMAP bitmap=MakeRasterBitmap(empty);HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,bitmap);
        switch(tool)
        {
        case 0: Line(dc,5,5,20,20,RGB(0,0,255),1);break;
        case 1: Brush(dc,5,5,20,20,RGB(0,0,255),BrushStyleRound,4);break;
        case 2: Rect(dc,5,5,20,20,RGB(0,0,255),RGB(0,0,255),1,1);break;
        case 3: Ellp(dc,5,5,20,20,RGB(0,0,255),RGB(0,0,255),1,1);break;
        case 4: Airbrush(dc,12,12,RGB(0,0,255),5);break;
        case 5: Erase(dc,5,5,20,20,RGB(0,0,255),3);break;
        case 6: Fill(dc,5,5,RGB(0,0,255));break;
        }
        RasterImage painted;ReadRaster(bitmap,painted);size_t opaque=0;
        for(DWORD pixel:painted.pixels) { if(pixel>>24) ++opaque;RequireCanvas((pixel&0xffffff)==255,"same RGB drawing does not invent another color"); }
        if(tool==6) RequireCanvas(opaque==1024,"flood fill with same RGB makes transparent pixels opaque");
        else RequireCanvas(opaque>0 && painted.pixels.back()==empty.pixels.back(),"drawing tools update opacity only in painted area");
        SelectObject(dc,old);DeleteDC(dc);DeleteObject(bitmap);
    }
    toolsModel.OnEndDraw(TRUE);toolsModel.SetActiveTool(TOOL_RECTSEL);toolsModel.SetBackgroundTransparent(FALSE);
    RasterImage patch;patch.width=patch.height=8;patch.pixels.assign(64,0x80ff0000);patch.pixels[0]=0xffffffff;patch.pixels[7]=0x0000ff00;
    HBITMAP bitmap=MakeTransparentBitmap(32,32),part=MakeRasterBitmap(patch);
    putSubImage(bitmap,CRect(0,0,8,8),part);DeleteObject(part);
    imageModel.PushImageForUndo(bitmap);imageModel.ClearHistory();
    selectionModel.SetRectFromPoints({0,0},{8,8});selectionModel.m_bShow=TRUE;selectionModel.TakeOff();
    HBITMAP copied=selectionModel.GetSelectionContents();RasterImage pixels;ReadRaster(copied,pixels);DeleteObject(copied);
    RequireCanvas(pixels.pixels==patch.pixels,"lifting/copying alpha selection preserves white and hidden RGB");
    selectionModel.moveSelection(12,12);selectionModel.Landing();pixels=DocumentRaster();
    RequireCanvas((pixels.pixels[0]>>24)==0 && pixels.pixels[12*32+12]==0xffffffff && pixels.pixels[13*32+13]==0x80ff0000,
        "moving alpha selection keeps partial opacity, opaque white and transparent original area");
    imageModel.Undo();RequireCanvas(DocumentRaster().pixels[0]==0xffffffff,"selection move Undo restores alpha image");
    for(int mode:{0,1,2})
    {
        part=MakeRasterBitmap(patch);HBITMAP target=MakeTransparentBitmap(16,16);HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,target);
        SelectionResampler sampler;RequireCanvas(sampler.Draw(dc,CRect(0,0,16,16),part,NULL,CLR_INVALID,mode,255),"alpha selection resamples");
        ReadRaster(target,pixels);RequireCanvas((pixels.pixels[8*16+8]>>24)>=126 && (pixels.pixels[8*16+8]>>24)<=129,"resampling keeps partial opacity on transparent canvas");
        SelectObject(dc,old);DeleteDC(dc);DeleteObject(target);DeleteObject(part);
    }
    puts("PASS actual drawing/fill on transparent pixels, selection white/partial alpha, move/Undo and all resampling modes");
}
static void TestImageFormats()
{
    puts("Checking PNG/WebP/ICO codecs and document transparency...");
    for(LPCWSTR name:{L"rgba",L"rgb-key",L"palette-alpha",L"gray-key"})
    {
        CStringW path=L"format-tests/";path+=name;RasterImage source=LoadFixture(path+L".png");
        std::vector<BYTE> expected;RequireCanvas(ReadImageBytes(path+L".rgba",expected),"read independent RGBA reference");
        RequireCanvas(expected.size()==source.pixels.size()*4,"decoded dimensions match independent reader");
        for(size_t i=0;i<source.pixels.size();++i)
            RequireCanvas(source.pixels[i]==(DWORD(expected[i*4+3])<<24|DWORD(expected[i*4])<<16|DWORD(expected[i*4+1])<<8|expected[i*4+2]),"PNG tRNS/gray/palette/partial alpha decoded exactly");
        HBITMAP bitmap=MakeRasterBitmap(source);RasterImage copied;RequireCanvas(ReadRaster(bitmap,copied) && copied.pixels==source.pixels,"internal BGRT conversion preserves all RGBA bits");
        HGLOBAL clip=BitmapToClipboardDIBV5(bitmap);HBITMAP pasted=BitmapFromClipboardDIB(clip);
        RequireCanvas(ReadRaster(pasted,copied) && copied.pixels==source.pixels,"V5 clipboard preserves straight alpha and RGB exactly");
        DeleteObject(pasted);GlobalFree(clip);
        clip=BitmapToClipboardDIB(bitmap);pasted=BitmapFromClipboardDIB(clip);
        RasterImage flat=source;FlattenRaster(flat);
        RequireCanvas(ReadRaster(pasted,copied) && copied.pixels==flat.pixels,"legacy clipboard flattens onto white");DeleteObject(pasted);GlobalFree(clip);
        for(bool interlaced:{false,true})
        {
            PngOptions options;options.interlaced=interlaced;std::vector<BYTE> bytes;
            RequireCanvas(EncodePng(source,options,bytes,144,72) && bytes[28]==int(interlaced),"PNG Adam7 flag is encoded");
            ImageFileInfo info;RequireCanvas(DecodePng(bytes,copied,&info) && copied.pixels==source.pixels,"PNG lossless round trip preserves hidden colors and opacity");
            RequireCanvas(abs(info.xDpi-144)<.1 && abs(info.yDpi-72)<.1,"PNG resolution survives");
            RequireCanvas(WriteImageBytes(path+(interlaced?L"-interlaced.png":L"-roundtrip.png"),bytes),"write PNG output for independent check");
        }
        DeleteObject(bitmap);
    }
    puts("PASS PNG RGB color key, indexed tRNS, grayscale key, partial/zero alpha, Adam7, DPI and clipboard round trips");
    RasterImage rgba=LoadFixture(L"format-tests/rgba.png"),decoded;
    std::vector<BYTE> bytes;
    WebpOptions webp;
    RequireCanvas(EncodeWebp(rgba,webp,bytes) && DecodeWebp(bytes,decoded) && decoded.pixels==rgba.pixels,"WebP lossless preserves all RGBA bytes");
    WriteImageBytes(L"format-tests/output.webp",bytes);webp.lossless=false;webp.quality=82;
    RequireCanvas(EncodeWebp(rgba,webp,bytes) && DecodeWebp(bytes,decoded),"WebP lossy color with alpha encodes");
    for(size_t i=0;i<rgba.pixels.size();++i) RequireCanvas((rgba.pixels[i]>>24)==(decoded.pixels[i]>>24),"lossy WebP keeps exact alpha");
    WriteImageBytes(L"format-tests/output-lossy.webp",bytes);
    ImageFileInfo animated;RequireCanvas(ReadImageBytes(L"format-tests/animated.webp",bytes) && DecodeWebp(bytes,decoded,&animated) && animated.animated,"animated WebP is identified for safe first-frame editing");
    for(int depth:{1,4,8,24})
    {
        RequireCanvas(EncodeBmp(rgba,depth,bytes,96,96) && bytes[28]==depth,"BMP header has requested bit depth");
        CStringW path;path.Format(L"format-tests/output-%d.bmp",depth);WriteImageBytes(path,bytes);
        HBITMAP bmp=DoLoadImageFile(mainWindow,path,FALSE);RequireCanvas(bmp && !BitmapHasAlpha(bmp),"all BMP variants reopen opaque");DeleteObject(bmp);
    }
    for(int depth:{1,4,8,24,32})
    {
        CStringW path;path.Format(L"format-tests/legacy-%d.ico",depth);RasterImage icon=LoadFixture(path);
        RequireCanvas(icon.width==16 && icon.height==16 && !(icon.pixels[0]>>24) && (icon.pixels[5]>>24)>0,"legacy ICO depth and AND mask decode");
    }
    for(int size:{0,16,32,256})
    {
        IcoOptions ico;ico.size=size;ImageFileInfo info;
        RequireCanvas(EncodeIco(rgba,ico,NULL,bytes) && DecodeIco(bytes,decoded,info),"ICO DIB and PNG payloads encode/decode");
        if(!size) RequireCanvas(decoded.pixels==rgba.pixels,"ICO preserves partial alpha and hidden RGB without resize");
        else RequireCanvas(decoded.width==size && decoded.height==size,"ICO size and aspect padding");
        CStringW path;path.Format(L"format-tests/output-%d.ico",size);WriteImageBytes(path,bytes);
        HICON handle=(HICON)LoadImageW(NULL,path,IMAGE_ICON,0,0,LR_LOADFROMFILE);
        RequireCanvas(handle!=NULL,"Windows loads the saved icon");DestroyIcon(handle);
    }
    ImageFileInfo multi;RequireCanvas(ReadImageBytes(L"format-tests/multi.ico",bytes) && DecodeIco(bytes,decoded,multi),"multi-image ICO opens");
    RequireCanvas(multi.icons.size()==3,"all ICO variants are retained");multi.selectedIcon=1;
    auto untouched0=multi.icons[0].encoded,untouched2=multi.icons[2].encoded;
    IcoOptions ico;ico.size=32;
    RequireCanvas(EncodeIco(rgba,ico,&multi,bytes),"edit one ICO image");
    ImageFileInfo updated;RequireCanvas(DecodeIco(bytes,decoded,updated,1) && updated.icons.size()==3 && updated.icons[0].encoded==untouched0 && updated.icons[2].encoded==untouched2,"other ICO images remain byte-for-byte intact");
    WriteImageBytes(L"format-tests/multi-edited.ico",bytes);
    puts("PASS BMP 1/4/8/24, WebP lossless/lossy/alpha, ICO legacy/PNG/native Windows loading and preservation of other frames");

    toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();toolsModel.SetActiveTool(TOOL_PEN);
    imageModel.PushImageForUndo(MakeRasterBitmap(rgba));imageModel.ClearHistory();
    imageModel.PushImageForUndo(CRect(0,0,1,1));SetPixelV(imageModel.GetDC(),0,0,RGB(17,43,99));
    auto edited=DocumentRaster();RequireCanvas(edited.pixels[0]==0xff112b63,"GDI pencil writes opaque color on a transparent pixel");
    for(size_t i=1;i<edited.pixels.size();++i) RequireCanvas(edited.pixels[i]==rgba.pixels[i],"editing does not change opacity elsewhere");
    imageModel.Undo();RequireCanvas(DocumentRaster().pixels==rgba.pixels,"Undo restores alpha exactly");
    imageModel.Redo();RequireCanvas(DocumentRaster().pixels==edited.pixels,"Redo restores alpha exactly");
    imageModel.InvertColors();auto inverted=DocumentRaster();
    for(size_t i=0;i<edited.pixels.size();++i) RequireCanvas(inverted.pixels[i]==(edited.pixels[i]^0xffffff),"invert changes RGB only");
    imageModel.Undo();imageModel.RotateNTimes90Degrees(1);imageModel.RotateNTimes90Degrees(3);
    RequireCanvas(DocumentRaster().pixels==edited.pixels,"rotation preserves all RGBA values");
    imageModel.FlipHorizontally();imageModel.FlipHorizontally();imageModel.FlipVertically();imageModel.FlipVertically();
    RequireCanvas(DocumentRaster().pixels==edited.pixels,"flips preserve opacity");
    HBITMAP snapshot=imageModel.CopyBitmap();SetCurrentPngOptions(PngOptions{});
    RequireCanvas(SaveDIBToFile(snapshot,L"format-tests/edited.png",FALSE),"application PNG save path");DeleteObject(snapshot);
    RequireCanvas(LoadFixture(L"format-tests/edited.png").pixels==edited.pixels,"edit-save-reopen does not flatten alpha");
    snapshot=MakeRasterBitmap(rgba);
    RequireCanvas(SaveDIBToFile(snapshot,L"format-tests/alpha-export.gif",FALSE) &&
        SaveDIBToFile(snapshot,L"format-tests/alpha-export.tif",FALSE) &&
        SaveDIBToFile(snapshot,L"format-tests/alpha-export.jpg",FALSE),"legacy export accepts RGBA canvas");DeleteObject(snapshot);
    imageModel.Crop(31,23,2,3);auto cropped=DocumentRaster();
    RequireCanvas(cropped.pixels[0]==edited.pixels[3*edited.width+2],"cropping preserves alpha");
    imageModel.Undo();RequireCanvas(DocumentRaster().pixels==edited.pixels,"crop Undo restores RGBA");
    puts("PASS document RGBA edit/save/reopen, undo/redo, inversion, rotation, flips and crop");
    TestTransparentDrawing();

    RasterImage sample=LoadFixture(L"png_example_ms_paint.PNG"),original=sample;
    PngOptions opt;opt.optimize=true;DWORD start=GetTickCount();
    RequireCanvas(EncodePng(sample,opt,bytes),"optimized example encodes");
    printf("Optimized sample: %zu bytes, %lu ms, PNG color type=%u\n",bytes.size(),GetTickCount()-start,bytes[25]);
    RequireCanvas(sample.pixels==original.pixels,"quantization never changes its input canvas");
    WriteImageBytes(L"format-tests/example-optimized.png",bytes);
    RequireCanvas(DecodePng(bytes,decoded) && decoded.width==1920,"optimized sample reopens as full RGBA");
    HBITMAP opened=DoLoadImageFile(mainWindow,L"png_example_tinypng.PNG",TRUE);
    RequireCanvas(opened && GetDIBWidth(opened)==1920,"actual TinyPNG example opens into document");
    RasterImage before=DocumentRaster();SetPixelV(imageModel.GetDC(),0,0,RGB(7,19,233));
    RequireCanvas(DocumentRaster().pixels[0]==0xff0713e9,"indexed PNG accepts an arbitrary new RGB color");
    snapshot=imageModel.CopyBitmap();SetCurrentPngOptions(opt);
    RequireCanvas(SaveDIBToFile(snapshot,L"format-tests/edit-optimized.png",FALSE),"optimized document export");
    RequireCanvas(DocumentRaster().pixels[0]==0xff0713e9,"optimized save leaves full-color document unchanged");
    SetCurrentPngOptions(PngOptions{});
    RequireCanvas(SaveDIBToFile(snapshot,L"format-tests/edit-fullcolor.png",FALSE),"lossless export after optimized save");DeleteObject(snapshot);
    RequireCanvas(LoadFixture(L"format-tests/edit-fullcolor.png").pixels[0]==0xff0713e9,"new color survives lossless save/reopen");
    for(size_t n:{size_t(0),size_t(6),size_t(30)})
    {
        std::vector<BYTE> bad(n,0xff);ImageFileInfo info;
        RequireCanvas(!DecodePng(bad,decoded) && !DecodeWebp(bad,decoded) && !DecodeIco(bad,decoded,info),"truncated files reject safely");
    }
    std::vector<BYTE> guard={1,2,3,4};WriteImageBytes(L"format-tests/protected.png",guard);SetFileAttributesW(L"format-tests/protected.png",FILE_ATTRIBUTE_READONLY);
    RequireCanvas(!WriteImageBytes(L"format-tests/protected.png",bytes),"read-only replacement is rejected");SetFileAttributesW(L"format-tests/protected.png",FILE_ATTRIBUTE_NORMAL);
    std::vector<BYTE> check;ReadImageBytes(L"format-tests/protected.png",check);RequireCanvas(check==guard,"failed replacement preserves original");
    puts("PASS TinyPNG input remains full-color, optimization only changes exported file, malformed inputs and atomic replacement");
    TestFormatDialogs();
}
