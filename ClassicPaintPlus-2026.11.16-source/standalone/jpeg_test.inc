#include "jpeg.h"
#include "jpeg_resource.h"
#include <dlgs.h>

static HWND FindJpegDialogControl(int control)
{
    struct Search { int control;HWND window; } search={control,NULL};
    EnumThreadWindows(GetCurrentThreadId(),[](HWND window,LPARAM value)->BOOL
    {
        auto* s=reinterpret_cast<Search*>(value);
        if(GetDlgItem(window,s->control)) { s->window=window;return FALSE; }return TRUE;
    },reinterpret_cast<LPARAM>(&search));return search.window;
}
static int jpegDialogAction=0;
static VOID CALLBACK DriveJpegOptions(HWND,UINT,UINT_PTR timer,DWORD)
{
    HWND dialog=FindJpegDialogControl(IDC_JPEG_QUALITY);if(!dialog) return;
    KillTimer(NULL,timer);
    if(jpegDialogAction==0)
    {
        CaptureCanvasDialog(dialog,L"jpeg-options.bmp");
        SetDlgItemInt(dialog,IDC_JPEG_QUALITY,83,FALSE);
        RequireCanvas(SendDlgItemMessage(dialog,IDC_JPEG_SLIDER,TBM_GETPOS,0,0)==83,"JPEG edit synchronizes quality slider");
        SendDlgItemMessage(dialog,IDC_JPEG_PROGRESSIVE,BM_CLICK,0,0);
        RequireCanvas(!IsWindowEnabled(GetDlgItem(dialog,IDC_JPEG_OPTIMIZE)) && IsDlgButtonChecked(dialog,IDC_JPEG_OPTIMIZE)==BST_CHECKED,"progressive JPEG indicates optimized Huffman tables");
        SendDlgItemMessage(dialog,IDC_JPEG_VERTICAL,BM_CLICK,0,0);
        SendDlgItemMessage(dialog,IDC_JPEG_ALWAYS,BM_CLICK,0,0);
        SendDlgItemMessage(dialog,IDC_JPEG_DCF,BM_CLICK,0,0);
        SendDlgItemMessage(dialog,IDC_JPEG_DEFAULTS,BM_CLICK,0,0);
        PostMessage(dialog,WM_COMMAND,IDOK,0);
    }
    else if(jpegDialogAction==1)
    {
        SetDlgItemInt(dialog,IDC_JPEG_QUALITY,7,FALSE);
        SendDlgItemMessage(dialog,IDC_JPEG_NEVER,BM_CLICK,0,0);
        RequireCanvas(!IsWindowEnabled(GetDlgItem(dialog,IDC_JPEG_DCF)),"DCF is disabled when thumbnails are not changed");
        PostMessage(dialog,WM_COMMAND,IDCANCEL,0);
    }
    else
    {
        SendDlgItemMessage(dialog,IDC_JPEG_RESET,BM_CLICK,0,0);
        RequireCanvas(GetDlgItemInt(dialog,IDC_JPEG_QUALITY,NULL,FALSE)==90,"JPEG Reset restores factory quality");
        RequireCanvas(IsWindowEnabled(GetDlgItem(dialog,IDC_JPEG_OPTIMIZE)),"Reset restores baseline encoding");
        PostMessage(dialog,WM_COMMAND,IDOK,0);
    }
}

static int saveStage=0,saveTicks=0;
static HWND FindSaveHook()
{
    HWND result=NULL;
    EnumThreadWindows(GetCurrentThreadId(),[](HWND window,LPARAM value)->BOOL
    {
        EnumChildWindows(window,[](HWND child,LPARAM out)->BOOL
        {
            if(GetDlgItem(child,IDC_JPEG_OPTIONS)) { *reinterpret_cast<HWND*>(out)=child;return FALSE; }return TRUE;
        },value);return *reinterpret_cast<HWND*>(value)==NULL;
    },reinterpret_cast<LPARAM>(&result));return result;
}
static void SelectSaveFilter(HWND hook,bool jpeg)
{
    auto* ofn=reinterpret_cast<OPENFILENAMEW*>(GetWindowLongPtr(hook,DWLP_USER));
    OPENFILENAMEW copy=*ofn;
    for(int index=1;index<=8;++index)
    {
        copy.nFilterIndex=index;
        if(JpegFilterSelected(copy)==jpeg)
        {
            HWND parent=GetParent(hook),combo=GetDlgItem(parent,cmb1);
            SendMessage(combo,CB_SETCURSEL,index-1,0);
            SendMessage(parent,WM_COMMAND,MAKEWPARAM(cmb1,CBN_SELCHANGE),reinterpret_cast<LPARAM>(combo));
            SendMessage(parent,WM_COMMAND,MAKEWPARAM(cmb1,CBN_SELENDOK),reinterpret_cast<LPARAM>(combo));return;
        }
    }
    RequireCanvas(false,"Save As contains JPEG and other file types");
}
static VOID CALLBACK DriveJpegSave(HWND,UINT,UINT_PTR timer,DWORD)
{
    if(++saveTicks>100) ExitProcess(80);
    HWND hook=FindSaveHook();if(!hook) return;
    HWND parent=GetParent(hook);
    if(saveStage==0) { SelectSaveFilter(hook,true);saveStage=1;return; }
    if(saveStage==1)
    {
        CaptureCanvasDialog(parent,L"jpeg-save-as.bmp");
        RequireCanvas(IsWindowEnabled(GetDlgItem(hook,IDC_JPEG_OPTIONS)),"JPEG filter enables Options in real Save As");
        RECT optionRect,cancelRect;GetWindowRect(GetDlgItem(hook,IDC_JPEG_OPTIONS),&optionRect);GetWindowRect(GetDlgItem(parent,IDCANCEL),&cancelRect);
        RequireCanvas(optionRect.left==cancelRect.left && optionRect.right==cancelRect.right,"Options aligns with Save/Cancel buttons");
        RECT hookRect;GetWindowRect(hook,&hookRect);
        RequireCanvas(optionRect.left>=hookRect.left && optionRect.right<=hookRect.right && optionRect.bottom<=hookRect.bottom,"Options stays inside its template and is not clipped");
        CaptureCanvasDialog(parent,L"jpeg-save-as.bmp");saveStage=2;
        PostMessage(GetDlgItem(hook,IDC_JPEG_OPTIONS),BM_CLICK,0,0);return;
    }
    if(saveStage==2)
    {
        HWND options=FindJpegDialogControl(IDC_JPEG_QUALITY);if(!options) return;
        SetDlgItemInt(options,IDC_JPEG_QUALITY,64,FALSE);saveStage=3;PostMessage(options,WM_COMMAND,IDOK,0);return;
    }
    if(saveStage==3)
    {
        if(FindJpegDialogControl(IDC_JPEG_QUALITY)) return;
        SelectSaveFilter(hook,false);saveStage=4;return;
    }
    RequireCanvas(!IsWindowEnabled(GetDlgItem(hook,IDC_JPEG_OPTIONS)),"non-JPEG filter disables JPEG options");
    KillTimer(NULL,timer);PostMessage(parent,WM_COMMAND,IDCANCEL,0);
}

static void TestJpegAndSliders()
{
    for(UINT command:{IDM_PANEL_150,IDM_PANEL_200,IDM_PANEL_250})
    {
        mainWindow.SendMessage(WM_COMMAND,command);
        HWND slider=CreateWindowExW(0,TRACKBAR_CLASSW,L"",WS_CHILD|WS_VISIBLE|TBS_VERT,230,120,PanelPx(40),PanelPx(90),mainWindow,NULL,g_hinstExe,NULL);
        InstallPanelTrackbar(slider);SendMessage(slider,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
        RECT rect;GetClientRect(slider,&rect);
        for(int position:{0,8,17,30,55,78,100,60,16})
        {
            ValidateRect(slider,NULL);SendMessage(slider,TBM_SETPOS,TRUE,position);
            RECT dirty={};GetUpdateRect(slider,&dirty,FALSE);
            RequireCanvas(EqualRect(&dirty,&rect)!=FALSE,"every scaled thumb move invalidates the entire custom slider");
            UpdateWindow(slider);
        }
        TestBitmap before(rect.right,rect.bottom,RGB(255,0,0)),after(rect.right,rect.bottom,RGB(0,255,0));
        HDC dc=GetDC(slider);BitBlt(before.dc,0,0,rect.right,rect.bottom,dc,0,0,SRCCOPY);ReleaseDC(slider,dc);
        RedrawWindow(slider,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW);
        dc=GetDC(slider);BitBlt(after.dc,0,0,rect.right,rect.bottom,dc,0,0,SRCCOPY);ReleaseDC(slider,dc);
        RequireCanvas(FigurePixels(before.dc)==FigurePixels(after.dc),"slider after multiple moves equals a clean repaint without a blue trail");
        WCHAR path[80];swprintf_s(path,L"slider-fixed-%d.bmp",PanelPercent());CaptureCanvasDialog(slider,path);DestroyWindow(slider);
    }
    mainWindow.SendMessage(WM_COMMAND,IDM_PANEL_100);
    puts("PASS scaled slider: full invalidation, repeated moves and clean repaint at 150/200/250%");

    CreateDirectoryW(L"jpeg-tests",NULL);TestBitmap source(320,180,RGB(255,255,255));
    for(int y=0;y<180;++y) for(int x=0;x<320;++x)
        SetPixelV(source.dc,x,y,RGB((x*3+y*7)%256,(x+y*2)%256,(x*x+y*3)%256));
    for(int progressive=0;progressive<2;++progressive) for(int sampling=0;sampling<4;++sampling)
    {
        JpegOptions o;o.progressive=progressive!=0;o.horizontal=(sampling&1)!=0;o.vertical=(sampling&2)!=0;
        WCHAR path[100];swprintf_s(path,L"jpeg-tests/mode-%d-%d.jpg",progressive,sampling);
        RequireCanvas(SaveJpegBitmap(source.bitmap,path,o,{},96,144),"JPEG sampling/progressive variants save");
        CImageDx decoded;RequireCanvas(SUCCEEDED(decoded.LoadDx(path,NULL,NULL)),"GDI+ reopens encoded JPEG");
    }
    for(int quality:{0,10,90,100})
    {
        JpegOptions o;o.quality=quality;WCHAR path[100];swprintf_s(path,L"jpeg-tests/quality-%d.jpg",quality);
        RequireCanvas(SaveJpegBitmap(source.bitmap,path,o,{},96,144),"quality endpoints and intermediate values save");
    }
    JpegOptions o;o.optimize=false;
    RequireCanvas(SaveJpegBitmap(source.bitmap,L"jpeg-tests/unoptimized.jpg",o,{},96,144),"nonoptimized baseline saves");
    o.optimize=true;RequireCanvas(SaveJpegBitmap(source.bitmap,L"jpeg-tests/optimized.jpg",o,{},96,144),"optimized baseline saves");
    JpegMetadata metadata={{0xfe,{'P','a','i','n','t',' ','t','e','s','t'}}};
    o.thumbnails=1;o.dcf=true;
    RequireCanvas(SaveJpegBitmap(source.bitmap,L"jpeg-tests/with-thumb.jpg",o,metadata,96,144),"DCF thumbnail and comment save");
    metadata=ReadJpegMetadata(L"jpeg-tests/with-thumb.jpg");
    RequireCanvas(metadata.size()==2,"EXIF thumbnail and comment read from JPEG");
    TestBitmap changed(80,160,RGB(20,190,70));o.thumbnails=0;
    RequireCanvas(SaveJpegBitmap(changed.bitmap,L"jpeg-tests/updated-thumb.jpg",o,metadata,96,144),"only-existing mode updates a thumbnail");
    RequireCanvas(SaveJpegBitmap(changed.bitmap,L"jpeg-tests/no-original-thumb.jpg",o,{},96,144),"only-existing mode does not invent a thumbnail");
    o.thumbnails=2;
    RequireCanvas(SaveJpegBitmap(changed.bitmap,L"jpeg-tests/unchanged-thumb.jpg",o,metadata,96,144),"never mode preserves existing thumbnail unchanged");
    o.preserveMetadata=false;
    RequireCanvas(SaveJpegBitmap(changed.bitmap,L"jpeg-tests/stripped.jpg",o,metadata,96,144),"metadata can be omitted");
    o.thumbnails=1;o.dcf=false;
    RequireCanvas(SaveJpegBitmap(changed.bitmap,L"jpeg-tests/aspect-thumb.jpg",o,metadata,96,144),"ordinary thumbnail follows image aspect ratio");
    RequireCanvas(!SaveJpegBitmap(source.bitmap,L"missing-jpeg-directory/out.jpg",o,{},96,144),"failed output path returns failure");
    FILE* protectedFile=_wfopen(L"jpeg-tests/protected.jpg",L"wb");
    RequireCanvas(protectedFile!=NULL,"protected overwrite fixture created");fwrite("original",1,8,protectedFile);fclose(protectedFile);
    SetFileAttributesW(L"jpeg-tests/protected.jpg",FILE_ATTRIBUTE_READONLY);
    RequireCanvas(!SaveJpegBitmap(source.bitmap,L"jpeg-tests/protected.jpg",o,{},96,144),"read-only overwrite fails safely");
    SetFileAttributesW(L"jpeg-tests/protected.jpg",FILE_ATTRIBUTE_NORMAL);
    protectedFile=_wfopen(L"jpeg-tests/protected.jpg",L"rb");char originalBytes[9]={};fread(originalBytes,1,8,protectedFile);fclose(protectedFile);
    RequireCanvas(!strcmp(originalBytes,"original"),"failed JPEG replacement leaves the original file intact");DeleteFileW(L"jpeg-tests/protected.jpg");
    o=JpegOptions();o.thumbnails=0;o.dcf=true;size_t previousSize=0;
    for(int i=0;i<8;++i)
    {
        RequireCanvas(SaveJpegBitmap(changed.bitmap,L"jpeg-tests/repeated.jpg",o,metadata,96,144),"repeated thumbnail update saves");
        metadata=ReadJpegMetadata(L"jpeg-tests/repeated.jpg");
        size_t size=0;for(const auto& marker:metadata) size+=marker.data.size();
        if(i) RequireCanvas(size==previousSize,"repeated saves do not accumulate unused EXIF thumbnail IFDs");previousSize=size;
    }
    JpegMarker be={0xe1,{'E','x','i','f',0,0,'M','M',0,42,0,0,0,8,
        0,1,1,14,0,2,0,0,0,11,0,0,0,26,0,0,0,0,
        'P','a','i','n','t',' ','n','o','t','e',0}};
    o.thumbnails=1;RequireCanvas(SaveJpegBitmap(source.bitmap,L"jpeg-tests/big-endian.jpg",o,{be},96,144),"big-endian EXIF survives adding a thumbnail");
    JpegMarker jfif={0xe0,{'J','F','I','F',0,1,1,1,0,72,0,72,1,1,255,0,0}};
    o.thumbnails=2;RequireCanvas(SaveJpegBitmap(source.bitmap,L"jpeg-tests/jfif-unchanged.jpg",o,{jfif},96,144),"never mode keeps JFIF thumbnails");
    auto jfifMetadata=ReadJpegMetadata(L"jpeg-tests/jfif-unchanged.jpg");
    RequireCanvas(jfifMetadata.size()==1 && jfifMetadata[0].code==0xe0,"JFIF thumbnail is detected when opening a JPEG");
    o.thumbnails=0;RequireCanvas(SaveJpegBitmap(changed.bitmap,L"jpeg-tests/jfif-updated.jpg",o,jfifMetadata,96,144),"only-existing mode also updates JFIF thumbnails");
    SetCurrentJpegOptions(JpegOptions());
    RequireCanvas(SaveDIBToFile(source.bitmap,L"jpeg-tests/app-save.jpg",FALSE),"application SaveDIB path uses the new JPEG encoder");
    puts("PASS JPEG encoding: 4 sampling modes, baseline/progressive, quality, Huffman, thumbnail policies and metadata outputs");

    o=JpegOptions();
    for(int action:{0,1,2})
    {
        jpegDialogAction=action;SetTimer(NULL,0,100,DriveJpegOptions);
        bool accepted=ShowJpegOptions(mainWindow,o);
        RequireCanvas(accepted==(action!=1),"JPEG options accepts or cancels correctly");
        if(action==0)
        {
            RequireCanvas(o.quality==83 && o.progressive && o.vertical && o.dcf && o.thumbnails==1,"JPEG options apply all controls");
            auto defaults=JpegOptions::LoadDefaults();
            RequireCanvas(defaults.quality==83 && defaults.progressive && defaults.vertical && defaults.dcf && defaults.thumbnails==1,"JPEG defaults round trip through the private config.ini");
        }
        if(action==1) RequireCanvas(o.quality==83 && o.thumbnails==1,"Cancel retains previous JPEG options");
    }
    JpegOptions saved=CurrentJpegOptions();SetCurrentJpegOptions(JpegOptions());
    WCHAR filename[MAX_PATH]=L"jpeg-tests\\save-as.png";saveStage=0;saveTicks=0;SetTimer(NULL,0,100,DriveJpegSave);
    RequireCanvas(!mainWindow.GetSaveFileName(filename,_countof(filename)),"Save As cancels without writing a file");
    RequireCanvas(saveStage==4 && CurrentJpegOptions().quality==90,"canceling Save As discards its temporary JPEG choices");
    SetCurrentJpegOptions(saved);
    for(WORD language:testLanguages) for(int index=0;index<21;++index)
        RequireCanvas(!ExactString(language,IDS_JPEG_BASE+index).IsEmpty(),"all JPEG strings have a translation in every application language");
    puts("PASS JPEG UI: slider/edit, Reset, Cancel, config.ini defaults, real Save As options button and file-type switching, translations");
}
