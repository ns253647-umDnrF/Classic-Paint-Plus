static void TestObjectSizeMemory()
{
    DWORD previousRemember=registrySettings.RememberObjectSizes;registrySettings.RememberObjectSizes=1;
    CreateDirectoryW(L"object-actions-tests",NULL);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    mainWindow.SetWindowPos(NULL,0,0,1080,900,SWP_NOZORDER|SWP_NOACTIVATE);
    toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();SetTestShift(false);
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;registrySettings.ShapeFromCenter=0;ApplyTheme();ApplyPanelScale();canvasWindow.zoomTo(1000);
    imageModel.PushImageForUndo(CreateColorDIB(600,450,RGB(255,255,255)));imageModel.ClearHistory();
    HWND width=toolSettingsWindow.GetDlgItem(IDC_OBJECT_WIDTH),height=toolSettingsWindow.GetDlgItem(IDC_OBJECT_HEIGHT);
    HWND save=toolSettingsWindow.GetDlgItem(IDC_SAVE_OBJECT_SIZE),reset=toolSettingsWindow.GetDlgItem(IDC_RESET_OBJECT_SIZE);
    auto create=[]() { FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,150,130);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,300,230);FigureMouse(WM_LBUTTONUP,0,300,230); };
    for(int kind=0;kind<=FIG_COUNT;++kind)
    {
        bool text=kind==FIG_COUNT;registrySettings.FigureKind=text?FIG_RECT:kind;
        toolsModel.SetActiveTool(text?TOOL_TEXT:TOOL_FIGURE);toolsModel.SetFigureAngle(27);textEditWindow.SetRotationAngle(27);
        SendMessage(reset,BM_CLICK,0,0);
        RequireCanvas(IsWindowEnabled(save) && IsWindowEnabled(reset),"S and R enabled before drawing");
        RequireCanvas(WindowLabel(width).IsEmpty() && WindowLabel(height).IsEmpty(),"R clears both idle fields");
        CStringW value;value.Format(L"%d px",170+kind);EnterObjectValue(width,value);value.Format(L"%d px",70+kind);EnterObjectValue(height,value);create();
        RequireCanvas(IsWindowEnabled(save) && !IsWindowEnabled(reset),"S enabled and R disabled for active object");
        // A value typed without Enter must be applied before the S snapshot.
        SetFocus(width);value.Format(L"%d",180+kind);SetWindowTextW(width,value);SendMessage(save,BM_CLICK,0,0);
        SIZE snapshot=registrySettings.SavedObjectSizes[kind];RequireCanvas(snapshot.cx==180+kind && snapshot.cy==70+kind,"S snapshots current dimensions including pending edit");
        EnterObjectValue(width,L"275 px");EnterObjectValue(height,L"125 px");
        RequireCanvas(registrySettings.SavedObjectSizes[kind].cx==snapshot.cx && registrySettings.SavedObjectSizes[kind].cy==snapshot.cy,"later resizing cannot overwrite S snapshot");
        if(text) textEditWindow.SetWindowTextW(L"Remembered text size");
        toolsModel.OnEndDraw(FALSE);toolSettingsWindow.UpdateObjectDimensions(true);
        RequireCanvas(IsWindowEnabled(save) && IsWindowEnabled(reset),"commit restores idle S/R state");
        create();CRect bounds;if(text) textEditWindow.GetEditRect(&bounds);else toolsModel.EditableFigureBounds(bounds);
        RequireCanvas(bounds.Width()==snapshot.cx && bounds.Height()==snapshot.cy,"next same object uses exact saved dimensions");
        for(int percent:{100,150,200,250}) if(kind==FIG_RECT || text)
        {
            registrySettings.PanelMagnification=percent;ApplyPanelScale();
            for(int dark:{0,1})
            {
                registrySettings.DarkTheme=dark;ApplyTheme();RECT s,r,h,p;GetWindowRect(save,&s);GetWindowRect(reset,&r);GetWindowRect(height,&h);GetWindowRect(ObjectActionsWindow(),&p);
                RequireCanvas(s.right-s.left==PanelPx(25) && s.bottom-s.top==PanelPx(25) && s.right==r.left && s.top==r.top && s.bottom<=p.bottom && s.left>=p.left,"square S/R buttons fit beside the bottom fields at every scale and theme");
                CStringW file;file.Format(L"object-actions-tests/%s-%d-%d.bmp",text?L"text":L"figure",percent,dark);CaptureCanvasDialog(ParameterBarWindow(),file);
            }
        }
        registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();toolsModel.OnEndDraw(TRUE);
    }
    // Reload a fresh settings object from the private INI (never production config.ini).
    RegistrySettings loaded;loaded.Load(SW_HIDE);
    for(int i=0;i<=FIG_COUNT;++i) RequireCanvas(loaded.SavedObjectSizes[i].cx==180+i && loaded.SavedObjectSizes[i].cy==70+i,"all nineteen independent snapshots survive INI reload");
    for(TOOLTYPE type:{TOOL_RECT,TOOL_ELLIPSE,TOOL_RRECT})
    {
        toolsModel.SetActiveTool(type);int kind=toolsModel.ObjectSizeKind(false);
        RequireCanvas(toolsModel.ObjectSizePreset(false).cx==180+kind,"standalone buttons share the corresponding gallery memory");
    }
    toolsModel.SetActiveTool(TOOL_FIGURE);registrySettings.FigureKind=FIG_DIAMOND;toolSettingsWindow.UpdateObjectDimensions(true);
    SetFocus(width);SetWindowTextW(width,L"999");SendMessage(reset,BM_CLICK,0,0);
    RequireCanvas(WindowLabel(width).IsEmpty() && WindowLabel(height).IsEmpty() && !registrySettings.SavedObjectSizes[FIG_DIAMOND].cx,"R discards pending edits and saved snapshot");
    loaded.Load(SW_HIDE);RequireCanvas(!loaded.SavedObjectSizes[FIG_DIAMOND].cx && loaded.SavedObjectSizes[FIG_RECT].cx==180+FIG_RECT && loaded.SavedObjectSizes[FIG_COUNT].cx==180+FIG_COUNT,"R removes only selected type from INI");
    create();CRect freeBounds;toolsModel.EditableFigureBounds(freeBounds);RequireCanvas(freeBounds.Width()!=180+FIG_DIAMOND,"reset restores free mouse sizing");toolsModel.OnEndDraw(TRUE);
    RequireCanvas(!toolsModel.ObjectSizePreset(false).cx && !toolsModel.ObjectSizePreset(false).cy,"unsaved drawing does not remember dimensions");
    // Clean test state so later integration tests start with free drawing.
    for(int kind=0;kind<FIG_COUNT;++kind) { registrySettings.FigureKind=kind;toolsModel.ResetObjectSize(false); }
    toolsModel.ResetObjectSize(true);textEditWindow.ResetRotation();toolsModel.SetFigureAngle(0);toolsModel.SetActiveTool(TOOL_RECTSEL);
    registrySettings.RememberObjectSizes=previousRemember;registrySettings.StorePreferences();
    puts("PASS S/R: 18 figures and text, pending input, exact snapshot, subsequent resize, next object, aliases, per-kind reset, INI persistence and four panel scales in both themes");
}

static void TestPaletteCopyArrows()
{
    toolsModel.OnEndDraw(TRUE);toolsModel.SetActiveTool(TOOL_PEN);
    auto& state=DocumentAlpha();state={};state.enabled=true;state.hasColor=true;state.color=RGB(14,127,239);state.backdrop=RGB(219,65,38);NotifyDocumentAlpha();
    auto before=DocumentRaster();
    HWND buttons[5];for(int i=0;i<5;++i) buttons[i]=paletteWindow.GetDlgItem(IDC_COPY_COLOR_FIRST+i);
    paletteModel.SetFgColor(RGB(1,2,3),0);paletteModel.SetBgColor(RGB(4,5,6),0);
    SendMessage(buttons[0],BM_CLICK,0,0);SendMessage(buttons[1],BM_CLICK,0,0);
    RequireCanvas(paletteModel.GetFgColor()==state.color && paletteModel.GetBgColor()==state.color && paletteModel.GetFgAlpha()==255 && paletteModel.GetBgAlpha()==255,"upper left and diagonal copy opaque RGB into foreground/background");
    SendMessage(buttons[3],BM_CLICK,0,0);SendMessage(buttons[4],BM_CLICK,0,0);
    RequireCanvas(paletteModel.GetFgColor()==state.backdrop && paletteModel.GetBgColor()==state.backdrop && paletteModel.GetFgAlpha()==255 && paletteModel.GetBgAlpha()==255,"lower arrows copy opaque backdrop RGB");
    SendMessage(buttons[2],BM_CLICK,0,0);RequireCanvas(DocumentBackdrop()==state.color && state.customized,"down arrow copies key RGB to document backdrop");
    RequireCanvas(DocumentRaster().pixels==before.pixels,"all palette shortcuts preserve bitmap pixels and alpha");
    state.hasColor=false;NotifyDocumentAlpha();COLORREF oldForeground=paletteModel.GetFgColor(),oldBackground=paletteModel.GetBgColor(),oldBackdrop=DocumentBackdrop();
    for(int i=0;i<5;++i) RequireCanvas(!!IsWindowEnabled(buttons[i])==(i>=3),"checkerboard disables exactly the three upper arrows");
    for(int i=0;i<3;++i) paletteWindow.SendMessage(WM_COMMAND,MAKEWPARAM(IDC_COPY_COLOR_FIRST+i,BN_CLICKED),LPARAM(buttons[i]));
    RequireCanvas(paletteModel.GetFgColor()==oldForeground && paletteModel.GetBgColor()==oldBackground && DocumentBackdrop()==oldBackdrop,"disabled checkerboard commands cannot copy an undefined color");
    CaptureCanvasDialog(paletteWindow,L"object-actions-tests/palette-checker.bmp");
    state.hasColor=true;NotifyDocumentAlpha();
    for(int percent:{100,150,200,250}) for(int dark:{0,1})
    {
        registrySettings.PanelMagnification=percent;registrySettings.DarkTheme=dark;ApplyTheme();ApplyPanelScale();
        RECT upper,lower;GetWindowRect(buttons[0],&upper);GetWindowRect(buttons[3],&lower);
        RequireCanvas(upper.right-upper.left==CXY_COLORBOX && upper.bottom-upper.top==CXY_COLORBOX && lower.top-upper.top==PanelPx(16),"arrow buttons match swatch dimensions and row spacing");
        CStringW file;file.Format(L"object-actions-tests/palette-%d-%d.bmp",percent,dark);CaptureCanvasDialog(paletteWindow,file);
    }
    mainWindow.SetWindowPos(NULL,0,0,500,700,SWP_NOZORDER|SWP_NOACTIVATE);paletteWindow.SendMessage(WM_HSCROLL,SB_RIGHT,0);
    RECT last,client;GetWindowRect(buttons[2],&last);paletteWindow.GetClientRect(&client);MapWindowPoints(paletteWindow,NULL,reinterpret_cast<POINT*>(&client),2);
    RequireCanvas(last.left>=client.left && last.right<=client.right,"rightmost button reachable with palette horizontal scrolling");
    SendMessage(buttons[2],WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(8,8));SendMessage(buttons[2],WM_LBUTTONUP,0,MAKELPARAM(8,8));
    RequireCanvas(GetCapture()!=paletteWindow.m_hWnd,"arrow click cannot drag the palette toolbar");
    state.enabled=false;NotifyDocumentAlpha();for(HWND button:buttons) RequireCanvas(!IsWindowVisible(button),"copy buttons hidden with alpha mode off");
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();mainWindow.SetWindowPos(NULL,0,0,1080,900,SWP_NOZORDER|SWP_NOACTIVATE);
    puts("PASS palette arrows: five destinations, opaque RGB, checkerboard disables upper three, unchanged image pixels, native clicks, themes/scales and horizontal scrolling");
}
static void TestObjectActions()
{
    TestObjectSizeMemory();TestPaletteCopyArrows();
    if(testLanguages.empty()) EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    for(WORD lang:testLanguages) for(UINT id:{IDS_SAVE_OBJECT_SIZE,IDS_RESET_OBJECT_SIZE,IDS_COPY_FOREGROUND,IDS_COPY_BACKGROUND,IDS_COPY_BACKDROP,IDS_REMEMBER_OBJECT_SIZES})
        RequireCanvas(!ExactString(lang,id).IsEmpty(),"all new tooltips translated");
    puts("PASS new action labels in all 30 languages");
}
