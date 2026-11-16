static RasterImage LiveSelectionRaster()
{
    HBITMAP bitmap=imageModel.CopyBitmap();HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,bitmap);
    toolsModel.OnDrawOverlayOnImage(dc);GdiFlush();
    SelectObject(dc,old);DeleteDC(dc);RasterImage image;RequireCanvas(ReadRaster(bitmap,image),"read floating selection preview");DeleteObject(bitmap);return image;
}
static void TestLiveSelectionBackground()
{
    const COLORREF white=RGB(255,255,255),blue=RGB(0,0,255),green=RGB(0,255,0);
    DWORD interpolation=registrySettings.SelectionInterpolation,replace=registrySettings.ReplaceDeletedWithAlpha;
    RasterImage original;original.width=32;original.height=20;original.pixels.assign(640,0xff0c1824);
    for(int y=4;y<12;++y) for(int x=4;x<12;++x) original.pixels[y*32+x]=0xffff0000;
    original.pixels[6*32+6]=0xff0000ff;original.pixels[6*32+7]=0xff00ff00;
    for(TOOLTYPE tool:{TOOL_RECTSEL,TOOL_FREESEL}) for(bool keyed:{false,true}) for(int mode:{0,1,2})
    {
        toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();toolsModel.SetActiveTool(tool);toolsModel.SetBackgroundTransparent(keyed);
        SetBitmapAndInfo(MakeRasterBitmap(original),NULL,NULL,FALSE);paletteModel.SetBgColor(white);
        registrySettings.SelectionInterpolation=mode;registrySettings.ReplaceDeletedWithAlpha=0;
        HBITMAP mask=tool==TOOL_FREESEL?CreateMonoBitmap(8,8,TRUE):NULL;
        if(mask) { HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,mask);PatBlt(dc,6,0,2,8,BLACKNESS);SelectObject(dc,old);DeleteDC(dc); }
        selectionModel.setMask(CRect(4,4,12,12),mask);selectionModel.m_bShow=TRUE;
        RequireCanvas(selectionModel.SetTransparency(50),"activate transparency before changing background");
        auto before=LiveSelectionRaster();RequireCanvas(before.pixels[7*32+5]==0xffff7f7f,"initial selection blends over white");
        paletteModel.SetBgColor(blue);
        auto current=LiveSelectionRaster();
        RequireCanvas(current.pixels[7*32+5]==0xff80007f,"background change immediately updates existing transparency without reselecting");
        RequireCanvas(selectionModel.m_bShow && selectionModel.GetTransparency()==50 && !imageModel.CanUndo(),"background change keeps selection, percentage and undo stack");
        paletteModel.SetFgColor(green);
        RequireCanvas(LiveSelectionRaster().pixels==current.pixels,"foreground-only change leaves selection background unchanged");
        paletteModel.SwapColors();
        RequireCanvas(LiveSelectionRaster().pixels[7*32+5]==0xff807f00,"swapping palette colours updates selection background immediately");
        for(int percent:{25,100,0,50})
        {
            selectionModel.SetTransparency(percent);current=LiveSelectionRaster();
            unsigned a=MulDiv(100-percent,255,100);
            RequireCanvas(current.pixels[7*32+5]==(0xff000000|(a<<16)|((255-a)<<8)),"subsequent opacity changes use current background without cumulative blending");
            if(mask) RequireCanvas(current.pixels[7*32+11]==original.pixels[7*32+11],"freehand mask leaves unselected pixels untouched");
        }
        RequireCanvas(DocumentRaster().pixels==original.pixels,"live preview never rewrites underlying image");
        HBITMAP copy=selectionModel.GetSelectionContents();RasterImage copied;ReadRaster(copy,copied);DeleteObject(copy);
        if(mask || keyed)
        {
            RequireCanvas(copied.pixels[3*8+1]==0x80ff0000,"masked/keyed clipboard pixels preserve selection opacity");
            FlattenRaster(copied,green);
        }
        RequireCanvas(copied.pixels[3*8+1]==current.pixels[7*32+5],"copied selection composited on current background agrees with preview");
        selectionModel.moveSelection(12,0);paletteModel.SetBgColor(blue);current=LiveSelectionRaster();
        RequireCanvas(current.pixels[7*32+5]==0xff0000ff,"vacated selection area uses updated background");
        RequireCanvas(current.pixels[6*32+18]==(keyed?0xff0c1824:0xff060c92),"moved selection combines current colour key and opacity over destination");
        paletteModel.SetBgColor(green);current=LiveSelectionRaster();
        RequireCanvas(current.pixels[7*32+5]==0xff00ff00 && current.pixels[6*32+18]==0xff060c92,"new colour key restores formerly keyed blue pixels from original selection");
        RequireCanvas(current.pixels[6*32+19]==(keyed?0xff0c1824:0xff068c12),"new background key applies without another selection");
        selectionModel.Landing();RequireCanvas(DocumentRaster().pixels==current.pixels,"commit exactly matches last live preview");
        imageModel.Undo();RequireCanvas(DocumentRaster().pixels==original.pixels && !imageModel.CanUndo(),"one Undo restores original before all transparency and colour changes");
        imageModel.Redo();RequireCanvas(DocumentRaster().pixels==current.pixels,"Redo restores committed preview");
    }
    // Alpha replacement remains independent of the ordinary palette background.
    for(int alphaReplace:{0,1})
    {
        toolsModel.SetActiveTool(TOOL_RECTSEL);toolsModel.SetBackgroundTransparent(FALSE);
        SetBitmapAndInfo(MakeRasterBitmap(original),NULL,NULL,FALSE);DocumentAlpha().enabled=true;DocumentAlpha().backdrop=RGB(140,170,200);
        registrySettings.ReplaceDeletedWithAlpha=alphaReplace;paletteModel.SetBgColor(white);
        selectionModel.setMask(CRect(4,4,12,12),NULL);selectionModel.m_bShow=TRUE;selectionModel.SetTransparency(50);paletteModel.SetBgColor(blue);
        auto current=LiveSelectionRaster();
        RequireCanvas(current.pixels[7*32+5]==(alphaReplace?0x80ff0000:0xff80007f),"alpha replacement preserves real alpha; ordinary replacement follows new background");
        RequireCanvas(DocumentBackdrop()==RGB(140,170,200),"palette background change leaves document matte unchanged");
        canvasWindow.SendMessage(WM_KEYDOWN,VK_ESCAPE,0);
        RequireCanvas(DocumentRaster().pixels==original.pixels && !imageModel.CanUndo(),"Escape cancels preview after background change");
    }
    registrySettings.SelectionInterpolation=interpolation;registrySettings.ReplaceDeletedWithAlpha=replace;
    InitializeImage(NULL,NULL,FALSE);paletteModel.SetBgColor(white);toolsModel.SetBackgroundTransparent(FALSE);
    puts("PASS live selection background: immediate colour/swap updates, all interpolation modes, rectangular/freehand masks, key/alpha, copy, movement, commit, Undo/Redo and Escape");
}
