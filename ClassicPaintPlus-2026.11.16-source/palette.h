/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Window procedure of the palette window
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 */

#pragma once
#include "object_actions_resource.h"

#define CXY_COLORBOX   PanelPx(14)
#define CXY_BIGBOX     PanelPx(32)
#define CY_PALETTE     64
#define PALETTE_EXTRA_GAP 6
#define PALETTE_FULL_WIDTH (340+PALETTE_EXTRA_GAP)

class CPaletteWindow : public CWindowImpl<CPaletteWindow>
{
public:
    DECLARE_WND_CLASS_EX(L"Palette", CS_DBLCLKS, COLOR_BTNFACE)

    BEGIN_MSG_MAP(CPaletteWindow)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_COMMAND, OnCommand)
        MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
        MESSAGE_HANDLER(WM_RBUTTONDOWN, OnRButtonDown)
        MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnLButtonDblClk)
        MESSAGE_HANDLER(WM_RBUTTONDBLCLK, OnRButtonDblClk)
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
        MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
        MESSAGE_HANDLER(WM_CAPTURECHANGED, OnCaptureChanged)
        MESSAGE_HANDLER(WM_PALETTEMODELCOLORCHANGED, OnPaletteModelColorChanged)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_HSCROLL, OnHScroll)
        MESSAGE_HANDLER(WM_MOUSEWHEEL, OnHScroll)
        MESSAGE_HANDLER(WM_NOTIFY, OnNotify)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    END_MSG_MAP()

    CPaletteWindow();
    virtual ~CPaletteWindow();

protected:
    HBITMAP m_hbmCached; // Cached buffer bitmap
    BOOL m_swapPressed;
    HWND m_tip=NULL;
    CStringW m_tipText;
    CWindow m_copyButtons[5];
    void LayoutCopyButtons();
    LRESULT OnCreate(UINT,WPARAM,LPARAM,BOOL&);
    LRESULT OnCommand(UINT,WPARAM,LPARAM,BOOL&);
    LRESULT OnDrawItem(UINT,WPARAM,LPARAM,BOOL&);
    void UpdateTransparencyTips();
    LRESULT OnNotify(UINT,WPARAM,LPARAM,BOOL&);
    LRESULT OnDestroy(UINT,WPARAM,LPARAM,BOOL&);
    LRESULT OnSize(UINT,WPARAM,LPARAM,BOOL&);
    LRESULT OnHScroll(UINT,WPARAM,LPARAM,BOOL&);

    LRESULT OnEraseBkgnd(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnPaint(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnLButtonDown(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnRButtonDown(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnLButtonDblClk(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnRButtonDblClk(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnMouseMove(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnLButtonUp(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnCaptureChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnPaletteModelColorChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

protected:
    INT DoHitTest(INT xPos, INT yPos) const;
    BOOL EditCurrentColor(INT xPos, INT yPos);
};
