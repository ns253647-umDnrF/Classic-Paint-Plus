#pragma once
// Shared geometry for editable text and vector figures; angles are in degrees.
CRect SizeEditFrame(const RECT& bounds,double degrees,int width,int height);
POINT EditFramePoint(POINT point,const RECT& bounds,double degrees);
HITTEST HitEditFrame(const RECT& bounds,double degrees,POINT canvasPoint);
void DrawEditFrame(HDC dc,const RECT& bounds,double degrees);
void SetEditFrameCursor(HITTEST hit,double degrees);
CRect DragEditFrame(const RECT& start,HITTEST hit,POINT startPoint,POINT point,double degrees,int minWidth,int minHeight,bool fixedWidth=false,bool fixedHeight=false);
POINT TransformHandle(const RECT& bounds,int hit,double degrees,double skewX,double skewY);
POINT TransformArrowHandle(const RECT& bounds,int hit,double degrees,double skewX,double skewY);
HITTEST HitTransformArrow(const RECT& bounds,double degrees,double skewX,double skewY,POINT canvasPoint);
HCURSOR TransformFrameCursor(HITTEST hit,double degrees,double skewX,double skewY,bool rotateMode);
HITTEST HitTransformFrame(const RECT& bounds,double degrees,double skewX,double skewY,POINT canvasPoint);
void DrawTransformFrame(HDC dc,const RECT& bounds,double degrees,double skewX,double skewY,bool rotateMode,bool muted=false);
CRect DragTransformFrame(const RECT& start,HITTEST hit,POINT origin,POINT point,double degrees,double skewX,double skewY,int minSize=1,bool fixedWidth=false,bool fixedHeight=false);
void DragFrameSkew(const RECT& start,HITTEST hit,POINT origin,POINT point,double degrees,double startX,double startY,bool snap,double& skewX,double& skewY);
bool IsCornerHandle(HITTEST hit);
