static void TestCanvasDragFrame()
{
    toolsModel.OnEndDraw(FALSE);
    selectionModel.HideSelection();
    toolsModel.SetZoom(1000);
    imageModel.PushImageForUndo(CreateColorDIB(320,200,RGB(255,255,255)));
    imageModel.ClearHistory();
    canvasWindow.updateScrollRange();
    canvasWindow.updateScrollPos();
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    for(TOOLTYPE tool:{TOOL_RECTSEL,TOOL_PEN}) for(int delta:{40,-40})
    {
        toolsModel.SetActiveTool(tool);
        CRect current(0,0,320,200); canvasWindow.ImageToCanvas(current);
        POINT start={current.right+1,current.bottom+1};
        POINT end={start.x+delta,start.y+delta};
        CRect target(0,0,320+delta,200+delta); canvasWindow.ImageToCanvas(target);
        canvasWindow.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW);
        HDC dc=canvasWindow.GetDC();
        COLORREF before[32];
        for(int x=0;x<32;++x) before[x]=GetPixel(dc,target.left+15+x,target.bottom-1);
        canvasWindow.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(start.x,start.y));
        canvasWindow.SendMessage(WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(end.x,end.y));
        canvasWindow.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW);
        RequireCanvas(imageModel.GetWidth()==320 && imageModel.GetHeight()==200 && !imageModel.CanUndo(),
                      "canvas drag preview does not modify image");
        int changed=0,transitions=0; bool last=false;
        for(int x=0;x<32;++x)
        {
            bool different=GetPixel(dc,target.left+15+x,target.bottom-1)!=before[x];
            changed+=different;
            if(x && different!=last) ++transitions;
            last=different;
        }
        RequireCanvas(changed>=8 && changed<=24 && transitions>=8,"dotted canvas preview on image and workspace");
        if(tool==TOOL_RECTSEL)
            CaptureCanvasDialog(mainWindow,delta>0?L"canvas-drag-grow.bmp":L"canvas-drag-shrink.bmp");
        canvasWindow.SendMessage(WM_KEYDOWN,VK_ESCAPE,0);
        canvasWindow.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW);
        for(int x=0;x<32;++x)
            RequireCanvas(GetPixel(dc,target.left+15+x,target.bottom-1)==before[x],"Escape removes preview cleanly");
        canvasWindow.ReleaseDC(dc);
        RequireCanvas(imageModel.GetWidth()==320 && !imageModel.CanUndo(),"Escape preserves size and history");
        canvasWindow.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(start.x,start.y));
        canvasWindow.SendMessage(WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(end.x,end.y));
        canvasWindow.SendMessage(WM_LBUTTONUP,0,MAKELPARAM(end.x,end.y));
        RequireCanvas(imageModel.GetWidth()==320+delta && imageModel.GetHeight()==200+delta,"mouse release applies preview size");
        imageModel.Undo(); imageModel.ClearHistory();
        canvasWindow.updateScrollRange();
    }
    // Losing capture must discard the rubber-band rectangle and ignore a subsequent release.
    CRect current(0,0,320,200); canvasWindow.ImageToCanvas(current);
    LPARAM start=MAKELPARAM(current.right+1,current.bottom+1);
    LPARAM end=MAKELPARAM(current.right+51,current.bottom+31);
    canvasWindow.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,start);
    canvasWindow.SendMessage(WM_MOUSEMOVE,MK_LBUTTON,end);
    ::SetCapture(mainWindow);
    canvasWindow.SendMessage(WM_LBUTTONUP,0,end);
    RequireCanvas(imageModel.GetWidth()==320 && !imageModel.CanUndo(),"capture loss cancels canvas resize");
    ::ReleaseCapture();
    mainWindow.ShowWindow(SW_HIDE);
    toolsModel.SetActiveTool(TOOL_RECTSEL);
    puts("PASS canvas drag: dotted growth/shrink preview, selection/pencil, Escape, release, capture loss and Undo");
}
