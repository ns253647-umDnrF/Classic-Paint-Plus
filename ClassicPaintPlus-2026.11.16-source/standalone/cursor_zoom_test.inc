static CStringW ZoomStatusText()
{ WCHAR text[128]={};SendMessage(g_hStatusBar,SB_GETTEXT,3,LPARAM(text));return text; }
static void RequireZoomStatus()
{
    CStringW expected;int z=toolsModel.GetZoom();if(z%10)expected.Format(L"%.1f%%",z/10.0);else expected.Format(L"%d%%",z/10);
    RequireCanvas(SendMessage(g_hStatusBar,SB_GETPARTS,0,0)==4 && ZoomStatusText()==expected,"persistent fourth status field shows exact image zoom");
}
static void CursorWheel(HWND target,POINT canvasPoint,int delta)
{
    const int before=toolsModel.GetZoom(),grip=CanvasGripSize();
    const double x=double(canvasPoint.x+canvasWindow.GetScrollPos(SB_HORZ)-grip)*1000/before;
    const double y=double(canvasPoint.y+canvasWindow.GetScrollPos(SB_VERT)-grip)*1000/before;
    POINT screen=canvasPoint;canvasWindow.ClientToScreen(&screen);
    SendMessage(target,WM_MOUSEWHEEL,MAKEWPARAM(MK_CONTROL,delta),MAKELPARAM(screen.x,screen.y));
    const int after=toolsModel.GetZoom();
    RequireCanvas(after==max(MIN_ZOOM,min(MAX_ZOOM,delta<0?before/2:before*2)),"Ctrl wheel applies a bounded zoom step");
    for(int axis:{SB_HORZ,SB_VERT})
    {
        SCROLLINFO range={sizeof(range),SIF_ALL};canvasWindow.GetScrollInfo(axis,&range);
        double image=axis==SB_HORZ?x:y;int point=axis==SB_HORZ?canvasPoint.x:canvasPoint.y;
        int wanted=int(lround(image*after/1000+grip-point));
        int expected=max(0,min(max(0,range.nMax-int(range.nPage)+1),wanted));
        RequireCanvas(range.nMin==0 && range.nPos==expected,"pointer anchor is clamped to natural nonnegative scroll range");
        if(wanted==expected)RequireCanvas(fabs(image*after/1000+grip-range.nPos-point)<=.50001,"unconstrained image point remains under pointer");
    }
    POINT origin={};canvasWindow.ImageToCanvas(origin);
    RequireCanvas(origin.x<=grip && origin.y<=grip,"wheel zoom never introduces blank space above or left of canvas");
    RequireZoomStatus();
}
static void TestCursorZoomAnchor()
{
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    for(SIZE image:{SIZE{2400,1800},SIZE{320,240},SIZE{10000,8}})
    {
        VectorReset(image.cx,image.cy);
        for(int windowX:{0,-1100})for(int start:{125,333,1000,1375,8000})for(int delta:{-WHEEL_DELTA,WHEEL_DELTA})
        {
            mainWindow.SetWindowPos(NULL,windowX,0,1050,800,SWP_NOACTIVATE|SWP_NOZORDER);
            canvasWindow.zoomTo(start);canvasWindow.ScrollToCorner(0);
            if(image.cx>3000 && start==8000)canvasWindow.updateScrollPos(70000,0);
            else if(image.cx>1000)canvasWindow.updateScrollPos(123,81);
            CRect canvas;canvasWindow.GetClientRect(canvas);
            POINT p={min(canvas.Width()/2,max(4,Zoomed(image.cx)/2)),min(canvas.Height()/2,max(4,Zoomed(image.cy)/2))};
            auto before=DocumentRaster().pixels;CursorWheel(canvasWindow,p,delta);
            RequireCanvas(DocumentRaster().pixels==before,"anchored zoom never modifies document pixels");
            // View scrolling remains a true 32-bit range, also for very wide images.
            SCROLLINFO x={sizeof(x),SIF_ALL};canvasWindow.GetScrollInfo(SB_HORZ,&x);
            RequireCanvas(x.nPos>=x.nMin && x.nPos<=max(x.nMin,x.nMax-int(x.nPage)+1),"anchor scroll position belongs to the actual scrollbar range");
        }
    }
    mainWindow.SetWindowPos(NULL,0,0,1100,900,SWP_NOACTIVATE|SWP_NOZORDER);
    VectorReset(320,240);CursorWheel(canvasWindow,{120,100},-WHEEL_DELTA);
    RequireCanvas(!canvasWindow.GetScrollPos(SB_HORZ) && !canvasWindow.GetScrollPos(SB_VERT),"small image remains at workspace top-left");
    for(int i=0;i<8;++i){CursorWheel(canvasWindow,{120,100},WHEEL_DELTA);CursorWheel(canvasWindow,{120,100},-WHEEL_DELTA);}
    canvasWindow.ScrollToCorner(0);RequireCanvas(!canvasWindow.GetScrollPos(SB_HORZ) && !canvasWindow.GetScrollPos(SB_VERT),"Home removes temporary workspace offset and returns to image top-left");
    VectorReset(1600,1200);selectionModel.CombineSelection(CRect(50,40,200,150),TriangleMask(150,110),0);selectionModel.SetRotationAngle(27);
    auto selected=SelectionRaster().pixels;CursorWheel(mainWindow,{120,100},WHEEL_DELTA);
    RequireCanvas(selectionModel.m_bShow && SelectionRaster().pixels==selected,"anchored zoom preserves rotated irregular selection");
    VectorReset(1600,1200);ActivateFigureChoice(FIG_STAR5);toolsModel.ResetObjectSize(false);NewParameterFigure();toolsModel.SetFigureAngle(32);
    CRect before,after;toolsModel.EditableFigureBounds(before);CursorWheel(mainWindow,{120,100},WHEEL_DELTA);toolsModel.EditableFigureBounds(after);
    RequireCanvas(toolsModel.HasEditableFigure() && before==after && toolsModel.FigureAngle()==32,"anchored zoom retains editable vector geometry");
    PrepareTextRegression();NewRegressionText();textEditWindow.SetWindowTextW(L"Pointer zoom");textEditWindow.SetRotationAngle(31);textEditWindow.GetEditRect(&before);
    CursorWheel(textEditWindow,{120,100},WHEEL_DELTA);textEditWindow.GetEditRect(&after);
    RequireCanvas(textEditWindow.IsWindowVisible() && before==after && WindowLabel(textEditWindow)==L"Pointer zoom","anchored wheel from RichEdit preserves text");
    RECT actual,expected=after;canvasWindow.ImageToCanvas(expected);textEditWindow.GetWindowRect(&actual);MapWindowPoints(NULL,canvasWindow,reinterpret_cast<POINT*>(&actual),2);
    RequireCanvas(EqualRect(&actual,&expected),"text input region follows the final anchored scroll origin");
    toolsModel.OnEndDraw(TRUE);
    puts("PASS pointer zoom: both wheel directions, fractional/min/max zoom, negative screen coordinates, small/large images, 32-bit scroll, selection/figure/text preservation and input geometry");
}
static void TestZoomStatusAndPaletteSpacing()
{
    CreateDirectoryW(L"cursor-zoom-tests",NULL);
    for(int dark:{0,1})for(int scale:{100,150,200,250})for(int width:{700,1500})
    {
        registrySettings.DarkTheme=dark;registrySettings.PanelMagnification=scale;ApplyTheme();ApplyPanelScale();
        mainWindow.SetWindowPos(NULL,0,0,width,900,SWP_NOACTIVATE|SWP_NOZORDER);
        for(int z:{125,1000,1375,8000}){canvasWindow.zoomTo(z);RequireZoomStatus();}
        for(int part:{0,1,2})SendMessage(g_hStatusBar,SB_SETTEXT,part,LPARAM(L"Test message"));RequireZoomStatus();
        mainWindow.SendMessage(WM_COMMAND,IDM_VIEWSTATUSBAR);mainWindow.SendMessage(WM_COMMAND,IDM_VIEWSTATUSBAR);RequireZoomStatus();
        int parts[4];SendMessage(g_hStatusBar,SB_GETPARTS,4,LPARAM(parts));
        for(int i=1;i<4;++i)RequireCanvas(parts[i]>parts[i-1],"all status fields remain ordered with permanent zoom");
        RequireCanvas(parts[3]-parts[2]>=64,"zoom field has enough room for decimal percentages");
        for(int alpha:{0,1})
        {
            DocumentAlpha().enabled=alpha;NotifyDocumentAlpha();
            CRect client;paletteWindow.GetClientRect(client);int y=client.Height()/2-PanelPx(15),side=scale==100?20:scale==150?28:scale==200?36:43;
            RECT arrow={PanelPx(20),y-PanelPx(14),PanelPx(20)+side,y-PanelPx(14)+side};
            RECT current={PanelPx(4),y+PanelPx(8),PanelPx(36),y+PanelPx(40)};
            RequireCanvas(arrow.top>=0 && arrow.bottom+PanelPx(2)<=current.top && current.bottom<=client.bottom,"raised arrow and current-colour area never overlap or clip, even with a palette scrollbar");
            RequireCanvas(PanelPx(46)-arrow.right>=PanelPx(5),"palette cells have a clear horizontal gap from exchange arrow");
            paletteWindow.SendMessage(WM_HSCROLL,SB_LEFT);POINT p={(arrow.left+arrow.right)/2,(arrow.top+arrow.bottom)/2};
            paletteModel.SetFgColor(RGB(20,60,90));paletteModel.SetBgColor(RGB(140,180,220));
            paletteWindow.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(p.x,p.y));paletteWindow.SendMessage(WM_LBUTTONUP,0,MAKELPARAM(p.x,p.y));
            RequireCanvas(paletteModel.GetFgColor()==RGB(140,180,220) && paletteModel.GetBgColor()==RGB(20,60,90),"raised exchange button hit testing matches rendering");
            paletteWindow.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(PanelPx(46)+2,y+2));paletteWindow.SendMessage(WM_LBUTTONUP,0,MAKELPARAM(PanelPx(46)+2,y+2));
            RequireCanvas(paletteModel.GetFgColor()==paletteModel.GetColor(0),"shifted first palette swatch selects its colour");
            paletteModel.SetFgColor(RGB(9,19,29));paletteWindow.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(PanelPx(43),y+2));paletteWindow.SendMessage(WM_LBUTTONUP,0,MAKELPARAM(PanelPx(43),y+2));
            RequireCanvas(paletteModel.GetFgColor()==RGB(9,19,29),"new gap is not an active colour swatch");
            CStringW name;name.Format(L"cursor-zoom-tests/palette-%d-%d-%d-%d.bmp",scale,dark,width,alpha);CaptureCanvasDialog(paletteWindow,name);
        }
        CStringW name;name.Format(L"cursor-zoom-tests/status-%d-%d-%d.bmp",scale,dark,width);CaptureCanvasDialog(g_hStatusBar,name);
    }
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();
    for(WORD language:{WORD(0x0409),WORD(0x0419)}){registrySettings.UILanguage=language;ApplyPreferredLanguage();mainWindow.ReloadLanguage();RequireZoomStatus();}
    DocumentAlpha().enabled=false;NotifyDocumentAlpha();canvasWindow.zoomTo(1000);
    puts("PASS permanent zoom status and palette spacing: four scales, both themes, normal/narrow windows, alpha swatches, scrollbar height, exact hit testing and language/theme/visibility changes");
}
static void TestCursorZoom(){TestCursorZoomAnchor();TestZoomStatusAndPaletteSpacing();}
