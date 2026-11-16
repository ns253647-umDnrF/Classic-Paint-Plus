static LPCWSTR menuValue;
static bool cancelMenuEdit;
static HMENU testNumberMenu;
static int expectedMenuValue;
static HWND FindMenuEdit()
{
    HWND menu=NULL;
    while((menu=FindWindowExW(NULL,menu,L"#32768",NULL))!=NULL)
        if(HWND edit=GetDlgItem(menu,IDC_MARKER_EDIT)) return edit;
    return NULL;
}
static void EditMenuValue(HMENU menu,LPCWSTR value,bool cancel,int expected)
{
    menuValue=value;cancelMenuEdit=cancel;testNumberMenu=menu;expectedMenuValue=expected;
    static int phase,attempts;phase=0;attempts=0;
    UINT_PTR timer=SetTimer(NULL,0,100,[](HWND,UINT,UINT_PTR timer,DWORD)
    {
        HWND edit=FindMenuEdit();
        if(!edit && ++attempts<10) return;
        if(!edit)
        {
            HWND window=NULL;
            while((window=FindWindowExW(NULL,window,L"#32768",NULL))!=NULL)
                printf("Menu window %p visible %d thread %lu\n",window,IsWindowVisible(window),GetWindowThreadProcessId(window,NULL));
        }
        RequireCanvas(edit!=NULL,"inline edit is a child of the native menu window");
        HWND popup=GetParent(edit);
        if(phase==0)
        {
            SetWindowTextW(edit,menuValue);
            if(cancelMenuEdit) PostMessage(mainWindow,WM_KEYDOWN,VK_ESCAPE,0);
            else PostMessage(mainWindow,WM_KEYDOWN,VK_RETURN,0);
            ++phase;return;
        }
        KillTimer(NULL,timer);
        if(!cancelMenuEdit)
        {
            RequireCanvas(IsWindowVisible(popup),"typing and Enter leave the menu visible");
            UINT id=GetMenuItemID(testNumberMenu,0);
            RequireCanvas((id==IDM_MARKER_SIZE?CanvasGripSize():id==IDM_RECENT_FONTS?int(registrySettings.RecentFontsCount):ToolboxTopMargin())==expectedMenuValue,"inline edit validates and commits values");
            CaptureCanvasDialog(popup,id==IDM_MARKER_SIZE?L"canvas-marker-input.bmp":L"toolbox-margin-input.bmp");
            EndMenu();
        }
    });
    TrackPopupMenu(menu,TPM_LEFTALIGN|TPM_TOPALIGN|TPM_RETURNCMD,200,100,0,mainWindow,NULL);
    KillTimer(NULL,timer);
    UINT id=GetMenuItemID(menu,0);
    RequireCanvas((id==IDM_MARKER_SIZE?CanvasGripSize():id==IDM_RECENT_FONTS?int(registrySettings.RecentFontsCount):ToolboxTopMargin())==expected,"menu close preserves accepted value or cancels edit");
}

static void SetTestShift(bool down)
{
    BYTE keys[256];GetKeyboardState(keys);keys[VK_SHIFT]=down?0x80:0;SetKeyboardState(keys);
}

static void TestPanelSettings()
{
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    HMENU settings=GetSubMenu(mainWindow.GetMenu(),4),sizes=GetSubMenu(settings,0),markers=GetSubMenu(settings,2);
    RequireCanvas(GetMenuItemCount(sizes)==4 && GetMenuItemID(sizes,0)==IDM_PANEL_100 &&
        GetMenuItemID(sizes,3)==IDM_PANEL_250,"panel size is the first settings submenu with four choices");
    RequireCanvas(GetSubMenu(markers,0) && GetMenuItemID(GetSubMenu(markers,0),0)==IDM_MARKER_SIZE &&
        GetMenuItemID(markers,1)==IDM_MARKER_COLOR,"marker size is a submenu and color is a command");

    // Verify the actual two-button gesture and Shift transitions without moving the pointer.
    toolsModel.SetActiveTool(TOOL_FIGURE);toolsModel.SetFigureAngle(0);registrySettings.FigureKind=FIG_TRIANGLE;registrySettings.ShapeFromCenter=FALSE;
    toolsModel.SetZoom(1000);toolsModel.SetShapeStyle(0);toolsModel.SetLineWidth(3);
    imageModel.PushImageForUndo(CreateColorDIB(180,180,RGB(255,255,255)));imageModel.ClearHistory();
    canvasWindow.updateScrollRange();canvasWindow.updateScrollPos();
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,40,70);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,140,110);
    FigureMouse(WM_RBUTTONDOWN,MK_LBUTTON|MK_RBUTTON,140,110);
    FigureMouse(WM_MOUSEMOVE,MK_LBUTTON|MK_RBUTTON,100,145);
    TestBitmap freeAngle(180,180,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(freeAngle.dc);
    SetTestShift(true);canvasWindow.SendMessage(WM_KEYDOWN,VK_SHIFT,0);
    TestBitmap snapped(180,180,RGB(255,255,255)),expected(180,180,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(snapped.dc);
    double actual=atan2(145-89.5,100-89.5)-atan2(110-89.5,140-89.5);
    double snap=FigureRotationAngle(actual,true);
    RequireCanvas(fabs(snap*180/M_PI-60)<.001,"rotation snaps to the nearest absolute 15 degrees");
    DrawFigure(expected.dc,FIG_TRIANGLE,CRect(40,70,140,110),toolsModel.GetFgBrush(),toolsModel.GetBgBrush(),3,0,snap);
    RequireCanvas(FigurePixels(snapped.dc)==FigurePixels(expected.dc),"Shift changes only angle, not dimensions");
    WCHAR status[128]={};SendMessageW(g_hStatusBar,SB_GETTEXTW,1,reinterpret_cast<LPARAM>(status));
    RequireCanvas(wcsstr(status,L"60.0") && wcschr(status,0xb0),"live angle is visible in status bar");
    SetTestShift(false);canvasWindow.SendMessage(WM_KEYUP,VK_SHIFT,0);
    TestBitmap released(180,180,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(released.dc);
    RequireCanvas(FigurePixels(released.dc)==FigurePixels(freeAngle.dc),"releasing Shift restores the free angle without moving");
    canvasWindow.SendMessage(WM_KEYDOWN,VK_ESCAPE,0);

    HWND toolbar=FindWindowExW(toolBoxContainer,NULL,TOOLBARCLASSNAMEW,NULL);
    const UINT commands[]={IDM_PANEL_100,IDM_PANEL_150,IDM_PANEL_200,IDM_PANEL_250};
    for(int index=0;index<4;++index)
    {
        toolsModel.SetActiveTool(TOOL_FIGURE);
        mainWindow.SendMessage(WM_COMMAND,commands[index]);
        UpdateSettingsChecks(mainWindow.GetMenu());
        for(int i=0;i<4;++i) RequireCanvas(!!(GetMenuState(sizes,i,MF_BYPOSITION)&MF_CHECKED)==(i==index),"exactly one panel-size checkbox");
        CRect box,button,toolRect;toolBoxContainer.GetClientRect(&box);
        RequireCanvas(box.Width()==PanelPx(50),"toolbox content is exactly scaled");
        SendMessage(toolbar,TB_GETITEMRECT,0,reinterpret_cast<LPARAM>(&button));
        RequireCanvas(button.Width()==PanelPx(50)/2 && button.Height()==PanelPx(25),"native tool button dimensions");
        GetWindowRect(toolbar,&toolRect);toolBoxContainer.ScreenToClient(&toolRect);
        RequireCanvas(toolRect.top==PanelPx(40),"classic top spacing scales with the panel");
        HIMAGELIST icons=(HIMAGELIST)SendMessage(toolbar,TB_GETIMAGELIST,0,0);int w=0,h=0;ImageList_GetIconSize(icons,&w,&h);
        RequireCanvas(w==PanelPx(16) && h==PanelPx(16) && ImageList_GetImageCount(icons)==16,"native-resolution icons for all original tools");
        HWND row=FindWindowExW(toolBoxContainer,NULL,L"PaintFigureControls",NULL),gallery=GetDlgItem(row,IDC_FIGURE_GALLERY);
        GetClientRect(gallery,&button);RequireCanvas(button.Width()==PanelPx(50)/2 && button.Height()==PanelPx(25),"gallery button scales with its toolbar row");
        SendMessage(gallery,WM_KEYDOWN,VK_DOWN,0);
        HWND popup=FindWindowW(L"PaintFigurePopup",NULL);GetClientRect(GetDlgItem(popup,IDC_FIGURE_FIRST),&button);
        RequireCanvas(IsWindowVisible(popup) && button.Width()==PanelPx(28),"flyout icons and buttons scale");
        SendMessage(GetFocus(),WM_KEYDOWN,VK_ESCAPE,0);
        // The last controls must remain reachable, even in a short window at 250%.
        toolBoxContainer.SendMessage(WM_VSCROLL,SB_PAGEDOWN);
        toolBoxContainer.SendMessage(WM_VSCROLL,SB_PAGEDOWN);
        HWND origin=GetDlgItem(toolSettingsWindow,IDC_FIGURE_ORIGIN);CRect originRect;GetWindowRect(origin,&originRect);toolBoxContainer.ScreenToClient(&originRect);
        RequireCanvas(originRect.bottom<=box.bottom,"vertical scrolling reaches lower tool controls");
        toolsModel.SetActiveTool(TOOL_ZOOM);
        HWND edit=GetDlgItem(toolSettingsWindow,IDC_ZOOM_EDIT);
        RequireCanvas(IsWindowVisible(edit),"zoom field appears under slider");
        if(PanelPercent()>100)
        {
            HWND track=FindWindowExW(toolSettingsWindow,NULL,TRACKBAR_CLASSW,NULL);CRect trackRect;GetClientRect(track,&trackRect);
            SendMessage(track,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(PanelPx(12),PanelPx(8)));
            RequireCanvas(toolsModel.GetZoom()==8000,"large zoom slider reaches maximum by mouse");
            SendMessage(track,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(PanelPx(12),trackRect.bottom-PanelPx(8)));
            SendMessage(track,WM_LBUTTONUP,0,MAKELPARAM(PanelPx(12),trackRect.bottom-PanelPx(8)));
            RequireCanvas(toolsModel.GetZoom()==125,"large slider dragging updates zoom and releases capture");
        }
        SetWindowTextW(edit,L"137,5%");SendMessage(edit,WM_KEYDOWN,VK_RETURN,0);
        RequireCanvas(toolsModel.GetZoom()==1375,"manual decimal zoom, comma and percent sign");
        SetWindowTextW(edit,L"175");SendMessage(edit,WM_KEYDOWN,VK_RETURN,0);
        RequireCanvas(toolsModel.GetZoom()==1750,"manual zoom without percent sign");
        for(const WCHAR* bad:{L"0",L"-1",L"801",L"nan",L"junk",L""})
        {
            SetWindowTextW(edit,bad);SendMessage(edit,WM_KILLFOCUS,0,0);
            RequireCanvas(toolsModel.GetZoom()==1750,"invalid zoom preserves the current scale");
        }
        toolsModel.SetZoom(125);WCHAR zoomText[32];GetWindowTextW(edit,zoomText,_countof(zoomText));
        RequireCanvas(!lstrcmpW(zoomText,L"12.5%"),"zoom changes synchronize the field");
        toolsModel.SetActiveTool(TOOL_RECTSEL);
        RequireCanvas(!IsWindowVisible(edit),"zoom field hides for another tool");
        // Palette: real gaps must be inert; colored interiors must remain clickable.
        paletteWindow.SendMessage(WM_HSCROLL,SB_LEFT);
        CRect pal;paletteWindow.GetClientRect(&pal);int top=pal.bottom/2-PanelPx(15);
        COLORREF sentinel=RGB(7,19,43);paletteModel.SetFgColor(sentinel);
        paletteWindow.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(PanelPx(40+PALETTE_EXTRA_GAP)+PanelPx(15),top+PanelPx(4)));
        paletteWindow.SendMessage(WM_LBUTTONUP,0,MAKELPARAM(PanelPx(40+PALETTE_EXTRA_GAP)+PanelPx(15),top+PanelPx(4)));
        RequireCanvas(paletteModel.GetFgColor()==sentinel,"palette gap does not select a color");
        paletteWindow.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(PanelPx(40+PALETTE_EXTRA_GAP)+PanelPx(5),top+PanelPx(5)));
        paletteWindow.SendMessage(WM_LBUTTONUP,0,0);
        RequireCanvas(paletteModel.GetFgColor()==paletteModel.GetColor(0),"scaled palette color hit testing");
        toolBoxContainer.SendMessage(WM_MOUSEWHEEL,MAKEWPARAM(0,1200));
        mainWindow.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);
        WCHAR path[80];swprintf_s(path,L"panels-%d-preview.bmp",PanelPercent());CaptureCanvasDialog(mainWindow,path);
        // A separate screenshot includes the zoom field, rounding slider and scrolled panel.
        toolsModel.SetActiveTool(PanelPercent()==200?TOOL_ZOOM:TOOL_RRECT);
        if(PanelPercent()>100) toolBoxContainer.SendMessage(WM_VSCROLL,SB_PAGEDOWN);
        mainWindow.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);
        swprintf_s(path,L"panels-%d-controls.bmp",PanelPercent());CaptureCanvasDialog(mainWindow,path);
    }
    mainWindow.SendMessage(WM_COMMAND,IDM_PANEL_100);toolsModel.SetActiveTool(TOOL_RECTSEL);toolsModel.SetZoom(1000);
    // Drive a real menu loop; Enter must keep the native popup alive.
    EditMenuValue(GetSubMenu(markers,0),L"16",false,3);
    EditMenuValue(GetSubMenu(markers,0),L"8",true,3);
    HMENU margin=GetSubMenu(settings,1);
    EditMenuValue(margin,L"72",false,72);
    CRect toolbarRect;GetWindowRect(toolbar,&toolbarRect);toolBoxContainer.ScreenToClient(&toolbarRect);
    RequireCanvas(toolbarRect.top==72,"custom top margin changes toolbar geometry");
    EditMenuValue(margin,L"501",false,72);
    EditMenuValue(margin,L"0",false,0);
    EditMenuValue(margin,L"40",false,40);
    for(int size=3;size<=15;++size)
    {
        WCHAR value[8];swprintf_s(value,L"%d",size);EditMenuValue(GetSubMenu(markers,0),value,false,size);
        RequireCanvas(registrySettings.CanvasMarkerSize==DWORD(size),"marker editor accepts every size from 3 to 15");
        CRect base,marker;canvasWindow.GetImageRect(base);canvasWindow.ImageToCanvas(base);base.InflateRect(size,size);
        getSizeBoxRect(&marker,HIT_LOWER_RIGHT,&base,CanvasGripSize());
        RequireCanvas(marker.Width()==size && marker.Height()==size,"marker square has exact configured pixel dimensions");
        canvasWindow.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW);HDC dc=canvasWindow.GetDC();
        for(int y=marker.top;y<marker.bottom;++y) for(int x=marker.left;x<marker.right;++x)
            RequireCanvas(GetPixel(dc,x,y)==registrySettings.CanvasMarkerColor,"marker uses configured color over its complete area");
        canvasWindow.ReleaseDC(dc);
        canvasWindow.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(marker.right-1,marker.bottom-1));
        canvasWindow.SendMessage(WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(marker.right+1,marker.bottom));
        canvasWindow.SendMessage(WM_LBUTTONUP,0,MAKELPARAM(marker.right+1,marker.bottom));
        RequireCanvas(imageModel.GetWidth()==182 && imageModel.GetHeight()==181,"whole expanded marker is draggable");
        imageModel.Undo();imageModel.ClearHistory();canvasWindow.updateScrollRange();
    }
    // Choose a marker color through the real palette dialog, then cancel a second change.
    toolsModel.SetZoom(1000);samplePoint={20,20};canvasWindow.ImageToCanvas(samplePoint);
    SetPixelV(imageModel.GetDC(),20,20,RGB(155,25,190));cancelDialog=false;ticks=0;exercised=false;
    UINT_PTR timer=SetTimer(NULL,0,30,DriveDialog);mainWindow.SendMessage(WM_COMMAND,IDM_MARKER_COLOR);KillTimer(NULL,timer);
    RequireCanvas(exercised && registrySettings.CanvasMarkerColor==RGB(155,25,190),"marker color dialog accepts sampled color");
    SetPixelV(imageModel.GetDC(),20,20,RGB(10,240,90));cancelDialog=true;ticks=0;exercised=false;
    timer=SetTimer(NULL,0,30,DriveDialog);mainWindow.SendMessage(WM_COMMAND,IDM_MARKER_COLOR);KillTimer(NULL,timer);
    RequireCanvas(registrySettings.CanvasMarkerColor==RGB(155,25,190),"cancel marker color keeps previous color");
    RegistrySettings loaded;loaded.Load(SW_HIDE);
    RequireCanvas(loaded.CanvasMarkerSize==15 && loaded.CanvasMarkerColor==RGB(155,25,190) && loaded.PanelMagnification==100,"panel and marker preferences persist");
    registrySettings.CanvasMarkerSize=3;registrySettings.CanvasMarkerColor=RGB(0,120,215);canvasWindow.updateScrollRange();
    toolsModel.SetActiveTool(TOOL_RECTSEL);mainWindow.ShowWindow(SW_HIDE);
    puts("PASS panel settings: Shift angle/status, 4 panel scales, native icons, scrolling, palette gaps, manual zoom, 3-15px markers, color dialog and persistence");
}
