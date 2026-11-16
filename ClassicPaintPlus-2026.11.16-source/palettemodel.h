/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Keep track of palette data, notify listeners
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 */

#pragma once

#define NUM_COLORS 28

enum PAL_TYPE
{
    PAL_MODERN = 1,
    PAL_OLDTYPE = 2,
    PAL_GRAYSCALE = 3,
    PAL_MONOCHROME = 4,
};

/* CLASSES **********************************************************/

class PaletteModel
{
private:
    COLORREF m_colors[NUM_COLORS];
    PAL_TYPE m_nSelectedPalette;
    COLORREF m_fgColor;
    COLORREF m_bgColor;
    BYTE m_fgAlpha=255,m_bgAlpha=255;

    void NotifyColorChanged();
    void NotifyPaletteChanged();

public:
    PaletteModel();
    PAL_TYPE SelectedPalette();
    void SelectPalette(PAL_TYPE nPalette);
    COLORREF GetColor(UINT nIndex) const;
    void SetColor(UINT nIndex, COLORREF newColor);
    COLORREF GetFgColor() const;
    void SetFgColor(COLORREF newColor,BYTE alpha=255);
    BYTE GetFgAlpha() const { return m_fgAlpha; }
    COLORREF GetBgColor() const;
    void SetBgColor(COLORREF newColor,BYTE alpha=255);
    BYTE GetBgAlpha() const { return m_bgAlpha; }
    void SwapColors();
};
