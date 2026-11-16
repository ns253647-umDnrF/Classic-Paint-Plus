// Save As extension and JPEG options. LGPL-2.0-or-later.
#include <vector>
#include "precomp.h"
#include "jpeg.h"
#include "jpeg_resource.h"

static CStringW JpegText(UINT id) { CStringW text;text.LoadString(g_hinstExe,id);return text; }
static void LayoutJpegSaveControls(HWND hook)
{
    HWND button=GetDlgItem(hook,IDC_JPEG_OPTIONS),cancel=GetDlgItem(GetParent(hook),IDCANCEL);
    if(!button || !cancel) return;
    RECT client,parent;GetClientRect(hook,&client);GetClientRect(GetParent(hook),&parent);
    POINT origin={0,0};MapWindowPoints(hook,GetParent(hook),&origin,1);
    int width=parent.right-origin.x;
    if(width>0 && client.right!=width)
        SetWindowPos(hook,NULL,0,0,width,client.bottom,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    RECT target,current;GetWindowRect(cancel,&target);GetWindowRect(button,&current);
    MapWindowPoints(NULL,hook,reinterpret_cast<POINT*>(&target),2);
    MapWindowPoints(NULL,hook,reinterpret_cast<POINT*>(&current),2);
    SetWindowPos(button,NULL,target.left,current.top,target.right-target.left,target.bottom-target.top,SWP_NOZORDER|SWP_NOACTIVATE);
    for(int id:{IDC_JPEG_PRESERVE,IDC_PAINT_DATA}) { HWND label=GetDlgItem(hook,id);RECT r;GetWindowRect(label,&r);
    MapWindowPoints(NULL,hook,reinterpret_cast<POINT*>(&r),2);
    SetWindowPos(label,NULL,0,0,max(1,target.left-r.left-12),r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE); }
}
JpegOptions JpegOptions::LoadDefaults()
{
    JpegOptions o;CStringW path=RegistrySettings::ConfigPath();
    auto read=[&](LPCWSTR name,int def) { return int(GetPrivateProfileIntW(L"JPEG",name,def,path)); };
    o.quality=max(0,min(100,read(L"Quality",90)));o.progressive=read(L"Progressive",0)!=0;
    o.optimize=read(L"OptimizeHuffman",1)!=0;o.horizontal=read(L"Horizontal2to1",1)!=0;
    o.vertical=read(L"Vertical2to1",0)!=0;o.thumbnails=max(0,min(2,read(L"Thumbnails",0)));
    o.dcf=read(L"DCFThumbnails",0)!=0;o.preserveMetadata=read(L"PreserveMetadata",1)!=0;
    return o;
}
void JpegOptions::SaveDefaults() const
{
    CStringW path=RegistrySettings::ConfigPath();
    auto write=[&](LPCWSTR name,int value)
    { WCHAR text[20];swprintf_s(text,L"%d",value);WritePrivateProfileStringW(L"JPEG",name,text,path); };
    write(L"Quality",quality);write(L"Progressive",progressive);write(L"OptimizeHuffman",optimize);
    write(L"Horizontal2to1",horizontal);write(L"Vertical2to1",vertical);write(L"Thumbnails",thumbnails);
    write(L"DCFThumbnails",dcf);write(L"PreserveMetadata",preserveMetadata);
}
static JpegOptions& CurrentOptions() { static JpegOptions value=JpegOptions::LoadDefaults();return value; }
const JpegOptions& CurrentJpegOptions() { return CurrentOptions(); }
void SetCurrentJpegOptions(const JpegOptions& o) { CurrentOptions()=o; }

namespace {
struct OptionsDialog
{
    JpegOptions options;
    bool changing=false;
    void Update(HWND window)
    {
        changing=true;
        SetDlgItemInt(window,IDC_JPEG_QUALITY,options.quality,FALSE);
        SendDlgItemMessage(window,IDC_JPEG_SLIDER,TBM_SETPOS,TRUE,options.quality);
        auto check=[&](int id,bool value) { CheckDlgButton(window,id,value?BST_CHECKED:BST_UNCHECKED); };
        check(IDC_JPEG_PROGRESSIVE,options.progressive);check(IDC_JPEG_OPTIMIZE,options.optimize || options.progressive);
        // Progressive JPEG uses optimized tables in libjpeg. Show that fact,
        // retaining the user's baseline preference when progressive is turned off.
        EnableWindow(GetDlgItem(window,IDC_JPEG_OPTIMIZE),!options.progressive);
        check(IDC_JPEG_HORIZONTAL,options.horizontal);check(IDC_JPEG_VERTICAL,options.vertical);
        CheckRadioButton(window,IDC_JPEG_EXISTING,IDC_JPEG_NEVER,IDC_JPEG_EXISTING+options.thumbnails);
        check(IDC_JPEG_DCF,options.dcf);EnableWindow(GetDlgItem(window,IDC_JPEG_DCF),options.thumbnails!=2);
        changing=false;
    }
};
INT_PTR CALLBACK OptionsProc(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    auto* state=reinterpret_cast<OptionsDialog*>(GetWindowLongPtr(window,DWLP_USER));
    if(message==WM_INITDIALOG)
    {
        state=reinterpret_cast<OptionsDialog*>(lp);SetWindowLongPtr(window,DWLP_USER,lp);
        SetWindowTextW(window,JpegText(IDS_JPEG_TITLE));
        const int ids[]={IDC_JPEG_QUALITY_GROUP,IDC_JPEG_SMALL,IDC_JPEG_BEST,IDC_JPEG_ENCODING,
            IDC_JPEG_PROGRESSIVE,IDC_JPEG_OPTIMIZE,IDC_JPEG_SAMPLING,IDC_JPEG_HORIZONTAL,
            IDC_JPEG_VERTICAL,IDC_JPEG_THUMBS,IDC_JPEG_EXISTING,IDC_JPEG_ALWAYS,IDC_JPEG_NEVER,
            IDC_JPEG_DCF,IDC_JPEG_DEFAULTS,IDC_JPEG_RESET,IDCANCEL};
        for(unsigned i=0;i<_countof(ids);++i) SetDlgItemTextW(window,ids[i],JpegText(IDS_JPEG_QUALITY+i));
        SendDlgItemMessage(window,IDC_JPEG_QUALITY,EM_SETLIMITTEXT,3,0);
        SendDlgItemMessage(window,IDC_JPEG_SLIDER,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
        SendDlgItemMessage(window,IDC_JPEG_SLIDER,TBM_SETPAGESIZE,0,10);
        SendDlgItemMessage(window,IDC_JPEG_SPIN,UDM_SETRANGE32,0,100);
        state->Update(window);CWindow(window).CenterWindow(GetParent(window));return TRUE;
    }
    if(!state) return FALSE;
    if(message==WM_HSCROLL && HWND(lp)==GetDlgItem(window,IDC_JPEG_SLIDER))
    { state->options.quality=int(SendMessage(HWND(lp),TBM_GETPOS,0,0));state->Update(window);return TRUE; }
    if(message!=WM_COMMAND || state->changing) return FALSE;
    int id=LOWORD(wp);auto checked=[&](int control) { return IsDlgButtonChecked(window,control)==BST_CHECKED; };
    if(id==IDCANCEL) { EndDialog(window,IDCANCEL);return TRUE; }
    if(id==IDC_JPEG_RESET)
    {
        bool preserve=state->options.preserveMetadata;state->options=JpegOptions();
        state->options.preserveMetadata=preserve;state->Update(window);return TRUE;
    }
    if(id==IDC_JPEG_QUALITY && HIWORD(wp)==EN_CHANGE)
    {
        BOOL translated=FALSE;UINT value=GetDlgItemInt(window,id,&translated,FALSE);
        if(translated && value<=100)
        { state->options.quality=value;SendDlgItemMessage(window,IDC_JPEG_SLIDER,TBM_SETPOS,TRUE,value); }
        return TRUE;
    }
    if(id==IDOK)
    {
        BOOL translated=FALSE;UINT value=GetDlgItemInt(window,IDC_JPEG_QUALITY,&translated,FALSE);
        if(!translated || value>100)
        {
            MessageBoxW(window,JpegText(IDS_JPEG_RANGE),JpegText(IDS_JPEG_TITLE),MB_OK|MB_ICONINFORMATION);
            SetFocus(GetDlgItem(window,IDC_JPEG_QUALITY));return TRUE;
        }
        state->options.quality=value;
        if(checked(IDC_JPEG_DEFAULTS)) state->options.SaveDefaults();
        EndDialog(window,IDOK);return TRUE;
    }
    if(HIWORD(wp)!=BN_CLICKED) return FALSE;
    if(id==IDC_JPEG_PROGRESSIVE) state->options.progressive=checked(id);
    else if(id==IDC_JPEG_OPTIMIZE) state->options.optimize=checked(id);
    else if(id==IDC_JPEG_HORIZONTAL) state->options.horizontal=checked(id);
    else if(id==IDC_JPEG_VERTICAL) state->options.vertical=checked(id);
    else if(id>=IDC_JPEG_EXISTING && id<=IDC_JPEG_NEVER) state->options.thumbnails=id-IDC_JPEG_EXISTING;
    else if(id==IDC_JPEG_DCF) state->options.dcf=checked(id);
    else return FALSE;
    state->Update(window);return TRUE;
}
}
bool ShowJpegOptions(HWND owner,JpegOptions& options)
{
    OptionsDialog state;state.options=options;
    if(DialogBoxParamW(g_hinstExe,MAKEINTRESOURCEW(IDD_JPEG_OPTIONS),owner,OptionsProc,reinterpret_cast<LPARAM>(&state))!=IDOK) return false;
    options=state.options;return true;
}
bool JpegFilterSelected(const OPENFILENAMEW& ofn)
{
    LPCWSTR p=ofn.lpstrFilter;
    for(DWORD index=1;p && *p;++index)
    {
        p+=wcslen(p)+1;
        if(index==ofn.nFilterIndex)
        { CStringW pattern(p);pattern.MakeLower();return pattern.Find(L"*.jpg")>=0 || pattern.Find(L"*.jpeg")>=0 || pattern.Find(L"*.jpe")>=0; }
        p+=wcslen(p)+1;
    }
    return false;
}
UINT_PTR JpegSaveHook(HWND hwnd,UINT message,WPARAM wp,LPARAM lp)
{
    auto* ofn=reinterpret_cast<OPENFILENAMEW*>(GetWindowLongPtr(hwnd,DWLP_USER));
    if(message==WM_INITDIALOG)
    {
        ofn=reinterpret_cast<OPENFILENAMEW*>(lp);SetWindowLongPtr(hwnd,DWLP_USER,lp);
        SetDlgItemTextW(hwnd,IDC_JPEG_OPTIONS,JpegText(IDS_JPEG_OPTIONS));
        SetDlgItemTextW(hwnd,IDC_JPEG_PRESERVE,JpegText(IDS_JPEG_PRESERVE));
        SetDlgItemTextW(hwnd,IDC_PAINT_DATA,JpegText(IDS_PAINT_DATA));
        CheckDlgButton(hwnd,IDC_JPEG_PRESERVE,reinterpret_cast<SaveImageOptions*>(ofn->lCustData)->jpeg.preserveMetadata?BST_CHECKED:BST_UNCHECKED);
    }
    if(!ofn) return 0;
    auto& options=*reinterpret_cast<SaveImageOptions*>(ofn->lCustData);
    if(message==WM_SIZE) LayoutJpegSaveControls(hwnd);
    if(message==WM_NOTIFY)
    {
        auto* notice=reinterpret_cast<OFNOTIFYW*>(lp);
        if(notice->hdr.code==CDN_INITDONE || notice->hdr.code==CDN_TYPECHANGE)
        {
            LayoutJpegSaveControls(hwnd);
            bool jpeg=JpegFilterSelected(*notice->lpOFN);
            SaveFormat format=SaveFormatFromFilterIndex(notice->lpOFN->nFilterIndex);
            EnableWindow(GetDlgItem(hwnd,IDC_JPEG_OPTIONS),jpeg || format==SaveFormat::Png || format==SaveFormat::WebP || format==SaveFormat::Ico || format==SaveFormat::Dds);
            ShowWindow(GetDlgItem(hwnd,IDC_JPEG_PRESERVE),jpeg?SW_SHOWNA:SW_HIDE);
            bool supports=SupportsPaintData(format),eligible=CanSavePaintData();
            ShowWindow(GetDlgItem(hwnd,IDC_PAINT_DATA),supports?SW_SHOWNA:SW_HIDE);
            EnableWindow(GetDlgItem(hwnd,IDC_PAINT_DATA),eligible);
            CheckDlgButton(hwnd,IDC_PAINT_DATA,eligible && options.preservePaintData?BST_CHECKED:BST_UNCHECKED);
        }
    }
    if(message==WM_COMMAND)
    {
        if(LOWORD(wp)==IDC_PAINT_DATA) options.preservePaintData=IsDlgButtonChecked(hwnd,IDC_PAINT_DATA)==BST_CHECKED;
        if(LOWORD(wp)==IDC_JPEG_PRESERVE) options.jpeg.preserveMetadata=IsDlgButtonChecked(hwnd,IDC_JPEG_PRESERVE)==BST_CHECKED;
        if(LOWORD(wp)==IDC_JPEG_OPTIONS)
        {
            SaveFormat format=SaveFormatFromFilterIndex(ofn->nFilterIndex);
            if(format==SaveFormat::Jpeg) ShowJpegOptions(GetParent(hwnd),options.jpeg);
            else if(format==SaveFormat::Png) ShowPngOptions(GetParent(hwnd),options.png);
            else if(format==SaveFormat::WebP) ShowWebpOptions(GetParent(hwnd),options.webp);
            else if(format==SaveFormat::Ico) ShowIcoOptions(GetParent(hwnd),options.ico);
            else if(format==SaveFormat::Dds) ShowDdsOptions(GetParent(hwnd),options.dds);
            return 1;
        }
    }
    return 0;
}
