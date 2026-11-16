static POINT ObjectCanvasPoint(const RECT& r,double angle,int handle)
{
    const int xs[]={0,1,2,0,2,0,1,2},ys[]={0,0,0,1,1,2,2,2};
    CRect screen=r;canvasWindow.ImageToCanvas(screen);int i=handle-HIT_UPPER_LEFT;
    POINT p={screen.left+(screen.right-screen.left)*xs[i]/2,screen.top+(screen.bottom-screen.top)*ys[i]/2};
    return EditFramePoint(p,screen,angle);
}
static void TextCaptureMouse(UINT message,WPARAM keys,POINT canvasPoint)
{
    canvasWindow.ClientToScreen(&canvasPoint);textEditWindow.ScreenToClient(&canvasPoint);
    textEditWindow.SendMessage(message,keys,MAKELPARAM(canvasPoint.x,canvasPoint.y));
}
static void CanvasObjectMouse(UINT message,WPARAM keys,POINT point)
{ canvasWindow.SendMessage(message,keys,MAKELPARAM(point.x,point.y)); }
static void EnterObjectValue(HWND edit,LPCWSTR value)
{ SetFocus(edit);SetWindowTextW(edit,value);SendMessageW(edit,WM_KEYDOWN,VK_RETURN,0); }
static void TestEditableObjects()
{
    CreateDirectoryW(L"editable-object-tests",NULL);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    mainWindow.SetWindowPos(NULL,0,0,1080,850,SWP_NOACTIVATE|SWP_NOZORDER);
    toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;
    ApplyTheme();ApplyPanelScale();canvasWindow.zoomTo(1000);SetTestShift(false);
    imageModel.PushImageForUndo(CreateColorDIB(520,400,RGB(255,255,255)));imageModel.ClearHistory();
    canvasWindow.updateScrollRange();canvasWindow.updateScrollPos();
    paletteModel.SetFgColor(RGB(30,80,160));paletteModel.SetBgColor(RGB(255,255,255));toolsModel.SetBackgroundTransparent(TRUE);
    toolsModel.SetActiveTool(TOOL_TEXT);
    HWND angle=toolSettingsWindow.GetDlgItem(IDC_SELECTION_ANGLE),width=toolSettingsWindow.GetDlgItem(IDC_LINE_WIDTH);
    RequireCanvas(IsWindowVisible(angle) && IsWindowEnabled(angle) && !IsWindowVisible(textEditWindow),"text angle is editable before creating a text box");
    EnterObjectValue(angle,L"-30,5");RequireCanvas(textEditWindow.Angle()==329.5,"preconfigured text angle accepts signed decimal values");
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,120,110);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,340,250);FigureMouse(WM_LBUTTONUP,0,340,250);
    RequireCanvas(IsWindowVisible(textEditWindow) && textEditWindow.Angle()==329.5,"new text box uses the configured angle");
    registrySettings.strFontName=L"Arial";registrySettings.PointSize=18;registrySettings.TextAlignment=0;textEditWindow.UpdateFont();
    textEditWindow.SetWindowTextW(L"RPaint+\r\nEditable text");
    HRGN region=CreateRectRgn(0,0,0,0);RequireCanvas(GetWindowRgn(textEditWindow,region)==NULLREGION,"RichEdit has no visible surface to erase or flash over canvas");DeleteObject(region);
    TestBitmap untouched(80,40,RGB(210,0,160));textEditWindow.SendMessage(WM_PRINTCLIENT,WPARAM(untouched.dc),PRF_CLIENT|PRF_ERASEBKGND);
    RequireCanvas(GetPixel(untouched.dc,10,10)==RGB(210,0,160),"native editor never paints a second text layer");
    for(int zoom:{500,1000,2000}) for(double rotation:{0.0,37.0,90.0,227.0})
    {
        canvasWindow.zoomTo(zoom);textEditWindow.SetRotationAngle(rotation);
        for(int hit=HIT_UPPER_LEFT;hit<=HIT_LOWER_RIGHT;++hit)
        {
            CRect start(120,110,340,250);textEditWindow.ValidateEditRect(&start);
            POINT p=ObjectCanvasPoint(start,rotation,hit);
            RequireCanvas(HitEditFrame(start,rotation,p)==hit,"all text handles hit correctly after rotation and zoom");
            CanvasObjectMouse(WM_LBUTTONDOWN,MK_LBUTTON,p);
            RequireCanvas(GetCapture()==textEditWindow.m_hWnd,"rotated text resize captures mouse");
            bool left=hit==1 || hit==4 || hit==6,right=hit==3 || hit==5 || hit==8,top=hit<=3,bottom=hit>=6;
            double dx=left?-20:right?20:0,dy=top?-16:bottom?16:0,a=rotation*M_PI/180;
            POINT end={p.x+Zoomed(LONG(lround(dx*cos(a)-dy*sin(a)))),p.y+Zoomed(LONG(lround(dx*sin(a)+dy*cos(a))))};
            TextCaptureMouse(WM_MOUSEMOVE,MK_LBUTTON,end);TextCaptureMouse(WM_LBUTTONUP,0,end);
            CRect changed;textEditWindow.GetEditRect(&changed);
            RequireCanvas(abs(changed.Width()-(220+int(abs(dx))))<=2 && abs(changed.Height()-(140+int(abs(dy))))<=2,"rotated text resize changes local dimensions without changing angle");
            RequireCanvas(textEditWindow.Angle()==rotation && WindowLabel(textEditWindow)==L"RPaint+\r\nEditable text","resize preserves text content and angle");
        }
    }
    canvasWindow.zoomTo(1000);textEditWindow.SetRotationAngle(30);CRect start(120,110,340,250);textEditWindow.ValidateEditRect(&start);
    POINT p=ObjectCanvasPoint(start,30,HIT_LOWER_RIGHT),end={p.x+50,p.y+35};CanvasObjectMouse(WM_LBUTTONDOWN,MK_LBUTTON,p);TextCaptureMouse(WM_MOUSEMOVE,MK_LBUTTON,end);
    textEditWindow.SendMessage(WM_KEYDOWN,VK_ESCAPE,0);CRect restored;textEditWindow.GetEditRect(&restored);RequireCanvas(restored==start && IsWindowVisible(textEditWindow),"Escape cancels only text resize, preserving active text");
    CanvasObjectMouse(WM_LBUTTONDOWN,MK_LBUTTON,p);TextCaptureMouse(WM_MOUSEMOVE,MK_LBUTTON,end);SetCapture(mainWindow);ReleaseCapture();textEditWindow.GetEditRect(&restored);
    RequireCanvas(restored==start,"capture loss restores original text bounds");
    textEditWindow.SetRotationAngle(0);toolSettingsWindow.ParameterCheckChanged(PARAM_ANGLE,false);p=ObjectCanvasPoint(start,0,HIT_MIDDLE_RIGHT);CanvasObjectMouse(WM_RBUTTONDOWN,MK_RBUTTON,p);
    CRect screen=start;canvasWindow.ImageToCanvas(screen);double a=31*M_PI/180;
    end={LONG((screen.left+screen.right)/2.0+110*cos(a)),LONG((screen.top+screen.bottom)/2.0+110*sin(a))};
    SetTestShift(true);TextCaptureMouse(WM_MOUSEMOVE,MK_RBUTTON|MK_SHIFT,end);
    RequireCanvas(textEditWindow.Angle()==30 && wcstod(WindowLabel(angle),NULL)==30,"text mouse rotation and Shift synchronize numeric angle");
    TextCaptureMouse(WM_RBUTTONUP,MK_SHIFT,end);SetTestShift(false);
    textEditWindow.SendMessage(WM_CHAR,L'!',1);RequireCanvas(WindowLabel(textEditWindow).Find(L'!')>=0,"typing works after rotated resize");
    EnterObjectValue(angle,L"45");RequireCanvas(GetFocus()==textEditWindow.m_hWnd && textEditWindow.Angle()==45,"Enter in text angle returns keyboard focus to editable text");
    CaptureCanvasDialog(mainWindow,L"editable-object-tests/text-rotated.bmp");
    TestBitmap textPreview(520,400,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(textPreview.dc);toolsModel.OnEndDraw(FALSE);
    RequireCanvas(FigurePixels(textPreview.dc)==FigurePixels(imageModel.GetDC()),"text preview commits once without doubled or missing text");imageModel.Undo();imageModel.ClearHistory();
    textEditWindow.ResetRotation();
    puts("PASS text objects: single buffered rendering surface, preset angle, all eight rotated grips at 3 zooms, cancel, Shift, typing and commit");

    toolsModel.SetActiveTool(TOOL_FIGURE);registrySettings.FigureKind=FIG_RECT;registrySettings.ShapeFromCenter=FALSE;toolsModel.SetShapeStyle(0);
    RequireCanvas(IsWindowVisible(angle) && IsWindowEnabled(angle) && IsWindowVisible(width),"figure angle and width enabled before drawing");
    EnterObjectValue(angle,L"25");EnterObjectValue(width,L"3");auto blank=FigurePixels(imageModel.GetDC());
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,140,130);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,300,230);FigureMouse(WM_LBUTTONUP,0,300,230);
    RECT figure;RequireCanvas(toolsModel.EditableFigureBounds(figure) && toolsModel.FigureAngle()==25 && !canvasWindow.m_drawing,"figure remains editable with preset angle after button release");
    RequireCanvas(FigurePixels(imageModel.GetDC())==blank && !imageModel.CanUndo() && mainWindow.CanUndo(),"editable vector preview leaves document pixels and history untouched");
    CRect frame=figure;--frame.right;--frame.bottom;p=ObjectCanvasPoint(frame,25,HIT_LOWER_RIGHT);a=25*M_PI/180;
    end={p.x+LONG(lround(60*cos(a)-30*sin(a))),p.y+LONG(lround(60*sin(a)+30*cos(a)))};
    CanvasObjectMouse(WM_LBUTTONDOWN,MK_LBUTTON,p);CanvasObjectMouse(WM_MOUSEMOVE,MK_LBUTTON,end);CanvasObjectMouse(WM_LBUTTONUP,0,end);
    toolsModel.EditableFigureBounds(figure);RequireCanvas(figure.right-figure.left==220 && figure.bottom-figure.top==130 && toolsModel.GetLineWidth()==3,"figure resize changes geometry and retains line thickness");
    EnterObjectValue(angle,L"0");EnterObjectValue(width,L"7");paletteModel.SetFgColor(RGB(230,30,70));
    TestBitmap vectorPreview(520,400,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(vectorPreview.dc);
    int cx=(figure.left+figure.right)/2,count=0;for(int y=figure.top-12;y<figure.top+12;++y) if(GetPixel(vectorPreview.dc,cx,y)==RGB(230,30,70)) ++count;
    RequireCanvas(count==7 && GetPixel(vectorPreview.dc,cx,(figure.top+figure.bottom)/2)==RGB(255,255,255),"live width/color changes redraw a seven-pixel outline with untouched interior");
    SetFocus(width);SetWindowTextW(width,L"0");SendMessageW(width,WM_KEYDOWN,VK_RETURN,0);RequireCanvas(toolsModel.GetLineWidth()==7 && GetFocus()==width,"invalid thickness does not alter object");SendMessageW(width,WM_KEYDOWN,VK_ESCAPE,0);
    EnterObjectValue(angle,L"35");frame=figure;--frame.right;--frame.bottom;p=ObjectCanvasPoint(frame,35,HIT_LOWER_RIGHT);end={p.x+45,p.y+20};
    CanvasObjectMouse(WM_LBUTTONDOWN,MK_LBUTTON,p);CanvasObjectMouse(WM_MOUSEMOVE,MK_LBUTTON,end);canvasWindow.SendMessage(WM_KEYDOWN,VK_ESCAPE,0);
    RECT after;toolsModel.EditableFigureBounds(after);RequireCanvas(EqualRect(&after,&figure) && toolsModel.HasEditableFigure(),"Escape during figure resize restores geometry and keeps frame");
    toolSettingsWindow.ParameterCheckChanged(PARAM_ANGLE,false);p=ObjectCanvasPoint(frame,35,HIT_MIDDLE_RIGHT);CanvasObjectMouse(WM_RBUTTONDOWN,MK_RBUTTON,p);CRect cr=frame;canvasWindow.ImageToCanvas(cr);
    end={LONG((cr.left+cr.right)/2.0),cr.bottom+70};SetTestShift(true);CanvasObjectMouse(WM_MOUSEMOVE,MK_RBUTTON|MK_SHIFT,end);
    RequireCanvas(fabs(fmod(toolsModel.FigureAngle(),15))<.001 && wcstod(WindowLabel(angle),NULL)==toolsModel.FigureAngle(),"editable figure rotation uses Shift and live angle field");
    CanvasObjectMouse(WM_RBUTTONUP,MK_SHIFT,end);SetTestShift(false);
    for(int percent:{100,150,200,250})
    {
        registrySettings.PanelMagnification=percent;ApplyPanelScale();RECT panel,aRect,wRect,origin;GetClientRect(toolSettingsWindow,&panel);
        RequireCanvas(toolsModel.HasEditableFigure(),"panel scale changes preserve the editable figure");
        GetWindowRect(angle,&aRect);RECT bottom;GetWindowRect(ParameterBarWindow(),&bottom);
        GetWindowRect(width,&wRect);MapWindowPoints(NULL,toolSettingsWindow,reinterpret_cast<POINT*>(&wRect),2);
        GetWindowRect(GetDlgItem(toolSettingsWindow,IDC_FIGURE_ORIGIN),&origin);MapWindowPoints(NULL,toolSettingsWindow,reinterpret_cast<POINT*>(&origin),2);
        RequireCanvas(aRect.top>=bottom.top && aRect.bottom<=bottom.bottom && wRect.bottom<=origin.top,"width/origin remain on left and angle fits bottom row");
    }
    registrySettings.PanelMagnification=100;ApplyPanelScale();CaptureCanvasDialog(mainWindow,L"editable-object-tests/figure-editing.bmp");
    TestBitmap finalFigure(520,400,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(finalFigure.dc);
    canvasWindow.SendMessage(WM_KEYDOWN,VK_RETURN,0);
    RequireCanvas(!toolsModel.HasEditableFigure() && FigurePixels(imageModel.GetDC())==FigurePixels(finalFigure.dc),"Enter commits exactly the final vector preview");
    imageModel.Undo();RequireCanvas(FigurePixels(imageModel.GetDC())==blank && !imageModel.CanUndo(),"all figure edits undo as one complete operation");
    imageModel.Redo();RequireCanvas(FigurePixels(imageModel.GetDC())==FigurePixels(finalFigure.dc),"redo restores final edited figure");imageModel.Undo();imageModel.ClearHistory();
    toolsModel.SetFigureAngle(0);FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,50,50);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,170,130);FigureMouse(WM_LBUTTONUP,0,170,130);
    canvasWindow.SendMessage(WM_KEYDOWN,VK_ESCAPE,0);RequireCanvas(!toolsModel.HasEditableFigure() && !imageModel.CanUndo() && FigurePixels(imageModel.GetDC())==blank,"Escape outside a gesture cancels the entire uncommitted figure");
    EnterObjectValue(width,L"500");RequireCanvas(toolsModel.GetLineWidth()==500,"numeric thickness supports the upper limit");toolsModel.SetLineWidth(1);toolsModel.SetFigureAngle(0);
    toolsModel.SetActiveTool(TOOL_RECTSEL);registrySettings.PanelMagnification=100;ApplyPanelScale();
    puts("PASS vector objects: preset angle, editable frame, constant-width resize, live style/color/width, Shift, cancel, all panel scales, one-step undo/redo");
}
