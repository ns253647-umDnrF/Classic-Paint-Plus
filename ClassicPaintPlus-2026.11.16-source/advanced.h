#pragma once
#include "advanced_resource.h"
void RememberFont(LPCWSTR name);
std::vector<CStringW> RecentFonts();
void TrimRecentFonts();
void DrawDashedRect(HDC dc,const RECT& rect);
void DrawDashedPolyline(HDC dc,const POINT* points,int count,bool closed);
void DrawColorSample(HDC dc,const RECT& rect,COLORREF color,BYTE alpha=255);
DWORD RasterPixel(HDC dc,int x,int y);
void FloodRaster(HDC dc,int x,int y,DWORD rgba);
void CreateBrushPanel(HWND owner);
void UpdateBrushPanel();
void DrawSoftBrushStroke(HDC dc,const POINT* points,size_t count,int shape,int size,int hardness,COLORREF color,BYTE alpha,COLORREF replaceColor=CLR_INVALID);
