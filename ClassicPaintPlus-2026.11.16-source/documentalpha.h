#pragma once

// Document state: never imported from or persisted as application preferences.
struct DocumentAlphaState
{
    bool enabled=false, hasColor=false, customized=false;
    COLORREF color=RGB(255,255,255), backdrop=RGB(255,255,255);
};
DocumentAlphaState& DocumentAlpha();
COLORREF DocumentBackdrop();
bool ReplaceWithDocumentAlpha();
void ResetDocumentAlpha(HBITMAP bitmap);
void NotifyDocumentAlpha();
bool DocumentAlphaPicking();
void BeginDocumentAlphaPick();
void CancelDocumentAlphaPick(bool restoreTool=true);
bool PickDocumentAlpha(int x,int y);
bool ConvertDocumentAlpha(COLORREF color);
void ToggleDocumentAlpha();
void SetDocumentBackdrop(COLORREF color);
bool ConfirmAlphaLoss(HBITMAP bitmap,bool supportsAlpha);
#ifdef PAINT_TESTS
extern int g_alphaLossAnswer; // 0 uses the real dialog; IDOK/IDCANCEL for file tests.
extern unsigned g_alphaLossWarnings;
#endif

#include "documentalpha_resource.h"
