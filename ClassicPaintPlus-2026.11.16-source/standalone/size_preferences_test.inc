static void TestRememberSizePreference()
{
    // Exercise actual menu commands and a fresh settings object against the isolated INI.
    CStringW path=RegistrySettings::ConfigPath();DWORD oldRemember=registrySettings.RememberObjectSizes;
    toolsModel.OnEndDraw(TRUE);toolsModel.SetActiveTool(TOOL_RECT);
    WritePrivateProfileStringW(L"Settings",L"RememberObjectSizes",NULL,path);
    WritePrivateProfileStringW(L"ObjectSizes",L"RectangleWidth",L"173",path);
    WritePrivateProfileStringW(L"ObjectSizes",L"RectangleHeight",L"91",path);
    RegistrySettings loaded;loaded.Load(SW_HIDE);
    RequireCanvas(!loaded.RememberObjectSizes && !loaded.SavedObjectSizes[FIG_RECT].cx,"missing checkbox defaults off and ignores old saved dimensions");
    registrySettings.RememberObjectSizes=0;toolsModel.SaveObjectSize(false,173,91);toolsModel.SaveObjectSize(true,211,83);
    RequireCanvas(toolsModel.ObjectSizePreset(false).cx==173 && toolsModel.ObjectSizePreset(true).cx==211,"S memory works for the current session while persistence is off");
    loaded.Load(SW_HIDE);RequireCanvas(!loaded.RememberObjectSizes && !loaded.SavedObjectSizes[FIG_RECT].cx && !loaded.SavedObjectSizes[FIG_COUNT].cy,"restart with checkbox off resets shape and text dimensions");
    RequireCanvas(GetPrivateProfileIntW(L"ObjectSizes",L"RectangleWidth",999,path)==0 && GetPrivateProfileIntW(L"ObjectSizes",L"TextHeight",999,path)==0,"disabled persistence writes zeros to INI");
    mainWindow.SendMessage(WM_COMMAND,IDM_REMEMBER_OBJECT_SIZES,0);UpdateSettingsChecks(mainWindow.GetMenu());
    RequireCanvas(registrySettings.RememberObjectSizes && (GetMenuState(mainWindow.GetMenu(),IDM_REMEMBER_OBJECT_SIZES,MF_BYCOMMAND)&MF_CHECKED),"settings command enables checkbox");
    loaded.Load(SW_HIDE);RequireCanvas(loaded.RememberObjectSizes && loaded.SavedObjectSizes[FIG_RECT].cx==173 && loaded.SavedObjectSizes[FIG_COUNT].cy==83,"enabled flag and both shape/text snapshots survive restart");
    mainWindow.SendMessage(WM_COMMAND,IDM_REMEMBER_OBJECT_SIZES,0);UpdateSettingsChecks(mainWindow.GetMenu());
    RequireCanvas(!(GetMenuState(mainWindow.GetMenu(),IDM_REMEMBER_OBJECT_SIZES,MF_BYCOMMAND)&MF_CHECKED),"settings command unchecks preference");
    loaded.Load(SW_HIDE);RequireCanvas(!loaded.RememberObjectSizes && !loaded.SavedObjectSizes[FIG_RECT].cx && !loaded.SavedObjectSizes[FIG_COUNT].cx,"disabling clears persisted values immediately");
    RequireCanvas(toolsModel.ObjectSizePreset(false).cx==173 && toolsModel.ObjectSizePreset(true).cx==211,"toggling persistence does not discard this session's S snapshots");
    toolsModel.ResetObjectSize(false);toolsModel.ResetObjectSize(true);registrySettings.RememberObjectSizes=oldRemember;registrySettings.StorePreferences();
    puts("PASS size preference: default unchecked, legacy sizes ignored, current-session S preserved, zeroed INI when off, checked state and shape/text snapshots restored when on");
}

static void TestFigureOutsideClick()
{
    CreateDirectoryW(L"size-preferences-tests",NULL);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    mainWindow.SetWindowPos(NULL,0,0,1080,900,SWP_NOACTIVATE|SWP_NOZORDER);
    toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();SetTestShift(false);registrySettings.ShapeFromCenter=0;
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();canvasWindow.zoomTo(1000);
    for(int kind=0;kind<FIG_COUNT+3;++kind)
    {
        TOOLTYPE type=kind<FIG_COUNT?TOOL_FIGURE:kind==FIG_COUNT?TOOL_RECT:kind==FIG_COUNT+1?TOOL_ELLIPSE:TOOL_RRECT;
        registrySettings.FigureKind=kind<FIG_COUNT?kind:FIG_RECT;toolsModel.SetActiveTool(type);toolsModel.ResetObjectSize(false);
        toolsModel.SetFigureAngle(33);toolsModel.SetLineWidth(3);toolsModel.SetShapeStyle(0);paletteModel.SetFgColor(RGB(21,61,171));
        HBITMAP blank=CreateColorDIB(600,450,RGB(255,255,255));ResetDocumentAlpha(blank);imageModel.PushImageForUndo(blank);imageModel.ClearHistory();
        FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,100,100);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,260,200);FigureMouse(WM_LBUTTONUP,0,260,200);
        RequireCanvas(toolsModel.HasEditableFigure(),"first gesture leaves figure editable");
        TestBitmap preview(600,450,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(preview.dc);auto expected=FigurePixels(preview.dc);
        FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,420,320);
        RequireCanvas(!toolsModel.HasEditableFigure() && !canvasWindow.m_drawing && GetCapture()!=canvasWindow.m_hWnd,"outside press commits without starting a new drawing or capture");
        FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,450,350);FigureMouse(WM_LBUTTONUP,0,450,350);
        RequireCanvas(!toolsModel.HasEditableFigure() && FigurePixels(imageModel.GetDC())==expected,"outside click or drag commits only previous shape");
        imageModel.Undo();RequireCanvas(!imageModel.CanUndo() && GetPixel(imageModel.GetDC(),180,100)==RGB(255,255,255),"outside commit is a single undo step");imageModel.Redo();
        FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,420,320);RequireCanvas(canvasWindow.m_drawing,"next independent press starts the next figure");
        FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,480,380);FigureMouse(WM_LBUTTONUP,0,480,380);RequireCanvas(toolsModel.HasEditableFigure(),"next figure becomes editable normally");
        toolsModel.OnEndDraw(TRUE);
    }
    // Windows sends WM_LBUTTONDBLCLK for the second press of a fast pair.
    toolsModel.SetActiveTool(TOOL_RECT);toolsModel.SetFigureAngle(0);
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,100,100);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,260,200);FigureMouse(WM_LBUTTONUP,0,260,200);
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,420,320);FigureMouse(WM_LBUTTONUP,0,420,320);
    FigureMouse(WM_LBUTTONDBLCLK,MK_LBUTTON,420,320);RequireCanvas(canvasWindow.m_drawing,"second press of double click follows normal figure creation path");
    FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,480,380);FigureMouse(WM_LBUTTONUP,0,480,380);
    RequireCanvas(!canvasWindow.m_drawing && toolsModel.HasEditableFigure(),"double click cannot leave a stuck uncommitted drawing gesture");
    CaptureCanvasDialog(mainWindow,L"size-preferences-tests/second-figure.bmp");toolsModel.OnEndDraw(TRUE);toolsModel.SetLineWidth(1);toolsModel.SetActiveTool(TOOL_RECTSEL);
    puts("PASS outside click: all 18 gallery shapes and three standalone aliases, drag consumption, exact committed pixels, single undo, next press and double-click sequencing");
}
static void TestSizePreferences()
{
    TestRememberSizePreference();TestFigureOutsideClick();TestPaletteCopyArrows();
    puts("PASS size preference follow-up");
}
