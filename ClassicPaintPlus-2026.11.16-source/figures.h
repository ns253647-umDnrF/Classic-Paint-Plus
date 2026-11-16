// Local shape gallery, LGPL-2.0-or-later.
#pragma once
#include "figures_resource.h"
enum FigureKind { FIG_ELLIPSE, FIG_RECT, FIG_RRECT, FIG_TRIANGLE, FIG_RIGHT_TRIANGLE,
    FIG_DIAMOND, FIG_PENTAGON, FIG_HEXAGON, FIG_RIGHT_ARROW, FIG_LEFT_ARROW,
    FIG_UP_ARROW, FIG_DOWN_ARROW, FIG_STAR4, FIG_STAR5, FIG_STAR6,
    FIG_RECT_CALLOUT, FIG_OVAL_CALLOUT, FIG_CLOUD_CALLOUT,
    FIG_STAR8, FIG_STAR16, FIG_STAR24, FIG_STAR32,
    FIG_LEFT_RIGHT_ARROW, FIG_UP_DOWN_ARROW, FIG_QUAD_ARROW, FIG_LEFT_RIGHT_UP_ARROW,
    FIG_BENT_ARROW, FIG_UTURN_ARROW, FIG_LEFT_UP_ARROW, FIG_BENT_UP_ARROW,
    FIG_CURVED_LEFT_ARROW, FIG_CURVED_RIGHT_ARROW, FIG_CURVED_UP_ARROW, FIG_CURVED_DOWN_ARROW,
    FIG_STRIPED_RIGHT_ARROW, FIG_NOTCHED_RIGHT_ARROW, FIG_PENTAGON_ARROW, FIG_CHEVRON,
    FIG_RIGHT_ARROW_CALLOUT, FIG_LEFT_ARROW_CALLOUT, FIG_UP_ARROW_CALLOUT, FIG_DOWN_ARROW_CALLOUT,
    FIG_LEFT_RIGHT_ARROW_CALLOUT, FIG_QUAD_ARROW_CALLOUT, FIG_CIRCULAR_ARROW, FIG_LEFT_CIRCULAR_ARROW,
    FIG_COUNT };
enum FigureGroup { GROUP_LINE, GROUP_POLYGON, GROUP_ELLIPSE, GROUP_STARS, GROUP_ARROWS, GROUP_CALLOUTS, GROUP_COUNT };
int FigureGroupForKind(int kind);
const std::vector<int>& FigureGroupItems(int group);
int FigureGroupSelection(int group);
void ActivateFigureChoice(int choice);
void RememberFigureGroupChoice(int kind);
bool CanRoundFigure(int kind);
int FigureRounding(int kind=-1);
constexpr int MAX_FIGURE_RADIUS=500;
void SetFigureRounding(int radius,int kind=-1);
void ResetFigureSession();
CStringW FigureLabel(int kind);
void DrawFigureIcon(HDC dc,int kind,RECT rect,COLORREF color);
bool IsBoundedFigureTool(int tool);
bool IsRoundedFigureTool();
double FigureRotationAngle(double radians, bool snap);
CRect FigureBounds(POINT start, POINT end, bool center, bool constrain);
void DrawFigure(HDC dc, int kind, const RECT& bounds, HBRUSH foreground, HBRUSH background, int thickness, int style, double angle = 0,double skewX=0,double skewY=0,int rounding=-1,int starBend=0);
void CreateFigureControls(HWND toolbox);
void CreateFigureOriginControls(HWND settings);
void UpdateFigureControls();
void LayoutFigureControls(int scrollOffset = 0);
void ShowFigureGallery(int group=GROUP_POLYGON);
