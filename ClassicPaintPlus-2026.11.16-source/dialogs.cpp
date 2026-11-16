#include <algorithm>
/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Window procedures of the dialog windows plus launching functions
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 */

#include "precomp.h"
#include "dialogs.h"

#include <winnls.h>

CMirrorRotateDialog mirrorRotateDialog;
CAttributesDialog attributesDialog;
CStretchSkewDialog stretchSkewDialog;
CFontsDialog fontsDialog;

/* FUNCTIONS ********************************************************/

void ShowError(INT stringID, ...)
{
    va_list va;
    va_start(va, stringID);

    CStringW strFormat, strText;
    strFormat.LoadString(stringID);
    strText.FormatV(strFormat, va);

    CStringW strProgramName;
    strProgramName.LoadString(IDS_PROGRAMNAME);

    mainWindow.MessageBox(strText, strProgramName, MB_ICONERROR);
    va_end(va);
}

LRESULT CMirrorRotateDialog::OnInitDialog(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    CheckDlgButton(IDD_MIRRORROTATERB1, BST_CHECKED);
    CheckDlgButton(IDD_MIRRORROTATERB4, BST_CHECKED);
    return TRUE;
}

LRESULT CMirrorRotateDialog::OnClose(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    EndDialog(0);
    return 0;
}

LRESULT CMirrorRotateDialog::OnOk(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    if (IsDlgButtonChecked(IDD_MIRRORROTATERB1))
        EndDialog(1);
    else if (IsDlgButtonChecked(IDD_MIRRORROTATERB2))
        EndDialog(2);
    else if (IsDlgButtonChecked(IDD_MIRRORROTATERB4))
        EndDialog(3);
    else if (IsDlgButtonChecked(IDD_MIRRORROTATERB5))
        EndDialog(4);
    else if (IsDlgButtonChecked(IDD_MIRRORROTATERB6))
        EndDialog(5);
    return 0;
}

LRESULT CMirrorRotateDialog::OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    EndDialog(0);
    return 0;
}

LRESULT CMirrorRotateDialog::OnRadioButton3(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    if (IsDlgButtonChecked(IDD_MIRRORROTATERB3) != BST_CHECKED)
        return 0;

    ::EnableWindow(GetDlgItem(IDD_MIRRORROTATERB4), TRUE);
    ::EnableWindow(GetDlgItem(IDD_MIRRORROTATERB5), TRUE);
    ::EnableWindow(GetDlgItem(IDD_MIRRORROTATERB6), TRUE);
    return 0;
}

LRESULT CMirrorRotateDialog::OnRadioButton12(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    if (IsDlgButtonChecked(IDD_MIRRORROTATERB1) != BST_CHECKED &&
        IsDlgButtonChecked(IDD_MIRRORROTATERB2) != BST_CHECKED)
    {
        return 0;
    }

    ::EnableWindow(GetDlgItem(IDD_MIRRORROTATERB4), FALSE);
    ::EnableWindow(GetDlgItem(IDD_MIRRORROTATERB5), FALSE);
    ::EnableWindow(GetDlgItem(IDD_MIRRORROTATERB6), FALSE);
    return 0;
}

LRESULT CAttributesDialog::OnInitDialog(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    newWidth = imageModel.GetWidth();
    newHeight = imageModel.GetHeight();

    CheckDlgButton(IDD_ATTRIBUTESRB3, BST_CHECKED);
    SetDlgItemInt(IDD_ATTRIBUTESEDIT1, newWidth, FALSE);
    SetDlgItemInt(IDD_ATTRIBUTESEDIT2, newHeight, FALSE);

    if (imageModel.IsBlackAndWhite())
        CheckRadioButton(IDD_ATTRIBUTESRB4, IDD_ATTRIBUTESRB5, IDD_ATTRIBUTESRB4);
    else
        CheckRadioButton(IDD_ATTRIBUTESRB4, IDD_ATTRIBUTESRB5, IDD_ATTRIBUTESRB5);

    if (g_isAFile)
    {
        WCHAR date[100], temp[100];
        GetDateFormatW(LOCALE_USER_DEFAULT, 0, &g_fileTime, NULL, date, _countof(date));
        GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &g_fileTime, NULL, temp, _countof(temp));
        StringCchCatW(date, _countof(date), L" ");
        StringCchCatW(date, _countof(date), temp);
        CStringW strSize;
        strSize.Format(IDS_FILESIZE, g_fileSize);
        SetDlgItemText(IDD_ATTRIBUTESTEXT6, date);
        SetDlgItemText(IDD_ATTRIBUTESTEXT7, strSize);
    }

    CStringW strUnit;
    GetDlgItemText(IDD_ATTRIBUTESTEXT8, strUnit);

    CStringW strRes;
    if (strUnit == L"dpi")
        strRes.Format(IDS_PRINTRES, ROUND(g_xDpi), ROUND(g_yDpi));
    else
        strRes.Format(IDS_PRINTRES, ROUND(PpcmFromDpi(g_xDpi)), ROUND(PpcmFromDpi(g_yDpi)));

    SetDlgItemText(IDD_ATTRIBUTESTEXT8, strRes);
    return TRUE;
}

LRESULT CAttributesDialog::OnClose(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    EndDialog(0);
    return 0;
}

LRESULT CAttributesDialog::OnOk(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    m_bBlackAndWhite = (IsDlgButtonChecked(IDD_ATTRIBUTESRB4) == BST_CHECKED);
    EndDialog(1);
    return 0;
}

LRESULT CAttributesDialog::OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    EndDialog(0);
    return 0;
}

LRESULT CAttributesDialog::OnDefault(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    newWidth = imageModel.GetWidth();
    newHeight = imageModel.GetHeight();
    CheckDlgButton(IDD_ATTRIBUTESRB3, BST_CHECKED);
    CheckDlgButton(IDD_ATTRIBUTESRB5, BST_CHECKED);
    SetDlgItemInt(IDD_ATTRIBUTESEDIT1, newWidth, FALSE);
    SetDlgItemInt(IDD_ATTRIBUTESEDIT2, newHeight, FALSE);
    return 0;
}

LRESULT CAttributesDialog::OnRadioButton1(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    if (IsDlgButtonChecked(IDD_ATTRIBUTESRB1) != BST_CHECKED)
        return 0;

    CStringW strNum;
    strNum.Format(L"%.3lf", newWidth / g_xDpi);
    SetDlgItemText(IDD_ATTRIBUTESEDIT1, strNum);
    strNum.Format(L"%.3lf", newHeight / g_yDpi);
    SetDlgItemText(IDD_ATTRIBUTESEDIT2, strNum);
    return 0;
}

LRESULT CAttributesDialog::OnRadioButton2(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    if (IsDlgButtonChecked(IDD_ATTRIBUTESRB2) != BST_CHECKED)
        return 0;

    CStringW strNum;
    strNum.Format(L"%.3lf", newWidth / PpcmFromDpi(g_xDpi));
    SetDlgItemText(IDD_ATTRIBUTESEDIT1, strNum);
    strNum.Format(L"%.3lf", newHeight / PpcmFromDpi(g_yDpi));
    SetDlgItemText(IDD_ATTRIBUTESEDIT2, strNum);
    return 0;
}

LRESULT CAttributesDialog::OnRadioButton3(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    if (IsDlgButtonChecked(IDD_ATTRIBUTESRB3) != BST_CHECKED)
        return 0;

    SetDlgItemInt(IDD_ATTRIBUTESEDIT1, newWidth, FALSE);
    SetDlgItemInt(IDD_ATTRIBUTESEDIT2, newHeight, FALSE);
    return 0;
}

LRESULT CAttributesDialog::OnEdit1(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    if (Edit_GetModify(hWndCtl))
    {
        WCHAR tempS[100];
        if (IsDlgButtonChecked(IDD_ATTRIBUTESRB1))
        {
            GetDlgItemText(IDD_ATTRIBUTESEDIT1, tempS, _countof(tempS));
            newWidth = max(1, (int)(wcstod(tempS, NULL) * g_xDpi));
        }
        else if (IsDlgButtonChecked(IDD_ATTRIBUTESRB2))
        {
            GetDlgItemText(IDD_ATTRIBUTESEDIT1, tempS, _countof(tempS));
            newWidth = max(1, (int)(wcstod(tempS, NULL) * PpcmFromDpi(g_xDpi)));
        }
        else if (IsDlgButtonChecked(IDD_ATTRIBUTESRB3))
        {
            GetDlgItemText(IDD_ATTRIBUTESEDIT1, tempS, _countof(tempS));
            newWidth = max(1, _wtoi(tempS));
        }
        Edit_SetModify(hWndCtl, FALSE);
    }
    return 0;
}

LRESULT CAttributesDialog::OnEdit2(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    if (Edit_GetModify(hWndCtl))
    {
        WCHAR tempS[100];
        if (IsDlgButtonChecked(IDD_ATTRIBUTESRB1))
        {
            GetDlgItemText(IDD_ATTRIBUTESEDIT2, tempS, _countof(tempS));
            newHeight = max(1, (int)(wcstod(tempS, NULL) * g_yDpi));
        }
        else if (IsDlgButtonChecked(IDD_ATTRIBUTESRB2))
        {
            GetDlgItemText(IDD_ATTRIBUTESEDIT2, tempS, _countof(tempS));
            newHeight = max(1, (int)(wcstod(tempS, NULL) * PpcmFromDpi(g_yDpi)));
        }
        else if (IsDlgButtonChecked(IDD_ATTRIBUTESRB3))
        {
            GetDlgItemText(IDD_ATTRIBUTESEDIT2, tempS, _countof(tempS));
            newHeight = max(1, _wtoi(tempS));
        }
        Edit_SetModify(hWndCtl, FALSE);
    }
    return 0;
}



LRESULT CStretchSkewDialog::OnInitDialog(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    SetDlgItemInt(IDD_STRETCHSKEWEDITHSTRETCH, 100, FALSE);
    SetDlgItemInt(IDD_STRETCHSKEWEDITVSTRETCH, 100, FALSE);
    SetDlgItemInt(IDD_STRETCHSKEWEDITHSKEW, 0, FALSE);
    SetDlgItemInt(IDD_STRETCHSKEWEDITVSKEW, 0, FALSE);
    return TRUE;
}

LRESULT CStretchSkewDialog::OnClose(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    EndDialog(0);
    return 0;
}

LRESULT CStretchSkewDialog::OnOk(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    CStringW strrcIntNumbers, strrcPercentage, strrcAngle;
    BOOL tr1, tr2, tr3, tr4;

    strrcIntNumbers.LoadString(g_hinstExe, IDS_INTNUMBERS);
    strrcPercentage.LoadString(g_hinstExe, IDS_PERCENTAGE);
    strrcAngle.LoadString(g_hinstExe, IDS_ANGLE);

    percentage.x = GetDlgItemInt(IDD_STRETCHSKEWEDITHSTRETCH, &tr1, FALSE);
    percentage.y = GetDlgItemInt(IDD_STRETCHSKEWEDITVSTRETCH, &tr2, FALSE);
    angle.x = GetDlgItemInt(IDD_STRETCHSKEWEDITHSKEW, &tr3, TRUE);
    angle.y = GetDlgItemInt(IDD_STRETCHSKEWEDITVSKEW, &tr4, TRUE);

    if (!(tr1 && tr2 && tr3 && tr4))
        MessageBox(strrcIntNumbers, NULL, MB_ICONEXCLAMATION);
    else if (percentage.x < 1 || percentage.x > 500 || percentage.y < 1 || percentage.y > 500)
        MessageBox(strrcPercentage, NULL, MB_ICONEXCLAMATION);
    else if (angle.x < -89 || angle.x > 89 || angle.y < -89 || angle.y > 89)
        MessageBox(strrcAngle, NULL, MB_ICONEXCLAMATION);
    else
        EndDialog(1);
    return 0;
}

LRESULT CStretchSkewDialog::OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    EndDialog(0);
    return 0;
}

static INT CALLBACK
EnumFontFamProc(ENUMLOGFONTW *lpelf, NEWTEXTMETRICW *lpntm, INT FontType, LPARAM lParam)
{
    CSimpleArray<CStringW>& arrFontNames = *reinterpret_cast<CSimpleArray<CStringW>*>(lParam);
    LPWSTR name = lpelf->elfLogFont.lfFaceName;
    if (name[0] == L'@') // Vertical fonts
        return TRUE;

    for (INT i = 0; i < arrFontNames.GetSize(); ++i)
    {
        if (arrFontNames[i] == name) // Already exists
            return TRUE;
    }

    arrFontNames.Add(name);
    return TRUE;
}

// TODO: AutoComplete font names
// TODO: Vertical text
CFontsDialog::CFontsDialog()
{
}

void CFontsDialog::InitFontNames()
{
    // List the fonts
    CSimpleArray<CStringW> arrFontNames;
    HDC hDC = CreateCompatibleDC(NULL);
    if (hDC)
    {
        EnumFontFamiliesW(hDC, NULL, (FONTENUMPROCW)EnumFontFamProc,
                          reinterpret_cast<LPARAM>(&arrFontNames));
        ::DeleteDC(hDC);
    }

    std::vector<CStringW> all;
    for(INT i=0;i<arrFontNames.GetSize();++i) all.push_back(arrFontNames[i]);
    std::sort(all.begin(),all.end(),[](const CStringW& a,const CStringW& b){return a.CompareNoCase(b)<0;});
    // Recent fonts precede the complete alphabetic list (as in Word).
    HWND hwndNames = GetDlgItem(IDD_FONTSNAMES);
    ::SendMessageW(hwndNames, CB_RESETCONTENT, 0, 0);
    ::SetWindowLongPtrW(hwndNames,GWL_STYLE,::GetWindowLongPtrW(hwndNames,GWL_STYLE)&~CBS_SORT);
    m_recentCount=0;
    for(const auto& recent:RecentFonts())
        if(std::any_of(all.begin(),all.end(),[&](const CStringW& name){return !name.CompareNoCase(recent);}))
        { ComboBox_AddString(hwndNames,recent);++m_recentCount; }
    for(const auto& name:all) ComboBox_AddString(hwndNames,name);
    ::SendMessageW(hwndNames,CB_SETDROPPEDWIDTH,280,0);

    ::SetWindowTextW(hwndNames, registrySettings.strFontName);
}

void CFontsDialog::InitFontSizes()
{
    static const INT s_sizes[] =
    {
        8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72
    };

    HWND hwndSizes = GetDlgItem(IDD_FONTSSIZES);
    ComboBox_ResetContent(hwndSizes);

    WCHAR szText[16];
    for (UINT i = 0; i < _countof(s_sizes); ++i)
    {
        StringCchPrintfW(szText, _countof(szText), L"%d", s_sizes[i]);
        INT iItem = ComboBox_AddString(hwndSizes, szText);
        if (s_sizes[i] == (INT)registrySettings.PointSize)
            ComboBox_SetCurSel(hwndSizes, iItem);
    }

    if (ComboBox_GetCurSel(hwndSizes) == CB_ERR)
    {
        StringCchPrintfW(szText, _countof(szText), L"%d", (INT)registrySettings.PointSize);
        ::SetWindowTextW(hwndSizes, szText);
    }
}

void CFontsDialog::InitToolbar()
{
    HWND hwndToolbar = GetDlgItem(IDD_FONTSTOOLBAR);
    while(::SendMessageW(hwndToolbar,TB_BUTTONCOUNT,0,0)>0) ::SendMessageW(hwndToolbar,TB_DELETEBUTTON,0,0);
    HIMAGELIST previous=reinterpret_cast<HIMAGELIST>(::SendMessageW(hwndToolbar,TB_SETIMAGELIST,0,0));
    if(previous) ImageList_Destroy(previous);
    ::SendMessageW(hwndToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    ::SendMessageW(hwndToolbar, TB_SETBITMAPSIZE, 0, MAKELPARAM(16, 16));
    ::SendMessageW(hwndToolbar, TB_SETBUTTONWIDTH, 0, MAKELPARAM(20, 20));
    
    HIMAGELIST himl = ImageList_LoadImage(g_hinstExe, MAKEINTRESOURCEW(IDB_FONTSTOOLBAR),
                                          16, 8, RGB(255, 0, 255), IMAGE_BITMAP,
                                          LR_CREATEDIBSECTION);
    ::SendMessageW(hwndToolbar, TB_SETIMAGELIST, 0, (LPARAM)himl);

    ImageList_Remove(himl,3);
    for(int alignment=0;alignment<4;++alignment)
    {
        HBITMAP icon=CreateColorDIB(16,16,RGB(255,0,255));HDC dc=CreateCompatibleDC(NULL);
        HGDIOBJ old=SelectObject(dc,icon);HBRUSH ink=CreateSolidBrush(GetSysColor(COLOR_BTNTEXT));
        for(int row=0;row<5;++row)
        { int width=(alignment==3 || row%2==0)?12:8,left=alignment==1?(16-width)/2:alignment==2?14-width:2;
          RECT r={left,2+row*3,left+width,3+row*3};FillRect(dc,&r,ink); }
        SelectObject(dc,old);DeleteDC(dc);DeleteObject(ink);ImageList_AddMasked(himl,icon,RGB(255,0,255));DeleteObject(icon);
    }
    HBITMAP warpIcon=CreateColorDIB(16,16,RGB(255,0,255));HDC warpDC=CreateCompatibleDC(NULL);HGDIOBJ oldWarp=SelectObject(warpDC,warpIcon);
    DrawTextWarpIcon(warpDC,CRect(0,0,16,16),GetSysColor(COLOR_BTNTEXT));SelectObject(warpDC,oldWarp);DeleteDC(warpDC);
    ImageList_AddMasked(himl,warpIcon,RGB(255,0,255));DeleteObject(warpIcon);
    TBBUTTON buttons[] =
    {
        { 0, IDM_BOLD, TBSTATE_ENABLED, TBSTYLE_CHECK },
        { 1, IDM_ITALIC, TBSTATE_ENABLED, TBSTYLE_CHECK },
        { 2, IDM_UNDERLINE, TBSTATE_ENABLED, TBSTYLE_CHECK },
        { 6, 0, TBSTATE_ENABLED, TBSTYLE_SEP },
        { 3, IDM_ALIGN_LEFT, TBSTATE_ENABLED, TBSTYLE_CHECKGROUP },
        { 4, IDM_ALIGN_CENTER, TBSTATE_ENABLED, TBSTYLE_CHECKGROUP },
        { 5, IDM_ALIGN_RIGHT, TBSTATE_ENABLED, TBSTYLE_CHECKGROUP },
        { 6, IDM_ALIGN_JUSTIFY, TBSTATE_ENABLED, TBSTYLE_CHECKGROUP },
        { 6, 0, TBSTATE_ENABLED, TBSTYLE_SEP },
        { 7, IDM_TEXT_WARP, TBSTATE_ENABLED, TBSTYLE_BUTTON },
    };
    ::SendMessageW(hwndToolbar, TB_ADDBUTTONS, _countof(buttons), (LPARAM)&buttons);

    ::SendMessageW(hwndToolbar,TB_CHECKBUTTON,IDM_ALIGN_LEFT+registrySettings.TextAlignment,TRUE);
    RECT toolbar,window,client;::GetWindowRect(hwndToolbar,&toolbar);ScreenToClient(&toolbar);GetWindowRect(&window);GetClientRect(&client);
    RECT lastButton={};::SendMessageW(hwndToolbar,TB_GETRECT,IDM_TEXT_WARP,LPARAM(&lastButton));
    int toolbarWidth=max(192,lastButton.right+2),toolbarHeight=max(26,lastButton.bottom);
    ::SetWindowPos(hwndToolbar,NULL,0,0,toolbarWidth,toolbarHeight,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    SetWindowPos(NULL,0,0,toolbar.left+toolbarWidth+6+window.right-window.left-client.right,window.bottom-window.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    ::SendMessageW(hwndToolbar, TB_CHECKBUTTON, IDM_BOLD, registrySettings.Bold);
    ::SendMessageW(hwndToolbar, TB_CHECKBUTTON, IDM_ITALIC, registrySettings.Italic);
    ::SendMessageW(hwndToolbar, TB_CHECKBUTTON, IDM_UNDERLINE, registrySettings.Underline);
}

LRESULT CFontsDialog::OnInitDialog(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    // TODO: Tooltips
    InitFontNames();
    InitFontSizes();
    InitToolbar();

    if (registrySettings.FontsPositionX != 0 || registrySettings.FontsPositionY != 0)
    {
        SetWindowPos(NULL,
                     registrySettings.FontsPositionX, registrySettings.FontsPositionY,
                     0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        SendMessage(DM_REPOSITION, 0, 0);
    }

    if (!registrySettings.ShowTextTool)
        ShowWindow(SW_HIDE);

    return TRUE;
}

LRESULT CFontsDialog::OnClose(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    ShowWindow(SW_HIDE); // Just hide. Recycle for optimization
    return 0;
}

void CFontsDialog::OnFontName(UINT codeNotify)
{
    HWND hwndNames = GetDlgItem(IDD_FONTSNAMES);
    INT iItem = CB_ERR;
    UINT cch;
    WCHAR szText[LF_FACESIZE]={};
    if(codeNotify==CBN_DROPDOWN) { InitFontNames();return; }
    if(codeNotify==CBN_SELENDOK || codeNotify==CBN_KILLFOCUS)
    { RememberFont(registrySettings.strFontName);registrySettings.StorePreferences();return; }

    switch (codeNotify)
    {
        case CBN_SELCHANGE:
            iItem = ComboBox_GetCurSel(hwndNames);
            cch = ComboBox_GetLBTextLen(hwndNames, iItem);
            if (iItem != CB_ERR && 0 < cch && cch < _countof(szText))
                ComboBox_GetLBText(hwndNames, iItem, szText);
            break;

        case CBN_EDITCHANGE:
            GetDlgItemText(IDD_FONTSNAMES, szText, _countof(szText));
            iItem = ComboBox_FindStringExact(hwndNames, -1, szText);
            break;
    }

    if (iItem != CB_ERR && registrySettings.strFontName.CompareNoCase(szText) != 0)
    {
        registrySettings.strFontName = szText;
        toolsModel.NotifyToolChanged();
    }
}

void CFontsDialog::OnFontSize(UINT codeNotify)
{
    HWND hwndSizes = GetDlgItem(IDD_FONTSSIZES);
    WCHAR szText[8]={};
    INT iItem, PointSize = 0;
    UINT cch;

    switch (codeNotify)
    {
        case CBN_SELCHANGE:
            iItem = ComboBox_GetCurSel(hwndSizes);
            cch = ComboBox_GetLBTextLen(hwndSizes, iItem);
            if (iItem != CB_ERR && 0 < cch && cch < _countof(szText))
            {
                ComboBox_GetLBText(hwndSizes, iItem, szText);
                szText[_countof(szText)-1]=0;
                PointSize = _wtoi(szText);
            }
            break;

        case CBN_EDITCHANGE:
            ::GetWindowTextW(hwndSizes, szText, _countof(szText));
            PointSize = _wtoi(szText);
            break;
    }

    if (PointSize > 0)
    {
        registrySettings.PointSize = PointSize;
        toolsModel.NotifyToolChanged();
    }
}

LRESULT CFontsDialog::OnCommand(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    UINT id = LOWORD(wParam);
    UINT codeNotify = HIWORD(wParam);
    HWND hwndToolbar = GetDlgItem(IDD_FONTSTOOLBAR);
    BOOL bChecked = (BOOL)::SendMessageW(hwndToolbar, TB_ISBUTTONCHECKED, id, 0);

    switch (id)
    {
        case IDM_TEXT_WARP:
            ShowTextWarpDialog(mainWindow);break;
        case IDCANCEL:
            ShowWindow(SW_HIDE);
            registrySettings.ShowTextTool = FALSE;
            break;

        case IDD_FONTSNAMES:
            OnFontName(codeNotify);
            break;

        case IDD_FONTSSIZES:
            OnFontSize(codeNotify);
            break;

        case IDM_BOLD:
            registrySettings.Bold = bChecked;
            toolsModel.NotifyToolChanged();
            break;

        case IDM_ITALIC:
            registrySettings.Italic = bChecked;
            toolsModel.NotifyToolChanged();
            break;

        case IDM_UNDERLINE:
            registrySettings.Underline = bChecked;
            toolsModel.NotifyToolChanged();
            break;

        case IDM_ALIGN_LEFT: case IDM_ALIGN_CENTER: case IDM_ALIGN_RIGHT: case IDM_ALIGN_JUSTIFY:
            registrySettings.TextAlignment=id-IDM_ALIGN_LEFT;registrySettings.StorePreferences();
            for(UINT i=0;i<4;++i) ::SendMessageW(hwndToolbar,TB_CHECKBUTTON,IDM_ALIGN_LEFT+i,i==registrySettings.TextAlignment);
            if(textEditWindow.IsWindow()) textEditWindow.UpdateFont();
            break;
    }
    return 0;
}

LRESULT CFontsDialog::OnNotify(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    NMHDR *pnmhdr = reinterpret_cast<NMHDR *>(lParam);
    if (pnmhdr->code == TTN_NEEDTEXT)
    {
        LPTOOLTIPTEXTW pToolTip = reinterpret_cast<LPTOOLTIPTEXTW>(lParam);
        pToolTip->hinst = g_hinstExe;
        switch (pnmhdr->idFrom)
        {
            case IDM_BOLD:      pToolTip->lpszText = MAKEINTRESOURCEW(IDS_BOLD); break;
            case IDM_ITALIC:    pToolTip->lpszText = MAKEINTRESOURCEW(IDS_ITALIC); break;
            case IDM_UNDERLINE: pToolTip->lpszText = MAKEINTRESOURCEW(IDS_UNDERLINE); break;
            case IDM_TEXT_WARP: pToolTip->lpszText=MAKEINTRESOURCEW(IDS_TEXT_WARP);break;
            case IDM_ALIGN_LEFT: case IDM_ALIGN_CENTER: case IDM_ALIGN_RIGHT: case IDM_ALIGN_JUSTIFY:
                pToolTip->lpszText=MAKEINTRESOURCEW(IDS_ALIGN_LEFT+pnmhdr->idFrom-IDM_ALIGN_LEFT);break;

            default:
                break;
        }
    }
    return 0;
}

LRESULT CFontsDialog::OnMove(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    RECT rc;
    GetWindowRect(&rc);
    registrySettings.FontsPositionX = rc.left;
    registrySettings.FontsPositionY = rc.top;
    return 0;
}

LRESULT CFontsDialog::OnToolsModelToolChanged(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (wParam != TOOL_TEXT)
        ShowWindow(SW_HIDE);

    return 0;
}

LRESULT CFontsDialog::OnMeasureItem(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (wParam == IDD_FONTSNAMES)
    {
        LPMEASUREITEMSTRUCT pMeasureItem = reinterpret_cast<LPMEASUREITEMSTRUCT>(lParam);
        RECT rc;
        ::GetClientRect(GetDlgItem(IDD_FONTSNAMES), &rc);
        pMeasureItem->itemWidth = rc.right - rc.left;
        pMeasureItem->itemHeight = GetSystemMetrics(SM_CYVSCROLL);
        return TRUE;
    }
    return 0;
}

LRESULT CFontsDialog::OnDrawItem(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    // TODO: Owner-draw the font types
    if (wParam == IDD_FONTSNAMES)
    {
        LPDRAWITEMSTRUCT pDrawItem = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
        if (pDrawItem->itemID == (UINT)-1)
            return TRUE;

        ::SetBkMode(pDrawItem->hDC, TRANSPARENT);

        HWND hwndItem = pDrawItem->hwndItem;
        RECT rcItem = pDrawItem->rcItem;
        if (pDrawItem->itemState & ODS_SELECTED)
        {
            ::FillRect(pDrawItem->hDC, &rcItem, GetSysColorBrush(COLOR_HIGHLIGHT));
            ::SetTextColor(pDrawItem->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
        }
        else
        {
            ::FillRect(pDrawItem->hDC, &rcItem, GetSysColorBrush(COLOR_WINDOW));
            ::SetTextColor(pDrawItem->hDC, GetSysColor(COLOR_WINDOWTEXT));
        }

        WCHAR szText[LF_FACESIZE];
        if ((UINT)ComboBox_GetLBTextLen(hwndItem, pDrawItem->itemID) < _countof(szText))
        {
            szText[0] = 0;
            ComboBox_GetLBText(hwndItem, pDrawItem->itemID, szText);

            LOGFONTW lf={};lf.lfHeight=-(rcItem.bottom-rcItem.top-3);lf.lfCharSet=DEFAULT_CHARSET;
            StringCchCopyW(lf.lfFaceName,LF_FACESIZE,szText);HFONT font=CreateFontIndirectW(&lf);
            HGDIOBJ old=SelectObject(pDrawItem->hDC,font);rcItem.left+=5;
            ::DrawTextW(pDrawItem->hDC,szText,-1,&rcItem,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            SelectObject(pDrawItem->hDC,old);DeleteObject(font);
            if(m_recentCount && pDrawItem->itemID==m_recentCount-1 && !(pDrawItem->itemState&ODS_COMBOBOXEDIT))
            { RECT line=pDrawItem->rcItem;line.top=line.bottom-1;FillRect(pDrawItem->hDC,&line,GetSysColorBrush(COLOR_3DSHADOW)); }
        }

        if (pDrawItem->itemState & ODS_FOCUS)
            ::DrawFocusRect(pDrawItem->hDC, &pDrawItem->rcItem);

        return TRUE;
    }
    return 0;
}
