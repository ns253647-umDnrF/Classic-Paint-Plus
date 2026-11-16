// Local canvas operations, LGPL-2.0-or-later.
#pragma once
void BuildCanvasMenu(HMENU menu);
void ShowCanvasDialog(HWND owner, bool cropping);
bool SelectionCornerCropRect(int corner,RECT& area);
bool CropCanvasAtSelectionCorner(int corner);
bool ValidCanvasSize(int width, int height);
int CanvasAnchorOffset(int oldSize, int newSize, int anchor);
HBITMAP MakeCanvasBitmap(HDC source, int oldWidth, int oldHeight,
                        int width, int height, int x, int y, COLORREF fill);
