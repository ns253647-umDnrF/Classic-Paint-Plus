// Local settings extension, LGPL-2.0-or-later.
#include <vector>
#include <algorithm>
#include "precomp.h"
#include "settings.h"

static std::vector<WORD> languages;

struct LanguageName { WORD id; LPCWSTR name; int priority; };
static const LanguageName languageNames[] = {
    {0x0409, L"English (USA)", 0}, {0x0809, L"English (UK)", 1},
    {0x0419, L"Russian", 2}, {0x0407, L"German", 3},
    {0x0c0a, L"Spanish", 4}, {0x040a, L"Spanish", 4},
    {0x0816, L"Portuguese (Portugal)", 5}, {0x0416, L"Portuguese (Brazil)", 6},
    {0x040c, L"French", 7}, {0x0410, L"Italian", 8},
    {0x0804, L"Chinese (Simplified)", 9}, {0x0404, L"Chinese (Traditional)", 10},
    {0x0c04, L"Chinese (Hong Kong)", 11},
    {0x041c, L"Albanian", 12}, {0x042d, L"Basque", 13}, {0x0402, L"Bulgarian", 14},
    {0x041a, L"Croatian", 15}, {0x0405, L"Czech", 15},
    {0x0413, L"Dutch", 15}, {0x0425, L"Estonian", 15},
    {0x040d, L"Hebrew", 15}, {0x040e, L"Hungarian", 15},
    {0x0421, L"Indonesian", 15}, {0x0411, L"Japanese", 15},
    {0x0414, L"Norwegian", 15}, {0x0415, L"Polish", 15},
    {0x0418, L"Romanian", 15}, {0x041b, L"Slovak", 15},
    {0x041d, L"Swedish", 15}, {0x041f, L"Turkish", 15},
    {0x042a, L"Vietnamese", 15}
};

static const LanguageName* LanguageInfo(WORD id)
{
    for (const auto& info : languageNames)
        if (info.id == id) return &info;
    // Some upstream translations use language-neutral IDs (e.g. 0x0007 German).
    if (SUBLANGID(id) == SUBLANG_NEUTRAL)
        for (const auto& info : languageNames)
            if (PRIMARYLANGID(info.id) == PRIMARYLANGID(id)) return &info;
    return NULL;
}

static BOOL CALLBACK CollectLanguage(HMODULE, LPCWSTR, LPCWSTR, WORD language, LONG_PTR context)
{
    auto& target = context ? *reinterpret_cast<std::vector<WORD>*>(context) : languages;
    if (LanguageInfo(language) && std::find(target.begin(), target.end(), language) == target.end())
        target.push_back(language);
    return TRUE;
}

static CStringW Label(UINT id)
{
    CStringW text;
    text.LoadString(g_hinstExe, id);
    return text;
}

static WORD AvailableLanguage(const std::vector<WORD>& availableLanguages, WORD requested)
{
    if (!requested) return 0;
    for (WORD available : availableLanguages)
        if (available == requested) return available;
    // Accept regional variants of a supported language, including neutral resources.
    WORD result = 0;
    int priority = 100;
    for (WORD available : availableLanguages)
    {
        if (PRIMARYLANGID(available) == PRIMARYLANGID(requested) &&
            LanguageInfo(available)->priority < priority)
        {
            result = available;
            priority = LanguageInfo(available)->priority;
        }
    }
    return result;
}

WORD ResolveStartupLanguage(WORD osLanguage, WORD savedLanguage)
{
    std::vector<WORD> available;
    EnumResourceLanguagesW(g_hinstExe, RT_MENU, MAKEINTRESOURCEW(ID_MENU), CollectLanguage,
                           reinterpret_cast<LONG_PTR>(&available));
    WORD result = AvailableLanguage(available, savedLanguage);
    if (result) return result; // A manual choice always wins over OS detection.
    switch (PRIMARYLANGID(osLanguage))
    {
        case 0x22: // Ukrainian
        case 0x23: // Belarusian
        case 0x3f: // Kazakh
        case 0x40: // Kyrgyz
        case 0x43: // Uzbek (Latin or Cyrillic)
        case 0x42: // Turkmen
        case 0x28: // Tajik
            return AvailableLanguage(available, 0x0419);
        case 0x2c: // Azerbaijani (Latin or Cyrillic)
            return AvailableLanguage(available, 0x041f);
    }
    result = AvailableLanguage(available, osLanguage);
    return result ? result : AvailableLanguage(available, 0x0409);
}

void ApplyPreferredLanguage()
{
    WORD language = ResolveStartupLanguage(GetUserDefaultUILanguage(), static_cast<WORD>(registrySettings.UILanguage));
    if (registrySettings.UILanguage != language)
    {
        registrySettings.UILanguage = language;
        registrySettings.StorePreferences(); // Persist the first-run decision immediately.
    }
    if (SUBLANGID(language) == SUBLANG_NEUTRAL)
        language = MAKELANGID(PRIMARYLANGID(language), SUBLANG_DEFAULT);
    WCHAR languageList[8] = {};
    StringCchPrintfW(languageList, _countof(languageList), L"%04X", language);
    ULONG count = 0;
    SetThreadPreferredUILanguages(MUI_LANGUAGE_ID, languageList, &count);
    SetThreadUILanguage(language);
}

void BuildSettingsMenu(HMENU menu)
{
    DeleteMenu(menu,IDM_IMAGEDRAWOPAQUE,MF_BYCOMMAND);
    // Locate the existing palette menu by command, preserving all its translations.
    for (int index = 0; index < GetMenuItemCount(menu); ++index)
    {
        HMENU palette = GetSubMenu(menu, index);
        if (!palette || GetMenuState(palette, IDM_COLORSEDITPALETTE, MF_BYCOMMAND) == UINT(-1))
            continue;
        DeleteMenu(palette, IDM_COLORSEDITPALETTE, MF_BYCOMMAND);
        if (GetMenuItemID(palette, 0) == 0)
            DeleteMenu(palette, 0, MF_BYPOSITION); // separator
        RemoveMenu(menu, index, MF_BYPOSITION); // detach, retain palette handle
        HMENU settings = CreatePopupMenu();
        BuildPanelSettingsMenu(settings);
        AppendMenuW(settings, MF_POPUP, reinterpret_cast<UINT_PTR>(palette), Label(IDS_PALETTE_MENU));
        HMENU tools = CreatePopupMenu();
        AppendMenuW(tools, MF_STRING, IDM_RETURN_AFTER_PICK, Label(IDS_RETURN_AFTER_PICK));
        AppendMenuW(settings, MF_POPUP, reinterpret_cast<UINT_PTR>(tools), Label(IDS_TOOLS_MENU));
        languages.clear();
        EnumResourceLanguagesW(g_hinstExe, RT_MENU, MAKEINTRESOURCEW(ID_MENU), CollectLanguage, 0);
        std::sort(languages.begin(), languages.end(), [](WORD a, WORD b) {
            const auto* first = LanguageInfo(a);
            const auto* second = LanguageInfo(b);
            if (first->priority != second->priority) return first->priority < second->priority;
            return lstrcmpW(first->name, second->name) < 0;
        });
        if (languages.size() > 1)
        {
            HMENU choices = CreatePopupMenu();
            for (size_t i = 0; i < languages.size() && IDM_LANGUAGE_FIRST + i <= IDM_LANGUAGE_LAST; ++i)
            {
                AppendMenuW(choices, MF_STRING, IDM_LANGUAGE_FIRST + i, LanguageInfo(languages[i])->name);
            }
            AppendMenuW(settings, MF_POPUP, reinterpret_cast<UINT_PTR>(choices), Label(IDS_LANGUAGE_MENU));
        }
        HMENU fonts=CreatePopupMenu();AppendMenuW(fonts,MF_OWNERDRAW,IDM_RECENT_FONTS,NULL);
        AppendMenuW(settings,MF_POPUP,reinterpret_cast<UINT_PTR>(fonts),Label(IDS_RECENT_FONTS));
        HMENU theme=CreatePopupMenu();AppendMenuW(theme,MF_STRING,IDM_THEME_LIGHT,Label(IDS_THEME_LIGHT));
        AppendMenuW(theme,MF_STRING,IDM_THEME_DARK,Label(IDS_THEME_DARK));
        AppendMenuW(settings,MF_POPUP,reinterpret_cast<UINT_PTR>(theme),Label(IDS_THEME));
        AppendMenuW(settings,MF_STRING,IDM_REMEMBER_OBJECT_SIZES,Label(IDS_REMEMBER_OBJECT_SIZES));
        InsertMenuW(menu, index, MF_BYPOSITION | MF_POPUP, reinterpret_cast<UINT_PTR>(settings), Label(IDS_SETTINGS_MENU));
        break;
    }
}

void UpdateSettingsChecks(HMENU menu)
{
    UpdatePanelSettingsChecks(menu);
    CheckMenuItem(menu,IDM_THEME_LIGHT,CHECKED_IF(!registrySettings.DarkTheme));
    CheckMenuItem(menu,IDM_THEME_DARK,CHECKED_IF(registrySettings.DarkTheme));
    CheckMenuItem(menu, IDM_RETURN_AFTER_PICK, CHECKED_IF(registrySettings.ReturnAfterColorPick));
    CheckMenuItem(menu,IDM_REMEMBER_OBJECT_SIZES,CHECKED_IF(registrySettings.RememberObjectSizes));
    for (size_t i = 0; i < languages.size(); ++i)
        CheckMenuItem(menu, IDM_LANGUAGE_FIRST + i, CHECKED_IF(registrySettings.UILanguage == languages[i]));
}

BOOL HandleSettingsCommand(HWND owner, UINT command)
{
    if(command==IDM_REMEMBER_OBJECT_SIZES)
    { registrySettings.RememberObjectSizes=!registrySettings.RememberObjectSizes;registrySettings.StorePreferences();return TRUE; }
    if(command==IDM_THEME_LIGHT || command==IDM_THEME_DARK)
    { registrySettings.DarkTheme=command==IDM_THEME_DARK;registrySettings.StorePreferences();ApplyTheme();return TRUE; }
    if(HandlePanelSettingsCommand(owner,command)) return TRUE;
    if (command == IDM_RETURN_AFTER_PICK)
    {
        registrySettings.ReturnAfterColorPick = !registrySettings.ReturnAfterColorPick;
        registrySettings.StorePreferences();
        return TRUE;
    }
    if (command >= IDM_LANGUAGE_FIRST && command <= IDM_LANGUAGE_LAST)
    {
        UINT index = command - IDM_LANGUAGE_FIRST;
        if (index >= languages.size()) return TRUE;
        DWORD language = languages[index];
        if (registrySettings.UILanguage != language)
        {
            bool untitled=!g_isAFile && Label(IDS_DEFAULTFILENAME)==g_szFileName;
            registrySettings.UILanguage = language;
            registrySettings.StorePreferences();
            ApplyPreferredLanguage();
            if(untitled) ::LoadStringW(g_hinstExe,IDS_DEFAULTFILENAME,g_szFileName,_countof(g_szFileName));
            mainWindow.ReloadLanguage();
        }
        return TRUE;
    }
    return FALSE;
}
