// Local panel metrics and canvas-marker settings, LGPL-2.0-or-later.
#pragma once
#include "panelsettings_resource.h"
int PanelScale();
int PanelPercent();
int ToolboxTopMargin();
int PanelPx(int value);
int PanelEditHeight(HWND edit);
void CompactPanelEdit(HWND edit);
int CanvasGripSize();
int ToolboxContentHeight();
int ToolboxWidth(int height);
void ApplyPanelScale();
void BuildPanelSettingsMenu(HMENU settings);
bool InitMarkerSizeMenu(HWND owner, HMENU popup);
void ShowMarkerSizeInput(HWND owner);
bool HandlePanelSettingsCommand(HWND owner, UINT command);
void UpdatePanelSettingsChecks(HMENU menu);
HIMAGELIST CreatePanelToolIcons(int percent);
void DrawPanelTransparency(HDC dc,const RECT& rect,bool transparent);
void DrawPanelSwapArrow(HDC dc,const RECT& rect);
void DrawPanelCopyArrow(HDC dc,const RECT& rect,int direction,COLORREF color,bool pressed);
void InstallPanelTrackbar(HWND trackbar);

LRESULT HandlePanelMenuMessage(HWND owner,UINT message,WPARAM wp,LPARAM lp,BOOL& handled);
int ToolbarHeight();
int ToolSettingsHeight();
