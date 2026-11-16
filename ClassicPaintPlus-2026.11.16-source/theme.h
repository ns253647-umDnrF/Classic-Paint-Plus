#pragma once
COLORREF PaintSysColor(int index);
HBRUSH PaintSysColorBrush(int index);
BOOL PaintDrawEdge(HDC dc,LPRECT rect,UINT edge,UINT flags);
BOOL PaintDrawFrameControl(HDC dc,LPRECT rect,UINT type,UINT state);
void InitializeTheme();
void ApplyTheme();
void ThemeMenu(HMENU menu,bool bar=false);
void ForgetThemedMenu(HMENU menu);
#define GetSysColor PaintSysColor
#define GetSysColorBrush PaintSysColorBrush
#define DrawEdge PaintDrawEdge
#define DrawFrameControl PaintDrawFrameControl
