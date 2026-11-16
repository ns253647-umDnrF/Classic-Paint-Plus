static int ddsChoice=0,ddsAction=0,ddsTicks=0;
static VOID CALLBACK DriveDdsOptions(HWND,UINT,UINT_PTR timer,DWORD)
{
    RequireCanvas(++ddsTicks<100,"DDS options test timeout");HWND dialog=FindJpegDialogControl(IDC_DDS_COMPRESSION);if(!dialog) return;
    KillTimer(NULL,timer);HWND combo=GetDlgItem(dialog,IDC_DDS_COMPRESSION);
    RequireCanvas(SendMessage(combo,CB_GETCOUNT,0,0)==4,"DDS dialog has all four compression modes");
    SendMessage(combo,CB_SETCURSEL,ddsChoice,0);SendMessage(dialog,WM_COMMAND,MAKEWPARAM(IDC_DDS_COMPRESSION,CBN_SELCHANGE),LPARAM(combo));
    if(ddsAction==2) SendDlgItemMessage(dialog,IDC_FORMAT_RESET,BM_CLICK,0,0);
    if(ddsAction==3) SendDlgItemMessage(dialog,IDC_FORMAT_DEFAULTS,BM_CLICK,0,0);
    CaptureCanvasDialog(dialog,registrySettings.DarkTheme?L"dds-options-tests/dialog-dark.bmp":L"dds-options-tests/dialog-light.bmp");
    PostMessage(dialog,WM_COMMAND,ddsAction==1?IDCANCEL:IDOK,0);
}
struct MenuTextSnapshot { HMENU menu;UINT position;CStringW text;UINT id; };
static void SnapshotMenuText(HMENU menu,std::vector<MenuTextSnapshot>& snapshot)
{
    for(int i=0;i<GetMenuItemCount(menu);++i)
    {
        MENUITEMINFOW item={sizeof(item)};WCHAR label[512];item.fMask=MIIM_FTYPE|MIIM_STRING|MIIM_SUBMENU|MIIM_ID;
        item.dwTypeData=label;item.cch=_countof(label);GetMenuItemInfoW(menu,i,TRUE,&item);
        if(!(item.fType&MFT_SEPARATOR) && label[0]) snapshot.push_back({menu,UINT(i),label,item.wID});
        if(item.hSubMenu) SnapshotMenuText(item.hSubMenu,snapshot);
    }
}
static int themeMenuIndex=0,themeMenuTicks=0;
static VOID CALLBACK CaptureThemeMenu(HWND,UINT,UINT_PTR timer,DWORD)
{
    RequireCanvas(++themeMenuTicks<100,"native themed popup test timeout");
    HWND menu=FindWindowExW(NULL,NULL,L"#32768",NULL);if(!menu || !IsWindowVisible(menu)) return;
    CStringW path;path.Format(L"dds-options-tests/menu-%d-%d.bmp",themeMenuIndex,int(registrySettings.DarkTheme));
    CaptureCanvasDialog(menu,path);
    if(!registrySettings.DarkTheme)
    {
        RECT bounds;GetWindowRect(menu,&bounds);TestBitmap capture(bounds.right-bounds.left,bounds.bottom-bounds.top,RGB(255,255,255));PrintWindow(menu,capture.dc,0);
        int visible=0;for(int y=3;y<bounds.bottom-bounds.top-3;++y) for(int x=25;x<bounds.right-bounds.left-45;++x)
            if(GetRValue(GetPixel(capture.dc,x,y))<100) ++visible;
        RequireCanvas(visible>50,"light popup menu text is actually painted, not only stored in item metadata");
    }
    KillTimer(NULL,timer);EndMenu();
}
static int ddsSaveStage=0,ddsSaveTicks=0;
static VOID CALLBACK DriveDdsSave(HWND,UINT,UINT_PTR timer,DWORD)
{
    if(++ddsSaveTicks==599) printf("DDS save timeout stage=%d action=%d choice=%d\n",ddsSaveStage,ddsAction,ddsChoice);
    RequireCanvas(ddsSaveTicks<600,"DDS Save As test timeout");HWND hook=FindSaveHook();if(!hook) return;
    if(ddsSaveStage==0)
    {
        auto* ofn=reinterpret_cast<OPENFILENAMEW*>(GetWindowLongPtr(hook,DWLP_USER));
        RequireCanvas(ofn && ofn->nFilterIndex==12 && IsWindowEnabled(GetDlgItem(hook,IDC_JPEG_OPTIONS)),"DDS file type enables real Options button");
        PostMessage(GetDlgItem(hook,IDC_JPEG_OPTIONS),BM_CLICK,0,0);++ddsSaveStage;return;
    }
    if(ddsSaveStage==1)
    {
        HWND dialog=FindJpegDialogControl(IDC_DDS_COMPRESSION);if(!dialog) return;
        SendDlgItemMessage(dialog,IDC_DDS_COMPRESSION,CB_SETCURSEL,ddsChoice,0);
        SendMessage(dialog,WM_COMMAND,MAKEWPARAM(IDC_DDS_COMPRESSION,CBN_SELCHANGE),LPARAM(GetDlgItem(dialog,IDC_DDS_COMPRESSION)));
        PostMessage(dialog,WM_COMMAND,IDOK,0);++ddsSaveStage;return;
    }
    if(FindJpegDialogControl(IDC_DDS_COMPRESSION)) return;
    if(ddsSaveStage==2) { ++ddsSaveStage;PostMessage(GetParent(hook),WM_COMMAND,ddsAction==1?IDCANCEL:IDOK,0); }
}
static void TestDdsOptionsAndTheme()
{
    WCHAR workingDirectory[MAX_PATH];GetCurrentDirectoryW(_countof(workingDirectory),workingDirectory);
    CreateDirectoryW(L"dds-options-tests",NULL);toolsModel.OnEndDraw(TRUE);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    puts("Checking DDS compression and alpha...");
    for(int width:{1,3,4,37,64})
    {
        int height=width==64?32:width==37?23:5;RasterImage source;source.width=width;source.height=height;source.pixels.resize(size_t(width)*height);
        for(int y=0;y<height;++y) for(int x=0;x<width;++x)
        { unsigned a=(x+y)*255/(width+height-2);source.pixels[size_t(y)*width+x]=a<<24|DWORD(x*255/max(1,width-1))<<16|DWORD(y*255/(height-1))<<8|128; }
        for(int mode=0;mode<4;++mode)
        {
            DdsOptions options;options.compression=mode;std::vector<BYTE> encoded;RasterImage decoded;ImageFileInfo info;
            printf("DDS mode=%d %dx%d\n",mode,width,height);
            RequireCanvas(EncodeDds(source,encoded,options),"DDS encoder accepts requested mode and non-block-aligned dimensions");
            RequireCanvas(DecodeDds(encoded,decoded,&info) && decoded.width==width && decoded.height==height && info.ddsCompression==mode,"DDS output advertises selected compression and exact dimensions");
            size_t blocks=size_t((width+3)/4)*((height+3)/4);size_t header=encoded[84]=='D' && encoded[86]=='1' && encoded[87]=='0'?148:128;
            RequireCanvas(encoded.size()==header+(mode?blocks*(mode==1?8:16):source.pixels.size()*4),"DDS block size agrees with DXT1/3/5 compression");
            for(size_t i=0;i<source.pixels.size();++i)
            {
                unsigned a=source.pixels[i]>>24,b=decoded.pixels[i]>>24;
                if(!mode) RequireCanvas(source.pixels[i]==decoded.pixels[i],"uncompressed DDS is bit-exact RGBA");
                else if(mode==1) { if(b!=(a<128?0:255)) printf("DXT1 alpha: source=%u decoded=%u pixel=%u\n",a,b,unsigned(i));RequireCanvas(b==(a<128?0:255),"DXT1 supports binary alpha"); }
                else RequireCanvas(abs(int(a)-int(b))<=(mode==2?8:20),"DXT3/5 alpha retains expected precision");
            }
            CStringW path;path.Format(L"dds-options-tests/encoded-%d-%d.dds",width,mode);WriteImageBytes(path,encoded);
            std::vector<BYTE> png;EncodePng(decoded,PngOptions(),png);path.Format(L"dds-options-tests/decoded-%d-%d.png",width,mode);WriteImageBytes(path,png);
        }
    }
    DdsOptions invalid;invalid.compression=4;std::vector<BYTE> bad;RasterImage original=LoadFixture(L"texture-tests/source.png");
    RequireCanvas(!EncodeDds(original,bad,invalid),"invalid DDS mode rejected");
    puts("PASS all four DDS encodings, odd sizes, alpha semantics and independently readable outputs");
    registrySettings.DarkTheme=0;ApplyTheme();DdsOptions defaults;defaults.SaveDefaults();SetCurrentDdsOptions(defaults);
    for(int action:{0,1,2,3})
    {
        printf("DDS dialog action=%d\n",action);
        DdsOptions options;options.compression=2;ddsChoice=3;ddsAction=action;ddsTicks=0;
        registrySettings.DarkTheme=action==3;ApplyTheme();SetTimer(NULL,0,60,DriveDdsOptions);
        RequireCanvas(ShowDdsOptions(mainWindow,options)==(action!=1),"DDS options OK and Cancel");
        RequireCanvas(options.compression==(action==1?2:action==2?0:3),"DDS selection Reset and Cancel semantics");
        RequireCanvas(DdsOptions::LoadDefaults().compression==(action==3?3:0),"only explicit DDS defaults are written to config.ini");
    }
    WritePrivateProfileStringW(L"DDS",L"Compression",L"99",RegistrySettings::ConfigPath());RequireCanvas(DdsOptions::LoadDefaults().compression==0,"invalid DDS INI option defaults to uncompressed");defaults.SaveDefaults();
    registrySettings.DarkTheme=0;ApplyTheme();imageModel.PushImageForUndo(MakeRasterBitmap(original));imageModel.ClearHistory();
    for(int mode=0;mode<4;++mode)
    {
        printf("DDS native save mode=%d\n",mode);
        CStringW relative;relative.Format(L"dds-options-tests/native-%d.dds",mode);WCHAR path[MAX_PATH];GetFullPathNameW(relative,_countof(path),path,NULL);DeleteFileW(path);
        ddsChoice=mode;ddsAction=0;ddsSaveStage=ddsSaveTicks=0;UINT_PTR timer=SetTimer(NULL,0,60,DriveDdsSave);
        BOOL saved=mainWindow.GetSaveFileName(path,_countof(path));KillTimer(NULL,timer);SetCurrentDirectoryW(workingDirectory);
        RequireCanvas(saved && ddsSaveStage==3,"Save As accepts DDS options and filename");
        RequireCanvas(CurrentDdsOptions().compression==mode && SaveDIBToFile(HBITMAP(GetCurrentObject(imageModel.GetDC(),OBJ_BITMAP)),path,TRUE),"DDS options reach native save path");
        ImageFileInfo info;RasterImage reopened;RequireCanvas(LoadCustomImage(path,reopened,info) && info.ddsCompression==mode,"real saved DDS uses selected compression");
        RequireCanvas(DocumentRaster().pixels==original.pixels,"lossy DDS export does not quantize editing canvas");
        RequireCanvas(DoLoadImageFile(mainWindow,path,TRUE)!=NULL && CurrentDdsOptions().compression==mode,"reopening DDS retains compression for Ctrl+S");
        imageModel.PushImageForUndo(MakeRasterBitmap(original));imageModel.ClearHistory();
    }
    DdsOptions current;current.compression=1;SetCurrentDdsOptions(current);ddsChoice=3;ddsAction=1;ddsSaveStage=ddsSaveTicks=0;
    WCHAR cancelled[MAX_PATH]=L"dds-options-tests/cancelled.dds";UINT_PTR timer=SetTimer(NULL,0,60,DriveDdsSave);
    BOOL saved=mainWindow.GetSaveFileName(cancelled,_countof(cancelled));KillTimer(NULL,timer);SetCurrentDirectoryW(workingDirectory);
    RequireCanvas(!saved && CurrentDdsOptions().compression==1,"Save As Cancel leaves current DDS settings unchanged");
    SetCurrentDdsOptions(defaults);defaults.SaveDefaults();
    if(testLanguages.empty()) EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    for(WORD lang:testLanguages) for(UINT id=IDS_DDS_TITLE;id<=IDS_DDS_DESCRIPTION;++id) RequireCanvas(!ExactString(lang,id).IsEmpty(),"DDS options translated in all 30 locales");
    puts("PASS DDS dialog, defaults, Reset/Cancel, native Save As and Ctrl+S settings, translations");

    std::vector<MenuTextSnapshot> baseline;SnapshotMenuText(mainWindow.GetMenu(),baseline);
    RequireCanvas(baseline.size()>50,"theme tests cover all nested menu captions");
    themeMenuIndex=9;themeMenuTicks=0;SetTimer(NULL,0,60,CaptureThemeMenu);TrackPopupMenu(GetSubMenu(mainWindow.GetMenu(),4),TPM_LEFTALIGN|TPM_TOPALIGN,200,100,0,mainWindow,NULL);
    for(const auto& expected:baseline) RequireCanvas(!expected.text.IsEmpty(),"native baseline menu labels are nonempty");
    for(int cycle=0;cycle<8;++cycle)
    {
        mainWindow.SendMessage(WM_COMMAND,IDM_THEME_DARK,0);
        // Simulate Windows losing the strings on owner-drawn Image/Settings
        // items: cached labels must restore both top-level and nested text.
        for(int index:{3,4})
        {
            HMENU menu=GetSubMenu(mainWindow.GetMenu(),index);
            for(int i=0;i<GetMenuItemCount(menu);++i)
            { MENUITEMINFOW item={sizeof(item)};item.fMask=MIIM_STRING;item.dwTypeData=const_cast<LPWSTR>(L"");SetMenuItemInfoW(menu,i,TRUE,&item); }
            themeMenuIndex=index;themeMenuTicks=0;SetTimer(NULL,0,60,CaptureThemeMenu);
            TrackPopupMenu(menu,TPM_LEFTALIGN|TPM_TOPALIGN,200,100,0,mainWindow,NULL);
        }
        mainWindow.SendMessage(WM_COMMAND,IDM_THEME_LIGHT,0);
        for(const auto& expected:baseline)
        {
            MENUITEMINFOW item={sizeof(item)};WCHAR actual[512];item.fMask=MIIM_FTYPE|MIIM_STRING|MIIM_DATA;item.dwTypeData=actual;item.cch=_countof(actual);
            GetMenuItemInfoW(expected.menu,expected.position,TRUE,&item);
            RequireCanvas(expected.text==actual,"light theme retains all native menu labels after repeated dark/light switches");
        }
        for(int index:{3,4}) { themeMenuIndex=index;themeMenuTicks=0;SetTimer(NULL,0,60,CaptureThemeMenu);TrackPopupMenu(GetSubMenu(mainWindow.GetMenu(),index),TPM_LEFTALIGN|TPM_TOPALIGN,200,100,0,mainWindow,NULL); }
    }
    TestNestedMenuEditing();
    for(int percent:{100,150,200,250})
    {
        registrySettings.PanelMagnification=percent;registrySettings.DarkTheme=1;ApplyTheme();ApplyPanelScale();
        int side=MulDiv(16,percent,100);HIMAGELIST images=CreatePanelToolIcons(percent);TestBitmap strip(side*16,side,RGB(0,0,0));
        for(int tool=0;tool<16;++tool) ImageList_Draw(images,tool,strip.dc,tool*side,0,ILD_TRANSPARENT);ImageList_Destroy(images);
        CStringW path;path.Format(L"dds-options-tests/icons-%d.png",percent);CImage picture;picture.Attach(strip.bitmap);picture.Save(path);picture.Detach();
        for(int tool:{0,1,9,10,11})
        { int bright=0;for(int y=0;y<side;++y) for(int x=tool*side;x<(tool+1)*side;++x) if(GetRValue(GetPixel(strip.dc,x,y))>96) ++bright;printf("Dark icon %d%% tool=%d bright=%d\n",percent,tool,bright);RequireCanvas(bright>5,"selection/text/line/curve icons have visible white strokes at all panel scales"); }
        path.Format(L"dds-options-tests/window-dark-%d.bmp",percent);CaptureCanvasDialog(mainWindow,path);
    }
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();
    puts("PASS menu text after eight theme round trips, editable submenus and white icon strokes at all four scales");
}
