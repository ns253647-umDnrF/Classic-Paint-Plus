#pragma once
#define IDD_RPAINT_ABOUT 7200
#define IDC_ABOUT_NAME 7201
#define IDC_ABOUT_BUILD 7202
#define IDC_ABOUT_DESCRIPTION 7203
#define IDC_ABOUT_CREDITS 7204
#define IDC_ABOUT_LICENSE 7205
#define IDC_ABOUT_HELP 7206
#define IDC_ABOUT_BANNER 7207
#define IDR_ABOUT_BANNER 7208
#define IDS_ABOUT_DESCRIPTION 7220
#define IDS_ABOUT_CREDITS 7221
#define IDS_HELP_FILE_ERROR 7222
#define IDS_ABOUT_HELP 7223
#ifndef RC_INVOKED
void ShowRPaintAbout(HWND owner);
CStringW RPaintHelpPath();
bool OpenRPaintHelp(HWND owner);
#ifdef PAINT_TESTS
using HelpLauncher=HINSTANCE (WINAPI *)(HWND,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,INT);
void SetHelpLauncherForTests(HelpLauncher launcher);
#endif
#endif
