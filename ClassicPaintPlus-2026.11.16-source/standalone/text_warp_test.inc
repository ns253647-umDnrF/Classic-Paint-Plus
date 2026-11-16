#include <cmath>
#include "../third_party/lodepng/lodepng.h"
static void TestPartialSizesAndEditKeys()
{
    CreateDirectoryW(L"text-warp-tests",NULL);toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    mainWindow.SetWindowPos(NULL,0,0,1100,900,SWP_NOACTIVATE|SWP_NOZORDER);registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();canvasWindow.zoomTo(1000);
    imageModel.PushImageForUndo(CreateColorDIB(650,480,RGB(255,255,255)));imageModel.ClearHistory();
    HWND width=toolSettingsWindow.GetDlgItem(IDC_OBJECT_WIDTH),height=toolSettingsWindow.GetDlgItem(IDC_OBJECT_HEIGHT),save=toolSettingsWindow.GetDlgItem(IDC_SAVE_OBJECT_SIZE);
    DWORD remember=registrySettings.RememberObjectSizes;registrySettings.RememberObjectSizes=1;registrySettings.FigureKind=FIG_DIAMOND;
    for(TOOLTYPE type:{TOOL_FIGURE,TOOL_TEXT})
    {
        bool text=type==TOOL_TEXT;toolsModel.SetActiveTool(type);toolsModel.ResetObjectSize(text);toolsModel.SetFigureAngle(0);textEditWindow.ResetRotation();textEditWindow.SetWarp({});
        EnterObjectValue(width,L"");EnterObjectValue(height,L"80");SendMessage(save,BM_CLICK,0,0);
        RequireCanvas(IsWindowEnabled(save) && toolsModel.ObjectSizePreset(text).cx==0 && toolsModel.ObjectSizePreset(text).cy==80,"S saves height-only preset before object creation");
        RegistrySettings loaded;loaded.Load(SW_HIDE);int kind=toolsModel.ObjectSizeKind(text);RequireCanvas(!loaded.SavedObjectSizes[kind].cx && loaded.SavedObjectSizes[kind].cy==80,"one-axis preset survives INI reload");
        int widths[2];for(int i=0;i<2;++i)
        {
            FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,130,130);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,300+i*70,260);FigureMouse(WM_LBUTTONUP,0,300+i*70,260);
            CRect r;if(text) textEditWindow.GetEditRect(&r);else toolsModel.EditableFigureBounds(r);
            widths[i]=r.Width();RequireCanvas(r.Height()==80,"height preset fixes only height");toolsModel.OnEndDraw(TRUE);
        }
        RequireCanvas(widths[1]>widths[0],"unspecified width follows mouse independently for each new object");
        EnterObjectValue(width,L"185");EnterObjectValue(height,L"");SendMessage(save,BM_CLICK,0,0);
        FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,130,130);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,300,260);FigureMouse(WM_LBUTTONUP,0,300,260);
        SetFocus(height);RequireCanvas(WindowLabel(height).Find(L"px")<0,"px disappears immediately on focus");SetWindowTextW(height,L"");SendMessage(save,BM_CLICK,0,0);
        RequireCanvas(registrySettings.SavedObjectSizes[kind].cx==185 && !registrySettings.SavedObjectSizes[kind].cy,"active object's cleared height can be saved without collapsing geometry");
        CRect r;if(text) textEditWindow.GetEditRect(&r);else toolsModel.EditableFigureBounds(r);RequireCanvas(r.Height()>1,"clearing one active dimension does not erase current object");
        toolsModel.OnEndDraw(TRUE);toolsModel.ResetObjectSize(text);
    }
    toolsModel.SetActiveTool(TOOL_FIGURE);EnterObjectValue(width,L"123");SetFocus(width);
    RequireCanvas(WindowLabel(width)==L"123","focused field contains digits only");SendMessage(width,EM_SETSEL,1,2);
    MSG key={width,WM_KEYDOWN,VK_DELETE,0};RequireCanvas(IsEditKeyMessage(key),"Delete is routed to numeric edit instead of canvas accelerator");
    if(!IsEditKeyMessage(key) && TranslateAcceleratorW(mainWindow,mainWindow.Accelerators(),&key)) RequireCanvas(false,"edit Delete must not reach accelerator");else DispatchMessageW(&key);
    RequireCanvas(WindowLabel(width)==L"13","Delete removes selected digit");canvasWindow.SetFocus();RequireCanvas(WindowLabel(width)==L"13 px","px returns after focus leaves");
    HWND angle=toolSettingsWindow.GetDlgItem(IDC_SELECTION_ANGLE);EnterObjectValue(angle,L"35");SetFocus(angle);RequireCanvas(WindowLabel(angle)==L"35","degrees disappear on focus");canvasWindow.SetFocus();RequireCanvas(WindowLabel(angle).Find(L'\u00b0')>=0,"degrees return after edit");
    toolsModel.SetActiveTool(TOOL_ZOOM);HWND zoom=toolSettingsWindow.GetDlgItem(IDC_ZOOM_EDIT);SetFocus(zoom);RequireCanvas(WindowLabel(zoom).Find(L'%')<0,"percent disappears on focus");canvasWindow.SetFocus();RequireCanvas(WindowLabel(zoom).Find(L'%')>=0,"percent returns after edit");
    registrySettings.RememberObjectSizes=remember;registrySettings.StorePreferences();toolsModel.SetActiveTool(TOOL_FIGURE);toolsModel.ResetObjectSize(false);toolsModel.SetFigureAngle(0);toolsModel.SetActiveTool(TOOL_RECTSEL);
    puts("PASS partial S presets: height-only and width-only, variable mouse axis, active blank field, INI, Delete routing and focus-only numeric suffixes");
}
static int warpDialogMode=0,warpDialogTicks=0;
static VOID CALLBACK CheckWarpLocale(HWND,UINT,UINT_PTR timer,DWORD)
{
    RequireCanvas(++warpDialogTicks<80,"translated warp dialog appears");HWND dialog=FindJpegDialogControl(IDC_WARP_STYLE);if(!dialog) return;KillTimer(NULL,timer);
    WORD language=WORD(registrySettings.UILanguage);RequireCanvas(WindowLabel(dialog)==ExactString(language,IDS_TEXT_WARP),"native warp caption uses selected language");
    for(int control:{7500,7503,7504,7505,IDC_WARP_HORIZONTAL,IDC_WARP_VERTICAL,IDCANCEL})
    {
        HWND window=GetDlgItem(dialog,control);CStringW label=WindowLabel(window);RECT rect;GetClientRect(window,&rect);
        HDC dc=GetDC(window);HGDIOBJ old=SelectObject(dc,HFONT(SendMessage(window,WM_GETFONT,0,0)));SIZE size;GetTextExtentPoint32W(dc,label,label.GetLength(),&size);SelectObject(dc,old);ReleaseDC(window,dc);
        int padding=control==IDC_WARP_HORIZONTAL || control==IDC_WARP_VERTICAL?18:4;
        RequireCanvas(!label.IsEmpty() && size.cx+padding<=rect.right,"translated warp control label fits without clipping");
    }
    for(int style=0;style<WARP_COUNT;++style)
    {
        WCHAR text[512];SendDlgItemMessage(dialog,IDC_WARP_STYLE,CB_GETLBTEXT,style,LPARAM(text));
        RequireCanvas(ExactString(language,IDS_WARP_FIRST+style)==text,"native warp list matches compiled translation");
    }
    PostMessage(dialog,WM_COMMAND,IDCANCEL,0);
}
static VOID CALLBACK DriveWarpDialog(HWND,UINT,UINT_PTR timer,DWORD)
{
    RequireCanvas(++warpDialogTicks<80,"warp dialog appears");HWND dialog=FindJpegDialogControl(IDC_WARP_STYLE);if(!dialog) return;KillTimer(NULL,timer);
    RequireCanvas(SendDlgItemMessage(dialog,IDC_WARP_STYLE,CB_GETCOUNT,0,0)==WARP_COUNT,"all sixteen warp styles are in the native list");
    SendDlgItemMessage(dialog,IDC_WARP_STYLE,CB_SETCURSEL,WARP_ARC,0);SendMessage(dialog,WM_COMMAND,MAKEWPARAM(IDC_WARP_STYLE,CBN_SELCHANGE),LPARAM(GetDlgItem(dialog,IDC_WARP_STYLE)));
    SetDlgItemTextW(dialog,IDC_WARP_BEND,L"65");SetDlgItemTextW(dialog,IDC_WARP_HDIST,L"20");SetDlgItemTextW(dialog,IDC_WARP_VDIST,L"-15");
    RequireCanvas(textEditWindow.Warp().style==WARP_ARC && textEditWindow.Warp().bend==65 && textEditWindow.Warp().horizontal==20,"manual inputs preview warp live");
    CaptureCanvasDialog(dialog,registrySettings.DarkTheme?L"text-warp-tests/warp-dialog-dark.bmp":L"text-warp-tests/warp-dialog.bmp");
    RECT client;GetClientRect(dialog,&client);TestBitmap capture(client.right,client.bottom,GetSysColor(COLOR_BTNFACE));
    SendMessage(dialog,WM_PRINT,WPARAM(capture.dc),PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
    CImageDx captured;captured.Attach(capture.bitmap);captured.SaveDx(registrySettings.DarkTheme?L"text-warp-tests/warp-dialog-dark-client.png":L"text-warp-tests/warp-dialog-client.png",Gdiplus::ImageFormatPNG);captured.Detach();
    PostMessage(dialog,WM_COMMAND,warpDialogMode?IDOK:IDCANCEL,0);
}
static void TestTextWarp()
{
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);toolsModel.OnEndDraw(TRUE);toolsModel.SetActiveTool(TOOL_TEXT);toolsModel.SetBackgroundTransparent(TRUE);
    if(!textEditWindow.IsWindow()) textEditWindow.Create(canvasWindow);
    registrySettings.strFontName=L"Arial";registrySettings.PointSize=25;registrySettings.Bold=0;registrySettings.TextAlignment=0;
    textEditWindow.UpdateFont();textEditWindow.ResetRotation();textEditWindow.SetWindowTextW(L"Classic Paint");textEditWindow.ValidateEditRect(CRect(30,50,280,140));textEditWindow.ShowWindow(SW_SHOWNOACTIVATE);
    paletteModel.SetFgColor(RGB(20,65,145));TestBitmap gallery(1040,560,RGB(255,255,255));
    std::vector<std::vector<BYTE>> signatures;
    for(int style=0;style<WARP_COUNT;++style)
    {
        TextWarpOptions o;o.style=style;o.bend=65;textEditWindow.SetWarp(o);
        int x=10+style%4*260,y=10+style/4*140;textEditWindow.ValidateEditRect(CRect(x,y+28,x+250,y+123));
        CStringW label=ExactString(0x409,IDS_WARP_FIRST+style);SetBkMode(gallery.dc,TRANSPARENT);SetTextColor(gallery.dc,RGB(0,0,0));TextOutW(gallery.dc,x,y,label,label.GetLength());textEditWindow.RenderDocument(gallery.dc);
        textEditWindow.ValidateEditRect(CRect(30,50,280,140));TestBitmap sample(320,180,RGB(255,255,255));textEditWindow.RenderDocument(sample.dc);signatures.push_back(FigurePixels(sample.dc));
        if(style) RequireCanvas(signatures.back()!=signatures[0],"each warp changes the rendered text");
        for(int bend:{-100,-50,0,50,100}) for(bool vertical:{false,true}) for(double aspect:{.03,.15,1.0,6.0,30.0}) for(int distortion:{-100,27,100})
        {
            o.bend=bend;o.verticalAxis=vertical;o.horizontal=distortion;o.vertical=-distortion;TextWarpMesh mesh(o,aspect);
            for(WarpPoint p: {WarpPoint{.25,.25},WarpPoint{.5,.5},WarpPoint{.75,.65}})
            { auto warped=mesh.Forward(p),inverse=mesh.Inverse(warped);RequireCanvas(std::isfinite(warped.x) && std::isfinite(warped.y) && fabs(inverse.x-p.x)<.015 && fabs(inverse.y-p.y)<.015,"warp mesh has finite positions and inverse mouse mapping at extreme values"); }
        }
    }
    CImageDx img;img.Attach(gallery.bitmap);img.SaveDx(L"text-warp-tests/warp-gallery.png",Gdiplus::ImageFormatPNG);img.Detach();
    textEditWindow.SetWarp({});TextWarpOptions original=textEditWindow.Warp();warpDialogTicks=0;warpDialogMode=0;SetTimer(NULL,0,60,DriveWarpDialog);RequireCanvas(!ShowTextWarpDialog(mainWindow) && textEditWindow.Warp().style==original.style,"Cancel restores original warp without changing text");
    warpDialogTicks=0;warpDialogMode=1;SetTimer(NULL,0,60,DriveWarpDialog);RequireCanvas(ShowTextWarpDialog(mainWindow) && textEditWindow.Warp().bend==65,"OK keeps all warp parameters");
    textEditWindow.SetRotationAngle(37);textEditWindow.SetDimensions(300,110);textEditWindow.SendMessage(WM_CHAR,L'!',1);RequireCanvas(textEditWindow.Warp().style==WARP_ARC && textEditWindow.GetWindowTextLength()>12,"warped text remains editable and resizable after rotation");
    imageModel.PushImageForUndo(CreateColorDIB(650,480,RGB(255,255,255)));imageModel.ClearHistory();TestBitmap preview(650,480,RGB(255,255,255));textEditWindow.RenderDocument(preview.dc);auto pixels=FigurePixels(preview.dc);toolsModel.OnEndDraw(FALSE);
    RequireCanvas(FigurePixels(imageModel.GetDC())==pixels,"warped and rotated preview commits exactly once");imageModel.Undo();RequireCanvas(!imageModel.CanUndo(),"warped text undo is one operation");
    registrySettings.DarkTheme=1;ApplyTheme();warpDialogTicks=0;SetTimer(NULL,0,60,DriveWarpDialog);ShowTextWarpDialog(mainWindow);
    registrySettings.DarkTheme=0;ApplyTheme();textEditWindow.SetWarp({});textEditWindow.ResetRotation();toolsModel.SetActiveTool(TOOL_RECTSEL);
    puts("PASS 15 text warps plus None: all directions and extreme inputs, inverse mapping, live modal dialog, Cancel/OK, typing, resize, rotation, exact commit and Undo");
}

static void TestPngBackdrop()
{
    toolsModel.OnEndDraw(TRUE);toolsModel.SetActiveTool(TOOL_RECTSEL);RasterImage source;source.width=64;source.height=32;source.pixels.resize(2048);
    for(int y=0;y<32;++y) for(int x=0;x<64;++x) source.pixels[y*64+x]=(DWORD(x<20?0:x<40?128:255)<<24)|0xe03060|(DWORD(y)<<8);
    HBITMAP bitmap=MakeRasterBitmap(source);auto& alpha=DocumentAlpha();COLORREF matte=RGB(57,149,203);
    for(bool optimize:{false,true}) for(bool interlace:{false,true}) for(bool service:{false,true})
    {
        alpha.enabled=alpha.customized=alpha.hasColor=true;alpha.color=RGB(224,48,96);alpha.backdrop=matte;SetPreservePaintData(service);
        PngOptions options;options.optimize=optimize;options.interlaced=interlace;SetCurrentPngOptions(options);
        CStringW file;file.Format(L"text-warp-tests/backdrop-%d-%d-%d.png",optimize,interlace,service);RequireCanvas(SaveCustomImage(bitmap,file,SaveFormat::Png,FALSE),"PNG backdrop export succeeds with every option");
        std::vector<BYTE> bytes;ReadImageBytes(file,bytes);RasterImage decoded;RequireCanvas(DecodePng(bytes,decoded),"PNG backdrop export decodes");
        for(size_t i=0;i<source.pixels.size();++i) RequireCanvas(decoded.pixels[i]==((source.pixels[i]>>24)?source.pixels[i]:DWORD(0x3995cb)),"fully transparent RGB is matte; alpha and all visible pixel colours stay unchanged");
        lodepng::State state;std::vector<BYTE> raw;unsigned w,h;RequireCanvas(!lodepng::decode(raw,w,h,state,bytes) && state.info_png.background_defined,"standard bKGD chunk is present independent of private service metadata");
        auto& info=state.info_png;COLORREF background=0;
        if(info.color.colortype==LCT_PALETTE) { auto* p=info.color.palette+4*info.background_r;background=RGB(p[0],p[1],p[2]); }
        else if(info.color.colortype==LCT_RGB || info.color.colortype==LCT_RGBA) background=RGB(info.background_r,info.background_g,info.background_b);
        else background=RGB(info.background_r,info.background_r,info.background_r);
        RequireCanvas(background==matte,"bKGD is the display backdrop, never the originally transparent colour");
        RasterImage unchanged;ReadRaster(bitmap,unchanged);RequireCanvas(unchanged.pixels==source.pixels,"export leaves in-memory transparent RGB untouched");
        RequireCanvas(DoLoadImageFile(mainWindow,file,TRUE)!=NULL && DocumentBackdrop()==matte,"native reload restores backdrop with optimization/interlace and with or without private data");
        RequireCanvas(DocumentAlpha().hasColor==service && CanSavePaintData()==service && g_imageSaved,"standard background alone keeps checker swatch, no private data and clean document");
        RequireCanvas(DocumentRaster().pixels==decoded.pixels,"restoring backdrop never flattens or changes decoded alpha");
    }
    DeleteObject(bitmap);SetCurrentPngOptions({});SetPreservePaintData(true);InitializeImage(NULL,NULL,FALSE);
    puts("PASS PNG backdrop: standard bKGD, transparent RGB, optimized/interlaced and service on/off, exact partial alpha/visible pixels, unchanged in-memory image");
}
static void TestNewSprayAndIcons()
{
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);toolsModel.OnEndDraw(TRUE);registrySettings.SpraySmoothing=0;toolsModel.SetActiveTool(TOOL_AIRBRUSH);paletteModel.SetFgColor(RGB(0,0,0));
    for(int i=0;i<8;++i)
    {
        registrySettings.PanelMagnification=100;ApplyPanelScale();
        const POINT centers[]={{13,21},{31,21},{11,51},{29,51},{16,88},{34,88},{16,118},{34,118}};
        toolSettingsWindow.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(centers[i].x,centers[i].y));ReleaseCapture();
        RequireCanvas(registrySettings.SprayStyle==DWORD(i<4?0:i-3),"all eight spray choices hit the intended pattern");
        imageModel.PushImageForUndo(CreateColorDIB(180,160,RGB(255,255,255)));imageModel.ClearHistory();
        toolsModel.OnButtonDown(TRUE,80,80,FALSE);toolsModel.OnMouseMove(TRUE,90,85);
        TestBitmap first(180,160,RGB(255,255,255)),second(180,160,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(first.dc);toolsModel.OnDrawOverlayOnImage(second.dc);
        RequireCanvas(FigurePixels(first.dc)==FigurePixels(second.dc),"spray preview is deterministic while repainting");toolsModel.OnButtonUp(TRUE,90,85);
        auto previewPixels=FigurePixels(first.dc),committedPixels=FigurePixels(imageModel.GetDC());
        for(size_t p=0;p<previewPixels.size();p+=4) if(previewPixels[p]!=255)
            RequireCanvas(committedPixels[p]==previewPixels[p],"mouse release retains every preview spray dot");
        imageModel.Undo();
        RequireCanvas(GetPixel(imageModel.GetDC(),90,85)==RGB(255,255,255) && !imageModel.CanUndo(),"spray stroke has one complete undo");
    }
    for(int percent:{100,150,200,250}) for(int dark:{0,1})
    {
        registrySettings.PanelMagnification=percent;registrySettings.DarkTheme=dark;ApplyTheme();ApplyPanelScale();CStringW file;
        file.Format(L"text-warp-tests/spray-%d-%d.bmp",percent,dark);CaptureCanvasDialog(toolSettingsWindow,file);
        file.Format(L"text-warp-tests/tools-%d-%d.bmp",percent,dark);CaptureCanvasDialog(toolBoxContainer,file);
    }
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;registrySettings.SprayStyle=0;toolsModel.SetAirBrushRadius(5);ApplyTheme();ApplyPanelScale();toolsModel.SetActiveTool(TOOL_RECTSEL);
    puts("PASS eight spray patterns: native panel choices, deterministic preview, commit/Undo and enlarged reference icons in all scales/themes");
}
static void TestTextWarpFollowup()
{
    TestPartialSizesAndEditKeys();TestTextWarp();TestPngBackdrop();TestNewSprayAndIcons();
    if(testLanguages.empty()) EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    for(WORD lang:testLanguages)
    { for(UINT id=IDS_TEXT_WARP;id<=IDS_WARP_VDIST;++id) RequireCanvas(!ExactString(lang,id).IsEmpty(),"warp labels translated");
      for(UINT id=IDS_WARP_FIRST;id<IDS_WARP_FIRST+WARP_COUNT;++id) RequireCanvas(!ExactString(lang,id).IsEmpty(),"warp style translated");
      for(UINT id=IDS_SPRAY_FIRST;id<IDS_SPRAY_FIRST+4;++id) RequireCanvas(!ExactString(lang,id).IsEmpty(),"spray pattern translated"); }
    puts("PASS 27 new labels/styles in all 30 languages");
    if(!fontsDialog.IsWindow()) fontsDialog.Create(mainWindow);fontsDialog.ShowWindow(SW_SHOWNOACTIVATE);
    for(int i=0;i<30;++i)
    {
        mainWindow.SendMessage(WM_COMMAND,IDM_LANGUAGE_FIRST+i,0);
        HWND toolbar=fontsDialog.GetDlgItem(IDD_FONTSTOOLBAR);RECT button,client;SendMessage(toolbar,TB_GETRECT,IDM_TEXT_WARP,LPARAM(&button));GetClientRect(toolbar,&client);
        RequireCanvas(button.right<=client.right && button.bottom<=client.bottom,"warp toolbar button is fully visible");
        MapWindowPoints(toolbar,fontsDialog,reinterpret_cast<POINT*>(&button),2);fontsDialog.GetClientRect(&client);
        RequireCanvas(button.right<=client.right && button.bottom<=client.bottom,"warp toolbar fits the font dialog");
        if(i==2) CaptureCanvasDialog(fontsDialog,L"text-warp-tests/font-toolbar.bmp");
        warpDialogTicks=0;SetTimer(NULL,0,60,CheckWarpLocale);fontsDialog.SendMessage(WM_COMMAND,IDM_TEXT_WARP,0);
    }
    mainWindow.SendMessage(WM_COMMAND,LanguageCommand(L"Russian"),0);fontsDialog.ShowWindow(SW_HIDE);
    puts("PASS native warp toolbar action, translated dialog/list and unclipped controls in all 30 language variants");
}
