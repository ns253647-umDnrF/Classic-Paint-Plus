/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    The main window and wWinMain etc.
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 *             Copyright 2017-2023 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
 *             Copyright 2018 Stanislav Motylkov <x86corez@gmail.com>
 */

#include "precomp.h"
#include "applicationinfo.h"
#include "colorpicker.h"
#include "settings.h"
#include "canvasdialogs.h"
#include "canvasresource.h"
#include "jpeg.h"
#include "jpeg_resource.h"

#include <dlgs.h>
#include <mapi.h>
#include <assert.h>

BOOL g_askBeforeEnlarging = FALSE;  // TODO: initialize from registry
HINSTANCE g_hinstExe = NULL;
WCHAR g_szFileName[MAX_LONG_PATH] = { 0 };
WCHAR g_szMailTempFile[MAX_LONG_PATH] = { 0 };
BOOL g_isAFile = FALSE;
BOOL g_imageSaved = FALSE;
BOOL g_showGrid = FALSE;
HWND g_hStatusBar = NULL;

CMainWindow mainWindow;

typedef HWND (WINAPI *FN_HtmlHelpW)(HWND, LPCWSTR, UINT, DWORD_PTR);

static HINSTANCE s_hHHCTRL_OCX = NULL; // HtmlHelpW needs "hhctrl.ocx"
static FN_HtmlHelpW s_pHtmlHelpW = NULL;

/* FUNCTIONS ********************************************************/

void ShowOutOfMemory(void)
{
    WCHAR szText[256];
    ::FormatMessageW(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
                     NULL,
                     ERROR_OUTOFMEMORY,
                     0,
                     szText, _countof(szText),
                     NULL);
    mainWindow.MessageBox(szText, NULL, MB_ICONERROR);
}

// Get the first extension from the selected, application-owned filter.
static CStringW FileExtFromFilter(const OPENFILENAMEW& ofn)
{
    DWORD index=1;
    for(LPCWSTR entry=ofn.lpstrFilter;entry && *entry;++index)
    {
        entry+=wcslen(entry)+1;
        if(index==ofn.nFilterIndex)
        {
            CStringW pattern(entry);int separator=pattern.Find(L';');
            if(separator>=0) pattern=pattern.Left(separator);
            if(pattern.Left(2)!=L"*.") return L"";
            pattern.Delete(0);pattern.MakeLower();return pattern;
        }
        entry+=wcslen(entry)+1;
    }
    return L"";
}

// Hook procedure for OPENFILENAME to change the file name extension
static UINT_PTR APIENTRY
OFNHookProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UINT_PTR handled=JpegSaveHook(hwnd,uMsg,wParam,lParam);
    if(handled) return handled;
    HWND hParent;
    OFNOTIFYW *pon;
    switch (uMsg)
    {
    case WM_NOTIFY:
        pon = (OFNOTIFYW *)lParam;
        if (pon->hdr.code == CDN_TYPECHANGE)
        {
            hParent = GetParent(hwnd);
            LRESULT length=SendMessageW(hParent,CDM_GETFILEPATH,0,0);
            if(length<=0 || length>MAX_LONG_PATH) break;
            CStringW path;LPWSTR buffer=path.GetBuffer(int(length));
            LRESULT copied=SendMessageW(hParent,CDM_GETFILEPATH,length,LPARAM(buffer));
            if(copied<=0 || copied>length) { path.ReleaseBuffer(0);break; }
            size_t used=wcsnlen_s(buffer,size_t(length));
            if(used>=size_t(length)) { path.ReleaseBuffer(0);break; }
            path.ReleaseBuffer(int(used));
            CStringW extension=FileExtFromFilter(*pon->lpOFN);if(extension.IsEmpty()) break;
            int dot=int(PathFindExtensionW(path)-LPCWSTR(path));
            path=path.Left(dot)+extension;
            if(path.GetLength()>=int(pon->lpOFN->nMaxFile)) break;
            SendMessageW(hParent,CDM_SETCONTROLTEXT,cmb13,LPARAM(PathFindFileNameW(path)));
            StringCchCopyW(pon->lpOFN->lpstrFile,pon->lpOFN->nMaxFile,path);
        }
        break;
    }
    return 0;
}

typedef ULONG (WINAPI *FN_MAPISendMail)(LHANDLE, ULONG_PTR, lpMapiMessage, FLAGS, ULONG);
typedef ULONG (WINAPI *FN_MAPISendMailW)(LHANDLE, ULONG_PTR, lpMapiMessageW, FLAGS, ULONG);

BOOL OpenMailer(HWND hWnd, LPCWSTR pszPathName)
{
    // Delete the temporary file if any
    if (g_szMailTempFile[0])
    {
        ::DeleteFileW(g_szMailTempFile);
        g_szMailTempFile[0] = UNICODE_NULL;
    }

    CStringW strFileTitle;
    if (PathFileExistsW(pszPathName) && imageModel.IsImageSaved())
    {
        strFileTitle = PathFindFileNameW(pszPathName);
    }
    else // Not existing or not saved
    {
        // Get the name of a temporary file
        WCHAR szTempDir[MAX_PATH];
        ::GetTempPathW(_countof(szTempDir), szTempDir);
        if (!::GetTempFileNameW(szTempDir, L"afx", 0, g_szMailTempFile))
            return FALSE; // Failure

        if (PathFileExistsW(g_szFileName))
        {
            // Set file title
            strFileTitle = PathFindFileNameW(g_szFileName);

            // Copy to the temporary file
            if (!::CopyFileW(g_szFileName, g_szMailTempFile, FALSE))
            {
                g_szMailTempFile[0] = UNICODE_NULL;
                return FALSE; // Failure
            }
        }
        else
        {
            // Set file title
            strFileTitle.LoadString(IDS_DEFAULTFILENAME);
            strFileTitle += L".png";

            // Save it to the temporary file
            HBITMAP hbmLocked = imageModel.LockBitmap();
            BOOL ret = SaveDIBToFile(hbmLocked, g_szMailTempFile, FALSE, Gdiplus::ImageFormatPNG);
            imageModel.UnlockBitmap(hbmLocked);
            if (!ret)
            {
                g_szMailTempFile[0] = UNICODE_NULL;
                return FALSE; // Failure
            }
        }

        // Use the temporary file 
        pszPathName = g_szMailTempFile;
    }

    // Load "mapi32.dll"
    HINSTANCE hMAPI = LoadLibraryW(L"mapi32.dll");
    if (!hMAPI)
        return FALSE; // Failure

    // Attachment
    MapiFileDescW attachmentW = { 0 };
    attachmentW.nPosition = (ULONG)-1;
    attachmentW.lpszPathName = (LPWSTR)pszPathName;
    attachmentW.lpszFileName = (LPWSTR)(LPCWSTR)strFileTitle;

    // Message with attachment
    MapiMessageW messageW = { 0 };
    messageW.lpszSubject = NULL;
    messageW.nFileCount = 1;
    messageW.lpFiles = &attachmentW;

    // First, try to open the mailer by the function of Unicode version
    FN_MAPISendMailW pMAPISendMailW = (FN_MAPISendMailW)::GetProcAddress(hMAPI, "MAPISendMailW");
    if (pMAPISendMailW)
    {
        pMAPISendMailW(0, (ULONG_PTR)hWnd, &messageW, MAPI_DIALOG | MAPI_LOGON_UI, 0);
        ::FreeLibrary(hMAPI);
        return TRUE; // MAPISendMailW will show an error message on failure
    }

    // Convert to ANSI strings
    CStringA szPathNameA(pszPathName), szFileTitleA(strFileTitle);

    MapiFileDesc attachment = { 0 };
    attachment.nPosition = (ULONG)-1;
    attachment.lpszPathName = (LPSTR)(LPCSTR)szPathNameA;
    attachment.lpszFileName = (LPSTR)(LPCSTR)szFileTitleA;

    MapiMessage message = { 0 };
    message.lpszSubject = NULL;
    message.nFileCount = 1;
    message.lpFiles = &attachment;

    // Try again but in ANSI version
    FN_MAPISendMail pMAPISendMail = (FN_MAPISendMail)::GetProcAddress(hMAPI, "MAPISendMail");
    if (pMAPISendMail)
    {
        pMAPISendMail(0, (ULONG_PTR)hWnd, &message, MAPI_DIALOG | MAPI_LOGON_UI, 0);
        ::FreeLibrary(hMAPI);
        return TRUE; // MAPISendMail will show an error message on failure
    }

    ::FreeLibrary(hMAPI);
    return FALSE; // Failure
}

BOOL CMainWindow::GetOpenFileName(IN OUT LPWSTR pszFile, INT cchMaxFile)
{
    static OPENFILENAMEW ofn = { 0 };
    static CStringW strFilter;

    // Rebuild translated captions after a live language switch.
    strFilter = BuildOpenImageFilters();
    if (ofn.lStructSize == 0)
    {
        strFilter = BuildOpenImageFilters();

        // Initializing the OPENFILENAME structure for GetOpenFileName
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = m_hWnd;
        ofn.hInstance   = g_hinstExe;
        ofn.lpstrFilter = strFilter;
        ofn.Flags       = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
        ofn.lpstrDefExt = L"png";
    }

    ofn.lpstrFilter = strFilter;
    ofn.lpstrFile = pszFile;
    ofn.nMaxFile  = cchMaxFile;
    return ::GetOpenFileNameW(&ofn);
}

BOOL CMainWindow::GetSaveFileName(IN OUT LPWSTR pszFile, INT cchMaxFile)
{
    static OPENFILENAMEW sfn = { 0 };
    static CStringW strFilter;

    strFilter = BuildSaveImageFilters();

    if (sfn.lStructSize == 0)
    {
        strFilter = BuildSaveImageFilters();

        // Initializing the OPENFILENAME structure for GetSaveFileName
        ZeroMemory(&sfn, sizeof(sfn));
        sfn.lStructSize = sizeof(sfn);
        sfn.hwndOwner   = m_hWnd;
        sfn.hInstance   = g_hinstExe;
        sfn.lpstrFilter = strFilter;
        sfn.Flags       = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_ENABLEHOOK | OFN_ENABLETEMPLATE | OFN_ENABLESIZING | OFN_NOCHANGEDIR;
        sfn.lpTemplateName = MAKEINTRESOURCEW(IDD_JPEG_SAVE);
        sfn.lpfnHook    = OFNHookProc;
        sfn.lpstrDefExt = L"png";

        LPWSTR pchDotExt = PathFindExtensionW(pszFile);
        if (*pchDotExt == UNICODE_NULL)
        {
            StringCchCatW(pszFile, cchMaxFile, L".png");
            sfn.nFilterIndex = SaveFormatFilterIndex(SaveFormat::Png);
        }
    }

    sfn.lpstrFilter = strFilter;
    sfn.lpstrFile = pszFile;
    sfn.nMaxFile  = cchMaxFile;
    // Opening Save As for an existing JPEG must select JPEG immediately.
    DWORD filterIndex=1;
    for(LPCWSTR p=sfn.lpstrFilter;*p;++filterIndex)
    {
        p+=wcslen(p)+1;
        if(PathMatchSpecW(pszFile,p)) { sfn.nFilterIndex=filterIndex;break; }
        p+=wcslen(p)+1;
    }
    if(FormatFromPath(pszFile)==SaveFormat::Bmp24)
        sfn.nFilterIndex=SaveFormatFilterIndex(int(CurrentSaveFormat())<=int(SaveFormat::Bmp8)?CurrentSaveFormat():SaveFormat::Bmp24);
    SaveImageOptions options;
    options.jpeg=CurrentJpegOptions();options.png=PngOptions::LoadDefaults();
    options.webp=CurrentWebpOptions();options.ico=CurrentIcoOptions();
    options.dds=CurrentDdsOptions();options.preservePaintData=PreservePaintData();
    sfn.lCustData=reinterpret_cast<LPARAM>(&options);
    BOOL result=::GetSaveFileNameW(&sfn);
    if(result)
    {
        SetCurrentJpegOptions(options.jpeg);SetCurrentPngOptions(options.png);
        SetCurrentWebpOptions(options.webp);SetCurrentIcoOptions(options.ico);
        SetCurrentDdsOptions(options.dds);SetPreservePaintData(options.preservePaintData);
        SetPendingImageFormat(pszFile,SaveFormatFromFilterIndex(sfn.nFilterIndex));
    }
    sfn.lCustData=0;
    return result;
}

BOOL CMainWindow::ChooseColor(IN OUT COLORREF *prgbColor)
{
    return ChooseColorFromCanvas(m_hWnd, prgbColor);
}

HWND CMainWindow::DoCreate()
{
    InitializeTheme();
    ::LoadStringW(g_hinstExe, IDS_DEFAULTFILENAME, g_szFileName, _countof(g_szFileName));

    CStringW strTitle;
    strTitle.Format(IDS_WINDOWTITLE, PathFindFileName(g_szFileName));

    RECT& rc = registrySettings.WindowPlacement.rcNormalPosition;
    return Create(HWND_DESKTOP, rc, strTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, WS_EX_ACCEPTFILES);
}

// A wrapper function for HtmlHelpW
static HWND DoHtmlHelpW(HWND hwndCaller, LPCWSTR pszFile, UINT uCommand, DWORD_PTR dwData)
{
    WCHAR szPath[MAX_PATH];

    if (!s_hHHCTRL_OCX && (uCommand != HH_CLOSE_ALL))
    {
        // The function loads the system library, not local
        GetSystemDirectoryW(szPath, _countof(szPath));
        StringCchCatW(szPath, _countof(szPath), L"\\hhctrl.ocx");
        s_hHHCTRL_OCX = LoadLibraryW(szPath);
        if (s_hHHCTRL_OCX)
            s_pHtmlHelpW = (FN_HtmlHelpW)GetProcAddress(s_hHHCTRL_OCX, "HtmlHelpW");
    }

    if (!s_pHtmlHelpW)
        return NULL;

    return s_pHtmlHelpW(hwndCaller, pszFile, uCommand, dwData);
}

void CMainWindow::alignChildrenToMainWindow()
{
    RECT clientRect, rc;
    GetClientRect(&clientRect);
    RECT rcSpace = clientRect;
    const UINT uFlags = (SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREPOSITION);

    if (::IsWindowVisible(g_hStatusBar))
    {
        ::GetWindowRect(g_hStatusBar, &rc);
        rcSpace.bottom -= rc.bottom - rc.top;
    }

    HDWP hDWP = ::BeginDeferWindowPos(5);
    int stripHeight=PanelPx(CY_PALETTE);
    const bool toolsVisible=::IsWindowVisible(toolBoxContainer)!=FALSE;
    int toolboxWidth=0;
    for(int pass=0;pass<3;++pass)
    {
        toolboxWidth=ToolboxWidth(max(1,rcSpace.bottom-rcSpace.top-stripHeight));
        int available=max(1,rcSpace.right-rcSpace.left-(toolsVisible?toolboxWidth:0));
        int width=min(PanelPx(PALETTE_FULL_WIDTH)+2,max(1,available/3));
        bool scroll=::IsWindowVisible(paletteWindow) && width-2<PanelPx((DocumentAlpha().enabled?340:268)+PALETTE_EXTRA_GAP);
        int next=PanelPx(CY_PALETTE)+(scroll?GetSystemMetrics(SM_CYHSCROLL):0);
        if(next==stripHeight)break;
        stripHeight=next;
    }
    const bool stripAtTop=::IsWindowVisible(paletteWindow) && registrySettings.Bar1ID!=BAR1ID_BOTTOM;
    const int stripY=stripAtTop?rcSpace.top:rcSpace.bottom-stripHeight;
    int toolX=registrySettings.Bar2ID==BAR2ID_RIGHT?rcSpace.right-toolboxWidth:rcSpace.left;
    if(toolsVisible)
    {
        hDWP=::DeferWindowPos(hDWP,toolBoxContainer,NULL,toolX,rcSpace.top+(stripAtTop?stripHeight:0),toolboxWidth,max(1,rcSpace.bottom-rcSpace.top-stripHeight),uFlags);
        if(registrySettings.Bar2ID==BAR2ID_RIGHT)rcSpace.right-=toolboxWidth;else rcSpace.left+=toolboxWidth;
    }
    if(::IsWindow(ObjectActionsWindow()))
    {
        ::ShowWindow(ObjectActionsWindow(),toolsVisible?SW_SHOWNA:SW_HIDE);
        hDWP=::DeferWindowPos(hDWP,ObjectActionsWindow(),NULL,toolX,stripY,toolboxWidth,stripHeight,uFlags);
    }
    int available=max(1,rcSpace.right-rcSpace.left),paletteWidth=0;
    if(::IsWindowVisible(paletteWindow))
    {
        paletteWidth=min(PanelPx(PALETTE_FULL_WIDTH)+2,max(1,available/3));
        hDWP=::DeferWindowPos(hDWP,paletteWindow,NULL,rcSpace.left,stripY,paletteWidth,stripHeight,uFlags);
    }
    if(::IsWindow(ParameterBarWindow()))
        hDWP=::DeferWindowPos(hDWP,ParameterBarWindow(),NULL,rcSpace.left+paletteWidth,stripY,max(1,available-paletteWidth),stripHeight,uFlags);
    if(stripAtTop)rcSpace.top+=stripHeight;else rcSpace.bottom-=stripHeight;
    if(canvasWindow.IsWindow())
        hDWP=::DeferWindowPos(hDWP,canvasWindow,NULL,rcSpace.left,rcSpace.top,max(1,rcSpace.right-rcSpace.left),max(1,rcSpace.bottom-rcSpace.top),uFlags);

    ::EndDeferWindowPos(hDWP);
}

void CMainWindow::saveImage(BOOL overwrite)
{
    canvasWindow.OnEndDraw(FALSE);

    WCHAR destination[MAX_LONG_PATH];
    StringCchCopyW(destination,_countof(destination),g_szFileName);

    // Is the extension not supported?
    PWCHAR pchDotExt = PathFindExtensionW(destination);
    if (pchDotExt && *pchDotExt && !CImageDx::IsExtensionSupported(pchDotExt) && lstrcmpiW(pchDotExt,L".webp") && lstrcmpiW(pchDotExt,L".ico") && lstrcmpiW(pchDotExt,L".tga") && lstrcmpiW(pchDotExt,L".targa") && lstrcmpiW(pchDotExt,L".dds"))
    {
        // Remove the extension
        PathRemoveExtensionW(destination);
        // No overwrite
        overwrite = FALSE;
    }

    if (g_isAFile && overwrite)
    {
        imageModel.SaveImage(g_szFileName);
    }
    else if (GetSaveFileName(destination, _countof(destination)))
    {
        imageModel.SaveImage(destination);
    }
}

void CMainWindow::InsertSelectionFromHBITMAP(HBITMAP bitmap, HWND window)
{
    int width = GetDIBWidth(bitmap);
    int height = GetDIBHeight(bitmap);
    int curWidth = imageModel.GetWidth();
    int curHeight = imageModel.GetHeight();

    if (width > curWidth || height > curHeight)
    {
        BOOL shouldEnlarge = TRUE;

        if (g_askBeforeEnlarging)
        {
            CStringW programname,shouldEnlargePromptText;
            programname.LoadString(g_hinstExe,IDS_PROGRAMNAME);
            shouldEnlargePromptText.LoadString(g_hinstExe,IDS_ENLARGEPROMPTTEXT);

            switch (MessageBox(shouldEnlargePromptText, programname, MB_YESNOCANCEL | MB_ICONQUESTION))
            {
                case IDYES:
                    break;
                case IDNO:
                    shouldEnlarge = FALSE;
                    break;
                case IDCANCEL:
                    DeleteObject(bitmap);
                    return;
            }
        }

        if (shouldEnlarge)
        {
            if (width > curWidth)
                curWidth = width;

            if (height > curHeight)
                curHeight = height;

        }
    }

    // Pasting replaces the active object, whereas merely switching between the
    // rectangle and lasso tools now preserves it. Commit explicitly, after Cancel.
    canvasWindow.OnEndDraw(FALSE);
    if(curWidth!=imageModel.GetWidth() || curHeight!=imageModel.GetHeight())
        imageModel.Crop(curWidth,curHeight,0,0);
    toolsModel.SetActiveTool(TOOL_RECTSEL);

    POINT origin=canvasWindow.VisibleImageOrigin();
    selectionModel.InsertFromHBITMAP(bitmap, origin.x, origin.y);
    selectionModel.m_bShow = TRUE;
    imageModel.NotifyImageChanged();
}

LRESULT CMainWindow::OnMouseWheel(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    INT zDelta = (SHORT)HIWORD(wParam);

    if ((LOWORD(wParam)&MK_CONTROL) || ::GetKeyState(VK_CONTROL) < 0) // Ctrl+Wheel
    {
        if(zDelta)
        {
            int zoom=toolsModel.GetZoom();
            int next=max(MIN_ZOOM,min(MAX_ZOOM,zDelta<0?zoom/2:zoom*2));
            POINT point={GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};
            canvasWindow.ScreenToClient(&point);
            CRect client;canvasWindow.GetClientRect(client);
            if(client.PtInRect(point))canvasWindow.zoomAt(next,point);
            else if(next!=zoom)canvasWindow.zoomTo(next);
        }
    }
    else // Wheel only
    {
        UINT nCount = 3;
        if (::GetAsyncKeyState(VK_SHIFT) < 0)
        {
#ifndef SPI_GETWHEELSCROLLCHARS
    #define SPI_GETWHEELSCROLLCHARS 0x006C  // Needed for pre-NT6 PSDK
#endif
            SystemParametersInfoW(SPI_GETWHEELSCROLLCHARS, 0, &nCount, 0);
            for (UINT i = 0; i < nCount; ++i)
            {
                if (zDelta < 0)
                    ::PostMessageW(canvasWindow, WM_HSCROLL, MAKEWPARAM(SB_LINERIGHT, 0), 0);
                else if (zDelta > 0)
                    ::PostMessageW(canvasWindow, WM_HSCROLL, MAKEWPARAM(SB_LINELEFT, 0), 0);
            }
        }
        else
        {
            SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &nCount, 0);
            for (UINT i = 0; i < nCount; ++i)
            {
                if (zDelta < 0)
                    ::PostMessageW(canvasWindow, WM_VSCROLL, MAKEWPARAM(SB_LINEDOWN, 0), 0);
                else if (zDelta > 0)
                    ::PostMessageW(canvasWindow, WM_VSCROLL, MAKEWPARAM(SB_LINEUP, 0), 0);
            }
        }
    }

    return 0;
}

LRESULT CMainWindow::OnDropFiles(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    WCHAR droppedfile[MAX_PATH];

    HDROP hDrop = (HDROP)wParam;
    DragQueryFile(hDrop, 0, droppedfile, _countof(droppedfile));
    DragFinish(hDrop);

    ConfirmSave() && DoLoadImageFile(m_hWnd, droppedfile, TRUE);

    return 0;
}

LRESULT CMainWindow::OnCreate(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    // Loading and setting the window menu from resource
    m_hMenu = ::LoadMenuW(g_hinstExe, MAKEINTRESOURCEW(ID_MENU));
    BuildSettingsMenu(m_hMenu);
    BuildCanvasMenu(m_hMenu);
    SetMenu(m_hMenu);
    m_hAccel=::LoadAcceleratorsW(g_hinstExe,MAKEINTRESOURCEW(ID_ACCELERATORS));

    // Create the status bar
    DWORD style = SBARS_SIZEGRIP | WS_CHILD | (registrySettings.ShowStatusBar ? WS_VISIBLE : 0);
    g_hStatusBar = ::CreateWindowExW(0, STATUSCLASSNAME, NULL, style, 0, 0, 0, 0, m_hWnd,
                                     NULL, g_hinstExe, NULL);
    ::SendMessageW(g_hStatusBar, SB_SETMINHEIGHT, 21, 0);

    // Create the tool box
    toolBoxContainer.DoCreate(m_hWnd);

    // Create the palette window
    RECT rcEmpty = { 0, 0, 0, 0 }; // Rely on WM_SIZE
    style = WS_CHILD | (registrySettings.ShowPalette ? WS_VISIBLE : 0);
    paletteWindow.Create(m_hWnd, rcEmpty, NULL, style, WS_EX_STATICEDGE);

    // Create the canvas
    style = WS_CHILD | WS_GROUP | WS_HSCROLL | WS_VSCROLL | WS_VISIBLE;
    canvasWindow.Create(m_hWnd, rcEmpty, NULL, style, WS_EX_CLIENTEDGE);
    CreateParameterBar(m_hWnd);

    // Create and show the miniature if necessary
    if (registrySettings.ShowThumbnail)
    {
        miniature.DoCreate(m_hWnd);
        miniature.ShowWindow(SW_SHOWNOACTIVATE);
    }

    // Set icon
    SendMessage(WM_SETICON, ICON_BIG, (LPARAM)::LoadIconW(g_hinstExe, MAKEINTRESOURCEW(IDI_APPICON)));
    SendMessage(WM_SETICON, ICON_SMALL, (LPARAM)::LoadIconW(g_hinstExe, MAKEINTRESOURCEW(IDI_APPICON)));

    return 0;
}

LRESULT CMainWindow::OnDestroy(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    registrySettings.WindowPlacement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(&(registrySettings.WindowPlacement));

    DoHtmlHelpW(NULL, NULL, HH_CLOSE_ALL, 0);

    if (s_hHHCTRL_OCX)
    {
        FreeLibrary(s_hHHCTRL_OCX);
        s_hHHCTRL_OCX = NULL;
        s_pHtmlHelpW = NULL;
    }

    SetMenu(NULL);
    if (m_hMenu)
    {
        ForgetThemedMenu(m_hMenu);
        ::DestroyMenu(m_hMenu);
        m_hMenu = NULL;
    }
    if(m_hAccel) { ::DestroyAcceleratorTable(m_hAccel);m_hAccel=NULL; }

    PostQuitMessage(0); /* send a WM_QUIT to the message queue */
    return 0;
}

void CMainWindow::ReloadLanguage()
{
    HMENU next=::LoadMenuW(g_hinstExe,MAKEINTRESOURCEW(ID_MENU));if(!next) return;
    BuildSettingsMenu(next);BuildCanvasMenu(next);ThemeMenu(next,true);
    HMENU previous=m_hMenu;m_hMenu=next;SetMenu(next);
    if(previous) { ForgetThemedMenu(previous);::DestroyMenu(previous); }
    if(m_hAccel) ::DestroyAcceleratorTable(m_hAccel);
    m_hAccel=::LoadAcceleratorsW(g_hinstExe,MAKEINTRESOURCEW(ID_ACCELERATORS));
    CStringW title;title.Format(IDS_WINDOWTITLE,PathFindFileNameW(g_szFileName));SetWindowText(title);
    if(toolBoxContainer.IsWindow()) toolBoxContainer.ReloadLanguage();
    if(miniature.IsWindow()) { title.LoadString(g_hinstExe,IDS_MINIATURETITLE);miniature.SetWindowText(title); }
    if(fontsDialog.IsWindow()) { title.LoadString(g_hinstExe,IDS_FONTS_TITLE);fontsDialog.SetWindowText(title);fontsDialog.InitToolbar(); }
    for(int part:{0,1,2}) ::SendMessageW(g_hStatusBar,SB_SETTEXT,part,LPARAM(L""));
    UpdateSettingsChecks(next);DrawMenuBar();
    RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_FRAME|RDW_ALLCHILDREN);
}
BOOL CMainWindow::ConfirmSave()
{
    canvasWindow.OnEndDraw(FALSE);

    if (imageModel.IsImageSaved())
        return TRUE;

    CStringW strProgramName;
    strProgramName.LoadString(IDS_PROGRAMNAME);

    CStringW strSavePromptText;
    strSavePromptText.Format(IDS_SAVEPROMPTTEXT, PathFindFileName(g_szFileName));

    switch (MessageBox(strSavePromptText, strProgramName, MB_YESNOCANCEL | MB_ICONQUESTION))
    {
        case IDYES:
            saveImage(TRUE);
            return imageModel.IsImageSaved();
        case IDNO:
            return TRUE;
        case IDCANCEL:
            return FALSE;
    }

    return TRUE;
}

LRESULT CMainWindow::OnClose(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (ConfirmSave())
    {
        DestroyWindow();
    }
    return 0;
}

void CMainWindow::ProcessFileMenu(HMENU hPopupMenu)
{
    for (INT iItem = 0; iItem < MAX_RECENT_FILES; ++iItem)
        RemoveMenu(hPopupMenu, IDM_FILE1 + iItem, MF_BYCOMMAND);

    if (registrySettings.strFiles[0].IsEmpty())
        return;

    RemoveMenu(hPopupMenu, IDM_FILEMOSTRECENTLYUSEDFILE, MF_BYCOMMAND);

    INT cMenuItems = GetMenuItemCount(hPopupMenu);

    for (INT iItem = 0; iItem < MAX_RECENT_FILES; ++iItem)
    {
        CStringW& strFile = registrySettings.strFiles[iItem];
        if (strFile.IsEmpty())
            break;

        // Condense the lengthy pathname by using '...'
#define MAX_RECENT_PATHNAME_DISPLAY 30
        CPath pathFile(strFile);
        pathFile.CompactPathEx(MAX_RECENT_PATHNAME_DISPLAY);
        assert(wcslen((LPCWSTR)pathFile) <= MAX_RECENT_PATHNAME_DISPLAY);

        // Add an accelerator (by '&') to the item number for quick access
        WCHAR szText[4 + MAX_RECENT_PATHNAME_DISPLAY + 1];
        StringCchPrintfW(szText, _countof(szText), L"&%u %s", iItem + 1, (LPCWSTR)pathFile);

        INT iMenuItem = (cMenuItems - 2) + iItem;
        InsertMenu(hPopupMenu, iMenuItem, MF_BYPOSITION | MF_STRING, IDM_FILE1 + iItem, szText);
    }
}

BOOL CMainWindow::CanUndo() const
{
    if(toolsModel.HasEditableFigure()) return TRUE;
    if (toolsModel.GetActiveTool() == TOOL_TEXT && ::IsWindowVisible(textEditWindow))
        return (BOOL)textEditWindow.SendMessage(EM_CANUNDO);
    if (selectionModel.m_bShow && toolsModel.IsSelection())
        return TRUE;
    return imageModel.CanUndo();
}

BOOL CMainWindow::CanRedo() const
{
    if (toolsModel.GetActiveTool() == TOOL_TEXT && ::IsWindowVisible(textEditWindow))
        return FALSE; // There is no "WM_REDO" in EDIT control
    return imageModel.CanRedo();
}

BOOL CMainWindow::CanPaste() const
{
    if (toolsModel.GetActiveTool() == TOOL_TEXT && ::IsWindowVisible(textEditWindow))
        return ::IsClipboardFormatAvailable(CF_UNICODETEXT);

    return (::IsClipboardFormatAvailable(FigureClipboardFormat()) || ::IsClipboardFormatAvailable(CF_ENHMETAFILE) ||
            ::IsClipboardFormatAvailable(CF_DIBV5) ||
            ::IsClipboardFormatAvailable(CF_DIB) ||
            ::IsClipboardFormatAvailable(CF_BITMAP));
}

LRESULT CMainWindow::OnInitMenuPopup(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if(InitMarkerSizeMenu(m_hWnd,reinterpret_cast<HMENU>(wParam))) return 0;
    HMENU menu = (HMENU)wParam;
    UpdateSettingsChecks(menu);
    BOOL trueSelection = (selectionModel.m_bShow && toolsModel.IsSelection());
    BOOL editableSelection=trueSelection || toolsModel.HasEditableFigure();
    BOOL textShown = (toolsModel.GetActiveTool() == TOOL_TEXT && ::IsWindowVisible(textEditWindow));
    DWORD dwStart = 0, dwEnd = 0;
    if (textShown)
        textEditWindow.SendMessage(EM_GETSEL, (WPARAM)&dwStart, (LPARAM)&dwEnd);
    BOOL hasTextSel = (dwStart < dwEnd);

    //
    // File menu
    //
    if (::GetSubMenu(GetMenu(), 0) == menu)
    {
        ProcessFileMenu(menu);
    }

    //
    // Edit menu
    //
    EnableMenuItem(menu, IDM_EDITUNDO, ENABLED_IF(CanUndo()));
    EnableMenuItem(menu, IDM_EDITREDO, ENABLED_IF(CanRedo()));
    EnableMenuItem(menu, IDM_EDITCUT, ENABLED_IF(textShown ? hasTextSel : editableSelection));
    EnableMenuItem(menu, IDM_EDITCOPY, ENABLED_IF(textShown ? hasTextSel : editableSelection));
    EnableMenuItem(menu, IDM_EDITDELETESELECTION,
                   ENABLED_IF(textShown ? hasTextSel : editableSelection));
    EnableMenuItem(menu, IDM_EDITINVERTSELECTION, ENABLED_IF(trueSelection));
    EnableMenuItem(menu, IDM_IMAGEINVERTCONTOUR, ENABLED_IF(trueSelection));
    EnableMenuItem(menu, IDM_EDITCOPYTO, ENABLED_IF(trueSelection));
    EnableMenuItem(menu, IDM_EDITPASTE, ENABLED_IF(CanPaste()));
    EnableMenuItem(menu, IDM_CROPSELECTION, ENABLED_IF(trueSelection));

    //
    // View menu
    //
    CheckMenuItem(menu, IDM_VIEWTOOLBOX, CHECKED_IF(::IsWindowVisible(toolBoxContainer)));
    CheckMenuItem(menu, IDM_VIEWCOLORPALETTE, CHECKED_IF(::IsWindowVisible(paletteWindow)));
    CheckMenuItem(menu, IDM_VIEWSTATUSBAR,    CHECKED_IF(::IsWindowVisible(g_hStatusBar)));
    CheckMenuItem(menu, IDM_FORMATICONBAR, CHECKED_IF(::IsWindowVisible(fontsDialog)));
    EnableMenuItem(menu, IDM_FORMATICONBAR, ENABLED_IF(toolsModel.GetActiveTool() == TOOL_TEXT));
    CheckMenuItem(menu, IDM_VIEWZOOM125, CHECKED_IF(toolsModel.GetZoom() == 125));
    CheckMenuItem(menu, IDM_VIEWZOOM25,  CHECKED_IF(toolsModel.GetZoom() == 250));
    CheckMenuItem(menu, IDM_VIEWZOOM50,  CHECKED_IF(toolsModel.GetZoom() == 500));
    CheckMenuItem(menu, IDM_VIEWZOOM100, CHECKED_IF(toolsModel.GetZoom() == 1000));
    CheckMenuItem(menu, IDM_VIEWZOOM200, CHECKED_IF(toolsModel.GetZoom() == 2000));
    CheckMenuItem(menu, IDM_VIEWZOOM400, CHECKED_IF(toolsModel.GetZoom() == 4000));
    CheckMenuItem(menu, IDM_VIEWZOOM800, CHECKED_IF(toolsModel.GetZoom() == 8000));
    CheckMenuItem(menu, IDM_VIEWSHOWGRID,      CHECKED_IF(g_showGrid));
    CheckMenuItem(menu, IDM_VIEWSHOWMINIATURE, CHECKED_IF(registrySettings.ShowThumbnail));

    //
    // Image menu
    //
    EnableMenuItem(menu, IDM_IMAGECROP, ENABLED_IF(selectionModel.m_bShow));
    for(int corner=0;corner<4;++corner)
    { RECT area;EnableMenuItem(menu,IDM_CROP_CORNER_FIRST+corner,ENABLED_IF(SelectionCornerCropRect(corner,area))); }
    EnableMenuItem(menu, IDM_IMAGEDELETEIMAGE, ENABLED_IF(!selectionModel.m_bShow));
    CheckMenuItem(menu,IDM_ALPHA_REPLACE,CHECKED_IF(registrySettings.ReplaceDeletedWithAlpha));
    CheckMenuItem(menu, IDM_IMAGEDRAWOPAQUE, CHECKED_IF(DocumentAlpha().enabled || DocumentAlphaPicking()));

    //
    // Palette menu
    //
    CheckMenuItem(menu, IDM_COLORSMODERNPALETTE, CHECKED_IF(paletteModel.SelectedPalette() == PAL_MODERN));
    CheckMenuItem(menu, IDM_COLORSOLDPALETTE,    CHECKED_IF(paletteModel.SelectedPalette() == PAL_OLDTYPE));
    CheckMenuItem(menu, IDM_COLORSGRAYSCALE,     CHECKED_IF(paletteModel.SelectedPalette() == PAL_GRAYSCALE));
    CheckMenuItem(menu, IDM_COLORSMONOCHROME,    CHECKED_IF(paletteModel.SelectedPalette() == PAL_MONOCHROME));
    return 0;
}

LRESULT CMainWindow::OnPanelMenu(UINT msg,WPARAM wp,LPARAM lp,BOOL& handled)
{ return HandlePanelMenuMessage(m_hWnd,msg,wp,lp,handled); }

LRESULT CMainWindow::OnShowMarkerInput(UINT,WPARAM,LPARAM,BOOL&)
{
    ShowMarkerSizeInput(m_hWnd);return 0;
}

LRESULT CMainWindow::OnSize(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    CRect client;GetClientRect(&client);
    const int right=max(0,client.Width()-GetSystemMetrics(SM_CXVSCROLL)-3);
    const int zoomWidth=72,infoWidth=min(120,max(0,(right-zoomWidth)/2));
    int test[] = { max(0,right-zoomWidth-2*infoWidth),max(0,right-zoomWidth-infoWidth),max(0,right-zoomWidth),right };
    if (::IsWindow(g_hStatusBar))
    {
        ::SendMessageW(g_hStatusBar, WM_SIZE, 0, 0);
        ::SendMessageW(g_hStatusBar, SB_SETPARTS, 4, (LPARAM)&test);
        toolsModel.UpdateZoomStatus();
    }
    alignChildrenToMainWindow();
    return 0;
}

LRESULT CMainWindow::OnGetMinMaxInfo(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    MINMAXINFO *mm = (MINMAXINFO*)lParam;
    mm->ptMinTrackSize = { 330, 540 }; // Keep the new shape row and origin controls accessible.
    return 0;
}

LRESULT CMainWindow::OnKeyDown(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    switch (wParam)
    {
        case VK_HOME:case VK_END:case VK_PRIOR:case VK_NEXT:
            canvasWindow.SendMessage(nMsg,wParam,lParam);
            break;
        case VK_ESCAPE:
            canvasWindow.PostMessage(nMsg, wParam, lParam);
            break;
        case VK_LEFT:
            selectionModel.moveSelection(-1, 0);
            break;
        case VK_RIGHT:
            selectionModel.moveSelection(+1, 0);
            break;
        case VK_UP:
            selectionModel.moveSelection(0, -1);
            break;
        case VK_DOWN:
            selectionModel.moveSelection(0, +1);
            break;
        default:
            break;
    }
    return 0;
}

LRESULT CMainWindow::OnSysColorChange(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    /* Redirect message to common controls */
    HWND hToolbar = FindWindowEx(toolBoxContainer.m_hWnd, NULL, TOOLBARCLASSNAME, NULL);
    ::SendMessageW(hToolbar, WM_SYSCOLORCHANGE, 0, 0);
    return 0;
}

LRESULT CMainWindow::OnCommand(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (HandleSettingsCommand(m_hWnd, LOWORD(wParam)))
        return 0;
    // Disable commands while dragging mouse
    if (canvasWindow.m_drawing && ::GetCapture())
    {
        ATLTRACE("locking!\n");
        return 0;
    }

    BOOL textShown = (toolsModel.GetActiveTool() == TOOL_TEXT && ::IsWindowVisible(textEditWindow));
    if(LOWORD(wParam)>=IDM_SCROLL_CORNER_FIRST && LOWORD(wParam)<IDM_SCROLL_CORNER_FIRST+4)
    { canvasWindow.ScrollToCorner(LOWORD(wParam)-IDM_SCROLL_CORNER_FIRST);return 0; }
    switch (LOWORD(wParam))
    {
        case IDM_HELPINFO:
            ShowRPaintAbout(m_hWnd);
            break;
        case IDM_HELPHELPTOPICS:
            OpenRPaintHelp(m_hWnd);
            break;
        case IDM_FILEEXIT:
            SendMessage(WM_CLOSE, wParam, lParam);
            break;
        case IDM_FILENEW:
            if (ConfirmSave())
            {
                InitializeImage(NULL, NULL, FALSE);
            }
            break;
        case IDM_FILEOPEN:
            {
                WCHAR szFileName[MAX_LONG_PATH] = L"";
                if (ConfirmSave() && GetOpenFileName(szFileName, _countof(szFileName)))
                {
                    DoLoadImageFile(m_hWnd, szFileName, TRUE);
                }
                break;
            }
        case IDM_FILESAVE:
            saveImage(TRUE);
            break;
        case IDM_FILESAVEAS:
            saveImage(FALSE);
            break;
        case IDM_FILEPAGESETUP:
            // DUMMY: Shows the dialog only, no functionality
            PAGESETUPDLG psd;
            ZeroMemory(&psd, sizeof(psd));
            psd.lStructSize = sizeof(psd);
            psd.hwndOwner = m_hWnd;
            PageSetupDlg(&psd);
            if(psd.hDevMode) GlobalFree(psd.hDevMode);
            if(psd.hDevNames) GlobalFree(psd.hDevNames);
            break;
        case IDM_FILEPRINT:
            canvasWindow.OnEndDraw(FALSE);
            PRINTDLG pd;
            ZeroMemory(&pd, sizeof(pd));
            pd.lStructSize = sizeof(pd);
            pd.hwndOwner = m_hWnd;
            pd.hDevMode = NULL;  // freed by user
            pd.hDevNames = NULL;  // freed by user
            pd.Flags = PD_USEDEVMODECOPIESANDCOLLATE | PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION;
            pd.nCopies = 1;
            pd.nFromPage = 0xffff;
            pd.nToPage = 0xffff;
            pd.nMinPage = 1;
            pd.nMaxPage = 0xffff;
            if (PrintDlg(&pd) == TRUE)
            {
                if(!PrintBitmapDocument(pd.hDC,HBITMAP(GetCurrentObject(imageModel.GetDC(),OBJ_BITMAP)),PathFindFileNameW(g_szFileName)))
                {
                    DWORD error=GetLastError();if(!error) error=ERROR_GEN_FAILURE;
                    WCHAR message[512]={};
                    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,error,0,message,_countof(message),NULL);
                    MessageBox(message,NULL,MB_OK|MB_ICONERROR);
                }
                DeleteDC(pd.hDC);
            }
            if (pd.hDevMode)
                GlobalFree(pd.hDevMode);
            if (pd.hDevNames)
                GlobalFree(pd.hDevNames);
            break;
        case IDM_FILESEND:
            canvasWindow.OnEndDraw(FALSE);
            if (!OpenMailer(m_hWnd, g_szFileName))
            {
                ShowError(IDS_CANTSENDMAIL);
            }
            break;
        case IDM_FILEASWALLPAPERPLANE:
            RegistrySettings::SetWallpaper(g_szFileName, RegistrySettings::TILED);
            break;
        case IDM_FILEASWALLPAPERCENTERED:
            RegistrySettings::SetWallpaper(g_szFileName, RegistrySettings::CENTERED);
            break;
        case IDM_FILEASWALLPAPERSTRETCHED:
            RegistrySettings::SetWallpaper(g_szFileName, RegistrySettings::STRETCHED);
            break;
        case IDM_FILE1:
        case IDM_FILE2:
        case IDM_FILE3:
        case IDM_FILE4:
        {
            INT iFile = LOWORD(wParam) - IDM_FILE1;
            if (ConfirmSave())
                DoLoadImageFile(m_hWnd, registrySettings.strFiles[iFile], TRUE);
            break;
        }
        case IDM_EDITUNDO:
            if (textShown)
            {
                textEditWindow.PostMessage(WM_UNDO, 0, 0);
                break;
            }
            canvasWindow.OnEndDraw(FALSE);
            imageModel.Undo();
            break;
        case IDM_EDITREDO:
            if (textShown)
            {
                // There is no "WM_REDO" in EDIT control
                break;
            }
            canvasWindow.OnEndDraw(FALSE);
            imageModel.Redo();
            break;
        case IDM_EDITCOPY:
            if (textShown)
            {
                textEditWindow.SendMessage(WM_COPY);
                break;
            }
            {
                FigureClipboardData figure={};bool isFigure=toolsModel.ExportFigure(figure);
                if(!isFigure && !selectionModel.m_bShow)return FALSE;
                HGLOBAL vector=isFigure?PackFigureClipboard(figure):NULL;
                if(isFigure && !vector)return FALSE;
                HBITMAP hbmCopy=isFigure?RasterizeFigure(figure):selectionModel.GetSelectionContents();
                HGLOBAL alpha=BitmapToClipboardDIBV5(hbmCopy);
                HGLOBAL hGlobal = BitmapToClipboardDIB(hbmCopy);
                ::DeleteObject(hbmCopy);
                // Prepare data before replacing the clipboard. Cut may delete
                // pixels only when at least one bitmap format was transferred.
                if(!alpha && !hGlobal) { if(vector)GlobalFree(vector);ShowOutOfMemory();return FALSE; }
                BOOL copied=FALSE,vectorCopied=!isFigure;
                if(OpenClipboard())
                {
                    if(EmptyClipboard())
                    {
                        if(vector && ::SetClipboardData(FigureClipboardFormat(),vector)) { vector=NULL;vectorCopied=TRUE; }
                        if(alpha && ::SetClipboardData(CF_DIBV5,alpha)) { alpha=NULL;copied=TRUE; }
                        if(hGlobal && ::SetClipboardData(CF_DIB,hGlobal)) { hGlobal=NULL;copied=TRUE; }
                    }
                    CloseClipboard();
                }
                if(alpha) GlobalFree(alpha);
                if(hGlobal) GlobalFree(hGlobal);
                if(vector)GlobalFree(vector);
                return copied && vectorCopied;
            }
        case IDM_EDITCUT:
            if (textShown)
            {
                textEditWindow.SendMessage(WM_CUT);
                break;
            }
            /* Copy */
            if(SendMessage(WM_COMMAND, IDM_EDITCOPY, 0))
                SendMessage(WM_COMMAND, IDM_EDITDELETESELECTION, 0);
            break;
        case IDM_EDITPASTE:
            if (textShown)
            {
                textEditWindow.SendMessage(WM_PASTE);
                break;
            }

            if (!OpenClipboard())
                break;

            {
                FigureClipboardData data={};
                if(UnpackFigureClipboard(::GetClipboardData(FigureClipboardFormat()),data))
                { CloseClipboard();toolsModel.ImportFigure(data);break; }
            }
            if (::IsClipboardFormatAvailable(CF_DIBV5))
            {
                HBITMAP hbm=BitmapFromClipboardDIB(::GetClipboardData(CF_DIBV5));
                if(hbm) { InsertSelectionFromHBITMAP(hbm,m_hWnd);CloseClipboard();break; }
            }

            // In many cases, CF_ENHMETAFILE provides a better image than CF_DIB
            if (::IsClipboardFormatAvailable(CF_ENHMETAFILE))
            {
                HENHMETAFILE hEMF = (HENHMETAFILE)::GetClipboardData(CF_ENHMETAFILE);
                if (hEMF)
                {
                    HBITMAP hbm = BitmapFromHEMF(hEMF);
                    // Clipboard handles remain owned by Windows.
                    if (hbm)
                    {
                        InsertSelectionFromHBITMAP(hbm, m_hWnd);
                        CloseClipboard();
                        break;
                    }
                }
            }

            // In many cases, CF_DIB provides a better image than CF_BITMAP
            if (::IsClipboardFormatAvailable(CF_DIB))
            {
                HBITMAP hbm = BitmapFromClipboardDIB(::GetClipboardData(CF_DIB));
                if (hbm)
                {
                    InsertSelectionFromHBITMAP(hbm, m_hWnd);
                    CloseClipboard();
                    break;
                }
            }

            // The last resort
            if (::IsClipboardFormatAvailable(CF_BITMAP))
            {
                HBITMAP hbm = (HBITMAP)::GetClipboardData(CF_BITMAP);
                if (hbm)
                {
                    RasterImage pixels;
                    if(ReadRaster(hbm,pixels,true)) InsertSelectionFromHBITMAP(MakeRasterBitmap(pixels),m_hWnd);
                    CloseClipboard();
                    break;
                }
            }

            // Failed to paste
            {
                CStringW strText, strTitle;
                strText.LoadString(IDS_CANTPASTE);
                strTitle.LoadString(IDS_PROGRAMNAME);
                MessageBox(strText, strTitle, MB_ICONINFORMATION);
            }

            CloseClipboard();
            break;
        case IDM_CROPSELECTION:
        {
            HBITMAP hbmSelection = selectionModel.GetSelectionContents();
            if (hbmSelection)
            {
                imageModel.PushImageForUndo(hbmSelection);
                selectionModel.HideSelection();
                imageModel.NotifyImageChanged();
            }
            break;
        }
        case IDM_EDITDELETESELECTION:
        {
            if (textShown)
            {
                textEditWindow.SendMessage(WM_CLEAR);
                break;
            }
            if(toolsModel.HasEditableFigure()) { canvasWindow.OnEndDraw(TRUE);break; }
            switch (toolsModel.GetActiveTool())
            {
                case TOOL_FREESEL:
                case TOOL_RECTSEL:
                    selectionModel.DeleteSelection();
                    break;

                case TOOL_TEXT:
                    canvasWindow.OnEndDraw(TRUE);
                    break;
                default:
                    break;
            }
            break;
        }
        case IDM_EDITSELECTALL:
        {
            if (textShown)
            {
                textEditWindow.SendMessage(EM_SETSEL, 0, -1);
                break;
            }
            HWND hToolbar = FindWindowEx(toolBoxContainer.m_hWnd, NULL, TOOLBARCLASSNAME, NULL);
            ::SendMessageW(hToolbar, TB_CHECKBUTTON, ID_RECTSEL, MAKELPARAM(TRUE, 0));
            toolsModel.selectAll();
            canvasWindow.Invalidate(TRUE);
            break;
        }
        case IDM_EDITCOPYTO:
        {
            WCHAR szFileName[MAX_LONG_PATH];
            ::LoadStringW(g_hinstExe, IDS_DEFAULTFILENAME, szFileName, _countof(szFileName));
            if (GetSaveFileName(szFileName, _countof(szFileName)))
            {
                HBITMAP hbmSelection = selectionModel.GetSelectionContents();
                if (!hbmSelection)
                {
                    ShowOutOfMemory();
                    break;
                }
                SaveDIBToFile(hbmSelection, szFileName, FALSE);
                DeleteObject(hbmSelection);
            }
            break;
        }
        case IDM_EDITPASTEFROM:
        {
            WCHAR szFileName[MAX_LONG_PATH] = L"";
            if (GetOpenFileName(szFileName, _countof(szFileName)))
            {
                HBITMAP hbmNew = DoLoadImageFile(m_hWnd, szFileName, FALSE);
                if (hbmNew)
                    InsertSelectionFromHBITMAP(hbmNew, m_hWnd);
            }
            break;
        }
        case IDM_COLORSEDITPALETTE:
        {
            COLORREF rgbColor = paletteModel.GetFgColor();
            if (ChooseColor(&rgbColor))
                paletteModel.SetFgColor(rgbColor);
            break;
        }
        case IDM_COLORSMODERNPALETTE:
            paletteModel.SelectPalette(PAL_MODERN);
            break;
        case IDM_COLORSOLDPALETTE:
            paletteModel.SelectPalette(PAL_OLDTYPE);
            break;
        case IDM_COLORSGRAYSCALE:
            paletteModel.SelectPalette(PAL_GRAYSCALE);
            break;
        case IDM_COLORSMONOCHROME:
            paletteModel.SelectPalette(PAL_MONOCHROME);
            break;
        case IDM_EDITINVERTSELECTION:
            if(selectionModel.m_bShow && toolsModel.IsSelection())selectionModel.InvertSelection();
            break;
        case IDM_IMAGEINVERTCONTOUR:
            if(selectionModel.m_bShow && toolsModel.IsSelection())selectionModel.InvertColors();
            break;
        case IDM_IMAGEINVERTCOLORS:
        {
            canvasWindow.OnEndDraw(FALSE);
            imageModel.InvertColors();
            break;
        }
        case IDM_IMAGEDELETEIMAGE:
            imageModel.PushImageForUndo();
            Rect(imageModel.GetDC(), 0, 0, imageModel.GetWidth(), imageModel.GetHeight(), paletteModel.GetBgColor(), paletteModel.GetBgColor(), 0, TRUE);
            imageModel.NotifyImageChanged();
            break;
        case IDM_IMAGEROTATEMIRROR:
            {
                CWaitCursor waitCursor;
                canvasWindow.updateScrollPos();
                switch (mirrorRotateDialog.DoModal(mainWindow.m_hWnd))
                {
                    case 1: /* flip horizontally */
                    {
                        if (selectionModel.m_bShow)
                            selectionModel.FlipHorizontally();
                        else
                            imageModel.FlipHorizontally();
                        break;
                    }
                    case 2: /* flip vertically */
                    {
                        if (selectionModel.m_bShow)
                            selectionModel.FlipVertically();
                        else
                            imageModel.FlipVertically();
                        break;
                    }
                    case 3: /* rotate 90 degrees */
                    {
                        if (selectionModel.m_bShow)
                            selectionModel.RotateNTimes90Degrees(1);
                        else
                            imageModel.RotateNTimes90Degrees(1);
                        break;
                    }
                    case 4: /* rotate 180 degrees */
                    {
                        if (selectionModel.m_bShow)
                            selectionModel.RotateNTimes90Degrees(2);
                        else
                            imageModel.RotateNTimes90Degrees(2);
                        break;
                    }
                    case 5: /* rotate 270 degrees */
                    {
                        if (selectionModel.m_bShow)
                            selectionModel.RotateNTimes90Degrees(3);
                        else
                            imageModel.RotateNTimes90Degrees(3);
                        break;
                    }
                }
            }
            break;
        case IDM_IMAGEATTRIBUTES:
        {
            if (attributesDialog.DoModal(mainWindow.m_hWnd))
            {
                CWaitCursor waitCursor;
                if (attributesDialog.m_bBlackAndWhite && !imageModel.IsBlackAndWhite())
                {
                    CStringW strText(MAKEINTRESOURCEW(IDS_LOSECOLOR));
                    CStringW strTitle(MAKEINTRESOURCEW(IDS_PROGRAMNAME));
                    INT id = MessageBox(strText, strTitle, MB_ICONINFORMATION | MB_YESNOCANCEL);
                    if (id != IDYES)
                        break;

                    imageModel.PushBlackAndWhite();
                }

                if (imageModel.GetWidth() != attributesDialog.newWidth ||
                    imageModel.GetHeight() != attributesDialog.newHeight)
                {
                    imageModel.Crop(attributesDialog.newWidth, attributesDialog.newHeight);
                }
            }
            break;
        }
        case IDM_IMAGESTRETCHSKEW:
        {
            if (stretchSkewDialog.DoModal(mainWindow.m_hWnd))
            {
                CWaitCursor waitCursor;
                if (selectionModel.m_bShow)
                {
                    selectionModel.StretchSkew(stretchSkewDialog.percentage.x, stretchSkewDialog.percentage.y,
                                               stretchSkewDialog.angle.x, stretchSkewDialog.angle.y);
                }
                else
                {
                    imageModel.StretchSkew(stretchSkewDialog.percentage.x, stretchSkewDialog.percentage.y,
                                           stretchSkewDialog.angle.x, stretchSkewDialog.angle.y);
                }
            }
            break;
        }
        case IDM_ALPHA_REPLACE:
            registrySettings.ReplaceDeletedWithAlpha=!registrySettings.ReplaceDeletedWithAlpha;registrySettings.StorePreferences();
            imageModel.NotifyImageChanged();break;
        case IDM_IMAGEDRAWOPAQUE:
            ToggleDocumentAlpha();
            break;
        case IDM_CROP_EDGES:
            ShowCanvasDialog(m_hWnd, true);
            break;
        case IDM_CROP_CORNER_FIRST:
        case IDM_CROP_CORNER_FIRST+1:
        case IDM_CROP_CORNER_FIRST+2:
        case IDM_CROP_CORNER_FIRST+3:
            CropCanvasAtSelectionCorner(LOWORD(wParam)-IDM_CROP_CORNER_FIRST);
            break;
        case IDM_CANVAS_SIZE:
            ShowCanvasDialog(m_hWnd, false);
            break;
        case IDM_IMAGECROP:
        {
            HBITMAP hbmCopy = selectionModel.GetSelectionContents();
            imageModel.PushImageForUndo(hbmCopy);
            selectionModel.HideSelection();
            break;
        }
        case IDM_VIEWTOOLBOX:
            registrySettings.ShowToolBox = !toolBoxContainer.IsWindowVisible();
            toolBoxContainer.ShowWindow(registrySettings.ShowToolBox ? SW_SHOWNOACTIVATE : SW_HIDE);
            alignChildrenToMainWindow();
            break;
        case IDM_VIEWCOLORPALETTE:
            registrySettings.ShowPalette = !paletteWindow.IsWindowVisible();
            paletteWindow.ShowWindow(registrySettings.ShowPalette ? SW_SHOWNOACTIVATE : SW_HIDE);
            alignChildrenToMainWindow();
            break;
        case IDM_VIEWSTATUSBAR:
            registrySettings.ShowStatusBar = !::IsWindowVisible(g_hStatusBar);
            ::ShowWindow(g_hStatusBar, (registrySettings.ShowStatusBar ? SW_SHOWNOACTIVATE : SW_HIDE));
            alignChildrenToMainWindow();
            break;
        case IDM_FORMATICONBAR:
            if (toolsModel.GetActiveTool() == TOOL_TEXT)
            {
                if (!fontsDialog.IsWindow())
                {
                    fontsDialog.Create(mainWindow);
                }
                registrySettings.ShowTextTool = !::IsWindowVisible(fontsDialog);
                fontsDialog.ShowWindow(registrySettings.ShowTextTool ? SW_SHOW : SW_HIDE);
                fontsDialog.SendMessage(DM_REPOSITION, 0, 0);
            }
            break;
        case IDM_VIEWSHOWGRID:
            g_showGrid = !g_showGrid;
            canvasWindow.Invalidate(FALSE);
            break;
        case IDM_VIEWSHOWMINIATURE:
            registrySettings.ShowThumbnail = !::IsWindowVisible(miniature);
            miniature.DoCreate(m_hWnd);
            miniature.ShowWindow(registrySettings.ShowThumbnail ? SW_SHOWNOACTIVATE : SW_HIDE);
            break;

        case IDM_VIEWZOOM125:
            canvasWindow.zoomTo(125);
            break;
        case IDM_VIEWZOOM25:
            canvasWindow.zoomTo(250);
            break;
        case IDM_VIEWZOOM50:
            canvasWindow.zoomTo(500);
            break;
        case IDM_VIEWZOOM100:
            canvasWindow.zoomTo(1000);
            break;
        case IDM_VIEWZOOM200:
            canvasWindow.zoomTo(2000);
            break;
        case IDM_VIEWZOOM400:
            canvasWindow.zoomTo(4000);
            break;
        case IDM_VIEWZOOM800:
            canvasWindow.zoomTo(8000);
            break;

        case IDM_VIEWFULLSCREEN:
            // Create and show the fullscreen window
            fullscreenWindow.DoCreate();
            fullscreenWindow.ShowWindow(SW_SHOWMAXIMIZED);
            break;

        case IDM_CTRL_PLUS:
            toolsModel.SpecialTweak(FALSE);
            break;
        case IDM_CTRL_MINUS:
            toolsModel.SpecialTweak(TRUE);
            break;
    }
    return 0;
}

void CMainWindow::AppendCanvasNavigationMenu(HMENU menu)
{
    ::AppendMenuW(menu,MF_SEPARATOR,0,NULL);
    const LPCWSTR keys[]={L"Home",L"End",L"Page Up",L"Page Down"};
    for(int corner=0;corner<4;++corner)
    {
        CStringW label;label.LoadString(g_hinstExe,IDS_SCROLL_CORNER_FIRST+corner);
        label+=L"\t";label+=keys[corner];
        ::AppendMenuW(menu,MF_STRING,IDM_SCROLL_CORNER_FIRST+corner,label);
    }
}

VOID CMainWindow::TrackPopupMenu(POINT ptScreen, INT iSubMenu)
{
    HMENU hMenu = ::LoadMenuW(g_hinstExe, MAKEINTRESOURCEW(ID_POPUPMENU));
    HMENU hSubMenu = ::GetSubMenu(hMenu, iSubMenu);

    if(iSubMenu==0)
    {
        CStringW crop;crop.LoadString(g_hinstExe,IDS_CV_CROP_SELECTION);
        // Reuse the translated canvas command, with the same selection logic.
        ::DeleteMenu(hSubMenu,IDM_CROPSELECTION,MF_BYCOMMAND);
        ::InsertMenuW(hSubMenu,IDM_EDITDELETESELECTION,MF_BYCOMMAND|MF_STRING,IDM_CROPSELECTION,crop);
        for(int corner=0;corner<4;++corner)
        {
            crop.LoadString(g_hinstExe,IDS_CV_CROP_CORNER_FIRST+corner);RECT area;
            ::InsertMenuW(hSubMenu,IDM_EDITDELETESELECTION,MF_BYCOMMAND|MF_STRING|
                (SelectionCornerCropRect(corner,area)?MF_ENABLED:MF_GRAYED),IDM_CROP_CORNER_FIRST+corner,crop);
        }
    }

    AppendCanvasNavigationMenu(hSubMenu);

    ::SetForegroundWindow(m_hWnd);
    INT_PTR id = ::TrackPopupMenu(hSubMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
                                  ptScreen.x, ptScreen.y, 0, m_hWnd, NULL);
    PostMessage(WM_NULL);
    if (id != 0)
        PostMessage(WM_COMMAND, id);

    ::DestroyMenu(hMenu);
}

// entry point
INT WINAPI
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
    g_hinstExe = hInstance;

    // Initialize common controls library
    INITCOMMONCONTROLSEX iccx;
    iccx.dwSize = sizeof(iccx);
    iccx.dwICC = ICC_STANDARD_CLASSES | ICC_USEREX_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&iccx);

    // Load portable config.ini; no registry preferences are imported.
    registrySettings.Load(nCmdShow);
    paletteModel.SelectPalette(static_cast<PAL_TYPE>(registrySettings.PaletteType));
    ApplyPreferredLanguage();

    // Create the main window
    if (!mainWindow.DoCreate())
    {
        MessageBox(NULL, L"Failed to create main window.", NULL, MB_ICONERROR);
        return 1;
    }

    // Initialize imageModel
    if (__argc < 2 || !DoLoadImageFile(mainWindow, __targv[1], TRUE))
        InitializeImage(NULL, NULL, FALSE);

    // Make the window visible on the screen
    mainWindow.ShowWindow(registrySettings.WindowPlacement.showCmd);

    // Load the access keys

    // The message loop
    MSG msg;
    while (::GetMessage(&msg, NULL, 0, 0))
    {
        if (fontsDialog.IsWindow() && fontsDialog.IsDialogMessage(&msg))
            continue;

        if (!IsEditKeyMessage(msg) && ::TranslateAcceleratorW(mainWindow, mainWindow.Accelerators(), &msg))
            continue;

        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    // Unload the access keys

    // Write back settings to config.ini beside the executable.
    registrySettings.Store();

    if (g_szMailTempFile[0])
        ::DeleteFileW(g_szMailTempFile);

    // Return the value that PostQuitMessage() gave
    return (INT)msg.wParam;
}
