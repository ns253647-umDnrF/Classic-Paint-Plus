#include "precomp.h"
#include "figuregeometry.h"

namespace {
int roundingValues[FIG_COUNT]={};
int freePolygonRounding=0;
int selected[GROUP_COUNT]={-TOOL_LINE,FIG_RECT,FIG_ELLIPSE,FIG_STAR5,FIG_RIGHT_ARROW,FIG_RECT_CALLOUT};
const std::vector<int> groups[GROUP_COUNT]={
    {-TOOL_LINE,-TOOL_BEZIER,-TOOL_SHAPE},
    {FIG_RECT,FIG_TRIANGLE,FIG_RIGHT_TRIANGLE,FIG_DIAMOND,FIG_PENTAGON,FIG_HEXAGON},
    {FIG_ELLIPSE},
    {FIG_STAR4,FIG_STAR5,FIG_STAR6,FIG_STAR8,FIG_STAR16,FIG_STAR24,FIG_STAR32},
    {FIG_RIGHT_ARROW,FIG_LEFT_ARROW,FIG_UP_ARROW,FIG_DOWN_ARROW,
     FIG_LEFT_RIGHT_ARROW,FIG_UP_DOWN_ARROW,FIG_QUAD_ARROW,FIG_LEFT_RIGHT_UP_ARROW,
     FIG_BENT_ARROW,FIG_UTURN_ARROW,FIG_LEFT_UP_ARROW,FIG_BENT_UP_ARROW,
     FIG_CURVED_LEFT_ARROW,FIG_CURVED_RIGHT_ARROW,FIG_CURVED_UP_ARROW,FIG_CURVED_DOWN_ARROW,
     FIG_STRIPED_RIGHT_ARROW,FIG_NOTCHED_RIGHT_ARROW,FIG_PENTAGON_ARROW,FIG_CHEVRON,
     FIG_RIGHT_ARROW_CALLOUT,FIG_LEFT_ARROW_CALLOUT,FIG_UP_ARROW_CALLOUT,FIG_DOWN_ARROW_CALLOUT,
     FIG_LEFT_RIGHT_ARROW_CALLOUT,FIG_QUAD_ARROW_CALLOUT,FIG_CIRCULAR_ARROW,FIG_LEFT_CIRCULAR_ARROW},
    {FIG_RECT_CALLOUT,FIG_OVAL_CALLOUT,FIG_CLOUD_CALLOUT}};
}
int FigureGroupForKind(int kind)
{
    if(kind==FIG_RRECT)return GROUP_POLYGON; // Legacy clipboard figures remain readable.
    for(int group=0;group<GROUP_COUNT;++group)
        for(int value:groups[group])if(kind==value)return group;
    return -1;
}
const std::vector<int>& FigureGroupItems(int group)
{ static const std::vector<int> empty;return group>=0 && group<GROUP_COUNT?groups[group]:empty; }
int FigureGroupSelection(int group)
{ return group>=0 && group<GROUP_COUNT?selected[group]:FIG_RECT; }
void RememberFigureGroupChoice(int kind)
{
    int group=FigureGroupForKind(kind);if(group>=0)selected[group]=kind==FIG_RRECT?FIG_RECT:kind;
}
void ActivateFigureChoice(int kind)
{
    if(FigureGroupForKind(kind)<0)return;
    toolsModel.OnEndDraw(FALSE);
    RememberFigureGroupChoice(kind);
    if(kind<0)toolsModel.SetActiveTool(TOOLTYPE(-kind));
    else if(kind==FIG_ELLIPSE)toolsModel.SetActiveTool(TOOL_ELLIPSE);
    else { registrySettings.FigureKind=kind;toolsModel.SetActiveTool(TOOL_FIGURE); }
    canvasWindow.SetFocus();
}
bool CanRoundFigure(int kind)
{ return kind==-TOOL_SHAPE || kind==FIG_RRECT || FigureGroupForKind(kind)==GROUP_POLYGON || FigureStarPoints(kind)>0; }
int FigureRounding(int kind)
{
    if(kind==-1)kind=toolsModel.GetActiveTool()==TOOL_SHAPE?-TOOL_SHAPE:toolsModel.ObjectSizeKind(false);
    if(kind==-TOOL_SHAPE)return freePolygonRounding;
    return kind>=0 && kind<FIG_COUNT?roundingValues[kind]:0;
}
void SetFigureRounding(int radius,int kind)
{
    if(kind==-1)kind=toolsModel.GetActiveTool()==TOOL_SHAPE?-TOOL_SHAPE:toolsModel.ObjectSizeKind(false);
    if(kind==-TOOL_SHAPE) { freePolygonRounding=max(0,min(MAX_FIGURE_RADIUS,radius));toolsModel.NotifyToolSettingsChanged();return; }
    if(kind<0 || kind>=FIG_COUNT || !CanRoundFigure(kind))return;
    roundingValues[kind]=max(0,min(MAX_FIGURE_RADIUS,radius));
    toolsModel.NotifyToolSettingsChanged();
}
void ResetFigureSession()
{
    ZeroMemory(roundingValues,sizeof(roundingValues));
    freePolygonRounding=0;
    const int defaults[]={-TOOL_LINE,FIG_RECT,FIG_ELLIPSE,FIG_STAR5,FIG_RIGHT_ARROW,FIG_RECT_CALLOUT};
    memcpy(selected,defaults,sizeof(selected));
}
CStringW FigureLabel(int kind)
{
    CStringW text;
    UINT id=kind<0?IDS_TOOLTIP1+(-kind-TOOL_FREESEL):kind<FIG_STAR8?IDS_FIGURE_FIRST+kind:IDS_EXTRA_FIGURE_FIRST+kind-FIG_STAR8;
    text.LoadString(g_hinstExe,id);return text;
}
