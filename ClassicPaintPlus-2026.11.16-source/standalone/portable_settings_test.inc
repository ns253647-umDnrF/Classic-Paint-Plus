static void TestPortableSettings()
{
    // Never touch the user's real INI or registry when checking a first launch.
    CStringW originalPath=RegistrySettings::ConfigPath();WCHAR temporary[MAX_PATH],fresh[MAX_PATH];
    GetTempPathW(_countof(temporary),temporary);RequireCanvas(GetTempFileNameW(temporary,L"rpi",0,fresh)!=0,"allocate isolated first-run config");
    DeleteFileW(fresh);RegistrySettings::SetConfigPathForTests(fresh);
    RegistrySettings firstRun=registrySettings;
    firstRun.PanelMagnification=250;firstRun.PaletteType=PAL_MONOCHROME;firstRun.DarkTheme=1;firstRun.UILanguage=0x411;
    firstRun.ReturnAfterColorPick=1;firstRun.strFontName=L"Old settings";firstRun.strFiles[0]=L"old-document.png";
    firstRun.Load(SW_HIDE);
    RequireCanvas(firstRun.PanelMagnification==100 && firstRun.PaletteType==PAL_OLDTYPE && firstRun.DarkTheme==0 && firstRun.UILanguage==0 && !firstRun.ReturnAfterColorPick,"missing config uses built-in defaults with OS-language detection pending");
    for(const auto& file:firstRun.strFiles) RequireCanvas(file.IsEmpty(),"fresh settings contain no inherited recent documents");
    LOGFONTW defaultFont={};GetObjectW(GetStockObject(DEFAULT_GUI_FONT),sizeof(defaultFont),&defaultFont);
    RequireCanvas(firstRun.strFontName==defaultFont.lfFaceName && GetFileAttributesW(fresh)!=INVALID_FILE_ATTRIBUTES,"first-run config is created from the default font and preferences");
    WritePrivateProfileStringW(L"Settings",L"PaletteType",L"",fresh);
    WritePrivateProfileStringW(L"Settings",L"PanelMagnification",L"200",fresh);
    firstRun.PaletteType=PAL_MONOCHROME;firstRun.Load(SW_HIDE);
    RequireCanvas(firstRun.PaletteType==PAL_OLDTYPE && firstRun.PanelMagnification==200,"missing individual INI values use defaults while present values are retained");
    WritePrivateProfileStringW(NULL,NULL,NULL,fresh);DeleteFileW(fresh);firstRun.strFiles[0]=L"stale.png";
    firstRun.Load(SW_HIDE);
    RequireCanvas(firstRun.PanelMagnification==100 && firstRun.strFiles[0].IsEmpty(),"removing config resets preferences and recent files on the next load");
    WritePrivateProfileStringW(NULL,NULL,NULL,fresh);DeleteFileW(fresh);RegistrySettings::SetConfigPathForTests(originalPath);
    const UINT commands[]={IDM_COLORSMODERNPALETTE,IDM_COLORSOLDPALETTE,IDM_COLORSGRAYSCALE,IDM_COLORSMONOCHROME};
    for(int i=0;i<4;++i)
    {
        mainWindow.SendMessage(WM_COMMAND,commands[i]);
        RegistrySettings loaded;loaded.Load(SW_HIDE);
        RequireCanvas(loaded.PaletteType==DWORD(PAL_MODERN+i),"every palette mode survives a fresh config load");
        paletteModel.SelectPalette(static_cast<PAL_TYPE>(loaded.PaletteType));
        RequireCanvas(paletteModel.SelectedPalette()==PAL_MODERN+i,"startup restores the saved palette mode");
    }
    RegistrySettings saved=registrySettings;
    registrySettings.PanelMagnification=150;registrySettings.ToolboxTopMargin=73;
    registrySettings.CanvasMarkerSize=11;registrySettings.CanvasMarkerColor=RGB(123,21,231);
    registrySettings.ReturnAfterColorPick=TRUE;registrySettings.SelectionInterpolation=1;
    registrySettings.FigureKind=FIG_HEXAGON;registrySettings.ShapeFromCenter=TRUE;registrySettings.FigureRounding=67;
    registrySettings.ShowToolBox=FALSE;registrySettings.ShowPalette=FALSE;registrySettings.ShowStatusBar=FALSE;
    registrySettings.ThumbXPos=123;registrySettings.ThumbYPos=234;registrySettings.ThumbWidth=345;
    registrySettings.Bold=TRUE;registrySettings.Italic=TRUE;registrySettings.Underline=TRUE;registrySettings.PointSize=27;
    registrySettings.strFontName=L"Тестовый шрифт 日本語";
    registrySettings.strFiles[0]=L"E:\\Рисунки\\日本語\\Проба.png";
    registrySettings.WindowPlacement.rcNormalPosition.left=-1920;
    registrySettings.StorePreferences();
    RegistrySettings loaded;loaded.Load(SW_HIDE);
    RequireCanvas(loaded.PanelMagnification==150 && loaded.ToolboxTopMargin==73 && loaded.CanvasMarkerSize==11 &&
        loaded.CanvasMarkerColor==RGB(123,21,231),"panel and marker settings read from portable INI");
    RequireCanvas(loaded.ReturnAfterColorPick && loaded.SelectionInterpolation==1 && loaded.FigureKind==FIG_HEXAGON &&
        loaded.ShapeFromCenter && loaded.FigureRounding==0,"tool preferences read from portable INI");
    RequireCanvas(!loaded.ShowToolBox && !loaded.ShowPalette && !loaded.ShowStatusBar &&
        loaded.ThumbXPos==123 && loaded.ThumbYPos==234 && loaded.ThumbWidth==345,"window visibility and miniature settings read from INI");
    RequireCanvas(loaded.Bold && loaded.Italic && loaded.Underline && loaded.PointSize==27 &&
        loaded.strFontName==registrySettings.strFontName && loaded.strFiles[0]==registrySettings.strFiles[0] &&
        loaded.WindowPlacement.rcNormalPosition.left==-1920,"Unicode font/file names and negative monitor coordinates round-trip");
    WritePrivateProfileStringW(L"Settings",L"PanelMagnification",L"300",RegistrySettings::ConfigPath());
    WritePrivateProfileStringW(L"Settings",L"PaletteType",L"0",RegistrySettings::ConfigPath());
    loaded.Load(SW_HIDE);
    RequireCanvas(loaded.PanelMagnification==100 && loaded.PaletteType==PAL_OLDTYPE,"invalid INI values use safe defaults");
    registrySettings=saved;registrySettings.StorePreferences();
    puts("PASS portable INI only: first launch/reset/partial file defaults, all palette modes, panel/tool/window/font settings, Unicode paths and invalid values");
}

static void TestNestedMenuEditing()
{
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);SetActiveWindow(mainWindow);
    static int phase,attempts;phase=0;attempts=0;
    UINT_PTR timer=SetTimer(NULL,0,100,[](HWND,UINT,UINT_PTR timer,DWORD)
    {
        RequireCanvas(++attempts<50,"nested menu editing completes within timeout");
        if(phase==0)
        {
            for(UINT key:{VK_DOWN,VK_DOWN,VK_RIGHT}) PostMessage(mainWindow,WM_KEYDOWN,key,0);
            phase=1;return;
        }
        HWND edit=FindMenuEdit();if(!edit) return;
        if(phase==1)
        {
            PostMessage(mainWindow,WM_CHAR,L'7',0);PostMessage(mainWindow,WM_CHAR,L'2',0);phase=2;return;
        }
        if(phase==2)
        {
            WCHAR text[16];GetWindowTextW(edit,text,_countof(text));
            RequireCanvas(!lstrcmpW(text,L"72"),"characters are routed to the real inline editor");
            int count=0;HWND menu=NULL;
            while((menu=FindWindowExW(NULL,menu,L"#32768",NULL))!=NULL) if(IsWindowVisible(menu)) ++count;
            RequireCanvas(count>=2,"parent settings menu stays visible while typing in the submenu");
            PostMessage(mainWindow,WM_KEYDOWN,VK_RETURN,0);phase=3;return;
        }
        RequireCanvas(ToolboxTopMargin()==72 && IsWindowVisible(GetParent(edit)),"Enter applies offset without closing the menu chain");
        // Save both actual native menu windows in one review image.
        RECT total={LONG_MAX,LONG_MAX,LONG_MIN,LONG_MIN};HWND menu=NULL;
        while((menu=FindWindowExW(NULL,menu,L"#32768",NULL))!=NULL) if(IsWindowVisible(menu))
        { RECT r;GetWindowRect(menu,&r);UnionRect(&total,&total,&r); }
        TestBitmap capture(total.right-total.left,total.bottom-total.top,GetSysColor(COLOR_MENU));
        menu=NULL;
        while((menu=FindWindowExW(NULL,menu,L"#32768",NULL))!=NULL) if(IsWindowVisible(menu))
        {
            RECT r;GetWindowRect(menu,&r);POINT old;SetViewportOrgEx(capture.dc,r.left-total.left,r.top-total.top,&old);
            PrintWindow(menu,capture.dc,0);SetViewportOrgEx(capture.dc,old.x,old.y,NULL);
        }
        CImage image;image.Attach(capture.bitmap);image.Save(L"settings-inline-menu.png");image.Detach();
        KillTimer(NULL,timer);EndMenu();phase=4;
    });
    TrackPopupMenu(GetSubMenu(mainWindow.GetMenu(),4),TPM_LEFTALIGN|TPM_TOPALIGN,200,100,0,mainWindow,NULL);
    KillTimer(NULL,timer);RequireCanvas(phase==4,"whole settings menu was edited successfully");
    registrySettings.ToolboxTopMargin=40;registrySettings.StorePreferences();ApplyPanelScale();
    puts("PASS nested native menu: typed digits, parent menu remains visible, Enter commits without dismissal");
}
