#include "precomp.h"
#include "applicationinfo.h"
#include "version.h"
#ifdef PAINT_TESTS
static HelpLauncher launchHelp=ShellExecuteW;
void SetHelpLauncherForTests(HelpLauncher launcher) { launchHelp=launcher?launcher:ShellExecuteW; }
#else
static auto launchHelp=ShellExecuteW;
#endif

CStringW RPaintHelpPath()
{
    WCHAR module[32768];DWORD length=GetModuleFileNameW(NULL,module,_countof(module));
    if(!length || length>=_countof(module)) return L"";
    CStringW path(module);int slash=path.ReverseFind(L'\\');path=path.Left(slash+1);
    path+=PRIMARYLANGID(GetThreadUILanguage())==LANG_RUSSIAN?L"ClassicPaintPlus-help.html":L"ClassicPaintPlus-help.en.html";
    return path;
}
bool OpenRPaintHelp(HWND owner)
{
    CStringW path=RPaintHelpPath();
    if(!path.IsEmpty() && GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES &&
       INT_PTR(launchHelp(owner,L"open",path,NULL,NULL,SW_SHOWNORMAL))>32) return true;
    CStringW format,text;format.LoadString(g_hinstExe,IDS_HELP_FILE_ERROR);text.Format(format,LPCWSTR(path));
    MessageBoxW(owner,text,L"Classic Paint Plus",MB_OK|MB_ICONINFORMATION);return false;
}
static void DrawAboutBanner(const DRAWITEMSTRUCT& item)
{
    // Embed the supplied artwork unchanged; rendering follows the dialog's DPI.
    HRSRC resource=FindResourceW(g_hinstExe,MAKEINTRESOURCEW(IDR_ABOUT_BANNER),RT_RCDATA);
    if(!resource) return;DWORD bytes=SizeofResource(g_hinstExe,resource);
    HGLOBAL loaded=LoadResource(g_hinstExe,resource);const void* source=loaded?LockResource(loaded):NULL;
    if(!source || !bytes) return;
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!memory) return;
    void* data=GlobalLock(memory);if(!data) { GlobalFree(memory);return; }
    CopyMemory(data,source,bytes);GlobalUnlock(memory);
    IStream* stream=NULL;if(FAILED(CreateStreamOnHGlobal(memory,TRUE,&stream))) { GlobalFree(memory);return; }
    ULONG_PTR token=0;Gdiplus::GdiplusStartupInput input;
    if(Gdiplus::GdiplusStartup(&token,&input,NULL)==Gdiplus::Ok)
    {
        { Gdiplus::Bitmap image(stream);Gdiplus::Graphics g(item.hDC);
          g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
          const RECT& r=item.rcItem;FillRect(item.hDC,&r,GetSysColorBrush(COLOR_BTNFACE));
          if(image.GetWidth() && image.GetHeight())
          {
              double scale=min(double(r.right-r.left)/image.GetWidth(),double(r.bottom-r.top)/image.GetHeight());
              int width=max(1,int(lround(image.GetWidth()*scale))),height=max(1,int(lround(image.GetHeight()*scale)));
              Gdiplus::ImageAttributes attributes;attributes.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
              g.DrawImage(&image,Gdiplus::Rect(r.left+(r.right-r.left-width)/2,r.top+(r.bottom-r.top-height)/2,width,height),
                          0,0,image.GetWidth(),image.GetHeight(),Gdiplus::UnitPixel,&attributes);
          } }
        Gdiplus::GdiplusShutdown(token);
    }
    stream->Release();
}
static INT_PTR CALLBACK AboutProc(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_INITDIALOG)
    {
        CStringW title,description,credits,license,help;
        title.LoadString(g_hinstExe,IDS_INFOTITLE);SetWindowTextW(window,title);
        description.LoadString(g_hinstExe,IDS_ABOUT_DESCRIPTION);SetDlgItemTextW(window,IDC_ABOUT_DESCRIPTION,description);
        credits.LoadString(g_hinstExe,IDS_ABOUT_CREDITS);CStringW authors;
        authors.Format(credits,L"Benedikt Freisen\r\nKatayama Hirofumi MZ\r\nStanislav Motylkov");
        CStringW plus;plus.LoadString(g_hinstExe,IDS_ABOUT_PLUS);authors+=L"\r\n\r\n"+plus+L"\r\nns253647@gmail.com";SetDlgItemTextW(window,IDC_ABOUT_CREDITS,authors);
        license.LoadString(g_hinstExe,IDS_INFOTEXT);SetDlgItemTextW(window,IDC_ABOUT_LICENSE,license);
        help.LoadString(g_hinstExe,IDS_ABOUT_HELP);SetDlgItemTextW(window,IDC_ABOUT_HELP,help);
        SetDlgItemTextW(window,IDC_ABOUT_BUILD,sizeof(void*)==8?CLASSIC_PAINT_PLUS_VERSION_WTEXT L"  •  x64":CLASSIC_PAINT_PLUS_VERSION_WTEXT L"  •  x86");
        LOGFONTW font={};GetObjectW(reinterpret_cast<HFONT>(SendMessageW(window,WM_GETFONT,0,0)),sizeof(font),&font);
        font.lfHeight=-22;font.lfWeight=FW_BOLD;HFONT heading=CreateFontIndirectW(&font);SetPropW(window,L"AboutHeading",heading);
        SendDlgItemMessageW(window,IDC_ABOUT_NAME,WM_SETFONT,WPARAM(heading),TRUE);
        // Dialog units have different horizontal/vertical scales. Size the band
        // in device pixels using the original 800 x 142 artwork proportions.
        HWND banner=GetDlgItem(window,IDC_ABOUT_BANNER);RECT band,outer;
        GetWindowRect(banner,&band);GetWindowRect(window,&outer);
        int bannerHeight=MulDiv(band.right-band.left,142,800),delta=bannerHeight-(band.bottom-band.top);
        SetWindowPos(banner,NULL,0,0,band.right-band.left,bannerHeight,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        SetWindowPos(window,NULL,0,0,outer.right-outer.left,outer.bottom-outer.top+delta,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        CWindow(window).CenterWindow(GetParent(window));SetFocus(GetDlgItem(window,IDOK));return FALSE;
    }
    if(message==WM_COMMAND)
    {
        if(LOWORD(wp)==IDC_ABOUT_HELP) { OpenRPaintHelp(window);return TRUE; }
        if(LOWORD(wp)==IDOK || LOWORD(wp)==IDCANCEL) { EndDialog(window,IDOK);return TRUE; }
    }
    if(message==WM_DRAWITEM && wp==IDC_ABOUT_BANNER) { DrawAboutBanner(*reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE; }
    if(message==WM_DESTROY) DeleteObject(RemovePropW(window,L"AboutHeading"));
    return FALSE;
}
void ShowRPaintAbout(HWND owner)
{ DialogBoxParamW(g_hinstExe,MAKEINTRESOURCEW(IDD_RPAINT_ABOUT),owner,AboutProc,0); }
