// Selection membership is independent of pixel alpha, the color key and the active tool.
#include "precomp.h"
#include <new>
#include <unordered_map>

namespace {
struct MonoPixels
{
    int width=0,height=0,stride=0;std::vector<BYTE> bits;
    bool Read(HBITMAP bitmap)
    {
        BITMAP bm={};if(!bitmap || !GetObjectW(bitmap,sizeof(bm),&bm)) return false;
        width=bm.bmWidth;height=bm.bmHeight;stride=((width+31)/32)*4;
        if(width<=0 || height<=0 || LONGLONG(width)*height>100000000) return false;
        struct Info { BITMAPINFOHEADER header;RGBQUAD colors[2]; } info={};
        info.header.biSize=sizeof(BITMAPINFOHEADER);info.header.biWidth=width;info.header.biHeight=-height;
        info.header.biPlanes=1;info.header.biBitCount=1;info.colors[1]={255,255,255,0};
        bits.resize(size_t(stride)*height);HDC dc=CreateCompatibleDC(NULL);
        int rows=GetDIBits(dc,bitmap,0,height,bits.data(),reinterpret_cast<BITMAPINFO*>(&info),DIB_RGB_COLORS);DeleteDC(dc);
        return rows==height;
    }
    bool At(int x,int y) const
    { return x>=0 && y>=0 && x<width && y<height && (bits[size_t(y)*stride+x/8]&(0x80>>(x&7))); }
};
HBITMAP MaskFromPixels(const std::vector<BYTE>& data,int width,const CRect& r)
{
    struct Info { BITMAPINFOHEADER header;RGBQUAD colors[2]; } info={};
    info.header.biSize=sizeof(BITMAPINFOHEADER);info.header.biWidth=r.Width();info.header.biHeight=-r.Height();
    info.header.biPlanes=1;info.header.biBitCount=1;info.colors[1]={255,255,255,0};
    BYTE* bits=NULL;HBITMAP bitmap=CreateDIBSection(NULL,reinterpret_cast<BITMAPINFO*>(&info),DIB_RGB_COLORS,reinterpret_cast<void**>(&bits),NULL,0);
    if(!bitmap)return NULL;int stride=((r.Width()+31)/32)*4;ZeroMemory(bits,size_t(stride)*r.Height());
    for(int y=0;y<r.Height();++y) for(int x=0;x<r.Width();++x)
        if(data[size_t(y+r.top)*width+x+r.left]) bits[size_t(y)*stride+x/8]|=0x80>>(x&7);
    return bitmap;
}
}

bool SelectionModel::ReadCanvasMask(std::vector<BYTE>& out) const
{
    int w=imageModel.GetWidth(),h=imageModel.GetHeight();
    if(w<=0 || h<=0 || LONGLONG(w)*h>100000000) return false;
    try {
        out.assign(size_t(w)*h,0);if(!m_bShow || m_rc.IsRectEmpty()) return true;
        MonoPixels mask;if(m_hbmMask && !mask.Read(m_hbmMask)) return false;
        CRect bounds=DrawingBounds(),canvas(0,0,w,h);bounds.IntersectRect(bounds,canvas);
        for(int y=bounds.top;y<bounds.bottom;++y) for(int x=bounds.left;x<bounds.right;++x)
        {
            auto p=TransformPoint(x+.5,y+.5,m_rc,m_angle,m_skewX,m_skewY,true);
            if(p.x<m_rc.left || p.y<m_rc.top || p.x>=m_rc.right || p.y>=m_rc.bottom) continue;
            if(!m_hbmMask || mask.At(int((p.x-m_rc.left)*mask.width/m_rc.Width()),int((p.y-m_rc.top)*mask.height/m_rc.Height())))
                out[size_t(y)*w+x]=1;
        }
        return true;
    } catch(const std::bad_alloc&) { return false; }
}
bool SelectionModel::ContainsImagePoint(POINT p) const
{
    if(!m_bShow || m_rc.IsRectEmpty()) return false;
    auto q=TransformPoint(p.x+.5,p.y+.5,m_rc,m_angle,m_skewX,m_skewY,true);
    if(q.x<m_rc.left || q.x>=m_rc.right || q.y<m_rc.top || q.y>=m_rc.bottom) return false;
    if(!m_hbmMask) return true;
    // A single pixel read avoids re-reading a large bitmap on each mouse move.
    HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,m_hbmMask);
    COLORREF c=GetPixel(dc,int((q.x-m_rc.left)*GetDIBWidth(m_hbmMask)/m_rc.Width()),int((q.y-m_rc.top)*GetDIBHeight(m_hbmMask)/m_rc.Height()));
    SelectObject(dc,old);DeleteDC(dc);return c!=CLR_INVALID && c!=RGB(0,0,0);
}
bool SelectionModel::CombineSelection(const CRect& r,HBITMAP bitmap,int operation)
{
    // Prepare the entire new membership before committing a floating image.
    // Allocation failure must neither change pixels nor discard the old selection.
    bool success=false;
    try {
        int w=imageModel.GetWidth(),h=imageModel.GetHeight();std::vector<BYTE> result;
        if(w<=0 || h<=0 || LONGLONG(w)*h>100000000) { DeleteObject(bitmap);return false; }
        if(operation && !ReadCanvasMask(result)) { DeleteObject(bitmap);return false; }
        if(!operation) result.assign(size_t(w)*h,0);
        MonoPixels mask;if(bitmap && !mask.Read(bitmap)) { DeleteObject(bitmap);return false; }
        CRect clipped;clipped.IntersectRect(r,CRect(0,0,w,h));
        for(int y=clipped.top;y<clipped.bottom;++y) for(int x=clipped.left;x<clipped.right;++x)
            if(!bitmap || mask.At(int(LONGLONG(x-r.left)*mask.width/r.Width()),int(LONGLONG(y-r.top)*mask.height/r.Height())))
                result[size_t(y)*w+x]=(operation>=0);
        CRect bounds(w,h,0,0);size_t count=0;
        for(int y=0;y<h;++y) for(int x=0;x<w;++x) if(result[size_t(y)*w+x])
        { bounds.left=min(bounds.left,x);bounds.top=min(bounds.top,y);bounds.right=max(bounds.right,x+1);bounds.bottom=max(bounds.bottom,y+1);++count; }
        HBITMAP combined=NULL;
        if(count && count!=size_t(bounds.Width())*bounds.Height())
        { combined=MaskFromPixels(result,w,bounds);if(!combined) { DeleteObject(bitmap);return false; } }
        Landing();
        if(count) { setMask(bounds,combined);m_bShow=TRUE; }
        imageModel.NotifyImageChanged();success=true;
    } catch(const std::bad_alloc&) { }
    DeleteObject(bitmap);return success;
}
void SelectionModel::InvertSelection()
{
    try {
        std::vector<BYTE> data;if(!ReadCanvasMask(data)) { ShowOutOfMemory();return; }
        int w=imageModel.GetWidth(),h=imageModel.GetHeight();CRect bounds(w,h,0,0);size_t count=0;
        for(int y=0;y<h;++y) for(int x=0;x<w;++x) if((data[size_t(y)*w+x]^=1)!=0)
        { bounds.left=min(bounds.left,x);bounds.top=min(bounds.top,y);bounds.right=max(bounds.right,x+1);bounds.bottom=max(bounds.bottom,y+1);++count; }
        HBITMAP mask=NULL;
        if(count && count!=size_t(bounds.Width())*bounds.Height())
        { mask=MaskFromPixels(data,w,bounds);if(!mask) { ShowOutOfMemory();return; } }
        Landing();if(count) { setMask(bounds,mask);m_bShow=TRUE; }imageModel.NotifyImageChanged();
    } catch(const std::bad_alloc&) { ShowOutOfMemory(); }
}
void SelectionModel::BuildContour()
{
    if(m_contourValid)return;m_contour.clear();
    try {
        MonoPixels mask;if(!m_hbmMask || !mask.Read(m_hbmMask)) { m_contourValid=true;return; }
        // Directed pixel edges, with selected pixels on the right. Trace complete
        // loops so dash phase continues around diagonal steps and inner holes.
        std::unordered_map<size_t,BYTE> edges;size_t stride=size_t(mask.width)+1;
        auto add=[&](int x,int y,int direction) { edges[size_t(y)*stride+x]|=BYTE(1<<direction); };
        for(int y=0;y<mask.height;++y)for(int x=0;x<mask.width;++x)if(mask.At(x,y))
        {
            if(!mask.At(x,y-1))add(x,y,0);
            if(!mask.At(x+1,y))add(x+1,y,1);
            if(!mask.At(x,y+1))add(x+1,y+1,2);
            if(!mask.At(x-1,y))add(x,y+1,3);
        }
        const int dx[]={1,0,-1,0},dy[]={0,1,0,-1};
        while(!edges.empty())
        {
            size_t first=edges.begin()->first,vertex=first;int previous=-1,firstDirection=-1;
            m_contour.push_back({LONG(vertex%stride),LONG(vertex/stride)});
            do
            {
                auto it=edges.find(vertex);if(it==edges.end())break;
                int direction=-1;
                if(previous<0) { for(int i=0;i<4;++i)if(it->second&(1<<i)) { direction=i;break; } }
                else for(int turn:{1,0,3,2}) { int d=(previous+turn)%4;if(it->second&(1<<d)) { direction=d;break; } }
                if(direction<0)break;
                it->second&=BYTE(~(1<<direction));if(!it->second)edges.erase(it);
                POINT next={LONG(vertex%stride)+dx[direction],LONG(vertex/stride)+dy[direction]};
                if(previous==direction)m_contour.back()=next;else m_contour.push_back(next);
                if(firstDirection<0)firstDirection=direction;previous=direction;
                vertex=size_t(next.y)*stride+next.x;
            } while(vertex!=first);
            m_contour.push_back({-1,-1});
        }
        m_contourValid=true;
    } catch(const std::bad_alloc&) { m_contour.clear();m_contourValid=true; }
}
void SelectionModel::drawFrameOnCanvas(HDC dc)
{
    if(!m_bShow)return;
    if(m_hbmMask)
    {
        BuildContour();int w=GetDIBWidth(m_hbmMask),h=GetDIBHeight(m_hbmMask);
        std::vector<POINT> path;
        for(auto vertex:m_contour)
        {
            if(vertex.x<0)
            { if(path.size()>1)DrawDashedPolyline(dc,path.data(),int(path.size()),false);path.clear();continue; }
            auto p=TransformPoint(m_rc.left+double(vertex.x)*m_rc.Width()/w,
                m_rc.top+double(vertex.y)*m_rc.Height()/h,m_rc,m_angle,m_skewX,m_skewY);
            POINT point={LONG(lround(p.x)),LONG(lround(p.y))};canvasWindow.ImageToCanvas(point);
            if(path.empty() || point.x!=path.back().x || point.y!=path.back().y)path.push_back(point);
        }
    }
    DrawTransformFrame(dc,m_rc,m_angle,m_skewX,m_skewY,m_rotateMode,m_hbmMask!=NULL);
}
HITTEST SelectionModel::hitTest(POINT point)
{
    if(!m_bShow)return HIT_NONE;
    HITTEST hit=HitTransformFrame(m_rc,m_angle,m_skewX,m_skewY,point);
    if(hit==HIT_INNER) { canvasWindow.CanvasToImage(point,TRUE);if(!ContainsImagePoint(point))return HIT_NONE; }
    return hit;
}
bool SelectionModel::SetSkew(double x,double y)
{
    if(!m_bShow || m_editing || m_rotating || !_finite(x) || !_finite(y) || fabs(x)>8 || fabs(y)>8)return false;
    if(IsLanded() && !TakeOff())return false;m_skewX=x;m_skewY=y;NotifyContentChanged();return true;
}
void SelectionModel::EndEdit(bool cancel)
{
    if(!m_editing)return;m_editing=false;
    if(cancel)
    {
        if(m_editDupUndo)
        {
            // Undo normally hides the selection. Detach its owned bitmaps while
            // reverting the Ctrl-drag stamp, then restore the still-editable object.
            HBITMAP color=m_hbmColor,mask=m_hbmMask,sourceMask=m_hbmSourceMask;
            int opacity=m_transparency;bool mode=m_rotateMode;
            m_hbmColor=m_hbmMask=m_hbmSourceMask=NULL;imageModel.Undo(TRUE);
            m_hbmColor=color;m_hbmMask=mask;m_hbmSourceMask=sourceMask;m_bShow=TRUE;
            m_transparency=opacity;m_rotateMode=mode;m_contourValid=false;
        }
        m_rc=m_editStart;m_rcOld=m_editOldRect;m_angle=m_editAngle;m_skewX=m_editSkewX;m_skewY=m_editSkewY;m_bContentChanged=m_editChanged;
    }
    if(m_editWasLanded && (cancel || !m_editMoved)) ClearColorImage();
    if(::GetCapture()==canvasWindow.m_hWnd) ::ReleaseCapture();
    ::SendMessageW(g_hStatusBar,SB_SETTEXT,2,LPARAM(L""));imageModel.NotifyImageChanged();
}
bool SelectionModel::EditMouse(UINT message,WPARAM keys,POINT p)
{
    if(!toolsModel.IsSelection())return false;
    if(m_editing)
    {
        if(message==WM_CAPTURECHANGED || message==WM_CANCELMODE || (message==WM_KEYDOWN && keys==VK_ESCAPE))
        { EndEdit(true);return true; }
        bool modifier=(message==WM_KEYDOWN || message==WM_KEYUP) && keys==VK_SHIFT;
        if(modifier)p=m_editLast;
        if(message==WM_MOUSEMOVE || message==WM_LBUTTONUP || modifier)
        {
            m_editLast=p;bool moved=abs(p.x-m_editCanvas.x)>=GetSystemMetrics(SM_CXDRAG) || abs(p.y-m_editCanvas.y)>=GetSystemMetrics(SM_CYDRAG);
            if(moved && !m_editMoved)
            {
                if(IsLanded() && !TakeOff()) { EndEdit(true);return true; }
                if(m_editCtrl)
                {
                    if(!m_editWasLanded) { imageModel.PushImageForUndo();toolsModel.OnDrawOverlayOnImage(imageModel.GetDC());m_editDupUndo=true; }
                    m_rcOld.SetRectEmpty();
                }
                m_editMoved=true;
            }
            if(m_editMoved)
            {
                canvasWindow.CanvasToImage(p,TRUE);bool snap=modifier?message==WM_KEYDOWN:(GetKeyState(VK_SHIFT)<0 || (keys&MK_SHIFT));
                if(m_rotateMode && m_editArrow && IsCornerHandle(m_editHit))
                {
                    double cx=(m_editStart.left+m_editStart.right)*.5,cy=(m_editStart.top+m_editStart.bottom)*.5;
                    double a=m_editAngle+(atan2(p.y-cy,p.x-cx)-atan2(m_editOrigin.y-cy,m_editOrigin.x-cx))*180/M_PI;
                    if(snap)a=round(a/15)*15;if(!ParameterIsFixed(PARAM_ANGLE))m_angle=fmod(a+3600,360);
                }
                else if(m_rotateMode && m_editArrow && m_editHit>=HIT_UPPER_LEFT && m_editHit<=HIT_LOWER_RIGHT)
                {
                    DragFrameSkew(m_editStart,m_editHit,m_editOrigin,p,m_editAngle,m_editSkewX,m_editSkewY,snap,m_skewX,m_skewY);
                    if(ParameterIsFixed(PARAM_SKEW_X))m_skewX=m_editSkewX;
                    if(ParameterIsFixed(PARAM_SKEW_Y))m_skewY=m_editSkewY;
                }
                else m_rc=DragTransformFrame(m_editStart,m_editHit,m_editOrigin,p,m_editAngle,m_editSkewX,m_editSkewY);
                SetFrameCursor(m_editLast);m_bContentChanged=TRUE;imageModel.NotifyImageChanged();
                CStringW angle;angle.Format(L"%.1f\x00b0",m_angle);::SendMessageW(g_hStatusBar,SB_SETTEXT,2,LPARAM(LPCWSTR(angle)));
            }
            if(message==WM_LBUTTONUP)
            { if(!m_editMoved && m_editHit==HIT_INNER) m_rotateMode=!m_rotateMode;EndEdit(false); }
        }
        return true;
    }
    if(message!=WM_LBUTTONDOWN || GetKeyState(VK_MENU)<0)return false;
    HITTEST hit=hitTest(p);if(hit==HIT_NONE)return false;
    m_editArrow=HitTransformArrow(m_rc,m_angle,m_skewX,m_skewY,p)==hit;
    if(((keys&MK_SHIFT) || GetKeyState(VK_SHIFT)<0) &&
       !(m_rotateMode && m_editArrow && hit>=HIT_UPPER_LEFT && hit<=HIT_LOWER_RIGHT))return false;
    m_editing=true;m_editMoved=false;m_editWasLanded=IsLanded();m_editChanged=m_bContentChanged;
    m_editCtrl=(keys&MK_CONTROL)!=0 || GetKeyState(VK_CONTROL)<0;
    m_editHit=hit;m_editStart=m_rc;m_editOldRect=m_rcOld;m_editDupUndo=false;m_editAngle=m_angle;m_editSkewX=m_skewX;m_editSkewY=m_skewY;
    m_editCanvas=m_editLast=p;canvasWindow.CanvasToImage(p,TRUE);m_editOrigin=p;
    canvasWindow.SetFocus();canvasWindow.SetCapture();SetFrameCursor(m_editCanvas);return true;
}

bool SelectionModel::SetFrameCursor(POINT point)
{
    if(!m_bShow)return false;
    if(m_rotating) { ::SetCursor(TransformFrameCursor(HIT_UPPER_RIGHT,m_angle,0,0,true));return true; }
    HITTEST hit=m_editing?m_editHit:hitTest(point);if(hit==HIT_NONE)return false;
    bool arrow=m_editing?m_editArrow:HitTransformArrow(m_rc,m_angle,m_skewX,m_skewY,point)==hit;
    ::SetCursor(TransformFrameCursor(hit,m_angle,m_skewX,m_skewY,m_rotateMode && arrow));return true;
}
