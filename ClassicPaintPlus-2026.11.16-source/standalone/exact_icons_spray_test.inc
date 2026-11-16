static void TestExactArtwork()
{
    CreateDirectoryW(L"exact-icons-spray-tests",NULL);
    for(int percent:{150,200,250})
    {
        int side=16*percent/100;UINT id=percent==150?IDR_EXACT_TOOLS_150:percent==200?IDR_EXACT_TOOLS_200:IDR_EXACT_TOOLS_250;
        HRSRC res=FindResourceW(g_hinstExe,MAKEINTRESOURCEW(id),RT_RCDATA);RequireCanvas(res!=NULL,"supplied icon bitmap is embedded");
        std::vector<BYTE> pixels;unsigned width,height;
        RequireCanvas(!lodepng::decode(pixels,width,height,static_cast<const BYTE*>(LockResource(LoadResource(g_hinstExe,res))),SizeofResource(g_hinstExe,res)),"embedded artwork decodes");
        RequireCanvas(width==unsigned(side*10) && height==unsigned(side),"artwork is already sized for image list");
        for(int dark:{0,1})
        {
            registrySettings.DarkTheme=dark;HIMAGELIST icons=CreatePanelToolIcons(percent);
            TestBitmap actual(side*10,side,RGB(170,170,170));
            for(int i=0;i<10;++i) ImageList_Draw(icons,i,actual.dc,i*side,0,ILD_TRANSPARENT);ImageList_Destroy(icons);GdiFlush();
            CStringW file;file.Format(L"exact-icons-spray-tests/artwork-%d-%d.png",percent,dark);CImage image;image.Attach(actual.bitmap);image.Save(file);image.Detach();
            int largest=0,visible=0;
            for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x)
            {
                BYTE* p=&pixels[(size_t(y)*width+x)*4];COLORREF c=GetPixel(actual.dc,x,y);
                int channel[]={GetRValue(c),GetGValue(c),GetBValue(c)};if(p[3]) ++visible;
                bool white=dark && (x/side==0 || x/side==1 || x/side==9);
                if(dark && x/side==3 && x%side<side*2/5 && max(p[0],max(p[1],p[2]))-min(p[0],min(p[1],p[2]))<24)
                {
                    int original=(int(p[0])*p[3]+170*(255-p[3])+127)/255;
                    int tinted=(255*p[3]+170*(255-p[3])+127)/255;
                    white=abs(channel[0]-tinted)<abs(channel[0]-original);
                }
                for(int k=0;k<3;++k) { int delta=abs(channel[k]-((white?255:p[k])*p[3]+170*(255-p[3])+127)/255);if(delta>largest) { largest=delta;if(largest>50) printf("pixel %u,%u rgba=%d,%d,%d,%d actual=%d,%d,%d\n",x,y,p[0],p[1],p[2],p[3],channel[0],channel[1],channel[2]); } }
            }
            if(largest>2) printf("Artwork mismatch: percent=%d dark=%d maxdelta=%d\n",percent,dark,largest);
            RequireCanvas(visible>300 && largest<=2,"original raster/alpha preserved; dark theme tints only selection, text and pouring paint white");
        }
    }
    registrySettings.DarkTheme=0;
    puts("PASS original raster artwork in all 3 enlarged scales and both themes; dark selection/text white with exact original alpha, other colours unchanged");
}

static void TestSprayRendering()
{
    const int radii[]={5,8,3,12,10,14,16,16};
    TestBitmap gallery(8*130,3*130,RGB(255,255,255));
    for(int i=0;i<8;++i)
    {
        int radius=radii[i],style=i<4?0:i-3;std::vector<DWORD> previous;
        for(int smooth:{0,50,100})
        {
            TestBitmap sample(128,128,RGB(255,255,255));srand(1832);
            SmoothSpray(sample.dc,64,64,RGB(24,80,160),radius,style,smooth,255);
            RasterImage pixels;ReadRaster(sample.bitmap,pixels);int changed=0,partial=0;
            for(DWORD color:pixels.pixels) { if(color!=0xffffffff) ++changed;if((color&0xffffff)!=0xffffff && (color&0xffffff)!=0x1850a0) ++partial; }
            RequireCanvas(changed>0 && (smooth?partial>0:partial==0),"0% preserves hard dots; smoothing produces intermediate coverage for every spray");
            RequireCanvas(previous.empty() || previous!=pixels.pixels,"50% and 100% generate different smoothing");previous=pixels.pixels;
            if(!smooth)
            {
                TestBitmap legacy(128,128,RGB(255,255,255));srand(1832);
                if(style) SprayPattern(legacy.dc,64,64,RGB(24,80,160),radius,style,255);else Airbrush(legacy.dc,64,64,RGB(24,80,160),radius);
                RequireCanvas(FigurePixels(legacy.dc)==FigurePixels(sample.dc),"all original random dot positions are unchanged at 0%");
            }
            BitBlt(gallery.dc,i*130,smooth/50*130,128,128,sample.dc,0,0,SRCCOPY);
            RequireCanvas(GetPixel(sample.dc,1,1)==RGB(255,255,255),"smoothing never affects unrelated canvas pixels");
        }
    }
    CImage image;image.Attach(gallery.bitmap);image.Save(L"exact-icons-spray-tests/smoothing.png");image.Detach();
    for(BYTE opacity:{BYTE(0),BYTE(128),BYTE(255)})
    {
        TestBitmap sample(128,128,RGB(70,80,90));
        if(opacity) { DIBSECTION dib={};GetObject(sample.bitmap,sizeof(dib),&dib);GdiFlush();for(int i=0;i<128*128;++i) static_cast<DWORD*>(dib.dsBm.bmBits)[i]|=0xff000000; }
        srand(22);SmoothSpray(sample.dc,64,64,RGB(20,60,120),12,0,75,opacity);
        RasterImage pixels;ReadRaster(sample.bitmap,pixels);int partial=0;
        for(DWORD p:pixels.pixels) if((p>>24)>0 && (p>>24)<255) ++partial;
        RequireCanvas(partial>0,"smoothed spray both paints and erases real partial alpha");
    }
    // Exercise the real tool/history path; dirty bounds must include the blurred edge.
    toolsModel.OnEndDraw(TRUE);toolsModel.SetActiveTool(TOOL_AIRBRUSH);registrySettings.SpraySmoothing=100;
    paletteModel.SetFgColor(RGB(0,0,0));paletteModel.SetBgColor(RGB(255,0,0));
    for(int style=0;style<=4;++style) for(BOOL left:{TRUE,FALSE})
    {
        registrySettings.SprayStyle=style;toolsModel.SetAirBrushRadius(16);
        imageModel.PushImageForUndo(CreateColorDIB(128,128,RGB(255,255,255)));imageModel.ClearHistory();auto blank=DocumentRaster();
        toolsModel.OnButtonDown(left,64,64,FALSE);toolsModel.OnMouseMove(left,70,72);
        TestBitmap a(128,128,RGB(255,255,255)),b(128,128,RGB(255,255,255));toolsModel.OnDrawOverlayOnImage(a.dc);toolsModel.OnDrawOverlayOnImage(b.dc);
        RequireCanvas(FigurePixels(a.dc)==FigurePixels(b.dc),"smoothed preview stays stable on repeated repaint");
        toolsModel.OnButtonUp(left,70,72);auto committed=DocumentRaster();RequireCanvas(committed.pixels!=blank.pixels,"smoothed stroke commits");
        imageModel.Undo();RequireCanvas(DocumentRaster().pixels==blank.pixels && !imageModel.CanUndo(),"one undo removes entire smoothed stroke including outer blur");
        imageModel.Redo();RequireCanvas(DocumentRaster().pixels==committed.pixels,"redo restores exact smoothed stroke and alpha");
    }
    registrySettings.SpraySmoothing=0;registrySettings.SprayStyle=0;toolsModel.SetAirBrushRadius(5);
    puts("PASS 8 spray presets at 0/50/100%, legacy dot compatibility, stable preview, partial alpha, both mouse colours, full Undo/Redo");
}

static void TestSprayControls()
{
    mainWindow.ShowWindow(SW_SHOWNOACTIVATE);mainWindow.SetWindowPos(NULL,0,0,1100,900,SWP_NOACTIVATE|SWP_NOZORDER);
    toolsModel.SetActiveTool(TOOL_AIRBRUSH);HWND slider=toolSettingsWindow.GetDlgItem(IDC_SPRAY_SMOOTHING),edit=toolSettingsWindow.GetDlgItem(IDC_SPRAY_SMOOTHING_EDIT);
    TestIniKey ini;ini.DeleteValue(L"SpraySmoothing");RegistrySettings loaded;loaded.Load(SW_HIDE);RequireCanvas(!loaded.SpraySmoothing,"missing smoothing setting defaults to original 0%");
    const POINT centers[]={{13,21},{31,21},{11,51},{29,51},{16,88},{34,88},{16,118},{34,118}};const int radii[]={5,8,3,12,10,14,16,16};
    for(int percent:{100,150,200,250}) for(int dark:{0,1})
    {
        registrySettings.PanelMagnification=percent;registrySettings.DarkTheme=dark;ApplyTheme();ApplyPanelScale();
        for(int i=0;i<8;++i)
        {
            toolSettingsWindow.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(PanelPx(centers[i].x),PanelPx(centers[i].y)));ReleaseCapture();
            RequireCanvas(registrySettings.SprayStyle==DWORD(i<4?0:i-3) && toolsModel.GetAirBrushRadius()==radii[i],"all four classic and four new presets work at every scale and theme");
        }
        for(HWND control:{slider,edit})
        {
            RECT r,p;GetWindowRect(control,&r);MapWindowPoints(NULL,toolSettingsWindow,reinterpret_cast<POINT*>(&r),2);toolSettingsWindow.GetClientRect(&p);
            RequireCanvas(IsWindowVisible(control) && r.left>=0 && r.right<=p.right && r.bottom<=p.bottom,"smoothing controls fit inside scrollable panel at all scales");
        }
        SendMessage(slider,TBM_SETPOS,TRUE,33);toolSettingsWindow.SendMessage(WM_VSCROLL,TB_THUMBPOSITION,LPARAM(slider));
        RequireCanvas(registrySettings.SpraySmoothing==67 && WindowLabel(edit)==L"67%","slider updates percentage immediately");
        EnterObjectValue(edit,L"42");RequireCanvas(registrySettings.SpraySmoothing==42 && SendMessage(slider,TBM_GETPOS,0,0)==58,"manual percent updates slider");
        SetFocus(edit);RequireCanvas(WindowLabel(edit)==L"42","percent suffix disappears on focus");canvasWindow.SetFocus();
        SetFocus(edit);SetWindowTextW(edit,L"101");canvasWindow.SetFocus();RequireCanvas(WindowLabel(edit)==L"42%" && registrySettings.SpraySmoothing==42,"invalid percentage restores valid value");
        loaded.Load(SW_HIDE);RequireCanvas(loaded.SpraySmoothing==42,"smoothing preference survives INI reload");
        CStringW file;file.Format(L"exact-icons-spray-tests/panel-%d-%d.bmp",percent,dark);CaptureCanvasDialog(toolSettingsWindow,file);
        file.Format(L"exact-icons-spray-tests/tools-%d-%d.bmp",percent,dark);CaptureCanvasDialog(toolBoxContainer,file);
    }
    if(testLanguages.empty()) EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    for(WORD lang:testLanguages) RequireCanvas(!ExactString(lang,IDS_SPRAY_SMOOTHING).IsEmpty(),"spray smoothing translated in every language");
    toolsModel.SetActiveTool(TOOL_TEXT);RequireCanvas(!IsWindowVisible(slider) && !IsWindowVisible(edit),"spray controls disappear on tool switch");
    registrySettings.PanelMagnification=100;registrySettings.DarkTheme=0;registrySettings.SpraySmoothing=0;registrySettings.SprayStyle=0;toolsModel.SetAirBrushRadius(5);ApplyTheme();ApplyPanelScale();toolsModel.SetActiveTool(TOOL_RECTSEL);
    puts("PASS restored preset layout, smoothing slider/edit, suffix, range, INI, all 4 scales/both themes and all translations");
}
static void TestExactIconsAndSpray() { TestExactArtwork();TestSprayRendering();TestSprayControls(); }
