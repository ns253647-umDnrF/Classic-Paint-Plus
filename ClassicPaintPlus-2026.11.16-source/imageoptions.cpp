#include <vector>
#include "precomp.h"
#include "imageformats.h"
#include "imageformats_resource.h"
#include "jpeg_resource.h"

namespace {
CStringW Text(UINT id) { CStringW text;text.LoadString(g_hinstExe,id);return text; }
int ReadOption(LPCWSTR section,LPCWSTR name,int value)
{ return int(GetPrivateProfileIntW(section,name,value,RegistrySettings::ConfigPath())); }
void WriteOption(LPCWSTR section,LPCWSTR name,int value)
{
    WCHAR text[24];swprintf_s(text,L"%d",value);WritePrivateProfileStringW(section,name,text,RegistrySettings::ConfigPath());
}
PngOptions& Png() { static PngOptions options=PngOptions::LoadDefaults();return options; }
WebpOptions& Webp() { static WebpOptions options=WebpOptions::LoadDefaults();return options; }
DdsOptions& Dds() { static DdsOptions options=DdsOptions::LoadDefaults();return options; }
IcoOptions& Ico()
{
    static IcoOptions options=[] { IcoOptions o;o.keepOtherImages=ReadOption(L"ICO",L"KeepOtherImages",1)!=0;return o; }();
    return options;
}
ImageFileInfo document;
SaveFormat saveFormat=SaveFormat::Png;
CStringW pendingPath;
SaveFormat pendingFormat=SaveFormat::Png;
struct Dialog
{
    int kind=0; // 0 PNG, 1 WebP, 2 icon options, 3 icon frame selection, 4 DDS
    PngOptions png;WebpOptions webp;IcoOptions ico;DdsOptions dds;
    const ImageFileInfo* file=NULL;int selected=0;HBITMAP preview=NULL;bool changing=false;
    ~Dialog() { DeleteObject(preview); }
    void Update(HWND window)
    {
        changing=true;
        auto check=[&](int id,bool value) { CheckDlgButton(window,id,value?BST_CHECKED:BST_UNCHECKED); };
        if(kind==0) { check(IDC_PNG_INTERLACE,png.interlaced);check(IDC_PNG_OPTIMIZE,png.optimize); }
        if(kind==1)
        {
            check(IDC_WEBP_LOSSLESS,webp.lossless);SetDlgItemInt(window,IDC_FORMAT_QUALITY,webp.quality,FALSE);
            SendDlgItemMessage(window,IDC_FORMAT_SLIDER,TBM_SETPOS,TRUE,webp.quality);
            EnableWindow(GetDlgItem(window,IDC_FORMAT_QUALITY),!webp.lossless);EnableWindow(GetDlgItem(window,IDC_FORMAT_SLIDER),!webp.lossless);
        }
        if(kind==2) check(IDC_ICO_KEEP,ico.keepOtherImages);
        if(kind==4) SendDlgItemMessage(window,IDC_DDS_COMPRESSION,CB_SETCURSEL,dds.compression,0);
        if(kind==3)
        {
            DeleteObject(preview);preview=NULL;RasterImage image;
            if(selected>=0 && selected<int(file->icons.size()) && DecodeIconFrame(file->icons[selected],image)) preview=MakeRasterBitmap(image);
            InvalidateRect(GetDlgItem(window,IDC_ICO_PREVIEW),NULL,TRUE);
        }
        changing=false;
    }
};
INT_PTR CALLBACK FormatDialogProc(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    auto* state=reinterpret_cast<Dialog*>(GetWindowLongPtr(window,DWLP_USER));
    if(message==WM_INITDIALOG)
    {
        state=reinterpret_cast<Dialog*>(lp);SetWindowLongPtr(window,DWLP_USER,lp);
        SetWindowTextW(window,Text(state->kind==0?IDS_PNG_TITLE:state->kind==1?IDS_WEBP_TITLE:state->kind==2?IDS_ICO_TITLE:state->kind==4?IDS_DDS_TITLE:IDS_ICO_SELECT));
        SetDlgItemTextW(window,IDCANCEL,Text(IDS_JPEG_CANCEL));SetDlgItemTextW(window,IDC_FORMAT_RESET,Text(IDS_JPEG_RESET));
        SetDlgItemTextW(window,IDC_FORMAT_DEFAULTS,Text(IDS_JPEG_DEFAULTS));
        if(state->kind==4)
        {
            SetDlgItemTextW(window,IDC_FORMAT_GROUP,Text(IDS_DDS_COMPRESSION));
            SetDlgItemTextW(window,IDC_FORMAT_DESCRIPTION,Text(IDS_DDS_DESCRIPTION));
            CStringW uncompressed=Text(IDS_DDS_UNCOMPRESSED);
            for(LPCWSTR label:{LPCWSTR(uncompressed),L"DXT1",L"DXT3",L"DXT5"})
                SendDlgItemMessageW(window,IDC_DDS_COMPRESSION,CB_ADDSTRING,0,LPARAM(label));
        }
        if(state->kind==0)
        {
            SetDlgItemTextW(window,IDC_FORMAT_GROUP,Text(IDS_JPEG_ENCODING));
            SetDlgItemTextW(window,IDC_PNG_INTERLACE,Text(IDS_PNG_INTERLACE));SetDlgItemTextW(window,IDC_PNG_OPTIMIZE,Text(IDS_PNG_OPTIMIZE));
            SetDlgItemTextW(window,IDC_FORMAT_DESCRIPTION,Text(IDS_PNG_DESCRIPTION));
        }
        if(state->kind==1)
        {
            SetDlgItemTextW(window,IDC_WEBP_LOSSLESS,Text(IDS_WEBP_LOSSLESS));SetDlgItemTextW(window,IDC_FORMAT_GROUP,Text(IDS_JPEG_QUALITY));
            SendDlgItemMessage(window,IDC_FORMAT_QUALITY,EM_SETLIMITTEXT,3,0);
            SendDlgItemMessage(window,IDC_FORMAT_SLIDER,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
        }
        if(state->kind==2)
        {
            SetDlgItemTextW(window,IDC_FORMAT_LABEL,Text(IDS_ICO_SIZE));SetDlgItemTextW(window,IDC_ICO_KEEP,Text(IDS_ICO_KEEP));
            SetDlgItemTextW(window,IDC_FORMAT_DESCRIPTION,Text(IDS_ICO_DESCRIPTION));
            HWND combo=GetDlgItem(window,IDC_ICO_SIZE);
            for(int size:{0,16,24,32,48,64,128,256})
            {
                CStringW value;if(size) value.Format(L"%d \u00d7 %d",size,size);else value=Text(IDS_ICO_CURRENT);
                int index=int(SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(LPCWSTR(value))));
                SendMessage(combo,CB_SETITEMDATA,index,size);if(state->ico.size==size) SendMessage(combo,CB_SETCURSEL,index,0);
            }
            EnableWindow(GetDlgItem(window,IDC_ICO_KEEP),ImageDocumentInfo().icons.size()>1);
        }
        if(state->kind==3)
        {
            SetDlgItemTextW(window,IDC_FORMAT_LABEL,Text(IDS_ICO_IMAGE));
            for(size_t i=0;i<state->file->icons.size();++i)
            {
                const auto& frame=state->file->icons[i];CStringW name;
                name.Format(L"%u:  %d \u00d7 %d  (%d bpp)",unsigned(i+1),frame.width,frame.height,frame.depth);
                SendDlgItemMessage(window,IDC_ICO_SIZE,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(LPCWSTR(name)));
            }
            SendDlgItemMessage(window,IDC_ICO_SIZE,CB_SETCURSEL,state->selected,0);
        }
        state->Update(window);CWindow(window).CenterWindow(GetParent(window));return TRUE;
    }
    if(!state) return FALSE;
    if(message==WM_DRAWITEM && int(wp)==IDC_ICO_PREVIEW)
    {
        auto& item=*reinterpret_cast<DRAWITEMSTRUCT*>(lp);FillRect(item.hDC,&item.rcItem,GetSysColorBrush(COLOR_WINDOW));
        if(state->preview)
        {
            int w=GetDIBWidth(state->preview),h=GetDIBHeight(state->preview);
            double scale=min(double(item.rcItem.right-item.rcItem.left-4)/w,double(item.rcItem.bottom-item.rcItem.top-4)/h);
            int pw=max(1,int(w*scale)),ph=max(1,int(h*scale));RECT r={item.rcItem.left+(item.rcItem.right-item.rcItem.left-pw)/2,
                item.rcItem.top+(item.rcItem.bottom-item.rcItem.top-ph)/2,0,0};r.right=r.left+pw;r.bottom=r.top+ph;
            DrawRasterPreview(item.hDC,r,state->preview);
        }
        return TRUE;
    }
    if(message==WM_HSCROLL && state->kind==1 && HWND(lp)==GetDlgItem(window,IDC_FORMAT_SLIDER))
    { state->webp.quality=int(SendMessage(HWND(lp),TBM_GETPOS,0,0));state->Update(window);return TRUE; }
    if(message!=WM_COMMAND || state->changing) return FALSE;
    int id=LOWORD(wp);auto checked=[&](int control) { return IsDlgButtonChecked(window,control)==BST_CHECKED; };
    if(id==IDCANCEL) { EndDialog(window,IDCANCEL);return TRUE; }
    if(id==IDC_FORMAT_RESET)
    { state->png=PngOptions{};state->webp=WebpOptions{};state->dds=DdsOptions{};state->Update(window);return TRUE; }
    if(id==IDC_DDS_COMPRESSION && HIWORD(wp)==CBN_SELCHANGE)
    { int choice=int(SendDlgItemMessage(window,id,CB_GETCURSEL,0,0));if(choice>=0 && choice<4) state->dds.compression=choice;return TRUE; }
    if(id==IDC_FORMAT_QUALITY && HIWORD(wp)==EN_CHANGE)
    {
        BOOL valid=FALSE;int value=GetDlgItemInt(window,id,&valid,FALSE);
        if(valid && value>=0 && value<=100) { state->webp.quality=value;SendDlgItemMessage(window,IDC_FORMAT_SLIDER,TBM_SETPOS,TRUE,value); }return TRUE;
    }
    if(id==IDC_ICO_SIZE && HIWORD(wp)==CBN_SELCHANGE)
    {
        int selected=int(SendDlgItemMessage(window,id,CB_GETCURSEL,0,0));
        if(state->kind==2) state->ico.size=int(SendDlgItemMessage(window,id,CB_GETITEMDATA,selected,0));
        else { state->selected=selected;state->Update(window); }return TRUE;
    }
    if(id==IDOK)
    {
        if(state->kind==1 && !state->webp.lossless)
        {
            BOOL valid=FALSE;int value=GetDlgItemInt(window,IDC_FORMAT_QUALITY,&valid,FALSE);
            if(!valid || value<0 || value>100)
            { MessageBoxW(window,Text(IDS_JPEG_RANGE),Text(IDS_WEBP_TITLE),MB_OK|MB_ICONINFORMATION);SetFocus(GetDlgItem(window,IDC_FORMAT_QUALITY));return TRUE; }
        }
        if(checked(IDC_FORMAT_DEFAULTS))
        { if(state->kind==0) state->png.SaveDefaults();else if(state->kind==1) state->webp.SaveDefaults();else if(state->kind==4) state->dds.SaveDefaults(); }
        EndDialog(window,IDOK);return TRUE;
    }
    if(HIWORD(wp)!=BN_CLICKED) return FALSE;
    if(id==IDC_PNG_INTERLACE) state->png.interlaced=checked(id);
    else if(id==IDC_PNG_OPTIMIZE) state->png.optimize=checked(id);
    else if(id==IDC_WEBP_LOSSLESS) state->webp.lossless=checked(id);
    else if(id==IDC_ICO_KEEP) state->ico.keepOtherImages=checked(id);
    else return FALSE;
    state->Update(window);return TRUE;
}
}
PngOptions PngOptions::LoadDefaults()
{ PngOptions o;o.interlaced=ReadOption(L"PNG",L"Interlaced",0)!=0;o.optimize=ReadOption(L"PNG",L"Optimize",0)!=0;return o; }
void PngOptions::SaveDefaults() const
{ WriteOption(L"PNG",L"Interlaced",interlaced);WriteOption(L"PNG",L"Optimize",optimize); }
WebpOptions WebpOptions::LoadDefaults()
{ WebpOptions o;o.lossless=ReadOption(L"WebP",L"Lossless",1)!=0;o.quality=max(0,min(100,ReadOption(L"WebP",L"Quality",90)));return o; }
void WebpOptions::SaveDefaults() const
{ WriteOption(L"WebP",L"Lossless",lossless);WriteOption(L"WebP",L"Quality",quality); }
DdsOptions DdsOptions::LoadDefaults()
{ DdsOptions o;int value=ReadOption(L"DDS",L"Compression",0);o.compression=value>=0 && value<=3?value:0;return o; }
void DdsOptions::SaveDefaults() const
{ WriteOption(L"DDS",L"Compression",compression); }
const PngOptions& CurrentPngOptions() { return Png(); }
const WebpOptions& CurrentWebpOptions() { return Webp(); }
const IcoOptions& CurrentIcoOptions() { return Ico(); }
const DdsOptions& CurrentDdsOptions() { return Dds(); }
void SetCurrentPngOptions(const PngOptions& o) { Png()=o; }
void SetCurrentWebpOptions(const WebpOptions& o) { Webp()=o; }
void SetCurrentIcoOptions(const IcoOptions& o) { Ico()=o;WriteOption(L"ICO",L"KeepOtherImages",o.keepOtherImages); }
void SetCurrentDdsOptions(const DdsOptions& o) { Dds()=o; }
ImageFileInfo& ImageDocumentInfo() { return document; }
void SetImageDocumentInfo(const ImageFileInfo& info)
{
    document=info;saveFormat=info.format;pendingPath.Empty();Ico().size=0;
    Dds()=DdsOptions::LoadDefaults();if(info.format==SaveFormat::Dds) Dds().compression=info.ddsCompression;
}
SaveFormat CurrentSaveFormat() { return saveFormat; }
void SetCurrentSaveFormat(SaveFormat format) { saveFormat=format; }
void SetPendingImageFormat(LPCWSTR path,SaveFormat format) { pendingPath=path;pendingFormat=format; }
SaveFormat ResolveSaveFormat(LPCWSTR path,bool mainFile)
{
    SaveFormat format=FormatFromPath(path);
    if(!pendingPath.IsEmpty() && !pendingPath.CompareNoCase(path))
    {
        if(format==SaveFormat::Bmp24 && int(pendingFormat)<=int(SaveFormat::Bmp8)) format=pendingFormat;
        pendingPath.Empty();
    }
    else if(mainFile && format==SaveFormat::Bmp24 && int(document.format)<=int(SaveFormat::Bmp8)) format=document.format;
    return format;
}
SaveFormat SaveFormatFromFilterIndex(UINT index)
{
    static const SaveFormat bmp[]={SaveFormat::Bmp1,SaveFormat::Bmp4,SaveFormat::Bmp8,SaveFormat::Bmp24};
    if(index>=1 && index<=4) return bmp[index-1];
    return index>=5 && index<=12?SaveFormat(index-1):SaveFormat::Png;
}
UINT SaveFormatFilterIndex(SaveFormat format)
{
    switch(format)
    { case SaveFormat::Bmp1:return 1;case SaveFormat::Bmp4:return 2;case SaveFormat::Bmp8:return 3;case SaveFormat::Bmp24:return 4;default:return UINT(format)+1; }
}
CStringW BuildSaveImageFilters()
{
    CStringW result;
    for(UINT id:{IDS_BMP_24+1,IDS_BMP_24+2,IDS_BMP_24+3,IDS_BMP_24}) { result+=Text(id);result+=L" (*.bmp;*.dib)|*.bmp;*.dib|"; }
    result+=L"JPEG (*.jpg;*.jpeg;*.jpe;*.jfif)|*.jpg;*.jpeg;*.jpe;*.jfif|PNG (*.png)|*.png|GIF (*.gif)|*.gif|TIFF (*.tif;*.tiff)|*.tif;*.tiff|WebP (*.webp)|*.webp|";
    result+=Text(IDS_ICON_FILE);result+=L" (*.ico)|*.ico|TGA (*.tga;*.targa)|*.tga;*.targa|DDS (*.dds)|*.dds||";result.Replace(L'|',UNICODE_NULL);return result;
}
CStringW BuildOpenImageFilters()
{
    CStringW all;all.LoadString(g_hinstExe,IDS_ALLPICTUREFILES);
    CStringW result=all+L"|*.bmp;*.dib;*.jpg;*.jpeg;*.jpe;*.jfif;*.png;*.gif;*.tif;*.tiff;*.webp;*.ico;*.tga;*.targa;*.dds|";
    result+=L"PNG (*.png)|*.png|JPEG (*.jpg;*.jpeg;*.jpe;*.jfif)|*.jpg;*.jpeg;*.jpe;*.jfif|BMP (*.bmp;*.dib)|*.bmp;*.dib|GIF (*.gif)|*.gif|TIFF (*.tif;*.tiff)|*.tif;*.tiff|WebP (*.webp)|*.webp|";
    result+=Text(IDS_ICON_FILE);result+=L" (*.ico)|*.ico|TGA (*.tga;*.targa)|*.tga;*.targa|DDS (*.dds)|*.dds||";result.Replace(L'|',UNICODE_NULL);return result;
}
bool ShowPngOptions(HWND owner,PngOptions& options)
{
    Dialog state;state.png=options;
    if(DialogBoxParamW(g_hinstExe,MAKEINTRESOURCEW(IDD_PNG_OPTIONS),owner,FormatDialogProc,reinterpret_cast<LPARAM>(&state))!=IDOK) return false;
    options=state.png;return true;
}
bool ShowDdsOptions(HWND owner,DdsOptions& options)
{
    Dialog state;state.kind=4;state.dds=options;
    if(DialogBoxParamW(g_hinstExe,MAKEINTRESOURCEW(IDD_DDS_OPTIONS),owner,FormatDialogProc,reinterpret_cast<LPARAM>(&state))!=IDOK) return false;
    options=state.dds;return true;
}
bool ShowWebpOptions(HWND owner,WebpOptions& options)
{
    Dialog state;state.kind=1;state.webp=options;
    if(DialogBoxParamW(g_hinstExe,MAKEINTRESOURCEW(IDD_WEBP_OPTIONS),owner,FormatDialogProc,reinterpret_cast<LPARAM>(&state))!=IDOK) return false;
    options=state.webp;return true;
}
bool ShowIcoOptions(HWND owner,IcoOptions& options)
{
    Dialog state;state.kind=2;state.ico=options;
    if(!state.ico.size && (imageModel.GetWidth()>256 || imageModel.GetHeight()>256)) state.ico.size=256;
    if(DialogBoxParamW(g_hinstExe,MAKEINTRESOURCEW(IDD_ICO_OPTIONS),owner,FormatDialogProc,reinterpret_cast<LPARAM>(&state))!=IDOK) return false;
    options=state.ico;return true;
}
int SelectIconFrame(HWND owner,const ImageFileInfo& info)
{
    if(info.icons.size()==1) return 0;
    Dialog state;state.kind=3;state.file=&info;state.selected=info.selectedIcon;
    if(DialogBoxParamW(g_hinstExe,MAKEINTRESOURCEW(IDD_ICO_SELECT),owner,FormatDialogProc,reinterpret_cast<LPARAM>(&state))!=IDOK) return -1;
    return state.selected;
}
