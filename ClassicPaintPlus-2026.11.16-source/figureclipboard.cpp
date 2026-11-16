#include "precomp.h"

UINT FigureClipboardFormat()
{ static UINT format=RegisterClipboardFormatW(L"Classic Paint Plus.Editable Figure.v1");return format; }
bool ValidFigureClipboard(const FigureClipboardData& d)
{
    return d.magic==0x46505043 && d.version==3 && d.bytes==sizeof(d) && d.kind<FIG_COUNT &&
        d.width>=1 && d.width<=32767 && d.height>=1 && d.height<=32767 &&
        d.lineWidth>=1 && d.lineWidth<=500 && d.style<=2 && d.rounding<=MAX_FIGURE_RADIUS && d.starBend>=-100 && d.starBend<=100 && d.palette>=PAL_MODERN && d.palette<=PAL_MONOCHROME &&
        !(d.foreground&0xff000000) && !(d.background&0xff000000) &&
        _finite(d.angle) && fabs(d.angle)<=360 && _finite(d.skewX) && _finite(d.skewY) && fabs(d.skewX)<=8 && fabs(d.skewY)<=8;
}
HGLOBAL PackFigureClipboard(const FigureClipboardData& data)
{
    if(!ValidFigureClipboard(data))return NULL;
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,sizeof(data));if(!memory)return NULL;
    void* p=GlobalLock(memory);if(!p) { GlobalFree(memory);return NULL; }
    memcpy(p,&data,sizeof(data));GlobalUnlock(memory);return memory;
}
bool UnpackFigureClipboard(HGLOBAL memory,FigureClipboardData& data)
{
    const size_t legacySize=offsetof(FigureClipboardData,starBend);
    if(!memory || GlobalSize(memory)<legacySize)return false;
    const void* p=GlobalLock(memory);if(!p)return false;
    data={};memcpy(&data,p,legacySize);
    bool legacy=data.version==1 && data.bytes==legacySize;
    bool current=(data.version==2 || data.version==3) && data.bytes==sizeof(data) && GlobalSize(memory)>=sizeof(data);
    if(current)memcpy(&data,p,sizeof(data));
    GlobalUnlock(memory);
    if(legacy){data.bytes=sizeof(data);data.starBend=0;}
    // Older rounded figures use proportional Beziers, not circular radii.
    // Keep their supplied raster fallback instead of silently changing shape.
    if((legacy || current) && data.version<3){if(data.rounding)return false;data.version=3;}
    return (legacy || current) && ValidFigureClipboard(data);
}
HBITMAP RasterizeFigure(const FigureClipboardData& d)
{
    if(!ValidFigureClipboard(d))return NULL;
    CRect bounds(0,0,d.width,d.height),output=TransformedBounds(bounds,d.angle,d.skewX,d.skewY);
    int margin=(d.lineWidth+1)/2+2;output.InflateRect(margin,margin);
    if(output.Width()>32767 || output.Height()>32767 || LONGLONG(output.Width())*output.Height()>100000000)return NULL;
    HBITMAP bitmap=MakeTransparentBitmap(output.Width(),output.Height());if(!bitmap)return NULL;
    HDC dc=CreateCompatibleDC(NULL);if(!dc) { DeleteObject(bitmap);return NULL; }
    HGDIOBJ old=SelectObject(dc,bitmap);bounds.OffsetRect(-output.left,-output.top);
    HBRUSH fg=ToolsModel::CreateBrush(PAL_TYPE(d.palette),d.foreground),bg=ToolsModel::CreateBrush(PAL_TYPE(d.palette),d.background);
    DrawFigure(dc,d.kind,bounds,fg,bg,d.lineWidth,d.style,d.angle*M_PI/180,d.skewX,d.skewY,d.rounding,d.starBend);
    DeleteObject(fg);DeleteObject(bg);SelectObject(dc,old);DeleteDC(dc);return bitmap;
}
HMENU CreateFigureContextMenu()
{
    HMENU menu=CreatePopupMenu(),source=LoadMenuW(g_hinstExe,MAKEINTRESOURCEW(ID_MENU));
    if(!menu || !source) { if(menu)DestroyMenu(menu);if(source)DestroyMenu(source);return NULL; }
    for(UINT id:{IDM_EDITCUT,IDM_EDITCOPY,IDM_EDITDELETESELECTION})
    {
        WCHAR label[256]={};GetMenuStringW(source,id,label,_countof(label),MF_BYCOMMAND);
        AppendMenuW(menu,MF_STRING,id,label);
    }
    DestroyMenu(source);return menu;
}
void ShowFigureContextMenu(POINT point)
{
    HMENU menu=CreateFigureContextMenu();if(!menu)return;
    UINT command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,mainWindow,NULL);
    DestroyMenu(menu);if(command)mainWindow.PostMessage(WM_COMMAND,command);
}
