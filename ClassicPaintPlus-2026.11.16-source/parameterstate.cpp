#include "precomp.h"
#include "figuregeometry.h"

namespace {
FigureTransformPreset prepared[FIG_COUNT+2],saved[FIG_COUNT+2];
bool initialized[FIG_COUNT+2]={};
UINT figureLocks=0,textLocks=0,selectionLocks=0,omitted=0;
int lastContext=-10;bool lastActive=false;
int Context()
{ return toolsModel.IsSelection()?-1:toolsModel.GetActiveTool()==TOOL_SHAPE?-2:toolsModel.ObjectSizeKind(toolsModel.GetActiveTool()==TOOL_TEXT); }
void SyncContext()
{
    int context=Context();bool active=ParameterObjectActive();
    if(context!=lastContext || active!=lastActive)omitted=0;
    lastContext=context;lastActive=active;
}
UINT& LiveLocks()
{ return toolsModel.IsSelection()?selectionLocks:toolsModel.GetActiveTool()==TOOL_TEXT?textLocks:figureLocks; }
}
FigureTransformPreset& PreparedFigureTransform(bool text)
{
    int kind=!text && toolsModel.GetActiveTool()==TOOL_SHAPE?FIG_COUNT+1:toolsModel.ObjectSizeKind(text);
    if(!initialized[kind])
    {
        initialized[kind]=true;
        if(kind<=FIG_COUNT)saved[kind].fixed=registrySettings.SavedObjectFixed[kind];
        if(text && registrySettings.SavedTextAnglePresent) { saved[kind].angle=registrySettings.SavedTextAngle;saved[kind].present|=1<<PARAM_ANGLE; }
        prepared[kind]=saved[kind];
    }
    return prepared[kind];
}
bool ParameterObjectActive()
{
    if(toolsModel.IsSelection())return selectionModel.m_bShow && !selectionModel.m_rc.IsRectEmpty();
    if(toolsModel.GetActiveTool()==TOOL_TEXT)return textEditWindow.IsWindowVisible()!=FALSE;
    if(IsBoundedFigureTool(toolsModel.GetActiveTool()))return toolsModel.HasEditableFigure() || canvasWindow.m_drawing;
    return false;
}
bool ParameterAvailable(int p)
{
    bool text=toolsModel.GetActiveTool()==TOOL_TEXT,figure=IsBoundedFigureTool(toolsModel.GetActiveTool());
    if(p<=PARAM_HEIGHT)return text || figure;
    if(p==PARAM_ANGLE)return text || figure || toolsModel.IsSelection();
    if(p<=PARAM_SKEW_Y)return figure || toolsModel.IsSelection();
    if(p==PARAM_ROUNDING)return IsRoundedFigureTool();
    return p==PARAM_STAR_BEND && figure && FigureStarPoints(toolsModel.ObjectSizeKind(false))!=0;
}
bool ParameterIsFixed(int p)
{
    if(p>=PARAM_ROUNDING)return false;
    SyncContext();
    if(ParameterObjectActive())return !!(LiveLocks()&(1<<p));
    if(toolsModel.IsSelection())return false;
    bool text=toolsModel.GetActiveTool()==TOOL_TEXT;
    return !!(PreparedFigureTransform(text).fixed&(1<<p));
}
bool ParameterHasValue(int p)
{
    SyncContext();if(ParameterObjectActive())return !(omitted&(1<<p));
    if(toolsModel.IsSelection())return false;
    bool text=toolsModel.GetActiveTool()==TOOL_TEXT;
    if(p<=PARAM_HEIGHT){SIZE size=toolsModel.ObjectSizePreset(text);return (p==PARAM_WIDTH?size.cx:size.cy)!=0;}
    const auto& state=PreparedFigureTransform(text);
    if(state.present&(1<<p))return true;
    if(p==PARAM_ANGLE)return (text?textEditWindow.Angle():state.angle)!=0;
    if(p==PARAM_SKEW_X)return state.skewX!=0;
    if(p==PARAM_SKEW_Y)return state.skewY!=0;
    return p==PARAM_ROUNDING?FigureRounding()!=0:state.starBend!=0;
}
void SetParameterOmitted(int p,bool value)
{ SyncContext();if(value)omitted|=1<<p;else omitted&=~(1<<p); }
void SetParameterFixed(int p,bool fixed)
{
    if(p>=PARAM_ROUNDING)return;
    SyncContext();bool text=toolsModel.GetActiveTool()==TOOL_TEXT;
    if(fixed && !ParameterHasValue(p))return;
    UINT& bits=ParameterObjectActive()?LiveLocks():PreparedFigureTransform(text).fixed;
    if(fixed)bits|=1<<p;else bits&=~(1<<p);
    omitted&=~(1<<p);
    if(text && ParameterObjectActive() && p==PARAM_HEIGHT)textEditWindow.SetHeightFixed(fixed);
}
void ParameterEntered(int p)
{
    SyncContext();omitted&=~(1<<p);
    if(!ParameterObjectActive() && !toolsModel.IsSelection())PreparedFigureTransform(toolsModel.GetActiveTool()==TOOL_TEXT).present|=1<<p;
}
void ClearParameter(int p)
{
    bool active=ParameterObjectActive(),text=toolsModel.GetActiveTool()==TOOL_TEXT;
    SetParameterFixed(p,false);
    if(!active && !toolsModel.IsSelection())
    {
        if(p<=PARAM_HEIGHT) { SIZE& size=toolsModel.ObjectSizePreset(text);(p==PARAM_WIDTH?size.cx:size.cy)=0; }
        else if(p==PARAM_ANGLE) { if(text)textEditWindow.SetRotationAngle(0);else PreparedFigureTransform().angle=0; }
        else if(p==PARAM_SKEW_X)PreparedFigureTransform().skewX=0;
        else if(p==PARAM_SKEW_Y)PreparedFigureTransform().skewY=0;
        else if(p==PARAM_ROUNDING)SetFigureRounding(0);
        else if(p==PARAM_STAR_BEND)toolsModel.SetStarBend(0);
        PreparedFigureTransform(text).present&=~(1<<p);
    }
    if(active)SetParameterOmitted(p,true);
}
void BeginParameterEdit(bool text)
{
    SIZE size=toolsModel.ObjectSizePreset(text);
    UINT mask=PreparedFigureTransform(text).fixed;
    if(!size.cx)mask&=~(1<<PARAM_WIDTH);if(!size.cy)mask&=~(1<<PARAM_HEIGHT);
    (text?textLocks:figureLocks)=mask;omitted=0;
}
void ResetSelectionParameterLocks() { selectionLocks=0;omitted=0; }
void RestoreTransformPreset(bool text)
{
    int kind=toolsModel.ObjectSizeKind(text);auto& state=PreparedFigureTransform(text);
    // Rounding is a separate per-kind session preference, as before.
    UINT rounding=state.present&(1<<PARAM_ROUNDING);state=saved[kind];state.present|=rounding;
}
void RestoreFigureRoundingPreset()
{
    const auto& preset=saved[toolsModel.ObjectSizeKind(false)];
    if(preset.present&(1<<PARAM_ROUNDING))SetFigureRounding(preset.rounding);
}
void SaveTransformPreset(bool text,int width,int height)
{
    int kind=toolsModel.ObjectSizeKind(text);auto& state=PreparedFigureTransform(text);
    FigureTransformPreset value;
    if(ParameterHasValue(PARAM_ANGLE)) { value.angle=text?textEditWindow.Angle():toolsModel.FigureAngle();value.present|=1<<PARAM_ANGLE; }
    if(!text)
    {
        double x,y;toolsModel.FigureSkew(x,y);
        if(ParameterHasValue(PARAM_SKEW_X)){value.skewX=x;value.present|=1<<PARAM_SKEW_X;}
        if(ParameterHasValue(PARAM_SKEW_Y)){value.skewY=y;value.present|=1<<PARAM_SKEW_Y;}
        if(ParameterAvailable(PARAM_ROUNDING) && ParameterHasValue(PARAM_ROUNDING)){value.rounding=FigureRounding();value.present|=1<<PARAM_ROUNDING;}
        if(ParameterAvailable(PARAM_STAR_BEND) && ParameterHasValue(PARAM_STAR_BEND)){value.starBend=toolsModel.StarBend();value.present|=1<<PARAM_STAR_BEND;}
    }
    if(width)value.present|=1<<PARAM_WIDTH;if(height)value.present|=1<<PARAM_HEIGHT;
    for(int p=0;p<PARAM_ROUNDING;++p)if((value.present&(1<<p)) && ParameterIsFixed(p))value.fixed|=1<<p;
    registrySettings.SavedObjectFixed[kind]=value.fixed;
    if(text)registrySettings.SavedTextAnglePresent=!!(value.present&(1<<PARAM_ANGLE));
    saved[kind]=state=value;
}
void ResetTransformPreset(bool text)
{
    int kind=toolsModel.ObjectSizeKind(text);PreparedFigureTransform(text)={};saved[kind]={};
    registrySettings.SavedObjectFixed[kind]=0;if(text)registrySettings.SavedTextAnglePresent=0;
    if(!text)SetFigureRounding(0,kind);
    omitted=0;
}
