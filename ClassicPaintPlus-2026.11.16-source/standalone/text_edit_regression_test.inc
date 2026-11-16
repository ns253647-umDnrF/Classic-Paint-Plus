static void PrepareTextRegression(int points=20)
{
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);SetActiveWindow(mainWindow);
    mainWindow.SetWindowPos(NULL,0,0,1200,900,SWP_NOACTIVATE|SWP_NOZORDER);
    toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();toolsModel.SetActiveTool(TOOL_TEXT);
    toolsModel.ResetObjectSize(true);textEditWindow.ResetRotation();textEditWindow.SetWarp({});
    registrySettings.PointSize=points;registrySettings.strFontName=L"Arial";registrySettings.TextAlignment=0;
    registrySettings.Bold=registrySettings.Italic=registrySettings.Underline=0;
    SetBitmapAndInfo(CreateColorDIB(1600,1200,RGB(255,255,255)),NULL,NULL,FALSE);canvasWindow.zoomTo(1000);
    if(!textEditWindow.IsWindow()) textEditWindow.Create(canvasWindow);
    textEditWindow.UpdateFont();toolsModel.SetBackgroundTransparent(TRUE);
}
static CRect NewRegressionText(int width=300,int height=70)
{
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,80,90);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,80+width,90+height);FigureMouse(WM_LBUTTONUP,0,80+width,90+height);
    RequireCanvas(textEditWindow.IsWindowVisible(),"new text frame is active");CRect r;textEditWindow.GetEditRect(&r);return r;
}
static void TestTextAnglePresets()
{
    PrepareTextRegression();HWND save=toolSettingsWindow.GetDlgItem(IDC_SAVE_OBJECT_SIZE),reset=toolSettingsWindow.GetDlgItem(IDC_RESET_OBJECT_SIZE);
    textEditWindow.SetRotationAngle(37);NewRegressionText();RequireCanvas(textEditWindow.Angle()==37,"prepared angle applies to first text frame");
    textEditWindow.SendMessage(WM_CHAR,L'A',1);toolsModel.OnEndDraw(FALSE);NewRegressionText();
    RequireCanvas(textEditWindow.Angle()==0,"unsaved angle is not inherited by the next text frame");
    textEditWindow.SetRotationAngle(65.25);SendMessage(save,BM_CLICK,0,0);textEditWindow.SetRotationAngle(103);toolsModel.OnEndDraw(TRUE);
    NewRegressionText();RequireCanvas(textEditWindow.Angle()==65.25,"S saves the exact angle snapshot, later rotations do not replace it");
    toolsModel.OnEndDraw(TRUE);SendMessage(reset,BM_CLICK,0,0);RequireCanvas(textEditWindow.Angle()==0,"R resets pending and saved text angle");
    NewRegressionText();RequireCanvas(textEditWindow.Angle()==0,"reset angle remains zero on creation");toolsModel.OnEndDraw(TRUE);
    textEditWindow.SetRotationAngle(-15);SendMessage(save,BM_CLICK,0,0);NewRegressionText();RequireCanvas(textEditWindow.Angle()==345,"S saves prepared angle before creating text");
    textEditWindow.SetRotationAngle(11);toolsModel.OnEndDraw(FALSE);NewRegressionText();RequireCanvas(textEditWindow.Angle()==345,"unsaved active angle never replaces S preset even after commit");
    DWORD remember=registrySettings.RememberObjectSizes;
    registrySettings.RememberObjectSizes=0;registrySettings.StorePreferences();RegistrySettings loaded;loaded.Load(SW_HIDE);
    RequireCanvas(loaded.SavedTextAngle==0 && registrySettings.SavedTextAngle==345,"disabled persistence keeps S angle in session but not across launches");
    registrySettings.RememberObjectSizes=1;registrySettings.StorePreferences();loaded.Load(SW_HIDE);
    RequireCanvas(loaded.SavedTextAngle==345,"saved angle persists with remembered text dimensions");
    for(LPCWSTR invalid:{L"NaN",L"Inf",L"-1",L"360",L"bad",L"12oops"})
    {
        WritePrivateProfileStringW(L"ObjectSizes",L"TextAngle",invalid,RegistrySettings::ConfigPath());loaded.Load(SW_HIDE);
        RequireCanvas(loaded.SavedTextAngle==0,"invalid stored angles use zero");
    }
    registrySettings.RememberObjectSizes=remember;
    toolsModel.OnEndDraw(TRUE);toolsModel.ResetObjectSize(true);
    puts("PASS text angle: single-use prepared angle, S snapshots before/after creation, later edits, commit/cancel and R reset");
}
static void TestTextInitialHeight()
{
    for(int points:{12,48,96}) for(int zoom:{500,1000,2000})
    {
        PrepareTextRegression(points);canvasWindow.zoomTo(zoom);textEditWindow.SetRotationAngle(33);
        CRect before=NewRegressionText(400,1);HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,textEditWindow.GetFont());TEXTMETRIC tm={};GetTextMetrics(dc,&tm);SelectObject(dc,old);DeleteDC(dc);
        RequireCanvas(before.Height()>=tm.tmHeight+tm.tmDescent+GRIP_SIZE,"empty text frame fits the selected font immediately at every zoom");
        textEditWindow.SendMessage(WM_CHAR,L'A',1);CRect after;textEditWindow.GetEditRect(&after);
        RequireCanvas(before==after,"first character does not resize or move an already fitted rotated frame");toolsModel.OnEndDraw(TRUE);
    }
    PrepareTextRegression(72);toolsModel.ObjectSizePreset(true)={300,18};SetParameterFixed(PARAM_HEIGHT,true);toolsModel.SaveObjectSize(true,300,18);CRect before=NewRegressionText(40,1);
    RequireCanvas(before.Width()==300 && before.Height()==18,"fixed saved height is honoured even for a larger font");
    textEditWindow.SendMessage(WM_CHAR,L'A',1);CRect after;textEditWindow.GetEditRect(&after);RequireCanvas(after==before,"typing respects explicit height");toolsModel.OnEndDraw(TRUE);
    toolsModel.ResetObjectSize(true);toolsModel.SaveObjectSize(true,300,0);before=NewRegressionText(40,1);RequireCanvas(before.Height()>18,"width-only preset still fits font height automatically");
    registrySettings.PointSize=110;textEditWindow.UpdateFont();textEditWindow.GetEditRect(&after);RequireCanvas(after.Height()>before.Height(),"changing font in an unsized empty frame immediately grows its height");
    toolsModel.OnEndDraw(TRUE);toolsModel.ResetObjectSize(true);
    puts("PASS text initial height: 12/48/96pt at three zooms, rotated empty frames, stable first glyph, fixed and width-only presets, live font change");
}
static void CheckTextNativeCaret()
{
    GUITHREADINFO gui={sizeof(gui)};RequireCanvas(GetGUIThreadInfo(GetCurrentThreadId(),&gui)!=FALSE,"query actual GUI caret state");
    RequireCanvas(gui.hwndFocus==textEditWindow.m_hWnd,"text keeps keyboard focus");
    RequireCanvas(gui.hwndCaret!=textEditWindow.m_hWnd || !(gui.flags&GUI_CARETBLINKING),"native upright caret stays hidden while custom caret is drawn");
}
static void TestTextCaretRendering()
{
    PrepareTextRegression(28);
    for(int angle:{0,33,90,315})
    {
        textEditWindow.SetRotationAngle(angle);NewRegressionText();textEditWindow.SetFocus();CheckTextNativeCaret();
        for(int i=0;i<16;++i)
        {
            textEditWindow.SendMessage(WM_CHAR,L'A'+i%20,1);CheckTextNativeCaret();
            textEditWindow.SendMessage(WM_KEYDOWN,VK_LEFT,1);CheckTextNativeCaret();
            textEditWindow.SendMessage(EM_SETSEL,-1,-1);CheckTextNativeCaret();
            textEditWindow.SendMessage(EM_REPLACESEL,TRUE,LPARAM(L"z"));CheckTextNativeCaret();
            if(i%4==0) { canvasWindow.SetFocus();textEditWindow.SetFocus();textEditWindow.UpdateFont();CheckTextNativeCaret(); }
        }
        for(int zoom:{500,1000,2000,1000}) { canvasWindow.zoomTo(zoom);CheckTextNativeCaret(); }
        textEditWindow.SetDimensions(330,90);CheckTextNativeCaret();
        for(int n=0;n<60;++n)
        {
            MSG msg;while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)) { TranslateMessage(&msg);DispatchMessageW(&msg); }
            CheckTextNativeCaret();Sleep(10);
        }
        textEditWindow.SendMessage(EM_SETSEL,0,0);textEditWindow.InvalidateEditRect();
        TestBitmap on(1000,700,RGB(255,255,255)),off(1000,700,RGB(255,255,255));
        // Keep rotated corners in the capture even when they extend above the canvas.
        SetViewportOrgEx(on.dc,200,200,NULL);SetViewportOrgEx(off.dc,200,200,NULL);
        textEditWindow.DrawRotatedFrame(on.dc);textEditWindow.SendMessage(WM_TIMER,0x5250,0);textEditWindow.DrawRotatedFrame(off.dc);
        GdiFlush();
        RasterImage a,b;ReadRaster(on.bitmap,a);ReadRaster(off.bitmap,b);int count=0,left=1000,right=0,top=700,bottom=0;
        for(int y=0;y<700;++y) for(int x=0;x<1000;++x) if(a.pixels[y*1000+x]!=b.pixels[y*1000+x]) { ++count;left=min(left,x);right=max(right,x);top=min(top,y);bottom=max(bottom,y); }
        if(count<=5) { CRect r;textEditWindow.GetEditRect(&r);canvasWindow.ImageToCanvas(r);POINT p={};textEditWindow.SendMessage(EM_POSFROMCHAR,WPARAM(&p),0);printf("Caret pixels=%d angle=%d screen=(%ld,%ld,%ld,%ld) local=(%ld,%ld) focus=%d\n",count,angle,r.left,r.top,r.right,r.bottom,p.x,p.y,GetFocus()==textEditWindow.m_hWnd); }
        RequireCanvas(count>5,"custom caret still blinks after native caret suppression");
        if(angle==33 || angle==315) RequireCanvas(right-left>5 && bottom-top>5,"the only drawn caret follows text rotation");
        if(angle==90) RequireCanvas(right-left>5 && bottom-top<=1,"90 degree text caret is horizontal");
        toolsModel.OnEndDraw(TRUE);
    }
    HWND edit=toolSettingsWindow.GetDlgItem(IDC_OBJECT_WIDTH);SetFocus(edit);SendMessage(edit,WM_CHAR,L'3',1);
    GUITHREADINFO gui={sizeof(gui)};GetGUIThreadInfo(GetCurrentThreadId(),&gui);
    RequireCanvas(gui.hwndCaret==edit && (gui.flags&GUI_CARETBLINKING),"numeric edit retains its ordinary native caret");canvasWindow.SetFocus();
    toolsModel.ResetObjectSize(true);textEditWindow.ResetRotation();InitializeImage(NULL,NULL,FALSE);toolsModel.SetActiveTool(TOOL_RECTSEL);
    puts("PASS text caret: native GUI visibility, typing/navigation/replace, focus/font/zoom/resize, timer pumping, rotated custom blink and unaffected numeric caret");
}
