static bool NearChannel(COLORREF c,int red,int green,int blue,int tolerance=2)
{
    return abs(int(GetRValue(c))-red)<=tolerance && abs(int(GetGValue(c))-green)<=tolerance && abs(int(GetBValue(c))-blue)<=tolerance;
}

static void TestSelectionTransparency()
{
    const COLORREF red=RGB(255,0,0),blue=RGB(0,0,255),white=RGB(255,255,255),green=RGB(0,255,0);
    toolsModel.OnEndDraw(TRUE);toolsModel.SetActiveTool(TOOL_RECTSEL);toolsModel.SetBackgroundTransparent(FALSE);
    paletteModel.SetBgColor(white);DWORD savedMode=registrySettings.SelectionInterpolation;
    for(int mode=0;mode<3;++mode)
    {
        registrySettings.SelectionInterpolation=mode;
        SelectionModel model;model.InsertFromHBITMAP(CreateColorDIB(4,4,red));model.m_bShow=TRUE;
        for(int percent:{0,25,50,100,0})
        {
            RequireCanvas(model.SetTransparency(percent),"valid transparency accepted");
            for(int size:{4,16})
            {
                TestBitmap preview(size,size,blue);model.DrawSelection(preview.dc,white,FALSE,CRect(0,0,size,size));
                RequireCanvas(NearChannel(GetPixel(preview.dc,size/2,size/2),MulDiv(100-percent,255,100),0,MulDiv(percent,255,100)),
                    "transparency blends selected pixels over destination with all three interpolation modes");
            }
        }
        RequireCanvas(!model.SetTransparency(-1) && !model.SetTransparency(101) && model.GetTransparency()==0,"invalid percentages preserve selection");
        model.SetTransparency(50);
        HBITMAP copied=model.GetSelectionContents();HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,copied);
        RequireCanvas(NearChannel(GetPixel(dc,1,1),255,127,127),"copied selection uses the visible transparency over the background color");
        SelectObject(dc,old);DeleteDC(dc);DeleteObject(copied);
        HBITMAP source=CreateColorDIB(4,4,green),mask=CreateMonoBitmap(4,4,FALSE);
        dc=CreateCompatibleDC(NULL);old=SelectObject(dc,source);
        for(int y=0;y<4;++y) for(int x=0;x<2;++x) SetPixelV(dc,x,y,red);
        SelectObject(dc,mask);
        for(int y=0;y<2;++y) for(int x=0;x<4;++x) SetPixelV(dc,x,y,white);
        SelectObject(dc,old);DeleteDC(dc);
        model.InsertFromHBITMAP(source,0,0,mask);model.m_bShow=TRUE;
        RequireCanvas(model.GetTransparency()==0,"new pasted selection starts opaque");model.SetTransparency(50);
        TestBitmap masked(16,16,blue);model.DrawSelection(masked.dc,green,TRUE,CRect(0,0,16,16));
        RequireCanvas(NearChannel(GetPixel(masked.dc,1,1),128,0,127),"mask and global transparency combine");
        RequireCanvas(GetPixel(masked.dc,15,1)==blue && GetPixel(masked.dc,1,15)==blue,"color key and outside-mask pixels remain transparent");
        for(int y=0;y<16;++y) for(int x=0;x<16;++x)
            RequireCanvas(GetGValue(GetPixel(masked.dc,x,y))==0,"no color-key fringe after transparency and resize");
    }
    registrySettings.SelectionInterpolation=2;
    imageModel.PushImageForUndo(CreateColorDIB(32,32,blue));imageModel.ClearHistory();
    RECT area={2,2,10,10};HBRUSH brush=CreateSolidBrush(red);FillRect(imageModel.GetDC(),&area,brush);DeleteObject(brush);
    selectionModel.HideSelection();selectionModel.SetRectFromPoints({2,2},{10,10});selectionModel.m_bShow=TRUE;imageModel.NotifyImageChanged();
    RequireCanvas(selectionModel.SetTransparency(50),"landed selection lifts its original pixels before blending");
    RequireCanvas(GetPixel(imageModel.GetDC(),3,3)==red && !imageModel.CanUndo(),"slider preview leaves the original image and undo stack untouched");
    selectionModel.moveSelection(10,10);
    TestBitmap preview(32,32,blue);BitBlt(preview.dc,0,0,32,32,imageModel.GetDC(),0,0,SRCCOPY);toolsModel.OnDrawOverlayOnImage(preview.dc);
    RequireCanvas(GetPixel(preview.dc,3,3)==white && NearChannel(GetPixel(preview.dc,13,13),128,0,127),"moved selection blends over pixels at the new location");
    selectionModel.SetTransparency(100);selectionModel.SetTransparency(0);selectionModel.SetTransparency(50);
    selectionModel.Landing();
    RequireCanvas(FigurePixels(preview.dc)==FigurePixels(imageModel.GetDC()),"committed transparency matches preview without accumulated fading");
    RequireCanvas(imageModel.CanUndo(),"transparency commit creates undo");imageModel.Undo();
    RequireCanvas(GetPixel(imageModel.GetDC(),3,3)==red && GetPixel(imageModel.GetDC(),13,13)==blue && !imageModel.CanUndo(),"one undo restores pixels and selection origin");
    imageModel.Redo();RequireCanvas(NearChannel(GetPixel(imageModel.GetDC(),13,13),128,0,127),"redo restores blended pixels");
    imageModel.ClearHistory();selectionModel.SetRectFromPoints({0,0},{8,8});selectionModel.m_bShow=TRUE;imageModel.NotifyImageChanged();
    TestBitmap before(32,32,white);BitBlt(before.dc,0,0,32,32,imageModel.GetDC(),0,0,SRCCOPY);
    selectionModel.SetTransparency(80);canvasWindow.SendMessage(WM_KEYDOWN,VK_ESCAPE,0);
    RequireCanvas(FigurePixels(before.dc)==FigurePixels(imageModel.GetDC()) && !imageModel.CanUndo(),"Escape cancels transparency without changing pixels");
    RequireCanvas(selectionModel.GetTransparency()==0,"new selection resets transparency");

    HWND edit=GetDlgItem(toolSettingsWindow,IDC_TRANSPARENCY_EDIT),slider=GetDlgItem(toolSettingsWindow,IDC_SELECTION_TRANSPARENCY);
    RequireCanvas(edit && slider && IsWindowVisible(edit) && !IsWindowEnabled(edit),"percentage and slider are disabled until an area is selected");
    selectionModel.SetRectFromPoints({0,0},{8,8});selectionModel.m_bShow=TRUE;imageModel.NotifyImageChanged();
    RequireCanvas(IsWindowEnabled(edit) && IsWindowEnabled(slider),"selecting an area enables transparency controls");
    SetWindowTextW(edit,L"37%");SendMessage(edit,WM_KEYDOWN,VK_RETURN,0);
    RequireCanvas(selectionModel.GetTransparency()==37 && SendMessage(slider,TBM_GETPOS,0,0)==63,"manual percentage updates the slider");
    for(const WCHAR* bad:{L"-1",L"101",L"nan",L"hello",L"",L"50.5"})
    { SetWindowTextW(edit,bad);SendMessage(edit,WM_KILLFOCUS,0,0);RequireCanvas(selectionModel.GetTransparency()==37,"invalid manual transparency leaves the value unchanged"); }
    SetWindowTextW(edit,L"75");SendMessage(edit,WM_KEYDOWN,VK_ESCAPE,0);
    RequireCanvas(selectionModel.GetTransparency()==37,"Escape cancels unaccepted percentage text");
    SendMessage(slider,TBM_SETPOS,TRUE,15);toolSettingsWindow.SendMessage(WM_VSCROLL,TB_THUMBTRACK,reinterpret_cast<LPARAM>(slider));
    WCHAR text[16];GetWindowTextW(edit,text,_countof(text));
    RequireCanvas(selectionModel.GetTransparency()==85 && !lstrcmpW(text,L"85%"),"slider updates numeric field and selection preview");
    for(UINT command:{IDM_PANEL_100,IDM_PANEL_150,IDM_PANEL_200,IDM_PANEL_250})
    {
        mainWindow.SendMessage(WM_COMMAND,command);
        RequireCanvas(selectionModel.m_bShow && IsWindowEnabled(slider),"changing panel size preserves the active selection");
        SendMessage(slider,WM_KEYDOWN,VK_HOME,0);
        RequireCanvas(selectionModel.GetTransparency()==100,"slider keyboard maximum at each UI scale");
        SendMessage(slider,WM_KEYDOWN,VK_END,0);
        RequireCanvas(selectionModel.GetTransparency()==0,"slider keyboard minimum at each UI scale");
        if(PanelPercent()>100)
        {
            RECT r;GetClientRect(slider,&r);
            SendMessage(slider,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(PanelPx(12),PanelPx(8)));
            RequireCanvas(selectionModel.GetTransparency()==100,"scaled transparency slider reaches maximum by mouse");
            SendMessage(slider,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(PanelPx(12),r.bottom-PanelPx(8)));
            SendMessage(slider,WM_LBUTTONUP,0,MAKELPARAM(PanelPx(12),r.bottom-PanelPx(8)));
            RequireCanvas(selectionModel.GetTransparency()==0,"scaled slider dragging reaches zero");
        }
        CRect child,parent;GetWindowRect(edit,&child);toolSettingsWindow.GetWindowRect(&parent);
        RequireCanvas(child.bottom<=parent.bottom && child.right<=parent.right,"transparency controls fit the scrollable left panel");
        selectionModel.SetTransparency(50);toolSettingsWindow.UpdateSelectionTransparency(true);
        toolSettingsWindow.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
        WCHAR file[80];swprintf_s(file,L"selection-transparency-%d.bmp",PanelPercent());CaptureCanvasDialog(toolSettingsWindow,file);
    }
    mainWindow.SendMessage(WM_COMMAND,IDM_PANEL_100);toolsModel.OnEndDraw(TRUE);
    toolsModel.SetActiveTool(TOOL_PEN);RequireCanvas(!IsWindowVisible(edit) && !IsWindowVisible(slider),"transparency controls hide for drawing tools");
    toolsModel.SetActiveTool(TOOL_RECTSEL);registrySettings.SelectionInterpolation=savedMode;
    puts("PASS selection transparency: 0-100%, modes/masks/color key, original pixels, copy/move/commit/Undo/Redo/Escape, percentage and slider at four panel scales");
}

static void TestPolishedVisuals()
{
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);SetActiveWindow(mainWindow);
    HBITMAP classic=(HBITMAP)LoadImageW(g_hinstExe,MAKEINTRESOURCEW(IDB_TOOLBARICONS),IMAGE_BITMAP,256,16,0);
    HDC source=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(source,classic);
    for(int percent:{150,200,250})
    {
        int side=MulDiv(16,percent,100);HIMAGELIST list=CreatePanelToolIcons(percent);
        for(int tool:{2,3,4,6,7,8})
        {
            COLORREF bg=RGB(190,180,170);TestBitmap target(side,side,bg);
            ImageList_Draw(list,tool,target.dc,0,0,ILD_TRANSPARENT);
            int different=0,edges=0;
            for(int y=0;y<side;++y) for(int x=0;x<side;++x)
            {
                COLORREF classicColor=GetPixel(source,tool*16+x*16/side,y*16/side);
                if(classicColor==RGB(255,0,255)) classicColor=bg;
                COLORREF actual=GetPixel(target.dc,x,y);
                if(actual!=classicColor) ++different;
                if(actual!=bg && GetRValue(actual)>30 && GetRValue(actual)<180) ++edges;
            }
            RequireCanvas(different>side*side/6 && edges>3,"large icons have independent contours and antialiased edges, not magnified pixel blocks");
        }
        ImageList_Destroy(list);
        for(int id:{IDI_NONTRANSPARENT,IDI_TRANSPARENT})
        {
            HICON icon=(HICON)LoadImageW(g_hinstExe,MAKEINTRESOURCEW(id),IMAGE_ICON,40,30,0);
            COLORREF bg=GetSysColor(COLOR_HIGHLIGHT);TestBitmap original(40,30,bg),large(MulDiv(40,percent,100),MulDiv(30,percent,100),bg);
            DrawIconEx(original.dc,0,0,icon,40,30,0,NULL,DI_NORMAL);
            RECT r={0,0,MulDiv(40,percent,100),MulDiv(30,percent,100)};
            DrawPanelTransparency(large.dc,r,id==IDI_TRANSPARENT);
            int different=0;
            for(int y=0;y<r.bottom;++y) for(int x=0;x<r.right;++x)
                if(GetPixel(large.dc,x,y)!=GetPixel(original.dc,x*40/r.right,y*30/r.bottom)) ++different;
            RequireCanvas(different>r.right*r.bottom/4,"larger transparency illustration is drawn with continuous shading and contours");
            DestroyIcon(icon);
        }
    }
    SelectObject(source,old);DeleteDC(source);DeleteObject(classic);
    for(UINT command:{IDM_PANEL_100,IDM_PANEL_150,IDM_PANEL_200,IDM_PANEL_250})
    {
        mainWindow.SendMessage(WM_COMMAND,command);toolBoxContainer.SendMessage(WM_MOUSEWHEEL,MAKEWPARAM(0,2400));
        mainWindow.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
        HWND toolbar=FindWindowExW(toolBoxContainer,NULL,TOOLBARCLASSNAMEW,NULL);
        WCHAR path[80];swprintf_s(path,L"classic-toolbar-%d.bmp",PanelPercent());CaptureCanvasDialog(toolbar,path);
        HWND row=FindWindowExW(toolBoxContainer,NULL,L"PaintFigureControls",NULL);
        SendDlgItemMessage(row,IDC_FIGURE_GALLERY,WM_KEYDOWN,VK_DOWN,0);
        HWND popup=FindWindowW(L"PaintFigurePopup",NULL);
        swprintf_s(path,L"polished-figures-%d.bmp",PanelPercent());CaptureCanvasDialog(popup,path);
        SendMessage(GetFocus(),WM_KEYDOWN,VK_ESCAPE,0);
    }
    auto proc=[](HWND,UINT,WPARAM,LPARAM)->INT_PTR { return FALSE; };
    HWND attributes=CreateDialogParamW(g_hinstExe,MAKEINTRESOURCEW(IDD_ATTRIBUTES),mainWindow,proc,0);
    RECT standard;GetWindowRect(GetDlgItem(attributes,IDD_ATTRIBUTESEDIT1),&standard);
    for(int id:{IDD_CROP_EDGES,IDD_CANVAS_SIZE})
    {
        HWND dialog=CreateDialogParamW(g_hinstExe,MAKEINTRESOURCEW(id),mainWindow,proc,0);
        for(int edit:id==IDD_CROP_EDGES?std::vector<int>{IDC_EDGE_LEFT,IDC_EDGE_RIGHT,IDC_EDGE_TOP,IDC_EDGE_BOTTOM}:std::vector<int>{IDC_NEW_WIDTH,IDC_NEW_HEIGHT})
        {
            RECT r;GetWindowRect(GetDlgItem(dialog,edit),&r);
            RequireCanvas(r.bottom-r.top==standard.bottom-standard.top && r.right-r.left==standard.right-standard.left,"canvas-dialog numeric cells match the standard Attributes cells");
        }
        DestroyWindow(dialog);
    }
    DestroyWindow(attributes);mainWindow.SendMessage(WM_COMMAND,IDM_PANEL_100);
    puts("PASS polished visuals: redrawn classic icons at 150/200/250%, figure gallery captures, standard dialog field sizes");
}
