static void TestPngBackdropImport()
{
    toolsModel.OnEndDraw(TRUE);selectionModel.HideSelection();toolsModel.SetActiveTool(TOOL_RECTSEL);
    struct Sample { LPCWSTR file;COLORREF backdrop; };
    const Sample samples[]={
        {L"rgba8.png",RGB(173,227,151)},{L"indexed.png",RGB(173,227,151)},
        {L"grey1.png",RGB(255,255,255)},{L"grey2.png",RGB(170,170,170)},
        {L"grey4.png",RGB(153,153,153)},{L"grey8.png",RGB(163,163,163)},
        {L"grey-alpha8.png",RGB(163,163,163)},{L"rgb8.png",RGB(173,227,151)},
        {L"grey16.png",RGB(163,163,163)},{L"grey-alpha16.png",RGB(163,163,163)},
        {L"rgb16.png",RGB(173,227,151)},{L"rgba16.png",RGB(173,227,151)},
        {L"no-background.png",CLR_INVALID},{L"invalid-sample.png",CLR_INVALID}
    };
    for(const auto& sample:samples)
    {
        CStringW path=L"png-backdrop-tests/";path+=sample.file;
        RasterImage decoded;ImageFileInfo info;info.pngBackdrop=RGB(1,2,3);
        RequireCanvas(LoadCustomImage(path,decoded,info) && info.pngBackdrop==sample.backdrop && !info.paintData.present,"independent PNG fixture decodes bKGD according to its colour type and bit depth");
        RequireCanvas(DoLoadImageFile(mainWindow,path,TRUE)!=NULL,"native PNG background import succeeds");
        COLORREF expected=sample.backdrop==CLR_INVALID?RGB(255,255,255):sample.backdrop;
        RequireCanvas(DocumentAlpha().enabled && !DocumentAlpha().hasColor && DocumentBackdrop()==expected,"standard background restores matte without inventing a colour key");
        RequireCanvas(!CanSavePaintData() && g_imageSaved && !imageModel.CanUndo(),"imported bKGD does not create private metadata or a document edit");
        RequireCanvas(DocumentRaster().pixels==decoded.pixels,"native import preserves straight alpha and all pixel RGB values");
        HBITMAP preview=imageModel.CopyBitmap();HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,preview);
        FlattenCanvasSurface(dc);RequireCanvas(GetPixel(dc,0,0)==expected,"transparent canvas is displayed in imported background colour");
        SelectObject(dc,old);DeleteDC(dc);DeleteObject(preview);
    }
    RequireCanvas(DoLoadImageFile(mainWindow,L"png-backdrop-tests/opaque.png",TRUE)!=NULL && !DocumentAlpha().enabled,"bKGD alone never enables alpha for an opaque image");

    // A file's explicit Plus data takes precedence over its generic PNG hint.
    std::vector<BYTE> bytes;RequireCanvas(ReadImageBytes(L"png-backdrop-tests/rgba8.png",bytes),"read priority fixture");
    PaintServiceData service;service.present=service.hasColor=true;service.color=RGB(255,255,128);service.backdrop=RGB(57,149,203);
    RequireCanvas(SetPngPaintData(bytes,&service) && WriteImageBytes(L"png-backdrop-tests/priority.png",bytes),"write conflicting private/standard backgrounds");
    RequireCanvas(DoLoadImageFile(mainWindow,L"png-backdrop-tests/priority.png",TRUE)!=NULL && DocumentBackdrop()==service.backdrop && DocumentAlpha().color==service.color && CanSavePaintData(),"private data retains priority and both original swatches");

    // Optional real-world samples supplied with the bug report.
    for(int i=1;i<=2;++i)
    {
        CStringW path;path.Format(L"png-backdrop-tests/alpha_%d.png",i);
        if(!PathFileExistsW(path)) continue;
        RequireCanvas(DoLoadImageFile(mainWindow,path,TRUE)!=NULL && DocumentBackdrop()==RGB(167,230,153),"user samples show the same green matte with and without Plus metadata");
        RequireCanvas(DocumentAlpha().hasColor==(i==1) && CanSavePaintData()==(i==1),"user sample without private data keeps checker swatch");
    }

    RequireCanvas(DoLoadImageFile(mainWindow,L"png-backdrop-tests/rgba8.png",TRUE)!=NULL,"load for round-trip");
    auto original=DocumentRaster();SetCurrentPngOptions({});SetPreservePaintData(false);
    HBITMAP copy=imageModel.CopyBitmap();
    RequireCanvas(SaveCustomImage(copy,L"png-backdrop-tests/resaved.png",SaveFormat::Png,TRUE),"save imported matte without private data");DeleteObject(copy);
    RequireCanvas(DoLoadImageFile(mainWindow,L"png-backdrop-tests/resaved.png",TRUE)!=NULL && DocumentBackdrop()==RGB(173,227,151) && !CanSavePaintData(),"standard matte survives repeated save and reopen");
    auto again=DocumentRaster();
    for(size_t i=0;i<original.pixels.size();++i)
        RequireCanvas((again.pixels[i]>>24)==(original.pixels[i]>>24) && (!(original.pixels[i]>>24) || again.pixels[i]==original.pixels[i]),"round-trip retains alpha and every visible pixel");
    imageModel.PushImageForUndo();SetPixelV(imageModel.GetDC(),2,0,RGB(23,45,67));imageModel.NotifyImageChanged();imageModel.Undo();
    RequireCanvas(DocumentBackdrop()==RGB(173,227,151) && DocumentRaster().pixels==again.pixels,"undo preserves imported matte and original RGBA");
    InitializeImage(NULL,NULL,FALSE);SetPreservePaintData(true);
    puts("PASS PNG background import: RGB/grey/indexed, 1/2/4/8/16-bit, alpha/tRNS, exact canvas display, service priority, defaults, user samples, resave and Undo");
}
