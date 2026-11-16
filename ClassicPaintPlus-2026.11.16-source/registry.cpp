/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Portable application preferences and Windows wallpaper integration
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 *             Copyright 2020 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
 */

#include "precomp.h"
#include <winreg.h>
#include <wincon.h>
#include <shlobj.h>
#include <cmath>

RegistrySettings registrySettings;

// Stable INI names: gallery order can change without reassigning saved sizes.
static const LPCWSTR objectSizeNames[]={L"Ellipse",L"Rectangle",L"RoundedRectangle",L"Triangle",L"RightTriangle",
    L"Diamond",L"Pentagon",L"Hexagon",L"RightArrow",L"LeftArrow",L"UpArrow",L"DownArrow",
    L"Star4",L"Star5",L"Star6",L"RectCallout",L"OvalCallout",L"CloudCallout",
    L"Star8",L"Star16",L"Star24",L"Star32",L"LeftRightArrow",L"UpDownArrow",L"QuadArrow",L"LeftRightUpArrow",
    L"BentArrow",L"UTurnArrow",L"LeftUpArrow",L"BentUpArrow",L"CurvedLeftArrow",L"CurvedRightArrow",L"CurvedUpArrow",L"CurvedDownArrow",
    L"StripedRightArrow",L"NotchedRightArrow",L"PentagonArrow",L"Chevron",L"RightArrowCallout",L"LeftArrowCallout",L"UpArrowCallout",L"DownArrowCallout",
    L"LeftRightArrowCallout",L"QuadArrowCallout",L"CircularArrow",L"LeftCircularArrow",L"Text"};
static_assert(_countof(objectSizeNames)==FIG_COUNT+1,"All object kinds need stable size keys");

/* FUNCTIONS ********************************************************/

void RegistrySettings::SetWallpaper(LPCWSTR szFileName, RegistrySettings::WallpaperStyle style)
{
    // Build the local path to the converted cached BMP wallpaper
    HRESULT hr;
    WCHAR szWallpaper[MAX_PATH];
    hr = SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, szWallpaper);
    if (FAILED(hr))
        return;
    hr = StringCchCatW(szWallpaper, _countof(szWallpaper), TEXT("\\Wallpaper1.bmp"));
    if (FAILED(hr))
        return;

    // Save the converted wallpaper BMP
    CImageDx img;
    HBITMAP hbmLocked = imageModel.LockBitmap();
    img.Attach(hbmLocked);
    hr = img.SaveDx(szWallpaper);
    img.Detach();
    imageModel.UnlockBitmap(hbmLocked);
    if (FAILED(hr))
        return;

    // Write the wallpaper settings to the registry
    CRegKey desktop;
    if (desktop.Open(HKEY_CURRENT_USER, L"Control Panel\\Desktop") == ERROR_SUCCESS)
    {
        desktop.SetStringValue(L"Wallpaper", szFileName);
        desktop.SetStringValue(L"WallpaperStyle", (style == RegistrySettings::STRETCHED) ? L"2" : L"0");
        desktop.SetStringValue(L"TileWallpaper", (style == RegistrySettings::TILED) ? L"1" : L"0");
        desktop.SetStringValue(L"ConvertedWallpaper", szWallpaper);
        desktop.SetStringValue(L"OriginalWallpaper", szFileName);
    }

    // Set the desktop wallpaper
    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, szWallpaper, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}

void RegistrySettings::LoadPresets(INT nCmdShow)
{
    for(auto& file:strFiles) file.Empty();
    BMPHeight = GetSystemMetrics(SM_CYSCREEN) / 2;
    BMPWidth = GetSystemMetrics(SM_CXSCREEN) / 2;
    GridExtent = 1;
    NoStretching = 0;
    ShowThumbnail = 0;
    SnapToGrid = 0;
    ThumbHeight = 100;
    ThumbWidth = 120;
    ThumbXPos = 180;
    ThumbYPos = 200;
    UnitSetting = 0;
    Bold = FALSE;
    Italic = FALSE;
    Underline = FALSE;
    CharSet = DEFAULT_CHARSET;
    PointSize = 14;
    FontsPositionX = 0;
    FontsPositionY = 0;
    ShowTextTool = TRUE;
    ShowStatusBar = TRUE;
    ShowPalette = TRUE;
    ShowToolBox = TRUE;
    Bar1ID = BAR1ID_BOTTOM;
    Bar2ID = BAR2ID_LEFT;
    ReturnAfterColorPick = FALSE;
    UILanguage = 0; // First run: detect, resolve and save a concrete UI language.
    SelectionInterpolation = 2; // High-quality bicubic.
    FigureKind = FIG_RECT;
    ZeroMemory(SavedObjectSizes,sizeof(SavedObjectSizes));
    ZeroMemory(SavedObjectFixed,sizeof(SavedObjectFixed));SavedTextAnglePresent=0;
    SavedTextAngle=0;
    RememberObjectSizes=0;
    SprayStyle=0;SprayRadius=5;SpraySmoothing=0;
    ShapeFromCenter = FALSE;
    FigureRounding = 0; // Legacy field; rounding now belongs to each figure for this session only.
    PanelMagnification = 100;
    ToolboxTopMargin = 40;
    PaletteType = PAL_OLDTYPE;
    RecentFontsCount=5;TextAlignment=0;DarkTheme=0;BrushSize=4;BrushShape=0;BrushHardness=100;EraserSize=8;EraserShape=1;EraserHardness=100;ReplaceDeletedWithAlpha=1;
    CanvasMarkerSize = 3;
    CanvasMarkerColor = RGB(0,120,215);

    LOGFONTW lf;
    ::GetObjectW(GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
    strFontName = lf.lfFaceName;

    ZeroMemory(&WindowPlacement, sizeof(WindowPlacement));
    RECT& rc = WindowPlacement.rcNormalPosition;
    rc.left = rc.top = CW_USEDEFAULT;
    rc.right = rc.left + 544;
    rc.bottom = rc.top + 375;
    WindowPlacement.showCmd = nCmdShow;
}


namespace
{
CStringW testConfigPath;
DWORD ReadNumber(LPCWSTR path,LPCWSTR section,LPCWSTR key,DWORD fallback)
{
    WCHAR value[64]={};
    GetPrivateProfileStringW(section,key,L"",value,_countof(value),path);
    if(!value[0]) return fallback;
    WCHAR* end=NULL;unsigned long long n=_wcstoui64(value,&end,10);
    return end && !*end && n<=MAXDWORD?static_cast<DWORD>(n):fallback;
}
void WriteNumber(LPCWSTR path,LPCWSTR section,LPCWSTR key,DWORD value)
{
    WCHAR text[32];StringCchPrintfW(text,_countof(text),L"%lu",value);
    WritePrivateProfileStringW(section,key,text,path);
}
}
CStringW RegistrySettings::ConfigPath()
{
    if(!testConfigPath.IsEmpty()) return testConfigPath;
    WCHAR path[MAX_LONG_PATH]={};GetModuleFileNameW(NULL,path,_countof(path));
    WCHAR* slash=wcsrchr(path,L'\\');if(slash) slash[1]=0;
    CStringW result(path);result+=L"config.ini";return result;
}
void RegistrySettings::SetConfigPathForTests(LPCWSTR path) { testConfigPath=path; }
void RegistrySettings::Load(INT nCmdShow)
{
    LoadPresets(nCmdShow);
    CStringW path=ConfigPath();
    if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)
    {
        StoreIni(); // First run starts from built-in defaults only.
        return;
    }
    BMPHeight=ReadNumber(path,L"Settings",L"BMPHeight",BMPHeight);
    BMPWidth=ReadNumber(path,L"Settings",L"BMPWidth",BMPWidth);
    GridExtent=ReadNumber(path,L"Settings",L"GridExtent",GridExtent);
    NoStretching=ReadNumber(path,L"Settings",L"NoStretching",NoStretching);
    ShowThumbnail=ReadNumber(path,L"Settings",L"ShowThumbnail",ShowThumbnail);
    SnapToGrid=ReadNumber(path,L"Settings",L"SnapToGrid",SnapToGrid);
    ThumbHeight=ReadNumber(path,L"Settings",L"ThumbHeight",ThumbHeight);
    ThumbWidth=ReadNumber(path,L"Settings",L"ThumbWidth",ThumbWidth);
    ThumbXPos=ReadNumber(path,L"Settings",L"ThumbXPos",ThumbXPos);
    ThumbYPos=ReadNumber(path,L"Settings",L"ThumbYPos",ThumbYPos);
    UnitSetting=ReadNumber(path,L"Settings",L"UnitSetting",UnitSetting);
    Bold=ReadNumber(path,L"Settings",L"Bold",Bold);
    Italic=ReadNumber(path,L"Settings",L"Italic",Italic);
    Underline=ReadNumber(path,L"Settings",L"Underline",Underline);
    CharSet=ReadNumber(path,L"Settings",L"CharSet",CharSet);
    PointSize=ReadNumber(path,L"Settings",L"PointSize",PointSize);
    FontsPositionX=ReadNumber(path,L"Settings",L"FontsPositionX",FontsPositionX);
    FontsPositionY=ReadNumber(path,L"Settings",L"FontsPositionY",FontsPositionY);
    ShowTextTool=ReadNumber(path,L"Settings",L"ShowTextTool",ShowTextTool);
    ShowStatusBar=ReadNumber(path,L"Settings",L"ShowStatusBar",ShowStatusBar);
    ShowPalette=ReadNumber(path,L"Settings",L"ShowPalette",ShowPalette);
    ShowToolBox=ReadNumber(path,L"Settings",L"ShowToolBox",ShowToolBox);
    Bar1ID=ReadNumber(path,L"Settings",L"Bar1ID",Bar1ID);
    Bar2ID=ReadNumber(path,L"Settings",L"Bar2ID",Bar2ID);
    ReturnAfterColorPick=ReadNumber(path,L"Settings",L"ReturnAfterColorPick",ReturnAfterColorPick);
    UILanguage=ReadNumber(path,L"Settings",L"UILanguage",UILanguage);
    SelectionInterpolation=ReadNumber(path,L"Settings",L"SelectionInterpolation",SelectionInterpolation);
    FigureKind=ReadNumber(path,L"Settings",L"FigureKind",FigureKind);
    ShapeFromCenter=ReadNumber(path,L"Settings",L"ShapeFromCenter",ShapeFromCenter);
    // Do not import the obsolete global rounding preference.
    PanelMagnification=ReadNumber(path,L"Settings",L"PanelMagnification",PanelMagnification);
    ToolboxTopMargin=ReadNumber(path,L"Settings",L"ToolboxTopMargin",ToolboxTopMargin);
    PaletteType=ReadNumber(path,L"Settings",L"PaletteType",PaletteType);
    CanvasMarkerSize=ReadNumber(path,L"Settings",L"CanvasMarkerSize",CanvasMarkerSize);
    CanvasMarkerColor=ReadNumber(path,L"Settings",L"CanvasMarkerColor",CanvasMarkerColor);
    WCHAR value[MAX_LONG_PATH]={};
    GetPrivateProfileStringW(L"Text",L"FontName",strFontName,value,_countof(value),path);strFontName=value;
    for(int i=0;i<MAX_RECENT_FILES;++i)
    {
        WCHAR key[16];StringCchPrintfW(key,_countof(key),L"File%d",i+1);
        GetPrivateProfileStringW(L"RecentFiles",key,L"",value,_countof(value),path);strFiles[i]=value;
    }
    WindowPlacement.flags=ReadNumber(path,L"Window",L"flags",WindowPlacement.flags);
    WindowPlacement.showCmd=ReadNumber(path,L"Window",L"showCmd",WindowPlacement.showCmd);
    WindowPlacement.ptMinPosition.x=ReadNumber(path,L"Window",L"ptMinPosition.x",WindowPlacement.ptMinPosition.x);
    WindowPlacement.ptMinPosition.y=ReadNumber(path,L"Window",L"ptMinPosition.y",WindowPlacement.ptMinPosition.y);
    WindowPlacement.ptMaxPosition.x=ReadNumber(path,L"Window",L"ptMaxPosition.x",WindowPlacement.ptMaxPosition.x);
    WindowPlacement.ptMaxPosition.y=ReadNumber(path,L"Window",L"ptMaxPosition.y",WindowPlacement.ptMaxPosition.y);
    WindowPlacement.rcNormalPosition.left=ReadNumber(path,L"Window",L"rcNormalPosition.left",WindowPlacement.rcNormalPosition.left);
    WindowPlacement.rcNormalPosition.top=ReadNumber(path,L"Window",L"rcNormalPosition.top",WindowPlacement.rcNormalPosition.top);
    WindowPlacement.rcNormalPosition.right=ReadNumber(path,L"Window",L"rcNormalPosition.right",WindowPlacement.rcNormalPosition.right);
    WindowPlacement.rcNormalPosition.bottom=ReadNumber(path,L"Window",L"rcNormalPosition.bottom",WindowPlacement.rcNormalPosition.bottom);
    if(PanelMagnification!=100 && PanelMagnification!=150 && PanelMagnification!=200 && PanelMagnification!=250) PanelMagnification=100;
    ToolboxTopMargin=min(ToolboxTopMargin,500);
    RecentFontsCount=ReadNumber(path,L"Settings",L"RecentFontsCount",RecentFontsCount);
    TextAlignment=ReadNumber(path,L"Settings",L"TextAlignment",TextAlignment);
    DarkTheme=ReadNumber(path,L"Settings",L"DarkTheme",DarkTheme);
    ReplaceDeletedWithAlpha=ReadNumber(path,L"Settings",L"ReplaceDeletedWithAlpha",ReplaceDeletedWithAlpha);
    RecentFontsCount=min(30u,RecentFontsCount);TextAlignment=min(3u,TextAlignment);DarkTheme=!!DarkTheme;
    BrushSize=max(1u,min(500u,BrushSize));BrushShape=min(3u,BrushShape);BrushHardness=min(100u,BrushHardness);
    EraserSize=max(1u,min(500u,EraserSize));EraserShape=min(3u,EraserShape);EraserHardness=min(100u,EraserHardness);ReplaceDeletedWithAlpha=!!ReplaceDeletedWithAlpha;
    if(PaletteType<PAL_MODERN || PaletteType>PAL_MONOCHROME) PaletteType=PAL_OLDTYPE;
    if(SelectionInterpolation>2) SelectionInterpolation=2;
    if(FigureKind>=FIG_COUNT) FigureKind=FIG_TRIANGLE;
    RememberObjectSizes=!!ReadNumber(path,L"Settings",L"RememberObjectSizes",0);
    if(RememberObjectSizes)
    {
        WCHAR value[64]={};GetPrivateProfileStringW(L"ObjectSizes",L"TextAngle",L"0",value,_countof(value),path);
        WCHAR* end=NULL;double angle=wcstod(value,&end);
        if(end!=value && !*end && std::isfinite(angle) && angle>=0 && angle<360) SavedTextAngle=angle;
        SavedTextAnglePresent=!!ReadNumber(path,L"ObjectSizes",L"TextAngleSet",SavedTextAngle!=0);
    }
    SprayStyle=min(4u,ReadNumber(path,L"Settings",L"SprayStyle",0));SprayRadius=max(1u,min(100u,ReadNumber(path,L"Settings",L"SprayRadius",5)));
    SpraySmoothing=min(100u,ReadNumber(path,L"Settings",L"SpraySmoothing",0));
    for(int i=0;i<=FIG_COUNT;++i)
    {
        CStringW w=objectSizeNames[i];w+=L"Width";CStringW h=objectSizeNames[i];h+=L"Height";
        DWORD width=ReadNumber(path,L"ObjectSizes",w,0),height=ReadNumber(path,L"ObjectSizes",h,0);
        SavedObjectSizes[i]=RememberObjectSizes?SIZE{width<=32767?LONG(width):0,height<=32767?LONG(height):0}:SIZE{};
        DWORD available=(SavedObjectSizes[i].cx?1u:0u)|(SavedObjectSizes[i].cy?2u:0u)|((i==FIG_COUNT && SavedTextAnglePresent)?4u:0u);
        CStringW fixed=objectSizeNames[i];fixed+=L"Fixed";
        SavedObjectFixed[i]=RememberObjectSizes?(ReadNumber(path,L"ObjectSizes",fixed,available)&available):0;
    }
    ShapeFromCenter=!!ShapeFromCenter;FigureRounding=min(FigureRounding,100);
    if(CanvasMarkerSize<3 || CanvasMarkerSize>15) CanvasMarkerSize=3;
    CanvasMarkerColor&=0xffffff;
    if(!BMPWidth || BMPWidth>5000) BMPWidth=GetSystemMetrics(SM_CXSCREEN)/2;
    if(!BMPHeight || BMPHeight>5000) BMPHeight=GetSystemMetrics(SM_CYSCREEN)/2;
}
void RegistrySettings::StoreIni()
{
    CStringW path=ConfigPath();
    // A UTF-16 BOM lets the Windows INI API preserve all localized names and paths.
    HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file!=INVALID_HANDLE_VALUE) { WORD bom=0xfeff;DWORD written;WriteFile(file,&bom,2,&written,NULL);CloseHandle(file); }
    WriteNumber(path,L"Settings",L"BMPHeight",BMPHeight);
    WriteNumber(path,L"Settings",L"BMPWidth",BMPWidth);
    WriteNumber(path,L"Settings",L"GridExtent",GridExtent);
    WriteNumber(path,L"Settings",L"NoStretching",NoStretching);
    WriteNumber(path,L"Settings",L"ShowThumbnail",ShowThumbnail);
    WriteNumber(path,L"Settings",L"SnapToGrid",SnapToGrid);
    WriteNumber(path,L"Settings",L"ThumbHeight",ThumbHeight);
    WriteNumber(path,L"Settings",L"ThumbWidth",ThumbWidth);
    WriteNumber(path,L"Settings",L"ThumbXPos",ThumbXPos);
    WriteNumber(path,L"Settings",L"ThumbYPos",ThumbYPos);
    WriteNumber(path,L"Settings",L"UnitSetting",UnitSetting);
    WriteNumber(path,L"Settings",L"Bold",Bold);
    WriteNumber(path,L"Settings",L"Italic",Italic);
    WriteNumber(path,L"Settings",L"Underline",Underline);
    WriteNumber(path,L"Settings",L"CharSet",CharSet);
    WriteNumber(path,L"Settings",L"PointSize",PointSize);
    WriteNumber(path,L"Settings",L"FontsPositionX",FontsPositionX);
    WriteNumber(path,L"Settings",L"FontsPositionY",FontsPositionY);
    WriteNumber(path,L"Settings",L"ShowTextTool",ShowTextTool);
    WriteNumber(path,L"Settings",L"ShowStatusBar",ShowStatusBar);
    WriteNumber(path,L"Settings",L"ShowPalette",ShowPalette);
    WriteNumber(path,L"Settings",L"ShowToolBox",ShowToolBox);
    WriteNumber(path,L"Settings",L"Bar1ID",Bar1ID);
    WriteNumber(path,L"Settings",L"Bar2ID",Bar2ID);
    WriteNumber(path,L"Settings",L"ReturnAfterColorPick",ReturnAfterColorPick);
    WriteNumber(path,L"Settings",L"UILanguage",UILanguage);
    WriteNumber(path,L"Settings",L"SelectionInterpolation",SelectionInterpolation);
    WriteNumber(path,L"Settings",L"FigureKind",FigureKind);
    WriteNumber(path,L"Settings",L"ShapeFromCenter",ShapeFromCenter);
    WritePrivateProfileStringW(L"Settings",L"FigureRounding",NULL,path);
    WriteNumber(path,L"Settings",L"PanelMagnification",PanelMagnification);
    WriteNumber(path,L"Settings",L"ToolboxTopMargin",ToolboxTopMargin);
    WriteNumber(path,L"Settings",L"RecentFontsCount",RecentFontsCount);
    WriteNumber(path,L"Settings",L"TextAlignment",TextAlignment);
    WriteNumber(path,L"Settings",L"DarkTheme",DarkTheme);
    WritePrivateProfileStringW(L"Settings",L"BrushSize",NULL,path); // Session-only brush setting.
    WritePrivateProfileStringW(L"Settings",L"BrushShape",NULL,path); // Session-only brush setting.
    WritePrivateProfileStringW(L"Settings",L"EraserSize",NULL,path); // Session-only brush setting.
    WritePrivateProfileStringW(L"Settings",L"EraserShape",NULL,path); // Session-only brush setting.
    WritePrivateProfileStringW(L"Settings",L"EraserHardness",NULL,path); // Session-only brush setting.
    WriteNumber(path,L"Settings",L"ReplaceDeletedWithAlpha",ReplaceDeletedWithAlpha);
    WritePrivateProfileStringW(L"Settings",L"BrushHardness",NULL,path); // Session-only brush setting.
    WriteNumber(path,L"Settings",L"RememberObjectSizes",RememberObjectSizes);
    CStringW textAngle;textAngle.Format(L"%.17g",RememberObjectSizes?SavedTextAngle:0.0);
    WritePrivateProfileStringW(L"ObjectSizes",L"TextAngle",textAngle,path);
    WriteNumber(path,L"ObjectSizes",L"TextAngleSet",RememberObjectSizes?SavedTextAnglePresent:0);
    WriteNumber(path,L"Settings",L"SprayStyle",SprayStyle);WriteNumber(path,L"Settings",L"SprayRadius",SprayRadius);
    WriteNumber(path,L"Settings",L"SpraySmoothing",SpraySmoothing);
    for(int i=0;i<=FIG_COUNT;++i)
    {
        CStringW w=objectSizeNames[i];w+=L"Width";CStringW h=objectSizeNames[i];h+=L"Height";
        WriteNumber(path,L"ObjectSizes",w,RememberObjectSizes?SavedObjectSizes[i].cx:0);
        WriteNumber(path,L"ObjectSizes",h,RememberObjectSizes?SavedObjectSizes[i].cy:0);
        CStringW fixed=objectSizeNames[i];fixed+=L"Fixed";
        DWORD available=(SavedObjectSizes[i].cx?1u:0u)|(SavedObjectSizes[i].cy?2u:0u)|((i==FIG_COUNT && SavedTextAnglePresent)?4u:0u);
        WriteNumber(path,L"ObjectSizes",fixed,RememberObjectSizes?(SavedObjectFixed[i]&available):0);
    }

    WriteNumber(path,L"Settings",L"PaletteType",PaletteType);
    WriteNumber(path,L"Settings",L"CanvasMarkerSize",CanvasMarkerSize);
    WriteNumber(path,L"Settings",L"CanvasMarkerColor",CanvasMarkerColor);
    WritePrivateProfileStringW(L"Text",L"FontName",strFontName,path);
    for(int i=0;i<MAX_RECENT_FILES;++i)
    {
        WCHAR key[16];StringCchPrintfW(key,_countof(key),L"File%d",i+1);
        WritePrivateProfileStringW(L"RecentFiles",key,strFiles[i],path);
    }
    WriteNumber(path,L"Window",L"flags",WindowPlacement.flags);
    WriteNumber(path,L"Window",L"showCmd",WindowPlacement.showCmd);
    WriteNumber(path,L"Window",L"ptMinPosition.x",WindowPlacement.ptMinPosition.x);
    WriteNumber(path,L"Window",L"ptMinPosition.y",WindowPlacement.ptMinPosition.y);
    WriteNumber(path,L"Window",L"ptMaxPosition.x",WindowPlacement.ptMaxPosition.x);
    WriteNumber(path,L"Window",L"ptMaxPosition.y",WindowPlacement.ptMaxPosition.y);
    WriteNumber(path,L"Window",L"rcNormalPosition.left",WindowPlacement.rcNormalPosition.left);
    WriteNumber(path,L"Window",L"rcNormalPosition.top",WindowPlacement.rcNormalPosition.top);
    WriteNumber(path,L"Window",L"rcNormalPosition.right",WindowPlacement.rcNormalPosition.right);
    WriteNumber(path,L"Window",L"rcNormalPosition.bottom",WindowPlacement.rcNormalPosition.bottom);
}
void RegistrySettings::Store()
{
    BMPWidth=imageModel.GetWidth();BMPHeight=imageModel.GetHeight();StoreIni();
}
void RegistrySettings::StorePreferences() { StoreIni(); }

void RegistrySettings::SetMostRecentFile(LPCWSTR szPathName)
{
    if(!szPathName || !szPathName[0]) return;
    // Register the file to the user's 'Recent' folder
    if (szPathName && szPathName[0])
        SHAddToRecentDocs(SHARD_PATHW, szPathName);

    // If szPathName is present in strFiles, move it to the top of the list
    for (INT i = MAX_RECENT_FILES - 1, iFound = -1; i > 0; --i)
    {
        if (iFound < 0 && strFiles[i].CompareNoCase(szPathName) == 0)
            iFound = i;

        if (iFound >= 0)
            Swap(strFiles[i], strFiles[i - 1]);
    }

    // If szPathName is not the first item in strFiles, insert it at the top of the list
    if (strFiles[0].CompareNoCase(szPathName) != 0)
    {
        for (INT i = MAX_RECENT_FILES - 1; i > 0; --i)
            strFiles[i] = strFiles[i - 1];

        strFiles[0] = szPathName;
    }
}
