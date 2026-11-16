// Included by picker_test.cpp: exercises the real native dialogs and image history.
static void RequireCanvas(bool ok, const char* what)
{
    if (!ok) { printf("FAIL canvas: %s\n", what); RestoreTestPreferences();ExitProcess(60); }
}

static void CaptureCanvasDialog(HWND dialog, const WCHAR* path)
{
    RECT r; GetWindowRect(dialog,&r);
    int w=r.right-r.left,h=r.bottom-r.top;
    HDC dc=CreateCompatibleDC(NULL);
    HBITMAP bmp=CreateColorDIB(w,h,RGB(255,255,255));
    HGDIOBJ old=SelectObject(dc,bmp);
    PrintWindow(dialog,dc,0);
    BITMAP bm; GetObject(bmp,sizeof(bm),&bm);
    BITMAPFILEHEADER file={}; file.bfType=0x4d42;
    file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);
    file.bfSize=file.bfOffBits+bm.bmWidthBytes*h;
    BITMAPINFOHEADER info={}; info.biSize=sizeof(info); info.biWidth=w; info.biHeight=h;
    info.biPlanes=1; info.biBitCount=bm.bmBitsPixel; info.biSizeImage=bm.bmWidthBytes*h;
    FILE* f=_wfopen(path,L"wb");
    if(f) { fwrite(&file,sizeof(file),1,f); fwrite(&info,sizeof(info),1,f); fwrite(bm.bmBits,info.biSizeImage,1,f); fclose(f); }
    SelectObject(dc,old); DeleteObject(bmp); DeleteDC(dc);
}

static int canvasCase=0;
static void ClickCanvasSpin(HWND dialog, int index, bool upper)
{
    HWND spin=GetDlgItem(dialog,IDC_SPIN_LEFT+index);
    RECT r; GetClientRect(spin,&r);
    LPARAM point=MAKELPARAM((r.right-r.left)/2,upper?2:r.bottom-3);
    SendMessage(spin,WM_LBUTTONDOWN,MK_LBUTTON,point);
    SendMessage(spin,WM_LBUTTONUP,0,point);
}

static VOID CALLBACK DriveCanvas(HWND,UINT,UINT_PTR timer,DWORD)
{
    HWND dialog=NULL;
    EnumThreadWindows(GetCurrentThreadId(),FindDialog,reinterpret_cast<LPARAM>(&dialog));
    if(!dialog) { if(++ticks>100) ExitProcess(61); return; }
    KillTimer(NULL,timer);
    if(canvasCase<=2 || canvasCase==5)
    {
        SetDlgItemInt(dialog,IDC_EDGE_LEFT,12,FALSE);
        SetDlgItemInt(dialog,IDC_EDGE_RIGHT,23,FALSE);
        SetDlgItemInt(dialog,IDC_EDGE_TOP,34,FALSE);
        SetDlgItemInt(dialog,IDC_EDGE_BOTTOM,15,FALSE);
        // Click the real controls, rather than inventing the sign of UDN_DELTAPOS.
        for(int i=0;i<4;++i)
        {
            int before=GetDlgItemInt(dialog,IDC_EDGE_LEFT+i,NULL,TRUE);
            ClickCanvasSpin(dialog,i,true);
            RequireCanvas(static_cast<int>(GetDlgItemInt(dialog,IDC_EDGE_LEFT+i,NULL,TRUE))==
                before+(i==2?-1:1),"actual upper spinner button direction");
            ClickCanvasSpin(dialog,i,false);
            RequireCanvas(static_cast<int>(GetDlgItemInt(dialog,IDC_EDGE_LEFT+i,NULL,TRUE))==before,
                "actual lower spinner button direction");
        }
        SetDlgItemInt(dialog,IDC_EDGE_LEFT,320,FALSE);
        RequireCanvas(!IsWindowEnabled(GetDlgItem(dialog,IDOK)),"empty crop rejected");
        SetDlgItemTextW(dialog,IDC_EDGE_LEFT,L"");
        RequireCanvas(!IsWindowEnabled(GetDlgItem(dialog,IDOK)),"empty field rejected");
        SetDlgItemInt(dialog,IDC_EDGE_LEFT,12,FALSE);
        RequireCanvas(IsWindowEnabled(GetDlgItem(dialog,IDOK)),"valid crop enabled");
        if(canvasCase==0) CaptureCanvasDialog(dialog,L"canvas-cropping-preview.bmp");
        if(canvasCase==0 || canvasCase==2 || canvasCase==5)
        {
            // Exercise typed minus signs (SetWindowText alone bypasses ES_NUMBER).
            for(int i=0;i<4;++i)
            {
                SetDlgItemTextW(dialog,IDC_EDGE_LEFT+i,L"");
                SendDlgItemMessage(dialog,IDC_EDGE_LEFT+i,WM_CHAR,L'-',1);
                SendDlgItemMessage(dialog,IDC_EDGE_LEFT+i,WM_CHAR,L'1',1);
                RequireCanvas(static_cast<int>(GetDlgItemInt(dialog,IDC_EDGE_LEFT+i,NULL,TRUE))==-1,
                    "negative offsets accepted from keyboard");
                ClickCanvasSpin(dialog,i,i==2);
                RequireCanvas(static_cast<int>(GetDlgItemInt(dialog,IDC_EDGE_LEFT+i,NULL,TRUE))==-2,
                    "spinner can decrease below zero");
            }
            SetDlgItemInt(dialog,IDC_EDGE_LEFT,-12,TRUE);
            SetDlgItemInt(dialog,IDC_EDGE_RIGHT,canvasCase==5?23:-23,TRUE);
            SetDlgItemInt(dialog,IDC_EDGE_TOP,canvasCase==5?34:-34,TRUE);
            SetDlgItemInt(dialog,IDC_EDGE_BOTTOM,-15,TRUE);
            RequireCanvas(IsWindowEnabled(GetDlgItem(dialog,IDOK)),"negative and mixed offsets enabled");
            RequireCanvas(SendDlgItemMessage(dialog,IDC_CANVAS_FILL,CB_GETCOUNT,0,0)==7,"crop fill choices include transparency");
            SendDlgItemMessage(dialog,IDC_CANVAS_FILL,CB_SETCURSEL,4,0);
            SendMessage(dialog,WM_COMMAND,MAKEWPARAM(IDC_CANVAS_FILL,CBN_SELCHANGE),0);
            if(canvasCase==2) CaptureCanvasDialog(dialog,L"canvas-extension-preview.bmp");
        }
    }
    else
    {
        SendDlgItemMessage(dialog,IDC_RELATIVE,BM_CLICK,0,0);
        RequireCanvas(GetDlgItemInt(dialog,IDC_NEW_WIDTH,NULL,TRUE)==0,"relative starts at zero");
        SetDlgItemInt(dialog,IDC_NEW_WIDTH,20,TRUE);
        SetDlgItemInt(dialog,IDC_NEW_HEIGHT,-10,TRUE);
        SendDlgItemMessage(dialog,IDC_RELATIVE,BM_CLICK,0,0);
        RequireCanvas(GetDlgItemInt(dialog,IDC_NEW_WIDTH,NULL,TRUE)==340 &&
            GetDlgItemInt(dialog,IDC_NEW_HEIGHT,NULL,TRUE)==190,"relative converts to absolute");
        SetDlgItemTextW(dialog,IDC_NEW_WIDTH,L"9999999");
        RequireCanvas(!IsWindowEnabled(GetDlgItem(dialog,IDOK)),"oversized input rejected");
        SetDlgItemTextW(dialog,IDC_NEW_WIDTH,L"abc");
        RequireCanvas(!IsWindowEnabled(GetDlgItem(dialog,IDOK)),"non-numeric input rejected");
        SetDlgItemInt(dialog,IDC_NEW_WIDTH,340,FALSE);
        SendDlgItemMessage(dialog,IDC_ANCHOR_FIRST+8,BM_CLICK,0,0);
        SendDlgItemMessage(dialog,IDC_CANVAS_FILL,CB_SETCURSEL,2,0);
        SendMessage(dialog,WM_COMMAND,MAKEWPARAM(IDC_CANVAS_FILL,CBN_SELCHANGE),0);
        if(canvasCase==3) CaptureCanvasDialog(dialog,L"canvas-size-preview.bmp");
    }
    ShowWindow(dialog,SW_HIDE);
    PostMessage(dialog,WM_COMMAND,(canvasCase==0 || canvasCase==3)?IDCANCEL:IDOK,0);
}

static void TestCanvas()
{
    // Exact pixel placement at all nine anchors, enlargement and reduction.
    HDC source=CreateCompatibleDC(NULL);
    HBITMAP src=CreateColorDIB(7,5,RGB(0,0,0));
    HGDIOBJ srcOld=SelectObject(source,src);
    for(int y=0;y<5;++y) for(int x=0;x<7;++x) SetPixelV(source,x,y,RGB(x*20,y*30,120));
    COLORREF fill=RGB(19,237,41);
    for(int a=0;a<9;++a) for(int mode=0;mode<2;++mode)
    {
        int w=mode?4:12,h=mode?2:10;
        int ox=CanvasAnchorOffset(7,w,a%3),oy=CanvasAnchorOffset(5,h,a/3);
        HBITMAP b=MakeCanvasBitmap(source,7,5,w,h,ox,oy,fill);
        RequireCanvas(b!=NULL,"bitmap allocation");
        HDC dc=CreateCompatibleDC(NULL); HGDIOBJ old=SelectObject(dc,b);
        for(int y=0;y<h;++y) for(int x=0;x<w;++x)
        {
            int sx=x-ox,sy=y-oy;
            COLORREF expected=(sx>=0 && sx<7 && sy>=0 && sy<5)?RGB(sx*20,sy*30,120):fill;
            RequireCanvas(GetPixel(dc,x,y)==expected,"anchor pixel placement and fill");
        }
        SelectObject(dc,old); DeleteDC(dc); DeleteObject(b);
    }
    RequireCanvas(!MakeCanvasBitmap(source,7,5,0,5,0,0,fill) &&
        !ValidCanvasSize(32767,32767) && ValidCanvasSize(1,1),"allocation bounds");
    SelectObject(source,srcOld); DeleteObject(src); DeleteDC(source);
    toolsModel.OnEndDraw(FALSE);
    imageModel.Crop(320,200);
    for(int y=0;y<200;++y) for(int x=0;x<320;++x) SetPixelV(imageModel.GetDC(),x,y,RGB(x%256,y,120));
    imageModel.ClearHistory();
    COLORREF atOrigin=GetPixel(imageModel.GetDC(),0,0);
    for(canvasCase=0;canvasCase<6;++canvasCase)
    {
        ticks=0;
        UINT_PTR timer=SetTimer(NULL,0,40,DriveCanvas);
        mainWindow.SendMessage(WM_COMMAND,(canvasCase<3 || canvasCase==5)?IDM_CROP_EDGES:IDM_CANVAS_SIZE);
        KillTimer(NULL,timer);
        if(canvasCase==0 || canvasCase==3)
        {
            RequireCanvas(imageModel.GetWidth()==320 && imageModel.GetHeight()==200 &&
                !imageModel.CanUndo() && GetPixel(imageModel.GetDC(),0,0)==atOrigin,"Cancel preserves document and history");
        }
        else
        {
            int w=canvasCase==1?285:canvasCase==2?355:canvasCase==5?309:340;
            int h=canvasCase==1?151:canvasCase==2?249:canvasCase==5?181:190;
            RequireCanvas(imageModel.GetWidth()==w && imageModel.GetHeight()==h,"dialog applies dimensions");
            COLORREF expected=canvasCase==1?RGB(12,34,120):
                (canvasCase==2 || canvasCase==5)?RGB(128,128,128):RGB(255,255,255);
            RequireCanvas(GetPixel(imageModel.GetDC(),0,0)==expected,"dialog applies offsets and fill");
            if(canvasCase==2 || canvasCase==5)
            {
                int xoff=12,yoff=canvasCase==2?34:-34;
                for(int y=0;y<h;++y) for(int x=0;x<w;++x)
                {
                    int sx=x-xoff,sy=y-yoff;
                    COLORREF pixel=(sx>=0 && sx<320 && sy>=0 && sy<200)?RGB(sx%256,sy,120):RGB(128,128,128);
                    RequireCanvas(GetPixel(imageModel.GetDC(),x,y)==pixel,"expanded/mixed canvas pixel and fill");
                }
            }
            imageModel.Undo();
            RequireCanvas(imageModel.GetWidth()==320 && imageModel.GetHeight()==200 &&
                GetPixel(imageModel.GetDC(),0,0)==atOrigin,"Undo restores canvas");
            imageModel.Redo();
            RequireCanvas(imageModel.GetWidth()==w && imageModel.GetHeight()==h,"Redo reapplies canvas");
            imageModel.Undo(); imageModel.ClearHistory();
        }
    }
    puts("PASS canvas: anchors, real spinner clicks, signed input, expansion/mixed fill, relative size, Cancel, Undo and Redo");
}
