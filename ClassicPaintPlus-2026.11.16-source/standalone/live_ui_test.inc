static CStringW WindowLabel(HWND window)
{ CStringW text;int n=GetWindowTextLengthW(window);GetWindowTextW(window,text.GetBuffer(n+1),n+1);text.ReleaseBuffer();return text; }
static UINT LanguageCommand(LPCWSTR name)
{
    HMENU languages=GetSubMenu(GetSubMenu(mainWindow.GetMenu(),4),5);WCHAR text[128];
    for(int i=0;i<GetMenuItemCount(languages);++i) { GetMenuStringW(languages,i,text,128,MF_BYPOSITION);if(!lstrcmpW(text,name)) return GetMenuItemID(languages,i); }
    RequireCanvas(false,"find live language command");return 0;
}
static void TestLiveUi()
{
    CreateDirectoryW(L"live-ui-tests",NULL);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);mainWindow.SetWindowPos(NULL,0,0,960,780,SWP_NOACTIVATE|SWP_NOZORDER);
    toolsModel.OnEndDraw(TRUE);toolsModel.SetActiveTool(TOOL_RECTSEL);registrySettings.DarkTheme=0;registrySettings.PanelMagnification=100;ApplyTheme();ApplyPanelScale();canvasWindow.zoomTo(1000);
    HMENU customMenu=mainWindow.GetMenu(),nativeMenu=LoadMenuW(g_hinstExe,MAKEINTRESOURCEW(ID_MENU));BuildSettingsMenu(nativeMenu);BuildCanvasMenu(nativeMenu);
    RECT nativeRects[6];SetMenu(mainWindow,nativeMenu);DrawMenuBar(mainWindow);
    for(int n=0;n<6;++n) GetMenuItemRect(mainWindow,nativeMenu,n,&nativeRects[n]);
    SetMenu(mainWindow,customMenu);DrawMenuBar(mainWindow);
    for(int n=0;n<6;++n) { RECT themed;GetMenuItemRect(mainWindow,customMenu,n,&themed);RequireCanvas(themed.left==nativeRects[n].left && themed.right==nativeRects[n].right,"top menu spacing matches native Windows menu"); }
    DestroyMenu(nativeMenu);
    RasterImage source;source.width=420;source.height=300;source.pixels.assign(420*300,0xffeeeeee);
    for(int y=60;y<160;++y) for(int x=60;x<200;++x) source.pixels[y*420+x]=0xff3285be;
    imageModel.PushImageForUndo(MakeRasterBitmap(source));imageModel.ClearHistory();
    selectionModel.setMask(CRect(60,60,200,160),NULL);selectionModel.m_bShow=TRUE;imageModel.NotifyImageChanged();
    HWND angle=toolSettingsWindow.GetDlgItem(IDC_SELECTION_ANGLE);RequireCanvas(IsWindowEnabled(angle) && IsWindowVisible(angle),"angle edit visible for active selection");
    auto enter=[&](LPCWSTR text) { SetFocus(angle);SetWindowTextW(angle,text);SendMessageW(angle,WM_KEYDOWN,VK_RETURN,0); };
    enter(L"37,25");RequireCanvas(selectionModel.RotationAngle()==37.25,"angle edit accepts decimal comma");
    enter(L"-22.5°");RequireCanvas(selectionModel.RotationAngle()==337.5,"negative angle normalizes without losing fractional precision");
    enter(L"720");RequireCanvas(selectionModel.RotationAngle()==0,"full turns normalize to zero");
    enter(L"12.34567");RequireCanvas(fabs(selectionModel.RotationAngle()-12.34567)<1e-8,"manual angle keeps exact input precision");
    enter(L"");RequireCanvas(fabs(selectionModel.RotationAngle()-12.34567)<1e-8 && !ParameterIsFixed(PARAM_ANGLE),"clearing a live angle unlocks it without changing geometry");
    for(LPCWSTR invalid:{L"nan",L"inf",L"1e300",L"12abc"})
    { enter(invalid);RequireCanvas(fabs(selectionModel.RotationAngle()-12.34567)<1e-8 && GetFocus()==angle,"invalid angle does not alter selection or leave field"); }
    SetWindowTextW(angle,L"80");SendMessageW(angle,WM_KEYDOWN,VK_ESCAPE,0);RequireCanvas(fabs(selectionModel.RotationAngle()-12.34567)<1e-8,"Escape discards uncommitted angle text");
    enter(L"0");SetParameterFixed(PARAM_ANGLE,false);UpdateParameterBar();BeginSelectionRotation();POINT pointer=SelectionRotationPointer(31);SelectionMouse(WM_MOUSEMOVE,pointer,MK_RBUTTON|MK_SHIFT);
    RequireCanvas(wcstod(WindowLabel(angle),NULL)==30,"mouse rotation updates numeric angle live");SelectionMouse(WM_RBUTTONUP,pointer,MK_SHIFT);
    RequireCanvas(DocumentRaster().pixels==source.pixels && !imageModel.CanUndo(),"manual/mouse preview does not mutate document or create intermediate undo steps");
    auto pixels=SelectionRaster();CRect bounds=selectionModel.m_rc;int transparency=selectionModel.GetTransparency();
    if(!fontsDialog.IsWindow()) fontsDialog.Create(mainWindow);fontsDialog.ShowWindow(SW_SHOWNOACTIVATE);
    if(!miniature.IsWindow()) miniature.DoCreate(mainWindow);
    // Materialize the persistent gallery before switching so its existing labels are tested too.
    ShowFigureGallery();HWND gallery=FindWindowW(L"PaintFigurePopup",NULL);ShowWindow(gallery,SW_HIDE);
    HWND toolbar=FindWindowExW(toolBoxContainer,NULL,TOOLBARCLASSNAMEW,NULL);
    HMENU languageMenu=GetSubMenu(GetSubMenu(mainWindow.GetMenu(),4),5);int languageCount=GetMenuItemCount(languageMenu);
    RequireCanvas(languageCount==30,"all 30 language variants are switchable live");
    for(int i=0;i<languageCount;++i)
    {
        mainWindow.SendMessage(WM_COMMAND,IDM_LANGUAGE_FIRST+i,0);WORD language=WORD(registrySettings.UILanguage);
        WCHAR caption[256];GetMenuStringW(mainWindow.GetMenu(),4,caption,256,MF_BYPOSITION);
        RequireCanvas(ExactString(language,IDS_SETTINGS_MENU)==caption,"settings menu changes immediately");
        for(int tool=0;tool<NUM_TOOLS;++tool) { if(ID_FREESEL+tool>=ID_LINE)continue;WCHAR text[512];SendMessageW(toolbar,TB_GETBUTTONTEXTW,ID_FREESEL+tool,LPARAM(text));RequireCanvas(ExactString(language,IDS_TOOLTIP1+tool)==text,"existing toolbar labels switch immediately"); }
        HWND interpolation=GetDlgItem(toolSettingsWindow,IDC_RESAMPLE_PANEL);
        RequireCanvas(WindowLabel(GetDlgItem(interpolation,IDC_RESAMPLE_FIRST))==ExactString(language,IDS_RESAMPLE_NEAREST),"existing interpolation labels change language");
        RequireCanvas(WindowLabel(GetDlgItem(gallery,IDC_FIGURE_FIRST))==ExactString(language,IDS_FIGURE_FIRST),"existing gallery labels change language");
        RequireCanvas(WindowLabel(fontsDialog)==ExactString(language,IDS_FONTS_TITLE) && WindowLabel(miniature)==ExactString(language,IDS_MINIATURETITLE),"existing floating window titles change language");
        RequireCanvas(selectionModel.m_bShow && selectionModel.m_rc==bounds && selectionModel.RotationAngle()==30 && selectionModel.GetTransparency()==transparency && SelectionRaster().pixels==pixels.pixels,"live language change preserves floating selection and pixels");
        RequireCanvas(DocumentRaster().pixels==source.pixels && !imageModel.CanUndo(),"language change preserves document and undo history");
        RECT before[6];for(int n=0;n<6;++n) GetMenuItemRect(mainWindow,mainWindow.GetMenu(),n,&before[n]);
        for(UINT theme:{IDM_THEME_DARK,IDM_THEME_LIGHT})
        {
            mainWindow.SendMessage(WM_COMMAND,theme,0);
            for(int n=0;n<6;++n) { RECT after;GetMenuItemRect(mainWindow,mainWindow.GetMenu(),n,&after);RequireCanvas(EqualRect(&after,&before[n]),"top menu positions and widths stay fixed across theme changes in every language"); }
        }
    }
    mainWindow.SendMessage(WM_COMMAND,LanguageCommand(L"Russian"),0);fontsDialog.ShowWindow(SW_HIDE);miniature.ShowWindow(SW_HIDE);
    for(int percent:{100,150,200,250})
    {
        registrySettings.PanelMagnification=percent;registrySettings.DarkTheme=1;ApplyTheme();ApplyPanelScale();
        int side=MulDiv(16,percent,100);HIMAGELIST icons=CreatePanelToolIcons(percent);TestBitmap strip(side*5,side,RGB(83,83,83));
        const int tools[]={0,1,9,10,11};for(int i=0;i<5;++i) ImageList_Draw(icons,tools[i],strip.dc,i*side,0,ILD_TRANSPARENT);ImageList_Destroy(icons);
        CStringW path;path.Format(L"live-ui-tests/dark-symbols-%d.png",percent);CImage picture;picture.Attach(strip.bitmap);picture.Save(path);picture.Detach();
        for(int i=0;i<5;++i) { int solid=0;for(int y=0;y<side;++y) for(int x=i*side;x<(i+1)*side;++x) if(GetRValue(GetPixel(strip.dc,x,y))>=235) ++solid;if(solid<=8) printf("Weak icon: percent=%d tool=%d solid=%d\n",percent,tools[i],solid);RequireCanvas(solid>8,"all dark monochrome symbols have sharp opaque white strokes"); }
        bool alreadyFocused=GetFocus()==angle;SetFocus(angle);RECT field;GetWindowRect(angle,&field);MapWindowPoints(NULL,ParameterBarWindow(),reinterpret_cast<POINT*>(&field),2);RECT panel;GetClientRect(ParameterBarWindow(),&panel);
        printf("Angle layout: scale=%d focusedBefore=%d focusedNow=%d parent=%d x=%ld..%ld y=%ld..%ld panel=%ldx%ld scroll=%d\n",percent,alreadyFocused,GetFocus()==angle,GetParent(angle)==ParameterBarWindow(),field.left,field.right,field.top,field.bottom,panel.right,panel.bottom,GetScrollPos(ParameterBarWindow(),SB_HORZ));
        RequireCanvas(GetParent(angle)==ParameterBarWindow() && field.left>=0 && field.right<=panel.right && field.top>=0 && field.bottom<=panel.bottom,"angle edit fits bottom panel at every scale and scrolls into view on focus");canvasWindow.SetFocus();
        path.Format(L"live-ui-tests/window-dark-%d.bmp",percent);CaptureCanvasDialog(mainWindow,path);
    }
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();
    selectionModel.Landing();imageModel.Undo();RequireCanvas(DocumentRaster().pixels==source.pixels,"manual angle and mouse rotation undo as one operation");
    toolsModel.SetActiveTool(TOOL_TEXT);if(!textEditWindow.IsWindow()) textEditWindow.Create(canvasWindow);
    textEditWindow.ValidateEditRect(CRect(40,40,340,200));textEditWindow.SetWindowTextW(L"Still editable — текст");textEditWindow.ShowWindow(SW_SHOWNOACTIVATE);
    HWND edit=textEditWindow;RECT textBounds;textEditWindow.GetEditRect(&textBounds);
    mainWindow.SendMessage(WM_COMMAND,LanguageCommand(L"English (USA)"),0);
    RequireCanvas(HWND(textEditWindow)==edit && textEditWindow.IsWindowVisible() && WindowLabel(edit)==L"Still editable — текст","language change preserves active text editor and contents");
    RECT afterBounds;textEditWindow.GetEditRect(&afterBounds);RequireCanvas(EqualRect(&textBounds,&afterBounds),"language change preserves text placement");
    mainWindow.SendMessage(WM_COMMAND,LanguageCommand(L"Russian"),0);toolsModel.OnEndDraw(TRUE);toolsModel.SetActiveTool(TOOL_RECTSEL);
    StringCchCopyW(g_szFileName,_countof(g_szFileName),L"texture-edited.dds");g_isAFile=FALSE;
    mainWindow.SendMessage(WM_COMMAND,LanguageCommand(L"English (USA)"),0);
    RequireCanvas(!lstrcmpW(g_szFileName,L"texture-edited.dds"),"switching language preserves a proposed Save As filename");
    mainWindow.SendMessage(WM_COMMAND,LanguageCommand(L"Russian"),0);
    ::LoadStringW(g_hinstExe,IDS_DEFAULTFILENAME,g_szFileName,_countof(g_szFileName));mainWindow.ReloadLanguage();
    for(bool dark:{false,true}) { registrySettings.DarkTheme=dark;ApplyTheme();SetTimer(NULL,0,60,DriveAboutWindow);ShowRPaintAbout(mainWindow); }
    registrySettings.DarkTheme=0;ApplyTheme();
    puts("PASS numeric/live rotation, all 30 live languages preserving document/selection/text, stable menu geometry, sharp dark icons and Plus About banner");
}
