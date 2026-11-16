// Local extension, 2026-11-13. Licensed under LGPL-2.0-or-later.
#include <vector>
#include "precomp.h"
#include "colorpicker.h"
#include <colordlg.h>

namespace
{
struct PickerSession
{
    HWND owner = NULL;
    HWND dialog = NULL;
    HDC pixels = NULL;
    HBITMAP bitmap = NULL;
    HGDIOBJ oldBitmap = NULL;
    UINT setRGB = 0;
    BYTE alpha=255;
    bool sampling=false,ready=false;
    std::vector<HWND> enabledChildren;

    bool ImagePoint(POINT& point)
    {
        RECT imageRect;
        canvasWindow.GetImageRect(imageRect);
        canvasWindow.ImageToCanvas(imageRect);
        if (!::PtInRect(&imageRect, point))
            return false;
        canvasWindow.CanvasToImage(point);
        return point.x >= 0 && point.y >= 0 &&
               point.x < imageModel.GetWidth() && point.y < imageModel.GetHeight();
    }

    void Pick(LPARAM pos)
    {
        POINT point = { GET_X_LPARAM(pos), GET_Y_LPARAM(pos) };
        if (dialog && pixels && ImagePoint(point))
        {
            COLORREF color = ::GetPixel(pixels, point.x, point.y);
            if (color != CLR_INVALID)
            {
                sampling=true;alpha=BYTE(RasterPixel(pixels,point.x,point.y)>>24);
                ::SendMessageW(dialog, setRGB, 0, color);sampling=false;
            }
        }
    }
};

BOOL CALLBACK CollectChildren(HWND window, LPARAM param)
{
    auto& session = *reinterpret_cast<PickerSession*>(param);
    if (window != canvasWindow.m_hWnd && ::IsWindowEnabled(window))
        session.enabledChildren.push_back(window);
    return TRUE;
}

LRESULT CALLBACK OwnerProc(HWND window, UINT msg, WPARAM wp, LPARAM lp,
                           UINT_PTR, DWORD_PTR data)
{
    auto& session = *reinterpret_cast<PickerSession*>(data);
    switch (msg)
    {
        case WM_DROPFILES:
            ::DragFinish(reinterpret_cast<HDROP>(wp));
            return 0;
        case WM_CLOSE:
        case WM_COMMAND:
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_MOUSEWHEEL:
        case WM_CONTEXTMENU:
            if (session.dialog)
                ::SetForegroundWindow(session.dialog);
            return 0;
        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_CLOSE || (wp & 0xFFF0) == SC_KEYMENU ||
                (wp & 0xFFF0) == SC_MOUSEMENU)
                return 0;
            break;
    }
    return ::DefSubclassProc(window, msg, wp, lp);
}

LRESULT CALLBACK CanvasProc(HWND window, UINT msg, WPARAM wp, LPARAM lp,
                            UINT_PTR, DWORD_PTR data)
{
    auto& session = *reinterpret_cast<PickerSession*>(data);
    switch (msg)
    {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE; // Keep typing/Enter/Escape in the color dialog.
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT)
            {
                POINT point;
                ::GetCursorPos(&point);
                ::ScreenToClient(window, &point);
                bool inside = session.ImagePoint(point);
                ::SetCursor(inside ? ::LoadCursorW(g_hinstExe, MAKEINTRESOURCEW(IDC_COLOR))
                                   : ::LoadCursorW(NULL, IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            session.Pick(lp);
            return 0;
        case WM_MOUSEMOVE:
            if (wp & MK_LBUTTON)
                session.Pick(lp);
            return 0;
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case WM_CONTEXTMENU:
        case WM_MOUSEWHEEL:
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_CHAR:
            return 0;
    }
    return ::DefSubclassProc(window, msg, wp, lp);
}

// At the white/black luminance limits the hue plane cannot change RGB at all.
// Only a deliberate click in that plane restores mid-luminance; opening the
// picker, editing RGB or sampling the canvas must keep the exact original color.
void PrepareHueClick(HWND dialog)
{
    BOOL valid=FALSE;UINT lum=GetDlgItemInt(dialog,COLOR_LUM,&valid,FALSE);
    if(valid && (lum==0 || lum==240)) SetDlgItemInt(dialog,COLOR_LUM,120,FALSE);
}
LRESULT CALLBACK HuePlaneProc(HWND window,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR)
{
    if(msg==WM_LBUTTONDOWN || msg==WM_LBUTTONDBLCLK) PrepareHueClick(GetParent(window));
    return DefSubclassProc(window,msg,wp,lp);
}
UINT_PTR CALLBACK ColorHook(HWND dialog, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG)
    {
        auto* choose = reinterpret_cast<CHOOSECOLORW*>(lp);
        auto* session = reinterpret_cast<PickerSession*>(choose->lCustData);
        session->dialog = dialog;
        ::SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(session));
        ::PostMessageW(dialog,WM_APP+118,0,0);
        HWND plane=GetDlgItem(dialog,COLOR_RAINBOW);
        if(plane) SetWindowSubclass(plane,HuePlaneProc,1,0);
        // ChooseColor disables its owner. Re-enable only for the guarded canvas;
        // all editing controls and owner commands stay blocked for this session.
        ::EnableWindow(session->owner, TRUE);
    }
    else if(msg==WM_APP+118)
    {
        auto* session=reinterpret_cast<PickerSession*>(::GetWindowLongPtrW(dialog,DWLP_USER));if(session) session->ready=true;
    }
    else if(msg==WM_LBUTTONDOWN || msg==WM_LBUTTONDBLCLK)
    {
        HWND plane=GetDlgItem(dialog,COLOR_RAINBOW);RECT r;
        if(plane && GetWindowRect(plane,&r))
        {
            MapWindowPoints(NULL,dialog,reinterpret_cast<POINT*>(&r),2);
            POINT point={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};if(PtInRect(&r,point)) PrepareHueClick(dialog);
        }
    }
    else if(msg==WM_COMMAND && HIWORD(wp)==EN_CHANGE)
    {
        auto* session=reinterpret_cast<PickerSession*>(::GetWindowLongPtrW(dialog,DWLP_USER));
        if(session && session->ready && !session->sampling) session->alpha=255;
    }
    else if (msg == WM_DESTROY)
    {
        auto* session = reinterpret_cast<PickerSession*>(::GetWindowLongPtrW(dialog, DWLP_USER));
        if (session)
        {
            session->dialog = NULL;
            ::EnableWindow(session->owner, FALSE);
        }
    }
    return 0;
}
}

BOOL ChooseColorFromCanvas(HWND owner, COLORREF* color,BYTE* alpha)
{
    static COLORREF customColors[16] = {
        0xffffff, 0xffffff, 0xffffff, 0xffffff, 0xffffff, 0xffffff, 0xffffff, 0xffffff,
        0xffffff, 0xffffff, 0xffffff, 0xffffff, 0xffffff, 0xffffff, 0xffffff, 0xffffff };
    PickerSession session;
    session.alpha=alpha?*alpha:255;
    session.owner = owner;
    session.setRGB = ::RegisterWindowMessageW(SETRGBSTRINGW);
    session.bitmap = imageModel.CopyBitmap();
    session.pixels = ::CreateCompatibleDC(NULL);
    if (!session.bitmap || !session.pixels)
    {
        if (session.bitmap) ::DeleteObject(session.bitmap);
        if (session.pixels) ::DeleteDC(session.pixels);
        ShowOutOfMemory();
        return FALSE;
    }
    session.oldBitmap = ::SelectObject(session.pixels, session.bitmap);
    // Include floating selections/shapes, but not grid lines or selection handles.
    toolsModel.OnDrawOverlayOnImage(session.pixels);

    ::EnumChildWindows(owner, CollectChildren, reinterpret_cast<LPARAM>(&session));
    for (HWND child : session.enabledChildren)
        ::EnableWindow(child, FALSE);
    BOOL ownerSubclass = ::SetWindowSubclass(owner, OwnerProc, 1, reinterpret_cast<DWORD_PTR>(&session));
    BOOL canvasSubclass = ::SetWindowSubclass(canvasWindow, CanvasProc, 1, reinterpret_cast<DWORD_PTR>(&session));
    CHOOSECOLORW choose = { sizeof(choose) };
    choose.hwndOwner = owner;
    choose.lpCustColors = customColors;
    choose.rgbResult = *color;
    choose.Flags = CC_RGBINIT | CC_FULLOPEN;
    if (ownerSubclass && canvasSubclass)
    {
        choose.Flags |= CC_ENABLEHOOK;
        choose.lpfnHook = ColorHook;
        choose.lCustData = reinterpret_cast<LPARAM>(&session);
    }
    BOOL accepted = ::ChooseColorW(&choose);
    if (canvasSubclass) ::RemoveWindowSubclass(canvasWindow, CanvasProc, 1);
    if (ownerSubclass) ::RemoveWindowSubclass(owner, OwnerProc, 1);
    ::EnableWindow(owner, TRUE);
    for (HWND child : session.enabledChildren)
        if (::IsWindow(child)) ::EnableWindow(child, TRUE);
    ::SelectObject(session.pixels, session.oldBitmap);
    ::DeleteDC(session.pixels);
    ::DeleteObject(session.bitmap);
    if (accepted)
    { *color = choose.rgbResult;if(alpha) *alpha=session.alpha; }
    return accepted;
}
