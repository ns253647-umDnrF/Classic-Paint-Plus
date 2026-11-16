/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Window procedure of the tool settings window
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 */

#pragma once
#include "object_actions_resource.h"
#include "exactspray_resource.h"
#include "transformcontrols_resource.h"

class CToolSettingsWindow : public CWindowImpl<CToolSettingsWindow>
{
public:
    DECLARE_WND_CLASS_EX(L"ToolSettings", CS_DBLCLKS, COLOR_BTNFACE)

    BEGIN_MSG_MAP(CToolSettingsWindow)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_COMMAND, OnCommand)
        MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
        MESSAGE_HANDLER(WM_VSCROLL, OnVScroll)
        MESSAGE_HANDLER(WM_HSCROLL, OnVScroll)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
        MESSAGE_HANDLER(WM_NOTIFY, OnNotify)
        MESSAGE_HANDLER(WM_TOOLSMODELTOOLCHANGED, OnToolsModelToolChanged)
        MESSAGE_HANDLER(WM_TOOLSMODELSETTINGSCHANGED, OnToolsModelSettingsChanged)
        MESSAGE_HANDLER(WM_TOOLSMODELCOLORPICKED, OnToolsModelColorPicked)
        MESSAGE_HANDLER(WM_TOOLSMODELZOOMCHANGED, OnToolsModelZoomChanged)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    END_MSG_MAP()

    BOOL DoCreate(HWND hwndParent);
    HWND GetDlgItem(int id) const { HWND window=::GetDlgItem(m_hWnd,id);return window?window:ParameterControl(id); }
    void MoveGeometryControls(HWND parent);
    void ParameterCheckChanged(int parameter,bool fixed);
    void ApplyScale();
    void UpdateSelectionTransparency(bool forceText=false);
    void UpdateSelectionAngle(bool forceText=false);
    void ReloadLanguage();
    void UpdateLineWidth(bool forceText=false);
    void UpdateObjectDimensions(bool forceText=false);

private:
    CWindow trackbarZoom;
    CWindow zoomPresets;
    CWindow zoomEdit;
    HFONT panelFont=NULL;
    bool m_applyingZoom=false;
    bool ApplyZoomText(bool finish=false);
    void UpdateZoomText();
    static LRESULT CALLBACK ZoomEditProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    CWindow trackbarTransparency, transparencyEdit;
    CStringW transparencyLabel;
    CStringW sprayLabel;
    CWindow trackbarSpray,sprayEdit;
    CStringW spraySmoothingLabel;
    void UpdateSpraySmoothing(bool forceText=false);
    bool ApplySprayText();
    static LRESULT CALLBACK SprayEditProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    CWindow transformSeparator,skewEdit[2],skewCaption[2];
    CStringW skewLabel[2],lastSkewText[2];
    void CreateSkewControls(TOOLINFOW& info);
    void UpdateSkew(bool forceText=false);
    bool ApplySkewText(HWND edit);
    static LRESULT CALLBACK SkewEditProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    CWindow angleEdit;
    CStringW angleLabel,lastAngleText;
    bool ApplyAngleText();
    CWindow objectWidth,objectHeight,dimensionX;
    bool m_dimensionOmitted[2]={},m_dimensionsActive=false;
    SIZE m_dimensionDisplaySize={};
    CWindow saveSizeButton,resetSizeButton;
    CStringW saveSizeLabel,resetSizeLabel;
    CStringW objectWidthLabel,objectHeightLabel;
    bool ApplyDimensionText(HWND edit);
    static LRESULT CALLBACK DimensionEditProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    CWindow widthEdit;
    CStringW widthLabel;
    bool ApplyWidthText();
    static LRESULT CALLBACK WidthEditProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    static LRESULT CALLBACK AngleEditProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    bool ApplyTransparencyText();
    static LRESULT CALLBACK TransparencyEditProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    CWindow trackbarRounding;
    HWND roundingTip = NULL;
    CStringW roundingLabel;
    CStringW zoomLabel;
    void UpdateRoundingLabel();
    HICON m_hNontranspIcon;
    HICON m_hTranspIcon;
    COLORREF m_rgbPickColor = CLR_INVALID;

    VOID drawTrans(HDC hdc, LPCRECT prc);
    VOID drawRubber(HDC hdc, LPCRECT prc);
    VOID drawBrush(HDC hdc, LPCRECT prc);
    VOID drawLine(HDC hdc, LPCRECT prc);
    VOID drawBox(HDC hdc, LPCRECT prc);
    VOID drawAirBrush(HDC hdc, LPCRECT prc);
    VOID calculateTwoBoxes(CRect& rect1, CRect& rect2);

    LRESULT OnCreate(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnCommand(UINT,WPARAM,LPARAM,BOOL&);
    LRESULT OnDrawItem(UINT,WPARAM,LPARAM,BOOL&);
    LRESULT OnDestroy(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnVScroll(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnPaint(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnLButtonDown(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnNotify(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnToolsModelToolChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnToolsModelSettingsChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnToolsModelColorPicked(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnToolsModelZoomChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
};
