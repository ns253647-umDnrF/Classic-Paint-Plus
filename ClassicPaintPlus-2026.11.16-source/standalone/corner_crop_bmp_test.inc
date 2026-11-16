static void TestCornerCropPixels()
{
    CreateDirectoryW(L"corner-crop-tests",NULL);
    RasterImage source;source.width=128;source.height=96;source.pixels.resize(128*96);
    for(int y=0;y<96;++y) for(int x=0;x<128;++x) source.pixels[y*128+x]=(DWORD((x+y)%3==0?0:(x+y)%3==1?128:255)<<24)|DWORD(x*2)<<16|DWORD(y*2)<<8|DWORD(x+y);
    for(int mode=0;mode<3;++mode) for(int corner=0;corner<4;++corner)
    {
        toolsModel.OnEndDraw(TRUE);imageModel.PushImageForUndo(MakeRasterBitmap(source));imageModel.ClearHistory();
        toolsModel.SetActiveTool(mode?TOOL_FREESEL:TOOL_RECTSEL);toolsModel.SetBackgroundTransparent(FALSE);
        DocumentAlpha().enabled=true;DocumentAlpha().backdrop=RGB(12,50,99);
        HBITMAP mask=NULL;
        if(mode)
        {
            mask=CreateMonoBitmap(53,41,FALSE);HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,mask),brush=SelectObject(dc,GetStockObject(WHITE_BRUSH));
            POINT triangle[]={{0,0},{52,20},{0,40}};Polygon(dc,triangle,3);SelectObject(dc,brush);SelectObject(dc,old);DeleteDC(dc);
        }
        selectionModel.setMask(CRect(20,15,73,56),mask);selectionModel.m_bShow=TRUE;
        if(mode==1) { selectionModel.TakeOff();selectionModel.moveSelection(7,9);selectionModel.SetTransparency(35); }
        if(mode==2) { selectionModel.SetRotationAngle(32);selectionModel.moveSelection(-25,-20); }
        CRect bounds=selectionModel.DrawingBounds();int l=corner<2?max(0L,bounds.left):0,t=corner%2?0:max(0L,bounds.top);
        int r=corner<2?128:min(128L,bounds.right),b=corner%2?min(96L,bounds.bottom):96;
        if(!mode) RequireCanvas(l==(corner<2?20:0) && t==(corner%2?0:15) && r==(corner<2?128:73) && b==(corner%2?56:96),"corner sides map to left/top, left/bottom, right/top, right/bottom");
        TestBitmap preview(128,96,RGB(255,255,255));BitBlt(preview.dc,0,0,128,96,imageModel.GetDC(),0,0,SRCCOPY);toolsModel.OnDrawOverlayOnImage(preview.dc);
        RasterImage visible;ReadRaster(preview.bitmap,visible);std::vector<DWORD> expected;
        for(int y=t;y<b;++y) for(int x=l;x<r;++x) expected.push_back(visible.pixels[y*128+x]);
        mainWindow.SendMessage(WM_COMMAND,IDM_CROP_CORNER_FIRST+corner,0);auto actual=DocumentRaster();
        RequireCanvas(actual.width==r-l && actual.height==b-t && actual.pixels==expected,"corner crop retains correct side, exact RGBA and current moved/rotated masked selection");
        RequireCanvas(!selectionModel.m_bShow && DocumentAlpha().enabled && DocumentAlpha().backdrop==RGB(12,50,99),"corner crop finishes selection and keeps transparency metadata");
        imageModel.Undo();RequireCanvas(DocumentRaster().pixels==source.pixels && !imageModel.CanUndo(),"corner crop undoes in one operation");
        imageModel.Redo();RequireCanvas(DocumentRaster().pixels==expected,"corner crop redo restores exact result");
    }
    toolsModel.OnEndDraw(TRUE);imageModel.PushImageForUndo(MakeRasterBitmap(source));imageModel.ClearHistory();toolsModel.SetActiveTool(TOOL_RECTSEL);
    RECT area;RequireCanvas(!SelectionCornerCropRect(3,area) && !CropCanvasAtSelectionCorner(3),"corner crop unavailable without selection");
    selectionModel.setMask(CRect(140,100,150,110),NULL);selectionModel.m_bShow=TRUE;
    RequireCanvas(!SelectionCornerCropRect(0,area) && !CropCanvasAtSelectionCorner(0) && !imageModel.CanUndo(),"empty out-of-canvas crop is rejected without modifying document");
    selectionModel.setMask(CRect(20,15,73,56),NULL);selectionModel.m_bShow=TRUE;
    puts("PASS all 4 corner crops: exact retained rectangle/RGBA, moved/free/rotated selections, clipping, invalid sizes and single-step Undo/Redo");
}
static int cornerMenuTicks=0;static bool cornerMenuSeen=false;
static VOID CALLBACK CheckCornerContext(HWND,UINT,UINT_PTR timer,DWORD)
{
    RequireCanvas(++cornerMenuTicks<60,"corner context menu appears");HWND window=FindWindowExW(NULL,NULL,L"#32768",NULL);if(!window) return;
    HMENU menu=HMENU(SendMessage(window,0x01e1,0,0));if(!menu) return;KillTimer(NULL,timer);
    int first=-1;for(int i=0;i<GetMenuItemCount(menu);++i) if(GetMenuItemID(menu,i)==IDM_CROPSELECTION) first=i;
    RequireCanvas(first>=0,"context has existing contour crop");
    for(int i=0;i<4;++i)
    {
        WCHAR text[256];GetMenuStringW(menu,IDM_CROP_CORNER_FIRST+i,text,_countof(text),MF_BYCOMMAND);
        RequireCanvas(GetMenuItemID(menu,first+i+1)==IDM_CROP_CORNER_FIRST+i && ExactString(WORD(registrySettings.UILanguage),IDS_CV_CROP_CORNER_FIRST+i)==text && !(GetMenuState(menu,IDM_CROP_CORNER_FIRST+i,MF_BYCOMMAND)&MF_GRAYED),"four localized enabled corner crops follow contour crop in selection context menu");
    }
    CaptureCanvasDialog(window,L"corner-crop-tests/context.bmp");cornerMenuSeen=true;EndMenu();
}
static void TestCornerMenus()
{
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);mainWindow.SendMessage(WM_COMMAND,LanguageCommand(L"Russian"),0);
    UINT_PTR timer=SetTimer(NULL,0,30,CheckCornerContext);mainWindow.TrackPopupMenu(POINT{170,120},0);KillTimer(NULL,timer);RequireCanvas(cornerMenuSeen,"native selection menu verified");
    if(testLanguages.empty()) EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    WORD old=WORD(registrySettings.UILanguage);
    for(WORD lang:testLanguages)
    {
        registrySettings.UILanguage=lang;ApplyPreferredLanguage();HMENU menu=LoadMenuW(g_hinstExe,MAKEINTRESOURCEW(ID_MENU));BuildCanvasMenu(menu);
        HMENU drawing=GetSubMenu(menu,3),canvas=NULL;
        for(int i=0;i<GetMenuItemCount(drawing);++i) { HMENU sub=GetSubMenu(drawing,i);if(sub && GetMenuItemID(sub,0)==IDM_IMAGECROP) canvas=sub; }
        RequireCanvas(canvas!=NULL,"canvas submenu exists in every locale");
        for(int i=0;i<4;++i)
        {
            WCHAR text[256];GetMenuStringW(canvas,i+1,text,_countof(text),MF_BYPOSITION);CStringW expected=ExactString(lang,IDS_CV_CROP_CORNER_FIRST+i);
            RequireCanvas(!expected.IsEmpty() && expected==text && GetMenuItemID(canvas,i+1)==IDM_CROP_CORNER_FIRST+i,"native canvas menu uses translated corner captions in correct order");
        }
        DestroyMenu(menu);
    }
    registrySettings.UILanguage=old;ApplyPreferredLanguage();toolsModel.OnEndDraw(TRUE);
    puts("PASS native selection context and canvas menus: 4 commands in order, all 30 compiled translations");
}

static int bmpFilterChoice=1,bmpSaveTicks=0,bmpSaveStage=0;static bool bmpSaveAccept=false,bmpSavePosted=false;
static VOID CALLBACK DriveBmpOrderSave(HWND,UINT,UINT_PTR timer,DWORD)
{
    RequireCanvas(++bmpSaveTicks<80,"native BMP save dialog completes");HWND hook=FindSaveHook();if(!hook) return;
    if(bmpSavePosted) return;
    HWND dialog=GetParent(hook),combo=GetDlgItem(dialog,cmb1);const UINT labels[]={IDS_BMP_24+1,IDS_BMP_24+2,IDS_BMP_24+3,IDS_BMP_24};
    if(bmpSaveStage==1)
    {
        // Allow the native combo's type-change notifications to settle before Save.
        WCHAR filename[64];swprintf_s(filename,L"order-%d.bmp",bmpFilterChoice-1);
        SendMessageW(dialog,CDM_SETCONTROLTEXT,edt1,LPARAM(filename));
        bmpSavePosted=true;printf("BMP posting %s choice=%d\n",bmpSaveAccept?"Save":"Cancel",bmpFilterChoice);PostMessage(dialog,WM_COMMAND,bmpSaveAccept?IDOK:IDCANCEL,0);return;
    }
    for(int i=0;i<4;++i)
    {
        WCHAR text[256];SendMessageW(combo,CB_GETLBTEXT,i,LPARAM(text));CStringW expected;expected.LoadString(g_hinstExe,labels[i]);
        RequireCanvas(wcsstr(text,expected)==text,"BMP list order is monochrome, 16-colour, 256-colour, 24-bit");
    }
    auto ofn=reinterpret_cast<OPENFILENAMEW*>(GetWindowLongPtr(hook,DWLP_USER));
    RequireCanvas(ofn->nFilterIndex==DWORD(bmpSaveAccept?4:bmpFilterChoice),"BMP initial selection uses actual depth; new BMP defaults to 24-bit");
    SendMessage(combo,CB_SETCURSEL,bmpFilterChoice-1,0);SendMessage(dialog,WM_COMMAND,MAKEWPARAM(cmb1,CBN_SELCHANGE),LPARAM(combo));SendMessage(dialog,WM_COMMAND,MAKEWPARAM(cmb1,CBN_SELENDOK),LPARAM(combo));
    if(bmpFilterChoice==4) CaptureCanvasDialog(dialog,L"corner-crop-tests/bmp-save.bmp");
    bmpSaveStage=1;
}
static void TestBmpFilterOrder()
{
    WCHAR root[MAX_PATH];GetCurrentDirectoryW(_countof(root),root);TestBitmap sample(32,24,RGB(80,155,210));
    const int depths[]={1,4,8,24};const SaveFormat formats[]={SaveFormat::Bmp1,SaveFormat::Bmp4,SaveFormat::Bmp8,SaveFormat::Bmp24};
    for(int i=0;i<4;++i)
    {
        RequireCanvas(SaveFormatFromFilterIndex(i+1)==formats[i] && SaveFormatFilterIndex(formats[i])==UINT(i+1),"BMP filter mapping is bidirectional");
        bmpFilterChoice=i+1;bmpSaveTicks=bmpSaveStage=0;bmpSaveAccept=true;bmpSavePosted=false;SetCurrentSaveFormat(SaveFormat::Png);
        WCHAR file[MAX_PATH];swprintf_s(file,L"%s\\corner-crop-tests\\order-%d.bmp",root,i);DeleteFileW(file);
        UINT_PTR timer=SetTimer(NULL,0,40,DriveBmpOrderSave);RequireCanvas(mainWindow.GetSaveFileName(file,_countof(file)),"native reordered BMP choice is accepted");KillTimer(NULL,timer);SetCurrentDirectoryW(root);
        printf("BMP dialog accepted choice=%d\n",bmpFilterChoice);
        RequireCanvas(SaveDIBToFile(sample.bitmap,file,FALSE),"chosen BMP filter saves successfully");std::vector<BYTE> bytes;RequireCanvas(ReadImageBytes(file,bytes) && bytes.size()>30 && bytes[28]==depths[i],"each displayed BMP option produces its corresponding bit depth");
        printf("BMP file checked choice=%d\n",bmpFilterChoice);
        bmpSaveTicks=bmpSaveStage=0;bmpSaveAccept=false;bmpSavePosted=false;SetCurrentSaveFormat(formats[i]);timer=SetTimer(NULL,0,40,DriveBmpOrderSave);
        RequireCanvas(!mainWindow.GetSaveFileName(file,_countof(file)),"cancel leaves existing BMP untouched");KillTimer(NULL,timer);SetCurrentDirectoryW(root);
    }
    SetCurrentSaveFormat(SaveFormat::Png);
    puts("PASS native BMP order, new-file 24-bit default, remembered depth, all 4 real files checked for 1/4/8/24-bit encoding");
}
static void TestCornerCropAndBmpOrder() { TestCornerCropPixels();TestCornerMenus();TestBmpFilterOrder(); }
