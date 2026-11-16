#include "applicationinfo.h"
static POINT SelectionCanvasPoint(int x,int y)
{ POINT p={x,y};canvasWindow.ImageToCanvas(p);return p; }
static void SelectionMouse(UINT message,POINT point,WPARAM keys=0)
{ canvasWindow.SendMessage(message,keys,MAKELPARAM(point.x,point.y)); }
static POINT SelectionRotationPointer(double degrees)
{
    CRect rc=selectionModel.m_rc;canvasWindow.ImageToCanvas(rc);
    double radius=rc.Width()/2.0+1,a=degrees*M_PI/180;
    return {LONG(lround((rc.left+rc.right)/2.0+radius*cos(a))),LONG(lround((rc.top+rc.bottom)/2.0+radius*sin(a)))};
}
static void BeginSelectionRotation()
{
    POINT start=SelectionRotationPointer(selectionModel.RotationAngle());
    SelectionMouse(WM_RBUTTONDOWN,start,MK_RBUTTON);
    RequireCanvas(selectionModel.IsRotating() && GetCapture()==canvasWindow,"right button on selection border starts rotation");
}
static RasterImage SelectionRaster()
{ HBITMAP bitmap=selectionModel.GetSelectionContents();RasterImage image;RequireCanvas(bitmap && ReadRaster(bitmap,image),"read rotated selection contents");DeleteObject(bitmap);return image; }
static bool helpCrop=false;
static VOID CALLBACK CaptureHelpCanvas(HWND,UINT,UINT_PTR timer,DWORD)
{
    HWND window=FindJpegDialogControl(IDC_CANVAS_FILL);if(!window) return;KillTimer(NULL,timer);
    if(helpCrop) { SetDlgItemInt(window,IDC_EDGE_LEFT,40,TRUE);SetDlgItemInt(window,IDC_EDGE_RIGHT,40,TRUE);SetDlgItemInt(window,IDC_EDGE_TOP,20,TRUE);SetDlgItemInt(window,IDC_EDGE_BOTTOM,20,TRUE); }
    CaptureCanvasDialog(window,helpCrop?L"selection-help-tests/crop.bmp":L"selection-help-tests/canvas-size.bmp");PostMessage(window,WM_COMMAND,IDCANCEL,0);
}
static VOID CALLBACK DriveAboutWindow(HWND,UINT,UINT_PTR timer,DWORD)
{
    HWND window=FindJpegDialogControl(IDC_ABOUT_NAME);if(!window) return;KillTimer(NULL,timer);
    WCHAR title[200],credits[2048];GetWindowTextW(window,title,_countof(title));GetDlgItemTextW(window,IDC_ABOUT_CREDITS,credits,_countof(credits));
    RequireCanvas(wcsstr(title,L"Classic Paint Plus") && wcsstr(credits,L"Benedikt Freisen") && wcsstr(credits,L"Katayama Hirofumi MZ") && wcsstr(credits,L"Stanislav Motylkov"),"About identifies application and verified upstream authors");
    RequireCanvas(wcsstr(credits,L"Nicolay S.") && GetDlgItem(window,IDC_ABOUT_BANNER),"About credits Plus developer and includes supplied banner");
    RequireCanvas(wcsstr(credits,L"Nicolay S.\r\nns253647@gmail.com"),"contact email appears immediately below Plus authorship in every language");
    RECT banner,dialog;GetWindowRect(GetDlgItem(window,IDC_ABOUT_BANNER),&banner);GetClientRect(window,&dialog);
    RequireCanvas(abs((banner.bottom-banner.top)*800-(banner.right-banner.left)*142)<=400,"About banner preserves original artwork aspect ratio within half a device pixel");
    for(int id:{IDC_ABOUT_DESCRIPTION,IDC_ABOUT_LICENSE})
    {
        HWND control=GetDlgItem(window,id);RECT available;GetClientRect(control,&available);RECT needed=available;WCHAR text[2048];GetWindowTextW(control,text,_countof(text));
        HDC dc=GetDC(control);HGDIOBJ old=SelectObject(dc,reinterpret_cast<HFONT>(SendMessage(control,WM_GETFONT,0,0)));
        DrawTextW(dc,text,-1,&needed,DT_CALCRECT|DT_WORDBREAK);SelectObject(dc,old);ReleaseDC(control,dc);
        if(needed.bottom>available.bottom) printf("About text clipped: language=%04x control=%d needed=%ld available=%ld\n",GetThreadUILanguage(),id,needed.bottom,available.bottom);
        RequireCanvas(needed.bottom<=available.bottom,"About text fits in every translated dialog");
    }
    if(PRIMARYLANGID(GetThreadUILanguage())==LANG_RUSSIAN)
        CaptureCanvasDialog(window,registrySettings.DarkTheme?L"selection-help-tests/about-dark.bmp":L"selection-help-tests/about-light.bmp");
    PostMessage(window,WM_COMMAND,IDOK,0);
}
static void CaptureRPaintHelpAssets()
{
    CreateDirectoryW(L"selection-help-tests",NULL);toolsModel.OnEndDraw(TRUE);
    registrySettings.UILanguage=MAKELANGID(LANG_RUSSIAN,SUBLANG_DEFAULT);ApplyPreferredLanguage();
    registrySettings.DarkTheme=0;registrySettings.PanelMagnification=100;registrySettings.ToolboxTopMargin=40;ApplyTheme();ApplyPanelScale();
    mainWindow.SetWindowPos(NULL,0,0,960,740,SWP_NOACTIVATE|SWP_NOZORDER);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);canvasWindow.zoomTo(1000);
    TestBitmap scene(640,420,RGB(227,240,250));
    Rect(scene.dc,0,305,640,420,RGB(94,156,92),RGB(152,198,122),1,2);
    Ellp(scene.dc,470,40,550,120,RGB(230,169,35),RGB(255,216,75),2,2);
    POINT hills[]={{0,305},{90,175},{220,305},{365,135},{530,305},{640,230},{640,320},{0,320}};
    Poly(scene.dc,hills,_countof(hills),RGB(103,145,151),RGB(161,193,192),1,2,TRUE,FALSE);
    Rect(scene.dc,160,205,330,330,RGB(79,88,99),RGB(250,239,207),2,2);
    POINT roof[]={{140,205},{245,115},{350,205}};Poly(scene.dc,roof,3,RGB(130,61,45),RGB(218,103,77),2,2,TRUE,FALSE);
    Rect(scene.dc,184,232,220,270,RGB(65,88,105),RGB(104,181,220),2,2);
    Rect(scene.dc,270,232,306,270,RGB(65,88,105),RGB(104,181,220),2,2);
    Rect(scene.dc,230,269,259,330,RGB(90,65,44),RGB(153,117,77),2,2);
    SetBkMode(scene.dc,TRANSPARENT);SetTextColor(scene.dc,RGB(32,60,85));TextOutW(scene.dc,26,26,L"Classic Paint Plus",17);
    imageModel.PushImageForUndo(CopyDIBImage(scene.bitmap));imageModel.ClearHistory();mainWindow.SetWindowTextW(L"Classic Paint Plus — sample.png");
    toolsModel.SetActiveTool(TOOL_BRUSH);CaptureCanvasDialog(mainWindow,L"selection-help-tests/overview.bmp");CaptureCanvasDialog(toolSettingsWindow,L"selection-help-tests/brush.bmp");CaptureCanvasDialog(paletteWindow,L"selection-help-tests/palette.bmp");
    for(bool crop:{false,true}) { helpCrop=crop;SetTimer(NULL,0,80,CaptureHelpCanvas);ShowCanvasDialog(mainWindow,crop); }
    toolsModel.SetActiveTool(TOOL_RECTSEL);selectionModel.setMask(CRect(135,110,355,335),NULL);selectionModel.m_bShow=TRUE;
    BeginSelectionRotation();POINT p=SelectionRotationPointer(30);SelectionMouse(WM_MOUSEMOVE,p,MK_RBUTTON|MK_SHIFT);
    CaptureCanvasDialog(mainWindow,L"selection-help-tests/rotation.bmp");SelectionMouse(WM_RBUTTONUP,p,MK_SHIFT);
    registrySettings.DarkTheme=1;ApplyTheme();CaptureCanvasDialog(mainWindow,L"selection-help-tests/dark.bmp");toolsModel.OnEndDraw(TRUE);
    for(bool dark:{false,true}) { registrySettings.DarkTheme=dark;ApplyTheme();SetTimer(NULL,0,60,DriveAboutWindow);mainWindow.SendMessage(WM_COMMAND,IDM_HELPINFO,0); }
    registrySettings.DarkTheme=0;ApplyTheme();
    puts("PASS native help illustrations and About dialog in both themes");
}
static CStringW launchedHelp;
static HINSTANCE WINAPI CaptureHelpLaunch(HWND,LPCWSTR verb,LPCWSTR file,LPCWSTR,LPCWSTR,INT)
{ RequireCanvas(!lstrcmpW(verb,L"open"),"help opens with registered browser");launchedHelp=file;return HINSTANCE(INT_PTR(33)); }
static void TestRPaintTranslationsAndHelp()
{
    if(testLanguages.empty()) EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    HWND toolbarHost=CreateWindowW(L"STATIC",L"",WS_POPUP,0,0,120,400,NULL,NULL,g_hinstExe,NULL);
    for(WORD language:testLanguages)
    {
        registrySettings.UILanguage=language;ApplyPreferredLanguage();
        for(UINT id=IDS_ABOUT_DESCRIPTION;id<=IDS_ABOUT_HELP;++id) RequireCanvas(!ExactString(language,id).IsEmpty(),"About and help strings translated in every locale");
        CPaintToolBar bar;RequireCanvas(bar.DoCreate(toolbarHost)!=FALSE,"create actual localized toolbar");
        for(int i=0;i<NUM_TOOLS;++i)
        {
            if(ID_FREESEL+i>=ID_LINE)continue;
            CStringW expected=ExactString(language,IDS_TOOLTIP1+i),actual;int length=int(bar.SendMessage(TB_GETBUTTONTEXTW,ID_FREESEL+i,0));
            RequireCanvas(length==expected.GetLength(),"localized toolbar tooltip is never truncated");
            bar.SendMessage(TB_GETBUTTONTEXTW,ID_FREESEL+i,LPARAM(actual.GetBuffer(length+1)));actual.ReleaseBuffer();
            RequireCanvas(actual==expected,"actual toolbar stores complete translated tooltip");
        }
        HIMAGELIST images=HIMAGELIST(bar.SendMessage(TB_SETIMAGELIST,0,0));bar.DestroyWindow();ImageList_Destroy(images);
        HACCEL accelerators=LoadAcceleratorsW(g_hinstExe,MAKEINTRESOURCEW(ID_ACCELERATORS));int count=CopyAcceleratorTableW(accelerators,NULL,0);std::vector<ACCEL> entries(count);CopyAcceleratorTableW(accelerators,entries.data(),count);
        bool f1=false;for(const auto& key:entries) if(key.key==VK_F1 && key.cmd==IDM_HELPHELPTOPICS && (key.fVirt&FVIRTKEY)) f1=true;
        RequireCanvas(f1,"F1 help accelerator present in every locale");
        SetTimer(NULL,0,60,DriveAboutWindow);ShowRPaintAbout(mainWindow);
    }
    DestroyWindow(toolbarHost);
    SetHelpLauncherForTests(CaptureHelpLaunch);
    for(WORD language:{WORD(MAKELANGID(LANG_RUSSIAN,SUBLANG_DEFAULT)),WORD(MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US))})
    {
        registrySettings.UILanguage=language;ApplyPreferredLanguage();CStringW expected=RPaintHelpPath();RequireCanvas(GetFileAttributesW(expected)!=INVALID_FILE_ATTRIBUTES,"help file shipped beside executable");
        WCHAR original[MAX_PATH];GetCurrentDirectoryW(_countof(original),original);SetCurrentDirectoryW(L"selection-help-tests");
        launchedHelp.Empty();mainWindow.SendMessage(WM_COMMAND,IDM_HELPHELPTOPICS,0);SetCurrentDirectoryW(original);
        RequireCanvas(launchedHelp==expected,"Help menu opens localized file relative to EXE, independent of current folder");
        MSG key={};key.hwnd=mainWindow;key.message=WM_KEYDOWN;key.wParam=VK_F1;launchedHelp.Empty();
        RequireCanvas(TranslateAcceleratorW(mainWindow,LoadAcceleratorsW(g_hinstExe,MAKEINTRESOURCEW(ID_ACCELERATORS)),&key)!=0 && launchedHelp==expected,"actual F1 route opens local help");
    }
    SetHelpLauncherForTests(NULL);registrySettings.UILanguage=MAKELANGID(LANG_RUSSIAN,SUBLANG_DEFAULT);ApplyPreferredLanguage();
    puts("PASS all 30 native localized toolbars and About dialogs, full tooltip text, local Help menu and F1 routes");
}
static void TestSelectionRotationAndHelp()
{
    CreateDirectoryW(L"selection-help-tests",NULL);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    registrySettings.DarkTheme=0;ApplyTheme();toolsModel.OnEndDraw(TRUE);toolsModel.SetActiveTool(TOOL_RECTSEL);
    canvasWindow.zoomTo(1000);paletteModel.SetBgColor(RGB(255,255,255));toolsModel.SetBackgroundTransparent(FALSE);registrySettings.SelectionInterpolation=0;
    RasterImage original;original.width=220;original.height=160;original.pixels.assign(220*160,0xffeeeeee);
    for(int y=40;y<80;++y) for(int x=40;x<100;++x) original.pixels[y*220+x]=0xff000000|DWORD(x*2)<<16|DWORD(y*3)<<8|70;
    imageModel.PushImageForUndo(MakeRasterBitmap(original));imageModel.ClearHistory();
    selectionModel.setMask(CRect(40,40,100,80),NULL);selectionModel.m_bShow=TRUE;
    RasterImage initial=SelectionRaster();
    BeginSelectionRotation();POINT p=SelectionRotationPointer(37);SelectionMouse(WM_MOUSEMOVE,p,MK_RBUTTON|MK_SHIFT);
    RequireCanvas(selectionModel.RotationAngle()==30,"Shift snaps selection rotation to 15 degrees");
    WCHAR angle[128]={};SendMessageW(g_hStatusBar,SB_GETTEXT,2,LPARAM(angle));RequireCanvas(angle[0]!=0,"selection angle shown in status bar");
    canvasWindow.SendMessage(WM_KEYUP,VK_SHIFT,0);RequireCanvas(fabs(selectionModel.RotationAngle()-30)>2,"Shift release restores unsnapped pointer angle without another mouse move");
    canvasWindow.SendMessage(WM_KEYDOWN,VK_SHIFT,0);RequireCanvas(selectionModel.RotationAngle()==30,"Shift press updates angle immediately");
    RequireCanvas(DocumentRaster().pixels==original.pixels,"rotation preview leaves document untouched");
    SelectionMouse(WM_RBUTTONUP,p,MK_SHIFT);RequireCanvas(!selectionModel.IsRotating() && GetCapture()!=canvasWindow && selectionModel.RotationAngle()==30,"release retains floating rotation and releases mouse capture");
    RasterImage rotated=SelectionRaster();RequireCanvas(rotated.width>initial.width && rotated.height>initial.height && (rotated.pixels.front()>>24)==0,"rotated selection grows bounds and leaves corners transparent");
    auto beforeCancel=rotated;BeginSelectionRotation();SelectionMouse(WM_MOUSEMOVE,SelectionRotationPointer(94),MK_RBUTTON);
    canvasWindow.SendMessage(WM_KEYDOWN,VK_ESCAPE,0);
    RequireCanvas(selectionModel.m_bShow && selectionModel.RotationAngle()==30 && SelectionRaster().pixels==beforeCancel.pixels,"Escape restores previous rotation and keeps selection");
    BeginSelectionRotation();SelectionMouse(WM_MOUSEMOVE,SelectionRotationPointer(121),MK_RBUTTON);ReleaseCapture();
    RequireCanvas(!selectionModel.IsRotating() && selectionModel.RotationAngle()==30,"capture loss cancels only current rotation gesture");
    BeginSelectionRotation();SelectionMouse(WM_MOUSEMOVE,SelectionRotationPointer(0),MK_RBUTTON|MK_SHIFT);SelectionMouse(WM_RBUTTONUP,SelectionRotationPointer(0),MK_SHIFT);
    RequireCanvas(selectionModel.RotationAngle()==0 && SelectionRaster().pixels==initial.pixels,"repeated rotations return to exact original pixels without cumulative blur");
    BeginSelectionRotation();SelectionMouse(WM_MOUSEMOVE,SelectionRotationPointer(90),MK_RBUTTON|MK_SHIFT);SelectionMouse(WM_RBUTTONUP,SelectionRotationPointer(90),MK_SHIFT);
    RasterImage quarter=SelectionRaster();RequireCanvas(quarter.width==40 && quarter.height==60,"90-degree rotation swaps dimensions exactly");
    for(int y=0;y<40;++y) for(int x=0;x<60;++x) RequireCanvas(quarter.pixels[size_t(x)*40+39-y]==initial.pixels[y*60+x],"90-degree selection pixels remain exact with nearest sampling");
    CaptureCanvasDialog(mainWindow,L"selection-help-tests/selection-rotation.bmp");
    selectionModel.Landing();RasterImage committed=DocumentRaster();RequireCanvas(committed.pixels!=original.pixels,"selection landing commits rotation");
    imageModel.Undo();RequireCanvas(DocumentRaster().pixels==original.pixels,"Undo restores document before rotated selection");
    imageModel.Redo();RequireCanvas(DocumentRaster().pixels==committed.pixels,"Redo restores rotated selection");
    // Exercise actual pointer hit testing after zooming and scrolling, then move
    // and resize the rotated selection rather than only rotating a stationary one.
    toolsModel.OnEndDraw(TRUE);imageModel.PushImageForUndo(MakeRasterBitmap(original));imageModel.ClearHistory();
    canvasWindow.zoomTo(8000);canvasWindow.updateScrollPos(120,100);
    selectionModel.setMask(CRect(40,40,100,80),NULL);selectionModel.m_bShow=TRUE;
    BeginSelectionRotation();p=SelectionRotationPointer(90);SelectionMouse(WM_MOUSEMOVE,p,MK_RBUTTON|MK_SHIFT);SelectionMouse(WM_RBUTTONUP,p,MK_SHIFT);
    RequireCanvas(selectionModel.RotationAngle()==90,"selection rotation accounts for zoom and scroll offset");
    POINT center=SelectionCanvasPoint(70,60),moved=SelectionCanvasPoint(80,70);
    SelectionMouse(WM_LBUTTONDOWN,center,MK_LBUTTON);SelectionMouse(WM_MOUSEMOVE,moved,MK_LBUTTON);SelectionMouse(WM_LBUTTONUP,moved);
    RequireCanvas(selectionModel.m_rc==CRect(50,50,110,90) && selectionModel.RotationAngle()==90,"left drag moves rotated selection without changing its angle");
    POINT fixed=RotateSelectionPoint({50,70},selectionModel.m_rc,90);
    p=SelectionRotationPointer(90);RequireCanvas(selectionModel.hitTest(p)==HIT_MIDDLE_RIGHT,"rotated side handle is hit at high zoom");
    POINT expanded={p.x,p.y+160};SelectionMouse(WM_LBUTTONDOWN,p,MK_LBUTTON);SelectionMouse(WM_MOUSEMOVE,expanded,MK_LBUTTON);SelectionMouse(WM_LBUTTONUP,expanded);
    POINT after=RotateSelectionPoint({selectionModel.m_rc.left,(selectionModel.m_rc.top+selectionModel.m_rc.bottom)/2},selectionModel.m_rc,90);
    RequireCanvas(selectionModel.m_rc.Width()==80 && selectionModel.m_rc.Height()==40 && fixed.x==after.x && fixed.y==after.y,"resizing follows rotated axes and anchors the opposite edge");
    selectionModel.Landing();imageModel.Undo();RequireCanvas(DocumentRaster().pixels==original.pixels,"move and rotated resize share one undo operation");
    canvasWindow.zoomTo(1000);
    for(int interpolation:{0,1,2})
    {
        toolsModel.OnEndDraw(TRUE);imageModel.PushImageForUndo(MakeRasterBitmap(original));imageModel.ClearHistory();
        toolsModel.SetActiveTool(TOOL_FREESEL);HBITMAP mask=CreateMonoBitmap(60,40,FALSE);HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,mask);
        HGDIOBJ brush=SelectObject(dc,GetStockObject(WHITE_BRUSH));POINT triangle[]={{0,0},{59,20},{0,39}};Polygon(dc,triangle,3);SelectObject(dc,brush);SelectObject(dc,old);DeleteDC(dc);
        selectionModel.setMask(CRect(40,40,100,80),mask);selectionModel.m_bShow=TRUE;registrySettings.SelectionInterpolation=interpolation;
        selectionModel.SetTransparency(25);BeginSelectionRotation();p=SelectionRotationPointer(45);SelectionMouse(WM_MOUSEMOVE,p,MK_RBUTTON|MK_SHIFT);SelectionMouse(WM_RBUTTONUP,p,MK_SHIFT);
        RasterImage cut=SelectionRaster();int visible=0;for(DWORD pixel:cut.pixels) { RequireCanvas((pixel>>24)<=191,"rotation keeps per-selection opacity and freehand mask");if(pixel>>24) ++visible; }
        RequireCanvas(visible>500 && visible<int(cut.pixels.size())/2,"free selection silhouette stays masked when rotated");
        mainWindow.SendMessage(WM_COMMAND,IDM_CROPSELECTION,0);RequireCanvas(DocumentRaster().pixels==cut.pixels,"crop-to-selection exports rotated mask and alpha");
        imageModel.Undo();RequireCanvas(DocumentRaster().pixels==original.pixels,"Undo cropped rotated free selection");
    }
    for(COLORREF bg:{RGB(255,255,255),RGB(0,0,0),RGB(128,128,128),RGB(10,90,150)})
    {
        TestBitmap rectangle(80,50,bg),freehand(80,50,bg);DrawDashedRect(rectangle.dc,CRect(4,4,76,46));POINT points[]={{4,4},{20,4},{40,4},{76,4}};DrawDashedPolyline(freehand.dc,points,4,false);
        for(int x=4;x<76;++x) RequireCanvas(GetPixel(rectangle.dc,x,4)==GetPixel(freehand.dc,x,4),"freehand contour uses same contrast and 4/4 dash pattern as rectangle");
    }
    toolsModel.OnEndDraw(TRUE);toolsModel.SetActiveTool(TOOL_RECTSEL);registrySettings.SelectionInterpolation=0;
    puts("PASS selection rotation: right-border capture, Shift/status, cancel, source quality, masks/alpha, crop, Undo/Redo and consistent freehand dash");
    TestRPaintTranslationsAndHelp();
}
