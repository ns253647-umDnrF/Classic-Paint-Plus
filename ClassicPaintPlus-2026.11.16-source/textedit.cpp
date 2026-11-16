/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Text editor and font chooser for the text tool
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 */

#include "precomp.h"
#include <richedit.h>

#define CXY_GRIP 3

CTextEditWindow textEditWindow;

/* FUNCTIONS ********************************************************/

CTextEditWindow::CTextEditWindow()
    : m_hFont(NULL)
    , m_hFontZoomed(NULL)
{
    SetRectEmpty(&m_rc);
}

void CTextEditWindow::SuppressNativeCaret()
{
    // The RichEdit control supplies layout/input, while the canvas draws the
    // rotated caret. RichEdit can recreate its native caret on many messages.
    // Check ownership/visibility so we never hide another edit's caret or
    // accumulate unmatched HideCaret calls on an already hidden caret.
    GUITHREADINFO info={sizeof(info)};
    if(GetGUIThreadInfo(GetCurrentThreadId(),&info) && info.hwndCaret==m_hWnd &&
       (info.flags&GUI_CARETBLINKING)) ::HideCaret(m_hWnd);
}
LRESULT CTextEditWindow::DefWindowProc(UINT message,WPARAM wp,LPARAM lp)
{
    LRESULT result=CWindowImpl<CTextEditWindow>::DefWindowProc(message,wp,lp);
    SuppressNativeCaret();return result;
}
LRESULT CTextEditWindow::OnNativeMessage(UINT message,WPARAM wp,LPARAM lp,BOOL&)
{ return DefWindowProc(message,wp,lp); }

INT CTextEditWindow::DoHitTest(RECT& rc, POINT pt)
{
    switch (getSizeBoxHitTest(pt, &rc))
    {
        case HIT_NONE:          return HTNOWHERE;
        case HIT_UPPER_LEFT:    return HTTOPLEFT;
        case HIT_UPPER_CENTER:  return HTTOP;
        case HIT_UPPER_RIGHT:   return HTTOPRIGHT;
        case HIT_MIDDLE_LEFT:   return HTLEFT;
        case HIT_MIDDLE_RIGHT:  return HTRIGHT;
        case HIT_LOWER_LEFT:    return HTBOTTOMLEFT;
        case HIT_LOWER_CENTER:  return HTBOTTOM;
        case HIT_LOWER_RIGHT:   return HTBOTTOMRIGHT;
        case HIT_BORDER:        return HTCAPTION; // Enable drag move
        case HIT_INNER:         return HTCLIENT;
    }
    return HTNOWHERE;
}

void CTextEditWindow::DrawGrip(HDC hDC, RECT& rc)
{
    drawSizeBoxes(hDC, &rc, TRUE, NULL);
}

void CTextEditWindow::FixEditPos(LPCWSTR)
{
    // Measure in document pixels. Scrolling and rounded screen coordinates must
    // never crop or move the editable box while typing or changing a font.
    if(!IsWindow() || !m_hFont || IsRectEmpty(&m_rc) || m_layout || m_dragHit!=HIT_NONE) return;
    CStringW text;GetWindowText(text);
    // An empty box and a trailing newline still need one full line. Do not add
    // a phantom character to other text: it can force a spurious extra line.
    if(text.IsEmpty() || text[text.GetLength()-1]==L'\r' || text[text.GetLength()-1]==L'\n') text+=L"x";
    HDC dc=::CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,m_hFont);
    RECT measured={0,0,max(1,m_rc.right-m_rc.left-GRIP_SIZE),0};TEXTMETRIC tm={};GetTextMetrics(dc,&tm);
    DrawTextW(dc,text,-1,&measured,DT_LEFT|DT_TOP|DT_EDITCONTROL|DT_NOPREFIX|DT_EXPANDTABS|DT_WORDBREAK|DT_CALCRECT);
    SelectObject(dc,old);DeleteDC(dc);
    int height=min(MAXWORD,measured.bottom+tm.tmDescent+GRIP_SIZE),delta=height-(m_rc.bottom-m_rc.top);
    if(delta>0 && !m_fixedHeight)
    {
        double radians=m_angle*M_PI/180;POINT change={LONG(lround(-sin(radians)*delta)),LONG(lround(cos(radians)*delta))};
        CRect next=DragEditFrame(m_rc,HIT_LOWER_CENTER,POINT{},change,m_angle,12,12);ValidateEditRect(&next);
    }
    DefWindowProc(WM_HSCROLL,SB_LEFT,0);DefWindowProc(WM_VSCROLL,SB_TOP,0);
    canvasWindow.Invalidate(FALSE);
}

LRESULT CTextEditWindow::OnChar(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (wParam == VK_TAB)
        return 0; // FIXME: Tabs

    CStringW szText;
    GetWindowText(szText);

    LRESULT ret = DefWindowProc(nMsg, wParam, lParam);
    FixEditPos(szText);
    InvalidateEditRect();

    return ret;
}

LRESULT CTextEditWindow::OnKeyDown(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if(m_dragHit!=HIT_NONE) { if(wParam==VK_ESCAPE) EndFrameDrag(true);return 0; }
    if(m_rotating) { if(wParam==VK_ESCAPE) EndRotation(true);else if(wParam==VK_SHIFT) RotateTo(m_lastPointer);return 0; }
    if (wParam == VK_ESCAPE)
    {
        toolsModel.OnEndDraw(TRUE);
        return 0;
    }

    CStringW szText;
    GetWindowText(szText);

    LRESULT ret = DefWindowProc(nMsg, wParam, lParam);
    FixEditPos(szText);
    InvalidateEditRect();
    return ret;
}

LRESULT CTextEditWindow::OnLButtonDown(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    LRESULT ret = DefWindowProc(nMsg, wParam, lParam);
    DefWindowProc(WM_HSCROLL, SB_LEFT, 0);
    DefWindowProc(WM_VSCROLL, SB_TOP, 0);
    return ret;
}

LRESULT CTextEditWindow::OnEraseBkGnd(UINT, WPARAM, LPARAM, BOOL&)
{ return TRUE; }
LRESULT CTextEditWindow::OnPaint(UINT message, WPARAM, LPARAM, BOOL&)
{ if(message==WM_PAINT) ValidateRect(NULL);return 0; }
LRESULT CTextEditWindow::OnNCPaint(UINT, WPARAM, LPARAM, BOOL&)
{ return 0; }

LRESULT CTextEditWindow::OnNCCalcSize(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    return 0; // No frame.
}

LRESULT CTextEditWindow::OnNCHitTest(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    RECT rc;
    GetWindowRect(&rc);
    return DoHitTest(rc, pt);
}

LRESULT CTextEditWindow::OnSetCursor(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (CWaitCursor::IsWaiting())
    {
        bHandled = FALSE;
        return 0;
    }

    UINT nHitTest = LOWORD(lParam);
    if (nHitTest == HTCAPTION)
    {
        ::SetCursor(::LoadCursorW(NULL, (LPCWSTR)IDC_SIZEALL)); // Enable drag move
        return FALSE;
    }
    return DefWindowProc(nMsg, wParam, lParam);
}

LRESULT CTextEditWindow::OnMove(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    LRESULT ret = DefWindowProc(nMsg, wParam, lParam);
    if(!m_layout) InvalidateEditRect();
    return ret;
}

LRESULT CTextEditWindow::OnSize(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    LRESULT ret = DefWindowProc(nMsg, wParam, lParam);

    RECT rc;
    GetClientRect(&rc);
    InflateRect(&rc,-Zoomed(GRIP_SIZE/2),-Zoomed(GRIP_SIZE/2));
    SendMessage(EM_SETRECTNP, 0, (LPARAM)&rc);
    SendMessage(EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(0, 0));
    if(!m_layout) InvalidateEditRect();

    return ret;
}

// Hack: Use DECLARE_WND_SUPERCLASS instead!
HWND CTextEditWindow::Create(HWND hwndParent)
{
    m_hwndParent = hwndParent;

    const DWORD style = ES_LEFT | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL |
                        WS_CHILD | WS_THICKFRAME;
    static HMODULE rich=[](){ WCHAR path[MAX_PATH];GetSystemDirectoryW(path,MAX_PATH);StringCchCatW(path,MAX_PATH,L"\\msftedit.dll");return LoadLibraryW(path); }();
    if(!rich) return NULL;
    HWND hwnd = ::CreateWindowEx(0, MSFTEDIT_CLASS, NULL, style, 0, 0, 0, 0,
                                 hwndParent, NULL, g_hinstExe, NULL);
    if (hwnd)
    {
#undef SubclassWindow // Don't use this macro
        SubclassWindow(hwnd);
        SendMessage(EM_SETTEXTMODE,TM_PLAINTEXT,0);
        UpdateRegion();

        UpdateFont();

        PostMessage(WM_SIZE, 0, 0);
    }

    return m_hWnd;
}

void CTextEditWindow::DoFillBack(HWND hwnd, HDC hDC)
{
    if (toolsModel.IsBackgroundTransparent())
        return;

    RECT rc={};
    SendMessage(EM_GETRECT, 0, (LPARAM)&rc);
    MapWindowPoints(hwnd, (LPPOINT)&rc, 2);

    HBRUSH hbr = CreateSolidBrush(paletteModel.GetBgColor());
    FillRect(hDC, &rc, hbr);
    DeleteObject(hbr);
}

LRESULT CTextEditWindow::OnCreate(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    UpdateFont();
    return 0;
}

LRESULT CTextEditWindow::OnClose(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    ShowWindow(SW_HIDE);
    if (m_hFont)
    {
        DeleteObject(m_hFont);
        m_hFont = NULL;
    }
    if (m_hFontZoomed)
    {
        DeleteObject(m_hFontZoomed);
        m_hFontZoomed = NULL;
    }
    return 0;
}

void CTextEditWindow::InvalidateEditRect()
{
    if(!IsWindow() || m_layout) return;
    m_caretVisible=true;canvasWindow.Invalidate(FALSE);
}

LRESULT CTextEditWindow::OnPaletteModelColorChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    UpdateFont();
    return 0;
}

LRESULT CTextEditWindow::OnToolsModelSettingsChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    UpdateFont();
    return 0;
}

LRESULT CTextEditWindow::OnToolsModelZoomChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    RECT original=m_rc;
    UpdateFont();
    ValidateEditRect(&original);
    return 0;
}

LRESULT CTextEditWindow::OnToolsModelToolChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (wParam == TOOL_TEXT || (wParam==TOOL_ZOOM && toolsModel.GetEditingTool()==TOOL_TEXT))
    {
        UpdateFont();
    }
    else
    {
        ShowWindow(SW_HIDE);
    }
    return 0;
}

void CTextEditWindow::UpdateFont()
{
    if (m_hFont)
    {
        DeleteObject(m_hFont);
        m_hFont = NULL;
    }
    if (m_hFontZoomed)
    {
        DeleteObject(m_hFontZoomed);
        m_hFontZoomed = NULL;
    }

    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET; // registrySettings.CharSet; // Ignore
    lf.lfWeight = (registrySettings.Bold ? FW_BOLD : FW_NORMAL);
    lf.lfItalic = (BYTE)registrySettings.Italic;
    lf.lfUnderline = (BYTE)registrySettings.Underline;
    StringCchCopyW(lf.lfFaceName, _countof(lf.lfFaceName), registrySettings.strFontName);

    HDC hdc = GetDC();
    if (hdc)
    {
        INT nFontSize = registrySettings.PointSize;
        lf.lfHeight = -MulDiv(nFontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
        ReleaseDC(hdc);
    }

    m_hFont = ::CreateFontIndirect(&lf);

    lf.lfHeight = Zoomed(lf.lfHeight);
    m_hFontZoomed = ::CreateFontIndirect(&lf);

    SetWindowFont(m_hWnd,m_hFont,TRUE);
    SendMessage(EM_SETZOOM,toolsModel.GetZoom(),1000);
    CHARFORMAT2W cf={};cf.cbSize=sizeof(cf);cf.dwMask=CFM_FACE|CFM_SIZE|CFM_BOLD|CFM_ITALIC|CFM_UNDERLINE|CFM_COLOR;
    cf.yHeight=registrySettings.PointSize*20;cf.crTextColor=paletteModel.GetFgColor();
    cf.dwEffects=(registrySettings.Bold?CFE_BOLD:0)|(registrySettings.Italic?CFE_ITALIC:0)|(registrySettings.Underline?CFE_UNDERLINE:0);
    StringCchCopyW(cf.szFaceName,LF_FACESIZE,registrySettings.strFontName);
    SendMessage(EM_SETCHARFORMAT,SCF_ALL,reinterpret_cast<LPARAM>(&cf));
    SendMessage(EM_SETCHARFORMAT,SCF_DEFAULT,reinterpret_cast<LPARAM>(&cf));
    CHARRANGE selection;SendMessage(EM_EXGETSEL,0,reinterpret_cast<LPARAM>(&selection));
    CHARRANGE all={0,-1};SendMessage(EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&all));
    PARAFORMAT2 pf={};pf.cbSize=sizeof(pf);pf.dwMask=PFM_ALIGNMENT;const WORD align[]={PFA_LEFT,PFA_CENTER,PFA_RIGHT,PFA_JUSTIFY};
    pf.wAlignment=align[min(3u,registrySettings.TextAlignment)];SendMessage(EM_SETPARAFORMAT,0,reinterpret_cast<LPARAM>(&pf));
    SendMessage(EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&selection));
    SendMessage(EM_SETBKGNDCOLOR,0,paletteModel.GetBgColor());
    DefWindowProc(EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(0, 0));

    FixEditPos(NULL);

    Invalidate(FALSE);
    canvasWindow.Invalidate(FALSE);
}

LRESULT CTextEditWindow::OnSetSel(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    LRESULT ret = DefWindowProc(nMsg, wParam, lParam);
    DefWindowProc(WM_HSCROLL, SB_LEFT, 0);
    DefWindowProc(WM_VSCROLL, SB_TOP, 0);
    InvalidateEditRect();
    return ret;
}

BOOL CTextEditWindow::GetEditRect(LPRECT prc) const
{
    *prc = m_rc;
    return TRUE;
}

void CTextEditWindow::ValidateEditRect(LPCRECT prc OPTIONAL)
{
    if (prc)
        m_rc = *prc;

    CRect rc = m_rc;
    canvasWindow.ImageToCanvas(rc);

    m_layout=true;MoveWindow(rc.left,rc.top,rc.Width(),rc.Height(),FALSE);m_layout=false;
    UpdateRegion();
    toolSettingsWindow.UpdateObjectDimensions();
}

LRESULT CTextEditWindow::OnMoving(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    // Restrict the window position to the image area
    LPRECT prcMoving = (LPRECT)lParam;
    CRect rcMoving = *prcMoving;

    CRect rcImage;
    canvasWindow.GetImageRect(rcImage);
    canvasWindow.ImageToCanvas(rcImage);
    canvasWindow.MapWindowPoints(NULL, &rcImage);

    CRect rcWnd;
    GetWindowRect(&rcWnd);
    INT cx = rcWnd.Width(), cy = rcWnd.Height();

    if (rcMoving.left < rcImage.left)
    {
        rcMoving.left = rcImage.left;
        rcMoving.right = rcImage.left + cx;
    }
    else if (rcMoving.right > rcImage.right)
    {
        rcMoving.right = rcImage.right;
        rcMoving.left = rcImage.right - cx;
    }

    if (rcMoving.top < rcImage.top)
    {
        rcMoving.top = rcImage.top;
        rcMoving.bottom = rcImage.top + cy;
    }
    else if (rcMoving.bottom > rcImage.bottom)
    {
        rcMoving.bottom = rcImage.bottom;
        rcMoving.top = rcImage.bottom - cy;
    }

    *prcMoving = rcMoving;
    Invalidate(FALSE);
    return TRUE;
}

LRESULT CTextEditWindow::OnSizing(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    // Restrict the window size to the image area
    LPRECT prcSizing = (LPRECT)lParam;
    CRect rcSizing = *prcSizing;

    CRect rcImage;
    canvasWindow.GetImageRect(rcImage);
    canvasWindow.ImageToCanvas(rcImage);
    canvasWindow.MapWindowPoints(NULL, &rcImage);

    // Horizontally
    switch (wParam)
    {
        case WMSZ_BOTTOMLEFT:
        case WMSZ_LEFT:
        case WMSZ_TOPLEFT:
            if (rcSizing.left < rcImage.left)
                rcSizing.left = rcImage.left;
            break;
        case WMSZ_BOTTOMRIGHT:
        case WMSZ_RIGHT:
        case WMSZ_TOPRIGHT:
            if (rcSizing.right > rcImage.right)
                rcSizing.right = rcImage.right;
            break;
        case WMSZ_TOP:
        case WMSZ_BOTTOM:
        default:
            break;
    }

    // Vertically
    switch (wParam)
    {
        case WMSZ_BOTTOM:
        case WMSZ_BOTTOMLEFT:
        case WMSZ_BOTTOMRIGHT:
            if (rcSizing.bottom > rcImage.bottom)
                rcSizing.bottom = rcImage.bottom;
            break;
        case WMSZ_TOP:
        case WMSZ_TOPLEFT:
        case WMSZ_TOPRIGHT:
            if (rcSizing.top < rcImage.top)
                rcSizing.top = rcImage.top;
            break;
        case WMSZ_LEFT:
        case WMSZ_RIGHT:
        default:
            break;
    }

    *prcSizing = rcSizing;
    Invalidate(FALSE);
    return TRUE;
}

LRESULT CTextEditWindow::OnMouseWheel(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    return ::SendMessageW(GetParent(), nMsg, wParam, lParam);
}

LRESULT CTextEditWindow::OnCut(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    LRESULT ret = DefWindowProc(nMsg, wParam, lParam);
    InvalidateEditRect();
    Invalidate(FALSE); // Redraw
    return ret;
}

LRESULT CTextEditWindow::OnPaste(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    LRESULT ret = DefWindowProc(nMsg, wParam, lParam);
    FixEditPos(NULL);
    InvalidateEditRect();
    return ret;
}

LRESULT CTextEditWindow::OnClear(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    LRESULT ret = DefWindowProc(nMsg, wParam, lParam);
    InvalidateEditRect();
    Invalidate(FALSE); // Redraw
    return ret;
}
