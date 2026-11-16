struct TestBitmap
{
    HBITMAP bitmap;
    HDC dc;
    HGDIOBJ old;
    TestBitmap(int w,int h,COLORREF fill) : bitmap(CreateColorDIB(w,h,fill)), dc(CreateCompatibleDC(NULL))
    { old=SelectObject(dc,bitmap); }
    ~TestBitmap() { SelectObject(dc,old); DeleteDC(dc); DeleteObject(bitmap); }
};

static void TestSelectionResize()
{
    DWORD savedMode=registrySettings.SelectionInterpolation;
    const COLORREF black=RGB(0,0,0),white=RGB(255,255,255),red=RGB(255,0,0),blue=RGB(0,0,255);
    HBITMAP source=CreateColorDIB(4,4,white);
    HDC dc=CreateCompatibleDC(NULL); HGDIOBJ old=SelectObject(dc,source);
    for(int y=0;y<4;++y) for(int x=0;x<2;++x) SetPixelV(dc,x,y,black);
    SelectObject(dc,old);
    SelectionModel model;
    COLORREF algorithmPixels[3][16];
    for(int mode=0;mode<3;++mode)
    {
        registrySettings.SelectionInterpolation=mode;
        TestBitmap result(16,16,RGB(10,20,30));
        model.DrawSelection(result.dc,white,FALSE,CRect(0,0,16,16),source);
        int intermediate=0;
        for(int x=0;x<16;++x)
        {
            COLORREF pixel=GetPixel(result.dc,x,8);
            algorithmPixels[mode][x]=pixel;
            if(pixel!=black && pixel!=white) ++intermediate;
        }
        RequireCanvas(mode?intermediate>0:intermediate==0,"selected interpolation actually changes pixels");
        RequireCanvas(GetPixel(result.dc,0,0)==black && GetPixel(result.dc,15,15)==white,"opaque borders do not fade");
    }
    RequireCanvas(memcmp(algorithmPixels[1],algorithmPixels[2],sizeof(algorithmPixels[1]))!=0,
                  "bilinear and bicubic are distinct");
    DeleteObject(source);
    source=CreateColorDIB(16,16,white); old=SelectObject(dc,source);
    for(int y=0;y<16;++y) for(int x=0;x<16;++x) SetPixelV(dc,x,y,((x+y)&1)?white:black);
    SelectObject(dc,old); model.NotifyContentChanged();
    for(int mode=1;mode<3;++mode)
    {
        registrySettings.SelectionInterpolation=mode;
        TestBitmap result(1,1,red);
        model.DrawSelection(result.dc,white,FALSE,CRect(0,0,1,1),source);
        int c=GetRValue(GetPixel(result.dc,0,0));
        RequireCanvas(c>=110 && c<=145,"shrinking checkerboard is antialiased");
    }
    DeleteObject(source);
    // Transparent color key is removed before filtering, avoiding green halos on blue.
    source=CreateColorDIB(4,4,RGB(0,255,0)); old=SelectObject(dc,source);
    for(int y=0;y<4;++y) for(int x=0;x<2;++x) SetPixelV(dc,x,y,red);
    SelectObject(dc,old); model.NotifyContentChanged();
    for(int mode=1;mode<3;++mode)
    {
        registrySettings.SelectionInterpolation=mode;
        TestBitmap result(16,16,blue);
        model.DrawSelection(result.dc,RGB(0,255,0),TRUE,CRect(0,0,16,16),source);
        bool mixed=false;
        for(int x=0;x<16;++x)
        {
            COLORREF c=GetPixel(result.dc,x,8);
            RequireCanvas(GetGValue(c)==0,"no color-key halo");
            if(GetRValue(c)>0 && GetBValue(c)>0) mixed=true;
        }
        RequireCanvas(mixed && GetPixel(result.dc,15,8)==blue,"smooth transparent boundary");
    }
    // A freehand mask receives the same filtered alpha treatment.
    HBITMAP mask=CreateMonoBitmap(4,4,FALSE); old=SelectObject(dc,mask);
    for(int y=0;y<4;++y) for(int x=0;x<2;++x) SetPixelV(dc,x,y,white);
    SelectObject(dc,old);
    model.setMask(CRect(0,0,4,4),mask);
    TestBitmap masked(16,16,blue);
    model.DrawSelection(masked.dc,white,FALSE,CRect(0,0,16,16),source);
    bool mixed=false;
    for(int x=0;x<16;++x)
    {
        COLORREF c=GetPixel(masked.dc,x,8);
        RequireCanvas(GetGValue(c)==0,"no outside-mask color bleed");
        if(GetRValue(c)>0 && GetBValue(c)>0) mixed=true;
    }
    RequireCanvas(mixed && GetPixel(masked.dc,15,8)==blue,"freehand mask smoothing");
    model.ClearMaskImage();
    DeleteObject(source); DeleteDC(dc);

    toolsModel.OnEndDraw(FALSE);
    toolsModel.SetActiveTool(TOOL_RECTSEL);
    toolsModel.SetBackgroundTransparent(FALSE);
    imageModel.Crop(64,64);
    imageModel.ClearHistory();
    source=CreateColorDIB(4,4,white);
    dc=CreateCompatibleDC(NULL); old=SelectObject(dc,source);
    for(int y=0;y<4;++y) for(int x=0;x<2;++x) SetPixelV(dc,x,y,black);
    SelectObject(dc,old); DeleteDC(dc);
    selectionModel.InsertFromHBITMAP(source,0,0);
    selectionModel.m_bShow=TRUE;
    selectionModel.m_rc=CRect(0,0,16,16);
    TestBitmap first(16,16,white),second(16,16,white);
    selectionModel.DrawSelection(first.dc,white,FALSE);
    selectionModel.m_rc=CRect(0,0,2,2);
    selectionModel.DrawSelection(second.dc,white,FALSE);
    selectionModel.m_rc=CRect(0,0,16,16);
    selectionModel.DrawSelection(second.dc,white,FALSE);
    for(int y=0;y<16;++y) for(int x=0;x<16;++x)
        RequireCanvas(GetPixel(first.dc,x,y)==GetPixel(second.dc,x,y),"dragging does not accumulate blur");
    HBITMAP copied=selectionModel.GetSelectionContents();
    dc=CreateCompatibleDC(NULL); old=SelectObject(dc,copied);
    for(int y=0;y<16;++y) for(int x=0;x<16;++x)
        RequireCanvas(GetPixel(first.dc,x,y)==GetPixel(dc,x,y),"copied selection matches preview");
    SelectObject(dc,old); DeleteDC(dc); DeleteObject(copied);
    selectionModel.StretchSkew(50,50,0,0);
    RequireCanvas(selectionModel.m_rc.Width()==8,"percentage resize uses selection extent");
    selectionModel.StretchSkew(200,200,0,0);
    selectionModel.Landing();
    for(int y=0;y<16;++y) for(int x=0;x<16;++x)
        RequireCanvas(GetPixel(first.dc,x,y)==GetPixel(imageModel.GetDC(),x,y),"committed pixels match filtered preview");
    RequireCanvas(imageModel.CanUndo(),"resampling participates in undo");
    imageModel.Undo(); imageModel.Redo();
    RequireCanvas(GetPixel(first.dc,7,8)==GetPixel(imageModel.GetDC(),7,8),"resampled redo");

    HWND panel=GetDlgItem(toolSettingsWindow,IDC_RESAMPLE_PANEL);
    RequireCanvas(panel && (GetWindowLongPtr(panel,GWL_STYLE)&WS_VISIBLE),"selection panel visible");
    RequireCanvas(!GetDlgItem(mainWindow,IDC_RESAMPLE_PANEL),"no right-side panel");
    RECT panelRect,parentRect;
    GetWindowRect(panel,&panelRect); GetWindowRect(toolSettingsWindow,&parentRect);
    RequireCanvas(panelRect.left>=parentRect.left && panelRect.right<=parentRect.right &&
        panelRect.bottom<=parentRect.bottom,"icon panel fits tool settings");
    HWND tip=static_cast<HWND>(GetPropW(panel,L"SelectionResizeTooltip"));
    printf("Tooltip exists=%d count=%lld\n",!!tip,static_cast<long long>(SendMessage(tip,TTM_GETTOOLCOUNT,0,0)));
    RequireCanvas(tip && SendMessage(tip,TTM_GETTOOLCOUNT,0,0)==3,"three algorithm tooltips");
    for(int i=0;i<3;++i)
    {
        WCHAR name[256]={}; GetDlgItemTextW(panel,IDC_RESAMPLE_FIRST+i,name,_countof(name));
        CStringW expected; expected.LoadString(g_hinstExe,IDS_RESAMPLE_NEAREST+i);
        RequireCanvas(expected==name,"icon buttons retain accessible translated names");
    }
    toolsModel.SetActiveTool(TOOL_PEN);
    RequireCanvas(!(GetWindowLongPtr(panel,GWL_STYLE)&WS_VISIBLE),"selection panel hidden for other tools");
    toolsModel.SetActiveTool(TOOL_FREESEL);
    RequireCanvas(GetWindowLongPtr(panel,GWL_STYLE)&WS_VISIBLE,"free selection panel visible");
    // Preserve the user's actual preferences while testing the radio buttons and persistence.
    TestIniKey key;
    key.Create(HKEY_CURRENT_USER,L"Software\\ReactOS Paint\\Settings");
    const WCHAR* names[]={L"UILanguage",L"ReturnAfterColorPick",L"SelectionInterpolation"};
    DWORD original[3]={}; LONG exists[3];
    for(int i=0;i<3;++i) exists[i]=key.QueryDWORDValue(names[i],original[i]);
    bool radioOk=true;
    for(int mode=0;mode<3;++mode)
    {
        SendDlgItemMessage(panel,IDC_RESAMPLE_FIRST+mode,BM_CLICK,0,0);
        DWORD stored=99;
        radioOk &= registrySettings.SelectionInterpolation==mode &&
            key.QueryDWORDValue(L"SelectionInterpolation",stored)==ERROR_SUCCESS && stored==mode;
    }
    for(int i=0;i<3;++i)
        if(exists[i]==ERROR_SUCCESS) key.SetDWORDValue(names[i],original[i]); else key.DeleteValue(names[i]);
    RequireCanvas(radioOk,"algorithm radio buttons and preference persistence");
    toolsModel.SetActiveTool(TOOL_RECTSEL);
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    mainWindow.RedrawWindow(NULL,NULL,RDW_UPDATENOW|RDW_ALLCHILDREN|RDW_INVALIDATE);
    CaptureCanvasDialog(mainWindow,L"selection-resize-preview.bmp");
    CaptureCanvasDialog(toolSettingsWindow,L"selection-resize-icons.bmp");
    mainWindow.ShowWindow(SW_HIDE);
    registrySettings.SelectionInterpolation=savedMode;
    puts("PASS selection resize: 3 modes, downsampling, alpha/mask edges, stable source, copy/commit/undo, panel and persistence");
}
