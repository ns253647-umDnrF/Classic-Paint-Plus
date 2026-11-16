// Nondestructive selection rotation: every preview samples the original pixels.
#include <cmath>
#include "precomp.h"

CRect RotatedSelectionBounds(const RECT& r,double angle)
{
    if(!angle) return CRect(r);
    double a=angle*M_PI/180,c=fabs(cos(a)),s=fabs(sin(a));
    double w=(r.right-r.left)*c+(r.bottom-r.top)*s,h=(r.right-r.left)*s+(r.bottom-r.top)*c;
    double cx=(r.left+r.right)/2.0,cy=(r.top+r.bottom)/2.0;
    return CRect(LONG(floor(cx-w/2+1e-8)),LONG(floor(cy-h/2+1e-8)),LONG(ceil(cx+w/2-1e-8)),LONG(ceil(cy+h/2-1e-8)));
}
POINT RotateSelectionPoint(POINT point,const RECT& r,double angle)
{
    double a=angle*M_PI/180,c=cos(a),s=sin(a),cx=(r.left+r.right)/2.0,cy=(r.top+r.bottom)/2.0;
    return {LONG(lround(cx+(point.x-cx)*c-(point.y-cy)*s)),LONG(lround(cy+(point.x-cx)*s+(point.y-cy)*c))};
}
static double SelectionDirection(POINT p,const RECT& r)
{ return atan2(p.y-(r.top+r.bottom)/2.0,p.x-(r.left+r.right)/2.0)*180/M_PI; }
bool SelectionModel::SetRotationAngle(double degrees)
{
    if(!m_bShow || m_rc.IsRectEmpty() || m_rotating || m_editing || !std::isfinite(degrees)) return false;
    degrees=fmod(degrees,360.0);if(degrees<0) degrees+=360;
    if(fabs(degrees-m_angle)<1e-10) return true;
    if(IsLanded() && !TakeOff()) return false;
    if(!m_hbmColor) return false;
    m_angle=degrees;NotifyContentChanged();return true;
}
bool SelectionModel::StartRotation(POINT point)
{
    HITTEST hit=hitTest(point);
    if(!toolsModel.IsSelection() || hit==HIT_NONE || hit==HIT_INNER || m_rotating) return false;
    m_rotationWasLanded=IsLanded();m_rotationChanged=m_bContentChanged;
    if(m_rotationWasLanded && !TakeOff()) return false;
    if(!m_hbmColor) return false;
    CRect canvas=m_rc;canvasWindow.ImageToCanvas(canvas);
    m_startAngle=m_angle;m_pointerAngle=SelectionDirection(point,canvas);m_rotationPointer=point;m_rotating=true;
    UpdateRotation(point,GetKeyState(VK_SHIFT)<0);return true;
}
void SelectionModel::UpdateRotation(POINT point,bool snap)
{
    if(!m_rotating) return;m_rotationPointer=point;if(ParameterIsFixed(PARAM_ANGLE))return;
    CRect canvas=m_rc;canvasWindow.ImageToCanvas(canvas);
    double angle=m_startAngle+SelectionDirection(point,canvas)-m_pointerAngle;
    if(snap) angle=round(angle/15.0)*15;
    angle=fmod(angle,360.0);if(angle<0) angle+=360;
    if(fabs(angle)<1e-8 || fabs(angle-360)<1e-8) angle=0;
    ::SetCursor(TransformFrameCursor(HIT_UPPER_RIGHT,angle,0,0,true));
    m_angle=angle;m_bContentChanged=m_rotationChanged || fabs(m_angle-m_startAngle)>1e-8;
    CStringW format,text;format.LoadString(g_hinstExe,IDS_TEXT_ROTATION);text.Format(format,m_angle);
    ::SendMessageW(g_hStatusBar,SB_SETTEXT,2,LPARAM(LPCWSTR(text)));imageModel.NotifyImageChanged();
}
void SelectionModel::EndRotation(bool cancel)
{
    if(!m_rotating) return;m_rotating=false;
    if(cancel) { m_angle=m_startAngle;m_bContentChanged=m_rotationChanged; }
    if(m_rotationWasLanded && (cancel || m_angle==m_startAngle)) ClearColorImage();
    ::SendMessageW(g_hStatusBar,SB_SETTEXT,2,LPARAM(L""));imageModel.NotifyImageChanged();
}
