#pragma once
#include "parameterbar_resource.h"
enum ObjectParameter { PARAM_WIDTH,PARAM_HEIGHT,PARAM_ANGLE,PARAM_SKEW_X,PARAM_SKEW_Y,PARAM_ROUNDING,PARAM_STAR_BEND,PARAM_COUNT };
struct FigureTransformPreset { double angle=0,skewX=0,skewY=0;UINT fixed=0,present=0;int starBend=0,rounding=0; };
FigureTransformPreset& PreparedFigureTransform(bool text=false);
void RestoreTransformPreset(bool text);
void RestoreFigureRoundingPreset();
void SaveTransformPreset(bool text,int width,int height);
void ResetTransformPreset(bool text);
void BeginParameterEdit(bool text);
void ResetSelectionParameterLocks();
bool ParameterObjectActive();
bool ParameterAvailable(int parameter);
bool ParameterIsFixed(int parameter);
bool ParameterHasValue(int parameter);
void SetParameterFixed(int parameter,bool fixed);
void ParameterEntered(int parameter);
void ClearParameter(int parameter);
void SetParameterOmitted(int parameter,bool omitted);
void CreateParameterBar(HWND owner);
HWND ParameterBarWindow();
HWND ObjectActionsWindow();
HWND ParameterControl(int id);
int ParameterBarHeight(int width);
void LayoutParameterBar();
void UpdateParameterBar();
void ReloadParameterBarLanguage();
