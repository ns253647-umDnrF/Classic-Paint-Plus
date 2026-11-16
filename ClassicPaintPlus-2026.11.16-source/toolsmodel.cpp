/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Keep track of tool parameters, notify listeners
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 */

#include "precomp.h"

ToolsModel toolsModel;

/* FUNCTIONS ********************************************************/

ToolsModel::VirtualBrush::~VirtualBrush()
{
    if (m_hBrush)
        ::DeleteObject(m_hBrush);
}

HBRUSH ToolsModel::VirtualBrush::GetBrush(PAL_TYPE palette, COLORREF rgbColor)
{
    if (m_hBrush &&
        m_palette == palette &&
        m_rgbColor == rgbColor)
    {
        return m_hBrush;
    }

    if (m_hBrush)
        ::DeleteObject(m_hBrush);

    m_hBrush = ToolsModel::CreateBrush(palette, rgbColor);
    m_palette = palette;
    m_rgbColor = rgbColor;
    return m_hBrush;
}

ToolsModel::ToolsModel()
{
    m_lineWidth = m_penWidth = 1;
    m_brushWidth = 4;
    m_shapeStyle = 0;
    m_brushStyle = BrushStyleRound;
    m_oldActiveTool = m_activeTool = TOOL_RECTSEL;
    m_airBrushRadius = 5;
    m_rubberRadius = 4;
    m_transpBg = FALSE;
    m_zoom = DEFAULT_ZOOM;
    m_pToolObject = GetOrCreateTool(m_activeTool);
}

ToolsModel::~ToolsModel()
{
    delete m_pZoomTool;
    delete m_pToolObject;
    m_pToolObject = NULL;
}

ToolBase *ToolsModel::GetOrCreateTool(TOOLTYPE nTool)
{
    delete m_pToolObject;
    m_pToolObject = ToolBase::createToolObject(nTool);
    return m_pToolObject;
}

BOOL ToolsModel::IsSelection() const
{
    return (GetActiveTool() == TOOL_RECTSEL || GetActiveTool() == TOOL_FREESEL);
}

int ToolsModel::GetLineWidth() const
{
    return m_lineWidth;
}

void ToolsModel::SetLineWidth(int nLineWidth)
{
    m_lineWidth = max(1,min(500,nLineWidth));
    NotifyToolSettingsChanged();
    imageModel.NotifyImageChanged();
}

INT ToolsModel::GetPenWidth() const
{
    return m_penWidth;
}

void ToolsModel::SetPenWidth(INT nPenWidth)
{
    m_penWidth = nPenWidth;
    NotifyToolSettingsChanged();
    imageModel.NotifyImageChanged();
}

int ToolsModel::ObjectSizeKind(bool text) const
{
    if(text) return FIG_COUNT;
    switch(GetEditingTool())
    {
        case TOOL_RECT:return FIG_RECT;
        case TOOL_ELLIPSE:return FIG_ELLIPSE;
        case TOOL_RRECT:return FIG_RRECT;
        default:return int(min(DWORD(FIG_COUNT-1),registrySettings.FigureKind));
    }
}
SIZE& ToolsModel::ObjectSizePreset(bool text)
{
    int kind=ObjectSizeKind(text);
    if(!m_objectSizeLoaded[kind])
    { m_objectSizePresets[kind]=registrySettings.SavedObjectSizes[kind];m_objectSizeLoaded[kind]=true; }
    return m_objectSizePresets[kind];
}
void ToolsModel::RestoreSavedObjectSize(bool text)
{ ObjectSizePreset(text)=registrySettings.SavedObjectSizes[ObjectSizeKind(text)];RestoreTransformPreset(text); }
bool ToolsModel::SaveObjectSize(bool text,int width,int height)
{
    if(width<0 || width>32767 || height<0 || height>32767) return false;
    SaveTransformPreset(text,width,height);
    registrySettings.SavedObjectSizes[ObjectSizeKind(text)]={width,height};
    if(text) { registrySettings.SavedTextAngle=PreparedFigureTransform(true).angle;textEditWindow.RestoreSavedRotation(); }
    RestoreSavedObjectSize(text);registrySettings.StorePreferences();return true;
}
void ToolsModel::ResetObjectSize(bool text)
{
    registrySettings.SavedObjectSizes[ObjectSizeKind(text)]={};
    ResetTransformPreset(text);
    if(text) { registrySettings.SavedTextAngle=0;textEditWindow.RestoreSavedRotation(); }
    RestoreSavedObjectSize(text);registrySettings.StorePreferences();
}

INT ToolsModel::GetBrushWidth() const
{
    return m_brushWidth;
}

void ToolsModel::SendSetCursor()
{
    canvasWindow.SendMessage(WM_SETCURSOR, (WPARAM)(HWND)canvasWindow,
                             MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
}

void ToolsModel::SetBrushWidth(INT nBrushWidth)
{
    m_brushWidth = max(1,min(500,nBrushWidth));registrySettings.BrushSize=m_brushWidth;
    NotifyToolSettingsChanged();
    SendSetCursor();
    imageModel.NotifyImageChanged();
}

void ToolsModel::MakeLineThickerOrThinner(BOOL bThinner)
{
    INT thickness = GetLineWidth();
    SetLineWidth(bThinner ? max(1, thickness - 1) : (thickness + 1));
}

void ToolsModel::MakePenThickerOrThinner(BOOL bThinner)
{
    INT thickness = GetPenWidth();
    SetPenWidth(bThinner ? max(1, thickness - 1) : (thickness + 1));
}

void ToolsModel::MakeBrushThickerOrThinner(BOOL bThinner)
{
    INT thickness = GetBrushWidth();
    SetBrushWidth(bThinner ? max(1, thickness - 1) : (thickness + 1));
}

void ToolsModel::MakeAirBrushThickerOrThinner(BOOL bThinner)
{
    INT thickness = GetAirBrushRadius();
    SetAirBrushRadius(bThinner ? max(1, thickness - 1) : (thickness + 1));
}

void ToolsModel::MakeRubberThickerOrThinner(BOOL bThinner)
{
    INT thickness = GetEraserSize();
    SetEraserSize(bThinner ? max(1, thickness - 1) : (thickness + 1));
}

int ToolsModel::GetShapeStyle() const
{
    return m_shapeStyle;
}

void ToolsModel::SetShapeStyle(int nShapeStyle)
{
    m_shapeStyle = nShapeStyle;
    NotifyToolSettingsChanged();
}

BrushStyle ToolsModel::GetBrushStyle() const
{
    return m_brushStyle;
}

void ToolsModel::SetBrushStyle(BrushStyle nBrushStyle)
{
    m_brushStyle = BrushStyle(max(0,min(3,int(nBrushStyle))));registrySettings.BrushShape=m_brushStyle;
    SendSetCursor();
    NotifyToolSettingsChanged();
}

TOOLTYPE ToolsModel::GetActiveTool() const
{
    return m_activeTool;
}

TOOLTYPE ToolsModel::GetOldActiveTool() const
{
    return m_oldActiveTool;
}

void ToolsModel::SetActiveTool(TOOLTYPE nActiveTool)
{
    if(nActiveTool==TOOL_ZOOM)
    {
        if(m_activeTool!=TOOL_ZOOM)
        {
            CancelDocumentAlphaPick(false);
            m_zoomReturnTool=m_activeTool;
            m_zoomReturnKind=ObjectSizeKind(false);
            if(!m_pZoomTool)m_pZoomTool=ToolBase::createToolObject(TOOL_ZOOM);
            m_activeTool=TOOL_ZOOM;NotifyToolChanged();
            imageModel.NotifyImageChanged();
        }
        return;
    }
    if(m_activeTool==TOOL_ZOOM)
    {
        m_activeTool=m_zoomReturnTool;
        if(nActiveTool==m_activeTool && (nActiveTool!=TOOL_FIGURE ||
            int(registrySettings.FigureKind)==m_zoomReturnKind))
        {
            NotifyToolChanged();imageModel.NotifyImageChanged();return;
        }
    }
    if(nActiveTool!=TOOL_COLOR) CancelDocumentAlphaPick(false);
    bool selectionSwitch=IsSelection() && (nActiveTool==TOOL_RECTSEL || nActiveTool==TOOL_FREESEL) && !canvasWindow.m_drawing;
    if(!selectionSwitch) { OnEndDraw(FALSE);selectionModel.Landing(); }
    m_activeTool = nActiveTool;

    switch (m_activeTool)
    {
        case TOOL_FREESEL:
        case TOOL_RECTSEL:
        case TOOL_COLOR:
        case TOOL_ZOOM:
        case TOOL_TEXT:
            // The active tool is not an actually drawing tool
            break;

        case TOOL_LINE:
        case TOOL_BEZIER:
        case TOOL_RECT:
        case TOOL_SHAPE:
        case TOOL_ELLIPSE:
        case TOOL_FILL:
        case TOOL_AIRBRUSH:
        case TOOL_RRECT:
        case TOOL_FIGURE:
        case TOOL_RUBBER:
        case TOOL_BRUSH:
        case TOOL_PEN:
            // The active tool is an actually drawing tool. Save it for TOOL_COLOR to restore
            m_oldActiveTool = nActiveTool;
            break;
    }

    m_pToolObject = GetOrCreateTool(m_activeTool);

    NotifyToolChanged();
}

void ToolsModel::ReturnFromZoom()
{
    if(m_activeTool!=TOOL_ZOOM)return;
    SetActiveTool(m_zoomReturnTool);
    if(m_activeTool==TOOL_TEXT && textEditWindow.IsWindowVisible())textEditWindow.SetFocus();
}

INT ToolsModel::GetAirBrushRadius() const
{
    return int(registrySettings.SprayRadius);
}

void ToolsModel::SetAirBrushRadius(INT nAirBrushRadius)
{
    registrySettings.SprayRadius=max(1,min(100,nAirBrushRadius));registrySettings.StorePreferences();
    NotifyToolSettingsChanged();
}

int ToolsModel::GetRubberRadius() const
{
    return (GetEraserSize()+1)/2;
}

void ToolsModel::SetRubberRadius(int nRubberRadius)
{
    SetEraserSize(nRubberRadius*2);
    SendSetCursor();
    NotifyToolSettingsChanged();
}

int ToolsModel::GetEraserSize() const { return int(registrySettings.EraserSize); }
void ToolsModel::SetEraserSize(int size)
{
    registrySettings.EraserSize=max(1,min(500,size));
    NotifyToolSettingsChanged();SendSetCursor();imageModel.NotifyImageChanged();
}
void ToolsModel::SetEraserShape(int shape)
{
    registrySettings.EraserShape=max(0,min(3,shape));
    NotifyToolSettingsChanged();SendSetCursor();imageModel.NotifyImageChanged();
}
SIZE ToolsModel::GetToolSize() const
{
    SIZE size={1,1};
    switch (m_activeTool)
    {
        case TOOL_FREESEL:
        case TOOL_RECTSEL:
            size.cx = selectionModel.m_rc.Width();
            size.cy = selectionModel.m_rc.Height();
            break;
        case TOOL_COLOR:
        case TOOL_ZOOM:
        case TOOL_TEXT:
        case TOOL_FILL:
            size.cx = size.cy = 1;
            break;
        case TOOL_LINE:
        case TOOL_BEZIER:
        case TOOL_RECT:
        case TOOL_RRECT:
        case TOOL_SHAPE:
        case TOOL_ELLIPSE:
        case TOOL_FIGURE:
            size.cx = size.cy = GetLineWidth();
            break;
        case TOOL_AIRBRUSH:
            size.cx = size.cy = GetAirBrushRadius() * 2+(registrySettings.SprayStyle==2?4:0)+2*SpraySmoothingPadding(GetAirBrushRadius(),registrySettings.SpraySmoothing);
            break;
        case TOOL_RUBBER:
            size.cx = size.cy = GetEraserSize();
            break;
        case TOOL_BRUSH:
            size.cx = size.cy = GetBrushWidth();
            break;
        case TOOL_PEN:
            size.cx = size.cy = GetPenWidth();
            break;
    }
    if (size.cx < 1)
        size.cx = 1;
    if (size.cy < 1)
        size.cy = 1;
    return size;
}

BOOL ToolsModel::IsBackgroundTransparent() const
{
    return m_transpBg;
}

void ToolsModel::SetBackgroundTransparent(BOOL bTransparent)
{
    m_transpBg = bTransparent;
    NotifyToolSettingsChanged();
    imageModel.NotifyImageChanged();
}

int ToolsModel::GetZoom() const
{
    return m_zoom;
}

void ToolsModel::SetZoom(int nZoom)
{
    m_zoom = nZoom;
    NotifyZoomChanged();
    SendSetCursor();
}

void ToolsModel::UpdateZoomStatus() const
{
    if(!::IsWindow(g_hStatusBar))return;
    CStringW value;
    if(m_zoom%10)value.Format(L"%.1f%%",m_zoom/10.0);else value.Format(L"%d%%",m_zoom/10);
    ::SendMessageW(g_hStatusBar,SB_SETTEXT,3,LPARAM(LPCWSTR(value)));
}

void ToolsModel::NotifyToolChanged()
{
    UpdateSelectionResizePanel();
    if (toolBoxContainer.IsWindow())
        toolBoxContainer.SendMessage(WM_TOOLSMODELTOOLCHANGED, m_activeTool);
    if (toolSettingsWindow.IsWindow())
        toolSettingsWindow.SendMessage(WM_TOOLSMODELTOOLCHANGED, m_activeTool);
    if (fontsDialog.IsWindow())
        fontsDialog.SendMessage(WM_TOOLSMODELTOOLCHANGED, m_activeTool);
    if (textEditWindow.IsWindow())
        textEditWindow.SendMessage(WM_TOOLSMODELTOOLCHANGED, m_activeTool);
}

void ToolsModel::NotifyToolSettingsChanged()
{
    if (toolSettingsWindow.IsWindow())
        toolSettingsWindow.SendMessage(WM_TOOLSMODELSETTINGSCHANGED);
    if (textEditWindow.IsWindow())
        textEditWindow.SendMessage(WM_TOOLSMODELSETTINGSCHANGED);
    // The canvas also owns live previews (editable figures, polygons, curves
    // and brush strokes). Repaint when their settings change, even if no text
    // editor has ever been created. This must not commit pixels or add undo.
    if (canvasWindow.IsWindow())
        canvasWindow.Invalidate(FALSE);
}

void ToolsModel::NotifyZoomChanged()
{
    UpdateZoomStatus();
    if (toolSettingsWindow.IsWindow())
        toolSettingsWindow.SendMessage(WM_TOOLSMODELZOOMCHANGED);
    if (textEditWindow.IsWindow())
        textEditWindow.SendMessage(WM_TOOLSMODELZOOMCHANGED);
    if (canvasWindow.IsWindow())
        canvasWindow.SendMessage(WM_TOOLSMODELZOOMCHANGED);
}

void ToolsModel::resetTool()
{
    if(m_activeTool==TOOL_ZOOM)return;
    m_pToolObject->reset();
}

void ToolsModel::selectAll()
{
    OnEndDraw(FALSE);
    SetActiveTool(TOOL_RECTSEL);
    selectionModel.setMask(CRect(0,0,imageModel.GetWidth(),imageModel.GetHeight()),NULL);
    selectionModel.m_bShow=TRUE;imageModel.NotifyImageChanged();
}

HBRUSH ToolsModel::CreateBrush(PAL_TYPE palette, COLORREF color)
{
    if (palette == PAL_MONOCHROME)
        return CreateDitherBrush(color, RGB(0, 0, 0), RGB(255, 255, 255));
    else
        return ::CreateSolidBrush(color);
}

HBRUSH ToolsModel::GetFgBrush()
{
    return m_fgBrush.GetBrush(paletteModel.SelectedPalette(), paletteModel.GetFgColor());
}

HBRUSH ToolsModel::GetBgBrush()
{
    return m_bgBrush.GetBrush(paletteModel.SelectedPalette(), paletteModel.GetBgColor());
}
