/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Portable application preferences and Windows wallpaper integration
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 */

#pragma once

#define MAX_RECENT_FILES 4
#include "figures.h"

class RegistrySettings
{
private:
    void LoadPresets(INT nCmdShow);
    void StoreIni();

public:
    DWORD BMPHeight;
    DWORD BMPWidth;
    DWORD GridExtent;
    DWORD NoStretching;
    DWORD ShowThumbnail;
    DWORD SnapToGrid;
    DWORD ThumbHeight;
    DWORD ThumbWidth;
    DWORD ThumbXPos;
    DWORD ThumbYPos;
    DWORD UnitSetting;
    WINDOWPLACEMENT WindowPlacement;

    CStringW strFiles[MAX_RECENT_FILES];

    CStringW strFontName;
    DWORD PointSize;
    DWORD Bold;
    DWORD Italic;
    DWORD Underline;
    DWORD CharSet;
    DWORD FontsPositionX;
    DWORD FontsPositionY;
    DWORD ShowTextTool;
    DWORD ShowStatusBar;
    DWORD ShowPalette;
    DWORD ShowToolBox;
    DWORD Bar1ID;
    DWORD Bar2ID;
    DWORD ReturnAfterColorPick;
    DWORD UILanguage;
    DWORD SelectionInterpolation;
    DWORD FigureKind;
    DWORD ShapeFromCenter;
    DWORD FigureRounding;
    DWORD PanelMagnification; // Percentage: 100, 150, 200, 250.
    DWORD ToolboxTopMargin;
    DWORD PaletteType;
    DWORD CanvasMarkerSize;
    DWORD CanvasMarkerColor;
    DWORD RecentFontsCount=5;
    DWORD TextAlignment=0;
    DWORD DarkTheme=0;
    DWORD EraserSize,EraserShape,EraserHardness,ReplaceDeletedWithAlpha;
    DWORD BrushSize=4,BrushShape=0,BrushHardness=100;
    SIZE SavedObjectSizes[FIG_COUNT+1]={}; // One snapshot per figure kind; final slot is text.
    DWORD SavedObjectFixed[FIG_COUNT+1]={}; // Fixation is independent of saved values.
    DWORD SavedTextAnglePresent=0;
    double SavedTextAngle=0;
    DWORD RememberObjectSizes=0;
    DWORD SprayStyle=0,SprayRadius=5,SpraySmoothing=0;
    void StorePreferences();
    static CStringW ConfigPath();
    static void SetConfigPathForTests(LPCWSTR path);

// Values for Bar1ID.
// I think these values are Win2k3 mspaint compatible but sometimes not working...
#define BAR1ID_TOP    0x0000e81b
#define BAR1ID_BOTTOM 0x0000e81e

// Values for Bar2ID.
// I think these values are Win2k3 mspaint compatible but sometimes not working...
#define BAR2ID_LEFT   0x0000e81c
#define BAR2ID_RIGHT  0x0000e81d

    enum WallpaperStyle {
        TILED,
        CENTERED,
        STRETCHED
    };

    static void SetWallpaper(LPCWSTR szFileName, WallpaperStyle style);

    void Load(INT nCmdShow);
    void Store();
    void SetMostRecentFile(LPCWSTR szPathName);
};
