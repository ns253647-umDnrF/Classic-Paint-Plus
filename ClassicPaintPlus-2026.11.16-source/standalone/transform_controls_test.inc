static bool popupSelectionPresent=false,popupSeen=false;
static VOID CALLBACK DismissSelectionPopup(HWND,UINT,UINT_PTR id,DWORD)
{
    popupSeen=FindWindowW(L"#32768",NULL)!=NULL;
    popupSelectionPresent=selectionModel.m_bShow && !canvasWindow.m_drawing;
    KillTimer(NULL,id);EndMenu();
}
static void TestSelectionPopupPreservation()
{
    VectorReset();selectionModel.CombineSelection(CRect(60,50,220,180),TriangleMask(160,130),0);
    selectionModel.SetTransparency(35);selectionModel.SetSkew(.2,-.1);
    auto before=SelectionRaster(),document=DocumentRaster();CRect bounds=selectionModel.m_rc;
    auto mapped=TransformPoint(90,80,bounds,0,.2,-.1);POINT inside=SelectionCanvasPoint(LONG(mapped.x),LONG(mapped.y));
    popupSeen=popupSelectionPresent=false;SetTimer(NULL,0,150,DismissSelectionPopup);
    SelectionMouse(WM_RBUTTONDOWN,inside,MK_RBUTTON);SelectionMouse(WM_RBUTTONUP,inside);
    RequireCanvas(popupSeen && popupSelectionPresent,"real right-click popup opens without entering or cancelling a drawing gesture");
    RequireCanvas(selectionModel.m_bShow && selectionModel.m_rc==bounds && SelectionRaster().pixels==before.pixels && DocumentRaster().pixels==document.pixels,"popup dismissal preserves mask, alpha, skew and underlying pixels");
    puts("PASS right-click selection popup: native menu, capture/cancel path, unchanged floating selection");
}
static void TestExactSkewFields()
{
    VectorReset();HWND x=toolSettingsWindow.GetDlgItem(IDC_SKEW_HORIZONTAL),y=toolSettingsWindow.GetDlgItem(IDC_SKEW_VERTICAL);
    RequireCanvas(IsWindowVisible(x) && IsWindowVisible(y) && !IsWindowEnabled(x),"selection skew fields visible and disabled without a selection");
    selectionModel.CombineSelection(CRect(70,60,210,170),NULL,0);
    EnterObjectValue(x,L"22,5");EnterObjectValue(y,L"-12.75");
    RequireCanvas(fabs(selectionModel.SkewX()-tan(22.5*M_PI/180))<1e-12 && fabs(selectionModel.SkewY()-tan(-12.75*M_PI/180))<1e-12,"exact signed horizontal and vertical degree input changes selection geometry");
    SetFocus(x);RequireCanvas(WindowLabel(x)==L"22.5","skew degree suffix disappears while editing");
    SendMessage(x,EM_SETSEL,0,1);SendMessage(x,WM_KEYDOWN,VK_DELETE,0);RequireCanvas(WindowLabel(x)==L"2.5","Delete works in new degree fields");
    SendMessage(x,WM_KEYDOWN,VK_ESCAPE,0);RequireCanvas(WindowLabel(x)==L"22.5\u00b0","Escape restores angle and degree suffix");
    for(LPCWSTR invalid:{L"90",L"nan",L"infinity",L"999"})
    { EnterObjectValue(x,invalid);RequireCanvas(GetFocus()==x,"invalid shear is rejected without modifying geometry");SendMessage(x,WM_KEYDOWN,VK_ESCAPE,0); }
    selectionModel.SetSkew(.123456789,-.23456789);SetFocus(x);canvasWindow.SetFocus();SetFocus(y);canvasWindow.SetFocus();
    RequireCanvas(fabs(selectionModel.SkewX()-.123456789)<1e-12 && fabs(selectionModel.SkewY()+.23456789)<1e-12,"focus-only changes never round a mouse-derived shear");
    selectionModel.HideSelection();toolsModel.SetActiveTool(TOOL_FIGURE);toolsModel.ResetObjectSize(false);toolsModel.SetFigureAngle(0);
    RequireCanvas(IsWindowEnabled(x) && IsWindowEnabled(y),"figure skew can be prepared before drawing");
    EnterObjectValue(x,L"15");EnterObjectValue(y,L"-20");
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,100,80);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,250,190);FigureMouse(WM_LBUTTONUP,0,250,190);
    FigureClipboardData d={};RequireCanvas(toolsModel.ExportFigure(d) && fabs(d.skewX-tan(M_PI/12))<1e-12 && fabs(d.skewY-tan(-M_PI/9))<1e-12,"new editable figure applies both prepared skew angles");
    toolsModel.SetLineWidth(5);EnterObjectValue(y,L"7.25");
    RequireCanvas(toolsModel.ExportFigure(d) && fabs(d.skewY-tan(7.25*M_PI/180))<1e-12 && d.lineWidth==5,"numeric figure skew immediately updates geometry without changing stroke width");
    puts("PASS exact skew fields: empty selection, live figure, prepared values, suffixes, Delete, Escape, range and precision");
}
static void TestSeparateTransformHandles()
{
    VectorReset();selectionModel.CombineSelection(CRect(90,80,250,200),NULL,0);VectorClick(SelectionCanvasPoint(170,140));
    CRect frame=selectionModel.m_rc;canvasWindow.ImageToCanvas(frame);
    POINT dot=TransformHandle(frame,HIT_LOWER_RIGHT,0,0,0),arrow=TransformArrowHandle(frame,HIT_LOWER_RIGHT,0,0,0);
    RequireCanvas(HitTransformArrow(selectionModel.m_rc,0,0,0,dot)==HIT_NONE && selectionModel.hitTest(arrow)==HIT_LOWER_RIGHT,"resize dot and external arrow have distinct hit targets");
    selectionModel.SetFrameCursor(dot);HCURSOR resize=GetCursor();selectionModel.SetFrameCursor(arrow);HCURSOR rotate=GetCursor();
    RequireCanvas(resize!=rotate && rotate==TransformFrameCursor(HIT_LOWER_RIGHT,0,0,0,true),"selection corner cursor matches external rotation arrow");
    VectorDrag(dot,{dot.x+20,dot.y+10});RequireCanvas(selectionModel.m_rc.Width()==180 && selectionModel.m_rc.Height()==130 && !selectionModel.RotationAngle(),"frame dots still resize in rotation mode");
    frame=selectionModel.m_rc;canvasWindow.ImageToCanvas(frame);arrow=TransformArrowHandle(frame,HIT_UPPER_CENTER,0,0,0);
    VectorDrag(arrow,{arrow.x+25,arrow.y});RequireCanvas(fabs(selectionModel.SkewX())>.3,"external side arrow skews selection");
    CStringW expected;expected.Format(L"%.2f",atan(selectionModel.SkewX())*180/M_PI);expected.TrimRight(L'0');expected.TrimRight(L'.');expected+=L"\u00b0";
    RequireCanvas(WindowLabel(toolSettingsWindow.GetDlgItem(IDC_SKEW_HORIZONTAL))==expected,"mouse skew synchronizes the precise angle field");
    for(int grip:{3,15}) for(int zoom:{500,1000,2000})
    {
        registrySettings.CanvasMarkerSize=grip;canvasWindow.zoomTo(zoom);
        CRect bounds(100,100,500,400),screen=bounds;canvasWindow.ImageToCanvas(screen);
        for(double angle:{0.,35.,90.}) for(int hit=HIT_UPPER_LEFT;hit<=HIT_LOWER_RIGHT;++hit)
        {
            dot=TransformHandle(screen,hit,angle,.3,-.2);arrow=TransformArrowHandle(screen,hit,angle,.3,-.2);
            RequireCanvas(max(abs(dot.x-arrow.x),abs(dot.y-arrow.y))>grip/2+10,"external arrow cannot cover its frame dot at any zoom or marker size");
            RequireCanvas(HitTransformFrame(bounds,angle,.3,-.2,dot)==hit && HitTransformArrow(bounds,angle,.3,-.2,arrow)==hit,"rotated and skewed dot/arrow targets match rendering");
        }
    }
    registrySettings.CanvasMarkerSize=3;canvasWindow.zoomTo(1000);
    VectorReset();toolsModel.SetActiveTool(TOOL_FIGURE);toolsModel.SetFigureAngle(0);toolsModel.SetFigureSkew(0,0);toolsModel.ResetObjectSize(false);
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,100,80);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,270,210);FigureMouse(WM_LBUTTONUP,0,270,210);
    VectorClick(SelectionCanvasPoint(180,140));CRect before;toolsModel.EditableFigureBounds(before);frame=before;--frame.right;--frame.bottom;canvasWindow.ImageToCanvas(frame);
    dot=TransformHandle(frame,HIT_LOWER_RIGHT,0,0,0);arrow=TransformArrowHandle(frame,HIT_LOWER_RIGHT,0,0,0);
    toolsModel.SetFigureCursor(dot);resize=GetCursor();toolsModel.SetFigureCursor(arrow);rotate=GetCursor();RequireCanvas(resize!=rotate,"figure cursor also distinguishes corner dot from rotation arrow");
    VectorDrag(dot,{dot.x+20,dot.y+10});CRect after;toolsModel.EditableFigureBounds(after);
    RequireCanvas(after.Width()==before.Width()+20 && after.Height()==before.Height()+10 && !toolsModel.FigureAngle(),"figure dot resizes in rotation mode");
    CaptureCanvasDialog(mainWindow,L"transform-controls-tests/figure-handles.bmp");
    puts("PASS separate frame dots/arrows, gesture modes, exact mouse feedback and matching cursors across zoom/marker sizes");
}
static void TestTransformPanelLayout()
{
    HWND toolbar=FindWindowExW(toolBoxContainer,NULL,TOOLBARCLASSNAMEW,NULL);
    HWND gallery=FindWindowExW(toolBoxContainer,NULL,L"PaintFigureControls",NULL);
    RequireCanvas(toolbar && gallery,"toolbar and gallery windows exist");
    RequireCanvas(SendMessage(toolbar,TB_BUTTONCOUNT,0,0)==10 && SendMessage(toolbar,TB_COMMANDTOINDEX,ID_RRECT,0)==-1,"standalone rounded-rectangle button is removed");
    for(int percent:{100,150,200,250})for(int dark:{0,1})
    {
        registrySettings.PanelMagnification=percent;registrySettings.DarkTheme=dark;ApplyTheme();ApplyPanelScale();
        RECT ellipse,button;GetWindowRect(GetDlgItem(gallery,IDC_SHAPE_GROUP_FIRST+GROUP_ELLIPSE),&ellipse);GetWindowRect(GetDlgItem(gallery,IDC_SHAPE_GROUP_FIRST+GROUP_STARS),&button);
        RequireCanvas(button.left==ellipse.right && button.top==ellipse.top && button.bottom==ellipse.bottom,"star group occupies slot beside ellipse");
        toolsModel.OnEndDraw(TRUE);toolsModel.SetActiveTool(TOOL_PEN);
        HWND galleryButton=GetDlgItem(gallery,IDC_FIGURE_GALLERY);
        SendMessage(galleryButton,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(12,12));
        SendMessage(galleryButton,WM_LBUTTONUP,0,MAKELPARAM(12,12));
        RequireCanvas(toolsModel.GetActiveTool()==TOOL_FIGURE,"short click on relocated button activates the selected figure");
        HoldFigureButton(gallery);HWND popup=FindWindowW(L"PaintFigurePopup",NULL);
        RequireCanvas(popup && IsWindowVisible(popup),"holding relocated figure button still opens the gallery");
        SendDlgItemMessage(popup,IDC_FIGURE_FIRST+FIG_RECT,BM_CLICK,0,0);
        RequireCanvas(registrySettings.FigureKind==FIG_RECT && !IsWindowVisible(popup),"rectangle is selectable from its polygon group");
        for(TOOLTYPE tool:{TOOL_RECTSEL,TOOL_FIGURE,TOOL_TEXT,TOOL_BRUSH})
        {
            toolsModel.SetActiveTool(tool);
            HWND angle=toolSettingsWindow.GetDlgItem(IDC_SELECTION_ANGLE),x=toolSettingsWindow.GetDlgItem(IDC_SKEW_HORIZONTAL),y=toolSettingsWindow.GetDlgItem(IDC_SKEW_VERTICAL),separator=toolSettingsWindow.GetDlgItem(IDC_TRANSFORM_SEPARATOR);
            RequireCanvas(bool(IsWindowVisible(x))==(tool==TOOL_RECTSEL || tool==TOOL_FIGURE),"skew fields hide for text and other tools");
            if(tool==TOOL_BRUSH) { RequireCanvas(!IsWindowVisible(separator),"separator hides for non-transform tools");continue; }
            RECT a,p;GetWindowRect(angle,&a);GetWindowRect(ParameterBarWindow(),&p);
            RequireCanvas(a.top>=p.top && a.bottom<=p.bottom,"angle is on the bottom bar");
            for(HWND field:{x,y})if(IsWindowVisible(field))
            { RECT r;GetWindowRect(field,&r);RECT overlap;RequireCanvas(r.top>=p.top && r.bottom<=p.bottom && !IntersectRect(&overlap,&a,&r),"bottom transform fields fit without overlap"); }
            CStringW file;file.Format(L"transform-controls-tests/panel-%d-%d-%d.bmp",tool,percent,dark);CaptureCanvasDialog(ParameterBarWindow(),file);
        }
    }
    if(testLanguages.empty())EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    for(WORD language:testLanguages)for(UINT id:{IDS_SKEW_HORIZONTAL,IDS_SKEW_VERTICAL})RequireCanvas(!ExactString(language,id).IsEmpty(),"both skew tooltips translated in every supported locale");
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();
    puts("PASS toolbar placement, panel controls/separator at four scales and both themes, translations in all languages");
}
static void TestTransformControls()
{
    CreateDirectoryW(L"transform-controls-tests",NULL);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);mainWindow.SetWindowPos(NULL,0,0,1100,950,SWP_NOZORDER|SWP_NOACTIVATE);
    TestSelectionPopupPreservation();TestExactSkewFields();TestSeparateTransformHandles();TestTransformPanelLayout();
    canvasWindow.OnEndDraw(TRUE);puts("PASS all transform-control integration checks");
}
