/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Keep track of tool parameters, notify listeners
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 */

#pragma once
#include "figureclipboard.h"
#include "figures.h"

enum TOOLTYPE
{
    TOOL_FREESEL  =  1,
    TOOL_RECTSEL  =  2,
    TOOL_RUBBER   =  3,
    TOOL_FILL     =  4,
    TOOL_COLOR    =  5,
    TOOL_ZOOM     =  6,
    TOOL_PEN      =  7,
    TOOL_BRUSH    =  8,
    TOOL_AIRBRUSH =  9,
    TOOL_TEXT     = 10,
    TOOL_LINE     = 11,
    TOOL_BEZIER   = 12,
    TOOL_RECT     = 13,
    TOOL_SHAPE    = 14,
    TOOL_ELLIPSE  = 15,
    TOOL_RRECT    = 16,
    TOOL_FIGURE   = 17,
    TOOL_MAX = TOOL_FIGURE,
};

enum BrushStyle : int
{
    BrushStyleRound,
    BrushStyleSquare,
    BrushStyleForeSlash,
    BrushStyleBackSlash,
};

/* CLASSES **********************************************************/

struct ToolBase
{
    HDC m_hdc;
    COLORREF m_fg, m_bg;

    ToolBase() : m_hdc(NULL) { }
    virtual ~ToolBase() { }

    virtual void OnButtonDown(BOOL bLeftButton, LONG x, LONG y, BOOL bDoubleClick) { }
    virtual BOOL OnMouseMove(BOOL bLeftButton, LONG& x, LONG& y) { return TRUE; }
    virtual BOOL OnButtonUp(BOOL bLeftButton, LONG& x, LONG& y) { return TRUE; }
    virtual BOOL OnRotate(BOOL begin, LONG x, LONG y) { return FALSE; }
    virtual BOOL GetRotationAngle(double& angle) const { return FALSE; }
    virtual bool SetRotationAngle(double degrees) { return false; }
    virtual bool SetDimensions(int width,int height) { return false; }
    virtual bool GetEditableBounds(RECT& bounds) const { return false; }
    virtual bool EditMouse(UINT message,WPARAM keys,POINT canvasPoint) { return false; }
    virtual bool SetFrameCursor(POINT canvasPoint) { return false; }
    virtual bool GetSkew(double& x,double& y) const { return false; }
    virtual bool SetSkew(double x,double y) { return false; }
    virtual bool ExportFigure(FigureClipboardData& data) const { return false; }
    virtual bool GetStarBend(int& value) const { return false; }
    virtual bool SetStarBend(int value) { return false; }
    virtual bool ImportFigure(const FigureClipboardData& data,POINT origin) { return false; }
    virtual void OnModifiersChanged() { }

    virtual void OnDrawOverlayOnImage(HDC hdc) { }
    virtual void OnDrawOverlayOnCanvas(HDC hdc) { }

    virtual void OnSpecialTweak(BOOL bMinus) { }

    virtual void OnEndDraw(BOOL bCancel);

    void beginEvent();
    void endEvent();
    void reset();

    static ToolBase* createToolObject(TOOLTYPE type);
};

class ToolsModel
{
private:
    class VirtualBrush
    {
    protected:
        COLORREF m_rgbColor = RGB(0, 0, 0);
        PAL_TYPE m_palette = PAL_MODERN;
        HBRUSH m_hBrush = NULL;

    public:
        ~VirtualBrush();

        HBRUSH GetBrush(PAL_TYPE palette, COLORREF rgbColor);
    };

    int m_lineWidth;
    INT m_penWidth;
    INT m_brushWidth;
    int m_shapeStyle;
    BrushStyle m_brushStyle;
    TOOLTYPE m_activeTool;
    TOOLTYPE m_oldActiveTool;
    TOOLTYPE m_zoomReturnTool=TOOL_RECTSEL;
    int m_zoomReturnKind=0;
    INT m_airBrushRadius;
    int m_rubberRadius;
    BOOL m_transpBg;
    int m_zoom;
    double m_figureAngle=0;
    double m_figureSkewX=0,m_figureSkewY=0;
    SIZE m_objectSizePresets[FIG_COUNT+1]={};
    bool m_objectSizeLoaded[FIG_COUNT+1]={};
    ToolBase *m_pToolObject;
    ToolBase *m_pZoomTool = NULL; // Temporary navigation tool; the editor object stays alive.
    VirtualBrush m_fgBrush;
    VirtualBrush m_bgBrush;

    ToolBase *GetOrCreateTool(TOOLTYPE nTool);
    void SendSetCursor();

public:
    ToolsModel();
    ~ToolsModel();

    BOOL IsSelection() const;

    int GetLineWidth() const;
    void SetLineWidth(int nLineWidth);
    void MakeLineThickerOrThinner(BOOL bThinner);

    INT GetPenWidth() const;
    void SetPenWidth(INT nPenWidth);
    void MakePenThickerOrThinner(BOOL bThinner);

    int GetShapeStyle() const;
    void SetShapeStyle(int nShapeStyle);

    INT GetBrushWidth() const;
    void SetBrushWidth(INT nBrushWidth);
    void MakeBrushThickerOrThinner(BOOL bThinner);

    BrushStyle GetBrushStyle() const;
    void SetBrushStyle(BrushStyle nBrushStyle);

    TOOLTYPE GetActiveTool() const;
    TOOLTYPE GetEditingTool() const { return m_activeTool==TOOL_ZOOM?m_zoomReturnTool:m_activeTool; }
    TOOLTYPE GetOldActiveTool() const;
    void ReturnFromZoom();
    void SetActiveTool(TOOLTYPE nActiveTool);

    INT GetAirBrushRadius() const;
    void SetAirBrushRadius(INT nAirBrushRadius);
    void MakeAirBrushThickerOrThinner(BOOL bThinner);

    int GetRubberRadius() const;
    void SetRubberRadius(int nRubberRadius);
    void MakeRubberThickerOrThinner(BOOL bThinner);

    int GetEraserSize() const;
    void SetEraserSize(int size);
    void SetEraserShape(int shape);
    SIZE GetToolSize() const;

    BOOL IsBackgroundTransparent() const;
    void SetBackgroundTransparent(BOOL bTransparent);

    int GetZoom() const;
    void SetZoom(int nZoom);
    void UpdateZoomStatus() const;

    void OnButtonDown(BOOL bLeftButton, LONG x, LONG y, BOOL bDoubleClick);
    void OnMouseMove(BOOL bLeftButton, LONG x, LONG y);
    void OnButtonUp(BOOL bLeftButton, LONG x, LONG y);
    void OnEndDraw(BOOL bCancel);
    BOOL OnRotateFigure(BOOL begin, LONG x, LONG y);
    void OnModifiersChanged();
    void ShowRotationAngle();
    double FigureAngle() const;
    bool SetFigureAngle(double degrees);
    void RememberFigureAngle(double degrees) { m_figureAngle=degrees; }
    int ObjectSizeKind(bool text) const;
    SIZE& ObjectSizePreset(bool text);
    void RestoreSavedObjectSize(bool text);
    bool SaveObjectSize(bool text,int width,int height);
    void ResetObjectSize(bool text);
    bool HasEditableFigure() const;
    void FigureSkew(double& x,double& y) const;
    bool SetFigureSkew(double x,double y);
    int StarBend() const;
    bool SetStarBend(int percent);
    void RememberFigureSkew(double x,double y) { m_figureSkewX=x;m_figureSkewY=y; }
    bool ExportFigure(FigureClipboardData& data) const;
    bool ImportFigure(const FigureClipboardData& data);
    bool SetFigureDimensions(int width,int height);
    bool EditableFigureBounds(RECT& bounds) const;
    bool EditFigureMouse(UINT message,WPARAM keys,POINT point);
    bool SetFigureCursor(POINT point);
    void OnDrawOverlayOnImage(HDC hdc);
    void OnDrawOverlayOnCanvas(HDC hdc);

    void resetTool();
    void selectAll();

    void NotifyToolChanged();
    void NotifyToolSettingsChanged();
    void NotifyZoomChanged();

    void SpecialTweak(BOOL bMinus);

    void DrawWithMouseTool(POINT pt, WPARAM wParam);

    HBRUSH GetFgBrush();
    HBRUSH GetBgBrush();
    static HBRUSH CreateBrush(PAL_TYPE palette, COLORREF color);
};

extern ToolsModel toolsModel;

static inline int Zoomed(int xy)
{
    return MulDiv(xy, toolsModel.GetZoom(), DEFAULT_ZOOM);
}

static inline int UnZoomed(int xy, BOOL bRound = FALSE)
{
    if (bRound)
        return MulDiv(xy, DEFAULT_ZOOM, toolsModel.GetZoom());
    return xy * DEFAULT_ZOOM / toolsModel.GetZoom();
}

static inline void Zoomed(POINT& pt)
{
    pt = { Zoomed(pt.x), Zoomed(pt.y) };
}

static inline void Zoomed(RECT& rc)
{
    rc = { Zoomed(rc.left), Zoomed(rc.top), Zoomed(rc.right), Zoomed(rc.bottom) };
}

static inline void UnZoomed(POINT& pt, BOOL bRound = FALSE)
{
    pt = { UnZoomed(pt.x, bRound), UnZoomed(pt.y, bRound) };
}

static inline void UnZoomed(RECT& rc, BOOL bRound = FALSE)
{
    rc = { UnZoomed(rc.left, bRound), UnZoomed(rc.top, bRound),
           UnZoomed(rc.right, bRound), UnZoomed(rc.bottom, bRound) };
}
