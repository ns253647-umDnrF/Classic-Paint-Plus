// Local canvas dialogs, LGPL-2.0-or-later.
#include "precomp.h"
#include "canvasdialogs.h"
#include "canvasresource.h"
#include <errno.h>

static CStringW Text(UINT id)
{
    CStringW s;
    s.LoadString(g_hinstExe, id);
    return s;
}

bool ValidCanvasSize(int width, int height)
{
    // GDI dimensions and a signed 32-bit DIB byte count, including row padding.
    return width > 0 && height > 0 && width <= 32767 && height <= 32767 &&
        static_cast<LONGLONG>(width) * 4 * height <= INT_MAX;
}

int CanvasAnchorOffset(int oldSize, int newSize, int anchor)
{
    return (newSize - oldSize) * anchor / 2; // 0: start, 1: center, 2: end
}

HBITMAP MakeCanvasBitmap(HDC source, int oldWidth, int oldHeight,
                        int width, int height, int x, int y, COLORREF fill)
{
    if (!ValidCanvasSize(width, height)) return NULL;
    HBITMAP result = fill==CLR_INVALID?MakeTransparentBitmap(width,height):CreateColorDIB(width, height, fill);
    if (!result) return NULL;
    HDC dest = CreateCompatibleDC(source);
    if (!dest) { DeleteObject(result); return NULL; }
    HGDIOBJ old = SelectObject(dest, result);
    BOOL copied = BitBlt(dest, x, y, oldWidth, oldHeight, source, 0, 0, SRCCOPY);
    SelectObject(dest, old);
    DeleteDC(dest);
    if (!copied) { DeleteObject(result); return NULL; }
    return result;
}

bool SelectionCornerCropRect(int corner,RECT& area)
{
    SetRectEmpty(&area);
    if(corner<0 || corner>3 || !toolsModel.IsSelection() || !selectionModel.m_bShow || selectionModel.m_rc.IsRectEmpty()) return false;
    int width=imageModel.GetWidth(),height=imageModel.GetHeight();CRect bounds=selectionModel.DrawingBounds();
    area={0,0,width,height};
    // Order: left/top, left/bottom, right/top, right/bottom. Keep the opposite
    // canvas edges; selection bounds are pixel boundaries, with exclusive ends.
    if(corner<2) area.left=max(0L,min(LONG(width),bounds.left));else area.right=max(0L,min(LONG(width),bounds.right));
    if(!(corner%2)) area.top=max(0L,min(LONG(height),bounds.top));else area.bottom=max(0L,min(LONG(height),bounds.bottom));
    return area.right>area.left && area.bottom>area.top;
}
bool CropCanvasAtSelectionCorner(int corner)
{
    RECT area;if(!SelectionCornerCropRect(corner,area)) return false;
    HBITMAP snapshot=imageModel.CopyBitmap();if(!snapshot) { ShowOutOfMemory();return false; }
    HDC dc=CreateCompatibleDC(NULL);if(!dc) { DeleteObject(snapshot);ShowOutOfMemory();return false; }
    HGDIOBJ old=SelectObject(dc,snapshot);
    // Flatten only into a temporary copy, so floating/rotated/masked selections
    // and the crop become one undo operation, without changing alpha pixels.
    toolsModel.OnDrawOverlayOnImage(dc);
    HBITMAP result=MakeCanvasBitmap(dc,imageModel.GetWidth(),imageModel.GetHeight(),
        area.right-area.left,area.bottom-area.top,-area.left,-area.top,CLR_INVALID);
    SelectObject(dc,old);DeleteDC(dc);DeleteObject(snapshot);
    if(!result) { ShowOutOfMemory();return false; }
    toolsModel.OnEndDraw(TRUE);canvasWindow.m_drawing=FALSE;
    imageModel.PushImageForUndo(result);canvasWindow.updateScrollRange();canvasWindow.Invalidate(FALSE);return true;
}

void BuildCanvasMenu(HMENU menu)
{
    for (int i = 0; i < GetMenuItemCount(menu); ++i)
    {
        HMENU image = GetSubMenu(menu, i);
        for (int j = 0; image && j < GetMenuItemCount(image); ++j)
        {
            if (GetMenuItemID(image, j) != IDM_IMAGECROP) continue;
            HMENU canvas = CreatePopupMenu();
            AppendMenuW(canvas, MF_STRING, IDM_IMAGECROP, Text(IDS_CV_CROP_SELECTION));
            for(int corner=0;corner<4;++corner) AppendMenuW(canvas,MF_STRING,IDM_CROP_CORNER_FIRST+corner,Text(IDS_CV_CROP_CORNER_FIRST+corner));
            AppendMenuW(canvas, MF_STRING, IDM_CROP_EDGES, Text(IDS_CV_CROP_EDGES));
            AppendMenuW(canvas, MF_STRING, IDM_CANVAS_SIZE, Text(IDS_CV_CANVAS_SIZE));
            WCHAR attributes[256]={};
            GetMenuStringW(image,IDM_IMAGEATTRIBUTES,attributes,_countof(attributes),MF_BYCOMMAND);
            DeleteMenu(image,IDM_IMAGEATTRIBUTES,MF_BYCOMMAND);
            DeleteMenu(image, j, MF_BYPOSITION);
            InsertMenuW(image, j, MF_BYPOSITION | MF_POPUP,
                        reinterpret_cast<UINT_PTR>(canvas), Text(IDS_CV_CANVAS_MENU));
            HMENU alpha=CreatePopupMenu();
            AppendMenuW(alpha,MF_STRING,IDM_IMAGEDRAWOPAQUE,Text(IDS_ALPHA_USE));
            AppendMenuW(alpha,MF_STRING,IDM_ALPHA_REPLACE,Text(IDS_ALPHA_REPLACE));
            AppendMenuW(image,MF_POPUP,reinterpret_cast<UINT_PTR>(alpha),Text(IDS_DOC_TRANSPARENT));
            AppendMenuW(image,MF_SEPARATOR,0,NULL);
            AppendMenuW(image,MF_STRING,IDM_IMAGEATTRIBUTES,attributes);
            return;
        }
    }
}

struct CanvasDialog
{
    bool crop, ready = false, relative = false;
    int width = imageModel.GetWidth(), height = imageModel.GetHeight();
    int anchor = 4, colorIndex = 1;
    COLORREF custom = paletteModel.GetBgColor();
    HWND window = NULL;
    HDC snapshotDC = NULL;
    HBITMAP snapshot = NULL, output = NULL;
    HGDIOBJ oldBitmap = NULL;

    explicit CanvasDialog(bool cropping) : crop(cropping)
    {
        colorIndex = ReplaceWithDocumentAlpha() ? 6 : 1;
        snapshot = imageModel.CopyBitmap();
        snapshotDC = CreateCompatibleDC(NULL);
        if (snapshot && snapshotDC)
        {
            oldBitmap = SelectObject(snapshotDC, snapshot);
            toolsModel.OnDrawOverlayOnImage(snapshotDC);
        }
    }
    ~CanvasDialog()
    {
        if (oldBitmap) SelectObject(snapshotDC, oldBitmap);
        if (snapshotDC) DeleteDC(snapshotDC);
        if (snapshot) DeleteObject(snapshot);
        if (output) DeleteObject(output);
    }
    void Label(int control, UINT text) { SetDlgItemTextW(window, control, Text(text)); }
    void Number(int control, int value) { SetDlgItemInt(window, control, value, TRUE); }
    bool Read(int control, int& value)
    {
        WCHAR s[64];
        GetDlgItemTextW(window, control, s, _countof(s));
        return ParsePixelValue(s,value,-32767,32767);
    }
    bool Dimensions(int& w, int& h, int& x, int& y)
    {
        if (crop)
        {
            int l, r, t, b;
            if (!Read(IDC_EDGE_LEFT, l) || !Read(IDC_EDGE_RIGHT, r) ||
                !Read(IDC_EDGE_TOP, t) || !Read(IDC_EDGE_BOTTOM, b)) return false;
            w = width - l - r; h = height - t - b; x = -l; y = -t;
        }
        else
        {
            if (!Read(IDC_NEW_WIDTH, w) || !Read(IDC_NEW_HEIGHT, h)) return false;
            if (relative) { w += width; h += height; }
            x = CanvasAnchorOffset(width, w, anchor % 3);
            y = CanvasAnchorOffset(height, h, anchor / 3);
        }
        return ValidCanvasSize(w, h);
    }
    COLORREF FillColor()
    {
        const COLORREF colors[] = {paletteModel.GetFgColor(), paletteModel.GetBgColor(),
            RGB(255,255,255), RGB(0,0,0), RGB(128,128,128), custom, CLR_INVALID};
        return colors[colorIndex];
    }
    void Refresh()
    {
        if (!ready) return;
        int w = 0, h = 0, x = 0, y = 0;
        bool valid = Dimensions(w, h, x, y);
        CStringW info;
        if (valid) info.Format(L"%d \x00d7 %d", w, h);
        else info = Text(IDS_CV_INVALID_SIZE);
        SetDlgItemTextW(window, IDC_CANVAS_RESULT, info);
        EnableWindow(GetDlgItem(window, IDOK), valid);
        if (crop) InvalidateRect(GetDlgItem(window, IDC_CANVAS_PREVIEW), NULL, FALSE);
    }
    void Init()
    {
        SetWindowTextW(window, Text(crop ? IDS_CV_CROP_EDGES : IDS_CV_CANVAS_SIZE));
        Label(IDCANCEL, IDS_CV_CANCEL);
        if (crop)
        {
            for (int i = 0; i < 4; ++i)
            {
                Label(IDC_EDGE_LEFT_LABEL + i, IDS_CV_EDGE_LEFT + i);
                Number(IDC_EDGE_LEFT + i, 0);
                SendDlgItemMessageW(window, IDC_EDGE_LEFT+i, EM_SETLIMITTEXT, 6, 0);
                SendDlgItemMessageW(window, IDC_SPIN_LEFT+i, UDM_SETBUDDY,
                    reinterpret_cast<WPARAM>(GetDlgItem(window, IDC_EDGE_LEFT+i)), 0);
                SendDlgItemMessageW(window, IDC_SPIN_LEFT+i, UDM_SETRANGE32, -32767, 32767);
            }
        }
        else
        {
            const int controls[] = {IDC_CURRENT_GROUP, IDC_NEW_GROUP, IDC_WIDTH_LABEL, IDC_HEIGHT_LABEL,
                IDC_WIDTH_UNITS, IDC_HEIGHT_UNITS, IDC_RELATIVE, IDC_ANCHOR_LABEL, IDC_FILL_LABEL};
            const UINT strings[] = {IDS_CV_CURRENT_SIZE, IDS_CV_NEW_SIZE, IDS_CV_WIDTH, IDS_CV_HEIGHT,
                IDS_CV_PIXELS, IDS_CV_PIXELS, IDS_CV_RELATIVE, IDS_CV_ANCHOR, IDS_CV_FILL};
            for (int i=0; i < _countof(controls); ++i) Label(controls[i], strings[i]);
            CStringW current;
            current.Format(L"%s: %d   %s\r\n%s: %d   %s", static_cast<LPCWSTR>(Text(IDS_CV_WIDTH)),
                width, static_cast<LPCWSTR>(Text(IDS_CV_PIXELS)), static_cast<LPCWSTR>(Text(IDS_CV_HEIGHT)),
                height, static_cast<LPCWSTR>(Text(IDS_CV_PIXELS)));
            SetDlgItemTextW(window, IDC_CURRENT_SIZE, current);
            Number(IDC_NEW_WIDTH, width); Number(IDC_NEW_HEIGHT, height);
            SendDlgItemMessageW(window, IDC_NEW_WIDTH, EM_SETLIMITTEXT, 7, 0);
            SendDlgItemMessageW(window, IDC_NEW_HEIGHT, EM_SETLIMITTEXT, 7, 0);
            for (int i=0; i<9; ++i)
            {
                CStringW name = Text(i/3 == 0 ? IDS_CV_EDGE_TOP : i/3 == 2 ? IDS_CV_EDGE_BOTTOM : IDS_CV_CENTER);
                name += L" / ";
                name += Text(i%3 == 0 ? IDS_CV_EDGE_LEFT : i%3 == 2 ? IDS_CV_EDGE_RIGHT : IDS_CV_CENTER);
                SetDlgItemTextW(window, IDC_ANCHOR_FIRST+i, name);
            }
        }
        if(crop) for(int i=0;i<4;++i) InstallPixelUnits(GetDlgItem(window,IDC_EDGE_LEFT+i));
        else { SetDlgItemTextW(window,IDC_WIDTH_UNITS,L"px");SetDlgItemTextW(window,IDC_HEIGHT_UNITS,L"px"); }
        Label(IDC_FILL_LABEL, IDS_CV_FILL);
        for (int i=0; i<6; ++i)
            SendDlgItemMessageW(window, IDC_CANVAS_FILL, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(Text(IDS_CV_FOREGROUND+i))));
        SendDlgItemMessageW(window,IDC_CANVAS_FILL,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(LPCWSTR(Text(IDS_TRANSPARENT))));
        SendDlgItemMessageW(window, IDC_CANVAS_FILL, CB_SETCURSEL, colorIndex, 0);
        Label(IDC_FILL_SWATCH, IDS_CV_OTHER);
        CWindow(window).CenterWindow(GetParent(window));
        ready = true;
        Refresh();
    }
    void Preview(const DRAWITEMSTRUCT& draw)
    {
        HDC dc = draw.hDC;
        RECT area = draw.rcItem;
        FillRect(dc, &area, GetSysColorBrush(COLOR_APPWORKSPACE));
        InflateRect(&area, -3, -3);
        int w=width,h=height,x=0,y=0;
        bool valid=Dimensions(w,h,x,y);
        if (!valid) { w=width; h=height; x=0; y=0; }
        // Fit the union of the original and resulting canvas so added margins remain visible.
        int left=min(0,-x), top=min(0,-y);
        int unionWidth=max(width,w-x)-left, unionHeight=max(height,h-y)-top;
        int aw = area.right-area.left, ah = area.bottom-area.top;
        int pw = aw, ph = MulDiv(unionHeight, aw, unionWidth);
        if (ph > ah) { ph = ah; pw = MulDiv(unionWidth, ah, unionHeight); }
        pw = max(1, pw); ph = max(1, ph);
        int px = area.left+(aw-pw)/2, py = area.top+(ah-ph)/2;
        int saved = SaveDC(dc);
        RECT keep = {px+MulDiv(-x-left,pw,unionWidth), py+MulDiv(-y-top,ph,unionHeight),
                     px+MulDiv(w-x-left,pw,unionWidth), py+MulDiv(h-y-top,ph,unionHeight)};
        HBRUSH fill=CreateSolidBrush(FillColor());
        if(FillColor()==CLR_INVALID) DrawColorSample(dc,keep,0,0);else FillRect(dc,&keep,fill); DeleteObject(fill);
        RECT full = {px+MulDiv(-left,pw,unionWidth), py+MulDiv(-top,ph,unionHeight),
                     px+MulDiv(width-left,pw,unionWidth), py+MulDiv(height-top,ph,unionHeight)};
        SetStretchBltMode(dc, HALFTONE); SetBrushOrgEx(dc, 0, 0, NULL);
        DrawRasterPreview(dc,full,reinterpret_cast<HBITMAP>(GetCurrentObject(snapshotDC,OBJ_BITMAP)));
        if (valid)
        {
            ExcludeClipRect(dc, keep.left,keep.top,keep.right,keep.bottom);
            HBRUSH hatch = CreateHatchBrush(HS_BDIAGONAL, RGB(96,96,96));
            SetBkMode(dc, TRANSPARENT); FillRect(dc, &full, hatch); DeleteObject(hatch);
            RestoreDC(dc, saved); saved = SaveDC(dc);
            // A white outline remains visible on dark pictures; the black inner frame on light ones.
            FrameRect(dc, &keep, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            if (keep.right-keep.left > 2 && keep.bottom-keep.top > 2)
            {
                InflateRect(&keep,-1,-1);
                FrameRect(dc, &keep, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            }
        }
        RestoreDC(dc, saved);
    }
    void Draw(const DRAWITEMSTRUCT& d)
    {
        if (d.CtlID == IDC_CANVAS_PREVIEW) { Preview(d); return; }
        RECT r = d.rcItem;
        bool selected = d.CtlID == IDC_ANCHOR_FIRST + anchor;
        DrawFrameControl(d.hDC, &r, DFC_BUTTON, DFCS_BUTTONPUSH |
            ((selected || (d.itemState & ODS_SELECTED)) ? DFCS_PUSHED : 0));
        InflateRect(&r,-4,-4);
        if (d.CtlID == IDC_FILL_SWATCH)
        {
            DrawColorSample(d.hDC,r,FillColor(),FillColor()==CLR_INVALID?0:255);
        }
        else
        {
            int cx=(r.left+r.right)/2, cy=(r.top+r.bottom)/2;
            int cell = d.CtlID-IDC_ANCHOR_FIRST;
            HPEN pen = CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNTEXT));
            HGDIOBJ oldPen=SelectObject(d.hDC,pen);
            if (selected)
            {
                HBRUSH dot=CreateSolidBrush(GetSysColor(COLOR_BTNTEXT));
                HGDIOBJ oldBrush=SelectObject(d.hDC,dot);
                Ellipse(d.hDC,cx-2,cy-2,cx+3,cy+3);
                SelectObject(d.hDC,oldBrush); DeleteObject(dot);
            }
            else
            {
                int dx = cell%3-anchor%3, dy=cell/3-anchor/3;
                dx = (dx>0)-(dx<0); dy=(dy>0)-(dy<0);
                int ex=cx+dx*4, ey=cy+dy*4;
                MoveToEx(d.hDC,cx-dx*3,cy-dy*3,NULL); LineTo(d.hDC,ex,ey);
                MoveToEx(d.hDC,ex-dx*3-dy*2,ey-dy*3+dx*2,NULL); LineTo(d.hDC,ex,ey);
                LineTo(d.hDC,ex-dx*3+dy*2,ey-dy*3-dx*2);
            }
            SelectObject(d.hDC,oldPen); DeleteObject(pen);
        }
        if (d.itemState & ODS_FOCUS) { r=d.rcItem; InflateRect(&r,-2,-2); DrawFocusRect(d.hDC,&r); }
    }
    void ChooseFill()
    {
        static COLORREF customColors[16] = {};
        COLORREF before = FillColor();
        CHOOSECOLORW cc = {sizeof(cc)};
        cc.hwndOwner=window; cc.lpCustColors=customColors; cc.rgbResult=before; cc.Flags=CC_FULLOPEN|CC_RGBINIT;
        if (ChooseColorW(&cc)) { custom=cc.rgbResult; colorIndex=5; }
        SendDlgItemMessageW(window, IDC_CANVAS_FILL, CB_SETCURSEL, colorIndex, 0);
        InvalidateRect(GetDlgItem(window,IDC_FILL_SWATCH),NULL,TRUE);
        Refresh();
    }
    INT_PTR Message(UINT msg, WPARAM wp, LPARAM lp)
    {
        switch(msg)
        {
        case WM_INITDIALOG: Init(); return TRUE;
        case WM_CLOSE: EndDialog(window,IDCANCEL); return TRUE;
        case WM_DRAWITEM: Draw(*reinterpret_cast<DRAWITEMSTRUCT*>(lp)); return TRUE;
        case WM_NOTIFY:
        {
            NMUPDOWN* n=reinterpret_cast<NMUPDOWN*>(lp);
            if (crop && n->hdr.code==UDN_DELTAPOS)
            {
                int i=static_cast<int>(n->hdr.idFrom)-IDC_SPIN_LEFT, value=0;
                if (i < 0 || i > 3) break;
                Read(IDC_EDGE_LEFT+i,value);
                int other=0; Read(IDC_EDGE_LEFT+(i^1),other);
                int size=i<2?width:height;
                int lower=max(-32767,size-other-32767);
                int upper=min(32767,size-other-1);
                // With the ascending UDM_SETRANGE32 range, the upper button emits a positive delta.
                value += (i==2 ? -n->iDelta : n->iDelta);
                Number(IDC_EDGE_LEFT+i,max(lower,min(upper,value)));
                SetWindowLongPtrW(window,DWLP_MSGRESULT,TRUE);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND:
        {
            int id=LOWORD(wp), code=HIWORD(wp);
            if (id==IDCANCEL) { EndDialog(window,IDCANCEL); return TRUE; }
            if (id==IDOK)
            {
                int w,h,x,y;
                if (!Dimensions(w,h,x,y)) { Refresh(); return TRUE; }
                if (w==width && h==height && x==0 && y==0) { EndDialog(window,IDCANCEL); return TRUE; }
                output=MakeCanvasBitmap(snapshotDC,width,height,w,h,x,y,FillColor());
                if (!output) { ShowOutOfMemory(); return TRUE; }
                EndDialog(window,IDOK); return TRUE;
            }
            if (!ready) break;
            if (code==EN_CHANGE) { Refresh(); return TRUE; }
            if (id==IDC_RELATIVE)
            {
                int w=width,h=height,x,y;
                bool valid=Dimensions(w,h,x,y);
                relative=IsDlgButtonChecked(window,IDC_RELATIVE)==BST_CHECKED;
                ready=false;
                Number(IDC_NEW_WIDTH,valid ? w-(relative?width:0) : (relative?0:width));
                Number(IDC_NEW_HEIGHT,valid ? h-(relative?height:0) : (relative?0:height));
                ready=true; Refresh(); return TRUE;
            }
            if (id>=IDC_ANCHOR_FIRST && id<IDC_ANCHOR_FIRST+9)
            {
                anchor=id-IDC_ANCHOR_FIRST;
                for(int i=0;i<9;++i) InvalidateRect(GetDlgItem(window,IDC_ANCHOR_FIRST+i),NULL,TRUE);
                return TRUE;
            }
            if (id==IDC_CANVAS_FILL && code==CBN_SELCHANGE)
            {
                int next=static_cast<int>(SendDlgItemMessageW(window,id,CB_GETCURSEL,0,0));
                if(next==5) ChooseFill();
                else { colorIndex=next; InvalidateRect(GetDlgItem(window,IDC_FILL_SWATCH),NULL,TRUE); Refresh(); }
                return TRUE;
            }
            if(id==IDC_FILL_SWATCH) { ChooseFill(); return TRUE; }
            break;
        }
        }
        return FALSE;
    }
    static INT_PTR CALLBACK Proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
    {
        CanvasDialog* self=reinterpret_cast<CanvasDialog*>(GetWindowLongPtrW(hwnd,DWLP_USER));
        if(msg==WM_INITDIALOG)
        {
            self=reinterpret_cast<CanvasDialog*>(lp); self->window=hwnd;
            SetWindowLongPtrW(hwnd,DWLP_USER,lp);
        }
        return self ? self->Message(msg,wp,lp) : FALSE;
    }
};

void ShowCanvasDialog(HWND owner, bool cropping)
{
    CanvasDialog dialog(cropping);
    if (!dialog.snapshot || !dialog.snapshotDC) { ShowOutOfMemory(); return; }
    if (DialogBoxParamW(g_hinstExe,MAKEINTRESOURCEW(cropping?IDD_CROP_EDGES:IDD_CANVAS_SIZE),
                        owner,CanvasDialog::Proc,reinterpret_cast<LPARAM>(&dialog)) != IDOK) return;
    // Finish pending text/selection only after OK, so Cancel leaves the document untouched.
    toolsModel.OnEndDraw(FALSE);
    imageModel.PushImageForUndo(dialog.output);
    dialog.output=NULL;
    canvasWindow.updateScrollRange();
    canvasWindow.Invalidate(FALSE);
}
