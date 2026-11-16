#include "shellintegration.h"

static bool polishCanvas=false;
static VOID CALLBACK CheckPolishedDialog(HWND,UINT,UINT_PTR timer,DWORD)
{
    HWND dialog=FindJpegDialogControl(polishCanvas?IDC_NEW_WIDTH:IDD_ATTRIBUTESEDIT1);
    if(!dialog)return;KillTimer(NULL,timer);
    CStringW title=WindowLabel(dialog);
    RequireCanvas(!title.IsEmpty() && title.Find(WCHAR(0xfffd))<0,"dialog caption is valid Unicode");
    if(!polishCanvas && PRIMARYLANGID(GetThreadUILanguage())==LANG_RUSSIAN)
    {
        RequireCanvas(title==L"Атрибуты","Russian Attributes caption is exact");
        CStringW expected;expected.Format(L"%d x %d точек/дюйм",ROUND(g_xDpi),ROUND(g_yDpi));
        RequireCanvas(WindowLabel(GetDlgItem(dialog,IDD_ATTRIBUTESTEXT8))==expected,"Russian inch label displays actual DPI, not dots per centimetre");
    }
    const int fields[]={polishCanvas?IDC_NEW_WIDTH:IDD_ATTRIBUTESEDIT1,polishCanvas?IDC_NEW_HEIGHT:IDD_ATTRIBUTESEDIT2};
    for(int field:fields)
    {
        HWND edit=GetDlgItem(dialog,field);CStringW before=WindowLabel(edit);
        RequireCanvas(!before.IsEmpty() && before.Find(L"px")<0,"dimension field contains only its number");
        SetFocus(edit);SetFocus(GetDlgItem(dialog,IDOK));
        RequireCanvas(WindowLabel(edit)==before,"focus does not reintroduce a unit suffix");
    }
    if(polishCanvas)
    {
        RequireCanvas(WindowLabel(GetDlgItem(dialog,IDC_WIDTH_UNITS))==L"px" && WindowLabel(GetDlgItem(dialog,IDC_HEIGHT_UNITS))==L"px","Canvas size shows px in separate labels");
        SendDlgItemMessageW(dialog,IDC_RELATIVE,BM_CLICK,0,0);
        SetDlgItemTextW(dialog,IDC_NEW_HEIGHT,L"-2");
        RequireCanvas(IsWindowEnabled(GetDlgItem(dialog,IDOK)),"signed relative dimensions still work without suffixes");
    }
    else
    {
        SendDlgItemMessageW(dialog,IDD_ATTRIBUTESRB2,BM_CLICK,0,0);
        RequireCanvas(WindowLabel(GetDlgItem(dialog,IDD_ATTRIBUTESEDIT1)).Find(L"px")<0,"centimetres have no px suffix");
        SendDlgItemMessageW(dialog,IDD_ATTRIBUTESRB3,BM_CLICK,0,0);
        RequireCanvas(WindowLabel(GetDlgItem(dialog,IDD_ATTRIBUTESEDIT1)).Find(L"px")<0,"return to pixels has no px suffix");
    }
    if(PRIMARYLANGID(GetThreadUILanguage())==LANG_RUSSIAN)
    {
        CStringW path;path.Format(L"ui-polish-tests/%s-%d.bmp",polishCanvas?L"canvas-size":L"attributes",registrySettings.DarkTheme);
        CaptureCanvasDialog(dialog,path);
    }
    PostMessageW(dialog,WM_COMMAND,IDCANCEL,0);
}

static void TestSelectionToolDoubleClick()
{
    HWND toolbar=FindWindowExW(toolBoxContainer,NULL,TOOLBARCLASSNAMEW,NULL);
    for(int percent:{100,150,200,250})for(UINT id:{ID_RECTSEL,ID_FREESEL})
    {
        registrySettings.PanelMagnification=percent;ApplyPanelScale();
        toolsModel.OnEndDraw(FALSE);toolsModel.SetActiveTool(id==ID_RECTSEL?TOOL_RECTSEL:TOOL_FREESEL);
        imageModel.PushImageForUndo(CreateColorDIB(100,80,RGB(255,255,255)));imageModel.ClearHistory();
        selectionModel.InsertFromHBITMAP(CreateColorDIB(12,10,RGB(33,77,155)),8,9);
        selectionModel.m_bShow=TRUE;
        CRect button;SendMessageW(toolbar,TB_GETRECT,id,LPARAM(&button));LPARAM point=MAKELPARAM(button.CenterPoint().x,button.CenterPoint().y);
        SendMessageW(toolbar,WM_LBUTTONDOWN,MK_LBUTTON,point);SendMessageW(toolbar,WM_LBUTTONUP,0,point);
        MSG msg;while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        RequireCanvas(selectionModel.m_bShow,"single selection tool click preserves floating pixels");
        SendMessageW(toolbar,WM_LBUTTONDBLCLK,MK_LBUTTON,point);SendMessageW(toolbar,WM_LBUTTONUP,0,point);
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        RequireCanvas(!selectionModel.m_bShow && selectionModel.IsLanded(),"double click commits and deselects with either selection button");
        RequireCanvas(GetPixel(imageModel.GetDC(),10,11)==RGB(33,77,155),"deselection preserves inserted pixels");
        RequireCanvas(toolsModel.GetActiveTool()==(id==ID_RECTSEL?TOOL_RECTSEL:TOOL_FREESEL),"double click keeps requested selection tool active");
        imageModel.Undo();RequireCanvas(GetPixel(imageModel.GetDC(),10,11)==RGB(255,255,255),"committed floating pixels remain undoable");
    }
    puts("PASS double-click deselection: both tools, all scales, single-click preservation, commit and Undo");
}

static CStringW PolishRegistryValue(HKEY root,LPCWSTR path,LPCWSTR name)
{
    CRegKey key;RequireCanvas(key.Open(root,path,KEY_READ)==ERROR_SUCCESS,"test shell registration key exists");
    WCHAR value[2048];ULONG size=_countof(value);
    RequireCanvas(key.QueryStringValue(name,value,&size)==ERROR_SUCCESS,"test shell registration value exists");return value;
}
static void TestOpenWithRegistration()
{
    CStringW path;path.Format(L"Software\\ClassicPaintPlusTests\\OpenWith-%lu",GetCurrentProcessId());
    CRegKey isolated;RequireCanvas(isolated.Create(HKEY_CURRENT_USER,path)==ERROR_SUCCESS,"isolated registry root");
    CRegKey png;RequireCanvas(png.Create(isolated,L"Classes\\.png")==ERROR_SUCCESS,"seed existing default");png.SetStringValue(NULL,L"Other.Image");png.Close();
    bool changed=false;LPCWSTR executable=L"C:\\Рисование с пробелами\\ClassicPaintPlus.exe";
    RequireCanvas(RegisterPaintFileTypes(isolated,L"Classes",executable,changed) && changed,"register alternative handlers");
    RequireCanvas(PolishRegistryValue(isolated,L"Classes\\.png",NULL)==L"Other.Image","existing default association is untouched");
    RequireCanvas(PolishRegistryValue(isolated,L"Classes\\Applications\\ClassicPaintPlus.exe",L"FriendlyAppName")==L"Classic Paint Plus","correct Open With display name");
    RequireCanvas(PolishRegistryValue(isolated,L"Classes\\Nicolay.ClassicPaintPlus.Image\\shell\\open\\command",NULL)==L"\"C:\\Рисование с пробелами\\ClassicPaintPlus.exe\" \"%1\"","Unicode executable and file argument are quoted independently");
    for(LPCWSTR ext:{L".png",L".bmp",L".jpeg",L".webp",L".ico",L".dds",L".tga",L".tiff"})
        RequireCanvas(PolishRegistryValue(isolated,CStringW(L"Classes\\")+ext+L"\\OpenWithProgids",L"Nicolay.ClassicPaintPlus.Image").IsEmpty(),"supported type advertises alternate handler");
    RequireCanvas(RegisterPaintFileTypes(isolated,L"Classes",executable,changed) && !changed,"unchanged registration is idempotent");
    RequireCanvas(!RegisterPaintFileTypes(isolated,L"Classes",L"relative.exe",changed),"relative executable cannot be registered");
    isolated.Close();RequireCanvas(RegDeleteTreeW(HKEY_CURRENT_USER,path)==ERROR_SUCCESS,"remove isolated registration test root");
    puts("PASS Open With registration: display name, supported formats, safe quoted command, preserved defaults and idempotence");
}

static void TestUiPolish(bool registryTest=true)
{
    CreateDirectoryW(L"ui-polish-tests",NULL);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    TestSelectionToolDoubleClick();if(registryTest)TestOpenWithRegistration();TestSmallUiFixes();
    registrySettings.PanelMagnification=100;ApplyPanelScale();
    if(testLanguages.empty())EnumResourceLanguagesW(g_hinstExe,RT_MENU,MAKEINTRESOURCEW(ID_MENU),CollectTestLanguage,0);
    for(WORD language:testLanguages)
    {
        registrySettings.UILanguage=language;ApplyPreferredLanguage();
        HMENU menu=LoadMenuW(g_hinstExe,MAKEINTRESOURCEW(ID_MENU));BuildSettingsMenu(menu);BuildCanvasMenu(menu);
        HMENU drawing=GetSubMenu(menu,3);int last=GetMenuItemCount(drawing)-1;
        RequireCanvas(GetMenuItemID(drawing,last)==IDM_IMAGEATTRIBUTES && (GetMenuState(drawing,last-1,MF_BYPOSITION)&MF_SEPARATOR),"Attributes is last in Drawing after a separator");
        for(int i=0;i<last;++i)if(HMENU child=GetSubMenu(drawing,i))for(int j=0;j<GetMenuItemCount(child);++j)
            RequireCanvas(GetMenuItemID(child,j)!=IDM_IMAGEATTRIBUTES,"Attributes is no longer inside Canvas");
        DestroyMenu(menu);
        RequireCanvas(ExactString(language,IDS_MARKER_SIZE).Find(L"(px)")>=0 && ExactString(language,IDS_TOOLBOX_MARGIN).Find(L"(px)")>=0,"pixel units translated in settings captions");
        RequireCanvas(ExactString(language,IDS_RECENT_FONTS).Find(L'(')>=0,"remembered fonts indicates count");
        RequireCanvas(ExactString(language,IDS_ABOUT_PLUS).Find(L"Classic Paint Plus")>=0,"credits use full application name");
        for(bool canvas:{false,true})
        {
            polishCanvas=canvas;SetTimer(NULL,0,20,CheckPolishedDialog);
            if(canvas)ShowCanvasDialog(mainWindow,false);else attributesDialog.DoModal(mainWindow);
        }
    }
    registrySettings.UILanguage=MAKELANGID(LANG_RUSSIAN,SUBLANG_DEFAULT);ApplyPreferredLanguage();
    for(int dark:{0,1})
    {
        registrySettings.DarkTheme=dark;ApplyTheme();
        for(int percent:{100,150,200,250})
        {
            registrySettings.PanelMagnification=percent;ApplyPanelScale();CStringW path;path.Format(L"ui-polish-tests/palette-%d-%d.bmp",percent,dark);CaptureCanvasDialog(paletteWindow,path);
        }
        for(bool canvas:{false,true}){polishCanvas=canvas;SetTimer(NULL,0,20,CheckPolishedDialog);if(canvas)ShowCanvasDialog(mainWindow,false);else attributesDialog.DoModal(mainWindow);}
    }
    registrySettings.DarkTheme=0;registrySettings.PanelMagnification=100;ApplyTheme();ApplyPanelScale();
    puts("PASS all UI polish checks: 30 localized dialogs/menus, units, credits, one-pixel brush samples and supplied exchange arrow captures");
}
