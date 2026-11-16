#include <cerrno>
#include <climits>
#include "precomp.h"
CStringW PixelText(int value) { CStringW text;text.Format(L"%d px",value);return text; }
bool ParsePixelValue(LPCWSTR input,int& value,int minimum,int maximum)
{
    CStringW text(input);text.Trim();
    if(text.GetLength()>=2 && !text.Right(2).CompareNoCase(L"px")) { text.Delete(text.GetLength()-2,2);text.TrimRight(); }
    WCHAR* end=nullptr;errno=0;long number=wcstol(text,&end,10);
    if(text.IsEmpty() || end==LPCWSTR(text) || *end || errno==ERANGE || number<minimum || number>maximum) return false;
    value=int(number);return true;
}
static CStringW NumericText(LPCWSTR input,LPCWSTR suffix,bool focused)
{
    CStringW text(input);text.Trim();int length=lstrlenW(suffix);
    if(length && text.GetLength()>=length && !text.Right(length).CompareNoCase(suffix)) { text.Delete(text.GetLength()-length,length);text.TrimRight(); }
    CStringW check(text);check.Replace(L',',L'.');WCHAR* end=nullptr;wcstod(check,&end);
    if(!focused && !text.IsEmpty() && end!=LPCWSTR(check) && !*end) { if(!lstrcmpW(suffix,L"px")) text+=L" ";text+=suffix; }
    return text;
}
static LRESULT CALLBACK PixelEditProc(HWND edit,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR context)
{
    auto suffix=reinterpret_cast<LPCWSTR>(context);
    if(msg==WM_SETTEXT && lp)
    {
        CStringW text=NumericText(LPCWSTR(lp),suffix,GetFocus()==edit || GetPropW(edit,L"CPP.NumericFocused"));
        return DefSubclassProc(edit,msg,wp,LPARAM(LPCWSTR(text)));
    }
    if(msg==WM_SETFOCUS || msg==WM_KILLFOCUS)
    {
        if(msg==WM_SETFOCUS) SetPropW(edit,L"CPP.NumericFocused",HANDLE(1));else RemovePropW(edit,L"CPP.NumericFocused");
        LRESULT result=DefSubclassProc(edit,msg,wp,lp);WCHAR text[64];GetWindowTextW(edit,text,_countof(text));
        CStringW next=NumericText(text,suffix,msg==WM_SETFOCUS);
        if(next!=text)
        {
            DWORD first=0,last=0;SendMessageW(edit,EM_GETSEL,WPARAM(&first),LPARAM(&last));
            DefSubclassProc(edit,WM_SETTEXT,0,LPARAM(LPCWSTR(next)));
            SendMessageW(edit,EM_SETSEL,min(first,DWORD(next.GetLength())),min(last,DWORD(next.GetLength())));
        }
        return result;
    }
    if(msg==WM_NCDESTROY) RemovePropW(edit,L"CPP.NumericFocused");
    return DefSubclassProc(edit,msg,wp,lp);
}
void InstallNumericSuffix(HWND edit,LPCWSTR suffix)
{ SetWindowSubclass(edit,PixelEditProc,0x7078,reinterpret_cast<DWORD_PTR>(suffix)); }
void InstallPixelUnits(HWND edit,bool enabled)
{
    SetWindowLongPtrW(edit,GWL_STYLE,GetWindowLongPtrW(edit,GWL_STYLE)&~ES_NUMBER);
    SendMessageW(edit,EM_SETLIMITTEXT,24,0);
    if(enabled) InstallNumericSuffix(edit,L"px");else RemoveWindowSubclass(edit,PixelEditProc,0x7078);
    WCHAR text[32];GetWindowTextW(edit,text,_countof(text));int value;
    if(ParsePixelValue(text,value,INT_MIN,INT_MAX))
    { CStringW next;if(enabled) next=PixelText(value);else next.Format(L"%d",value);SetWindowTextW(edit,next); }
}
bool IsEditKeyMessage(const MSG& message)
{
    if(message.message!=WM_KEYDOWN && message.message!=WM_SYSKEYDOWN) return false;
    WCHAR type[32];GetClassNameW(message.hwnd,type,_countof(type));
    if(lstrcmpiW(type,L"EDIT") && _wcsnicmp(type,L"RICHEDIT",8)) return false;
    WPARAM key=message.wParam;
    return key==VK_DELETE || key==VK_BACK || key==VK_INSERT || key==VK_HOME || key==VK_END ||
        (key>=VK_LEFT && key<=VK_DOWN) || (GetKeyState(VK_CONTROL)<0 && (key=='A'||key=='C'||key=='V'||key=='X'||key=='Z'||key=='Y'));
}
