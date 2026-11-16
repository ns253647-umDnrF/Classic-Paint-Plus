static int InkMinX(HDC dc,int y0,int y1)
{ int left=10000;for(int y=y0;y<y1;++y) for(int x=0;x<400;++x) if(GetPixel(dc,x,y)!=RGB(255,255,255)) left=min(left,x);return left; }
static bool advancedCrop=false;
static VOID CALLBACK DriveAlphaCanvas(HWND,UINT,UINT_PTR timer,DWORD)
{
    HWND dialog=FindJpegDialogControl(IDC_CANVAS_FILL);if(!dialog) return;KillTimer(NULL,timer);
    SendDlgItemMessage(dialog,IDC_CANVAS_FILL,CB_SETCURSEL,6,0);SendMessage(dialog,WM_COMMAND,MAKEWPARAM(IDC_CANVAS_FILL,CBN_SELCHANGE),0);
    if(advancedCrop) for(int id=IDC_EDGE_LEFT;id<=IDC_EDGE_BOTTOM;++id) SetDlgItemInt(dialog,id,-4,TRUE);
    else { SetDlgItemInt(dialog,IDC_NEW_WIDTH,imageModel.GetWidth()+8,FALSE);SetDlgItemInt(dialog,IDC_NEW_HEIGHT,imageModel.GetHeight()+8,FALSE); }
    CaptureCanvasDialog(dialog,advancedCrop?L"advanced-dark-crop.bmp":L"advanced-dark-canvas.bmp");PostMessage(dialog,WM_COMMAND,IDOK,0);
}
static void TestAdvanced()
{
    toolsModel.OnEndDraw(TRUE);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    for(WORD language:testLanguages) for(UINT id=IDS_ADV_BASE;id<IDS_ADV_BASE+16;++id) RequireCanvas(!ExactString(language,id).IsEmpty(),"advanced strings present in every locale");
    registrySettings.RecentFontsCount=3;TrimRecentFonts();RememberFont(L"Arial");RememberFont(L"Times New Roman");RememberFont(L"Courier New");RememberFont(L"Arial");
    auto recent=RecentFonts();RequireCanvas(recent.size()==3 && recent[0]==L"Arial" && recent[1]==L"Courier New","font MRU ordering and deduplication");
    if(!fontsDialog.IsWindow()) fontsDialog.Create(mainWindow);fontsDialog.InitFontNames();HWND fonts=fontsDialog.GetDlgItem(IDD_FONTSNAMES);
    WCHAR name[LF_FACESIZE]={};SendMessageW(fonts,CB_GETLBTEXT,0,LPARAM(name));RequireCanvas(!lstrcmpW(name,L"Arial"),"recent font appears first in native combo");
    HWND toolbar=fontsDialog.GetDlgItem(IDD_FONTSTOOLBAR);RequireCanvas(SendMessageW(toolbar,TB_COMMANDTOINDEX,IDM_VERTICAL,0)<0,"vertical toolbar button removed");
    RequireCanvas(SendMessageW(toolbar,TB_COMMANDTOINDEX,IDM_ALIGN_JUSTIFY,0)>=0,"four alignment buttons installed");
    fontsDialog.ShowWindow(SW_SHOWNOACTIVATE);CaptureCanvasDialog(fontsDialog,L"advanced-font-toolbar.bmp");
    SendMessageW(fonts,CB_SHOWDROPDOWN,TRUE,0);COMBOBOXINFO fontInfo={sizeof(fontInfo)};GetComboBoxInfo(fonts,&fontInfo);CaptureCanvasDialog(fontInfo.hwndList,L"advanced-font-list.bmp");SendMessageW(fonts,CB_SHOWDROPDOWN,FALSE,0);fontsDialog.ShowWindow(SW_HIDE);
    registrySettings.RecentFontsCount=1;TrimRecentFonts();RequireCanvas(RecentFonts().size()==1,"changing MRU limit trims persisted list");
    registrySettings.RecentFontsCount=5;
    HMENU fontMenu=GetSubMenu(GetSubMenu(mainWindow.GetMenu(),4),6);
    EditMenuValue(fontMenu,L"9",false,9);EditMenuValue(fontMenu,L"2",true,9);EditMenuValue(fontMenu,L"31",false,9);EditMenuValue(fontMenu,L"5",false,5);
    TestBitmap frame(80,50,RGB(255,255,255));DrawDashedRect(frame.dc,CRect(4,4,76,46));
    for(int x=4;x<72;++x) RequireCanvas(GetPixel(frame.dc,x,4)==(((x-4)%8<4)?RGB(0,0,0):RGB(255,255,255)),"selection frame exact black 4/4");
    TestBitmap original(16,16,RGB(12,34,56));HBITMAP extended=MakeCanvasBitmap(original.dc,16,16,24,24,4,4,CLR_INVALID);RasterImage rgba;ReadRaster(extended,rgba);DeleteObject(extended);
    RequireCanvas(rgba.pixels[0]>>24==0 && rgba.pixels[8*24+8]==0xff0c2238,"transparent canvas expansion retains original pixels");
    imageModel.PushImageForUndo(MakeRasterBitmap(rgba));imageModel.ClearHistory();registrySettings.ReturnAfterColorPick=FALSE;
    toolsModel.SetZoom(1000);canvasWindow.updateScrollRange();canvasWindow.updateScrollPos(0,0);
    for(bool cancel:{false,true})
    {
        samplePoint={1,1};canvasWindow.ImageToCanvas(samplePoint);cancelDialog=cancel;ticks=0;exercised=false;
        BYTE alpha=77;COLORREF color=RGB(12,34,56);UINT_PTR timer=SetTimer(NULL,0,30,DriveDialog);
        BOOL accepted=ChooseColorFromCanvas(mainWindow,&color,&alpha);KillTimer(NULL,timer);
        RequireCanvas(accepted==!cancel && alpha==(cancel?77:0),"palette-dialog eyedropper samples transparency, Cancel preserves alpha");
    }
    toolsModel.SetActiveTool(TOOL_COLOR);toolsModel.OnButtonDown(TRUE,0,0,FALSE);toolsModel.OnButtonUp(TRUE,0,0);
    RequireCanvas(paletteModel.GetFgAlpha()==0,"eyedropper samples transparent alpha");
    toolsModel.SetActiveTool(TOOL_FILL);toolsModel.OnButtonDown(TRUE,8,8,FALSE);toolsModel.OnButtonUp(TRUE,8,8);
    RequireCanvas(RasterPixel(imageModel.GetDC(),8,8)>>24==0,"fill applies sampled transparency");imageModel.Undo();RequireCanvas(RasterPixel(imageModel.GetDC(),8,8)>>24==255,"transparent fill undo restores opacity");
    paletteModel.SetFgColor(RGB(0,0,0));paletteModel.SetBgColor(RGB(255,255,255));
    toolsModel.SetActiveTool(TOOL_BRUSH);HWND panel=GetDlgItem(toolSettingsWindow,IDC_BRUSH_PANEL);RequireCanvas(IsWindowVisible(panel),"new brush panel visible");
    for(int shape=0;shape<4;++shape)
    {
        SendMessage(panel,WM_COMMAND,IDC_BRUSH_SHAPE_FIRST+shape,0);
        HWND list=GetDlgItem(panel,IDC_BRUSH_PRESET_FIRST);RequireCanvas(SendMessage(list,LB_GETCOUNT,0,0)==3,"one row of three brush presets");
        for(int i=0;i<3;++i) { SendMessage(list,LB_SETCURSEL,i,0);SendMessage(panel,WM_COMMAND,MAKEWPARAM(IDC_BRUSH_PRESET_FIRST,LBN_SELCHANGE),LPARAM(list));const int round[]={7,4,1},other[]={8,5,2};int expected=shape==0?round[i]:other[i];
            RequireCanvas(toolsModel.GetBrushWidth()==expected && GetDlgItemInt(panel,IDC_BRUSH_SIZE_EDIT,NULL,FALSE)==expected && !GetDlgItem(panel,IDC_BRUSH_SIZE),"brush presets synchronize shape and numeric input, size slider removed"); }
    }
    SetDlgItemTextW(panel,IDC_BRUSH_SIZE_EDIT,L"127");SendDlgItemMessage(panel,IDC_BRUSH_SIZE_EDIT,WM_KEYDOWN,VK_RETURN,0);
    RequireCanvas(toolsModel.GetBrushWidth()==127,"brush size manual input");
    for(int hardness:{0,50,100})
    {
        imageModel.PushImageForUndo(CreateColorDIB(160,160,RGB(255,255,255)));imageModel.ClearHistory();toolsModel.SetBrushStyle(BrushStyleRound);toolsModel.SetBrushWidth(40);registrySettings.BrushHardness=hardness;
        toolsModel.OnButtonDown(TRUE,60,80,FALSE);toolsModel.OnMouseMove(TRUE,100,80);
        TestBitmap preview(160,160,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(preview.dc);toolsModel.OnButtonUp(TRUE,100,80);
        RequireCanvas(FigurePixels(preview.dc)==FigurePixels(imageModel.GetDC()),"soft brush preview and commit agree");
        RequireCanvas(GetPixel(imageModel.GetDC(),60,80)==RGB(0,0,0) && GetRValue(GetPixel(imageModel.GetDC(),80,80))<5,"soft brush center opaque and stroke continuous");
        RequireCanvas(hardness==100 || GetRValue(GetPixel(imageModel.GetDC(),80,94))>0,"brush hardness creates gradual edge");
        imageModel.Undo();RequireCanvas(GetPixel(imageModel.GetDC(),80,80)==RGB(255,255,255),"soft brush single-step undo");
    }
    for(int size:{1,2,3,7,8,127,500}) for(int shape=0;shape<4;++shape)
    { TestBitmap sample(520,520,RGB(255,255,255));POINT center={260,260};DrawSoftBrushStroke(sample.dc,&center,1,shape,size,100,RGB(0,0,0),255);
      RequireCanvas(GetPixel(sample.dc,260,260)==RGB(0,0,0),"all brush shapes and boundary sizes draw"); }
    imageModel.PushImageForUndo(CreateColorDIB(400,260,RGB(255,255,255)));imageModel.ClearHistory();toolsModel.SetActiveTool(TOOL_TEXT);toolsModel.SetBackgroundTransparent(TRUE);
    if(!textEditWindow.IsWindow()) textEditWindow.Create(canvasWindow);RequireCanvas(textEditWindow.IsWindow(),"RichEdit text editor created");
    registrySettings.strFontName=L"Arial";registrySettings.PointSize=18;textEditWindow.UpdateFont();textEditWindow.ResetRotation();
    textEditWindow.ValidateEditRect(CRect(40,40,340,200));textEditWindow.SetWindowTextW(L"Paint");textEditWindow.ShowWindow(SW_SHOWNOACTIVATE);
    int positions[3];
    for(int alignment=0;alignment<3;++alignment)
    { fontsDialog.SendMessage(WM_COMMAND,IDM_ALIGN_LEFT+alignment,0);TestBitmap result(400,260,RGB(255,255,255));textEditWindow.RenderDocument(result.dc);positions[alignment]=InkMinX(result.dc,40,95); }
    printf("Text alignment x: %d %d %d\n",positions[0],positions[1],positions[2]);
    RequireCanvas(positions[0]<positions[1] && positions[1]<positions[2] && positions[2]<340,"text pixels honor left, center and right paragraph alignment");
    registrySettings.TextAlignment=3;textEditWindow.UpdateFont();textEditWindow.SetWindowTextW(L"One two three four five six seven eight nine ten eleven twelve thirteen fourteen.");
    TestBitmap justified(400,260,RGB(255,255,255));textEditWindow.RenderDocument(justified.dc);RequireCanvas(InkMinX(justified.dc,40,95)<60,"justified paragraph renders");
    int right=0;for(int y=40;y<68;++y) for(int x=40;x<340;++x) if(GetPixel(justified.dc,x,y)!=RGB(255,255,255)) right=max(right,x);
    RequireCanvas(right>=335,"justification stretches the first wrapped line to the right margin");
    RECT textBounds;textEditWindow.GetEditRect(&textBounds);auto justifiedPixels=FigurePixels(justified.dc);
    for(int zoom:{125,1000,2000,8000,1000})
    {
        toolsModel.SetZoom(zoom);RECT bounds;textEditWindow.GetEditRect(&bounds);RequireCanvas(EqualRect(&bounds,&textBounds),"text image bounds survive zoom changes");
        TestBitmap zoomText(400,260,RGB(255,255,255));textEditWindow.RenderDocument(zoomText.dc);
        RequireCanvas(FigurePixels(zoomText.dc)==justifiedPixels,"text rasterization stays identical at all editor zooms");
    }
    textEditWindow.SetWindowTextW(L"Editable rotation");registrySettings.TextAlignment=0;textEditWindow.UpdateFont();
    RECT editRect;textEditWindow.GetWindowRect(&editRect);POINT start={editRect.right-1,(editRect.top+editRect.bottom)/2};
    textEditWindow.SendMessage(WM_NCRBUTTONDOWN,HTRIGHT,MAKELPARAM(start.x,start.y));
    POINT moved={(editRect.left+editRect.right)/2,editRect.bottom+70};textEditWindow.ScreenToClient(&moved);textEditWindow.SendMessage(WM_MOUSEMOVE,MK_RBUTTON,MAKELPARAM(moved.x,moved.y));
    RequireCanvas(fabs(textEditWindow.Angle()-90)<1,"right border drag rotates text 90 degrees");textEditWindow.SendMessage(WM_RBUTTONUP,0,MAKELPARAM(moved.x,moved.y));
    textEditWindow.SendMessage(WM_NCRBUTTONDOWN,HTRIGHT,MAKELPARAM(start.x,start.y));SetTestShift(true);
    POINT snapped={editRect.right-12,editRect.bottom-17};textEditWindow.ScreenToClient(&snapped);textEditWindow.SendMessage(WM_MOUSEMOVE,MK_RBUTTON|MK_SHIFT,MAKELPARAM(snapped.x,snapped.y));
    RequireCanvas(fabs(fmod(textEditWindow.Angle(),15))<.001,"Shift snaps text rotation to 15 degrees");
    WCHAR angleLabel[128]={};SendMessageW(g_hStatusBar,SB_GETTEXT,2,LPARAM(angleLabel));RequireCanvas(wcslen(angleLabel)>0,"text angle shown in status bar");
    textEditWindow.SendMessage(WM_KEYDOWN,VK_ESCAPE,0);SetTestShift(false);RequireCanvas(fabs(textEditWindow.Angle()-90)<1,"Escape cancels only rotation gesture");
    textEditWindow.SendMessage(WM_CHAR,L'!',1);RequireCanvas(textEditWindow.GetWindowTextLength()>17,"rotated text remains editable");
    TestBitmap rotated(400,260,RGB(255,255,255));textEditWindow.RenderDocument(rotated.dc);CaptureCanvasDialog(mainWindow,L"advanced-text-rotation.bmp");
    toolsModel.OnEndDraw(FALSE);RequireCanvas(FigurePixels(rotated.dc)==FigurePixels(imageModel.GetDC()),"rotated text preview equals committed pixels");imageModel.Undo();
    RequireCanvas(GetPixel(imageModel.GetDC(),200,80)==RGB(255,255,255),"rotated text undo");
    imageModel.PushImageForUndo(MakeTransparentBitmap(400,260));toolsModel.SetActiveTool(TOOL_TEXT);textEditWindow.ResetRotation();
    textEditWindow.SetWindowTextW(L"Transparent text");textEditWindow.ValidateEditRect(CRect(40,40,340,200));textEditWindow.ShowWindow(SW_SHOWNOACTIVATE);
    canvasWindow.zoomTo(1000);RECT client;canvasWindow.GetClientRect(&client);TestBitmap liveText(client.right,client.bottom,RGB(255,0,255));
    canvasWindow.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);HDC visible=canvasWindow.GetDC();BitBlt(liveText.dc,0,0,client.right,client.bottom,visible,0,0,SRCCOPY);canvasWindow.ReleaseDC(visible);
    POINT a={60,140},b={68,140};canvasWindow.ImageToCanvas(a);canvasWindow.ImageToCanvas(b);
    RequireCanvas(GetPixel(liveText.dc,a.x,a.y)==RGB(224,224,224) || GetPixel(liveText.dc,a.x,a.y)==RGB(255,255,255),"buffered live text paints the transparent checker");
    RequireCanvas(GetPixel(liveText.dc,a.x,a.y)!=GetPixel(liveText.dc,b.x,b.y),"live text background keeps document transparency instead of an opaque rectangle");
    CaptureCanvasDialog(mainWindow,L"advanced-transparent-text.bmp");toolsModel.OnEndDraw(FALSE);auto textRaster=DocumentRaster();
    RequireCanvas(textRaster.pixels[140*400+60]>>24==0,"committing transparent text preserves background alpha");
    imageModel.PushImageForUndo(CreateColorDIB(400,260,RGB(255,255,255)));
    toolsModel.SetActiveTool(TOOL_BRUSH);toolsModel.SetBrushWidth(25);registrySettings.BrushHardness=35;UpdateBrushPanel();
    for(int theme:{0,1,0})
    {
        auto before=FigurePixels(imageModel.GetDC());HandleSettingsCommand(mainWindow,theme?IDM_THEME_DARK:IDM_THEME_LIGHT);UpdateSettingsChecks(mainWindow.GetMenu());
        RequireCanvas(registrySettings.DarkTheme==theme,"theme command switches theme");RequireCanvas(FigurePixels(imageModel.GetDC())==before,"UI theme does not recolor canvas");
        RequireCanvas(!!(GetMenuState(mainWindow.GetMenu(),IDM_THEME_DARK,MF_BYCOMMAND)&MF_CHECKED)==bool(theme),"theme checkbox follows active theme");
        UpdateWindow(mainWindow);CaptureCanvasDialog(mainWindow,theme?L"advanced-dark.bmp":L"advanced-light.bmp");
    }
    registrySettings.DarkTheme=1;registrySettings.BrushSize=127;registrySettings.BrushShape=2;registrySettings.BrushHardness=35;registrySettings.TextAlignment=3;registrySettings.RecentFontsCount=7;registrySettings.StorePreferences();
    ApplyTheme();toolsModel.SetActiveTool(TOOL_RECTSEL);
    for(bool crop:{false,true})
    { advancedCrop=crop;SetTimer(NULL,0,60,DriveAlphaCanvas);ShowCanvasDialog(mainWindow,crop);RequireCanvas(RasterPixel(imageModel.GetDC(),0,0)>>24==0,"native canvas dialog adds transparent border");imageModel.Undo(); }
    RegistrySettings restored;restored.Load(SW_HIDE);RequireCanvas(restored.DarkTheme==1 && restored.BrushSize==4 && restored.BrushShape==0 && restored.BrushHardness==100 && restored.TextAlignment==3 && restored.RecentFontsCount==7,"UI preferences persist while brush settings use startup defaults");
    registrySettings.DarkTheme=0;ApplyTheme();registrySettings.TextAlignment=0;registrySettings.BrushHardness=100;toolsModel.SetActiveTool(TOOL_RECTSEL);registrySettings.StorePreferences();
    PngOptions plain,optimized;optimized.optimize=optimized.interlaced=true;plain.SaveDefaults();SetCurrentPngOptions(optimized);
    HBITMAP png=imageModel.CopyBitmap();RequireCanvas(SaveDIBToFile(png,L"format-tests/once-options.png",FALSE),"save with temporary PNG options");
    std::vector<BYTE> bytes;ReadImageBytes(L"format-tests/once-options.png",bytes);RequireCanvas(bytes.size()>28 && bytes[28]==1,"temporary PNG interlace used for current file");
    RequireCanvas(!CurrentPngOptions().optimize && !CurrentPngOptions().interlaced,"temporary PNG options reset after successful save");
    optimized.SaveDefaults();SetCurrentPngOptions(plain);RequireCanvas(SaveDIBToFile(png,L"format-tests/default-options.png",FALSE),"save with explicit PNG defaults available");
    RequireCanvas(CurrentPngOptions().optimize && CurrentPngOptions().interlaced,"explicit PNG defaults survive save reset");DeleteObject(png);plain.SaveDefaults();SetCurrentPngOptions(plain);
    puts("PASS advanced fonts, alignment, rotation, brush, alpha fill, themes and preferences");
}
