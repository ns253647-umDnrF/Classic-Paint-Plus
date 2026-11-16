static void TestExactDimensions()
{
    CreateDirectoryW(L"document-alpha-tests",NULL);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    mainWindow.SetWindowPos(NULL,0,0,1050,900,SWP_NOACTIVATE|SWP_NOZORDER);
    toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;
    ApplyTheme();ApplyPanelScale();canvasWindow.zoomTo(1000);SetTestShift(false);
    HBITMAP blank=CreateColorDIB(600,450,RGB(255,255,255));ResetDocumentAlpha(blank);imageModel.PushImageForUndo(blank);imageModel.ClearHistory();
    HWND width=toolSettingsWindow.GetDlgItem(IDC_OBJECT_WIDTH),height=toolSettingsWindow.GetDlgItem(IDC_OBJECT_HEIGHT);
    for(TOOLTYPE type:{TOOL_TEXT,TOOL_RECT,TOOL_ELLIPSE,TOOL_RRECT,TOOL_FIGURE})
    {
        toolsModel.SetActiveTool(type);toolsModel.SetFigureAngle(0);textEditWindow.ResetRotation();
        FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,150,130);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,320,240);FigureMouse(WM_LBUTTONUP,0,320,240);
        if(type==TOOL_TEXT) textEditWindow.SetWindowTextW(L"Exact size");
        for(double angle:{0.,37.,90.,227.})
        {
            if(type==TOOL_TEXT) textEditWindow.SetRotationAngle(angle);else toolsModel.SetFigureAngle(angle);
            EnterObjectValue(width,L"213 px");EnterObjectValue(height,L"137");CRect r;
            if(type==TOOL_TEXT) textEditWindow.GetEditRect(&r);else toolsModel.EditableFigureBounds(r);
            RequireCanvas(r.Width()==213 && r.Height()==137,"manual fields set exact local width and height at arbitrary rotation");
            RequireCanvas(WindowLabel(width)==L"213 px" && WindowLabel(height)==L"137 px","pixel suffix visible after Enter and focus transfer");
            RequireCanvas((type==TOOL_TEXT?textEditWindow.Angle():toolsModel.FigureAngle())==angle,"manual dimensions preserve rotation");
        }
        EnterObjectValue(width,L"0");RequireCanvas(GetFocus()==width,"zero width rejected");SendMessage(width,WM_KEYDOWN,VK_ESCAPE,0);
        EnterObjectValue(height,L"32768 px");RequireCanvas(GetFocus()==height,"oversized height rejected");SendMessage(height,WM_KEYDOWN,VK_ESCAPE,0);
        for(int percent:{100,150,200,250})
        {
            registrySettings.PanelMagnification=percent;ApplyPanelScale();RECT a,b,c;GetWindowRect(width,&a);GetWindowRect(height,&b);GetWindowRect(ParameterBarWindow(),&c);
            RequireCanvas(IsWindowEnabled(width) && IsWindowVisible(height) && a.bottom<=b.top && b.bottom<=c.bottom && a.top>=c.top,"both dimensions fit at all four panel scales");
        }
        registrySettings.PanelMagnification=100;ApplyPanelScale();CaptureCanvasDialog(mainWindow,type==TOOL_TEXT?L"document-alpha-tests/text-dimensions.bmp":L"document-alpha-tests/figure-dimensions.bmp");
        toolsModel.OnEndDraw(TRUE);
    }
    toolsModel.SetActiveTool(TOOL_PEN);int parsed;
    RequireCanvas(ParsePixelValue(L" -15 px ",parsed,-32767,32767) && parsed==-15,"signed pixel units parse for crop extensions");
    RequireCanvas(!ParsePixelValue(L"5oops",parsed,1,500) && !ParsePixelValue(L"99999999999999999 px",parsed,1,500),"pixel parser rejects suffix errors and overflow");
    puts("PASS exact text/figure dimensions: pixel suffix, arbitrary angles, validation, four panel scales and retained geometry");
}

static VOID CALLBACK DriveAlphaWarning(HWND,UINT,UINT_PTR timer,DWORD)
{
    HWND dialog=NULL;EnumThreadWindows(GetCurrentThreadId(),FindDialog,LPARAM(&dialog));if(!dialog) return;
    KillTimer(NULL,timer);CaptureCanvasDialog(dialog,L"document-alpha-tests/alpha-warning.bmp");PostMessage(dialog,WM_COMMAND,IDCANCEL,0);
}
static void TestParallelTransparency()
{
    toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();
    RasterImage source;source.width=100;source.height=40;source.pixels.assign(4000,0xff00a040);
    for(int y=0;y<12;++y) for(int x=0;x<16;++x)
        source.pixels[y*100+x]=x<4?0x000012ab:x<8?0xffffffff:x<12?0xffff0000:0x800000ff;
    for(TOOLTYPE tool:{TOOL_RECTSEL,TOOL_FREESEL}) for(int mode:{0,1,2})
    {
        toolsModel.SetActiveTool(tool);HBITMAP bitmap=MakeRasterBitmap(source);ResetDocumentAlpha(bitmap);imageModel.PushImageForUndo(bitmap);imageModel.ClearHistory();
        SetDocumentBackdrop(RGB(242,210,155));paletteModel.SetBgColor(RGB(255,255,255));toolsModel.SetBackgroundTransparent(TRUE);registrySettings.SelectionInterpolation=mode;
        HBITMAP mask=tool==TOOL_FREESEL?CreateBitmap(16,12,1,1,NULL):NULL;
        if(mask) { HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,mask);PatBlt(dc,0,0,16,12,WHITENESS);SelectObject(dc,old);DeleteDC(dc); }
        selectionModel.setMask(CRect(0,0,16,12),mask);selectionModel.m_bShow=TRUE;RequireCanvas(selectionModel.TakeOff(),"lift selection containing both alpha and ordinary background color");
        HBITMAP copy=selectionModel.GetSelectionContents();RasterImage keyed;ReadRaster(copy,keyed);DeleteObject(copy);
        RequireCanvas((keyed.pixels[2]>>24)==0 && (keyed.pixels[6]>>24)==0 && keyed.pixels[10]==0xffff0000 && (keyed.pixels[14]>>24)==128,"alpha, color key, mask and partial opacity combine in selection export");
        toolsModel.SetBackgroundTransparent(FALSE);copy=selectionModel.GetSelectionContents();ReadRaster(copy,keyed);DeleteObject(copy);
        RequireCanvas((keyed.pixels[2]>>24)==0 && keyed.pixels[6]==0xffffffff,"opaque selection mode restores color-key pixels but never removes original alpha");
        toolsModel.SetBackgroundTransparent(TRUE);selectionModel.moveSelection(40,15);selectionModel.Landing();auto moved=DocumentRaster();
        RequireCanvas(moved.pixels[15*100+42]==0xff00a040 && moved.pixels[15*100+46]==0xff00a040 && moved.pixels[15*100+50]==0xffff0000,"moving selection exposes destination through both alpha pixels and the independent background color");
        RequireCanvas((moved.pixels[10]>>24)==0,"vacated area remains actual alpha in transparent document mode");
        imageModel.Undo();RequireCanvas(DocumentRaster().pixels==source.pixels && DocumentAlpha().enabled,"undo combined transparency movement restores exact source RGBA and mode");
    }
    toolsModel.SetBackgroundTransparent(FALSE);registrySettings.SelectionInterpolation=0;toolsModel.SetActiveTool(TOOL_RECTSEL);
    puts("PASS parallel transparency: alpha plus independent color key, toggling without destructive baking, both selection tools and all interpolation modes");
}
static void TestDocumentAlpha()
{
    CreateDirectoryW(L"document-alpha-tests",NULL);toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();toolsModel.SetActiveTool(TOOL_PEN);
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);registrySettings.PanelMagnification=100;ApplyPanelScale();canvasWindow.zoomTo(1000);
    RasterImage original;original.width=240;original.height=160;original.pixels.assign(240*160,0xff19aacc);
    for(int y=45;y<110;++y) for(int x=70;x<160;++x) original.pixels[y*240+x]=0xffd32a54;
    HBITMAP bitmap=MakeRasterBitmap(original);SetBitmapAndInfo(bitmap,NULL,NULL,FALSE);
    RequireCanvas(!DocumentAlpha().enabled,"new opaque document resets transparency state");
    BOOL oldToolBackground=toolsModel.IsBackgroundTransparent();
    mainWindow.SendMessage(WM_COMMAND,IDM_IMAGEDRAWOPAQUE,0);
    RequireCanvas(DocumentAlphaPicking() && toolsModel.GetActiveTool()==TOOL_COLOR && toolsModel.IsBackgroundTransparent()==oldToolBackground,"menu activates document picker independently of tool background buttons");
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,10,10);FigureMouse(WM_LBUTTONUP,0,10,10);
    RequireCanvas(!DocumentAlphaPicking() && toolsModel.GetActiveTool()==TOOL_PEN && DocumentAlpha().enabled && DocumentAlpha().hasColor,"canvas picker restores previous tool and activates real alpha");
    RequireCanvas(DocumentAlpha().color==RGB(25,170,204) && DocumentBackdrop()==RGB(25,170,204),"initial key also becomes display backdrop");
    auto transparent=DocumentRaster();RequireCanvas((transparent.pixels[0]>>24)==0 && transparent.pixels[50*240+90]==0xffd32a54,"only chosen color becomes transparent");
    imageModel.Undo();RequireCanvas(!DocumentAlpha().enabled && DocumentRaster().pixels==original.pixels,"undo restores pixels and disabled document mode");
    imageModel.Redo();RequireCanvas(DocumentAlpha().enabled && DocumentRaster().pixels==transparent.pixels,"redo restores pixels and key-color metadata");
    SetDocumentBackdrop(RGB(242,210,155));RequireCanvas(DocumentRaster().pixels==transparent.pixels,"changing backdrop never mutates document alpha or RGB");
    HBITMAP preview=imageModel.CopyBitmap();HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,preview);FlattenCanvasSurface(dc);
    RequireCanvas(GetPixel(dc,0,0)==RGB(242,210,155) && GetPixel(dc,90,50)==RGB(211,42,84),"canvas composites against chosen backdrop while keeping opaque pixels");SelectObject(dc,old);DeleteDC(dc);DeleteObject(preview);
    paletteModel.SetFgColor(RGB(10,20,30));FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,30,30);FigureMouse(WM_LBUTTONUP,0,30,30);
    RequireCanvas(DocumentRaster().pixels[30*240+30]==0xff0a141e && (DocumentRaster().pixels[0]>>24)==0,"drawing creates opaque ink while surrounding pixels retain real alpha");imageModel.Undo();
    SetDocumentBackdrop(RGB(242,210,155));CRect palette;paletteWindow.GetClientRect(&palette);POINT top={PanelPx(279),palette.bottom/2-PanelPx(15)+PanelPx(7)};
    paletteWindow.SendMessage(WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(top.x,top.y));RequireCanvas(DocumentAlphaPicking(),"double-click upper swatch starts re-key eyedropper");
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,90,50);FigureMouse(WM_LBUTTONUP,0,90,50);
    auto rekeyed=DocumentRaster();RequireCanvas(rekeyed.pixels[0]==0xfff2d29b && (rekeyed.pixels[50*240+90]>>24)==0,"re-key fills old transparency with current backdrop then removes new color");
    imageModel.Undo();RequireCanvas(DocumentRaster().pixels==transparent.pixels && DocumentAlpha().color==RGB(25,170,204),"re-key is one undoable operation including key metadata");
    SetDocumentBackdrop(RGB(242,210,155));auto before=DocumentRaster().pixels;
    BeginDocumentAlphaPick();canvasWindow.SendMessage(WM_KEYDOWN,VK_ESCAPE,0);RequireCanvas(!DocumentAlphaPicking() && DocumentRaster().pixels==before,"Esc cancels picker without changing existing transparency");
    BeginDocumentAlphaPick();toolsModel.SetActiveTool(TOOL_BRUSH);RequireCanvas(!DocumentAlphaPicking() && DocumentRaster().pixels==before,"choosing another tool cancels pending picker");
    ToggleDocumentAlpha();RequireCanvas(!DocumentAlpha().enabled && DocumentRaster().pixels[0]==0xfff2d29b,"unchecking mode fills transparency using backdrop");imageModel.Undo();
    RequireCanvas(DocumentAlpha().enabled && DocumentRaster().pixels==before,"undo unchecking restores real alpha");
    SetDocumentBackdrop(RGB(242,210,155));
    HBITMAP snapshot=imageModel.CopyBitmap();unsigned warnings=g_alphaLossWarnings;
    SetCurrentPngOptions(PngOptions{});SetCurrentSaveFormat(SaveFormat::Png);
    SetPreservePaintData(false); // This fixture intentionally represents an ordinary external PNG.
    auto exported=before;for(DWORD& pixel:exported) if(!(pixel>>24)) pixel=0xf2d29b;
    RequireCanvas(SaveDIBToFile(snapshot,L"document-alpha-tests/exact.png",FALSE) && LoadFixture(L"document-alpha-tests/exact.png").pixels==exported,"PNG export keeps visible RGBA and alpha, with backdrop RGB in fully transparent pixels");
    RequireCanvas(g_alphaLossWarnings==warnings,"alpha-capable output does not warn");
    g_alphaLossAnswer=IDCANCEL;
    for(auto format:{SaveFormat::Bmp1,SaveFormat::Bmp4,SaveFormat::Bmp8,SaveFormat::Bmp24,SaveFormat::Jpeg,SaveFormat::Gif})
    {
        SetCurrentSaveFormat(format);LPCWSTR path=format==SaveFormat::Jpeg?L"document-alpha-tests/cancel.jpg":format==SaveFormat::Gif?L"document-alpha-tests/cancel.gif":L"document-alpha-tests/cancel.bmp";
        DeleteFileW(path);RequireCanvas(!SaveDIBToFile(snapshot,path,TRUE) && GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES,"cancel alpha-loss warning aborts export before creating file");
    }
    RequireCanvas(g_alphaLossWarnings==warnings+6,"all opaque save variants warn");
    g_alphaLossAnswer=0;SetTimer(NULL,0,100,DriveAlphaWarning);
    RequireCanvas(!SaveDIBToFile(snapshot,L"document-alpha-tests/warning.bmp",FALSE),"real native warning can be canceled");g_alphaLossAnswer=IDOK;
    SetCurrentSaveFormat(SaveFormat::Bmp24);RequireCanvas(SaveDIBToFile(snapshot,L"document-alpha-tests/matte.bmp",FALSE),"confirmed BMP export succeeds");
    HBITMAP bmp=DoLoadImageFile(mainWindow,L"document-alpha-tests/matte.bmp",FALSE);RasterImage opaque;
    RequireCanvas(bmp && ReadRaster(bmp,opaque),"read exported bitmap");DeleteObject(bmp);
    RequireCanvas(opaque.pixels[0]==0xfff2d29b && opaque.pixels[50*240+90]==0xffd32a54,"opaque export uses chosen display backdrop");
    RequireCanvas(DocumentRaster().pixels==before,"saving flattened copy leaves working document unchanged");DeleteObject(snapshot);
    for(LPCWSTR file:{L"document-alpha-tests/exact.png",L"format-tests/rgb-key.png",L"format-tests/palette-alpha.png",L"format-tests/gray-key.png"})
    {
        RequireCanvas(DoLoadImageFile(mainWindow,file,TRUE)!=NULL,"transparent PNG loads through application path");
        COLORREF expected=!lstrcmpW(file,L"document-alpha-tests/exact.png")?RGB(242,210,155):RGB(255,255,255);
        RequireCanvas(DocumentAlpha().enabled && !DocumentAlpha().hasColor && DocumentBackdrop()==expected,"PNG alpha or tRNS uses standard background when present, otherwise white");
    }
    DoLoadImageFile(mainWindow,L"document-alpha-tests/exact.png",TRUE);CaptureCanvasDialog(mainWindow,L"document-alpha-tests/transparent-document.bmp");
    if(testLanguages.empty()) EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    for(WORD language:testLanguages) for(UINT id:{IDS_DOC_TRANSPARENT,IDS_ALPHA_LOSS,IDS_ALPHA_PICK,IDS_OBJECT_WIDTH,IDS_OBJECT_HEIGHT,IDS_ALPHA_COLOR,IDS_ALPHA_BACKDROP})
        RequireCanvas(!ExactString(language,id).IsEmpty(),"document alpha and exact dimensions translated into all 30 languages");
    TestParallelTransparency();InitializeImage(NULL,NULL,FALSE);toolsModel.SetActiveTool(TOOL_RECTSEL);
    SetPreservePaintData(true);
    puts("PASS document alpha: picker, key/matte swatches, real RGBA drawing, undo/redo, re-key, cancel, opaque-format warnings, PNG and tRNS reload, 30 translations");
}
