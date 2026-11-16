// Integration tests for the real common dialog and guarded canvas message handlers.
#include <initializer_list>
#include <vector>
#include "precomp.h"
#include "colorpicker.h"
#include "settings.h"
#include "canvasdialogs.h"
#include "canvasresource.h"
#include <stdio.h>

static POINT samplePoint;
static bool cancelDialog;
static int ticks;
static bool exercised;
// Tests use a private temporary INI, never the user's registry or config.ini.
static WCHAR testConfig[MAX_PATH]={};
static void RestoreTestPreferences() { if(testConfig[0]) DeleteFileW(testConfig); }
static void SaveTestPreferences()
{
    WCHAR dir[MAX_PATH];GetTempPathW(_countof(dir),dir);GetTempFileNameW(dir,L"pnt",0,testConfig);
    HANDLE f=CreateFileW(testConfig,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
    WORD bom=0xfeff;DWORD bytes;WriteFile(f,&bom,2,&bytes,NULL);CloseHandle(f);
    RegistrySettings::SetConfigPathForTests(testConfig);atexit(RestoreTestPreferences);
}
struct TestIniKey
{
    LONG Create(HKEY,LPCWSTR) { return ERROR_SUCCESS; }
    LONG QueryDWORDValue(LPCWSTR name,DWORD& value)
    {
        WCHAR text[32]={};GetPrivateProfileStringW(L"Settings",name,L"",text,_countof(text),RegistrySettings::ConfigPath());
        if(!text[0]) return ERROR_FILE_NOT_FOUND;value=wcstoul(text,NULL,10);return ERROR_SUCCESS;
    }
    void SetDWORDValue(LPCWSTR name,DWORD value)
    {
        WCHAR text[32];StringCchPrintfW(text,_countof(text),L"%lu",value);
        WritePrivateProfileStringW(L"Settings",name,text,RegistrySettings::ConfigPath());
    }
    void DeleteValue(LPCWSTR name) { WritePrivateProfileStringW(L"Settings",name,NULL,RegistrySettings::ConfigPath()); }
};
static std::vector<WORD> testLanguages;
static BOOL CALLBACK CollectTestLanguage(HMODULE, LPCWSTR, LPCWSTR, WORD language, LONG_PTR)
{
    testLanguages.push_back(language);
    return TRUE;
}

static CStringW ExactString(WORD language, UINT id)
{
    HRSRC resource = FindResourceExW(g_hinstExe, RT_STRING, MAKEINTRESOURCEW(id / 16 + 1), language);
    if (!resource) return L"";
    const WCHAR* p = static_cast<const WCHAR*>(LockResource(LoadResource(g_hinstExe, resource)));
    for (UINT i = 0; i < id % 16; ++i) p += *p + 1;
    return CStringW(p + 1, *p);
}

static BOOL CALLBACK FindDialog(HWND window, LPARAM result)
{
    WCHAR name[64];
    GetClassNameW(window, name, _countof(name));
    if (GetWindow(window, GW_OWNER) == mainWindow.m_hWnd && !lstrcmpW(name, L"#32770"))
        *reinterpret_cast<HWND*>(result) = window;
    return TRUE;
}

static VOID CALLBACK DriveDialog(HWND, UINT, UINT_PTR timer, DWORD)
{
    HWND dialog = NULL;
    EnumThreadWindows(GetCurrentThreadId(), FindDialog, reinterpret_cast<LPARAM>(&dialog));
    if (!dialog)
    {
        if (++ticks > 100) ExitProcess(20);
        return;
    }
    ShowWindow(dialog, SW_HIDE);
    KillTimer(NULL, timer);
    // These would draw/resize/edit the document if the guard were missing.
    LPARAM pos = MAKELPARAM(samplePoint.x, samplePoint.y);
    canvasWindow.SendMessage(WM_LBUTTONDOWN, MK_LBUTTON, pos);
    canvasWindow.SendMessage(WM_MOUSEMOVE, MK_LBUTTON, pos);
    canvasWindow.SendMessage(WM_LBUTTONUP, 0, pos);
    canvasWindow.SendMessage(WM_RBUTTONDOWN, MK_RBUTTON, pos);
    canvasWindow.SendMessage(WM_RBUTTONUP, 0, pos);
    mainWindow.SendMessage(WM_CLOSE);
    exercised = IsWindowEnabled(mainWindow) && IsWindowEnabled(canvasWindow) &&
                !IsWindowEnabled(paletteWindow) && IsWindow(mainWindow);
    PostMessageW(dialog, WM_COMMAND, cancelDialog ? IDCANCEL : IDOK, 0);
}

#include "canvas_test.inc"
#include "selection_resize_test.inc"
#include "canvas_drag_test.inc"
#include "figures_test.inc"
#include "figure_interaction_test.inc"
#include "panel_settings_test.inc"
#include "portable_settings_test.inc"
#include "final_polish_test.inc"
#include "jpeg_test.inc"
#include "imageformats_test.inc"
#include "advanced_test.inc"
#include "brush_formats_test.inc"
#include "dds_theme_test.inc"
#include "selection_help_test.inc"
#include "live_ui_test.inc"
#include "editable_objects_test.inc"
#include "document_alpha_test.inc"
#include "alpha_followup_test.inc"
#include "object_actions_test.inc"
#include "size_preferences_test.inc"
#include "text_warp_test.inc"
#include "png_backdrop_test.inc"
#include "selection_background_test.inc"
#include "text_edit_regression_test.inc"
#include "navigation_brush_test.inc"
#include "classic_zoom_test.inc"
#include "live_repaint_test.inc"
#include "exact_icons_spray_test.inc"
#include "corner_crop_bmp_test.inc"
#include "general_audit_test.inc"
#include "vector_selection_test.inc"
#include "transform_controls_test.inc"
#include "shape_groups_test.inc"
#include "parameter_bar_test.inc"
#include "parameter_flags_test.inc"
#include "ui_polish_test.inc"
#include "pixel_rounding_test.inc"
#include "zoom_edit_test.inc"
#include "cursor_zoom_test.inc" // Cursor anchoring, status field and palette spacing.
#include "final_release_test.inc"

int main(int argc,char** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    // A private desktop keeps GUI automation independent of the user's typing
    // and focus changes in other applications. It is never switched on screen.
    if(argc>1 && !strcmp(argv[1],"--isolated"))
    {
        WCHAR name[64];swprintf_s(name,L"PaintTests-%lu",GetCurrentProcessId());
        typedef HDESK (WINAPI *CreateDesktopFn)(LPCWSTR,LPCWSTR,DEVMODEW*,DWORD,ACCESS_MASK,LPSECURITY_ATTRIBUTES);
        auto createDesktop=reinterpret_cast<CreateDesktopFn>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"CreateDesktopW"));
        HDESK desktop=createDesktop?createDesktop(name,NULL,NULL,0,GENERIC_ALL,NULL):NULL;
        if(!desktop || !SetThreadDesktop(desktop)) { printf("Cannot create test desktop: %lu\n",GetLastError());return 70; }
    }
    SaveTestPreferences();
    g_hinstExe = GetModuleHandleW(NULL);
    INITCOMMONCONTROLSEX controls = { sizeof(controls), ICC_STANDARD_CLASSES | ICC_USEREX_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&controls);
    registrySettings.Load(SW_HIDE);
    registrySettings.PanelMagnification=100;registrySettings.CanvasMarkerSize=3;registrySettings.CanvasMarkerColor=RGB(0,120,215);
    registrySettings.UILanguage = MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT);
    ApplyPreferredLanguage();
    registrySettings.ShowThumbnail = FALSE;
    if (!mainWindow.DoCreate()) return 1;
    mainWindow.SetWindowPos(NULL, 0, 0, 900, 700, SWP_NOACTIVATE | SWP_NOZORDER);
    InitializeImage(NULL, NULL, FALSE);
    if(argc>2 && !strcmp(argv[2],"--final-release-only")) { TestFinalRelease();return 0; }
    if(argc>2 && !strcmp(argv[2],"--filter-icons-capture"))
    {
        CreateDirectoryW(L"filter-outline-preview",NULL);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
        mainWindow.SetWindowPos(NULL,0,0,1300,1000,SWP_NOACTIVATE|SWP_NOZORDER);toolsModel.SetActiveTool(TOOL_RECTSEL);
        for(int dark:{0,1})for(int scale:{100,150,200,250})
        {
            registrySettings.DarkTheme=dark;registrySettings.PanelMagnification=scale;ApplyTheme();ApplyPanelScale();
            CStringW path;path.Format(L"filter-outline-preview/filters-%d-%d.bmp",scale,dark);
            CaptureCanvasDialog(GetDlgItem(toolSettingsWindow,IDC_RESAMPLE_PANEL),path);
        }
        puts("Captured filtering buttons at four scales in both themes");return 0;
    }
    if(argc>2 && !strcmp(argv[2],"--cursor-zoom-only")) { TestCursorZoom();return 0; }
    if(argc>2 && !strcmp(argv[2],"--zoom-edit-only")) { TestZoomEdit();return 0; }
    if(argc>2 && !strcmp(argv[2],"--pixel-rounding-only")) { TestPixelRounding();return 0; }
    if(argc>2 && !strcmp(argv[2],"--parameter-bar-only")) { TestParameterBar();return 0; }
    if(argc>2 && !strcmp(argv[2],"--parameter-flags-only")) { TestParameterFlags();return 0; }
    if(argc>2 && !strcmp(argv[2],"--ui-polish-only")) { TestUiPolish();return 0; }
    if(argc>2 && !strcmp(argv[2],"--ui-polish-no-registry-only")) { TestUiPolish(false);return 0; }
    if(argc>2 && !strcmp(argv[2],"--canvas-only")) { TestCanvas();return 0; }
    if(argc>2 && !strcmp(argv[2],"--shape-groups-only")) { TestShapeGroups();return 0; }
    if(argc>2 && !strcmp(argv[2],"--figure-basics-only")) { TestFigures();TestFigureInteraction();return 0; }
    if(argc>2 && !strcmp(argv[2],"--shape-rounding-only")) { TestFreePolygonRounding();return 0; }
    if(argc>2 && !strcmp(argv[2],"--transform-controls-only")) { TestTransformControls();return 0; }
    if(argc>2 && !strcmp(argv[2],"--vector-selection-only")) { TestVectorSelection();return 0; }
    if(argc>2 && !strcmp(argv[2],"--live-repaint-only")) { TestLiveRepaint();return 0; }
    if(argc>2 && !strcmp(argv[2],"--navigation-brush-only"))
    { TestVisiblePasteAndNavigation();TestBrushSessionDefaults();TestHardBrushEdges();CaptureTransparencyArtwork();return 0; }
    if(argc>2 && !strcmp(argv[2],"--classic-zoom-only"))
    { TestVisiblePasteAndNavigation();TestClassicZoomPanel();return 0; }
    if(argc>2 && !strcmp(argv[2],"--text-angle-only")) { TestTextAnglePresets();return 0; }
    if(argc>2 && !strcmp(argv[2],"--text-height-only")) { TestTextInitialHeight();return 0; }
    if(argc>2 && !strcmp(argv[2],"--text-caret-only")) { TestTextCaretRendering();return 0; }
    if(argc>2 && !strcmp(argv[2],"--text-edit-regressions-only"))
    { TestTextAnglePresets();TestTextInitialHeight();TestTextCaretRendering();return 0; }
    if(argc>2 && !strcmp(argv[2],"--selection-background-only"))
    { mainWindow.ShowWindow(SW_SHOWNOACTIVATE);TestLiveSelectionBackground();TestSelectionTransparency();TestParallelTransparency();return 0; }
    if(argc>2 && !strcmp(argv[2],"--png-backdrop-only"))
    {
        CreateDirectoryW(L"text-warp-tests",NULL);CreateDirectoryW(L"alpha-followup-tests",NULL);
        TestPngBackdrop();TestPngBackdropImport();TestPaintServiceData();return 0;
    }
    if(argc>2 && !strcmp(argv[2],"--general-audit-only")) return TestGeneralAudit();
    if(argc>2 && !strcmp(argv[2],"--help-capture")) { CaptureRPaintHelpAssets();return 0; }
    if(argc>2 && !strcmp(argv[2],"--ini-only")) { TestPortableSettings();return 0; }
    if(argc>2 && !strcmp(argv[2],"--objects-only")) { TestEditableObjects();return 0; }
    if(argc>2 && !strcmp(argv[2],"--document-alpha-only")) { TestExactDimensions();TestDocumentAlpha();return 0; }
    if(argc>2 && !strcmp(argv[2],"--alpha-followup-only")) { TestAlphaFollowup();return 0; }
    if(argc>2 && !strcmp(argv[2],"--object-actions-only")) { TestObjectActions();return 0; }
    if(argc>2 && !strcmp(argv[2],"--size-preferences-only")) { TestSizePreferences();return 0; }
    if(argc>2 && !strcmp(argv[2],"--icons-spray-only")) { TestExactIconsAndSpray();return 0; }
    if(argc>2 && !strcmp(argv[2],"--corner-bmp-only")) { TestCornerCropAndBmpOrder();return 0; }
    if(argc>2 && !strcmp(argv[2],"--text-warp-only")) { TestTextWarpFollowup();return 0; }
    if(argc>2 && !strcmp(argv[2],"--live-ui-only")) { TestLiveUi();return 0; }
    if(argc>2 && !strcmp(argv[2],"--selection-help-only")) { TestSelectionRotationAndHelp();return 0; }
    if(argc>2 && !strcmp(argv[2],"--dds-theme-only")) { TestDdsOptionsAndTheme();return 0; }
    if(argc>2 && !strcmp(argv[2],"--brush-formats-only")) { TestBrushAndFormats();return 0; }
    if(argc>2 && !strcmp(argv[2],"--advanced-only"))
    { EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);TestAdvanced();return 0; }
    if(argc>2 && !strcmp(argv[2],"--jpeg-only"))
    { mainWindow.ShowWindow(SW_SHOWNOACTIVATE);TestJpegAndSliders();return 0; }
    if(argc>2 && !strcmp(argv[2],"--formats-only"))
    { mainWindow.ShowWindow(SW_SHOWNOACTIVATE);TestSelectionTransparency();TestImageFormats();return 0; }
    if (toolsModel.GetActiveTool() != TOOL_RECTSEL) return 30;
    HMENU settings = GetSubMenu(mainWindow.GetMenu(), 4);
    HMENU palette = GetSubMenu(settings, 3);
    HMENU toolMenu = GetSubMenu(settings, 4);
    HMENU languageMenu = GetSubMenu(settings, 5);
    WCHAR label[128] = {};
    GetMenuStringW(mainWindow.GetMenu(), 4, label, _countof(label), MF_BYPOSITION);
    printf("Menu palette=%d tool=%u languages=%d UI=%04x label:", GetMenuItemCount(palette), GetMenuItemID(toolMenu, 0), GetMenuItemCount(languageMenu), GetThreadUILanguage());
    for (const WCHAR* p = label; *p; ++p) printf(" %04x", *p);
    puts("");
    if (lstrcmpW(label, L"&\u041d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0438") || GetMenuItemCount(palette) != 4 ||
        GetMenuItemID(toolMenu, 0) != IDM_RETURN_AFTER_PICK || GetMenuItemCount(languageMenu) < 3)
        return 31;
    GetMenuStringW(palette, IDM_COLORSGRAYSCALE, label, _countof(label), MF_BYCOMMAND);
    if (lstrcmpW(label, L"\u041e\u0442\u0442\u0435\u043d\u043a\u0438 \u0441\u0435\u0440\u043e\u0433\u043e")) return 32;
    puts("PASS selection startup and Russian settings menu");
    registrySettings.UILanguage = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
    ApplyPreferredLanguage();
    HMENU english = LoadMenuW(g_hinstExe, MAKEINTRESOURCEW(ID_MENU));
    BuildSettingsMenu(english);
    GetMenuStringW(english, 4, label, _countof(label), MF_BYPOSITION);
    if (lstrcmpW(label, L"&Settings")) return 33;
    DestroyMenu(english);
    puts("PASS explicit English resource selection");
    const WCHAR* firstNames[] = {L"English (USA)", L"English (UK)", L"Russian", L"German", L"Spanish",
        L"Portuguese (Portugal)", L"Portuguese (Brazil)", L"French", L"Italian",
        L"Chinese (Simplified)", L"Chinese (Traditional)", L"Chinese (Hong Kong)",
        L"Albanian", L"Basque", L"Bulgarian"};
    if (GetMenuItemCount(languageMenu) != 30) return 40;
    for (int i = 0; i < _countof(firstNames); ++i)
    {
        GetMenuStringW(languageMenu, i, label, _countof(label), MF_BYPOSITION);
        if (lstrcmpW(label, firstNames[i])) return 41;
    }
    CStringW previous;
    for (int i = _countof(firstNames); i < GetMenuItemCount(languageMenu); ++i)
    {
        GetMenuStringW(languageMenu, i, label, _countof(label), MF_BYPOSITION);
        if (label[0] < L'A' || label[0] > L'Z' || previous.Compare(label) > 0 || wcsstr(label, L"Ukrainian")) return 42;
        previous = label;
    }
    struct StartupCase { WORD os; WORD saved; WORD expected; };
    const StartupCase startupCases[] = {
        {0x0422, 0, 0x0419}, {0x0423, 0, 0x0419}, {0x043f, 0, 0x0419},
        {0x0440, 0, 0x0419}, {0x0443, 0, 0x0419}, {0x0843, 0, 0x0419},
        {0x0442, 0, 0x0419}, {0x0428, 0, 0x0419},
        {0x042c, 0, 0x041f}, {0x082c, 0, 0x041f},
        {0x0401, 0, 0x0409}, {0x040b, 0, 0x0409}, {0xffff, 0, 0x0409},
        {0x0409, 0, 0x0409}, {0x0809, 0, 0x0809},
        {0x0816, 0, 0x0816}, {0x0416, 0, 0x0416},
        {0x0804, 0, 0x0804}, {0x0404, 0, 0x0404}, {0x0c04, 0, 0x0c04},
        {0x0422, 0x0809, 0x0809}, {0x042c, 0x0416, 0x0416},
        {0x0419, 0xffff, 0x0419}
    };
    for (const auto& item : startupCases)
        if (ResolveStartupLanguage(item.os, item.saved) != item.expected) return 43;
    if (PRIMARYLANGID(ResolveStartupLanguage(0x0407, 0)) != LANG_GERMAN ||
        PRIMARYLANGID(ResolveStartupLanguage(0x0813, 0)) != LANG_DUTCH) return 48;
    puts("PASS OS language detection, fallback rules and saved preference precedence");
    EnumResourceLanguagesW(g_hinstExe, RT_MENU, MAKEINTRESOURCEW(ID_MENU), CollectTestLanguage, 0);
    if (testLanguages.size() != 30) return 44;
    for (WORD language : testLanguages)
    {
        if (PRIMARYLANGID(language) == LANG_UKRAINIAN) return 45;
        registrySettings.UILanguage = language;
        ApplyPreferredLanguage();
        HMENU translated = LoadMenuW(g_hinstExe, MAKEINTRESOURCEW(ID_MENU));
        BuildSettingsMenu(translated);
        BuildCanvasMenu(translated);
        for(UINT id=IDS_PANEL_SIZE;id<=IDS_SELECTION_TRANSPARENCY;++id)
        {
            CStringW value;value.LoadString(g_hinstExe,id);
            if(value.IsEmpty() || value!=ExactString(language,id)) return 54;
        }
        for (UINT id = IDS_CV_CANVAS_MENU; id <= IDS_CV_CENTER; ++id)
        {
            CStringW value;
            value.LoadString(g_hinstExe, id);
            if (value.IsEmpty() || value != ExactString(language, id)) return 50;
        }
        for (UINT id=IDS_RESAMPLE_TITLE; id<=IDS_RESAMPLE_CUBIC; ++id)
        {
            CStringW value; value.LoadString(g_hinstExe,id);
            if (value.IsEmpty() || value!=ExactString(language,id)) return 52;
        }
        for (UINT id=IDS_FIGURE_GALLERY; id<IDS_FIGURE_FIRST+FIG_STAR8; ++id)
        {
            if(id>IDS_FIGURE_ROUNDING && id<IDS_FIGURE_FIRST) continue;
            CStringW value;value.LoadString(g_hinstExe,id);
            if(value.IsEmpty() || value!=ExactString(language,id)) return 53;
        }
        HMENU imageMenu = GetSubMenu(translated, 3);
        HMENU canvasMenu = NULL;
        for (int i=0; i<GetMenuItemCount(imageMenu); ++i)
        {
            HMENU sub = GetSubMenu(imageMenu, i);
            if (sub && GetMenuItemID(sub,0)==IDM_IMAGECROP) canvasMenu=sub;
        }
        if (!canvasMenu || GetMenuItemCount(canvasMenu)!=7 ||
            GetMenuItemID(canvasMenu,5)!=IDM_CROP_EDGES || GetMenuItemID(canvasMenu,6)!=IDM_CANVAS_SIZE) return 51;
        for(int corner=0;corner<4;++corner) if(GetMenuItemID(canvasMenu,1+corner)!=IDM_CROP_CORNER_FIRST+corner) return 51;
        for (UINT id = IDS_SETTINGS_MENU; id <= IDS_LANGUAGE_RESTART; ++id)
        {
            CStringW loaded;
            loaded.LoadString(g_hinstExe, id);
            CStringW expected = ExactString(language, id);
            if (expected.IsEmpty() || loaded != expected)
            {
                printf("FAIL translation language=%04x string=%u\n", language, id);
                return 46;
            }
        }
        for(UINT id=IDS_PANEL_SIZE;id<=IDS_ZOOM_RANGE;++id)
        {
            CStringW value;value.LoadString(g_hinstExe,id);
            if(value.IsEmpty() || value!=ExactString(language,id)) return 54;
        }
        HMENU choices = GetSubMenu(GetSubMenu(translated, 4), 5);
        GetMenuStringW(choices, 0, label, _countof(label), MF_BYPOSITION);
        if (lstrcmpW(label, L"English (USA)")) return 47;
        UpdateSettingsChecks(translated);
        int checked = 0;
        for (int i = 0; i < GetMenuItemCount(choices); ++i)
            if (GetMenuState(choices, i, MF_BYPOSITION) & MF_CHECKED) ++checked;
        if (checked != 1) return 49;
        DestroyMenu(translated);
    }
    puts("PASS 30 locale translations, language order, Chinese variants and removal of Ukrainian");
    registrySettings.UILanguage = MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT);
    ApplyPreferredLanguage();
    TestCanvas();
    TestSelectionResize();
    TestCanvasDragFrame();
    TestFigures();
    TestFigureInteraction();
    TestPanelSettings();
    TestPortableSettings();
    TestNestedMenuEditing();
    TestSelectionTransparency();
    TestLiveSelectionBackground();
    TestPolishedVisuals();
    TestJpegAndSliders();
    TestImageFormats();
    TestAdvanced();
    TestBrushAndFormats();
    TestDdsOptionsAndTheme();
    TestSelectionRotationAndHelp();
    TestLiveUi();
    TestTextAnglePresets();TestTextInitialHeight();TestTextCaretRendering();
    TestEditableObjects();
    TestExactDimensions();TestDocumentAlpha();
    TestAlphaFollowup();
    TestObjectActions();
    TestRememberSizePreference();TestFigureOutsideClick();
    TestTextWarpFollowup();
    TestPngBackdropImport();
    TestExactIconsAndSpray();
    TestCornerCropAndBmpOrder();
    TestVisiblePasteAndNavigation();TestBrushSessionDefaults();TestHardBrushEdges();CaptureTransparencyArtwork();
    TestClassicZoomPanel();
    toolMenu=GetSubMenu(GetSubMenu(mainWindow.GetMenu(),4),4); // Live language switching replaces menus.
    imageModel.Crop(256, 256, 0, 0);
    const COLORREF sample = RGB(17, 93, 201), original = RGB(122, 35, 67);
    SetPixelV(imageModel.GetDC(), 20, 20, sample);
    for (bool returnAfterPick : {false, true})
    {
        registrySettings.ReturnAfterColorPick = returnAfterPick;
        UpdateSettingsChecks(toolMenu);
        if (!!(GetMenuState(toolMenu, IDM_RETURN_AFTER_PICK, MF_BYCOMMAND) & MF_CHECKED) != returnAfterPick)
            return 34;
        toolsModel.SetActiveTool(TOOL_RECTSEL);
        toolsModel.SetActiveTool(TOOL_COLOR);
        TOOLTYPE returnTool = toolsModel.GetOldActiveTool();
        toolsModel.OnButtonDown(TRUE, 20, 20, FALSE);
        toolsModel.OnButtonUp(TRUE, 20, 20);
        if (toolsModel.GetActiveTool() != (returnAfterPick ? returnTool : TOOL_COLOR) ||
            paletteModel.GetFgColor() != sample)
        {
            printf("FAIL picker return=%d tool=%d color=%06lx source=%06lx expected=%06lx\n", returnAfterPick,
                toolsModel.GetActiveTool(),paletteModel.GetFgColor(),GetPixel(imageModel.GetDC(),20,20),sample);
            return 35;
        }
    }
    puts("PASS both eyedropper return modes and checkmarks");
    toolsModel.SetActiveTool(TOOL_RECTSEL);
    for (int zoom : {125, 1000, 2000, 8000})
    {
        toolsModel.SetZoom(zoom);
        canvasWindow.updateScrollRange();
        canvasWindow.updateScrollPos(zoom == 8000 ? 80 : 0, zoom == 8000 ? 64 : 0);
        // At 12.5% choose the actual source pixel mapped by the displayed position.
        POINT pt = {20, 20};
        canvasWindow.ImageToCanvas(pt);
        POINT actual = pt;
        canvasWindow.CanvasToImage(actual);
        SetPixelV(imageModel.GetDC(), actual.x, actual.y, sample);
        for (bool cancel : {false, true})
        {
            samplePoint = pt;
            cancelDialog = cancel;
            ticks = 0;
            exercised = false;
            COLORREF result = original;
            UINT_PTR timer = SetTimer(NULL, 0, 30, DriveDialog);
            BOOL accepted = ChooseColorFromCanvas(mainWindow, &result);
            KillTimer(NULL, timer);
            if (!exercised || accepted != !cancel || result != (cancel ? original : sample) ||
                GetPixel(imageModel.GetDC(), actual.x, actual.y) != sample ||
                !IsWindowEnabled(paletteWindow) || !IsWindowEnabled(toolBoxContainer))
            {
                printf("FAIL zoom=%d cancel=%d accepted=%d color=%06lx exercised=%d\n",
                       zoom, cancel, accepted, result, exercised);
                return 2;
            }
            printf("PASS zoom=%d cancel=%d\n", zoom, cancel);
        }
    }
    if(TestGeneralAudit()) return 80;
    mainWindow.DestroyWindow();
    puts("All 8 picker integration cases passed.");
    return 0;
}
