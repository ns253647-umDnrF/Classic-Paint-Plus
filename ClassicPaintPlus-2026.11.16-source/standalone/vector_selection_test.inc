static void SaveShapeSheet(HBITMAP bitmap,LPCWSTR path);
static void VectorReset(int width=360,int height=260)
{
    canvasWindow.OnEndDraw(TRUE);selectionModel.HideSelection();ResetDocumentAlpha(NULL);
    toolsModel.RememberFigureSkew(0,0);toolsModel.SetActiveTool(TOOL_RECTSEL);toolsModel.SetBackgroundTransparent(FALSE);
    SetTestShift(false);BYTE keys[256];GetKeyboardState(keys);keys[VK_MENU]=0;keys[VK_CONTROL]=0;SetKeyboardState(keys);
    registrySettings.SelectionInterpolation=0;registrySettings.ShapeFromCenter=FALSE;
    canvasWindow.zoomTo(1000);imageModel.PushImageForUndo(CreateColorDIB(width,height,RGB(40,90,160)));imageModel.ClearHistory();
    canvasWindow.updateScrollRange();canvasWindow.ScrollToCorner(0);
}
static size_t SelectedCount()
{
    std::vector<BYTE> mask;RequireCanvas(selectionModel.ReadCanvasMask(mask),"read canvas membership");
    size_t count=0;for(BYTE b:mask)count+=b!=0;return count;
}
static HBITMAP TriangleMask(int width,int height)
{
    HBITMAP mask=CreateMonoBitmap(width,height,FALSE);HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,mask);
    POINT p[]={{0,0},{width,0},{0,height}};HGDIOBJ pen=SelectObject(dc,GetStockObject(NULL_PEN)),brush=SelectObject(dc,GetStockObject(WHITE_BRUSH));
    Polygon(dc,p,3);SelectObject(dc,brush);SelectObject(dc,pen);SelectObject(dc,old);DeleteDC(dc);return mask;
}
static void SetVectorAlt(bool down)
{ BYTE keys[256];GetKeyboardState(keys);keys[VK_MENU]=down?0x80:0;SetKeyboardState(keys); }
static void VectorDrag(POINT start,POINT end,WPARAM modifiers=0)
{
    SelectionMouse(WM_LBUTTONDOWN,start,MK_LBUTTON|modifiers);SelectionMouse(WM_MOUSEMOVE,end,MK_LBUTTON|modifiers);SelectionMouse(WM_LBUTTONUP,end,modifiers);
}
static void VectorClick(POINT point)
{ SelectionMouse(WM_LBUTTONDOWN,point,MK_LBUTTON);SelectionMouse(WM_LBUTTONUP,point); }
static void TestSelectionMembership()
{
    VectorReset();auto pixels=DocumentRaster().pixels;
    selectionModel.CombineSelection(CRect(30,30,130,110),NULL,0);
    RequireCanvas(SelectedCount()==8000,"rectangular membership is exact");
    selectionModel.CombineSelection(CRect(100,60,160,140),NULL,1);
    RequireCanvas(SelectedCount()==11300,"union includes overlap only once");
    selectionModel.CombineSelection(CRect(60,60,120,90),NULL,-1);
    RequireCanvas(SelectedCount()==9500,"subtraction creates a real hole");
    std::vector<BYTE> before;selectionModel.ReadCanvasMask(before);
    mainWindow.SendMessage(WM_COMMAND,IDM_EDITINVERTSELECTION);
    RequireCanvas(SelectedCount()==360*260-9500 && DocumentRaster().pixels==pixels,"Edit inversion complements membership without changing any pixel");
    mainWindow.SendMessage(WM_COMMAND,IDM_EDITINVERTSELECTION);std::vector<BYTE> twice;selectionModel.ReadCanvasMask(twice);
    RequireCanvas(twice==before,"double inversion restores disconnected contours and holes exactly");
    auto copy=SelectionRaster();RequireCanvas((copy.pixels[size_t(40)*copy.width+40]>>24)==0,"hole stays transparent even in opaque selection mode");
    toolsModel.SetActiveTool(TOOL_FREESEL);RequireCanvas(SelectedCount()==9500,"switching to lasso preserves compound membership");
    toolsModel.SetActiveTool(TOOL_RECTSEL);RequireCanvas(SelectedCount()==9500,"switching to rectangle does not discard a compound mask");
    mainWindow.SendMessage(WM_COMMAND,IDM_IMAGEINVERTCONTOUR);selectionModel.Landing();auto changed=DocumentRaster();
    for(size_t i=0;i<pixels.size();++i) RequireCanvas(changed.pixels[i]==(before[i]?(pixels[i]^0xffffff):pixels[i]),"contour color inversion affects exactly selected pixels");
    imageModel.Undo();RequireCanvas(DocumentRaster().pixels==pixels,"contour color inversion is one undo step");
    selectionModel.CombineSelection(CRect(20,20,50,50),NULL,0);mainWindow.SendMessage(WM_COMMAND,IDM_IMAGEINVERTCOLORS);
    for(DWORD p:DocumentRaster().pixels)RequireCanvas(p==(pixels[0]^0xffffff),"Drawing invert colors affects the whole canvas");
    VectorReset();selectionModel.CombineSelection(CRect(20,20,100,100),NULL,0);
    SetTestShift(true);VectorDrag(SelectionCanvasPoint(60,60),SelectionCanvasPoint(140,140),MK_SHIFT);SetTestShift(false);
    RequireCanvas(SelectedCount()==11200,"Shift drag through an existing selection adds a rectangle");
    SetVectorAlt(true);VectorDrag(SelectionCanvasPoint(70,70),SelectionCanvasPoint(90,90));SetVectorAlt(false);
    RequireCanvas(SelectedCount()==10800,"Alt drag subtracts rather than moving existing selection");
    before.clear();selectionModel.ReadCanvasMask(before);
    SetTestShift(true);SelectionMouse(WM_LBUTTONDOWN,SelectionCanvasPoint(160,80),MK_LBUTTON|MK_SHIFT);
    SelectionMouse(WM_MOUSEMOVE,SelectionCanvasPoint(220,140),MK_LBUTTON|MK_SHIFT);canvasWindow.SendMessage(WM_KEYDOWN,VK_ESCAPE);SetTestShift(false);
    selectionModel.ReadCanvasMask(twice);RequireCanvas(twice==before,"Escape cancels additive outline without discarding the original selection");
    toolsModel.SetActiveTool(TOOL_FREESEL);SetTestShift(true);
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON|MK_SHIFT,180,60);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON|MK_SHIFT,250,60);
    FigureMouse(WM_MOUSEMOVE,MK_LBUTTON|MK_SHIFT,215,130);FigureMouse(WM_LBUTTONUP,MK_SHIFT,180,60);SetTestShift(false);
    RequireCanvas(SelectedCount()>10800 && selectionModel.ContainsImagePoint({215,80}) && !selectionModel.ContainsImagePoint({245,120}),"Shift lasso adds only its contour and leaves bounding-box corners unselected");
    CaptureCanvasDialog(mainWindow,L"vector-selection-tests/compound-selection.bmp");
    VectorReset();selectionModel.CombineSelection(CRect(40,40,120,120),TriangleMask(80,80),0);
    auto original=SelectionRaster();RequireCanvas((original.pixels.back()>>24)==0 && (original.pixels.front()>>24)==255,"irregular copy has transparent exterior and opaque selected pixels");
    mainWindow.SendMessage(WM_COMMAND,IDM_EDITCOPY);
    RequireCanvas(OpenClipboard(mainWindow)!=FALSE,"open selection clipboard");
    HBITMAP external=BitmapFromClipboardDIB(GetClipboardData(CF_DIBV5));RasterImage raster;RequireCanvas(external && ReadRaster(external,raster),"external DIBV5 representation is readable");DeleteObject(external);
    RequireCanvas(!IsClipboardFormatAvailable(FigureClipboardFormat()) && raster.pixels==original.pixels,"selection copy clears private vector format and keeps mask alpha");CloseClipboard();
    toolsModel.SetBackgroundTransparent(TRUE);auto transparent=SelectionRaster();
    RequireCanvas((transparent.pixels.back()>>24)==0,"color-key toggle cannot expose pixels outside contour");
    toolsModel.SetBackgroundTransparent(FALSE);paletteModel.SetBgColor(RGB(255,255,255));selectionModel.TakeOff();selectionModel.moveSelection(100,80);
    TestBitmap preview(360,260,RGB(40,90,160));toolsModel.OnDrawOverlayOnImage(preview.dc);
    RequireCanvas(GetPixel(preview.dc,45,45)==RGB(255,255,255) && GetPixel(preview.dc,115,115)==RGB(40,90,160),"moving a lasso clears its original contour only");
    RequireCanvas(GetPixel(preview.dc,215,195)==RGB(40,90,160),"opaque mode does not paint bounding-box corners at destination");
    auto landed=LiveSelectionRaster();mainWindow.SendMessage(WM_COMMAND,IDM_EDITPASTE);
    RequireCanvas(DocumentRaster().pixels==landed.pixels && selectionModel.m_bShow,"raster paste commits the previous floating selection before replacing it");
    mainWindow.SendMessage(WM_COMMAND,IDM_EDITSELECTALL);
    RequireCanvas(SelectedCount()==360*260,"Select all replaces an existing selection even when its top-left handle is at the origin");
    mainWindow.SendMessage(WM_COMMAND,IDM_EDITINVERTSELECTION);
    RequireCanvas(!selectionModel.m_bShow && SelectedCount()==0,"inverting a full-canvas selection produces no selection");
    puts("PASS selection masks: union/subtract/invert, actual Shift/Alt rectangle/lasso gestures, cancel, holes, switching tools, exact color inversion and clipboard alpha");
}
static void TestSelectionTransformModes()
{
    VectorReset();selectionModel.CombineSelection(CRect(70,60,190,150),TriangleMask(120,90),0);auto original=SelectionRaster();auto doc=DocumentRaster();
    VectorClick(SelectionCanvasPoint(100,80));RequireCanvas(selectionModel.RotateMode() && selectionModel.IsLanded(),"click toggles rotation mode without lifting pixels");
    SelectionMouse(WM_LBUTTONDBLCLK,SelectionCanvasPoint(100,80),MK_LBUTTON);SelectionMouse(WM_LBUTTONUP,SelectionCanvasPoint(100,80));
    RequireCanvas(selectionModel.m_bShow && !selectionModel.RotateMode(),"a fast second click toggles back instead of committing or dropping selection");
    VectorClick(SelectionCanvasPoint(100,80));
    CRect r=selectionModel.m_rc;canvasWindow.ImageToCanvas(r);POINT start=TransformArrowHandle(r,HIT_LOWER_RIGHT,0,0,0);
    double cx=(r.left+r.right)*.5,cy=(r.top+r.bottom)*.5,dx=start.x-cx,dy=start.y-cy,a=31*M_PI/180;
    POINT end={LONG(lround(cx+dx*cos(a)-dy*sin(a))),LONG(lround(cy+dx*sin(a)+dy*cos(a)))};
    SetTestShift(true);VectorDrag(start,end,MK_SHIFT);SetTestShift(false);
    RequireCanvas(selectionModel.RotationAngle()==30 && DocumentRaster().pixels==doc.pixels,"left corner rotation snaps to 15 degrees without committing pixels");
    // Return to an exact axis-aligned position before testing side shear.
    selectionModel.SetRotationAngle(0);r=selectionModel.m_rc;canvasWindow.ImageToCanvas(r);start=TransformArrowHandle(r,HIT_UPPER_CENTER,0,0,0);end={start.x+30,start.y};
    VectorDrag(start,end);RequireCanvas(fabs(selectionModel.SkewX()+2.0/3)<.001 && selectionModel.SkewY()==0,"top rotation-mode handle applies horizontal skew");
    auto skewed=SelectionRaster();RequireCanvas(skewed.width>original.width && (skewed.pixels.back()>>24)==0,"skew expands output bounds with transparent corners");
    CaptureCanvasDialog(mainWindow,L"vector-selection-tests/selection-skew.bmp");
    start=TransformArrowHandle(r,HIT_MIDDLE_RIGHT,0,selectionModel.SkewX(),0);end={start.x,start.y+25};
    SelectionMouse(WM_LBUTTONDOWN,start,MK_LBUTTON);SelectionMouse(WM_MOUSEMOVE,end,MK_LBUTTON);canvasWindow.SendMessage(WM_KEYDOWN,VK_ESCAPE);
    RequireCanvas(selectionModel.SkewY()==0 && SelectionRaster().pixels==skewed.pixels,"Escape restores both skew axes without resampling original content");
    selectionModel.SetSkew(0,0);RequireCanvas(SelectionRaster().pixels==original.pixels,"zero skew returns exact original pixels");
    VectorClick(SelectionCanvasPoint(100,80));RequireCanvas(!selectionModel.RotateMode(),"second click returns resize mode");
    r=selectionModel.m_rc;canvasWindow.ImageToCanvas(r);start=TransformHandle(r,HIT_LOWER_RIGHT,0,0,0);end={start.x+40,start.y+20};VectorDrag(start,end);
    RequireCanvas(selectionModel.m_rc.Width()==160 && selectionModel.m_rc.Height()==110,"resize arrows still resize after mode switching");
    selectionModel.m_rc=CRect(70,60,190,150);selectionModel.NotifyContentChanged();
    BeginSelectionRotation();SelectionMouse(WM_MOUSEMOVE,SelectionRotationPointer(45),MK_RBUTTON|MK_SHIFT);SelectionMouse(WM_RBUTTONUP,SelectionRotationPointer(45),MK_SHIFT);
    RequireCanvas(selectionModel.RotationAngle()==45,"original right-button rotation remains available");
    // A cancelled Ctrl-drag of an already floating selection must not erase its
    // owned source bitmaps or leave a stamped copy behind.
    auto source=SelectionRaster();auto beforeCtrl=DocumentRaster();CRect beforeBounds=selectionModel.m_rc;
    POINT local={90,80};auto mapped=TransformPoint(local.x,local.y,selectionModel.m_rc,45,0,0);
    POINT inside=SelectionCanvasPoint(LONG(lround(mapped.x)),LONG(lround(mapped.y)));
    SelectionMouse(WM_LBUTTONDOWN,inside,MK_LBUTTON|MK_CONTROL);POINT shifted={inside.x+45,inside.y+25};
    SelectionMouse(WM_MOUSEMOVE,shifted,MK_LBUTTON|MK_CONTROL);canvasWindow.SendMessage(WM_KEYDOWN,VK_ESCAPE);
    RequireCanvas(selectionModel.m_bShow && selectionModel.m_rc==beforeBounds && SelectionRaster().pixels==source.pixels && DocumentRaster().pixels==beforeCtrl.pixels,"cancelled Ctrl-drag restores floating pixels, membership, angle and underlying document");
    puts("PASS raster transforms: click toggle, left/right rotation, Shift, side skew, cancel, source preservation and resize");
}
static void TestFigureClipboardAndTransforms()
{
    VectorReset();auto blank=DocumentRaster();toolsModel.SetActiveTool(TOOL_FIGURE);registrySettings.FigureKind=FIG_DIAMOND;SetFigureRounding(35,FIG_DIAMOND);
    toolsModel.SetShapeStyle(1);toolsModel.SetLineWidth(3);toolsModel.SetFigureAngle(0);toolsModel.ResetObjectSize(false);
    paletteModel.SetFgColor(RGB(20,40,200));paletteModel.SetBgColor(RGB(180,230,90));
    FigureMouse(WM_LBUTTONDOWN,MK_LBUTTON,100,80);FigureMouse(WM_MOUSEMOVE,MK_LBUTTON,240,180);FigureMouse(WM_LBUTTONUP,0,240,180);
    FigureClipboardData data={};RequireCanvas(toolsModel.ExportFigure(data),"editable figure exports a versioned value snapshot");
    VectorClick(SelectionCanvasPoint(170,130));RECT bounds;toolsModel.EditableFigureBounds(bounds);CRect frame=bounds;--frame.right;--frame.bottom;CRect canvas=frame;canvasWindow.ImageToCanvas(canvas);
    POINT start=TransformArrowHandle(canvas,HIT_UPPER_CENTER,0,0,0),end={start.x+25,start.y};VectorDrag(start,end);
    RequireCanvas(toolsModel.ExportFigure(data) && data.skewX<-.49 && data.skewX>-.51 && data.lineWidth==3,"figure side handle skews vector geometry and preserves stroke thickness");
    start=TransformArrowHandle(canvas,HIT_LOWER_RIGHT,0,data.skewX,0);double cx=(canvas.left+canvas.right)*.5,cy=(canvas.top+canvas.bottom)*.5,dx=start.x-cx,dy=start.y-cy,a=31*M_PI/180;
    end={LONG(lround(cx+dx*cos(a)-dy*sin(a))),LONG(lround(cy+dx*sin(a)+dy*cos(a)))};
    SetTestShift(true);VectorDrag(start,end,MK_SHIFT);SetTestShift(false);
    RequireCanvas(fabs(toolsModel.FigureAngle()-30)<.01,"figure corner arrow rotates with Shift snapping");
    toolsModel.ExportFigure(data);CaptureCanvasDialog(mainWindow,L"vector-selection-tests/figure-transform.bmp");
    RequireCanvas(mainWindow.SendMessage(WM_COMMAND,IDM_EDITCOPY)!=0 && toolsModel.HasEditableFigure(),"copy preserves active vector edit frame");
    RequireCanvas(OpenClipboard(mainWindow)!=FALSE,"open figure clipboard");FigureClipboardData clipboard={};
    RequireCanvas(UnpackFigureClipboard(GetClipboardData(FigureClipboardFormat()),clipboard) && memcmp(&clipboard,&data,sizeof(data))==0,"clipboard retains geometry, style and both colors");
    HBITMAP bitmap=BitmapFromClipboardDIB(GetClipboardData(CF_DIBV5));RasterImage raster;RequireCanvas(bitmap && ReadRaster(bitmap,raster),"figure exposes a standard raster format for other programs");DeleteObject(bitmap);
    bool alpha=false,painted=false;for(DWORD p:raster.pixels){alpha|=(p>>24)==0;painted|=(p>>24)==255;}
    RequireCanvas(alpha && painted && IsClipboardFormatAvailable(CF_DIB),"figure raster contains transparent outside pixels plus legacy bitmap fallback");CloseClipboard();
    mainWindow.SendMessage(WM_COMMAND,IDM_EDITCUT);RequireCanvas(!toolsModel.HasEditableFigure() && DocumentRaster().pixels==blank.pixels,"cut removes uncommitted vector without drawing it to document");
    mainWindow.SendMessage(WM_COMMAND,IDM_EDITPASTE);FigureClipboardData pasted={};RequireCanvas(toolsModel.ExportFigure(pasted) && memcmp(&pasted,&data,sizeof(data))==0,"paste reconstructs an editable figure exactly");
    toolsModel.SetLineWidth(7);toolsModel.SetShapeStyle(2);paletteModel.SetFgColor(RGB(250,30,80));
    RequireCanvas(toolsModel.ExportFigure(pasted) && pasted.lineWidth==7 && pasted.style==2 && pasted.foreground==RGB(250,30,80),"pasted vector remains fully editable");
    HMENU menu=CreateFigureContextMenu();RequireCanvas(menu && GetMenuItemCount(menu)==3 && GetMenuItemID(menu,0)==IDM_EDITCUT && GetMenuItemID(menu,1)==IDM_EDITCOPY && GetMenuItemID(menu,2)==IDM_EDITDELETESELECTION,"figure context menu contains exactly cut, copy and delete");DestroyMenu(menu);
    mainWindow.SendMessage(WM_COMMAND,IDM_EDITDELETESELECTION);RequireCanvas(!toolsModel.HasEditableFigure(),"Delete discards editable figure");
    // Corrupt private data must safely fall back to the public bitmap.
    RequireCanvas(OpenClipboard(mainWindow)!=FALSE,"open clipboard for malformed-record regression");
    HGLOBAL invalid=GlobalAlloc(GMEM_MOVEABLE,sizeof(data));void* ptr=GlobalLock(invalid);memcpy(ptr,&data,sizeof(data));reinterpret_cast<FigureClipboardData*>(ptr)->width=0xffffffff;GlobalUnlock(invalid);
    HBITMAP source=imageModel.LockBitmap();HGLOBAL dib=BitmapToClipboardDIBV5(source);imageModel.UnlockBitmap(source);
    EmptyClipboard();SetClipboardData(FigureClipboardFormat(),invalid);SetClipboardData(CF_DIBV5,dib);CloseClipboard();
    mainWindow.SendMessage(WM_COMMAND,IDM_EDITPASTE);RequireCanvas(selectionModel.m_bShow && !toolsModel.HasEditableFigure(),"invalid private clipboard data falls back to raster paste");
    puts("PASS figure clipboard: private editable record, public RGBA/DIB, cut/copy/delete, editable paste, malformed fallback, rotation/skew and unchanged stroke width");
}
static void TestVectorMenus()
{
    if(testLanguages.empty())EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    for(WORD language:testLanguages)
    {
        registrySettings.UILanguage=language;ApplyPreferredLanguage();HMENU menu=LoadMenuW(g_hinstExe,MAKEINTRESOURCEW(ID_MENU)),edit=GetSubMenu(menu,1),draw=GetSubMenu(menu,3);
        int cut=-1,paste=-1,del=-1,allColors=-1,contourColors=-1;
        for(int i=0;i<GetMenuItemCount(edit);++i)
        { UINT id=GetMenuItemID(edit,i);if(id==IDM_EDITCUT)cut=i;if(id==IDM_EDITPASTE)paste=i;if(id==IDM_EDITDELETESELECTION)del=i;RequireCanvas(id!=IDM_CROPSELECTION,"Edit menu has no Crop selection item"); }
        for(int i=0;i<GetMenuItemCount(draw);++i) { UINT id=GetMenuItemID(draw,i);if(id==IDM_IMAGEINVERTCOLORS)allColors=i;if(id==IDM_IMAGEINVERTCONTOUR)contourColors=i; }
        RequireCanvas(cut>=0 && paste==cut+2 && del==paste+1 && allColors>=0 && contourColors==allColors+1,"localized Edit groups and color inversion ordering are correct");
        HMENU popup=CreateFigureContextMenu();WCHAR text[256];for(int i=0;i<3;++i)RequireCanvas(GetMenuStringW(popup,i,text,_countof(text),MF_BYPOSITION)>0,"figure context labels exist in every language");DestroyMenu(popup);DestroyMenu(menu);
    }
    registrySettings.UILanguage=MAKELANGID(LANG_RUSSIAN,SUBLANG_DEFAULT);ApplyPreferredLanguage();
    puts("PASS menus in all 30 languages: Delete group, no Edit crop, separate contour colors and translated vector context menu");
}
static void TestSelectionModifierCursors()
{
    HCURSOR add=SelectionCombineCursor(false),subtract=SelectionCombineCursor(true);
    RequireCanvas(add && subtract && add!=subtract,"selection modes have separate native plus and minus cursors");
    ICONINFO a={},b={};RequireCanvas(GetIconInfo(add,&a) && GetIconInfo(subtract,&b),"selection cursors expose native hotspot information");
    RequireCanvas(a.xHotspot==b.xHotspot && a.yHotspot==b.yHotspot && a.xHotspot<4 && a.yHotspot<4,"both signs keep the arrow-tip hotspot");
    DeleteObject(a.hbmMask);DeleteObject(a.hbmColor);DeleteObject(b.hbmMask);DeleteObject(b.hbmColor);
    TestBitmap sheet(160,80,RGB(255,255,255));RECT dark={0,40,160,80};FillRect(sheet.dc,&dark,(HBRUSH)GetStockObject(BLACK_BRUSH));
    for(int y:{0,40}){DrawIconEx(sheet.dc,10,y+4,add,32,32,0,NULL,DI_NORMAL);DrawIconEx(sheet.dc,70,y+4,subtract,32,32,0,NULL,DI_NORMAL);}
    SaveShapeSheet(sheet.bitmap,L"vector-selection-tests/modifier-cursors.bmp");
    for(auto tool:{TOOL_RECTSEL,TOOL_FREESEL})
    {
        VectorReset();toolsModel.SetActiveTool(tool);selectionModel.CombineSelection(CRect(20,20,130,110),NULL,0);
        POINT inside=SelectionCanvasPoint(70,60);
        SetTestShift(true);RequireCanvas(toolsModel.SetFigureCursor(inside) && GetCursor()==add,"Shift cursor replaces the movement cursor inside both kinds of selection");
        SetVectorAlt(true);RequireCanvas(toolsModel.SetFigureCursor(inside) && GetCursor()==subtract,"Alt takes precedence over Shift exactly as selection subtraction does");
        SetTestShift(false);RequireCanvas(toolsModel.SetFigureCursor(inside) && GetCursor()==subtract,"Alt alone shows minus");
        SetVectorAlt(false);RequireCanvas(!toolsModel.SetFigureCursor(inside),"releasing modifiers restores ordinary frame cursor handling");
        SetTestShift(true);SelectionMouse(WM_LBUTTONDOWN,SelectionCanvasPoint(150,140),MK_LBUTTON|MK_SHIFT);SetTestShift(false);
        SelectionMouse(WM_MOUSEMOVE,SelectionCanvasPoint(180,170),MK_LBUTTON);
        RequireCanvas(GetCursor()==add,"captured selection retains the operation chosen at mouse-down after Shift release");
        canvasWindow.SendMessage(WM_KEYDOWN,VK_ESCAPE);
        RequireCanvas(!toolsModel.SetFigureCursor(inside),"cancel releases the latched modifier cursor");
        VectorClick(inside);CRect bounds=selectionModel.m_rc;canvasWindow.ImageToCanvas(bounds);
        POINT handle=TransformArrowHandle(bounds,HIT_LOWER_RIGHT,0,0,0);
        SetTestShift(true);RequireCanvas(!toolsModel.SetFigureCursor(handle) && selectionModel.SetFrameCursor(handle) && GetCursor()!=add,"Shift on a rotation arrow keeps the rotation cursor and snapping");SetTestShift(false);
    }
    VectorReset();toolsModel.SetActiveTool(TOOL_PEN);SetTestShift(true);RequireCanvas(!toolsModel.SetFigureCursor(SelectionCanvasPoint(60,60)),"modifier cursor does not affect other tools");SetTestShift(false);
    puts("PASS selection cursors: plus/minus, both tools, Alt precedence, hotspot, capture, cancellation and rotation snapping");
}
static void TestVectorSelection()
{
    CreateDirectoryW(L"vector-selection-tests",NULL);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);mainWindow.SetWindowPos(NULL,0,0,1000,760,SWP_NOZORDER|SWP_NOACTIVATE);
    TestSelectionMembership();TestSelectionTransformModes();TestFigureClipboardAndTransforms();TestVectorMenus();TestSelectionModifierCursors();
    canvasWindow.OnEndDraw(TRUE);puts("PASS all vector/selection integration checks");
}
