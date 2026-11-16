static int shapeCanvasSizes=0,shapeCanvasErases=0;
static void TestFreePolygonRounding()
{
    VectorReset();SetBitmapAndInfo(CreateColorDIB(360,260,RGB(255,255,255)),NULL,NULL,FALSE);
    ActivateFigureChoice(-TOOL_SHAPE);SetFigureRounding(50);toolsModel.SetShapeStyle(2);toolsModel.SetLineWidth(1);paletteModel.SetFgColor(RGB(0,0,0));
    toolsModel.OnButtonDown(TRUE,60,60,FALSE);toolsModel.OnMouseMove(TRUE,220,60);toolsModel.OnButtonUp(TRUE,220,60);
    toolsModel.OnButtonDown(TRUE,220,180,FALSE);toolsModel.OnMouseMove(TRUE,220,180);toolsModel.OnButtonUp(TRUE,220,180);
    toolsModel.OnButtonDown(TRUE,60,180,FALSE);toolsModel.OnMouseMove(TRUE,60,180);toolsModel.OnButtonUp(TRUE,60,180);
    toolsModel.OnEndDraw(FALSE);
    for(POINT p:std::initializer_list<POINT>{{63,63},{217,63},{217,177},{63,177}})
        RequireCanvas(GetPixel(imageModel.GetDC(),p.x,p.y)==RGB(255,255,255),"every free-polygon corner rounds despite duplicate construction endpoints");
    RequireCanvas(GetPixel(imageModel.GetDC(),140,120)==RGB(0,0,0),"rounded free polygon remains filled");
    imageModel.Undo();RequireCanvas(GetPixel(imageModel.GetDC(),140,120)==RGB(255,255,255),"rounded free polygon remains a single undo step");
    ResetFigureSession();puts("PASS native free polygon: every corner rounded, fill and undo");
}
static LRESULT CALLBACK CountShapeCanvas(HWND window,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR)
{
    if(msg==WM_SIZE)++shapeCanvasSizes;
    if(msg==WM_ERASEBKGND)
    {
        HDC dc=reinterpret_cast<HDC>(wp);COLORREF before=GetPixel(dc,30,30);
        LRESULT result=DefSubclassProc(window,msg,wp,lp);
        if(!result || GetPixel(dc,30,30)!=before)++shapeCanvasErases;
        return result;
    }
    return DefSubclassProc(window,msg,wp,lp);
}
static int ShapeChoiceId(int choice)
{ return choice>=0?IDC_FIGURE_FIRST+choice:IDC_LINE_CHOICE_FIRST+(choice==-TOOL_LINE?0:choice==-TOOL_BEZIER?1:2); }
static void SaveShapeSheet(HBITMAP bitmap,LPCWSTR path)
{
    GdiFlush();BITMAP b={};GetObject(bitmap,sizeof(b),&b);
    BITMAPFILEHEADER f={};f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(BITMAPINFOHEADER);f.bfSize=f.bfOffBits+b.bmWidthBytes*b.bmHeight;
    BITMAPINFOHEADER h={};h.biSize=sizeof(h);h.biWidth=b.bmWidth;h.biHeight=b.bmHeight;h.biPlanes=1;h.biBitCount=b.bmBitsPixel;h.biSizeImage=b.bmWidthBytes*b.bmHeight;
    FILE* out=_wfopen(path,L"wb");RequireCanvas(out!=NULL,"open native shape contact sheet");fwrite(&f,sizeof(f),1,out);fwrite(&h,sizeof(h),1,out);fwrite(b.bmBits,h.biSizeImage,1,out);fclose(out);
}
static void TestShapeGroups()
{
    CreateDirectoryW(L"shape-groups-tests",NULL);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    mainWindow.SetWindowPos(NULL,0,0,1100,950,SWP_NOZORDER|SWP_NOACTIVATE);
    RequireCanvas(FigureGroupSelection(GROUP_LINE)==-TOOL_LINE && FigureGroupSelection(GROUP_POLYGON)==FIG_RECT && FigureGroupSelection(GROUP_STARS)==FIG_STAR5,"fresh session group defaults");
    for(int kind=0;kind<FIG_COUNT;++kind)RequireCanvas(FigureRounding(kind)==0,"fresh process rounding starts at zero");
    VectorReset();HWND toolbar=FindWindowExW(toolBoxContainer,NULL,TOOLBARCLASSNAMEW,NULL),groups=FindWindowExW(toolBoxContainer,NULL,L"PaintFigureControls",NULL);
    RequireCanvas(SendMessage(toolbar,TB_BUTTONCOUNT,0,0)==10 && SendMessage(toolbar,TB_COMMANDTOINDEX,ID_LINE,0)==-1,"ten classic tools followed by grouped tools");
    int count=0;std::vector<int> unique;
    for(int g=0;g<GROUP_COUNT;++g)for(int kind:FigureGroupItems(g))
    { RequireCanvas(kind!=FIG_RRECT && std::find(unique.begin(),unique.end(),kind)==unique.end(),"each exposed item appears once, no separate rounded rectangle");unique.push_back(kind);++count; }
    RequireCanvas(count==48 && FigureGroupItems(GROUP_ARROWS).size()==28 && FigureGroupItems(GROUP_STARS).size()==7,"complete line, polygon, ellipse, star, arrow and callout groups");
    for(int percent:{100,150,200,250})for(int dark:{0,1})
    {
        registrySettings.PanelMagnification=percent;registrySettings.DarkTheme=dark;ApplyTheme();ApplyPanelScale();
        RECT row;GetWindowRect(groups,&row);
        for(int g=0;g<GROUP_COUNT;++g)
        {
            HWND button=GetDlgItem(groups,IDC_SHAPE_GROUP_FIRST+g);RECT r;GetWindowRect(button,&r);
            RequireCanvas(r.left==row.left+(g%2)*(PanelPx(50)/2) && r.top==row.top+(g/2)*PanelPx(25),"group buttons form the requested three rows");
            SendMessage(button,BM_CLICK,0,0);
            if(g==GROUP_ELLIPSE) { RequireCanvas(toolsModel.GetActiveTool()==TOOL_ELLIPSE,"ellipse is a single button");continue; }
            if(percent==100 && !dark && g==GROUP_POLYGON)HoldFigureButton(groups);
            else SendMessage(button,WM_KEYDOWN,VK_DOWN,0);
            HWND popup=FindWindowW(L"PaintFigurePopup",NULL);RequireCanvas(popup && IsWindowVisible(popup),"hold or keyboard opens the appropriate group");
            int visible=0;for(HWND child=GetWindow(popup,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))if(IsWindowVisible(child))++visible;
            RequireCanvas(visible==int(FigureGroupItems(g).size()),"popup contains only this group's buttons");
            CStringW path;path.Format(L"shape-groups-tests/gallery-%d-%d-%d.bmp",g,percent,dark);CaptureCanvasDialog(popup,path);
            for(int kind:FigureGroupItems(g))
            {
                if(!IsWindowVisible(popup))ShowFigureGallery(g);
                SendDlgItemMessage(popup,ShapeChoiceId(kind),BM_CLICK,0,0);
                RequireCanvas(!IsWindowVisible(popup) && FigureGroupSelection(g)==kind,"choosing an item closes popup and updates its group button");
                RequireCanvas(kind<0?toolsModel.GetActiveTool()==-kind:toolsModel.GetActiveTool()==TOOL_FIGURE && registrySettings.FigureKind==kind,"every choice activates its own drawing tool");
            }
            SendMessage(button,WM_KEYDOWN,VK_DOWN,0);SendMessage(GetFocus(),WM_KEYDOWN,VK_LEFT,0);SendMessage(GetFocus(),WM_KEYDOWN,VK_RETURN,0);
            RequireCanvas(FigureGroupSelection(g)==FigureGroupItems(g)[FigureGroupItems(g).size()-2],"popup keyboard navigation follows its group");
        }
        CStringW path;path.Format(L"shape-groups-tests/panel-%d-%d.bmp",percent,dark);CaptureCanvasDialog(toolBoxContainer,path);
    }
    puts("PASS grouped native UI: all 48 choices, click/hold/keyboard, four panel scales and both themes");
    SetWindowSubclass(canvasWindow,CountShapeCanvas,555,0);
    for(int percent:{150,200})
    {
        registrySettings.PanelMagnification=percent;ApplyPanelScale();PumpFigureMessages(30);
        RECT original;canvasWindow.GetWindowRect(&original);shapeCanvasSizes=shapeCanvasErases=0;
        for(int i=0;i<8;++i)for(int kind:std::initializer_list<int>{-TOOL_SHAPE,FIG_DIAMOND,-TOOL_LINE,FIG_STAR5})
        {
            ActivateFigureChoice(kind);PumpFigureMessages(1);RECT current;canvasWindow.GetWindowRect(&current);
            RequireCanvas(EqualRect(&original,&current),"switching short/tall tools preserves canvas position and width");
        }
        printf("Switching at %d%%: canvas resizes=%d, background erases=%d\n",percent,shapeCanvasSizes,shapeCanvasErases);
        RequireCanvas(shapeCanvasSizes==0 && shapeCanvasErases==0,"tool switches never resize or erase the canvas");
    }
    RemoveWindowSubclass(canvasWindow,CountShapeCanvas,555);
    RequireCanvas(GetWindowLongPtr(mainWindow,GWL_STYLE)&WS_CLIPCHILDREN,"main window background excludes child windows");
    puts("PASS flash regression: stable scrollbar gutter and no canvas resize/erase on tool changes at 150/200%");
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();VectorReset();ResetFigureSession();
    HWND slider=toolSettingsWindow.GetDlgItem(IDC_FIGURE_ROUNDING);
    for(int kind:unique)
    {
        ActivateFigureChoice(kind);RequireCanvas(bool(IsWindowVisible(slider))==CanRoundFigure(kind),"rounding offered for every polygon/star and hidden for other tools");
        if(CanRoundFigure(kind)){SetFigureRounding(kind<0?25:(kind*13)%101);RequireCanvas(SendMessage(slider,TBM_GETPOS,0,0)==FigureRounding(kind),"slider follows this shape's own rounding");}
    }
    ActivateFigureChoice(FIG_TRIANGLE);SetFigureRounding(21);ActivateFigureChoice(FIG_STAR5);SetFigureRounding(68);ActivateFigureChoice(FIG_TRIANGLE);
    RequireCanvas(FigureRounding()==21,"rounding is remembered independently while switching shapes");
    toolsModel.ResetObjectSize(false);RequireCanvas(FigureRounding()==0,"R resets the current kind's rounding together with its other parameters");SetFigureRounding(21);
    toolsModel.SetFigureAngle(0);toolsModel.SetShapeStyle(1);toolsModel.SetLineWidth(3);registrySettings.ShapeFromCenter=FALSE;
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,80,70);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,250,210);FigureMouse(WM_LBUTTONUP,0,250,210);
    FigureClipboardData data={};RequireCanvas(toolsModel.ExportFigure(data) && data.rounding==21,"editable figure exports its own rounding");
    CheckLiveRepaint("rounding immediately updates the active vector figure",[]{SetFigureRounding(85);});
    RequireCanvas(toolsModel.ExportFigure(data) && data.rounding==85 && data.lineWidth==3 && data.width==170,"rounding leaves width and stroke unchanged");
    HGLOBAL packed=PackFigureClipboard(data);FigureClipboardData copy={};RequireCanvas(packed && UnpackFigureClipboard(packed,copy) && copy.rounding==85,"private figure clipboard preserves rounding");GlobalFree(packed);
    TestIniKey key;key.Create(HKEY_CURRENT_USER,L"Software\\ReactOS Paint\\Settings");key.SetDWORDValue(L"FigureRounding",73);registrySettings.StorePreferences();DWORD value=0;
    RequireCanvas(key.QueryDWORDValue(L"FigureRounding",value)!=ERROR_SUCCESS,"legacy rounding preference removed from INI");
    RegistrySettings loaded;loaded.Load(SW_HIDE);RequireCanvas(loaded.FigureRounding==0 && FigureRounding(FIG_TRIANGLE)==85,"INI reload cannot change current session rounding");
    toolsModel.OnEndDraw(TRUE);ResetFigureSession();for(int kind:unique)RequireCanvas(FigureRounding(kind)==0,"new session clears all rounding values");
    puts("PASS per-shape rounding: slider visibility, independence, immediate preview, clipboard and session-only persistence");
    TestBitmap sheet(840,((FIG_COUNT+5)/6)*140,RGB(255,255,255));SetBkMode(sheet.dc,TRANSPARENT);
    for(int kind=0;kind<FIG_COUNT;++kind)
    {
        int x=(kind%6)*140,y=(kind/6)*140;RECT r={x+15,y+15,x+125,y+112};
        HBRUSH foreground=CreateSolidBrush(RGB(25,60,100)),background=CreateSolidBrush(RGB(180,215,250));
        DrawFigure(sheet.dc,kind,r,foreground,background,2,1,0,0,0,0);DeleteObject(foreground);DeleteObject(background);
        WCHAR number[20];swprintf_s(number,L"%d",kind);TextOutW(sheet.dc,x+55,y+120,number,lstrlenW(number));
        for(int round:{0,50,100})for(int style:{0,1,2})
        {
            data.kind=kind;data.width=110;data.height=90;data.rounding=CanRoundFigure(kind)?round:0;data.style=style;data.angle=27;data.skewX=.2;data.skewY=-.1;
            HBITMAP bitmap=RasterizeFigure(data);RequireCanvas(bitmap!=NULL,"every figure rasterizes with rotation, skew, all styles and rounding endpoints");
            RasterImage raster;RequireCanvas(ReadRaster(bitmap,raster),"read rasterized figure");int opaque=0,transparent=0;for(DWORD pixel:raster.pixels){if(pixel>>24)++opaque;else ++transparent;}
            RequireCanvas(opaque>20 && transparent>20,"figure raster has visible pixels and transparent exterior");DeleteObject(bitmap);
        }
    }
    SaveShapeSheet(sheet.bitmap,L"shape-groups-tests/drawn-figures.bmp");
    TestBitmap rounded(840,420,RGB(255,255,255));int slot=0;
    for(int kind:{FIG_RECT,FIG_TRIANGLE,FIG_DIAMOND,FIG_HEXAGON,FIG_STAR5,FIG_STAR16})for(int round:{0,10,30})
    {
        int x=(slot/3)*140,y=(slot%3)*140;RECT r={x+15,y+15,x+125,y+125};
        DrawFigure(rounded.dc,kind,r,(HBRUSH)GetStockObject(BLACK_BRUSH),(HBRUSH)GetStockObject(LTGRAY_BRUSH),2,1,0,0,0,round);++slot;
    }
    SaveShapeSheet(rounded.bitmap,L"shape-groups-tests/rounding.bmp");
    if(testLanguages.empty())EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    for(WORD language:testLanguages)
    {
        for(UINT id=IDS_SHAPE_GROUP_FIRST;id<IDS_SHAPE_GROUP_FIRST+GROUP_COUNT;++id)RequireCanvas(!ExactString(language,id).IsEmpty(),"all group names translated");
        for(UINT id=IDS_EXTRA_FIGURE_FIRST;id<IDS_EXTRA_FIGURE_FIRST+FIG_COUNT-FIG_STAR8;++id)RequireCanvas(!ExactString(language,id).IsEmpty(),"all additional figures translated");
    }
    printf("PASS all figure geometry, rounding contact sheets and %zu resource languages\n",testLanguages.size());
    TestFreePolygonRounding();
    puts("PASS all shape-group integration checks");
}
