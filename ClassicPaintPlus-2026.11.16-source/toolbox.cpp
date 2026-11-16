/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Window procedure of the main window and all children apart from
 *             hPalWin, hToolSettings and hSelection
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 */

#include "precomp.h"

CToolBox toolBoxContainer;

/* FUNCTIONS ********************************************************/

LRESULT CALLBACK
CPaintToolBar::ToolBarWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WNDPROC oldWndProc = (WNDPROC)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (uMsg == WM_LBUTTONDBLCLK)
    {
        POINT pt={GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};
        INT index=INT(::SendMessageW(hwnd,TB_HITTEST,0,LPARAM(&pt)));
        TBBUTTON button={};
        if(index>=0 && ::SendMessageW(hwnd,TB_GETBUTTON,index,LPARAM(&button)) &&
           (button.idCommand==ID_RECTSEL || button.idCommand==ID_FREESEL))
        {
            // Commit floating pixels just as changing to a drawing tool does.
            // A single click still preserves the contour between selection tools.
            toolsModel.OnEndDraw(FALSE);selectionModel.Landing();
            toolsModel.SetActiveTool(button.idCommand==ID_RECTSEL?TOOL_RECTSEL:TOOL_FREESEL);
        }
    }
    if (uMsg == WM_LBUTTONUP)
    {
        // We have to detect clicking on toolbar even if no change of pressed button
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        INT index = (INT)::SendMessageW(hwnd, TB_HITTEST, 0, (LPARAM)&pt);
        if (index >= 0)
        {
            TBBUTTON button;
            if (::SendMessageW(hwnd, TB_GETBUTTON, index, (LPARAM)&button))
                ::PostMessageW(::GetParent(hwnd), WM_COMMAND, button.idCommand, 0);
        }
    }
    return ::CallWindowProc(oldWndProc, hwnd, uMsg, wParam, lParam);
}

BOOL CPaintToolBar::DoCreate(HWND hwndParent)
{
    // NOTE: The horizontal line above the toolbar is hidden by CCS_NODIVIDER style.
    RECT toolbarPos =
    {
        0, PanelPx(ToolboxTopMargin()),
        CX_TOOLBAR + 2 * GetSystemMetrics(SM_CXBORDER),
        CY_TOOLBAR + 2 * GetSystemMetrics(SM_CYBORDER)
    };
    DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | CCS_NOPARENTALIGN | CCS_VERT | CCS_NORESIZE |
                  TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | CCS_NODIVIDER;
    if (!CWindow::Create(TOOLBARCLASSNAMEW, hwndParent, toolbarPos, NULL, style))
        return FALSE;

    HIMAGELIST hImageList = CreatePanelToolIcons(PanelPercent());
    SendMessage(TB_SETIMAGELIST, 0, (LPARAM)hImageList);

    SendMessage(TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

    CStringW toolTip;
    TBBUTTON tbbutton;
    ZeroMemory(&tbbutton, sizeof(tbbutton));
    tbbutton.fsStyle = TBSTYLE_CHECKGROUP;
    for (INT i = 0; i < NUM_TOOLS; i++)
    {
        if(ID_FREESEL+i>=ID_LINE)continue; // Lines and figures are grouped in the three rows below.
        toolTip.LoadString(g_hinstExe, IDS_TOOLTIP1 + i);
        tbbutton.iString   = (INT_PTR)LPCWSTR(toolTip);
        tbbutton.fsState   = TBSTATE_ENABLED | ((i % 2 == 1 || ID_FREESEL+i==ID_ELLIPSE) ? TBSTATE_WRAP : 0);
        tbbutton.idCommand = ID_FREESEL + i;
        tbbutton.iBitmap   = i;
        SendMessage(TB_ADDBUTTONS, 1, (LPARAM) &tbbutton);
    }

    SendMessage(TB_CHECKBUTTON, ID_RECTSEL, MAKELPARAM(TRUE, 0));
    SendMessage(TB_SETMAXTEXTROWS, 0, 0);
    SendMessage(TB_SETBUTTONSIZE, 0, MAKELPARAM(PanelPx(50)/2, PanelPx(CXY_TB_BUTTON)));

    SetWindowLongPtr(GWLP_USERDATA, SetWindowLongPtr(GWLP_WNDPROC, (LONG_PTR)ToolBarWndProc));
    return TRUE;
}

void CPaintToolBar::ApplyScale()
{
    HIMAGELIST list=CreatePanelToolIcons(PanelPercent());
    if(!list) return;
    HIMAGELIST old=(HIMAGELIST)SendMessage(TB_SETIMAGELIST,0,reinterpret_cast<LPARAM>(list));
    SendMessage(TB_SETBITMAPSIZE,0,MAKELPARAM(PanelPx(16),PanelPx(16)));
    SendMessage(TB_SETBUTTONSIZE,0,MAKELPARAM(PanelPx(50)/2,PanelPx(25)));
    if(old) ImageList_Destroy(old);
    Invalidate();
}
void CPaintToolBar::ReloadLanguage()
{
    HWND tip=HWND(SendMessage(TB_GETTOOLTIPS,0,0));if(tip) ::SendMessage(tip,TTM_POP,0,0);
    for(int i=0;i<NUM_TOOLS;++i)
    {
        CStringW label;label.LoadString(g_hinstExe,IDS_TOOLTIP1+i);
        TBBUTTONINFOW info={sizeof(info)};info.dwMask=TBIF_TEXT;info.pszText=label.GetBuffer();
        SendMessage(TB_SETBUTTONINFOW,ID_FREESEL+i,LPARAM(&info));label.ReleaseBuffer();
    }
}

void CToolBox::ApplyScale()
{
    scrollOffset=0;
    toolbar.ApplyScale();toolSettingsWindow.ApplyScale();LayoutFigureControls();Layout();
}
void CToolBox::ReloadLanguage() { toolbar.ReloadLanguage();toolSettingsWindow.ReloadLanguage(); }
void CToolBox::Layout()
{
    if(layingOut)return;layingOut=true;
    CRect rc;GetClientRect(&rc);
    int maximum=max(0,ToolboxContentHeight()-rc.Height());scrollOffset=min(scrollOffset,maximum);
    SCROLLINFO si={sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS,0,ToolboxContentHeight()-1,UINT(max(1,rc.Height())),scrollOffset};
    if(ToolboxWidth(rc.Height())>PanelPx(50))si.fMask|=SIF_DISABLENOSCROLL;
    SetScrollInfo(SB_VERT,&si,TRUE);
    toolbar.MoveWindow(0,PanelPx(ToolboxTopMargin())-scrollOffset,PanelPx(50),5*PanelPx(25)+2);
    LayoutFigureControls(scrollOffset);
    toolSettingsWindow.MoveWindow(0,(PanelPx(ToolboxTopMargin()+3)+ToolbarHeight())-scrollOffset,PanelPx(50),PanelPx(ToolSettingsHeight()));
    layingOut=false;
}
LRESULT CToolBox::OnSize(UINT,WPARAM,LPARAM,BOOL&) { if(toolbar.IsWindow() && toolSettingsWindow.IsWindow()) Layout();return 0; }
LRESULT CToolBox::OnVScroll(UINT,WPARAM wp,LPARAM,BOOL&)
{
    SCROLLINFO si={sizeof(si),SIF_ALL};GetScrollInfo(SB_VERT,&si);
    switch(LOWORD(wp))
    {
    case SB_LINEUP: scrollOffset-=PanelPx(25);break;
    case SB_LINEDOWN: scrollOffset+=PanelPx(25);break;
    case SB_PAGEUP: scrollOffset-=si.nPage;break;
    case SB_PAGEDOWN: scrollOffset+=si.nPage;break;
    case SB_THUMBTRACK: case SB_THUMBPOSITION: scrollOffset=si.nTrackPos;break;
    }
    scrollOffset=max(0,scrollOffset);Layout();Invalidate();return 0;
}
LRESULT CToolBox::OnMouseWheel(UINT,WPARAM wp,LPARAM,BOOL&)
{
    scrollOffset=max(0,scrollOffset-GET_WHEEL_DELTA_WPARAM(wp)*PanelPx(50)/WHEEL_DELTA);
    Layout();Invalidate();return 0;
}

BOOL CToolBox::DoCreate(HWND hwndParent)
{
    RECT rcToolBox = { 0, 0, 0, 0 }; // Rely on mainWindow's WM_SIZE
    DWORD style = WS_CHILD | WS_CLIPCHILDREN | (registrySettings.ShowToolBox ? WS_VISIBLE : 0);
    return !!Create(hwndParent, rcToolBox, NULL, style);
}

LRESULT CToolBox::OnCreate(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    toolbar.DoCreate(m_hWnd);
    CreateFigureControls(m_hWnd);
    toolSettingsWindow.DoCreate(m_hWnd);
    return 0;
}

LRESULT CToolBox::OnSysColorChange(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    toolbar.SendMessage(WM_SYSCOLORCHANGE, 0, 0);
    return 0;
}

struct COMMAND_TO_TOOL
{
    UINT id;
    TOOLTYPE tool;
};

static const COMMAND_TO_TOOL CommandToToolMapping[] =
{
    { ID_FREESEL, TOOL_FREESEL },
    { ID_RECTSEL, TOOL_RECTSEL },
    { ID_RUBBER, TOOL_RUBBER },
    { ID_FILL, TOOL_FILL },
    { ID_COLOR, TOOL_COLOR },
    { ID_ZOOM, TOOL_ZOOM },
    { ID_PEN, TOOL_PEN },
    { ID_BRUSH, TOOL_BRUSH },
    { ID_AIRBRUSH, TOOL_AIRBRUSH },
    { ID_TEXT, TOOL_TEXT },
    { ID_LINE, TOOL_LINE },
    { ID_BEZIER, TOOL_BEZIER },
    { ID_RECT, TOOL_RECT },
    { ID_SHAPE, TOOL_SHAPE },
    { ID_ELLIPSE, TOOL_ELLIPSE },
    { ID_RRECT, TOOL_RRECT },
};
static_assert(_countof(CommandToToolMapping) == TOOL_RRECT, "Logical error");

LRESULT CToolBox::OnCommand(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    UINT id = LOWORD(wParam);
    for (size_t i = 0; i < _countof(CommandToToolMapping); ++i)
    {
        if (CommandToToolMapping[i].id == id)
        {
            toolsModel.SetActiveTool(CommandToToolMapping[i].tool);
            break;
        }
    }
    return 0;
}

LRESULT CToolBox::OnToolsModelToolChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    // Tool changes are finalized by ToolsModel before notifying views. A view
    // refresh must not discard a compound selection or a newly pasted figure.

    // Check the toolbar button
    TOOLTYPE tool = toolsModel.GetActiveTool();
    UpdateFigureControls();
    for (size_t i = 0; i < _countof(CommandToToolMapping); ++i)
    {
        toolbar.SendMessage(TB_CHECKBUTTON, CommandToToolMapping[i].id, CommandToToolMapping[i].tool == tool);
    }

    return 0;
}

LRESULT CToolBox::OnLButtonDown(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    SetCapture();
    return 0;
}

LRESULT CToolBox::OnMouseMove(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (::GetCapture() != m_hWnd)
        return 0;
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    ClientToScreen(&pt);

    RECT rc;
    mainWindow.GetWindowRect(&rc);

    POINT ptCenter = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };

    DWORD dwExpectedBar2ID = ((pt.x < ptCenter.x) ? BAR2ID_LEFT : BAR2ID_RIGHT);

    if (registrySettings.Bar2ID != dwExpectedBar2ID)
    {
        registrySettings.Bar2ID = dwExpectedBar2ID;
        mainWindow.PostMessage(WM_SIZE, 0, 0);
    }

    return 0;
}

LRESULT CToolBox::OnLButtonUp(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (::GetCapture() != m_hWnd)
        return 0;

    ::ReleaseCapture();
    return 0;
}
