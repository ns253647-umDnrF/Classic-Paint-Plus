#include <cmath>
#include <algorithm>
#include "precomp.h"
#include "textwarp.h"
#include "jpeg_resource.h"

bool TextWarpActive(const TextWarpOptions& o)
{ return o.style>WARP_NONE && o.style<WARP_COUNT && (o.bend || o.horizontal || o.vertical); }
namespace {
WarpPoint Deform(WarpPoint p,const TextWarpOptions& o,double aspect)
{
    double u=p.x*2-1,v=p.y*2-1;if(o.verticalAxis) { std::swap(u,v);aspect=1/aspect; }
    double b=o.bend*.01,x=u,y=v,q=max(0.0,1-u*u);
    switch(o.style)
    {
    case WARP_ARC: if(fabs(b)>1e-8) { double a=b*1.2,depth=min(.6,aspect),r=2/a;x=(r-v*depth)*sin(u*a);y=(r-(r-v*depth)*cos(u*a))/depth; }break;
    case WARP_ARC_LOWER: y=v+b*.85*q*(v+1)/2;break;
    case WARP_ARC_UPPER: y=v-b*.85*q*(1-v)/2;break;
    case WARP_ARCH: y=v-b*.8*q;break;
    case WARP_BULGE: y=v*(1+b*.8*q);break;
    case WARP_SHELL_LOWER: x=u*(1+b*.4*(v+1)/2);y=v+b*.7*q*(v+1)/2;break;
    case WARP_SHELL_UPPER: x=u*(1+b*.4*(1-v)/2);y=v-b*.7*q*(1-v)/2;break;
    case WARP_FLAG: y=v+b*.55*sin(M_PI*u);break;
    case WARP_WAVE: y=v+b*.45*sin(2*M_PI*u);break;
    case WARP_FISH: y=v*(1+b*.75*cos(M_PI*u/2))*(1-b*.3*u);break;
    case WARP_RISE: y=v-b*.8*u;break;
    case WARP_FISHEYE: { double f=1+b*.4*q*max(0.0,1-v*v);x=u*f;y=v*f; }break;
    case WARP_INFLATE: x=u*(1+b*.3*(1-v*v));y=v*(1+b*.4*q);break;
    case WARP_SQUEEZE: x=u*(1-b*.65*(1-v*v));y=v*(1+b*.3*q);break;
    case WARP_TWIST: { double depth=min(1.0,aspect),z=v*depth,a=b*.7*(1-min(1.0,(u*u+z*z)/2));x=u*cos(a)-z*sin(a);y=(u*sin(a)+z*cos(a))/depth; }break;
    }
    if(o.verticalAxis) std::swap(x,y);
    return {x,y};
}
bool Barycentric(WarpPoint p,WarpPoint a,WarpPoint b,WarpPoint c,double& s,double& t)
{
    double d=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);if(fabs(d)<1e-12) return false;
    s=((p.x-a.x)*(c.y-a.y)-(p.y-a.y)*(c.x-a.x))/d;
    t=((b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x))/d;
    return s>=-1e-8 && t>=-1e-8 && s+t<=1+1e-8;
}
}
TextWarpMesh::TextWarpMesh(const TextWarpOptions& options,double aspect)
{
    aspect=max(.03,min(30.0,aspect));
    points.resize((Columns+1)*(Rows+1));double left=1e30,top=1e30,right=-1e30,bottom=-1e30;
    bool active=TextWarpActive(options);
    for(int y=0;y<=Rows;++y) for(int x=0;x<=Columns;++x)
    {
        WarpPoint p={double(x)/Columns,double(y)/Rows};if(active) p=Deform(p,options,aspect);
        points[y*(Columns+1)+x]=p;left=min(left,p.x);top=min(top,p.y);right=max(right,p.x);bottom=max(bottom,p.y);
    }
    for(auto& p:points) { p.x=(p.x-left)/(right-left);p.y=(p.y-top)/(bottom-top); }
    // Perspective uses bounded coordinates, including for very wide/tall text.
    // Applying it to the raw circular coordinates can introduce a zero divisor.
    if(active && (options.horizontal || options.vertical))
    {
        left=top=1e30;right=bottom=-1e30;
        for(auto& p:points)
        {
            double x=p.x*2-1,y=p.y*2-1;
            p={x/(1+options.horizontal*.0035*y),y/(1+options.vertical*.0035*x)};
            left=min(left,p.x);top=min(top,p.y);right=max(right,p.x);bottom=max(bottom,p.y);
        }
        for(auto& p:points) { p.x=(p.x-left)/(right-left);p.y=(p.y-top)/(bottom-top); }
    }
}
WarpPoint TextWarpMesh::Forward(WarpPoint p) const
{
    double xx=max(0.0,min(1.0,p.x))*Columns,yy=max(0.0,min(1.0,p.y))*Rows;
    int x=min(Columns-1,int(xx)),y=min(Rows-1,int(yy));double u=xx-x,v=yy-y;int i=y*(Columns+1)+x;
    WarpPoint a=points[i],b=points[i+1],c=points[i+Columns+1],d=points[i+Columns+2];
    if(u+v<=1) return {a.x+(b.x-a.x)*u+(c.x-a.x)*v,a.y+(b.y-a.y)*u+(c.y-a.y)*v};
    return {d.x+(c.x-d.x)*(1-u)+(b.x-d.x)*(1-v),d.y+(c.y-d.y)*(1-u)+(b.y-d.y)*(1-v)};
}
WarpPoint TextWarpMesh::Inverse(WarpPoint p) const
{
    double nearest=1e30;WarpPoint result=p;
    for(int y=0;y<Rows;++y) for(int x=0;x<Columns;++x)
    {
        int i=y*(Columns+1)+x;double s,t;
        if(Barycentric(p,points[i],points[i+1],points[i+Columns+1],s,t)) return {(x+s)/Columns,(y+t)/Rows};
        if(Barycentric(p,points[i+Columns+2],points[i+Columns+1],points[i+1],s,t)) return {(x+1-s)/Columns,(y+1-t)/Rows};
        double d=pow(points[i].x-p.x,2)+pow(points[i].y-p.y,2);if(d<nearest) { nearest=d;result={double(x)/Columns,double(y)/Rows}; }
    }
    return result;
}
void TextWarpMesh::ApplyMask(RasterImage& image,int outputHeight) const
{
    RasterImage output;output.width=image.width;output.height=outputHeight>0?outputHeight:image.height;
    output.pixels.assign(size_t(output.width)*output.height,0xff000000);
    const int width=output.width,height=output.height;
    auto triangle=[&](int ia,int ib,int ic,WarpPoint sa,WarpPoint sb,WarpPoint sc)
    {
        WarpPoint a=points[ia],b=points[ib],c=points[ic];
        int left=max(0,int(floor(min(a.x,min(b.x,c.x))*width))),right=min(width,int(ceil(max(a.x,max(b.x,c.x))*width)));
        int top=max(0,int(floor(min(a.y,min(b.y,c.y))*height))),bottom=min(height,int(ceil(max(a.y,max(b.y,c.y))*height)));
        for(int y=top;y<bottom;++y) for(int x=left;x<right;++x)
        {
            double s,t;if(!Barycentric({(x+.5)/width,(y+.5)/height},a,b,c,s,t)) continue;
            double sx=(sa.x+(sb.x-sa.x)*s+(sc.x-sa.x)*t)*image.width-.5,sy=(sa.y+(sb.y-sa.y)*s+(sc.y-sa.y)*t)*image.height-.5;
            int ix=int(floor(sx)),iy=int(floor(sy));double fx=sx-ix,fy=sy-iy,coverage=0;
            for(int dy=0;dy<2;++dy) for(int dx=0;dx<2;++dx)
                if(ix+dx>=0 && iy+dy>=0 && ix+dx<image.width && iy+dy<image.height)
                { DWORD p=image.pixels[size_t(iy+dy)*width+ix+dx];coverage+=max(p&255,max(p>>8&255,p>>16&255))*(dx?fx:1-fx)*(dy?fy:1-fy); }
            DWORD k=DWORD(coverage+.5),&p=output.pixels[size_t(y)*width+x];if(k>(p&255)) p=0xff000000|k*0x010101;
        }
    };
    for(int y=0;y<Rows;++y) for(int x=0;x<Columns;++x)
    {
        int i=y*(Columns+1)+x;WarpPoint a={double(x)/Columns,double(y)/Rows},b={(x+1.)/Columns,double(y)/Rows},c={double(x)/Columns,(y+1.)/Rows},d={(x+1.)/Columns,(y+1.)/Rows};
        triangle(i,i+1,i+Columns+1,a,b,c);triangle(i+Columns+2,i+Columns+1,i+1,d,c,b);
    }
    image=std::move(output);
}

namespace {
CStringW WarpLabel(UINT id) { CStringW s;s.LoadString(g_hinstExe,id);return s; }
struct WarpDialog
{
    TextWarpOptions original,value;bool updating=false;
    void Sync(HWND window)
    {
        updating=true;SendDlgItemMessageW(window,IDC_WARP_STYLE,CB_SETCURSEL,value.style,0);
        CheckRadioButton(window,IDC_WARP_HORIZONTAL,IDC_WARP_VERTICAL,value.verticalAxis?IDC_WARP_VERTICAL:IDC_WARP_HORIZONTAL);
        int values[]={value.bend,value.horizontal,value.vertical};bool enabled=value.style!=WARP_NONE;
        for(int i=0;i<3;++i)
        { CStringW s;s.Format(L"%d%%",values[i]);if(GetFocus()!=GetDlgItem(window,IDC_WARP_BEND+i)) SetDlgItemTextW(window,IDC_WARP_BEND+i,s);
          SendDlgItemMessage(window,IDC_WARP_SLIDER_FIRST+i,TBM_SETPOS,TRUE,values[i]);EnableWindow(GetDlgItem(window,IDC_WARP_BEND+i),enabled);EnableWindow(GetDlgItem(window,IDC_WARP_SLIDER_FIRST+i),enabled); }
        EnableWindow(GetDlgItem(window,IDC_WARP_HORIZONTAL),enabled);EnableWindow(GetDlgItem(window,IDC_WARP_VERTICAL),enabled);
        updating=false;
    }
    bool Read(HWND window)
    {
        int values[3];for(int i=0;i<3;++i)
        { WCHAR raw[32];GetDlgItemTextW(window,IDC_WARP_BEND+i,raw,_countof(raw));CStringW s(raw);s.Trim();s.TrimRight(L"% ");WCHAR* end=nullptr;long v=wcstol(s,&end,10);
          if(s.IsEmpty() || *end || v< -100 || v>100) return false;values[i]=int(v); }
        value.bend=values[0];value.horizontal=values[1];value.vertical=values[2];return true;
    }
    void Preview() { textEditWindow.SetWarp(value);canvasWindow.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW); }
};
INT_PTR CALLBACK WarpDialogProc(HWND window,UINT msg,WPARAM wp,LPARAM lp)
{
    auto state=reinterpret_cast<WarpDialog*>(GetWindowLongPtrW(window,DWLP_USER));
    if(msg==WM_INITDIALOG)
    {
        state=reinterpret_cast<WarpDialog*>(lp);SetWindowLongPtrW(window,DWLP_USER,lp);SetWindowTextW(window,WarpLabel(IDS_TEXT_WARP));
        for(int i=0;i<6;++i) SetDlgItemTextW(window,7500+i,WarpLabel(IDS_WARP_STYLE+i));
        SetDlgItemTextW(window,IDC_WARP_HORIZONTAL,WarpLabel(IDS_WARP_HORIZONTAL));SetDlgItemTextW(window,IDC_WARP_VERTICAL,WarpLabel(IDS_WARP_VERTICAL));
        SetDlgItemTextW(window,IDOK,L"OK");SetDlgItemTextW(window,IDCANCEL,WarpLabel(IDS_JPEG_CANCEL));
        for(int i=0;i<WARP_COUNT;++i) SendDlgItemMessageW(window,IDC_WARP_STYLE,CB_ADDSTRING,0,LPARAM(LPCWSTR(WarpLabel(IDS_WARP_FIRST+i))));
        for(int i=0;i<3;++i) { SendDlgItemMessage(window,IDC_WARP_SLIDER_FIRST+i,TBM_SETRANGE,TRUE,MAKELPARAM(-100,100));SendDlgItemMessage(window,IDC_WARP_SLIDER_FIRST+i,TBM_SETTICFREQ,25,0);InstallNumericSuffix(GetDlgItem(window,IDC_WARP_BEND+i),L"%"); }
        state->Sync(window);return TRUE;
    }
    if(!state) return FALSE;
    if(msg==WM_CLOSE) { textEditWindow.SetWarp(state->original);EndDialog(window,IDCANCEL);return TRUE; }
    if(msg==WM_HSCROLL)
    {
        int i=GetDlgCtrlID(HWND(lp))-IDC_WARP_SLIDER_FIRST;if(i>=0 && i<3)
        { int v=int(SendMessage(HWND(lp),TBM_GETPOS,0,0));(i==0?state->value.bend:i==1?state->value.horizontal:state->value.vertical)=v;
          state->updating=true;CStringW s;s.Format(L"%d%%",v);SetDlgItemTextW(window,IDC_WARP_BEND+i,s);state->updating=false;state->Preview();return TRUE; }
    }
    if(msg==WM_COMMAND)
    {
        int id=LOWORD(wp),code=HIWORD(wp);if(state->updating) return TRUE;
        if(id==IDCANCEL) { textEditWindow.SetWarp(state->original);EndDialog(window,IDCANCEL);return TRUE; }
        if(id==IDOK) { if(!state->Read(window)) { MessageBeep(MB_OK);return TRUE; }state->Preview();EndDialog(window,IDOK);return TRUE; }
        if(id==IDC_WARP_STYLE && code==CBN_SELCHANGE) { state->value.style=int(SendDlgItemMessage(window,id,CB_GETCURSEL,0,0));state->Sync(window);state->Preview();return TRUE; }
        if(id==IDC_WARP_HORIZONTAL || id==IDC_WARP_VERTICAL) { state->value.verticalAxis=id==IDC_WARP_VERTICAL;state->Sync(window);state->Preview();return TRUE; }
        if(id>=IDC_WARP_BEND && id<=IDC_WARP_VDIST && code==EN_CHANGE && state->Read(window)) { state->Sync(window);state->Preview();return TRUE; }
        if(id>=IDC_WARP_BEND && id<=IDC_WARP_VDIST && code==EN_KILLFOCUS) state->Sync(window);
    }
    return FALSE;
}
}
bool ShowTextWarpDialog(HWND owner)
{
    WarpDialog state;state.original=state.value=textEditWindow.Warp();
    bool accepted=DialogBoxParamW(g_hinstExe,MAKEINTRESOURCEW(IDD_TEXT_WARP),owner,WarpDialogProc,LPARAM(&state))==IDOK;
    if(textEditWindow.IsWindowVisible()) textEditWindow.SetFocus();return accepted;
}
void DrawTextWarpIcon(HDC dc,const RECT& r,COLORREF color)
{
    int old=SaveDC(dc);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);HFONT font=CreateFontW(-12,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Times New Roman");SelectObject(dc,font);
    RECT a=r;a.bottom-=4;DrawTextW(dc,L"T",1,&a,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    HPEN pen=CreatePen(PS_SOLID,1,color);SelectObject(dc,pen);POINT p[]={{r.left+2,r.bottom-2},{r.left+4,r.bottom-4},{r.left+11,r.bottom-4},{r.right-2,r.bottom-2}};PolyBezier(dc,p,4);
    RestoreDC(dc,old);DeleteObject(pen);DeleteObject(font);
}
