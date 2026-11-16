#include <colordlg.h>
static void TestPreparedDimensions()
{
    CreateDirectoryW(L"alpha-followup-tests",NULL);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    mainWindow.SetWindowPos(NULL,0,0,1040,900,SWP_NOACTIVATE|SWP_NOZORDER);canvasWindow.zoomTo(1000);
    toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();SetTestShift(false);
    imageModel.PushImageForUndo(CreateColorDIB(600,450,RGB(255,255,255)));imageModel.ClearHistory();
    HWND width=toolSettingsWindow.GetDlgItem(IDC_OBJECT_WIDTH),height=toolSettingsWindow.GetDlgItem(IDC_OBJECT_HEIGHT);
    for(int percent:{100,150,200,250}) for(TOOLTYPE tool:{TOOL_FIGURE,TOOL_TEXT,TOOL_RECT,TOOL_TEXT})
    {
        registrySettings.PanelMagnification=percent;ApplyPanelScale();toolsModel.SetActiveTool(tool);
        RequireCanvas(IsWindowEnabled(width) && IsWindowEnabled(height),"both dimensions enabled before creation");
        RECT h,p;GetWindowRect(height,&h);GetWindowRect(toolSettingsWindow,&p);
        RequireCanvas(h.bottom<=p.bottom,"figure height field fully laid out without resizing app");
        EnterObjectValue(width,L"173 px");EnterObjectValue(height,L"67 px");
        bool text=tool==TOOL_TEXT;if(text) textEditWindow.SetRotationAngle(37);else toolsModel.SetFigureAngle(37);
        FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,140,120);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,310,240);FigureMouse(WM_LBUTTONUP,0,310,240);
        CRect r;if(text) textEditWindow.GetEditRect(&r);else RequireCanvas(toolsModel.EditableFigureBounds(r),"new figure stays editable");
        RequireCanvas(r.Width()==173 && r.Height()==67,"prepared dimensions applied exactly on creation at an arbitrary angle");
        RequireCanvas(WindowLabel(width)==L"173 px" && WindowLabel(height)==L"67 px","created dimensions synchronized with controls");
        if(percent==100) CaptureCanvasDialog(toolSettingsWindow,text?L"alpha-followup-tests/text-panel.bmp":L"alpha-followup-tests/figure-panel.bmp");
        toolsModel.OnEndDraw(TRUE);EnterObjectValue(width,L"");EnterObjectValue(height,L"");
        RequireCanvas(!toolsModel.ObjectSizePreset(text).cx && !toolsModel.ObjectSizePreset(text).cy,"blank pre-creation fields restore mouse sizing");
    }
    toolsModel.ObjectSizePreset(false)={};toolsModel.ObjectSizePreset(true)={};
    registrySettings.PanelMagnification=100;ApplyPanelScale();textEditWindow.ResetRotation();toolsModel.SetFigureAngle(0);toolsModel.SetActiveTool(TOOL_RECTSEL);
    puts("PASS prepared dimensions: text/figures, preset application, exact rotated sizes, switching and immediate full panel layout at all scales");
}
static void TestEraserSettingsAndDelete()
{
    toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();toolsModel.SetActiveTool(TOOL_BRUSH);toolsModel.SetBrushWidth(19);toolsModel.SetBrushStyle(BrushStyleForeSlash);registrySettings.BrushHardness=42;
    toolsModel.SetActiveTool(TOOL_RUBBER);HWND panel=toolSettingsWindow.GetDlgItem(IDC_BRUSH_PANEL),list=GetDlgItem(panel,IDC_BRUSH_PRESET_FIRST),size=GetDlgItem(panel,IDC_BRUSH_SIZE_EDIT),slider=GetDlgItem(panel,IDC_BRUSH_HARDNESS);
    for(int percent:{100,150,200,250})
    {
        registrySettings.PanelMagnification=percent;ApplyPanelScale();RequireCanvas(IsWindowVisible(panel),"eraser uses brush settings panel");
        for(int shape=0;shape<4;++shape)
        {
            SendMessage(GetDlgItem(panel,IDC_BRUSH_SHAPE_FIRST+shape),BM_CLICK,0,0);
            RequireCanvas(registrySettings.EraserShape==shape && SendMessage(list,LB_GETCOUNT,0,0)==3,"eraser shape updates three presets");
            SendMessage(list,LB_SETCURSEL,0,0);SendMessage(panel,WM_COMMAND,MAKEWPARAM(IDC_BRUSH_PRESET_FIRST,LBN_SELCHANGE),LPARAM(list));
            RequireCanvas(toolsModel.GetEraserSize()==(shape?8:7),"eraser preset sets exact size");
        }
        EnterObjectValue(size,L"29 px");SendMessage(slider,TBM_SETPOS,TRUE,65);SendMessage(panel,WM_VSCROLL,TB_THUMBPOSITION,LPARAM(slider));
        RequireCanvas(toolsModel.GetEraserSize()==29 && registrySettings.EraserHardness==35,"eraser manual size and hardness apply");
        RECT a,b;GetWindowRect(slider,&a);GetWindowRect(panel,&b);RequireCanvas(a.bottom<=b.bottom,"eraser hardness visible at every scale");
        RequireCanvas(toolsModel.GetBrushWidth()==19 && toolsModel.GetBrushStyle()==BrushStyleForeSlash && registrySettings.BrushHardness==42,"eraser leaves brush settings unchanged");
    }
    registrySettings.PanelMagnification=100;ApplyPanelScale();registrySettings.StorePreferences();RegistrySettings loaded;loaded.Load(SW_HIDE);
    RequireCanvas(loaded.EraserSize==8 && loaded.EraserHardness==100 && loaded.EraserShape==1,"eraser uses square 8px at full hardness after loading settings");
    paletteModel.SetBgColor(RGB(22,57,103),0); // Standard erase must still be opaque.
    for(int alpha:{0,1}) for(int replace:{0,1})
    {
        auto fresh=[&]() { toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();HBITMAP b=CreateColorDIB(100,80,RGB(200,100,50));ResetDocumentAlpha(b);imageModel.PushImageForUndo(b);imageModel.ClearHistory();DocumentAlpha().enabled=alpha!=0;registrySettings.ReplaceDeletedWithAlpha=replace; };
        fresh();toolsModel.SetActiveTool(TOOL_RECTSEL);toolsModel.OnButtonDown(TRUE,10,10,FALSE);toolsModel.OnMouseMove(TRUE,30,30);toolsModel.OnButtonUp(TRUE,30,30);selectionModel.DeleteSelection();
        DWORD pixel=RasterPixel(imageModel.GetDC(),20,20);RequireCanvas(alpha && replace?(pixel>>24)==0:pixel==0xff163967,"delete respects alpha replacement checkbox and otherwise uses opaque background");
        imageModel.Undo();RequireCanvas(RasterPixel(imageModel.GetDC(),20,20)==0xffc86432,"delete undo restores original pixel");
        for(int shape=0;shape<4;++shape)
        {
            fresh();toolsModel.SetActiveTool(TOOL_RUBBER);toolsModel.SetEraserShape(shape);toolsModel.SetEraserSize(21);registrySettings.EraserHardness=50;
            toolsModel.OnButtonDown(TRUE,40,40,FALSE);toolsModel.OnMouseMove(TRUE,60,40);toolsModel.OnButtonUp(TRUE,60,40);
            pixel=RasterPixel(imageModel.GetDC(),50,40);RequireCanvas(alpha && replace?(pixel>>24)==0:pixel==0xff163967,"all eraser shapes obey alpha/background switch");
            if(shape==0) { DWORD edge=RasterPixel(imageModel.GetDC(),50,48);RequireCanvas(alpha && replace?(edge>>24)>0 && (edge>>24)<255:edge!=pixel && edge!=0xffc86432,"eraser hardness produces a smooth edge"); }
            imageModel.Undo();RequireCanvas(RasterPixel(imageModel.GetDC(),50,40)==0xffc86432,"eraser stroke is one undo step");
        }
    }
    registrySettings.ReplaceDeletedWithAlpha=1;paletteModel.SetBgColor(paletteModel.GetBgColor(),255);toolsModel.SetActiveTool(TOOL_RECTSEL);
    HMENU image=GetSubMenu(mainWindow.GetMenu(),3),settings=GetSubMenu(mainWindow.GetMenu(),4);HMENU alpha=GetSubMenu(image,GetMenuItemCount(image)-1);
    RequireCanvas(alpha && GetMenuItemID(alpha,0)==IDM_IMAGEDRAWOPAQUE && GetMenuItemID(alpha,1)==IDM_ALPHA_REPLACE,"alpha commands grouped in Drawing submenu");
    RequireCanvas(GetMenuState(settings,IDM_IMAGEDRAWOPAQUE,MF_BYCOMMAND)==UINT(-1),"alpha command removed from Settings");
    puts("PASS eraser and delete: four shapes, presets, manual pixels, hardness, session settings, alpha/background replacement and Undo");
}
static int hueTestMode=0,hueTicks=0;
static VOID CALLBACK DriveHuePicker(HWND,UINT,UINT_PTR timer,DWORD)
{
    RequireCanvas(++hueTicks<100,"hue picker appears");HWND dialog=FindJpegDialogControl(COLOR_LUM);if(!dialog) return;
    KillTimer(NULL,timer);
    if(hueTestMode!=2)
    {
        HWND plane=GetDlgItem(dialog,COLOR_RAINBOW);RECT r;GetWindowRect(plane,&r);MapWindowPoints(NULL,dialog,reinterpret_cast<POINT*>(&r),2);
        LPARAM point=MAKELPARAM(r.left+(r.right-r.left)*2/3,r.top+(r.bottom-r.top)/4);
        SendMessage(dialog,WM_LBUTTONDOWN,MK_LBUTTON,point);SendMessage(dialog,WM_LBUTTONUP,0,point);
        RequireCanvas(GetDlgItemInt(dialog,COLOR_LUM,NULL,FALSE)==120,"white or black hue click resets extreme luminance");
        CaptureCanvasDialog(dialog,L"alpha-followup-tests/white-hue.bmp");
    }
    PostMessage(dialog,WM_COMMAND,hueTestMode==3?IDCANCEL:IDOK,0);
}
static void TestWhiteHuePicker()
{
    for(hueTestMode=0;hueTestMode<4;++hueTestMode)
    {
        COLORREF color=hueTestMode==1?RGB(0,0,0):RGB(255,255,255),original=color;hueTicks=0;
        SetTimer(NULL,0,100,DriveHuePicker);bool ok=ChooseColorFromCanvas(mainWindow,&color)!=FALSE;
        RequireCanvas(ok==(hueTestMode!=3),"color dialog honors OK/Cancel");
        if(hueTestMode<2) RequireCanvas(color!=original && GetRValue(color)!=GetBValue(color),"direct hue selection changes white/black without selecting a preset");
        else RequireCanvas(color==original,"opening picker or cancelling leaves white untouched");
    }
    puts("PASS native color picker: direct hue click from white/black, untouched white on OK, exact Cancel");
}
static void TestPaintServiceData()
{
    toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();RasterImage source;source.width=source.height=32;source.pixels.resize(1024);
    for(int y=0;y<32;++y) for(int x=0;x<32;++x) source.pixels[y*32+x]=(DWORD(x<8?0:x<16?128:255)<<24)|0x103050|(DWORD(y)<<16);
    imageModel.PushImageForUndo(MakeRasterBitmap(source));imageModel.ClearHistory();auto& state=DocumentAlpha();state={};state.enabled=true;
    RequireCanvas(!CanSavePaintData(),"ordinary imported alpha cannot save untouched service data");
    SetDocumentBackdrop(RGB(42,91,180));RequireCanvas(CanSavePaintData(),"backdrop customization enables service data without a color key");
    state.hasColor=true;state.color=RGB(10,90,210);auto expected=CurrentPaintData();
    SetCurrentPngOptions(PngOptions{});SetCurrentWebpOptions(WebpOptions{});SetCurrentIcoOptions(IcoOptions{});
    const SaveFormat formats[]={SaveFormat::Png,SaveFormat::WebP,SaveFormat::Tga,SaveFormat::Tiff,SaveFormat::Ico};
    const LPCWSTR suffixes[]={L"png",L"webp",L"tga",L"tif",L"ico"};
    for(int i=0;i<5;++i) for(int preserve:{0,1})
    {
        state.enabled=state.customized=state.hasColor=true;state.color=expected.color;state.backdrop=expected.backdrop;SetPreservePaintData(preserve!=0);
        CStringW file;file.Format(L"alpha-followup-tests/service-%d.%s",preserve,suffixes[i]);
        HBITMAP bitmap=MakeRasterBitmap(source);RequireCanvas(SaveCustomImage(bitmap,file,formats[i],FALSE),"save image with optional service colors");DeleteObject(bitmap);
        ImageFileInfo info;RasterImage decoded;RasterImage exported=source;
        if(formats[i]==SaveFormat::Png) for(DWORD& p:exported.pixels) if(!(p>>24)) p=RGB(GetBValue(state.backdrop),GetGValue(state.backdrop),GetRValue(state.backdrop));
        RequireCanvas(LoadCustomImage(file,decoded,info) && decoded.pixels==exported.pixels,"metadata preserves pixels; PNG transparent RGB follows the backdrop");
        RequireCanvas(info.paintData.present==(preserve!=0),"private data present only when requested");
        if(preserve) RequireCanvas(info.paintData.hasColor && info.paintData.color==expected.color && info.paintData.backdrop==expected.backdrop,"both service colors round-trip exactly");
        if(formats[i]!=SaveFormat::Ico)
        {
            RequireCanvas(DoLoadImageFile(mainWindow,file,TRUE)!=NULL,"native main document loads metadata");
            RequireCanvas(DocumentAlpha().enabled && CanSavePaintData()==(preserve!=0),"opening own metadata restores eligible alpha mode");
            COLORREF importedBackdrop=formats[i]==SaveFormat::Png?expected.backdrop:RGB(255,255,255);
            RequireCanvas(preserve?DocumentAlpha().hasColor && DocumentAlpha().color==expected.color && DocumentBackdrop()==expected.backdrop:!DocumentAlpha().hasColor && DocumentBackdrop()==importedBackdrop,"native load restores service swatches, standard PNG background or white default");
        }
    }
    // Checker swatch itself is meaningful: only the matte was changed.
    state.enabled=state.customized=true;state.hasColor=false;state.backdrop=RGB(51,101,151);SetPreservePaintData(true);
    std::vector<BYTE> png;RequireCanvas(EncodePng(source,PngOptions{},png),"encode test PNG");auto data=CurrentPaintData();
    RequireCanvas(SetPngPaintData(png,&data) && ReadPngPaintData(png).present && !ReadPngPaintData(png).hasColor,"checker swatch plus customized backdrop round trip");
    auto text=SerializePaintData(data);for(size_t n=0;n<text.size();++n) RequireCanvas(!ParsePaintData(reinterpret_cast<const BYTE*>(text.data()),n).present,"truncated service record rejected");
    text[17]='9';RequireCanvas(!ParsePaintData(reinterpret_cast<const BYTE*>(text.data()),text.size()).present,"unknown service version rejected");
    for(size_t p=8;p+12<png.size();++p) if(!memcmp(png.data()+p,"cpPs",4)) { png[p+9]^=1;break; }
    RequireCanvas(!ReadPngPaintData(png).present,"bad private chunk CRC ignored");RasterImage decoded;
    RequireCanvas(DecodePng(png,decoded) && decoded.pixels==source.pixels,"malformed ancillary data never damages image decoding");
    InitializeImage(NULL,NULL,FALSE);SetPreservePaintData(true);
    puts("PASS service metadata: PNG/WebP/TGA/TIFF/ICO, both colors, checker state, optional omission, exact RGBA, automatic native restoration and malformed records");
}
static int paintSaveStage=0,paintSaveTicks=0;static bool paintSaveEligible=false;
static CStringW paintSaveRoot;
static VOID CALLBACK DrivePaintSave(HWND,UINT,UINT_PTR timer,DWORD)
{
    RequireCanvas(++paintSaveTicks<150,"Save As metadata test completes");HWND hook=FindSaveHook();if(!hook) return;
    HWND parent=GetParent(hook);int index=paintSaveStage/2;
    if(index==12) { KillTimer(NULL,timer);PostMessage(parent,WM_COMMAND,IDCANCEL,0);return; }
    if(!(paintSaveStage%2))
    {
        HWND combo=GetDlgItem(parent,cmb1);SendMessage(combo,CB_SETCURSEL,index,0);
        SendMessage(parent,WM_COMMAND,MAKEWPARAM(cmb1,CBN_SELCHANGE),LPARAM(combo));SendMessage(parent,WM_COMMAND,MAKEWPARAM(cmb1,CBN_SELENDOK),LPARAM(combo));
    }
    else
    {
        SaveFormat f=SaveFormatFromFilterIndex(index+1);HWND jpeg=GetDlgItem(hook,IDC_JPEG_PRESERVE),service=GetDlgItem(hook,IDC_PAINT_DATA);
        RequireCanvas(!!IsWindowVisible(jpeg)==(f==SaveFormat::Jpeg),"JPEG metadata is hidden for irrelevant formats");
        RequireCanvas(!!IsWindowVisible(service)==SupportsPaintData(f),"service data checkbox visible only for supported formats");
        RequireCanvas(!!IsWindowEnabled(service)==paintSaveEligible,"service eligibility reflected in disabled checkbox");
        if(f==SaveFormat::Png)
        {
            CaptureCanvasDialog(parent,paintSaveRoot+(paintSaveEligible?L"\\alpha-followup-tests\\png-save-enabled.bmp":L"\\alpha-followup-tests\\png-save-disabled.bmp"));
            if(paintSaveEligible) SendMessage(service,BM_CLICK,0,0);
        }
    }
    ++paintSaveStage;
}
static void TestPaintSaveControls()
{
    WCHAR directory[MAX_PATH];GetCurrentDirectoryW(_countof(directory),directory);paintSaveRoot=directory;
    for(int mode=0;mode<3;++mode)
    {
        auto& state=DocumentAlpha();state={};state.enabled=mode!=0;state.customized=mode==2;paintSaveEligible=mode==2;
        paintSaveStage=paintSaveTicks=0;SetPreservePaintData(true);WCHAR file[MAX_PATH];GetFullPathNameW(L"alpha-followup-tests/test.png",_countof(file),file,NULL);SetTimer(NULL,0,60,DrivePaintSave);
        BOOL accepted=mainWindow.GetSaveFileName(file,_countof(file));
        if(accepted || paintSaveStage!=24) printf("Save As mode=%d stage=%d ticks=%d accepted=%d error=%lu\n",mode,paintSaveStage,paintSaveTicks,accepted,CommDlgExtendedError());
        RequireCanvas(!accepted && paintSaveStage==24,"cancel native Save As after testing every format");
        RequireCanvas(PreservePaintData(),"cancel does not commit changed service checkbox");
    }
    InitializeImage(NULL,NULL,FALSE);puts("PASS real Save As: all 12 filters, hidden irrelevant metadata, disabled untouched alpha state, staged checkbox and Cancel");
}
static void TestAlphaFollowup()
{
    TestPreparedDimensions();TestEraserSettingsAndDelete();TestWhiteHuePicker();TestPaintServiceData();TestPaintSaveControls();
    if(testLanguages.empty()) EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    for(WORD language:testLanguages) for(UINT id:{IDS_ALPHA_USE,IDS_ALPHA_REPLACE,IDS_PAINT_DATA}) RequireCanvas(!ExactString(language,id).IsEmpty(),"new menu and save labels translated");
    puts("PASS alpha follow-up translations in all 30 languages");
}
