static void TestVisiblePasteAndNavigation()
{
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);SetActiveWindow(mainWindow);
    mainWindow.SetWindowPos(NULL,0,0,1000,850,SWP_NOACTIVATE|SWP_NOZORDER);
    toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();toolsModel.SetActiveTool(TOOL_RECTSEL);
    toolsModel.SetBackgroundTransparent(FALSE);
    SetBitmapAndInfo(CreateColorDIB(2400,1800,RGB(255,255,255)),NULL,NULL,FALSE);
    const POINT scrolls[]={{0,0},{1,0},{0,1},{3,3},{4,3},{3,4},{137,0},{0,137},{800,831}};
    for(int zoom:{125,333,1000,2250,8000}) for(POINT scroll:scrolls)
    {
        canvasWindow.zoomTo(zoom);canvasWindow.updateScrollPos(scroll.x,scroll.y);
        POINT canvasOrigin={};canvasWindow.ImageToCanvas(canvasOrigin);
        int x=canvasWindow.GetScrollPos(SB_HORZ),y=canvasWindow.GetScrollPos(SB_VERT);
        RasterImage pixels;pixels.width=3;pixels.height=2;pixels.pixels={0xffff0000,0x8000ff00,0x000000ff,0xff000000,0xffffffff,0xff123456};
        mainWindow.PasteBitmapForTests(MakeRasterBitmap(pixels));
        RequireCanvas(selectionModel.m_bShow && selectionModel.m_rc.Width()==3 && selectionModel.m_rc.Height()==2,"paste retains original image dimensions");
        POINT p={selectionModel.m_rc.left,selectionModel.m_rc.top};canvasWindow.ImageToCanvas(p);
        for(int axis=0;axis<2;++axis)
        {
            int edge=axis?canvasOrigin.y:canvasOrigin.x,screen=axis?p.y:p.x;
            int coordinate=axis?selectionModel.m_rc.top:selectionModel.m_rc.left;
            if(edge>=0) RequireCanvas(coordinate==0 && screen==edge,"visible canvas edges receive no paste inset");
            else
            {
                int expected=int((LONGLONG(3-edge)*1000+zoom-1)/zoom);
                RequireCanvas(coordinate==expected && screen>=3 && screen<3+(zoom+999)/1000,"hidden canvas edges receive a three-screen-pixel inset rounded to image pixels independently on each axis");
            }
        }
        RequireCanvas(canvasWindow.GetScrollPos(SB_HORZ)==x && canvasWindow.GetScrollPos(SB_VERT)==y,"paste does not jump back to image origin");
        HBITMAP copy=selectionModel.GetSelectionContents();RasterImage actual;ReadRaster(copy,actual);DeleteObject(copy);
        RequireCanvas(actual.pixels==pixels.pixels,"pasted fragment preserves source colours and alpha");
        toolsModel.OnEndDraw(TRUE);
    }
    const WPARAM keys[]={VK_HOME,VK_END,VK_PRIOR,VK_NEXT};
    for(int zoom:{125,1000,8000})
    {
        canvasWindow.zoomTo(zoom);selectionModel.setMask(CRect(10,20,80,70),NULL);selectionModel.m_bShow=TRUE;
        RECT rect=selectionModel.m_rc;auto original=DocumentRaster().pixels;
        for(int corner=0;corner<4;++corner) for(int route=0;route<3;++route)
        {
            canvasWindow.updateScrollPos(71,59);
            if(route==0) canvasWindow.SendMessage(WM_KEYDOWN,keys[corner],0);
            if(route==1) mainWindow.SendMessage(WM_KEYDOWN,keys[corner],0);
            if(route==2) mainWindow.SendMessage(WM_COMMAND,IDM_SCROLL_CORNER_FIRST+corner,0);
            SCROLLINFO sx={sizeof(sx),SIF_ALL},sy={sizeof(sy),SIF_ALL};canvasWindow.GetScrollInfo(SB_HORZ,&sx);canvasWindow.GetScrollInfo(SB_VERT,&sy);
            RequireCanvas(sx.nPos==(corner&2?max(0,sx.nMax-int(sx.nPage)+1):0) && sy.nPos==(corner&1?max(0,sy.nMax-int(sy.nPage)+1):0),"all four keyboard/menu destinations match scroll limits");
            RequireCanvas(EqualRect(&rect,&selectionModel.m_rc) && selectionModel.m_bShow && DocumentRaster().pixels==original,"navigation preserves selection and document");
        }
        selectionModel.HideSelection();
    }
    SetBitmapAndInfo(CreateColorDIB(10000,8,RGB(255,255,255)),NULL,NULL,FALSE);canvasWindow.zoomTo(8000);
    canvasWindow.SendMessage(WM_KEYDOWN,VK_PRIOR,0);RequireCanvas(canvasWindow.GetScrollPos(SB_HORZ)>65535,"corner scrolling is not truncated to 16-bit positions");
    canvasWindow.ScrollToCorner(0);canvasWindow.m_drawing=TRUE;canvasWindow.ScrollToCorner(3);
    RequireCanvas(canvasWindow.GetScrollPos(SB_HORZ)==0,"navigation does not interfere with a drawing gesture");canvasWindow.m_drawing=FALSE;
    for(WORD language: {WORD(0x0409),WORD(0x0419)})
    {
        registrySettings.UILanguage=language;ApplyPreferredLanguage();HMENU menu=CreatePopupMenu();mainWindow.AppendCanvasNavigationMenu(menu);
        for(int i=0;i<4;++i) { WCHAR text[160];GetMenuStringW(menu,IDM_SCROLL_CORNER_FIRST+i,text,160,MF_BYCOMMAND);RequireCanvas(wcslen(text)>8 && wcschr(text,L'\t'),"navigation menu captions include shortcuts"); }
        DestroyMenu(menu);
    }
    std::vector<WORD> languages;testLanguages.clear();EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    for(WORD language:testLanguages) for(int i=0;i<4;++i) RequireCanvas(!ExactString(language,IDS_SCROLL_CORNER_FIRST+i).IsEmpty(),"all 30 navigation translations are compiled");
    PrepareTextRegression(20);NewRegressionText();textEditWindow.SetWindowTextW(L"abc");textEditWindow.SendMessage(EM_SETSEL,3,3);
    textEditWindow.SendMessage(WM_KEYDOWN,VK_HOME,0);DWORD first=9,last=9;textEditWindow.SendMessage(EM_GETSEL,WPARAM(&first),LPARAM(&last));
    RequireCanvas(first==0 && last==0,"Home remains text navigation in the text editor");toolsModel.OnEndDraw(TRUE);
    toolsModel.SetActiveTool(TOOL_RECTSEL);InitializeImage(NULL,NULL,FALSE);
    puts("PASS visible paste/navigation: fractional zoom, alpha, no scroll jump, 4 corners through 3 routes, large ranges, unchanged selection, edit keys and 30 locales");
}

static void TestBrushSessionDefaults()
{
    for(LPCWSTR key:{L"BrushSize",L"BrushShape",L"BrushHardness",L"EraserSize",L"EraserShape",L"EraserHardness"})
        WritePrivateProfileStringW(L"Settings",key,L"33",RegistrySettings::ConfigPath());
    RegistrySettings fresh;fresh.Load(SW_HIDE);
    RequireCanvas(fresh.BrushSize==4 && fresh.BrushShape==0 && fresh.BrushHardness==100 && fresh.EraserSize==8 && fresh.EraserShape==1 && fresh.EraserHardness==100,"legacy stored values do not override startup brush/eraser defaults");
    toolsModel.SetActiveTool(TOOL_BRUSH);toolsModel.SetBrushWidth(31);toolsModel.SetBrushStyle(BrushStyleRound);registrySettings.BrushHardness=45;
    toolsModel.SetActiveTool(TOOL_RUBBER);toolsModel.SetEraserSize(63);toolsModel.SetEraserShape(3);registrySettings.EraserHardness=20;
    toolsModel.SetActiveTool(TOOL_BRUSH);registrySettings.StorePreferences();
    RequireCanvas(toolsModel.GetBrushWidth()==31 && registrySettings.BrushHardness==45 && toolsModel.GetEraserSize()==63 && registrySettings.EraserHardness==20,"brush and eraser retain independent current-session values");
    fresh.Load(SW_HIDE);RequireCanvas(fresh.BrushSize==4 && fresh.BrushHardness==100 && fresh.EraserSize==8 && fresh.EraserShape==1 && fresh.EraserHardness==100,"relaunch restores defaults after changed session values");
    for(LPCWSTR key:{L"BrushSize",L"BrushShape",L"BrushHardness",L"EraserSize",L"EraserShape",L"EraserHardness"})
    { WCHAR value[32];GetPrivateProfileStringW(L"Settings",key,L"missing",value,32,RegistrySettings::ConfigPath());RequireCanvas(!wcscmp(value,L"missing"),"obsolete brush/eraser INI keys removed"); }
    toolsModel.SetBrushWidth(4);toolsModel.SetBrushStyle(BrushStyleRound);registrySettings.BrushHardness=100;
    toolsModel.SetEraserSize(8);toolsModel.SetEraserShape(1);registrySettings.EraserHardness=100;
    puts("PASS brush defaults: ignore legacy INI, current-session independence, no persistence, round 4px / square 8px at 100%");
}

static void TestHardBrushEdges()
{
    const POINT points[]={{270,270},{301,289},{305,315}};
    for(int shape=0;shape<4;++shape) for(int size:{1,2,4,7,8,21,40,500}) for(bool erase:{false,true})
    {
        std::vector<BYTE> previous(600*600,0);
        for(int hardness:{0,20,50,80,99,100})
        {
            TestBitmap bitmap(600,600,RGB(255,255,255));
            DrawSoftBrushStroke(bitmap.dc,points,3,shape,size,hardness,RGB(0,0,0),erase?0:255);
            RasterImage pixels;ReadRaster(bitmap.bitmap,pixels);int changed=0;
            for(size_t i=0;i<pixels.pixels.size();++i)
            {
                unsigned cov=erase?255-(pixels.pixels[i]>>24):255-(pixels.pixels[i]&255);
                RequireCanvas(cov>=previous[i],"hardness is monotonic at every pixel for every shape and size");
                if(hardness==100) RequireCanvas(cov==0 || cov==255,"100 percent hardness has no translucent or blended edge pixels");
                previous[i]=BYTE(cov);if(cov) ++changed;
            }
            RequireCanvas(changed>0,"every brush size produces a mark");
        }
    }
    for(int size:{1,2,4,8,21,500})
    {
        TestBitmap bitmap(600,600,RGB(255,255,255));DrawSoftBrushStroke(bitmap.dc,points,1,1,size,100,0,255);
        RasterImage pixels;ReadRaster(bitmap.bitmap,pixels);int changed=0;for(DWORD p:pixels.pixels) if((p&0xffffff)==0) ++changed;
        RequireCanvas(changed==size*size,"hard square footprint is exactly size by size, for odd and even sizes");
    }
    TestBitmap half(600,600,RGB(255,255,255));DrawSoftBrushStroke(half.dc,points,1,0,40,100,0,128);
    RequireCanvas(GetRValue(GetPixel(half.dc,270,270))==127,"hardness keeps explicit paint opacity independent");
    puts("PASS hardness: round/square/slanted, odd/even 1..500px, 0/20/50/80/99/100%, monotonic paint/erase coverage, exact square sizes and independent opacity");
}

static void CaptureTransparencyArtwork()
{
    CreateDirectoryW(L"navigation-brush-tests",NULL);
    for(int percent:{100,150,200,250}) for(int dark:{0,1})
    {
        registrySettings.PanelMagnification=percent;registrySettings.DarkTheme=dark;ApplyTheme();ApplyPanelScale();
        toolsModel.SetActiveTool(TOOL_RECTSEL);selectionModel.HideSelection();
        for(int transparent:{0,1})
        {
            toolsModel.SetBackgroundTransparent(transparent);CStringW file;file.Format(L"navigation-brush-tests/panel-%d-%d-%d.bmp",percent,dark,transparent);
            CaptureCanvasDialog(toolSettingsWindow,file);
        }
    }
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();
    puts("PASS supplied transparency artwork captured at all four panel scales and both themes");
}
