#include "precomp.h"
#include "shellintegration.h"
#include <shlobj.h>

namespace {
bool WriteShellString(HKEY root,LPCWSTR path,LPCWSTR name,LPCWSTR value,bool& changed)
{
    CRegKey key;
    if(key.Create(root,path,REG_NONE,REG_OPTION_NON_VOLATILE,KEY_QUERY_VALUE|KEY_SET_VALUE)!=ERROR_SUCCESS)return false;
    ULONG size=0;
    if(key.QueryStringValue(name,NULL,&size)==ERROR_SUCCESS && size && size<=32768)
    {
        std::vector<WCHAR> current(size);
        if(key.QueryStringValue(name,current.data(),&size)==ERROR_SUCCESS && !lstrcmpW(current.data(),value))return true;
    }
    if(key.SetStringValue(name,value)!=ERROR_SUCCESS)return false;
    changed=true;return true;
}
}

bool RegisterPaintFileTypes(HKEY root,LPCWSTR classesPath,LPCWSTR executable,bool& changed)
{
    changed=false;
    if(!executable || !*executable || PathIsRelativeW(executable) || wcschr(executable,L'"'))return false;
    const LPCWSTR name=L"Classic Paint Plus",progid=L"Nicolay.ClassicPaintPlus.Image";
    CStringW base(classesPath),app,document,command,icon;
    if(!base.IsEmpty() && base[base.GetLength()-1]!=L'\\')base+=L'\\';
    app=base+L"Applications\\"+PathFindFileNameW(executable);document=base+progid;
    command.Format(L"\"%s\" \"%%1\"",executable);icon.Format(L"\"%s\",0",executable);
    bool ok=true;
    auto write=[&](LPCWSTR path,LPCWSTR key,LPCWSTR value) { if(!WriteShellString(root,path,key,value,changed))ok=false; };
    write(app,L"FriendlyAppName",name);
    write(app+L"\\shell\\open\\command",NULL,command);
    write(app+L"\\DefaultIcon",NULL,icon);
    write(document,NULL,name);
    write(document+L"\\Application",L"ApplicationName",name);
    write(document+L"\\Application",L"ApplicationIcon",icon);
    write(document+L"\\shell\\open\\command",NULL,command);
    write(document+L"\\DefaultIcon",NULL,icon);
    const LPCWSTR extensions[]={L".bmp",L".dib",L".jpg",L".jpeg",L".jpe",L".jfif",L".gif",L".png",L".tif",L".tiff",L".webp",L".ico",L".tga",L".targa",L".dds"};
    for(LPCWSTR extension:extensions)
    {
        write(app+L"\\SupportedTypes",extension,L"");
        write(base+extension+L"\\OpenWithProgids",progid,L"");
    }
    return ok;
}

void RegisterPaintWithWindows()
{
#ifndef PAINT_TESTS
    // Register after a successful image open, also from Explorer's Open With.
    // Program preferences remain in config.ini. No extension default or
    // Explorer UserChoice/MRU value is read, replaced or removed here.
    WCHAR executable[32768];DWORD count=GetModuleFileNameW(NULL,executable,_countof(executable));
    if(!count || count>=_countof(executable))return;
    bool changed=false;
    RegisterPaintFileTypes(HKEY_CURRENT_USER,L"Software\\Classes",executable,changed);
    if(changed)SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST|SHCNF_FLUSHNOWAIT,NULL,NULL);
#endif
}
