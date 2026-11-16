static HWND ParamEdit(int p)
{
    const int ids[]={IDC_OBJECT_WIDTH,IDC_OBJECT_HEIGHT,IDC_SELECTION_ANGLE,IDC_SKEW_HORIZONTAL,IDC_SKEW_VERTICAL,IDC_ROUNDING_EDIT,IDC_STAR_BEND_EDIT};
    return ParameterControl(ids[p]);
}
static void ParamCheck(int p,bool fixed)
{
    HWND w=ParameterControl(IDC_PARAMETER_FIXED_FIRST+p);
    if((SendMessage(w,BM_GETCHECK,0,0)==BST_CHECKED)!=fixed)SendMessage(w,BM_CLICK,0,0);
    RequireCanvas(ParameterIsFixed(p)==fixed,"checkbox sets requested fixation state");
}
static void NewParameterFigure()
{ FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,80,70);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,240,180);FigureMouse(WM_LBUTTONUP,0,240,180);RequireCanvas(toolsModel.HasEditableFigure(),"editable figure created"); }
static void TestParameterPresets()
{
    VectorReset(800,600);ActivateFigureChoice(FIG_RECT);toolsModel.ResetObjectSize(false);
    NewParameterFigure();toolsModel.SetFigureAngle(35);toolsModel.SetFigureSkew(.2,-.3);toolsModel.OnEndDraw(FALSE);
    NewParameterFigure();double x,y;toolsModel.FigureSkew(x,y);
    RequireCanvas(!toolsModel.FigureAngle() && !x && !y,"unsaved live angle and skew never leak into next figure");
    toolsModel.OnEndDraw(TRUE);EnterObjectValue(ParamEdit(PARAM_ANGLE),L"27");EnterObjectValue(ParamEdit(PARAM_SKEW_X),L"15");
    EnterObjectValue(ParamEdit(PARAM_HEIGHT),L"90");ParamCheck(PARAM_HEIGHT,true);ParamCheck(PARAM_ANGLE,true);ParamCheck(PARAM_SKEW_X,true);SendMessage(ParameterControl(IDC_SAVE_OBJECT_SIZE),BM_CLICK,0,0);
    NewParameterFigure();CRect bounds;toolsModel.EditableFigureBounds(bounds);toolsModel.FigureSkew(x,y);
    RequireCanvas(bounds.Height()==90 && bounds.Width()==160 && toolsModel.FigureAngle()==27 && fabs(x-tan(M_PI/12))<1e-12,"S keeps partial dimensions, angle and skew snapshot");
    RequireCanvas(!ParameterIsFixed(PARAM_WIDTH) && ParameterIsFixed(PARAM_HEIGHT) && ParameterIsFixed(PARAM_ANGLE) && ParameterIsFixed(PARAM_SKEW_X),"saved fields checked on next object");
    toolsModel.SetFigureAngle(73);toolsModel.SetFigureSkew(.4,.5);toolsModel.OnEndDraw(TRUE);NewParameterFigure();toolsModel.FigureSkew(x,y);
    RequireCanvas(toolsModel.FigureAngle()==27 && fabs(x-tan(M_PI/12))<1e-12 && !y,"later edits do not overwrite S snapshot");
    // S saves shown values even after unlocking; clearing a field deliberately omits it.
    ParamCheck(PARAM_HEIGHT,false);RequireCanvas(WindowLabel(ParamEdit(PARAM_HEIGHT))==L"90 px","unlocking active field keeps displayed geometry");
    EnterObjectValue(ParamEdit(PARAM_WIDTH),L"");SendMessage(ParameterControl(IDC_SAVE_OBJECT_SIZE),BM_CLICK,0,0);toolsModel.OnEndDraw(TRUE);NewParameterFigure();
    RequireCanvas(!ParameterIsFixed(PARAM_WIDTH) && !ParameterIsFixed(PARAM_HEIGHT) && toolsModel.ObjectSizePreset(false).cy==90,"S preserves blank width and shown unlocked height together with its unchecked state");
    toolsModel.OnEndDraw(TRUE);SendMessage(ParameterControl(IDC_RESET_OBJECT_SIZE),BM_CLICK,0,0);
    for(int p=0;p<=PARAM_SKEW_Y;++p)RequireCanvas(WindowLabel(ParamEdit(p)).IsEmpty() && !ParameterIsFixed(p),"R clears current type's prepared dimensions and transforms");
    NewParameterFigure();toolsModel.FigureSkew(x,y);RequireCanvas(!toolsModel.FigureAngle() && !x && !y,"R resets transforms for actual new geometry");toolsModel.OnEndDraw(TRUE);
    EnterObjectValue(ParamEdit(PARAM_WIDTH),L"123");ParamCheck(PARAM_WIDTH,true);ParamCheck(PARAM_WIDTH,false);RequireCanvas(WindowLabel(ParamEdit(PARAM_WIDTH))==L"123 px" && toolsModel.ObjectSizePreset(false).cx==123,"uncheck before creation preserves prepared value");
    EnterObjectValue(ParamEdit(PARAM_ANGLE),L"0");RequireCanvas(!ParameterIsFixed(PARAM_ANGLE),"explicit zero does not fix the field");ParamCheck(PARAM_ANGLE,true);
    SetFocus(ParamEdit(PARAM_ANGLE));SendMessage(ParamEdit(PARAM_ANGLE),EM_SETSEL,0,-1);SendMessage(ParamEdit(PARAM_ANGLE),WM_KEYDOWN,VK_DELETE,0);canvasWindow.SetFocus();
    RequireCanvas(!ParameterIsFixed(PARAM_ANGLE) && WindowLabel(ParamEdit(PARAM_ANGLE)).IsEmpty(),"Delete followed by focus transfer clears fixation");
    toolsModel.ResetObjectSize(false);puts("PASS parameter presets: no inheritance, S/R unchanged, partial snapshots, exact transforms, clearing/unchecking and zero");
}
static void TestParameterLocks()
{
    VectorReset(800,600);ActivateFigureChoice(FIG_RECT);toolsModel.ResetObjectSize(false);
    EnterObjectValue(ParamEdit(PARAM_WIDTH),L"160");ParamCheck(PARAM_WIDTH,true);NewParameterFigure();CRect before;toolsModel.EditableFigureBounds(before);CRect screen=before;--screen.right;--screen.bottom;canvasWindow.ImageToCanvas(screen);
    POINT handle=TransformHandle(screen,HIT_LOWER_RIGHT,0,0,0);VectorDrag(handle,{handle.x+40,handle.y+30});CRect after;toolsModel.EditableFigureBounds(after);
    RequireCanvas(after.Width()==160 && after.Height()==before.Height()+30,"mouse resize preserves fixed width but changes unlocked height");
    ParamCheck(PARAM_WIDTH,false);screen=after;--screen.right;--screen.bottom;canvasWindow.ImageToCanvas(screen);handle=TransformHandle(screen,HIT_LOWER_RIGHT,0,0,0);VectorDrag(handle,{handle.x+20,handle.y+10});toolsModel.EditableFigureBounds(before);
    RequireCanvas(before.Width()==180,"unlock restores mouse resizing");
    EnterObjectValue(ParamEdit(PARAM_ANGLE),L"0");EnterObjectValue(ParamEdit(PARAM_SKEW_X),L"0");ParamCheck(PARAM_ANGLE,true);ParamCheck(PARAM_SKEW_X,true);
    VectorClick(SelectionCanvasPoint(before.CenterPoint().x,before.CenterPoint().y));screen=before;--screen.right;--screen.bottom;canvasWindow.ImageToCanvas(screen);
    handle=TransformArrowHandle(screen,HIT_LOWER_RIGHT,0,0,0);VectorDrag(handle,{handle.x-35,handle.y+35});RequireCanvas(!toolsModel.FigureAngle(),"fixed angle resists rotation handle");
    handle=TransformArrowHandle(screen,HIT_UPPER_CENTER,0,0,0);VectorDrag(handle,{handle.x+30,handle.y});double x,y;toolsModel.FigureSkew(x,y);RequireCanvas(!x && !y,"fixed horizontal skew resists shear handle");
    ParamCheck(PARAM_SKEW_X,false);VectorDrag(handle,{handle.x+30,handle.y});toolsModel.FigureSkew(x,y);RequireCanvas(fabs(x)>.1,"unlocked horizontal skew changes by mouse");
    toolsModel.OnEndDraw(TRUE);toolsModel.ResetObjectSize(false);
    PrepareTextRegression();EnterObjectValue(ParamEdit(PARAM_ANGLE),L"22");EnterObjectValue(ParamEdit(PARAM_HEIGHT),L"50");ParamCheck(PARAM_HEIGHT,true);ParamCheck(PARAM_ANGLE,true);NewRegressionText();
    RequireCanvas(ParameterIsFixed(PARAM_ANGLE) && ParameterIsFixed(PARAM_HEIGHT),"text inherits fixed prepared angle and height");
    ParamCheck(PARAM_HEIGHT,false);registrySettings.PointSize=96;textEditWindow.UpdateFont();textEditWindow.GetEditRect(&after);RequireCanvas(after.Height()>50,"unlocking text height restores font fitting");
    toolsModel.OnEndDraw(TRUE);toolsModel.ResetObjectSize(true);
    VectorReset();selectionModel.CombineSelection(CRect(50,50,200,180),NULL,0);EnterObjectValue(ParamEdit(PARAM_ANGLE),L"10");RequireCanvas(!ParameterIsFixed(PARAM_ANGLE),"selection angle input does not fix rotation");ParamCheck(PARAM_ANGLE,true);selectionModel.HideSelection();selectionModel.CombineSelection(CRect(50,50,200,180),NULL,0);RequireCanvas(!ParameterIsFixed(PARAM_ANGLE),"new selection starts unlocked");
    puts("PASS parameter locks: native resize, rotate/skew handles, text auto-height and independent selection state");
}
static void CheckAlignedParameterLabels()
{
    for(auto pair:{std::pair<int,int>{PARAM_WIDTH,PARAM_HEIGHT},{PARAM_SKEW_X,PARAM_SKEW_Y},{PARAM_ROUNDING,PARAM_STAR_BEND}})
    {
        RECT a,b;GetWindowRect(ParamEdit(pair.first),&a);GetWindowRect(ParamEdit(pair.second),&b);
        RequireCanvas(a.left==b.left && a.right==b.right,"paired parameter fields align vertically regardless of caption width");
    }
    for(int p=0;p<PARAM_COUNT;++p)
    {
        HWND label=ParameterControl(IDC_PARAMETER_FIXED_FIRST+p);RECT a,b;GetWindowRect(label,&a);GetWindowRect(ParamEdit(p),&b);
        HDC dc=GetDC(label);HGDIOBJ old=SelectObject(dc,HFONT(SendMessage(label,WM_GETFONT,0,0)));
        CStringW text=WindowLabel(label);SIZE size={};GetTextExtentPoint32W(dc,text,text.GetLength(),&size);SelectObject(dc,old);ReleaseDC(label,dc);
        RequireCanvas(b.left-a.left-size.cx-PanelPx(p<PARAM_ROUNDING?17:3)>=PanelPx(6),"every translated caption has a visible scaled gap before its edit");
    }
}
static void TestParameterLayout()
{
    VectorReset();ActivateFigureChoice(FIG_STAR5);toolsModel.ResetObjectSize(false);
    EnterObjectValue(ParamEdit(PARAM_WIDTH),L"160");EnterObjectValue(ParamEdit(PARAM_HEIGHT),L"90");
    EnterObjectValue(ParamEdit(PARAM_ANGLE),L"15");EnterObjectValue(ParamEdit(PARAM_SKEW_X),L"10");
    EnterObjectValue(ParamEdit(PARAM_SKEW_Y),L"0");EnterObjectValue(ParamEdit(PARAM_STAR_BEND),L"+25");
    for(int percent:{100,150,200,250})for(int dark:{0,1})for(int width:{1100,650})
    {
        mainWindow.SetWindowPos(NULL,0,0,width,950,SWP_NOZORDER|SWP_NOACTIVATE);registrySettings.PanelMagnification=percent;registrySettings.DarkTheme=dark;ApplyTheme();ApplyPanelScale();
        CheckAlignedParameterLabels();
        RECT bar,canvas,palette;GetWindowRect(ParameterBarWindow(),&bar);canvasWindow.GetWindowRect(&canvas);paletteWindow.GetWindowRect(&palette);
        if(bar.top!=canvas.bottom || bar.top!=palette.top)printf("Layout scale=%d width=%d canvasBottom=%ld bar=%ld..%ld paletteTop=%ld\n",percent,width,canvas.bottom,bar.top,bar.bottom,palette.top);
        RequireCanvas(bar.top==canvas.bottom && bar.top==palette.top && bar.left==palette.right && bar.bottom==palette.bottom,"geometry bar sits to the right of palette on the same bottom row");
        std::vector<RECT> controls;
        RECT roundEdit,starEdit,roundSlider,starSlider;GetWindowRect(ParamEdit(PARAM_ROUNDING),&roundEdit);GetWindowRect(ParamEdit(PARAM_STAR_BEND),&starEdit);
        GetWindowRect(ParameterControl(IDC_FIGURE_ROUNDING),&roundSlider);GetWindowRect(ParameterControl(IDC_STAR_BEND_SLIDER),&starSlider);
        RequireCanvas(roundEdit.left==starEdit.left && roundEdit.right==starEdit.right && roundSlider.left==starSlider.left && roundSlider.right==starSlider.right,"rounding and star-bend edits and sliders align exactly in shared columns");
        for(HWND c=GetWindow(ParameterBarWindow(),GW_CHILD);c;c=GetWindow(c,GW_HWNDNEXT))if(IsWindowVisible(c))
        {
            RECT r;GetWindowRect(c,&r);RequireCanvas(r.top>=bar.top && r.bottom<=bar.bottom,"two parameter rows fit vertically at every scale and window width");
            for(const auto& other:controls){RECT intersection;if(IntersectRect(&intersection,&r,&other)){printf("Overlap scale=%d width=%d id=%d rect=%ld,%ld,%ld,%ld other=%ld,%ld,%ld,%ld\n",percent,width,GetDlgCtrlID(c),r.left,r.top,r.right,r.bottom,other.left,other.top,other.right,other.bottom);CaptureCanvasDialog(ParameterBarWindow(),L"parameter-bar-tests/overlap.bmp");}RequireCanvas(!IntersectRect(&intersection,&r,&other),"bottom controls never overlap");}controls.push_back(r);
        }
        CStringW name;name.Format(L"parameter-bar-tests/bar-%d-%d-%d.bmp",percent,dark,width);SendMessage(ParameterBarWindow(),WM_HSCROLL,SB_LEFT,0);CaptureCanvasDialog(ParameterBarWindow(),name);
        SendMessage(ParameterBarWindow(),WM_HSCROLL,SB_RIGHT,0);RECT last;GetWindowRect(ParameterControl(IDC_STAR_BEND_SLIDER),&last);RequireCanvas(last.right<=bar.right && last.right>bar.left,"horizontal scroll reaches the final star slider");SendMessage(ParameterBarWindow(),WM_HSCROLL,SB_LEFT,0);
        RECT actions;GetWindowRect(ObjectActionsWindow(),&actions);RequireCanvas(actions.top==palette.top && actions.bottom==palette.bottom && actions.right==palette.left,"S/R remain at the foot of the left panel level with palette");
        if(width==1100){name.Format(L"parameter-bar-tests/window-%d-%d.bmp",percent,dark);CaptureCanvasDialog(mainWindow,name);}
        if(width==1100 && percent==100 && !dark)
        {
            RECT total=palette,actions;GetWindowRect(ObjectActionsWindow(),&actions);UnionRect(&total,&total,&actions);UnionRect(&total,&total,&bar);
            TestBitmap strip(total.right-total.left,total.bottom-total.top,GetSysColor(COLOR_BTNFACE));
            for(HWND child:{ObjectActionsWindow(),paletteWindow.m_hWnd,ParameterBarWindow()})
            { RECT r;GetWindowRect(child,&r);SetViewportOrgEx(strip.dc,r.left-total.left,r.top-total.top,NULL);PrintWindow(child,strip.dc,0); }
            SaveShapeSheet(strip.bitmap,L"parameter-bar-tests/bottom-strip.bmp");
        }
    }
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();
    HWND slider=ParameterControl(IDC_FIGURE_ROUNDING);RequireCanvas(!(GetWindowLong(slider,GWL_STYLE)&TBS_VERT),"rounding slider is horizontal");
    EnterObjectValue(ParamEdit(PARAM_ROUNDING),L"43");RequireCanvas(FigureRounding()==43 && SendMessage(slider,TBM_GETPOS,0,0)==43 && !ParameterIsFixed(PARAM_ROUNDING),"rounding edit synchronizes slider without fixation");
    SetFocus(slider);SendMessage(slider,WM_KEYDOWN,VK_END,0);SendMessage(slider,WM_KEYUP,VK_END,0);printf("Rounding End: model=%d slider=%lld text=%ls\n",FigureRounding(),(long long)SendMessage(slider,TBM_GETPOS,0,0),LPCWSTR(WindowLabel(ParamEdit(PARAM_ROUNDING))));RequireCanvas(FigureRounding()==MAX_FIGURE_RADIUS && WindowLabel(ParamEdit(PARAM_ROUNDING))==L"500 px","horizontal End sets maximum pixel radius");
    SendMessage(slider,WM_KEYDOWN,VK_HOME,0);SendMessage(slider,WM_KEYUP,VK_HOME,0);RequireCanvas(FigureRounding()==0,"horizontal Home sets rounding zero");
    if(testLanguages.empty())EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    for(WORD language:testLanguages)for(int p=0;p<PARAM_COUNT;++p)RequireCanvas(!ExactString(language,IDS_PARAMETER_FIRST+p).IsEmpty(),"every parameter caption translated");
    puts("PASS bottom layout: four scales, light/dark, two window widths, no clipping/overlap, horizontal rounding and translations");
}
#include "figuregeometry.h"
static void TestStarBend()
{
    VectorReset(800,600);ActivateFigureChoice(FIG_STAR5);toolsModel.ResetObjectSize(false);HWND edit=ParamEdit(PARAM_STAR_BEND),slider=ParameterControl(IDC_STAR_BEND_SLIDER);
    EnterObjectValue(edit,L"-65");RequireCanvas(!ParameterIsFixed(PARAM_STAR_BEND) && toolsModel.StarBend()==-65,"signed star bend can be prepared before creation");
    NewParameterFigure();RequireCanvas(toolsModel.StarBend()==-65 && !ParameterIsFixed(PARAM_STAR_BEND),"new star applies prepared bend");
    CheckLiveRepaint("star bend redraws editable geometry immediately",[]{EnterObjectValue(ParamEdit(PARAM_STAR_BEND),L"+70");});
    RequireCanvas(toolsModel.StarBend()==70 && SendMessage(slider,TBM_GETPOS,0,0)==70 && WindowLabel(edit)==L"+70%","star bend editor and slider synchronize");
    SendMessage(ParameterControl(IDC_SAVE_OBJECT_SIZE),BM_CLICK,0,0);EnterObjectValue(edit,L"-20");toolsModel.OnEndDraw(TRUE);NewParameterFigure();RequireCanvas(toolsModel.StarBend()==70 && !ParameterIsFixed(PARAM_STAR_BEND),"S snapshots star bend independently of later changes");
    ParamCheck(PARAM_STAR_BEND,false);RequireCanvas(WindowLabel(edit)==L"+70%" && toolsModel.StarBend()==70,"unlocking active star retains its geometry");
    FigureClipboardData d={};RequireCanvas(toolsModel.ExportFigure(d) && d.starBend==70,"editable copy exports exact star bend");HGLOBAL memory=PackFigureClipboard(d);FigureClipboardData copy={};RequireCanvas(UnpackFigureClipboard(memory,copy) && copy.starBend==70,"clipboard preserves custom star bend");GlobalFree(memory);
    memory=GlobalAlloc(GMEM_MOVEABLE,offsetof(FigureClipboardData,starBend));auto old=d;old.rounding=0;old.version=1;old.bytes=DWORD(offsetof(FigureClipboardData,starBend));void* bytes=GlobalLock(memory);memcpy(bytes,&old,old.bytes);GlobalUnlock(memory);RequireCanvas(UnpackFigureClipboard(memory,copy) && copy.version==3 && copy.starBend==0,"legacy clipboard stars keep original outline");GlobalFree(memory);
    toolsModel.OnEndDraw(TRUE);ActivateFigureChoice(FIG_STAR4);toolsModel.ResetObjectSize(false);NewParameterFigure();RequireCanvas(!toolsModel.StarBend(),"other star kind has independent default");toolsModel.OnEndDraw(TRUE);ActivateFigureChoice(FIG_STAR5);NewParameterFigure();RequireCanvas(toolsModel.StarBend()==70,"switching kinds preserves S snapshot");toolsModel.OnEndDraw(TRUE);
    SendMessage(ParameterControl(IDC_RESET_OBJECT_SIZE),BM_CLICK,0,0);RequireCanvas(WindowLabel(edit).IsEmpty() && !ParameterIsFixed(PARAM_STAR_BEND) && !toolsModel.StarBend(),"R clears bend preset to standard star");NewParameterFigure();
    SetFocus(slider);SendMessage(slider,WM_KEYDOWN,VK_HOME,0);RequireCanvas(toolsModel.StarBend()==-100,"star slider Home selects negative endpoint");SendMessage(slider,WM_KEYDOWN,VK_END,0);RequireCanvas(toolsModel.StarBend()==100,"star slider End selects positive endpoint");
    toolsModel.OnEndDraw(TRUE);NewParameterFigure();RequireCanvas(!toolsModel.StarBend(),"unsaved bend does not leak into next star");toolsModel.OnEndDraw(TRUE);
    TestBitmap sheet(840,420,RGB(255,255,255));int col=0;
    for(int kind:{FIG_STAR4,FIG_STAR5,FIG_STAR6,FIG_STAR8,FIG_STAR16,FIG_STAR32})
    {
        int points=FigureStarPoints(kind);auto normal=FigureGeometry(kind),deep=FigureGeometry(kind,120,-100),flat=FigureGeometry(kind,120,100);
        RequireCanvas(deep[0][1].x==.5 && deep[0][1].y==.5,"negative endpoint brings inner vertices to centre");
        auto a=flat[0][0],b=flat[0][1],c=flat[0][2];RequireCanvas(fabs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))<1e-12,"positive endpoint lies exactly on polygon edge");
        double radius=hypot(normal[0][1].x-.5,normal[0][1].y-.5);RequireCanvas(fabs(radius-(points==5?(3-sqrt(5.0))/4:points==4?.16:points<8?.23:.38))<1e-12,"zero bend preserves the standard star geometry");
        for(int row=0;row<3;++row){RECT bounds={col*140+15,row*140+15,col*140+125,row*140+125};DrawFigure(sheet.dc,kind,bounds,(HBRUSH)GetStockObject(BLACK_BRUSH),(HBRUSH)GetStockObject(LTGRAY_BRUSH),2,1,0,0,0,0,(row-1)*80);}++col;
    }
    SaveShapeSheet(sheet.bitmap,L"parameter-bar-tests/star-bend.bmp");
    puts("PASS star bend: negative/zero/positive geometry, live repaint, signed slider, per-kind S/R, unlock and versioned clipboard");
}
static void TestSmallUiFixes()
{
    for(int percent:{100,150,200,250})for(int dark:{0,1})for(int selected:{0,1})
    {
        toolsModel.SetActiveTool(TOOL_BRUSH);toolsModel.SetBrushStyle(BrushStyleRound);toolsModel.SetBrushWidth(selected?1:4);
        registrySettings.PanelMagnification=percent;registrySettings.DarkTheme=dark;ApplyTheme();ApplyPanelScale();
        HWND panel=toolSettingsWindow.GetDlgItem(IDC_BRUSH_PANEL);TestBitmap sample(PanelPx(14),PanelPx(16),RGB(255,0,255));
        DRAWITEMSTRUCT d={};d.CtlType=ODT_LISTBOX;d.CtlID=IDC_BRUSH_PRESET_FIRST;d.itemID=2;d.hDC=sample.dc;d.rcItem={0,0,PanelPx(14),PanelPx(16)};
        SendMessage(panel,WM_DRAWITEM,d.CtlID,LPARAM(&d));int dots=0;COLORREF ink=GetSysColor(selected?COLOR_HIGHLIGHTTEXT:COLOR_BTNTEXT);
        for(int y=0;y<PanelPx(16);++y)for(int x=0;x<PanelPx(14);++x)if(GetPixel(sample.dc,x,y)==ink)++dots;
        RequireCanvas(dots==1,"1px round preset stays exactly one physical pixel at every panel scale, selected or not, in both themes");
        CStringW path;path.Format(L"parameter-bar-tests/brush-%d-%d-%d.bmp",percent,dark,selected);CaptureCanvasDialog(panel,path);
    }
    for(int dark:{0,1})
    {
        registrySettings.DarkTheme=dark;HIMAGELIST icons=CreatePanelToolIcons(100);TestBitmap sample(16,16,dark?RGB(0,0,0):RGB(255,255,255));ImageList_Draw(icons,9,sample.dc,0,0,ILD_TRANSPARENT);ImageList_Destroy(icons);
        HBITMAP source=(HBITMAP)LoadImageW(g_hinstExe,MAKEINTRESOURCEW(IDB_TOOLBARICONS),IMAGE_BITMAP,256,16,LR_CREATEDIBSECTION);
        HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,source);
        for(int y=0;y<16;++y)for(int x=0;x<16;++x){COLORREF expected=GetPixel(dc,9*16+x,y);if(expected==RGB(255,0,255))expected=dark?RGB(0,0,0):RGB(255,255,255);else if(dark && expected==RGB(0,0,0))expected=RGB(255,255,255);RequireCanvas(GetPixel(sample.dc,x,y)==expected,"100 percent text A exactly matches the classic bitmap, with dark ink adaptation");}
        SelectObject(dc,old);DeleteDC(dc);DeleteObject(source);
        CStringW path;path.Format(L"parameter-bar-tests/text-icon-100-%d.bmp",dark);SaveShapeSheet(sample.bitmap,path);
    }
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();
    for(WORD language:testLanguages)
    {
        registrySettings.UILanguage=language;ApplyPreferredLanguage();toolSettingsWindow.ReloadLanguage();
        CheckAlignedParameterLabels();
        for(int p=0;p<PARAM_COUNT;++p)RequireCanvas(WindowLabel(ParameterControl(IDC_PARAMETER_FIXED_FIRST+p))==ExactString(language,IDS_PARAMETER_FIRST+p),"bottom labels change immediately to each supported language");
        RECT a,b;GetWindowRect(ParamEdit(PARAM_ROUNDING),&a);GetWindowRect(ParamEdit(PARAM_STAR_BEND),&b);RequireCanvas(a.left==b.left && a.right==b.right,"rounding and bend fields align for every translated caption");
        GetWindowRect(ParameterControl(IDC_FIGURE_ROUNDING),&a);GetWindowRect(ParameterControl(IDC_STAR_BEND_SLIDER),&b);RequireCanvas(a.left==b.left && a.right==b.right,"rounding and bend sliders align for every translated caption");
        HMENU menu=LoadMenuW(g_hinstExe,MAKEINTRESOURCEW(ID_MENU));BuildSettingsMenu(menu);HMENU settings=GetSubMenu(menu,4);
        WCHAR languageTitle[64];GetMenuStringW(settings,5,languageTitle,_countof(languageTitle),MF_BYPOSITION);
        RequireCanvas(!lstrcmpW(languageTitle,L"Language"),"Language menu caption stays English in every interface language");
        RequireCanvas(GetMenuItemID(GetSubMenu(settings,0),0)==IDM_PANEL_100 && GetMenuItemID(GetSubMenu(settings,1),0)==IDM_TOOLBOX_MARGIN,"left-panel offset is immediately after panel size in every language");DestroyMenu(menu);
    }
    registrySettings.UILanguage=MAKELANGID(LANG_RUSSIAN,SUBLANG_DEFAULT);ApplyPreferredLanguage();toolSettingsWindow.ReloadLanguage();
    toolsModel.SetActiveTool(TOOL_RECTSEL);toolsModel.SetActiveTool(TOOL_ZOOM);HWND edit=toolSettingsWindow.GetDlgItem(IDC_ZOOM_EDIT);SetFocus(edit);SetWindowTextW(edit,L"147");canvasWindow.SetFocus();
    RequireCanvas(toolsModel.GetZoom()==1470 && toolsModel.GetActiveTool()==TOOL_RECTSEL,"focus transfer applies numeric zoom and returns to selection");
    toolsModel.SetActiveTool(TOOL_ZOOM);EnterObjectValue(edit,L"99999");RequireCanvas(GetFocus()==edit && toolsModel.GetActiveTool()==TOOL_ZOOM,"invalid zoom does not return or change tool");SendMessage(edit,WM_KEYDOWN,VK_ESCAPE,0);RequireCanvas(toolsModel.GetActiveTool()==TOOL_ZOOM,"Escape discards zoom input without switching tool");
    SetFocus(edit);canvasWindow.SetFocus();RequireCanvas(toolsModel.GetActiveTool()==TOOL_ZOOM,"focus alone in zoom field keeps Zoom active");
    canvasWindow.zoomTo(1000);puts("PASS UI refinements: visible 1px preset, original 100% A, menu order and all live translations; numeric zoom return, validation and Escape");
}
static INT_PTR CALLBACK NumericReferenceDialog(HWND,UINT,WPARAM,LPARAM){return FALSE;}
static void TestCompactNumericFields()
{
    HWND reference=CreateDialogParamW(g_hinstExe,MAKEINTRESOURCEW(IDD_ATTRIBUTES),mainWindow,NumericReferenceDialog,0);
    RequireCanvas(reference!=NULL,"original attributes dialog available as numeric-field reference");
    RECT legacy;GetWindowRect(GetDlgItem(reference,IDD_ATTRIBUTESEDIT1),&legacy);DestroyWindow(reference);
    for(int percent:{100,150,200,250})for(int dark:{0,1})
    {
        registrySettings.PanelMagnification=percent;registrySettings.DarkTheme=dark;ApplyTheme();ApplyPanelScale();
        std::vector<HWND> edits;
        for(int p=0;p<PARAM_COUNT;++p)edits.push_back(ParamEdit(p));
        for(int id:{IDC_ZOOM_EDIT,IDC_TRANSPARENCY_EDIT,IDC_SPRAY_SMOOTHING_EDIT,IDC_LINE_WIDTH})edits.push_back(toolSettingsWindow.GetDlgItem(id));
        edits.push_back(GetDlgItem(toolSettingsWindow.GetDlgItem(IDC_BRUSH_PANEL),IDC_BRUSH_SIZE_EDIT));
        for(HWND edit:edits)
        {
            RECT window,client,text;GetWindowRect(edit,&window);GetClientRect(edit,&client);SendMessage(edit,EM_GETRECT,0,LPARAM(&text));
            HDC dc=GetDC(edit);HGDIOBJ old=SelectObject(dc,HFONT(SendMessage(edit,WM_GETFONT,0,0)));TEXTMETRICW metric={};GetTextMetricsW(dc,&metric);SelectObject(dc,old);ReleaseDC(edit,dc);
            if(percent==100)RequireCanvas(window.bottom-window.top==legacy.bottom-legacy.top,"100 percent panel edits match original dialog edit height exactly");
            int padding=client.bottom-metric.tmHeight;
            printf("Numeric field: scale=%d dark=%d id=%d height=%ld client=%ld text=%ld top=%ld padding=%d\n",percent,dark,GetDlgCtrlID(edit),window.bottom-window.top,client.bottom,metric.tmHeight,text.top,padding);
            RequireCanvas(padding>=1 && padding<=3,"native edit has compact padding at every scale");
            RequireCanvas(abs(text.top-(padding-text.top))<=1,"native edit text line stays vertically centered");
        }
    }
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();
    puts("PASS numeric field heights: match original dialogs at 100 percent, centered native text on both themes at all four scales");
}
static void TestSuppliedSwapArrows();
static void TestPanelArtwork()
{
    TestSuppliedSwapArrows(); // Current user artwork replaces the former vector shaft.
    for(int dark:{0,1})for(int percent:{100,150,200,250})
    {
        registrySettings.DarkTheme=dark;registrySettings.PanelMagnification=percent;ApplyTheme();ApplyPanelScale();
        int side=percent==100?20:percent==150?28:percent==200?36:43;TestBitmap arrow(side,side,GetSysColor(COLOR_BTNFACE));RECT r={0,0,side,side};DrawPanelSwapArrow(arrow.dc,r);
        CStringW path;path.Format(L"parameter-bar-tests/swap-%d-%d.bmp",percent,dark);SaveShapeSheet(arrow.bitmap,path);
        path.Format(L"parameter-bar-tests/palette-%d-%d.bmp",percent,dark);CaptureCanvasDialog(paletteWindow,path);
        HWND figures=FindWindowExW(toolBoxContainer,NULL,L"PaintFigureControls",NULL);
        path.Format(L"parameter-bar-tests/figures-%d-%d.bmp",percent,dark);CaptureCanvasDialog(figures,path);
        TestBitmap icon(40,40,GetSysColor(COLOR_BTNFACE));r={0,0,40,40};DrawFigureIcon(icon.dc,FIG_RECT,r,GetSysColor(COLOR_BTNTEXT));
        COLORREF fill=GetPixel(icon.dc,20,20);
        RequireCanvas(dark?(GetRValue(fill)<100 && GetGValue(fill)<100 && GetBValue(fill)<100):GetRValue(fill)>230,"figure interiors use a dark fill only in the dark theme");
    }
    registrySettings.DarkTheme=0;registrySettings.PanelMagnification=100;ApplyTheme();ApplyPanelScale();
    puts("PASS artwork at four scales in both themes: supplied swap arrows, dark figure fills and native button captures");
}
static void TestParameterBar()
{
    CreateDirectoryW(L"parameter-bar-tests",NULL);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);mainWindow.SetWindowPos(NULL,0,0,1100,950,SWP_NOZORDER|SWP_NOACTIVATE);
    TestParameterPresets();TestParameterLocks();TestParameterLayout();TestStarBend();TestSmallUiFixes();TestCompactNumericFields();TestPanelArtwork();puts("PASS all parameter-bar checks");
}
