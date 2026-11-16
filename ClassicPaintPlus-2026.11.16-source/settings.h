#pragma once
void ApplyPreferredLanguage();
WORD ResolveStartupLanguage(WORD osLanguage, WORD savedLanguage);
void BuildSettingsMenu(HMENU menu);
void UpdateSettingsChecks(HMENU menu);
BOOL HandleSettingsCommand(HWND owner, UINT command);
