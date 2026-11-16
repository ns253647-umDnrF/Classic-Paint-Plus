static void ApplyTestZoom(int method)
{
    if(method==0){mainWindow.SendMessage(WM_COMMAND,IDM_VIEWZOOM200);return;}
    if(method==1)
    {
        BYTE saved[256],keys[256];GetKeyboardState(saved);CopyMemory(keys,saved,256);keys[VK_CONTROL]=0x80;SetKeyboardState(keys);
        mainWindow.SendMessage(WM_MOUSEWHEEL,MAKEWPARAM(MK_CONTROL,WHEEL_DELTA),0);SetKeyboardState(saved);
        RequireCanvas(toolsModel.GetZoom()==2000,"Ctrl+wheel actually changes zoom");return;
    }
    toolsModel.SetActiveTool(TOOL_ZOOM);
    if(method==2)
    {
        HWND slider=FindWindowExW(toolSettingsWindow,NULL,TRACKBAR_CLASSW,NULL);
        SendMessage(slider,TBM_SETPOS,TRUE,2);toolSettingsWindow.SendMessage(WM_VSCROLL,TB_THUMBTRACK,LPARAM(slider));
        RequireCanvas(toolsModel.GetActiveTool()==TOOL_ZOOM,"slider keeps temporary zoom mode");toolsModel.ReturnFromZoom();
    }
    if(method==3)
    {
        HWND list=toolSettingsWindow.GetDlgItem(IDC_ZOOM_PRESETS);RECT row;SendMessage(list,LB_GETITEMRECT,1,LPARAM(&row));
        LPARAM p=MAKELPARAM((row.left+row.right)/2,(row.top+row.bottom)/2);
        SendMessage(list,WM_LBUTTONDOWN,MK_LBUTTON,p);SendMessage(list,WM_LBUTTONUP,0,p);
    }
    if(method==4)
    {
        HWND edit=toolSettingsWindow.GetDlgItem(IDC_ZOOM_EDIT);SetFocus(edit);SetWindowTextW(edit,L"137.5");SendMessage(edit,WM_KEYDOWN,VK_RETURN,0);
    }
    if(method==5 || method==6)
    {
        FigureMouse(method==5?WM_LBUTTONDOWN:WM_RBUTTONDOWN,method==5?MK_LBUTTON:MK_RBUTTON,100,110);
        FigureMouse(method==5?WM_LBUTTONUP:WM_RBUTTONUP,0,100,110);
    }
    RequireCanvas(toolsModel.GetActiveTool()!=TOOL_ZOOM,"discrete zoom resumes original editor");
}
static void TestZoomEditors()
{
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);mainWindow.SetWindowPos(NULL,0,0,1200,900,SWP_NOACTIVATE|SWP_NOZORDER);
    for(int method=0;method<7;++method)
    {
        VectorReset(1600,1200);selectionModel.CombineSelection(CRect(70,60,190,150),TriangleMask(120,90),0);
        selectionModel.SetRotationAngle(31);selectionModel.SetSkew(.2,-.1);
        auto pixels=SelectionRaster().pixels,document=DocumentRaster().pixels;CRect bounds=selectionModel.m_rc;
        ApplyTestZoom(method);
        RequireCanvas(toolsModel.IsSelection() && selectionModel.m_bShow && selectionModel.m_rc==bounds,"zoom preserves active irregular selection bounds");
        RequireCanvas(SelectionRaster().pixels==pixels && DocumentRaster().pixels==document,"zoom never lands or resamples selection pixels");
        RequireCanvas(selectionModel.SetRotationAngle(42),"selection stays transformable after zoom");
        VectorReset(1600,1200);ActivateFigureChoice(FIG_DIAMOND);toolsModel.ResetObjectSize(false);NewParameterFigure();
        toolsModel.SetFigureAngle(27);toolsModel.SetFigureSkew(.15,-.2);toolsModel.EditableFigureBounds(bounds);document=DocumentRaster().pixels;
        ApplyTestZoom(method);CRect after;double x,y;toolsModel.FigureSkew(x,y);toolsModel.EditableFigureBounds(after);
        RequireCanvas(toolsModel.HasEditableFigure() && bounds==after && toolsModel.FigureAngle()==27 && fabs(x-.15)<1e-10 && fabs(y+.2)<1e-10,"zoom preserves editable figure geometry and transform");
        RequireCanvas(DocumentRaster().pixels==document,"zoom does not commit vector figure to document");
        RequireCanvas(toolsModel.SetFigureDimensions(200,100) && toolsModel.SetFigureAngle(45),"figure stays editable after zoom");
        PrepareTextRegression();NewRegressionText();textEditWindow.SetWindowTextW(L"Zoom keeps this text");textEditWindow.SetRotationAngle(33);
        textEditWindow.SendMessage(EM_SETSEL,5,10);textEditWindow.GetEditRect(&bounds);document=DocumentRaster().pixels;
        ApplyTestZoom(method);textEditWindow.GetEditRect(&after);DWORD first,last;textEditWindow.SendMessage(EM_GETSEL,WPARAM(&first),LPARAM(&last));
        RequireCanvas(toolsModel.GetActiveTool()==TOOL_TEXT && textEditWindow.IsWindowVisible() && WindowLabel(textEditWindow)==L"Zoom keeps this text","zoom preserves editable text and original tool");
        RequireCanvas(bounds==after && textEditWindow.Angle()==33 && first==5 && last==10,"zoom preserves text bounds, angle and character selection");
        RequireCanvas(DocumentRaster().pixels==document,"zoom does not rasterize active text");
        if(method>=2)RequireCanvas(GetFocus()==textEditWindow.m_hWnd,"returning from zoom restores text keyboard focus");
        textEditWindow.SendMessage(EM_REPLACESEL,TRUE,LPARAM(L"retains"));RequireCanvas(WindowLabel(textEditWindow)==L"Zoom retains this text","text can still be edited after zoom");
        printf("PASS zoom route %d: selection mask/pixels, vector geometry, rotated text, continued editing\n",method);
    }
    VectorReset(1600,1200);ActivateFigureChoice(FIG_DIAMOND);NewParameterFigure();toolsModel.SetActiveTool(TOOL_ZOOM);
    ActivateFigureChoice(FIG_STAR5);RequireCanvas(!toolsModel.HasEditableFigure(),"choosing a different figure while zooming commits the old one");
    NewParameterFigure();toolsModel.SetActiveTool(TOOL_ZOOM);toolsModel.OnEndDraw(TRUE);
    RequireCanvas(!toolsModel.HasEditableFigure(),"cancel while zooming cancels the suspended editor");toolsModel.ReturnFromZoom();
}
static void TestSuppliedSwapArrows()
{
    CreateDirectoryW(L"zoom-edit-tests",NULL);
    for(int dark:{0,1})for(int scale:{100,150,200,250})
    {
        registrySettings.DarkTheme=dark;registrySettings.PanelMagnification=scale;ApplyTheme();ApplyPanelScale();
        const int side=scale==100?20:scale==150?28:scale==200?36:43;
        TestBitmap bitmap(side,side,RGB(255,0,255));RECT r={0,0,side,side};DrawPanelSwapArrow(bitmap.dc,r);
        if(!dark)
        {
            for(int p=0;p<side;++p)RequireCanvas(GetPixel(bitmap.dc,p,0)==0 && GetPixel(bitmap.dc,p,side-1)==0 && GetPixel(bitmap.dc,0,p)==0 && GetPixel(bitmap.dc,side-1,p)==0,"light swap arrow has a complete square black frame");
            if(scale==100 || scale==250)
            {
                CImage original;RequireCanvas(SUCCEEDED(original.Load(scale==100?L"4444/1.png":L"4444/2.png")),"load supplied reference arrow");
                for(int y=1;y<side-1;++y)for(int x=1;x<side-1;++x)
                {
                    if(GetPixel(bitmap.dc,x,y)!=original.GetPixel(x,y))printf("Arrow %d at %d,%d: got %06lx expected %06lx\n",scale,x,y,GetPixel(bitmap.dc,x,y),original.GetPixel(x,y));
                    RequireCanvas(GetPixel(bitmap.dc,x,y)==original.GetPixel(x,y),"100/250 percent arrow reproduces supplied pixels exactly inside frame");
                }
            }
        }
        CStringW file;file.Format(L"zoom-edit-tests/arrow-%d-%d.bmp",scale,dark);SaveShapeSheet(bitmap.bitmap,file);
        file.Format(L"zoom-edit-tests/palette-%d-%d.bmp",scale,dark);CaptureCanvasDialog(paletteWindow,file);
        CRect client;paletteWindow.GetClientRect(client);POINT p={PanelPx(20)+side/2,client.bottom/2-PanelPx(15)-PanelPx(14)+side/2};
        COLORREF foreground=paletteModel.GetFgColor(),background=paletteModel.GetBgColor();
        paletteWindow.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(p.x,p.y));paletteWindow.SendMessage(WM_LBUTTONUP,0,MAKELPARAM(p.x,p.y));
        RequireCanvas(paletteModel.GetFgColor()==background && paletteModel.GetBgColor()==foreground,"swap arrow button hit region works at each scale/theme");
    }
    registrySettings.DarkTheme=0;registrySettings.PanelMagnification=100;ApplyTheme();ApplyPanelScale();
    puts("PASS supplied swap arrows: exact native pixels, black frame, four sizes, both themes and button hit testing");
}
static void TestZoomEdit(){TestZoomEditors();TestSuppliedSwapArrows();}
