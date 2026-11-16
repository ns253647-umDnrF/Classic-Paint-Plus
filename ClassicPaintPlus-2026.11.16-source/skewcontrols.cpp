// Exact affine controls share the same geometry as the on-canvas handles.
#include <cmath>
#include "precomp.h"

void CToolSettingsWindow::CreateSkewControls(TOOLINFOW& info)
{
    transformSeparator.Create(L"STATIC",m_hWnd,CRect(3,0,47,2),NULL,WS_CHILD|SS_ETCHEDHORZ,0,IDC_TRANSFORM_SEPARATOR);
    for(int i=0;i<2;++i)
    {
        skewCaption[i].Create(L"STATIC",m_hWnd,CRect(3,0,47,12),i?L"\u2195":L"\u2194",WS_CHILD|SS_CENTER|SS_NOTIFY);
        skewEdit[i].Create(L"EDIT",m_hWnd,CRect(3,0,47,21),NULL,
            WS_CHILD|WS_TABSTOP|ES_CENTER|ES_AUTOHSCROLL,WS_EX_CLIENTEDGE,IDC_SKEW_HORIZONTAL+i);
        skewEdit[i].SendMessage(EM_SETLIMITTEXT,24,0);InstallNumericSuffix(skewEdit[i],L"\u00b0");
        SetWindowSubclass(skewEdit[i],SkewEditProc,1,reinterpret_cast<DWORD_PTR>(this));
        for(HWND control:{skewEdit[i].m_hWnd,skewCaption[i].m_hWnd})
        { info.uId=reinterpret_cast<UINT_PTR>(control);::SendMessage(roundingTip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&info)); }
    }
}
void CToolSettingsWindow::UpdateSkew(bool forceText)
{
    if(!skewEdit[0].IsWindow())return;
    bool selection=toolsModel.IsSelection(),figure=IsBoundedFigureTool(toolsModel.GetActiveTool());
    bool visible=selection || figure,enabled=figure || (selection && selectionModel.m_bShow && !selectionModel.m_rc.IsRectEmpty());
    double values[2]={selectionModel.SkewX(),selectionModel.SkewY()};
    if(figure)toolsModel.FigureSkew(values[0],values[1]);
    int y=selection?246:204;
    for(int i=0;i<2;++i)
    {
        skewLabel[i].LoadString(g_hinstExe,IDS_SKEW_HORIZONTAL+i);


        skewCaption[i].ShowWindow(SW_HIDE);skewCaption[i].EnableWindow(enabled);
        skewEdit[i].ShowWindow(visible?SW_SHOWNA:SW_HIDE);skewEdit[i].EnableWindow(enabled);
        if(forceText || ::GetFocus()!=skewEdit[i].m_hWnd)
        {
            CStringW number;number.Format(L"%.2f",atan(values[i])*180/M_PI);
            number.TrimRight(L'0');number.TrimRight(L'.');if(number==L"-0")number=L"0";number+=L"\u00b0";
            if(!ParameterHasValue(PARAM_SKEW_X+i))number.Empty();
            lastSkewText[i]=number;skewEdit[i].SetWindowText(number);
        }
    }
}
bool CToolSettingsWindow::ApplySkewText(HWND edit)
{
    if(!::IsWindowEnabled(edit))return false;
    int index=edit==skewEdit[1].m_hWnd;CStringW text;skewEdit[index].GetWindowText(text);
    text.Trim();text.Replace(L',',L'.');text.TrimRight(L"\u00b0 ");
    if(text.IsEmpty()) { ClearParameter(PARAM_SKEW_X+index);UpdateSkew(true);UpdateParameterBar();return true; }
    CStringW displayed(lastSkewText[index]);displayed.TrimRight(L"\u00b0 ");
    if(text==displayed)return true; // Preserve the exact mouse angle on focus-only changes.
    WCHAR* end=NULL;double degrees=wcstod(text,&end);
    if(text.IsEmpty() || end==LPCWSTR(text) || *end || !std::isfinite(degrees) || fabs(degrees)>=90)return false;
    double value=tan(degrees*M_PI/180);if(fabs(value)>8)return false;
    double values[2]={selectionModel.SkewX(),selectionModel.SkewY()};
    bool selection=toolsModel.IsSelection();if(!selection)toolsModel.FigureSkew(values[0],values[1]);values[index]=value;
    bool applied=selection?selectionModel.SetSkew(values[0],values[1]):toolsModel.SetFigureSkew(values[0],values[1]);
    if(applied){ParameterEntered(PARAM_SKEW_X+index);UpdateSkew(true);UpdateParameterBar();}return applied;
}
LRESULT CALLBACK CToolSettingsWindow::SkewEditProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR context)
{
    auto self=reinterpret_cast<CToolSettingsWindow*>(context);
    if(message==WM_GETDLGCODE)return DLGC_WANTALLKEYS;
    if(message==WM_KEYDOWN && (wp==VK_RETURN || wp==VK_ESCAPE))
    {
        if(wp==VK_ESCAPE)self->UpdateSkew(true);
        else if(!self->ApplySkewText(hwnd)) { MessageBeep(MB_OK);::SendMessage(hwnd,EM_SETSEL,0,-1);return 0; }
        canvasWindow.SetFocus();return 0;
    }
    if(message==WM_CHAR && (wp==VK_RETURN || wp==VK_ESCAPE))return 0;
    if(message==WM_KILLFOCUS && !self->ApplySkewText(hwnd))self->UpdateSkew(true);
    return DefSubclassProc(hwnd,message,wp,lp);
}
