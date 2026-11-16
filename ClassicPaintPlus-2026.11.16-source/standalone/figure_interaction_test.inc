static std::vector<BYTE> FigurePixels(HDC dc)
{
    GdiFlush();
    BITMAP bitmap={};GetObject(GetCurrentObject(dc,OBJ_BITMAP),sizeof(bitmap),&bitmap);
    RequireCanvas(bitmap.bmBits!=NULL,"figure test uses a DIB");
    BYTE* begin=static_cast<BYTE*>(bitmap.bmBits);
    return std::vector<BYTE>(begin,begin+bitmap.bmWidthBytes*bitmap.bmHeight);
}

static void FigureMouse(UINT message,WPARAM buttons,int x,int y)
{
    POINT pt={x,y};canvasWindow.ImageToCanvas(pt);
    canvasWindow.SendMessage(message,buttons,MAKELPARAM(pt.x,pt.y));
}

static void TestFigureInteraction()
{
    DWORD oldKind=registrySettings.FigureKind,oldCenter=registrySettings.ShapeFromCenter,oldRounding=registrySettings.FigureRounding;
    int oldStyle=toolsModel.GetShapeStyle(),oldWidth=toolsModel.GetLineWidth();
    COLORREF oldFg=paletteModel.GetFgColor(),oldBg=paletteModel.GetBgColor();
    toolsModel.SetZoom(1000);toolsModel.SetLineWidth(3);
    paletteModel.SetFgColor(RGB(0,0,0));paletteModel.SetBgColor(RGB(255,60,20));
    imageModel.PushImageForUndo(CreateColorDIB(180,180,RGB(255,255,255)));imageModel.ClearHistory();
    canvasWindow.updateScrollRange();canvasWindow.updateScrollPos();
    auto blank=FigurePixels(imageModel.GetDC());
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    for(int variant=0;variant<FIG_COUNT+3;++variant) for(bool center:{false,true})
    {
        int kind=variant<FIG_COUNT?variant:variant-FIG_COUNT;
        TOOLTYPE tool=variant<FIG_COUNT?TOOL_FIGURE:(kind==FIG_ELLIPSE?TOOL_ELLIPSE:kind==FIG_RECT?TOOL_RECT:TOOL_RRECT);
        registrySettings.FigureKind=kind;registrySettings.ShapeFromCenter=center;registrySettings.FigureRounding=35;
        toolsModel.SetActiveTool(tool);toolsModel.SetFigureAngle(0);toolsModel.SetShapeStyle(variant%3);
        FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,center?90:40,center?90:70);
        FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,140,110);
        TestBitmap before(180,180,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(before.dc);
        FigureMouse(WM_RBUTTONDOWN,MK_LBUTTON|MK_RBUTTON,140,110);
        TestBitmap entered(180,180,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(entered.dc);
        RequireCanvas(canvasWindow.m_drawing && GetCapture()==canvasWindow.m_hWnd && !imageModel.CanUndo(),"right press keeps current left drag and capture");
        RequireCanvas(FigurePixels(entered.dc)==FigurePixels(before.dc),"enter rotation without changing figure");
        FigureMouse(WM_MOUSEMOVE,MK_LBUTTON|MK_RBUTTON,69,140); // Exactly +90 degrees around (89.5,89.5).
        TestBitmap rotated(180,180,RGB(255,255,255)),expected(180,180,RGB(255,255,255));
        toolsModel.OnDrawOverlayOnImage(rotated.dc);
        DrawFigure(expected.dc,kind,CRect(40,70,140,110),toolsModel.GetFgBrush(),toolsModel.GetBgBrush(),3,variant%3,M_PI/2);
        RequireCanvas(FigurePixels(rotated.dc)==FigurePixels(expected.dc),"rotate around center with unchanged dimensions and colors");
        FigureMouse(WM_RBUTTONUP,MK_LBUTTON,69,140);
        FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,69,140);
        TestBitmap resumed(180,180,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(resumed.dc);
        RequireCanvas(FigurePixels(resumed.dc)==FigurePixels(rotated.dc) && canvasWindow.m_drawing,"right release resumes size mode without jump");
        FigureMouse(WM_LBUTTONUP,0,69,140);
        RequireCanvas(!canvasWindow.m_drawing && GetCapture()!=canvasWindow.m_hWnd,"left release finishes rotation gesture");
        toolsModel.OnEndDraw(FALSE);
        RequireCanvas(FigurePixels(imageModel.GetDC())==FigurePixels(rotated.dc),"rotated preview equals final pixels");
        imageModel.Undo();RequireCanvas(FigurePixels(imageModel.GetDC())==blank && !imageModel.CanUndo(),"one Undo restores the complete rotated figure");
        imageModel.Redo();RequireCanvas(FigurePixels(imageModel.GetDC())==FigurePixels(rotated.dc),"rotation Redo");
        imageModel.Undo();imageModel.ClearHistory();
    }
    // Release left first; right release must not add another shape or undo entry.
    registrySettings.ShapeFromCenter=FALSE;registrySettings.FigureKind=FIG_TRIANGLE;
    toolsModel.SetActiveTool(TOOL_FIGURE);toolsModel.SetFigureAngle(0);toolsModel.SetShapeStyle(0);
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,40,70);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,140,110);
    FigureMouse(WM_RBUTTONDOWN,MK_LBUTTON|MK_RBUTTON,140,110);
    FigureMouse(WM_MOUSEMOVE,MK_LBUTTON|MK_RBUTTON,90,150);
    TestBitmap diagonal(180,180,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(diagonal.dc);
    FigureMouse(WM_LBUTTONUP,MK_RBUTTON,90,150);FigureMouse(WM_RBUTTONUP,0,90,150);
    toolsModel.OnEndDraw(FALSE);
    RequireCanvas(FigurePixels(imageModel.GetDC())==FigurePixels(diagonal.dc),"left-first release commits the arbitrary rotation angle");
    imageModel.Undo();RequireCanvas(FigurePixels(imageModel.GetDC())==blank && !imageModel.CanUndo(),"left-first release makes one history entry");imageModel.ClearHistory();

    toolsModel.SetFigureAngle(0);
    // Resume resizing in rotated coordinates, then enter rotation again.
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,40,70);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,140,110);
    FigureMouse(WM_RBUTTONDOWN,MK_LBUTTON|MK_RBUTTON,140,110);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON|MK_RBUTTON,69,140);
    FigureMouse(WM_RBUTTONUP,MK_LBUTTON,69,140);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,59,160);
    TestBitmap enlarged(180,180,RGB(255,255,255)),expectedSize(180,180,RGB(255,255,255));
    toolsModel.OnDrawOverlayOnImage(enlarged.dc);
    DrawFigure(expectedSize.dc,FIG_TRIANGLE,CRect(40,70,160,120),toolsModel.GetFgBrush(),toolsModel.GetBgBrush(),3,0,M_PI/2);
    RequireCanvas(FigurePixels(enlarged.dc)==FigurePixels(expectedSize.dc),"continued resize preserves rotation and follows mouse delta");
    FigureMouse(WM_RBUTTONDBLCLK,MK_LBUTTON|MK_RBUTTON,59,160);
    TestBitmap reentered(180,180,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(reentered.dc);
    RequireCanvas(FigurePixels(reentered.dc)==FigurePixels(enlarged.dc),"reenter rotation, including double-click, without resetting geometry");
    canvasWindow.SendMessage(WM_KEYDOWN,VK_ESCAPE,0);
    FigureMouse(WM_LBUTTONUP,0,59,160);
    RequireCanvas(!canvasWindow.m_drawing && !imageModel.CanUndo() && FigurePixels(imageModel.GetDC())==blank,"Escape cancels rotated figure");
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,40,70);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,140,110);
    FigureMouse(WM_RBUTTONDOWN,MK_LBUTTON|MK_RBUTTON,140,110);SetCapture(mainWindow);ReleaseCapture();
    FigureMouse(WM_LBUTTONUP,0,69,140);
    RequireCanvas(!canvasWindow.m_drawing && !imageModel.CanUndo() && FigurePixels(imageModel.GetDC())==blank,"capture loss cancels rotated figure");

    // Rotation also works for an in-progress multi-point polygon.
    toolsModel.SetActiveTool(TOOL_SHAPE);toolsModel.SetShapeStyle(0);
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,40,60);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,120,60);FigureMouse(WM_LBUTTONUP,0,120,60);
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,120,110);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,120,110);
    TestBitmap polyBefore(180,180,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(polyBefore.dc);
    FigureMouse(WM_RBUTTONDOWN,MK_LBUTTON|MK_RBUTTON,120,110);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON|MK_RBUTTON,55,125);
    TestBitmap polyRotated(180,180,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(polyRotated.dc);
    RequireCanvas(canvasWindow.m_drawing && FigurePixels(polyBefore.dc)!=FigurePixels(polyRotated.dc),"polygon rotates while left button stays down");
    FigureMouse(WM_RBUTTONUP,MK_LBUTTON,55,125);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,55,125);
    TestBitmap polyResumed(180,180,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(polyResumed.dc);
    RequireCanvas(FigurePixels(polyRotated.dc)==FigurePixels(polyResumed.dc),"polygon right release has no jump");
    FigureMouse(WM_LBUTTONUP,0,55,125);toolsModel.OnEndDraw(FALSE);
    RequireCanvas(imageModel.CanUndo(),"rotated polygon finishes normally");
    imageModel.Undo();RequireCanvas(FigurePixels(imageModel.GetDC())==blank,"Undo rotated polygon");imageModel.ClearHistory();

    // Native rounding slider, real keyboard events, saved preference and visible effect.
    TestIniKey key;key.Create(HKEY_CURRENT_USER,L"Software\\ReactOS Paint\\Settings");
    const WCHAR* names[]={L"UILanguage",L"ReturnAfterColorPick",L"SelectionInterpolation",L"FigureKind",L"ShapeFromCenter",L"FigureRounding"};
    DWORD saved[6]={};LONG exists[6];for(int i=0;i<6;++i) exists[i]=key.QueryDWORDValue(names[i],saved[i]);
    HWND slider=ParameterControl(IDC_FIGURE_ROUNDING);
    toolsModel.SetActiveTool(TOOL_RECT);RequireCanvas(IsWindowVisible(slider),"rounding slider visible for rectangle");
    toolsModel.SetActiveTool(TOOL_RRECT);toolsModel.SetFigureAngle(0);toolsModel.SetShapeStyle(2);
    RequireCanvas(IsWindowVisible(slider),"rounding slider visible for legacy rounded rectangle");
    SendMessage(slider,WM_KEYDOWN,VK_HOME,0); // Left: square corners.
    bool settingsOK=FigureRounding()==0;
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,50,50);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,130,130);FigureMouse(WM_LBUTTONUP,0,130,130);toolsModel.OnEndDraw(FALSE);
    settingsOK &= GetPixel(imageModel.GetDC(),53,53)==RGB(0,0,0);
    imageModel.Undo();imageModel.ClearHistory();
    SendMessage(slider,WM_KEYDOWN,VK_END,0); // Right: maximum rounding.
    settingsOK &= FigureRounding()==MAX_FIGURE_RADIUS;
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,50,50);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,130,130);FigureMouse(WM_LBUTTONUP,0,130,130);toolsModel.OnEndDraw(FALSE);
    settingsOK &= GetPixel(imageModel.GetDC(),53,53)==RGB(255,255,255) && GetPixel(imageModel.GetDC(),90,90)==RGB(0,0,0);
    DWORD stored=0;settingsOK &= (registrySettings.StorePreferences(),key.QueryDWORDValue(L"FigureRounding",stored)!=ERROR_SUCCESS);
    RegistrySettings loaded;loaded.Load(SW_HIDE);settingsOK &= loaded.FigureRounding==0 && FigureRounding(FIG_RRECT)==MAX_FIGURE_RADIUS;
    registrySettings.FigureKind=FIG_RRECT;toolsModel.SetActiveTool(TOOL_FIGURE);toolsModel.SetFigureAngle(0);settingsOK &= !!IsWindowVisible(slider);
    SendMessage(slider,WM_KEYDOWN,VK_LEFT,0);settingsOK &= FigureRounding()==MAX_FIGURE_RADIUS-1;
    mainWindow.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);
    CaptureCanvasDialog(mainWindow,L"figures-rounding-preview.bmp");
    for(int i=0;i<6;++i) if(exists[i]==ERROR_SUCCESS) key.SetDWORDValue(names[i],saved[i]);else key.DeleteValue(names[i]);
    RequireCanvas(settingsOK,"rounding slider endpoints, keyboard adjustment, rendered corners and persistence");
    imageModel.Undo();imageModel.ClearHistory();
    registrySettings.FigureKind=oldKind;registrySettings.ShapeFromCenter=oldCenter;registrySettings.FigureRounding=oldRounding;
    toolsModel.SetShapeStyle(oldStyle);toolsModel.SetLineWidth(oldWidth);toolsModel.SetActiveTool(TOOL_RECTSEL);
    paletteModel.SetFgColor(oldFg);paletteModel.SetBgColor(oldBg);
    mainWindow.ShowWindow(SW_HIDE);
    puts("PASS figure interaction: hold/click, rotation of all shapes, both release orders, resize continuation, cancel, polygon, rounding and persistence");
}
