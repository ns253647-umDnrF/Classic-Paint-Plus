#include "precomp.h"

namespace {
DocumentAlphaState state;
bool picking=false;
TOOLTYPE previousTool=TOOL_RECTSEL;
}
DocumentAlphaState& DocumentAlpha() { return state; }
COLORREF DocumentBackdrop() { return state.enabled?state.backdrop:RGB(255,255,255); }
bool ReplaceWithDocumentAlpha()
{
    // Transparency can also be introduced by a canvas extension or an RGBA paste.
    return registrySettings.ReplaceDeletedWithAlpha && (state.enabled ||
        BitmapHasAlpha(HBITMAP(GetCurrentObject(imageModel.GetDC(),OBJ_BITMAP))));
}
bool DocumentAlphaPicking() { return picking; }
void NotifyDocumentAlpha()
{
    if(paletteWindow.IsWindow()) { paletteWindow.SendMessage(WM_SIZE);paletteWindow.Invalidate(FALSE); }
    if(mainWindow.IsWindow())
    {
        CheckMenuItem(GetMenu(mainWindow),IDM_IMAGEDRAWOPAQUE,CHECKED_IF(state.enabled || picking));
        // Additional alpha swatches can introduce a horizontal palette scrollbar.
        mainWindow.SendMessage(WM_SIZE);
    }
    if(canvasWindow.IsWindow()) canvasWindow.Invalidate(FALSE);
    if(miniature.IsWindow()) miniature.Invalidate(FALSE);
}
void ResetDocumentAlpha(HBITMAP bitmap)
{
    CancelDocumentAlphaPick();state=DocumentAlphaState();state.enabled=BitmapHasAlpha(bitmap);NotifyDocumentAlpha();
}
void BeginDocumentAlphaPick()
{
    if(picking) return;
    previousTool=toolsModel.GetActiveTool();
    toolsModel.SetActiveTool(TOOL_COLOR); // Land any pending text, figure or selection first.
    picking=true;NotifyDocumentAlpha();
    CStringW prompt;prompt.LoadString(g_hinstExe,IDS_ALPHA_PICK);
    ::SendMessageW(g_hStatusBar,SB_SETTEXT,0,LPARAM(LPCWSTR(prompt)));
    canvasWindow.SetFocus();
}
void CancelDocumentAlphaPick(bool restoreTool)
{
    if(!picking) return;
    picking=false;
    if(restoreTool) toolsModel.SetActiveTool(previousTool);
    ::SendMessageW(g_hStatusBar,SB_SETTEXT,0,LPARAM(L""));NotifyDocumentAlpha();
}
bool ConvertDocumentAlpha(COLORREF color)
{
    RasterImage image;HBITMAP source=imageModel.LockBitmap();bool read=ReadRaster(source,image);imageModel.UnlockBitmap(source);
    if(!read) return false;
    // Re-keying deliberately turns ALL previous alpha into the current matte.
    if(state.enabled) FlattenRaster(image,state.backdrop);
    DWORD rgb=RGB(GetBValue(color),GetGValue(color),GetRValue(color));
    for(DWORD& p:image.pixels) if((p&0xffffff)==rgb) p&=0xffffff;
    HBITMAP result=MakeRasterBitmap(image);if(!result) return false;
    imageModel.PushImageForUndo(result);
    if(!state.enabled) state.backdrop=color;
    state.enabled=true;state.hasColor=true;state.color=color;state.customized=true;
    imageModel.NotifyImageChanged();return true;
}
bool PickDocumentAlpha(int x,int y)
{
    if(!picking) return false;
    if(x<0 || y<0 || x>=imageModel.GetWidth() || y>=imageModel.GetHeight()) return true;
    DWORD p=RasterPixel(imageModel.GetDC(),x,y);unsigned a=p>>24;
    COLORREF matte=DocumentBackdrop();
    COLORREF color=RGB(((p>>16&255)*a+GetRValue(matte)*(255-a)+127)/255,
        ((p>>8&255)*a+GetGValue(matte)*(255-a)+127)/255,((p&255)*a+GetBValue(matte)*(255-a)+127)/255);
    if(ConvertDocumentAlpha(color)) CancelDocumentAlphaPick();
    return true;
}
void ToggleDocumentAlpha()
{
    if(picking) { CancelDocumentAlphaPick();return; }
    if(!state.enabled) { BeginDocumentAlphaPick();return; }
    canvasWindow.OnEndDraw(FALSE);selectionModel.Landing();
    RasterImage image;HBITMAP source=imageModel.LockBitmap();bool read=ReadRaster(source,image);imageModel.UnlockBitmap(source);
    if(!read) return;FlattenRaster(image,state.backdrop);HBITMAP result=MakeRasterBitmap(image);if(!result) return;
    imageModel.PushImageForUndo(result);state=DocumentAlphaState();imageModel.NotifyImageChanged();
}
void SetDocumentBackdrop(COLORREF color)
{
    if(!state.enabled || color==state.backdrop) return;
    state.backdrop=color;state.customized=true;g_imageSaved=FALSE;NotifyDocumentAlpha();
}
#ifdef PAINT_TESTS
int g_alphaLossAnswer=IDOK;
unsigned g_alphaLossWarnings=0;
#endif
bool ConfirmAlphaLoss(HBITMAP bitmap,bool supportsAlpha)
{
    if(supportsAlpha || !BitmapHasAlpha(bitmap)) return true;
#ifdef PAINT_TESTS
    ++g_alphaLossWarnings;if(g_alphaLossAnswer) return g_alphaLossAnswer==IDOK;
#endif
    CStringW message;message.LoadString(g_hinstExe,IDS_ALPHA_LOSS);
    return MessageBoxW(mainWindow,message,L"Classic Paint Plus",MB_OKCANCEL|MB_ICONWARNING|MB_DEFBUTTON2)==IDOK;
}
