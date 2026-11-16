static VOID CALLBACK InspectSelectionMenu(HWND,UINT,UINT_PTR timer,DWORD)
{
    HWND window=FindWindowExW(NULL,NULL,L"#32768",NULL);if(!window) return;
    HMENU menu=HMENU(SendMessage(window,0x01e1,0,0));if(!menu) return;
    KillTimer(NULL,timer);WCHAR caption[160];CStringW expected;expected.LoadString(IDS_CV_CROP_SELECTION);
    GetMenuStringW(menu,IDM_CROPSELECTION,caption,_countof(caption),MF_BYCOMMAND);
    RequireCanvas(expected==caption && !(GetMenuState(menu,IDM_CROPSELECTION,MF_BYCOMMAND)&MF_GRAYED),"selection context menu contains enabled localized crop command");
    CaptureCanvasDialog(window,L"texture-tests/selection-menu.bmp");EndMenu();
}
static bool multiImageNotice=false;static int noticeTicks=0;
static VOID CALLBACK AcceptMultiImageNotice(HWND,UINT,UINT_PTR timer,DWORD)
{
    RequireCanvas(++noticeTicks<100,"multi-image notice appears in time");
    HWND dialog=NULL;
    EnumThreadWindows(GetCurrentThreadId(),[](HWND window,LPARAM data)->BOOL
    {
        if(!IsWindowVisible(window) || GetWindow(window,GW_OWNER)!=mainWindow.m_hWnd) return TRUE;
        CStringW expected;expected.LoadString(IDS_MULTI_IMAGE_FIRST);
        for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
        { WCHAR text[1024];GetWindowTextW(child,text,_countof(text));if(expected==text) { *reinterpret_cast<HWND*>(data)=window;return FALSE; } }
        return TRUE;
    },LPARAM(&dialog));
    if(!dialog) return;
    multiImageNotice=true;
    if(noticeTicks==1) { puts("Verified multi-image notice");CaptureCanvasDialog(dialog,L"texture-tests/multi-image-notice.bmp"); }
    // MessageBox buttons on a noninteractive desktop can defer BM_CLICK.
    // End its modal loop directly after verifying the actual localized text.
    EndDialog(dialog,IDOK);
}
static void TestBrushAndFormats()
{
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);toolsModel.OnEndDraw(TRUE);
    puts("Checking compact brush controls and ordinary dark-theme icons...");
    toolsModel.SetActiveTool(TOOL_BRUSH);
    for(int percent:{100,150,200,250})
    {
        registrySettings.PanelMagnification=percent;ApplyPanelScale();
        std::vector<COLORREF> lightIcons;
        for(int dark:{0,1})
        {
            registrySettings.DarkTheme=dark;ApplyTheme();
            HIMAGELIST icons=CreatePanelToolIcons(percent);int side=MulDiv(16,percent,100);TestBitmap strip(side*16,side,RGB(255,255,255));
            for(int i=0;i<16;++i) ImageList_Draw(icons,i,strip.dc,side*i,0,ILD_NORMAL);ImageList_Destroy(icons);
            for(int y=0;y<side;++y) for(int x=0;x<side*16;++x)
            {
                int tool=x/side;COLORREF pixel=GetPixel(strip.dc,x,y);
                if(!dark) lightIcons.push_back(pixel);
                else if(tool!=0 && tool!=1 && tool!=3 && tool!=9 && tool!=10 && tool!=11)
                    RequireCanvas(pixel==lightIcons[y*side*16+x],"coloured tool icons remain identical in both themes");
            }
            HWND panel=GetDlgItem(toolSettingsWindow,IDC_BRUSH_PANEL),list=GetDlgItem(panel,IDC_BRUSH_PRESET_FIRST);
            for(int shape=0;shape<4;++shape)
            {
                HWND button=GetDlgItem(panel,IDC_BRUSH_SHAPE_FIRST+shape);SendMessage(button,BM_CLICK,0,0);
                RequireCanvas(toolsModel.GetBrushStyle()==shape,"actual shape button selects exactly its shape");
                RedrawWindow(panel,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
                RequireCanvas(SendMessage(list,LB_GETCOUNT,0,0)==3,"shape switch replaces the three preset items");
                for(int i=0;i<3;++i)
                {
                    RECT r;SendMessage(list,LB_GETITEMRECT,i,LPARAM(&r));RECT client;GetClientRect(list,&client);
                    RequireCanvas(r.top==0 && r.left>=0 && r.right<=client.right && r.bottom<=client.bottom,"all three presets form one visible row");
                    WCHAR size[16];SendMessageW(list,LB_GETTEXT,i,LPARAM(size));const int round[]={7,4,1},other[]={8,5,2};
                    RequireCanvas(_wtoi(size)==(shape?other[i]:round[i]),"preset values follow selected shape");
                    LPARAM point=MAKELPARAM((r.left+r.right)/2,(r.top+r.bottom)/2);
                    SendMessage(list,WM_LBUTTONDOWN,MK_LBUTTON,point);SendMessage(list,WM_LBUTTONUP,0,point);
                    RequireCanvas(toolsModel.GetBrushWidth()==_wtoi(size),"clicking native preset row updates brush width");
                }
                CStringW path;path.Format(L"texture-tests/brush-%d-%d-%d.bmp",percent,dark,shape);CaptureCanvasDialog(panel,path);
            }
            HWND hardness=GetDlgItem(panel,IDC_BRUSH_HARDNESS);RECT slider,panelRect;GetWindowRect(hardness,&slider);GetWindowRect(panel,&panelRect);
            RequireCanvas(IsWindowVisible(hardness) && slider.top>panelRect.top && slider.bottom<panelRect.bottom && slider.right<=panelRect.right,"hardness slider fits compact panel at every scale");
            if(percent==100) { RECT container;GetWindowRect(toolBoxContainer,&container);RequireCanvas(slider.bottom<=container.bottom,"100 percent hardness slider visible without scrolling"); }
            RequireCanvas(!GetDlgItem(panel,IDC_BRUSH_SIZE),"no brush size slider");
            CStringW path;path.Format(L"texture-tests/window-%d-%d.bmp",percent,dark);CaptureCanvasDialog(mainWindow,path);
        }
    }
    registrySettings.DarkTheme=0;registrySettings.PanelMagnification=100;ApplyTheme();ApplyPanelScale();
    for(COLORREF bg:{RGB(255,255,255),RGB(0,0,0),RGB(128,128,128),RGB(245,150,20)})
    {
        TestBitmap surface(60,40,bg);DrawDashedRect(surface.dc,CRect(3,3,57,37));
        for(int x=3;x<57;++x) RequireCanvas(((x-3)%8<4)?GetPixel(surface.dc,x,3)!=bg:GetPixel(surface.dc,x,3)==bg,"contrast-aware selection frame retains exact 4/4 pattern");
        if(bg==0 || bg==0xffffff) RequireCanvas(GetPixel(surface.dc,3,3)==(bg^0xffffff),"black on white and white on black");
    }
    imageModel.PushImageForUndo(CreateColorDIB(60,40,RGB(50,100,150)));imageModel.ClearHistory();toolsModel.SetActiveTool(TOOL_RECTSEL);
    toolsModel.OnButtonDown(TRUE,5,7,FALSE);toolsModel.OnMouseMove(TRUE,25,27);toolsModel.OnButtonUp(TRUE,25,27);
    POINT point={300,200};SetTimer(NULL,0,80,InspectSelectionMenu);mainWindow.TrackPopupMenu(point,0);
    mainWindow.SendMessage(WM_COMMAND,IDM_CROPSELECTION,0);
    RequireCanvas(imageModel.GetWidth()==20 && imageModel.GetHeight()==20,"context crop uses selected contour");imageModel.Undo();
    RequireCanvas(imageModel.GetWidth()==60 && imageModel.GetHeight()==40,"context crop undo restores full canvas");
    puts("PASS brush presets, actual clicks, four panel scales, both themes, 4/4 adaptive frame and context crop/Undo");

    FILE* manifest=fopen("texture-tests/fixtures.txt","r");RequireCanvas(manifest!=NULL,"texture fixture manifest exists");
    char filename[128];int width,height,count=0;
    while(fscanf_s(manifest,"%127s %d %d",filename,unsigned(sizeof(filename)),&width,&height)==3)
    {
        CStringW path=L"texture-tests/";path+=CStringW(filename);RasterImage decoded;ImageFileInfo info;
        printf("Decode %s\n",filename);RequireCanvas(LoadCustomImage(path,decoded,info),"open independent TGA/DDS/TIFF fixture");
        RequireCanvas(decoded.width==width && decoded.height==height,"texture dimensions and orientation match independent reader");
        std::vector<BYTE> expected;RequireCanvas(ReadImageBytes(path+L".rgba",expected) && expected.size()==decoded.pixels.size()*4,"read independent texture reference");
        // BC endpoint interpolation and TIFF's 16-bit palette quantization can
        // round differently between independent decoders, by at most 1–3 units.
        int tolerance=strstr(filename,"dds-DXT") || strstr(filename,"dds-BC") || strstr(filename,"dds-variable-BC5")?3:(strstr(filename,"tiff-P.") || strstr(filename,"tiff-I16."))?1:0,maxError=0;
        for(size_t i=0;i<decoded.pixels.size();++i)
        {
            DWORD actual=decoded.pixels[i];for(int c=0;c<4;++c)
            { int value=actual>>(c==3?24:(2-c)*8)&255;maxError=max(maxError,abs(value-int(expected[i*4+c]))); }
        }
        if(strstr(filename,"tiff-CMYK"))
        {
            // WIC uses a printing colour profile for CMYK. Pillow's default
            // convert() uses elementary channel arithmetic, not that profile.
            RequireCanvas(decoded.pixels[0]!=decoded.pixels[1],"CMYK conversion produces distinct RGB colors");
            for(DWORD pixel:decoded.pixels) RequireCanvas(pixel>>24==255,"CMYK TIFF stays opaque");
        }
        else
        {
            if(maxError>tolerance) { printf("Mismatch: %s, maximum channel difference %d\n",filename,maxError);std::vector<BYTE> png;EncodePng(decoded,PngOptions(),png);WriteImageBytes(L"texture-tests/mismatch.png",png); }
            RequireCanvas(maxError<=tolerance,"decoded texture pixels and alpha agree with independent decoder");
        }
        HBITMAP bitmap=DoLoadImageFile(mainWindow,path,FALSE);RasterImage opened;
        RequireCanvas(bitmap && ReadRaster(bitmap,opened) && opened.pixels==decoded.pixels,"native image open retains unrestricted RGBA");DeleteObject(bitmap);
        if(strstr(filename,"tiff-pages")) RequireCanvas(info.multipleImages,"multipage TIFF is recognized");
        if(strstr(filename,"tiff-raw")) RequireCanvas(info.xDpi==144 && info.yDpi==72,"TIFF reads DPI");
        ++count;
    }
    fclose(manifest);RequireCanvas(count>=38,"all independent codec fixtures tested");
    auto source=LoadFixture(L"texture-tests/source.png");imageModel.PushImageForUndo(MakeRasterBitmap(source));imageModel.ClearHistory();g_xDpi=144;g_yDpi=72;
    paletteModel.SelectPalette(PAL_MODERN);toolsModel.SetPenWidth(1);
    for(LPCWSTR extension:{L"tga",L"dds",L"tif"})
    {
        g_xDpi=144;g_yDpi=72;
        CStringW path=L"texture-tests/output.";path+=extension;
        RequireCanvas(SaveDIBToFile(HBITMAP(GetCurrentObject(imageModel.GetDC(),OBJ_BITMAP)),path,TRUE),"save new texture format through native document path");
        RasterImage out;ImageFileInfo info;RequireCanvas(LoadCustomImage(path,out,info) && source.pixels==out.pixels,"texture export retains every RGBA byte, including hidden colors");
        if(info.format==SaveFormat::Tiff) RequireCanvas(info.xDpi==144 && info.yDpi==72,"saved TIFF retains DPI");
        RequireCanvas(DocumentRaster().pixels==source.pixels,"export never restricts canvas palette or changes pixels");
        HBITMAP opened=DoLoadImageFile(mainWindow,path,TRUE);RequireCanvas(opened && DocumentRaster().pixels==source.pixels,"native reopen of saved texture");
        toolsModel.SetActiveTool(TOOL_PEN);paletteModel.SetFgColor(RGB(17,149,237));toolsModel.OnButtonDown(TRUE,2,2,FALSE);toolsModel.OnButtonUp(TRUE,2,2);
        RequireCanvas(RasterPixel(imageModel.GetDC(),2,2)==0xff1195ed,"new texture is editable in arbitrary RGB colors");imageModel.Undo();
        RequireCanvas(DocumentRaster().pixels==source.pixels,"texture edit undo preserves original alpha");
    }
    for(LPCWSTR name:{L"tiff-pages.tif",L"dds-mipmaps.dds"})
    {
        printf("Opening multi-image document: %ls\n",name);
        CStringW path=L"texture-tests/";path+=name;std::vector<BYTE> before,after;ReadImageBytes(path,before);
        multiImageNotice=false;noticeTicks=0;UINT_PTR timer=SetTimer(NULL,0,60,AcceptMultiImageNotice);
        RequireCanvas(DoLoadImageFile(mainWindow,path,TRUE)!=NULL,"open first image of multipage TIFF or DDS texture");KillTimer(NULL,timer);
        RequireCanvas(multiImageNotice && !g_isAFile && !g_imageSaved && lstrcmpW(g_szFileName,path),"multi-image import explains flattening and suggests separate output");
        ReadImageBytes(path,after);RequireCanvas(before==after,"multi-image original file remains intact");
    }
    // Truncation and oversized declarations must fail without partial images.
    for(LPCWSTR name:{L"tga-RGBA-1.tga",L"dds-DXT5.dds",L"tiff-raw.tif"})
    {
        CStringW path=L"texture-tests/";path+=name;std::vector<BYTE> input;ReadImageBytes(path,input);
        for(size_t size:{size_t(0),size_t(4),size_t(17),size_t(127),size_t(150)})
        { if(size>=input.size()) continue;std::vector<BYTE> broken(input.begin(),input.begin()+size);RasterImage out;bool ok;
          if(FormatFromPath(path)==SaveFormat::Tga) ok=DecodeTga(broken,out);else if(FormatFromPath(path)==SaveFormat::Dds) ok=DecodeDds(broken,out);else ok=DecodeTiff(broken,out);
          RequireCanvas(!ok,"truncated texture safely rejected"); }
    }
    puts("PASS independent TGA/DDS BC1-7/TIFF fixtures, native open-edit-save-reopen, exact RGBA/DPI, undo and malformed inputs");
    if(testLanguages.empty()) EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    for(WORD language:testLanguages) RequireCanvas(!ExactString(language,IDS_MULTI_IMAGE_FIRST).IsEmpty(),"multi-image notice translated in all locales");
    toolsModel.SetActiveTool(TOOL_RECTSEL);
}
