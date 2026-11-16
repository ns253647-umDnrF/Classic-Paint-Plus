static std::vector<BYTE> CanvasScreenPixels()
{
    RECT r;canvasWindow.GetClientRect(&r);TestBitmap bitmap(r.right,r.bottom,RGB(255,0,255));
    HDC dc=canvasWindow.GetDC();BitBlt(bitmap.dc,0,0,r.right,r.bottom,dc,0,0,SRCCOPY);canvasWindow.ReleaseDC(dc);
    return FigurePixels(bitmap.dc);
}
template<class Action> static void CheckLiveRepaint(const char* label,Action change)
{
    canvasWindow.UpdateWindow();auto before=CanvasScreenPixels();ValidateRect(canvasWindow,NULL);
    change();
    RECT dirty;bool invalidated=GetUpdateRect(canvasWindow,&dirty,FALSE)!=FALSE;
    if(!invalidated) puts("No canvas invalidation after setting change:");
    RequireCanvas(invalidated,label);
    // UpdateWindow processes only a requested repaint; it never invalidates the
    // window itself. PrintWindow/WM_PRINT would hide a missing-invalidation bug.
    canvasWindow.UpdateWindow();auto after=CanvasScreenPixels();
    if(before==after) puts("Canvas was repainted but its pixels did not change:");
    RequireCanvas(before!=after,label);
}
static void ClickFigureStyle(int style)
{
    int top=PanelPx(5),bottom=PanelPx(68),height=bottom-top;
    int y=top+(2*style+1)*height/6;
    toolSettingsWindow.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(PanelPx(25),y));
    toolBoxContainer.SendMessage(WM_LBUTTONUP,0,0);if(GetCapture()) ReleaseCapture();
}
static void TestLiveRepaint()
{
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);mainWindow.SetWindowPos(NULL,0,0,1100,1000,SWP_NOZORDER|SWP_NOACTIVATE);
    toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();canvasWindow.zoomTo(1000);
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();
    RequireCanvas(!textEditWindow.IsWindow(),"repaint regression runs before the text window can incidentally invalidate the canvas");
    SetBitmapAndInfo(CreateColorDIB(400,300,RGB(255,255,255)),NULL,NULL,FALSE);
    paletteModel.SetFgColor(RGB(25,70,170));paletteModel.SetBgColor(RGB(230,90,40));
    registrySettings.FigureKind=FIG_DIAMOND;registrySettings.ShapeFromCenter=FALSE;
    for(int percent:{100,250}) for(int dark:{0,1}) for(TOOLTYPE tool:{TOOL_RECT,TOOL_ELLIPSE,TOOL_RRECT,TOOL_FIGURE})
    {
        registrySettings.PanelMagnification=percent;registrySettings.DarkTheme=dark;ApplyTheme();ApplyPanelScale();
        toolsModel.SetActiveTool(tool);toolsModel.SetShapeStyle(0);toolsModel.SetLineWidth(3);toolsModel.SetFigureAngle(0);
        FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,80,70);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,250,200);FigureMouse(WM_LBUTTONUP,0,250,200);
        RECT bounds;RequireCanvas(toolsModel.EditableFigureBounds(bounds),"active vector frame exists for immediate style test");
        auto original=FigurePixels(imageModel.GetDC());int undo=imageModel.CanUndo();
        for(int style:{1,2,0})
        {
            CheckLiveRepaint("contour/fill option click immediately repaints the untouched figure",[&]{ClickFigureStyle(style);});
            RequireCanvas(toolsModel.GetShapeStyle()==style,"clicked contour/fill option was applied");
            RECT current;toolsModel.EditableFigureBounds(current);
            RequireCanvas(EqualRect(&bounds,&current) && FigurePixels(imageModel.GetDC())==original && imageModel.CanUndo()==undo,"style changes preserve geometry, pixels and undo until commit");
        }
        CheckLiveRepaint("line width repaints active figure",[]{toolsModel.SetLineWidth(9);});
        CheckLiveRepaint("foreground repaints active figure",[]{paletteModel.SetFgColor(RGB(180,25,120));});
        CheckLiveRepaint("numeric angle repaints active figure",[]{toolsModel.SetFigureAngle(27);});
        if(tool==TOOL_RRECT)
        {
            HWND rounding=toolSettingsWindow.GetDlgItem(IDC_FIGURE_ROUNDING);
            CheckLiveRepaint("corner radius slider repaints active rounded rectangle",[&]{
                SendMessage(rounding,TBM_SETPOS,TRUE,95);toolSettingsWindow.SendMessage(WM_VSCROLL,TB_THUMBTRACK,LPARAM(rounding));});
            SetFigureRounding(50,FIG_RRECT);
        }
        toolsModel.OnEndDraw(TRUE);paletteModel.SetFgColor(RGB(25,70,170));
    }
    puts("PASS live figure repaint: native contour/fill clicks, line width, colour, angle and corner radius; unchanged bounds/history; fresh process without text window; light/dark at 100/250%");
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();

    for(TOOLTYPE tool:{TOOL_LINE,TOOL_BEZIER})
    {
        toolsModel.SetActiveTool(tool);toolsModel.SetLineWidth(2);toolsModel.OnButtonDown(TRUE,80,80,FALSE);toolsModel.OnMouseMove(TRUE,250,180);
        CheckLiveRepaint("line/curve thickness refreshes an unfinished preview",[]{toolsModel.SetLineWidth(12);});toolsModel.OnEndDraw(TRUE);
    }
    toolsModel.SetActiveTool(TOOL_SHAPE);toolsModel.SetShapeStyle(0);
    toolsModel.OnButtonDown(TRUE,70,60,FALSE);toolsModel.OnMouseMove(TRUE,250,70);toolsModel.OnButtonUp(TRUE,250,70);
    toolsModel.OnButtonDown(TRUE,160,210,FALSE);toolsModel.OnMouseMove(TRUE,140,190);
    // An open polygon has no filled interior until it is closed; its live line
    // width is the setting whose visible result can be checked at this stage.
    CheckLiveRepaint("polygon line width refreshes while polygon remains editable",[]{toolsModel.SetLineWidth(5);});toolsModel.OnEndDraw(TRUE);

    for(TOOLTYPE tool:{TOOL_BRUSH,TOOL_RUBBER})
    {
        toolsModel.SetActiveTool(tool);
        toolsModel.SetBrushWidth(40);toolsModel.SetEraserSize(40);toolsModel.SetBrushStyle(BrushStyleRound);toolsModel.SetEraserShape(0);
        registrySettings.BrushHardness=registrySettings.EraserHardness=100;
        toolsModel.OnButtonDown(TRUE,100,100,FALSE);toolsModel.OnMouseMove(TRUE,200,140);
        CheckLiveRepaint("brush/eraser shape immediately refreshes a live stroke",[&]{if(tool==TOOL_BRUSH) toolsModel.SetBrushStyle(BrushStyleSquare);else toolsModel.SetEraserShape(1);});
        HWND panel=toolSettingsWindow.GetDlgItem(IDC_BRUSH_PANEL),slider=GetDlgItem(panel,IDC_BRUSH_HARDNESS);
        CheckLiveRepaint("brush/eraser hardness slider immediately refreshes a live stroke",[&]{SendMessage(slider,TBM_SETPOS,TRUE,80);SendMessage(panel,WM_VSCROLL,TB_THUMBTRACK,LPARAM(slider));});
        toolsModel.OnEndDraw(TRUE);
    }
    toolsModel.SetActiveTool(TOOL_AIRBRUSH);registrySettings.SprayStyle=0;registrySettings.SpraySmoothing=0;toolsModel.SetAirBrushRadius(10);
    toolsModel.OnButtonDown(TRUE,100,100,FALSE);toolsModel.OnMouseMove(TRUE,160,130);
    CheckLiveRepaint("spray radius immediately refreshes a live stroke",[]{toolsModel.SetAirBrushRadius(25);});
    HWND spray=toolSettingsWindow.GetDlgItem(IDC_SPRAY_SMOOTHING);
    CheckLiveRepaint("spray smoothing slider immediately refreshes a live stroke",[&]{SendMessage(spray,TBM_SETPOS,TRUE,20);toolSettingsWindow.SendMessage(WM_VSCROLL,TB_THUMBTRACK,LPARAM(spray));});
    HWND sprayEdit=toolSettingsWindow.GetDlgItem(IDC_SPRAY_SMOOTHING_EDIT);
    CheckLiveRepaint("spray smoothing edit immediately refreshes a live stroke",[&]{SetWindowTextW(sprayEdit,L"30");SendMessage(sprayEdit,WM_KEYDOWN,VK_RETURN,0);});
    toolsModel.OnEndDraw(TRUE);
    puts("PASS unfinished tools: line, curve, polygon, brush, eraser and spray refresh after parameter changes without mouse movement");

    toolsModel.SetActiveTool(TOOL_RECTSEL);toolsModel.SetBackgroundTransparent(FALSE);
    RasterImage raster;raster.width=3;raster.height=2;raster.pixels={0xffff0000,0xff00ff00,0xff0000ff,0xffffffff,0xff000000,0xffe65a28};
    selectionModel.InsertFromHBITMAP(MakeRasterBitmap(raster),70,70);selectionModel.m_rc=CRect(70,70,200,170);selectionModel.m_bShow=TRUE;
    registrySettings.SelectionInterpolation=0;imageModel.NotifyImageChanged();
    HWND resample=toolSettingsWindow.GetDlgItem(IDC_RESAMPLE_PANEL);
    CheckLiveRepaint("selection interpolation repaints immediately",[&]{SendMessage(resample,WM_COMMAND,MAKEWPARAM(IDC_RESAMPLE_FIRST+1,BN_CLICKED),0);});
    CheckLiveRepaint("selection opacity repaints immediately",[]{selectionModel.SetTransparency(40);});
    CheckLiveRepaint("selection background mode repaints immediately",[]{toolsModel.SetBackgroundTransparent(TRUE);});
    CheckLiveRepaint("selection angle repaints immediately",[]{selectionModel.SetRotationAngle(30);});
    toolsModel.OnEndDraw(TRUE);

    toolsModel.SetActiveTool(TOOL_TEXT);if(!textEditWindow.IsWindow()) textEditWindow.Create(canvasWindow);
    registrySettings.PointSize=18;registrySettings.strFontName=L"Arial";registrySettings.Bold=registrySettings.Italic=registrySettings.Underline=FALSE;registrySettings.TextAlignment=0;
    textEditWindow.ResetRotation();textEditWindow.UpdateFont();CRect textBounds(70,70,330,220);textEditWindow.ValidateEditRect(&textBounds);
    textEditWindow.SetWindowTextW(L"Live preview");textEditWindow.ShowWindow(SW_SHOWNOACTIVATE);toolSettingsWindow.SetFocus();
    CheckLiveRepaint("text size repaints immediately",[]{registrySettings.PointSize=26;textEditWindow.UpdateFont();});
    CheckLiveRepaint("text font face repaints immediately",[]{registrySettings.strFontName=L"Times New Roman";textEditWindow.UpdateFont();});
    CheckLiveRepaint("text bold repaints immediately",[]{registrySettings.Bold=TRUE;textEditWindow.UpdateFont();});
    CheckLiveRepaint("text alignment repaints immediately",[]{registrySettings.TextAlignment=2;textEditWindow.UpdateFont();});
    CheckLiveRepaint("text foreground colour repaints immediately",[]{paletteModel.SetFgColor(RGB(170,15,80));});
    CheckLiveRepaint("text background mode repaints immediately",[]{toolsModel.SetBackgroundTransparent(FALSE);});
    CheckLiveRepaint("text angle repaints immediately",[]{textEditWindow.SetRotationAngle(25);});
    CheckLiveRepaint("text warp repaints immediately",[]{TextWarpOptions warp;warp.style=WARP_ARC;warp.bend=50;textEditWindow.SetWarp(warp);});
    CheckLiveRepaint("text exact dimensions repaint immediately",[]{textEditWindow.SetDimensions(200,150);});
    toolsModel.OnEndDraw(TRUE);toolsModel.SetActiveTool(TOOL_RECTSEL);
    puts("PASS live selection/text repaint: resampling, opacity, background modes, angles, font size/face/bold/alignment/colour, warp and exact text dimensions");
}
