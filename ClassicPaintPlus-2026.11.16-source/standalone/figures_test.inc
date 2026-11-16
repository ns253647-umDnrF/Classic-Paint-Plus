static bool FigureCheck(bool ok,int line) { if(!ok) printf("Gallery check failed at %d\n",line);return ok; }
static void PumpFigureMessages(DWORD milliseconds)
{
    DWORD start=GetTickCount();
    do
    {
        MSG msg;
        while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)) { TranslateMessage(&msg);DispatchMessage(&msg); }
        Sleep(1);
    } while(GetTickCount()-start<milliseconds);
}

static void HoldFigureButton(HWND row)
{
    HWND button=GetDlgItem(row,IDC_FIGURE_GALLERY);
    SendMessage(button,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(12,12));
    DWORD start=GetTickCount();
    do { PumpFigureMessages(20); } while(!IsWindowVisible(FindWindowW(L"PaintFigurePopup",NULL)) && GetTickCount()-start<1500);
    SendMessage(button,WM_LBUTTONUP,0,MAKELPARAM(12,12));
}

static void TestFigures()
{
    DWORD oldKind=registrySettings.FigureKind,oldCenter=registrySettings.ShapeFromCenter;
    int oldStyle=toolsModel.GetShapeStyle(),oldWidth=toolsModel.GetLineWidth();
    COLORREF oldFg=paletteModel.GetFgColor(),oldBg=paletteModel.GetBgColor();
    toolsModel.OnEndDraw(FALSE);
    toolsModel.SetZoom(1000);
    imageModel.PushImageForUndo(CreateColorDIB(180,180,RGB(255,255,255)));
    imageModel.ClearHistory();
    paletteModel.SetFgColor(RGB(0,0,0));paletteModel.SetBgColor(RGB(255,255,255));
    toolsModel.SetLineWidth(3);
    for(int kind=0;kind<FIG_COUNT;++kind) for(bool center:{false,true})
    {
        registrySettings.FigureKind=kind;registrySettings.ShapeFromCenter=center;
        toolsModel.SetActiveTool(TOOL_FIGURE);toolsModel.SetFigureAngle(0);
        toolsModel.SetShapeStyle(0);
        toolsModel.OnButtonDown(TRUE,80,80,FALSE);
        toolsModel.OnMouseMove(TRUE,110,100);
        TestBitmap preview(180,180,RGB(255,255,255));
        toolsModel.OnDrawOverlayOnImage(preview.dc);
        toolsModel.OnButtonUp(TRUE,110,100);toolsModel.OnEndDraw(FALSE);
        int changed=0;
        CRect expected=FigureBounds({80,80},{110,100},center,false);expected.InflateRect(3,3);
        for(int y=0;y<180;++y) for(int x=0;x<180;++x)
        {
            COLORREF c=GetPixel(imageModel.GetDC(),x,y);
            RequireCanvas(c==GetPixel(preview.dc,x,y),"figure preview matches committed pixels");
            if(c!=RGB(255,255,255))
            {
                ++changed;
                if(!expected.PtInRect(CPoint(x,y))) printf("Figure bounds kind=%d center=%d point=%d,%d color=%06lx shift=%x\n",kind,center,x,y,c,GetKeyState(VK_SHIFT));
                RequireCanvas(expected.PtInRect(CPoint(x,y)),"figure remains inside its corner/center bounds");
            }
        }
        RequireCanvas(changed>20,"each gallery figure produces a visible outline");
        imageModel.Undo();
        for(int y=0;y<180;++y) for(int x=0;x<180;++x)
            RequireCanvas(GetPixel(imageModel.GetDC(),x,y)==RGB(255,255,255),"Undo restores whole centered figure");
        imageModel.Redo();
        RequireCanvas(GetPixel(imageModel.GetDC(),80,80)==GetPixel(preview.dc,80,80),"figure Redo");
        imageModel.Undo();imageModel.ClearHistory();
    }
    for(TOOLTYPE tool:{TOOL_RECT,TOOL_ELLIPSE,TOOL_RRECT})
    {
        registrySettings.ShapeFromCenter=TRUE;
        toolsModel.SetActiveTool(tool);toolsModel.SetShapeStyle(2);
        toolsModel.OnButtonDown(TRUE,80,80,FALSE);
        toolsModel.OnMouseMove(TRUE,100,100);toolsModel.OnButtonUp(TRUE,100,100);toolsModel.OnEndDraw(FALSE);
        RequireCanvas(GetPixel(imageModel.GetDC(),70,80)==RGB(0,0,0),"legacy shape draws on both sides of center");
        imageModel.Undo();imageModel.ClearHistory();
        toolsModel.OnButtonDown(TRUE,80,80,FALSE);
        toolsModel.OnMouseMove(TRUE,100,100);toolsModel.OnEndDraw(TRUE);
        RequireCanvas(!imageModel.CanUndo() && GetPixel(imageModel.GetDC(),70,80)==RGB(255,255,255),"cancel figure without history");
    }
    RequireCanvas(FigureBounds({80,80},{60,90},true,true)==CRect(60,60,100,100),"center + Shift + reverse drag bounds");
    RequireCanvas(FigureBounds({80,80},{60,90},false,false)==CRect(60,80,80,90),"corner reverse drag bounds");
    HWND origin=GetDlgItem(toolSettingsWindow,IDC_FIGURE_ORIGIN);
    RequireCanvas(origin && (GetWindowLongPtr(origin,GWL_STYLE)&WS_VISIBLE),"origin controls for bounded shape");
    toolsModel.SetActiveTool(TOOL_SHAPE);
    RequireCanvas(!(GetWindowLongPtr(origin,GWL_STYLE)&WS_VISIBLE),"polygon has no origin controls");
    HWND toolbar=FindWindowExW(toolBoxContainer,NULL,TOOLBARCLASSNAMEW,NULL);
    RequireCanvas(SendMessage(toolbar,TB_BUTTONCOUNT,0,0)==10,"gallery replaces the standalone rounded-rectangle button");
    HWND row=FindWindowExW(toolBoxContainer,NULL,L"PaintFigureControls",NULL);
    RequireCanvas(row && GetDlgItem(row,IDC_FIGURE_GALLERY) && !GetDlgItem(row,4901),"one gallery button, no separate indicator");

    // Exercise actual gallery/origin buttons and restore every preference written by them.
    TestIniKey key;key.Create(HKEY_CURRENT_USER,L"Software\\ReactOS Paint\\Settings");
    const WCHAR* names[]={L"UILanguage",L"ReturnAfterColorPick",L"SelectionInterpolation",L"FigureKind",L"ShapeFromCenter",L"FigureRounding"};
    DWORD saved[6]={};LONG exists[6];
    for(int i=0;i<6;++i) exists[i]=key.QueryDWORDValue(names[i],saved[i]);
    bool ok=true;
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    SetForegroundWindow(mainWindow);
    SetActiveWindow(mainWindow);
    PumpFigureMessages(100); // Drain activation/capture messages before the synthetic press.
    SendDlgItemMessage(row,IDC_FIGURE_GALLERY,BM_CLICK,0,0);
    ok &= FigureCheck(toolsModel.GetActiveTool()==TOOL_FIGURE && !IsWindowVisible(FindWindowW(L"PaintFigurePopup",NULL)),__LINE__);
    HWND galleryButton=GetDlgItem(row,IDC_FIGURE_GALLERY);
    toolsModel.SetActiveTool(TOOL_PEN);
    SendMessage(galleryButton,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(12,12));
    PumpFigureMessages(50);
    ok &= FigureCheck(!IsWindowVisible(FindWindowW(L"PaintFigurePopup",NULL)),__LINE__);
    SendMessage(galleryButton,WM_LBUTTONUP,0,MAKELPARAM(12,12));
    ok &= FigureCheck(toolsModel.GetActiveTool()==TOOL_FIGURE,__LINE__);
    HoldFigureButton(row);
    HWND popup=FindWindowW(L"PaintFigurePopup",NULL);
    ok &= FigureCheck(popup && IsWindowVisible(popup),__LINE__);
    if(popup)
    {
        CaptureCanvasDialog(popup,L"figures-gallery-preview.bmp");
        for(int i=0;i<FIG_COUNT;++i) if(i!=FIG_RRECT) ok &= FigureCheck(!!GetDlgItem(popup,IDC_FIGURE_FIRST+i),__LINE__);
        SendDlgItemMessage(popup,IDC_FIGURE_FIRST+FIG_TRIANGLE,BM_CLICK,0,0);
        ok &= FigureCheck(registrySettings.FigureKind==FIG_TRIANGLE && toolsModel.GetActiveTool()==TOOL_FIGURE && !IsWindowVisible(popup),__LINE__);
        SendDlgItemMessage(origin,IDC_FIGURE_CENTER,BM_CLICK,0,0);
        ok &= FigureCheck(!!registrySettings.ShapeFromCenter,__LINE__);
        DWORD stored=0;ok &= FigureCheck(key.QueryDWORDValue(L"ShapeFromCenter",stored)==ERROR_SUCCESS && stored==1,__LINE__);
        SendDlgItemMessage(origin,IDC_FIGURE_CORNER,BM_CLICK,0,0);
        ok &= FigureCheck(!registrySettings.ShapeFromCenter,__LINE__);
        HoldFigureButton(row);
        SendMessage(GetFocus(),WM_KEYDOWN,VK_ESCAPE,0);
        ok &= FigureCheck(!IsWindowVisible(popup) && registrySettings.FigureKind==FIG_TRIANGLE,__LINE__);
        HoldFigureButton(row);
        SendMessage(GetFocus(),WM_KEYDOWN,VK_LEFT,0);
        SendMessage(GetFocus(),WM_KEYDOWN,VK_RETURN,0);
        ok &= FigureCheck(registrySettings.FigureKind==FIG_RECT && !IsWindowVisible(popup),__LINE__);
        SendMessage(galleryButton,WM_KEYDOWN,VK_DOWN,0);
        SetActiveWindow(mainWindow);
        ok &= FigureCheck(!IsWindowVisible(popup),__LINE__);
        // Moving off the held button or losing capture must not open the popup later.
        SendMessage(galleryButton,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(12,12));
        SendMessage(galleryButton,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(40,12));
        PumpFigureMessages(450);
        SendMessage(galleryButton,WM_LBUTTONUP,0,MAKELPARAM(40,12));
        ok &= FigureCheck(!IsWindowVisible(popup),__LINE__);
        SendMessage(galleryButton,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(12,12));
        SetCapture(mainWindow);PumpFigureMessages(450);ReleaseCapture();
        ok &= FigureCheck(!IsWindowVisible(popup),__LINE__);
        mainWindow.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);
        CaptureCanvasDialog(mainWindow,L"figures-controls-preview.bmp");
    }
    for(int i=0;i<6;++i) if(exists[i]==ERROR_SUCCESS) key.SetDWORDValue(names[i],saved[i]);else key.DeleteValue(names[i]);
    RequireCanvas(ok,"gallery choice, dismissal, origin buttons and persistence");
    mainWindow.ShowWindow(SW_HIDE);
    registrySettings.FigureKind=oldKind;registrySettings.ShapeFromCenter=oldCenter;
    toolsModel.SetShapeStyle(oldStyle);toolsModel.SetLineWidth(oldWidth);
    paletteModel.SetFgColor(oldFg);paletteModel.SetBgColor(oldBg);
    toolsModel.SetActiveTool(TOOL_RECTSEL);
    puts("PASS figures: all grouped shapes in both origins, legacy shapes, preview/commit, Undo/Redo, cancel and gallery UI");
}
