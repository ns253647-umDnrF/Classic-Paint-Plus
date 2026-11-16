#include <cmath>
#include "precomp.h"
#include <richedit.h>

namespace {
double Direction(POINT point,const RECT& r)
{ return atan2(point.y-(r.top+r.bottom)/2.0,point.x-(r.left+r.right)/2.0)*180/M_PI; }
double NormalizeTextAngle(double angle)
{ angle=fmod(angle,360.0);return angle<0?angle+360:angle; }
}
void CTextEditWindow::ResetRotation()
{ m_rotating=false;m_angle=m_defaultAngle=0;m_hasPreparedAngle=true;m_dragHit=HIT_NONE;UpdateRegion(); }
double CTextEditWindow::Angle() const
{ return IsWindowVisible()?m_angle:m_hasPreparedAngle?m_defaultAngle:registrySettings.SavedTextAngle; }
void CTextEditWindow::RestoreSavedRotation()
{
    m_hasPreparedAngle=false;
    if(!IsWindowVisible()) m_angle=registrySettings.SavedTextAngle;
    if(toolSettingsWindow.IsWindow()) toolSettingsWindow.UpdateSelectionAngle(true);
}
void CTextEditWindow::BeginTextBox(bool fixedHeight)
{
    BeginParameterEdit(true);
    m_rotating=false;m_angle=m_hasPreparedAngle?m_defaultAngle:registrySettings.SavedTextAngle;
    m_hasPreparedAngle=false;m_fixedHeight=fixedHeight;m_dragHit=HIT_NONE;m_selecting=false;
    SetRectEmpty(&m_rc);UpdateRegion();
}
bool CTextEditWindow::SetWarp(const TextWarpOptions& options)
{
    if(options.style<0 || options.style>=WARP_COUNT || abs(options.bend)>100 || abs(options.horizontal)>100 || abs(options.vertical)>100) return false;
    m_warp=options;UpdateRegion();return true;
}
POINT CTextEditWindow::WarpEditorPoint(POINT point,bool inverse) const
{
    if(!TextWarpActive(m_warp)) return point;
    double pad=Zoomed(GRIP_SIZE/2),width=max(1,Zoomed(m_rc.right-m_rc.left-2*(GRIP_SIZE/2))),height=max(1,Zoomed(m_rc.bottom-m_rc.top-2*(GRIP_SIZE/2)));
    double sourceHeight=m_warpSourceHeight?max(1,Zoomed(m_warpSourceHeight)):height;
    double targetHeight=m_warpTargetHeight?max(1,Zoomed(m_warpTargetHeight)):height;
    TextWarpMesh mesh(m_warp,sourceHeight/width);WarpPoint p={(point.x-pad)/width,(point.y-pad)/(inverse?targetHeight:sourceHeight)};p=inverse?mesh.Inverse(p):mesh.Forward(p);
    return {LONG(lround(p.x*width+pad)),LONG(lround(p.y*(inverse?sourceHeight:targetHeight)+pad))};
}
bool CTextEditWindow::SetRotationAngle(double degrees)
{
    if(!std::isfinite(degrees) || m_rotating || m_dragHit!=HIT_NONE) return false;
    m_angle=NormalizeTextAngle(degrees);
    if(!IsWindowVisible()) { m_defaultAngle=m_angle;m_hasPreparedAngle=true;PreparedFigureTransform(true).present|=1<<PARAM_ANGLE; }
    if(IsWindow()) UpdateRegion();
    toolSettingsWindow.UpdateSelectionAngle(true);return true;
}
bool CTextEditWindow::SetDimensions(int width,int height)
{
    if(!IsWindow() || !IsWindowVisible() || m_rotating || m_dragHit!=HIT_NONE || width<1 || height<1 || width>32767 || height>32767) return false;
    if(height!=m_rc.bottom-m_rc.top) m_fixedHeight=ParameterIsFixed(PARAM_HEIGHT);
    CRect next=SizeEditFrame(m_rc,m_angle,width,height);ValidateEditRect(&next);return true;
}
void CTextEditWindow::UpdateRegion()
{
    // RichEdit remains the keyboard/layout engine, but never paints directly.
    // Both upright and rotated text are composited once into the canvas buffer.
    if(IsWindow()) SetWindowRgn(CreateRectRgn(0,0,0,0),FALSE);
    if(canvasWindow.IsWindow()) canvasWindow.Invalidate(FALSE);
}
void CTextEditWindow::StartRotation(POINT screen)
{
    InvalidateEditRect();RECT r;GetWindowRect(&r);m_startAngle=m_angle;m_pointerAngle=Direction(screen,r);
    m_rotating=true;SetFocus();SetCapture();UpdateRegion();RotateTo(screen);
}
void CTextEditWindow::RotateTo(POINT screen)
{
    m_lastPointer=screen;if(ParameterIsFixed(PARAM_ANGLE))return;RECT r;GetWindowRect(&r);m_angle=m_startAngle+Direction(screen,r)-m_pointerAngle;
    if(GetKeyState(VK_SHIFT)<0) m_angle=round(m_angle/15)*15;
    m_angle=NormalizeTextAngle(m_angle);
    CStringW format,angle;format.LoadString(g_hinstExe,IDS_TEXT_ROTATION);angle.Format(format,m_angle);
    ::SendMessageW(g_hStatusBar,SB_SETTEXT,2,reinterpret_cast<LPARAM>(LPCWSTR(angle)));
    toolSettingsWindow.UpdateSelectionAngle(true);canvasWindow.Invalidate(FALSE);
}
void CTextEditWindow::EndRotation(bool cancel)
{
    if(!m_rotating) return;m_rotating=false;if(cancel) m_angle=m_startAngle;
    if(::GetCapture()==m_hWnd) ::ReleaseCapture();UpdateRegion();toolSettingsWindow.UpdateSelectionAngle(true);
    ::SendMessageW(g_hStatusBar,SB_SETTEXT,2,reinterpret_cast<LPARAM>(L""));
}
void CTextEditWindow::DragFrame(POINT screen)
{
    canvasWindow.ScreenToClient(&screen);canvasWindow.CanvasToImage(screen,TRUE);
    CRect next=DragEditFrame(m_dragRect,m_dragHit,m_dragStart,screen,m_angle,12,12,ParameterIsFixed(PARAM_WIDTH),ParameterIsFixed(PARAM_HEIGHT));
    ValidateEditRect(&next);canvasWindow.Invalidate(FALSE);
}
void CTextEditWindow::EndFrameDrag(bool cancel)
{
    if(m_dragHit==HIT_NONE) return;m_dragHit=HIT_NONE;
    if(cancel) ValidateEditRect(&m_dragRect);
    else if(m_rc.bottom-m_rc.top!=m_dragRect.bottom-m_dragRect.top) m_fixedHeight=true;
    if(::GetCapture()==m_hWnd) ::ReleaseCapture();canvasWindow.Invalidate(FALSE);
}
POINT CTextEditWindow::EditorPoint(POINT screen)
{
    RECT r;GetWindowRect(&r);POINT point=EditFramePoint(screen,r,-m_angle);point.x-=r.left;point.y-=r.top;return WarpEditorPoint(point,true);
}
LRESULT CTextEditWindow::OnRotate(UINT message,WPARAM wp,LPARAM lp,BOOL& handled)
{
    if(message==WM_NCRBUTTONDOWN && wp!=HTCLIENT)
    { StartRotation(POINT{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)});return 0; }
    if(m_dragHit!=HIT_NONE)
    {
        if(message==WM_MOUSEMOVE || message==WM_LBUTTONUP)
        { POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ClientToScreen(&p);DragFrame(p);if(message==WM_LBUTTONUP) EndFrameDrag(false);return 0; }
        if(message==WM_CAPTURECHANGED) { EndFrameDrag(true);return 0; }
    }
    if(m_rotating)
    {
        if(message==WM_MOUSEMOVE || (message==WM_KEYUP && wp==VK_SHIFT))
        { POINT p=m_lastPointer;if(message==WM_MOUSEMOVE) { p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ClientToScreen(&p); }RotateTo(p);return 0; }
        if(message==WM_RBUTTONUP) { POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ClientToScreen(&p);RotateTo(p);EndRotation(false);return 0; }
        if(message==WM_CAPTURECHANGED) { EndRotation(true);return 0; }
    }
    if(m_selecting)
    {
        if(message==WM_MOUSEMOVE || message==WM_LBUTTONUP)
        {
            POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ClientToScreen(&p);p=EditorPoint(p);
            DefWindowProc(message,wp,MAKELPARAM(p.x,p.y));
            if(message==WM_LBUTTONUP) { m_selecting=false;if(::GetCapture()==m_hWnd) ::ReleaseCapture(); }
            canvasWindow.Invalidate(FALSE);return 0;
        }
        if(message==WM_CAPTURECHANGED) m_selecting=false;
    }
    handled=FALSE;return 0;
}
bool CTextEditWindow::HitRotatedBorder(POINT point)
{ HITTEST hit=IsWindowVisible()?HitEditFrame(m_rc,m_angle,point):HIT_NONE;return hit!=HIT_NONE && hit!=HIT_INNER; }
bool CTextEditWindow::SetCanvasCursor(POINT point)
{
    if(toolsModel.GetActiveTool()==TOOL_ZOOM)return false;
    if(!IsWindowVisible()) return false;HITTEST hit=HitEditFrame(m_rc,m_angle,point);if(hit==HIT_NONE) return false;
    if(hit==HIT_INNER) ::SetCursor(::LoadCursorW(NULL,IDC_IBEAM));else SetEditFrameCursor(hit,m_angle);return true;
}
bool CTextEditWindow::CanvasMouse(UINT message,WPARAM keys,POINT point)
{
    if(toolsModel.GetActiveTool()==TOOL_ZOOM)return false;
    if(!IsWindowVisible()) return false;
    HITTEST hit=HitEditFrame(m_rc,m_angle,point);POINT screen=point;canvasWindow.ClientToScreen(&screen);
    if(message==WM_RBUTTONDOWN && hit!=HIT_NONE && hit!=HIT_INNER) { StartRotation(screen);return true; }
    if(message==WM_RBUTTONDOWN && hit==HIT_INNER) { SetFocus();return true; }
    if(message==WM_LBUTTONDOWN && hit!=HIT_NONE)
    {
        SetFocus();
        if(hit==HIT_INNER)
        {
            POINT p=EditorPoint(screen);m_selecting=true;
            DefWindowProc(WM_LBUTTONDOWN,keys,MAKELPARAM(p.x,p.y));SetCapture();
        }
        else
        {
            m_dragHit=hit;m_dragRect=m_rc;canvasWindow.CanvasToImage(point,TRUE);m_dragStart=point;SetCapture();
        }
        canvasWindow.Invalidate(FALSE);return true;
    }
    return false;
}
LRESULT CTextEditWindow::OnEditorState(UINT message,WPARAM wp,LPARAM lp,BOOL&)
{
    if(message==WM_TIMER && wp==0x5250)
    { SuppressNativeCaret();m_caretVisible=!m_caretVisible;if(IsWindowVisible() && ::GetFocus()==m_hWnd) canvasWindow.Invalidate(FALSE);return 0; }
    LRESULT result=DefWindowProc(message,wp,lp);
    if(message==WM_SETFOCUS)
    { m_caretVisible=true;UINT interval=GetCaretBlinkTime();if(interval && interval!=INFINITE) SetTimer(0x5250,interval); }
    if(message==WM_KILLFOCUS || (message==WM_SHOWWINDOW && !wp)) KillTimer(0x5250);
    if(message==WM_SETTEXT || message==EM_REPLACESEL) { m_caretVisible=true;FixEditPos(NULL); }
    toolSettingsWindow.UpdateObjectDimensions();canvasWindow.Invalidate(FALSE);return result;
}
void CTextEditWindow::DrawRotatedFrame(HDC dc)
{
    DrawEditFrame(dc,m_rc,m_angle);
    CRect r=m_rc;canvasWindow.ImageToCanvas(r);
    CHARRANGE selection={};SendMessage(EM_EXGETSEL,0,LPARAM(&selection));
    HGDIOBJ oldFont=SelectObject(dc,m_hFontZoomed);TEXTMETRIC tm={};GetTextMetrics(dc,&tm);SelectObject(dc,oldFont);
    if(selection.cpMin!=selection.cpMax)
    {
        int first=int(SendMessage(EM_EXLINEFROMCHAR,0,selection.cpMin)),last=int(SendMessage(EM_EXLINEFROMCHAR,0,selection.cpMax));
        int saved=SaveDC(dc);SetROP2(dc,R2_NOT);SelectObject(dc,GetStockObject(NULL_PEN));SelectObject(dc,GetStockObject(WHITE_BRUSH));
        for(int line=first;line<=last;++line)
        {
            LONG start=max(selection.cpMin,LONG(SendMessage(EM_LINEINDEX,line,0))),end=selection.cpMax;
            if(line<last) end=LONG(SendMessage(EM_LINEINDEX,line+1,0))-1;
            POINT a={},b={};SendMessage(EM_POSFROMCHAR,WPARAM(&a),start);SendMessage(EM_POSFROMCHAR,WPARAM(&b),end);
            int right=line<last?r.Width()-Zoomed(GRIP_SIZE/2):b.x;
            std::vector<POINT> poly;
            for(int i=0;i<=32;++i) poly.push_back(POINT{a.x+MulDiv(right-a.x,i,32),a.y});
            for(int i=32;i>=0;--i) poly.push_back(POINT{a.x+MulDiv(right-a.x,i,32),a.y+tm.tmHeight});
            for(auto& p:poly) { p=WarpEditorPoint(p,false);p.x+=r.left;p.y+=r.top;p=EditFramePoint(p,r,m_angle); }Polygon(dc,poly.data(),int(poly.size()));
        }
        RestoreDC(dc,saved);
    }
    if(::GetFocus()==m_hWnd && m_caretVisible && !m_rotating && m_dragHit==HIT_NONE)
    {
        POINT caret={};SendMessage(EM_POSFROMCHAR,WPARAM(&caret),selection.cpMax);
        POINT a=WarpEditorPoint(caret,false),b=WarpEditorPoint(POINT{caret.x,caret.y+tm.tmHeight},false);a.x+=r.left;a.y+=r.top;b.x+=r.left;b.y+=r.top;
        a=EditFramePoint(a,r,m_angle);b=EditFramePoint(b,r,m_angle);
        int saved=SaveDC(dc);SetROP2(dc,R2_NOT);SelectObject(dc,GetStockObject(WHITE_PEN));MoveToEx(dc,a.x,a.y,NULL);LineTo(dc,b.x,b.y);RestoreDC(dc,saved);
    }
}
void CTextEditWindow::RenderDocument(HDC destination)
{
    CStringW text;GetWindowText(text);if(text.IsEmpty()) return;
    CRect box=m_rc;box.DeflateRect(GRIP_SIZE/2,GRIP_SIZE/2);int width=box.Width(),height=box.Height();
    if(width<=0 || height<=0 || size_t(width)*height>100000000) return;
    // Use the same RichEdit paragraph formatter for the live editor and pixels.
    HWND formatter=::CreateWindowExW(0,MSFTEDIT_CLASS,text,WS_POPUP|ES_MULTILINE,0,0,width,height,NULL,NULL,g_hinstExe,NULL);
    if(!formatter) return;
    CHARFORMAT2W cf={};cf.cbSize=sizeof(cf);cf.dwMask=CFM_FACE|CFM_SIZE|CFM_COLOR|CFM_BOLD|CFM_ITALIC|CFM_UNDERLINE;
    cf.yHeight=registrySettings.PointSize*20;cf.crTextColor=RGB(255,255,255);
    cf.dwEffects=(registrySettings.Bold?CFE_BOLD:0)|(registrySettings.Italic?CFE_ITALIC:0)|(registrySettings.Underline?CFE_UNDERLINE:0);
    StringCchCopyW(cf.szFaceName,LF_FACESIZE,registrySettings.strFontName);
    ::SendMessageW(formatter,EM_SETCHARFORMAT,SCF_ALL,reinterpret_cast<LPARAM>(&cf));
    CHARRANGE all={0,-1};::SendMessageW(formatter,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&all));
    PARAFORMAT2 pf={};pf.cbSize=sizeof(pf);pf.dwMask=PFM_ALIGNMENT;const WORD aligns[]={PFA_LEFT,PFA_CENTER,PFA_RIGHT,PFA_JUSTIFY};
    pf.wAlignment=aligns[min(3u,registrySettings.TextAlignment)];::SendMessageW(formatter,EM_SETPARAFORMAT,0,reinterpret_cast<LPARAM>(&pf));
    ::SendMessageW(formatter,EM_SETBKGNDCOLOR,0,RGB(0,0,0));
    HBITMAP mask=CreateColorDIB(width,height,RGB(0,0,0));HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,mask);
    FORMATRANGE format={};format.hdc=format.hdcTarget=dc;format.rc={0,0,MulDiv(width,1440,GetDeviceCaps(dc,LOGPIXELSX)),MulDiv(height,1440,GetDeviceCaps(dc,LOGPIXELSY))};
    format.rcPage=format.rc;format.chrg=all;
    ::SendMessageW(formatter,EM_FORMATRANGE,TRUE,reinterpret_cast<LPARAM>(&format));::SendMessageW(formatter,EM_FORMATRANGE,FALSE,0);
    ::DestroyWindow(formatter);SelectObject(dc,old);DeleteDC(dc);
    RasterImage source;ReadRaster(mask,source);DeleteObject(mask);
    m_warpSourceHeight=m_warpTargetHeight=height;
    if(TextWarpActive(m_warp))
    {
        // Deform the occupied text lines, not the empty area below them.
        int bottom=0;for(int y=0;y<height;++y) for(int x=0;x<width;++x)
            if(source.pixels[size_t(y)*width+x]&0xffffff) bottom=y+1;
        int contentHeight=min(height,max(1,bottom+2));m_warpSourceHeight=contentHeight;
        double expansion=0;
        switch(m_warp.style)
        {
        case WARP_ARC:case WARP_ARCH:case WARP_FLAG:case WARP_WAVE:case WARP_RISE: expansion=.8;break;
        case WARP_ARC_LOWER:case WARP_ARC_UPPER:case WARP_SHELL_LOWER:case WARP_SHELL_UPPER: expansion=.4;break;
        case WARP_BULGE:case WARP_FISH:case WARP_INFLATE: expansion=.5;break;
        }
        m_warpTargetHeight=min(height,int(contentHeight*(1+expansion*abs(m_warp.bend)/100.0)+.5));
        if(m_warp.style==WARP_ARC || m_warp.style==WARP_TWIST)
            m_warpTargetHeight=min(height,contentHeight+int(width*.4*abs(m_warp.bend)/100.0));
        source.height=contentHeight;source.pixels.resize(size_t(width)*contentHeight);
        TextWarpMesh(m_warp,double(contentHeight)/width).ApplyMask(source,m_warpTargetHeight);
        source.height=height;source.pixels.resize(size_t(width)*height,0xff000000);
    }
    COLORREF fg=paletteModel.GetFgColor(),bg=paletteModel.GetBgColor();unsigned ba=toolsModel.IsBackgroundTransparent()?0:paletteModel.GetBgAlpha();
    for(DWORD& p:source.pixels)
    {
        unsigned coverage=max(p&255,max(p>>8&255,p>>16&255)),fa=(coverage*paletteModel.GetFgAlpha()+127)/255;
        unsigned weight=fa*255+ba*(255-fa),a=(weight+127)/255;DWORD value=a<<24;
        if(weight) for(int c=0;c<3;++c) { unsigned f= c==0?GetBValue(fg):c==1?GetGValue(fg):GetRValue(fg),b=c==0?GetBValue(bg):c==1?GetGValue(bg):GetRValue(bg);
            unsigned straight=(f*fa*255+b*ba*(255-fa)+weight/2)/weight;value|=((straight*a+127)/255)<<(c*8); }
        p=value;
    }
    double radians=m_angle*M_PI/180,c=cos(radians),s=sin(radians),cx=(box.left+box.right)/2.0,cy=(box.top+box.bottom)/2.0;
    int outWidth=int(ceil(fabs(width*c)+fabs(height*s))),outHeight=int(ceil(fabs(width*s)+fabs(height*c)));
    if(outWidth<=0 || outHeight<=0 || size_t(outWidth)*outHeight>100000000) return;
    CRect target(int(floor(cx-outWidth/2.0)),int(floor(cy-outHeight/2.0)),0,0);target.right=target.left+outWidth;target.bottom=target.top+outHeight;
    BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=outWidth;info.bmiHeader.biHeight=-outHeight;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    DWORD* pixels=NULL;HBITMAP bitmap=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),NULL,0);if(!bitmap) return;
    for(int y=0;y<outHeight;++y) for(int x=0;x<outWidth;++x)
    {
        double dx=target.left+x+.5-cx,dy=target.top+y+.5-cy,sx=dx*c+dy*s+width/2.0-.5,sy=-dx*s+dy*c+height/2.0-.5;
        int ix=int(floor(sx)),iy=int(floor(sy));double fx=sx-ix,fy=sy-iy;double channels[4]={};
        for(int yy=0;yy<2;++yy) for(int xx=0;xx<2;++xx) if(ix+xx>=0 && iy+yy>=0 && ix+xx<width && iy+yy<height)
        { DWORD p=source.pixels[size_t(iy+yy)*width+ix+xx];double w=(xx?fx:1-fx)*(yy?fy:1-fy);for(int k=0;k<4;++k) channels[k]+=(p>>(8*k)&255)*w; }
        DWORD p=0;for(int k=0;k<4;++k) p|=DWORD(channels[k]+.5)<<(8*k);pixels[size_t(y)*outWidth+x]=p;
    }
    BlendRasterSelection(destination,target,bitmap,255);DeleteObject(bitmap);
}
