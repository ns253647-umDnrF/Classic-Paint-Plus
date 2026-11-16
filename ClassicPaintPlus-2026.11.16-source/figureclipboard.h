#pragma once
// Fixed-width, versioned clipboard record. No pointers, handles or process-local state.
#pragma pack(push, 1)
struct FigureClipboardData
{
    DWORD magic,version,bytes,kind,width,height,lineWidth,style,rounding,foreground,background,palette;
    double angle,skewX,skewY;
    LONG starBend;
};
#pragma pack(pop)
UINT FigureClipboardFormat();
bool ValidFigureClipboard(const FigureClipboardData& value);
HGLOBAL PackFigureClipboard(const FigureClipboardData& value);
bool UnpackFigureClipboard(HGLOBAL memory,FigureClipboardData& value);
HBITMAP RasterizeFigure(const FigureClipboardData& value);
HMENU CreateFigureContextMenu();
void ShowFigureContextMenu(POINT screenPoint);
