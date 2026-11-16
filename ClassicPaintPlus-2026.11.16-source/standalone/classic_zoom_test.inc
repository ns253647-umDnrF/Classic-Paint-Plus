static void TestClassicZoomPanel()
{
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);mainWindow.SetWindowPos(NULL,0,0,1100,950,SWP_NOZORDER|SWP_NOACTIVATE);
    toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();
    CreateDirectoryW(L"classic-zoom-tests",NULL);
    const int presets[]={1000,2000,6000,8000},positions[]={4,3,1,0};
    for(int percent:{100,150,200,250}) for(int dark:{0,1})
    {
        registrySettings.PanelMagnification=percent;registrySettings.DarkTheme=dark;ApplyTheme();ApplyPanelScale();
        toolsModel.SetActiveTool(TOOL_ZOOM);
        HWND list=toolSettingsWindow.GetDlgItem(IDC_ZOOM_PRESETS),edit=toolSettingsWindow.GetDlgItem(IDC_ZOOM_EDIT);
        HWND slider=FindWindowExW(toolSettingsWindow,NULL,TRACKBAR_CLASSW,NULL);
        RequireCanvas(IsWindowVisible(list) && SendMessage(list,LB_GETCOUNT,0,0)==4,"four classic zoom choices visible");
        RECT a,b,c,client;GetWindowRect(list,&a);GetWindowRect(slider,&b);GetWindowRect(edit,&c);toolSettingsWindow.GetClientRect(&client);
        RequireCanvas(a.bottom<b.top && b.bottom<c.top,"zoom choices, slider and edit appear in the requested order without overlaps");
        MapWindowPoints(NULL,toolSettingsWindow,reinterpret_cast<POINT*>(&c),2);
        RequireCanvas(c.bottom<=client.bottom,"zoom percent field fits its panel at every panel size");
        for(int i=0;i<4;++i)
        {
            toolsModel.SetActiveTool(TOOL_BRUSH);toolsModel.SetActiveTool(TOOL_ZOOM);
            RECT item;SendMessage(list,LB_GETITEMRECT,i,LPARAM(&item));
            LPARAM pos=MAKELPARAM((item.left+item.right)/2,(item.top+item.bottom)/2);
            SendMessage(list,WM_LBUTTONDOWN,MK_LBUTTON,pos);SendMessage(list,WM_LBUTTONUP,0,pos);
            RequireCanvas(toolsModel.GetZoom()==presets[i] && SendMessage(list,LB_GETCURSEL,0,0)==i,"native mouse clicks apply each classic zoom preset");
            RequireCanvas(toolsModel.GetActiveTool()==TOOL_BRUSH,"classic preset returns to the actual previous tool");
            RequireCanvas(SendMessage(slider,TBM_GETPOS,0,0)==positions[i],"classic zoom presets map to exact slider stops including 600 percent");
            WCHAR actual[32];GetWindowTextW(edit,actual,32);CStringW expected;expected.Format(L"%d%%",presets[i]/10);
            RequireCanvas(!wcscmp(actual,expected),"zoom preset synchronizes percentage field");
        }
        toolsModel.SetActiveTool(TOOL_TEXT);toolsModel.SetActiveTool(TOOL_ZOOM);SetFocus(edit);
        SetWindowTextW(edit,L"137.5");SendMessage(edit,WM_KEYDOWN,VK_RETURN,0);
        RequireCanvas(toolsModel.GetZoom()==1375 && SendMessage(list,LB_GETCURSEL,0,0)==LB_ERR,"custom zoom clears classic preset highlight");
        RequireCanvas(toolsModel.GetActiveTool()==TOOL_TEXT,"numeric zoom returns to Text even when another painting tool was used earlier");
        toolsModel.SetActiveTool(TOOL_ZOOM);SetFocus(list);SendMessage(list,WM_KEYDOWN,VK_HOME,0);
        RequireCanvas(toolsModel.GetZoom()==1000,"Home in the zoom list selects its first row without canvas navigation");
        toolsModel.SetActiveTool(TOOL_ZOOM);SetFocus(list);SendMessage(list,WM_KEYDOWN,VK_END,0);
        RequireCanvas(toolsModel.GetZoom()==8000,"End in the zoom list selects its last row");
        toolsModel.SetActiveTool(TOOL_ZOOM);
        const int sliderValues[]={8000,6000,4000,2000,1000,500,250,125};
        for(int pos=0;pos<8;++pos)
        {
            SendMessage(slider,TBM_SETPOS,TRUE,pos);toolSettingsWindow.SendMessage(WM_VSCROLL,TB_THUMBTRACK,LPARAM(slider));
            RequireCanvas(toolsModel.GetZoom()==sliderValues[pos] && toolsModel.GetActiveTool()==TOOL_ZOOM,"zoom slider retains its full 12.5 to 800 percent range");
        }
        SendMessage(slider,TBM_SETPOS,TRUE,1);toolSettingsWindow.SendMessage(WM_VSCROLL,TB_THUMBTRACK,LPARAM(slider));
        RequireCanvas(toolsModel.GetZoom()==6000 && SendMessage(list,LB_GETCURSEL,0,0)==2,"slider 600 percent synchronizes the classic list");
        CStringW file;file.Format(L"classic-zoom-tests/zoom-%d-%d.bmp",percent,dark);CaptureCanvasDialog(toolSettingsWindow,file);
        toolsModel.SetActiveTool(TOOL_RECTSEL);RequireCanvas(!IsWindowVisible(list) && !IsWindowVisible(edit),"zoom choices hide with the zoom tool");
        for(int transparent:{0,1})
        {
            toolsModel.SetBackgroundTransparent(transparent);
            file.Format(L"classic-zoom-tests/trans-%d-%d-%d.bmp",percent,dark,transparent);CaptureCanvasDialog(toolSettingsWindow,file);
            // Inspect the actual owner-painted panel, including its selection fill.
            HBITMAP capture=(HBITMAP)LoadImageW(NULL,file,IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_CREATEDIBSECTION);
            HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,capture);
            int left=PanelPx(3)+PanelPx(2),right=PanelPx(47)-PanelPx(2);
            int top=PanelPx(5)+PanelPx(2),bottom=PanelPx(68)-PanelPx(2),middle=(top+bottom)/2;
            int y=transparent?(middle+bottom)/2:(top+middle)/2;
            RequireCanvas(GetPixel(dc,left,y)==GetSysColor(COLOR_HIGHLIGHT) && GetPixel(dc,right-1,y)==GetSysColor(COLOR_HIGHLIGHT),"selected transparency option has visible blue margins on both sides");
            SelectObject(dc,old);DeleteDC(dc);DeleteObject(capture);
        }
    }
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;ApplyTheme();ApplyPanelScale();canvasWindow.zoomTo(1000);
    puts("PASS classic zoom: native presets, 600% slider stop, numeric synchronization, custom zoom, layout/hiding and transparency highlight margins at four panel scales in both themes");
}
