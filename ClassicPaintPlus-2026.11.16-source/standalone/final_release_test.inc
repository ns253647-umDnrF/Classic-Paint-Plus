static int finalCanvasRoute=0,finalFillChoice=-1;
static bool finalTransparent=false;
static VOID CALLBACK DriveFinalCanvas(HWND,UINT,UINT_PTR timer,DWORD)
{
    HWND dialog=FindJpegDialogControl(finalCanvasRoute==2?IDD_ATTRIBUTESEDIT1:finalCanvasRoute==3?IDC_NEW_WIDTH:IDC_EDGE_LEFT);
    if(!dialog)return;KillTimer(NULL,timer);
    if(finalCanvasRoute==2)
    {
        SendDlgItemMessageW(dialog,IDD_ATTRIBUTESRB3,BM_CLICK,0,0);
        for(int id:{IDD_ATTRIBUTESEDIT1,IDD_ATTRIBUTESEDIT2})
        {
            SendDlgItemMessageW(dialog,id,EM_SETSEL,0,-1);
            SendDlgItemMessageW(dialog,id,EM_REPLACESEL,TRUE,LPARAM(id==IDD_ATTRIBUTESEDIT1?L"100":L"80"));
        }
    }
    else
    {
        RequireCanvas(SendDlgItemMessageW(dialog,IDC_CANVAS_FILL,CB_GETCURSEL,0,0)==(finalTransparent?6:1),"both canvas dialogs default to the document replacement policy");
        if(finalCanvasRoute==3)
        {
            SendDlgItemMessageW(dialog,IDC_ANCHOR_FIRST,BM_CLICK,0,0);
            SetDlgItemInt(dialog,IDC_NEW_WIDTH,100,FALSE);SetDlgItemInt(dialog,IDC_NEW_HEIGHT,80,FALSE);
        }
        else
        {
            SetDlgItemInt(dialog,IDC_EDGE_RIGHT,-20,TRUE);SetDlgItemInt(dialog,IDC_EDGE_BOTTOM,-20,TRUE);
        }
        if(finalFillChoice>=0)
        {
            SendDlgItemMessageW(dialog,IDC_CANVAS_FILL,CB_SETCURSEL,finalFillChoice,0);
            SendMessageW(dialog,WM_COMMAND,MAKEWPARAM(IDC_CANVAS_FILL,CBN_SELCHANGE),LPARAM(GetDlgItem(dialog,IDC_CANVAS_FILL)));
        }
    }
    PostMessageW(dialog,WM_COMMAND,IDOK,0);
}
static void TestCanvasBackgroundPolicy()
{
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);mainWindow.SetWindowPos(NULL,0,0,1100,850,SWP_NOACTIVATE|SWP_NOZORDER);
    toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();toolsModel.SetActiveTool(TOOL_PEN);canvasWindow.zoomTo(1000);
    for(int alpha:{0,1})for(int replace:{0,1})for(finalCanvasRoute=0;finalCanvasRoute<5;++finalCanvasRoute)
    {
        RasterImage original;original.width=80;original.height=60;original.pixels.resize(80*60);
        for(int y=0;y<60;++y)for(int x=0;x<80;++x)original.pixels[y*80+x]=(DWORD(alpha && x<8?0:alpha && x<16?128:255)<<24)|DWORD(x*3)<<16|DWORD(y*4)<<8|67;
        HBITMAP bitmap=MakeRasterBitmap(original);ResetDocumentAlpha(bitmap);imageModel.PushImageForUndo(bitmap);imageModel.ClearHistory();
        DocumentAlpha().enabled=alpha!=0;DocumentAlpha().backdrop=RGB(140,180,220);
        registrySettings.ReplaceDeletedWithAlpha=replace;paletteModel.SetBgColor(RGB(22,57,103),0);
        finalTransparent=alpha && replace;finalFillChoice=-1;
        canvasWindow.updateScrollRange();canvasWindow.ScrollToCorner(0);
        if(finalCanvasRoute==0)imageModel.Crop(100,80);
        else if(finalCanvasRoute==1)
        {
            CRect r(0,0,80,60);canvasWindow.ImageToCanvas(r);
            LPARAM first=MAKELPARAM(r.right+1,r.bottom+1),last=MAKELPARAM(r.right+21,r.bottom+21);
            canvasWindow.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,first);
            canvasWindow.SendMessage(WM_MOUSEMOVE,MK_LBUTTON,last);canvasWindow.SendMessage(WM_LBUTTONUP,0,last);
        }
        else
        {
            UINT_PTR timer=SetTimer(NULL,0,30,DriveFinalCanvas);
            mainWindow.SendMessage(WM_COMMAND,finalCanvasRoute==2?IDM_IMAGEATTRIBUTES:finalCanvasRoute==3?IDM_CANVAS_SIZE:IDM_CROP_EDGES);
            KillTimer(NULL,timer);
        }
        RasterImage result=DocumentRaster();
        printf("Canvas policy alpha=%d replace=%d route=%d size=%dx%d\n",alpha,replace,finalCanvasRoute,result.width,result.height);
        RequireCanvas(result.width==100 && result.height==80,"all native canvas enlargement routes apply the requested size");
        for(int y=0;y<80;++y)for(int x=0;x<100;++x)
        {
            DWORD p=result.pixels[y*100+x];
            RequireCanvas(x<80 && y<60?p==original.pixels[y*80+x]:finalTransparent?(p>>24)==0:p==0xff163967,"old RGBA pixels stay exact; new area uses alpha only when enabled and checked, otherwise opaque background RGB");
        }
        imageModel.Undo();RequireCanvas(DocumentRaster().pixels==original.pixels,"canvas undo restores exact source RGBA");
        imageModel.Redo();RequireCanvas(DocumentRaster().pixels==result.pixels,"canvas redo restores exact fill and alpha");
    }
    for(int route:{3,4})for(int choice:{2,6})
    {
        finalCanvasRoute=route;finalFillChoice=choice;
        HBITMAP bitmap=CreateColorDIB(80,60,RGB(13,45,78));ResetDocumentAlpha(bitmap);imageModel.PushImageForUndo(bitmap);
        DocumentAlpha().enabled=true;registrySettings.ReplaceDeletedWithAlpha=TRUE;finalTransparent=true;
        UINT_PTR timer=SetTimer(NULL,0,30,DriveFinalCanvas);mainWindow.SendMessage(WM_COMMAND,finalCanvasRoute==3?IDM_CANVAS_SIZE:IDM_CROP_EDGES);KillTimer(NULL,timer);
        DWORD pixel=RasterPixel(imageModel.GetDC(),99,79);RequireCanvas(finalFillChoice==6?(pixel>>24)==0:pixel==0xffffffff,"explicit canvas fill choice overrides the default");
    }
    paletteModel.SetBgColor(RGB(255,255,255),255);InitializeImage(NULL,NULL,FALSE);
    puts("PASS canvas background policy: mouse, Attributes, offsets, both dialogs, all alpha/checkbox combinations, exact RGBA, explicit overrides, Undo/Redo");
}
static void TestRegularFivePointStar()
{
    auto points=FigureGeometry(FIG_STAR5,64,0)[0];RequireCanvas(points.size()==10,"five-point star has ten corners");
    auto cross=[](FigureVertex a,FigureVertex b,FigureVertex c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);};
    double length=hypot(points[1].x-points[0].x,points[1].y-points[0].y);
    for(int i=0;i<10;++i)
    {
        RequireCanvas(fabs(hypot(points[(i+1)%10].x-points[i].x,points[(i+1)%10].y-points[i].y)-length)<1e-12,"all ten pentagram edges are equal");
        if(!(i&1))RequireCanvas(fabs(cross(points[i],points[(i+4)%10],points[(i+1)%10]))<1e-12 && fabs(cross(points[(i+2)%10],points[(i+8)%10],points[(i+1)%10]))<1e-12,"each valley lies exactly on both straight pentagram diagonals");
    }
    auto negative=FigureGeometry(FIG_STAR5,64,-50)[0],positive=FigureGeometry(FIG_STAR5,64,50)[0];
    RequireCanvas(hypot(negative[1].x-.5,negative[1].y-.5)<hypot(points[1].x-.5,points[1].y-.5) && hypot(positive[1].x-.5,positive[1].y-.5)>hypot(points[1].x-.5,points[1].y-.5),"signed star bend still works around the regular default");
    TestBitmap image(360,360,RGB(255,255,255));RECT r={20,20,340,340};
    DrawFigure(image.dc,FIG_STAR5,r,(HBRUSH)GetStockObject(BLACK_BRUSH),(HBRUSH)GetStockObject(LTGRAY_BRUSH),2,1,0,0,0,0,0);
    SaveShapeSheet(image.bitmap,L"final-release-tests/regular-star.bmp");
    puts("PASS regular five-point star: equal edges, straight diagonals, signed bend and native rendering");
}
static HMENU FinalCommandMenu(HMENU menu,UINT command)
{
    for(int i=0;i<GetMenuItemCount(menu);++i)
    {
        if(GetMenuItemID(menu,i)==command)return menu;
        if(HMENU child=GetSubMenu(menu,i))if(HMENU found=FinalCommandMenu(child,command))return found;
    }
    return NULL;
}
static void TestFinalMenus()
{
    if(testLanguages.empty())EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    for(WORD lang:testLanguages)
    {
        registrySettings.UILanguage=lang;ApplyPreferredLanguage();mainWindow.ReloadLanguage();
        auto caption=ExactString(lang,IDS_ALPHA_REPLACE);RequireCanvas(!caption.IsEmpty(),"canvas replacement label exists in every language");
        if(PRIMARYLANGID(lang)==LANG_RUSSIAN)RequireCanvas(caption==L"Замещать фон на холсте","exact renamed Russian canvas setting");
        HMENU settings=GetSubMenu(mainWindow.GetMenu(),4),palette=FinalCommandMenu(settings,IDM_COLORSOLDPALETTE);
        RequireCanvas(palette && GetMenuItemID(palette,0)==IDM_COLORSOLDPALETTE && GetMenuItemID(palette,1)==IDM_COLORSMODERNPALETTE,"Classic palette comes first in every language");
        HACCEL table=LoadAcceleratorsW(g_hinstExe,MAKEINTRESOURCEW(ID_ACCELERATORS));
        int count=CopyAcceleratorTableW(table,NULL,0);std::vector<ACCEL> entries(count);CopyAcceleratorTableW(table,entries.data(),count);
        for(int i=0;i<count;++i)for(int j=0;j<i;++j)RequireCanvas(entries[i].key!=entries[j].key || (entries[i].fVirt&0x1d)!=(entries[j].fVirt&0x1d),"localized shortcuts have no conflicting key bindings");
        DestroyAcceleratorTable(table);
    }
    registrySettings.UILanguage=MAKELANGID(LANG_RUSSIAN,SUBLANG_DEFAULT);ApplyPreferredLanguage();mainWindow.ReloadLanguage();
    WritePrivateProfileStringW(L"Settings",L"PaletteType",NULL,RegistrySettings::ConfigPath());
    RegistrySettings defaults;defaults.Load(SW_HIDE);RequireCanvas(defaults.PaletteType==PAL_OLDTYPE,"new installations default to Classic palette");
    for(int cycle=0;cycle<8;++cycle)
    {
        registrySettings.DarkTheme=cycle%2;ApplyTheme();
        registrySettings.strFiles[0].Format(L"C:\\Тест\\image-%d.png",cycle);registrySettings.strFiles[1]=L"C:\\Тест\\second.png";
        HMENU menu=GetSubMenu(mainWindow.GetMenu(),0);mainWindow.SendMessage(WM_INITMENUPOPUP,WPARAM(menu),0);
        MENUITEMINFOW item={sizeof(item)};WCHAR label[512]={};item.fMask=MIIM_FTYPE|MIIM_DATA|MIIM_STRING;item.dwTypeData=label;item.cch=_countof(label);
        RequireCanvas(GetMenuItemInfoW(menu,IDM_FILE1,FALSE,&item) && label[0],"recent filename is rebuilt before display");
        if(!(item.fType&MFT_OWNERDRAW)) {RequireCanvas(!registrySettings.DarkTheme,"native recent entries are only used in the light theme");continue;}
        RequireCanvas((item.fType&MFT_OWNERDRAW) && item.dwItemData,"dynamic recent files receive themed drawing after insertion");
        TestBitmap sample(360,28,GetSysColor(COLOR_MENU));DRAWITEMSTRUCT draw={};draw.CtlType=ODT_MENU;draw.itemID=IDM_FILE1;draw.itemData=item.dwItemData;
        draw.hwndItem=HWND(menu);draw.hDC=sample.dc;draw.rcItem={0,0,360,28};draw.itemAction=ODA_DRAWENTIRE;
        RequireCanvas(mainWindow.SendMessage(WM_DRAWITEM,0,LPARAM(&draw))!=0,"MRU uses the theme's owner-draw handler");
        int visible=0;for(int y=0;y<28;++y)for(int x=27;x<338;++x){int red=GetRValue(GetPixel(sample.dc,x,y));if(registrySettings.DarkTheme?red>180:red<100)++visible;}
        RequireCanvas(visible>40,"MRU text is light on dark and dark on light after repeated rebuilds");
        CStringW path;path.Format(L"final-release-tests/recent-%d.bmp",cycle);SaveShapeSheet(sample.bitmap,path);
    }
    puts("PASS all 30 localized canvas/palette menus, non-conflicting accelerators, Classic default, and live Recent Files rendering across repeated theme changes");
}
static void TestFinalRelease()
{
    CreateDirectoryW(L"final-release-tests",NULL);TestCanvasBackgroundPolicy();TestRegularFivePointStar();TestFinalMenus();
}
