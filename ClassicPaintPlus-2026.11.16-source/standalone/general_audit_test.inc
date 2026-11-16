// Regression cases for inherited document commands, not just Plus features.
static int auditFailures=0;
static void AuditCheck(bool ok,const char* message);
class AuditImageCodecs:public CImageDx
{
public:
    static bool CheckBuffer()
    {
        UINT count=0,required=0;auto entries=GetAllEncoders(count);
        if(!entries || !count) { delete[] entries;return false; }
        _shared()->m_GetImageEncodersSize(&count,&required);
        size_t allocated=_msize(entries);
        printf("audit encoder bytes: required=%u allocated=%zu old allocation=%zu\n",required,allocated,size_t(required)/sizeof(*entries)*sizeof(*entries));
        bool enough=allocated>=required;delete[] entries;return enough;
    }
};
static void AuditLegacyCodecs()
{
    AuditImageCodecs image;AuditCheck(image.CheckBuffer(),"legacy GDI+ encoder buffer includes all variable-length data");
    image.Attach(CreateColorDIB(5,3,RGB(120,50,20)));
    LPCWSTR path=L"general-audit-tests/atomic.gif";
    std::vector<BYTE> original={'o','r','i','g','i','n','a','l'};WriteImageBytes(path,original);
    HANDLE lock=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    RequireCanvas(lock!=INVALID_HANDLE_VALUE,"locked GIF fixture opened");
    HRESULT saved=image.SaveDx(path);CloseHandle(lock);
    std::vector<BYTE> result;ReadImageBytes(path,result);
    AuditCheck(FAILED(saved) && result==original,"failed legacy GIF replacement preserves original file");
    AuditCheck(SUCCEEDED(image.SaveDx(path)) && ReadImageBytes(path,result) && result.size()>6 && !memcmp(result.data(),"GIF8",4),"legacy GIF writes through temporary file");
    HDC dc=CreateEnhMetaFileW(NULL,NULL,NULL,NULL);RECT rect={50,60,70,70};HBRUSH brush=CreateSolidBrush(RGB(100,20,200));
    FillRect(dc,&rect,brush);DeleteObject(brush);HENHMETAFILE emf=CloseEnhMetaFile(dc);
    HBITMAP bitmap=BitmapFromHEMF(emf);RasterImage pixels;bool ok=ReadRaster(bitmap,pixels);
    DeleteObject(bitmap);DeleteEnhMetaFile(emf);
    AuditCheck(ok && pixels.width==20 && pixels.height==10 && pixels.pixels.front()==0xff6414c8 && pixels.pixels.back()==0xff6414c8,
        "nonzero-origin enhanced metafile pastes without blank offset or lost edge");
}
static void AuditPrintToPdf()
{
    // Explicit virtual printer and explicit file: never use the user's default printer.
    HDC printer=CreateDCW(L"WINSPOOL",L"Microsoft Print to PDF",NULL,NULL);
    if(!printer) { puts("SKIP audit: Microsoft Print to PDF is unavailable; no physical printer used");return; }
    WCHAR root[MAX_PATH],path[MAX_PATH];GetCurrentDirectoryW(_countof(root),root);
    swprintf_s(path,L"%s\\general-audit-tests\\print.pdf",root);DeleteFileW(path);
    HBITMAP bitmap=CreateColorDIB(80,60,RGB(120,50,20));
    BOOL ok=PrintBitmapDocument(printer,bitmap,L"Classic Paint Plus audit",path);DeleteObject(bitmap);DeleteDC(printer);
    std::vector<BYTE> bytes;
    for(int attempt=0;attempt<100;++attempt) { if(ReadImageBytes(path,bytes) && bytes.size()>100) break;Sleep(50); }
    AuditCheck(ok && bytes.size()>100 && !memcmp(bytes.data(),"%PDF-",5),"real GDI print job writes PDF through an explicit virtual printer");
}
static HANDLE auditStopDialogs=NULL;
static DWORD auditUiThread=0;
static WCHAR auditAppTitle[128]={};
static DWORD WINAPI AuditMessageBoxes(void*)
{
    while(WaitForSingleObject(auditStopDialogs,30)==WAIT_TIMEOUT)
        EnumThreadWindows(auditUiThread,[](HWND window,LPARAM)->BOOL {
            WCHAR title[128]={},cls[32]={};GetWindowTextW(window,title,_countof(title));GetClassNameW(window,cls,_countof(cls));
            if(!lstrcmpW(cls,L"#32770") && !lstrcmpW(title,auditAppTitle))
                PostMessageW(window,WM_CLOSE,0,0);
            return TRUE;
        },0);
    return 0;
}
static void AuditCheck(bool ok,const char* message)
{
    printf("%s audit: %s\n",ok?"PASS":"FAIL",message);
    if(!ok) ++auditFailures;
}
static VOID CALLBACK AuditDismissError(HWND,UINT,UINT_PTR,DWORD)
{
    EnumThreadWindows(GetCurrentThreadId(),[](HWND dialog,LPARAM)->BOOL {
        WCHAR cls[32]={};GetClassNameW(dialog,cls,_countof(cls));
        if(!lstrcmpW(cls,L"#32770") && IsWindowVisible(dialog) && GetDlgItem(dialog,IDOK))
            PostMessage(dialog,WM_COMMAND,IDOK,0);
        return TRUE;
    },0);
}
static void AuditHistory()
{
    toolsModel.OnEndDraw(TRUE);toolsModel.SetActiveTool(TOOL_RECTSEL);
    SetBitmapAndInfo(CreateColorDIB(8,6,RGB(255,255,255)),NULL,NULL,FALSE);
    imageModel.InvertColors();
    // SetFileInfo is the production save/open checkpoint.
    SetFileInfo(NULL,NULL,FALSE);
    imageModel.Undo();AuditCheck(!imageModel.IsImageSaved(),"undo after save is dirty");
    imageModel.Redo();AuditCheck(imageModel.IsImageSaved(),"redo to save checkpoint is clean");
    imageModel.InvertColors();imageModel.Undo();AuditCheck(imageModel.IsImageSaved(),"undo a new edit returns to save checkpoint");
    imageModel.Undo();imageModel.InvertColors();
    AuditCheck(!imageModel.IsImageSaved(),"branch at same undo position is a distinct unsaved revision");
    BOOL undo=imageModel.CanUndo();RasterImage before;ReadRaster(HBITMAP(GetCurrentObject(imageModel.GetDC(),OBJ_BITMAP)),before);
    std::vector<BYTE> bad={'B','M',1,2,3};WriteImageBytes(L"general-audit-tests/broken.bmp",bad);
    UINT_PTR timer=SetTimer(NULL,0,30,AuditDismissError);
    HBITMAP loaded=DoLoadImageFile(mainWindow,L"general-audit-tests/broken.bmp",TRUE);KillTimer(NULL,timer);
    RasterImage after;ReadRaster(HBITMAP(GetCurrentObject(imageModel.GetDC(),OBJ_BITMAP)),after);
    AuditCheck(!loaded && imageModel.CanUndo()==undo && before.pixels==after.pixels,"failed legacy BMP load preserves document and Undo");
    SetBitmapAndInfo(CreateColorDIB(1,10,RGB(60,90,120)),NULL,NULL,FALSE);
    imageModel.StretchSkew(1,200,0,0);
    AuditCheck(imageModel.GetWidth()==1 && imageModel.GetHeight()==20,"minimum one-pixel resize preserves the other dimension");
    SetBitmapAndInfo(CreateColorDIB(10,10,RGB(60,90,120)),NULL,NULL,FALSE);
    imageModel.StretchSkew(200,200,15,20);imageModel.Undo();
    AuditCheck(imageModel.GetWidth()==10 && imageModel.GetHeight()==10 && !imageModel.CanUndo(),"one Stretch/Skew command is one undo step");
}
static HANDLE auditClipboardReady=NULL,auditClipboardRelease=NULL;
static DWORD WINAPI AuditLockClipboard(void*)
{
    HWND owner=CreateWindowExW(0,L"STATIC",L"",0,0,0,1,1,HWND_MESSAGE,NULL,g_hinstExe,NULL);
    bool locked=OpenClipboard(owner)!=FALSE;SetEvent(auditClipboardReady);
    WaitForSingleObject(auditClipboardRelease,10000);
    if(locked) CloseClipboard();DestroyWindow(owner);return locked?0:1;
}
static void AuditCutFailure()
{
    SetBitmapAndInfo(CreateColorDIB(12,8,RGB(10,30,60)),NULL,NULL,FALSE);
    selectionModel.m_rc=selectionModel.m_rcOld=CRect(1,1,5,5);selectionModel.m_bShow=TRUE;
    auditClipboardReady=CreateEvent(NULL,TRUE,FALSE,NULL);auditClipboardRelease=CreateEvent(NULL,TRUE,FALSE,NULL);
    HANDLE thread=CreateThread(NULL,0,AuditLockClipboard,NULL,0,NULL);WaitForSingleObject(auditClipboardReady,5000);
    mainWindow.SendMessage(WM_COMMAND,IDM_EDITCUT);
    bool intact=selectionModel.m_bShow && GetPixel(imageModel.GetDC(),2,2)==RGB(10,30,60);
    SetEvent(auditClipboardRelease);WaitForSingleObject(thread,5000);DWORD code=1;GetExitCodeThread(thread,&code);
    CloseHandle(thread);CloseHandle(auditClipboardReady);CloseHandle(auditClipboardRelease);
    RequireCanvas(code==0,"clipboard contention fixture acquired clipboard");
    AuditCheck(intact,"Cut with busy clipboard keeps selected pixels");
    selectionModel.HideSelection();
}
static void AuditSelectionTransforms()
{
    registrySettings.SelectionInterpolation=0;toolsModel.SetBackgroundTransparent(FALSE);
    RasterImage source;source.width=4;source.height=2;
    source.pixels={0xffff0000,0xff00ff00,0xff0000ff,0xffffff00,0xff00ffff,0xffff00ff,0xff123456,0xffabcdef};
    {
        SelectionModel model;model.InsertFromHBITMAP(MakeRasterBitmap(source));model.m_bShow=TRUE;
        model.m_rc=CRect(0,0,8,4);model.FlipHorizontally();
        HBITMAP result=model.GetSelectionContents();RasterImage pixels;bool ok=ReadRaster(result,pixels);DeleteObject(result);
        AuditCheck(ok && pixels.pixels.front()==source.pixels[3] && pixels.pixels[7]==source.pixels[0],"mirror resized selection uses whole source bitmap");
    }
    {
        SelectionModel model;model.InsertFromHBITMAP(MakeRasterBitmap(source));model.m_bShow=TRUE;
        model.m_rc=CRect(0,0,2,1);model.RotateNTimes90Degrees(1);
        HBITMAP result=model.GetSelectionContents();RasterImage pixels;bool ok=ReadRaster(result,pixels);DeleteObject(result);
        RasterImage expected;expected.width=2;expected.height=4;expected.pixels.resize(8);
        for(int y=0;y<2;++y) for(int x=0;x<4;++x) expected.pixels[x*2+1-y]=source.pixels[y*4+x];
        SelectionModel reference;reference.InsertFromHBITMAP(MakeRasterBitmap(expected));reference.m_rc=CRect(0,0,1,2);
        HBITMAP referenceBitmap=reference.GetSelectionContents();RasterImage scaled;ReadRaster(referenceBitmap,scaled);DeleteObject(referenceBitmap);
        AuditCheck(ok && pixels.width==1 && pixels.height==2 && pixels.pixels==scaled.pixels,"rotate shrunk selection preserves source stride and dimensions");
        model.m_rc=CRect(0,0,12,16);model.RotateNTimes90Degrees(3);
        result=model.GetSelectionContents();ok=ReadRaster(result,pixels);DeleteObject(result);
        AuditCheck(ok && pixels.width==16 && pixels.height==12,"rotation after enlarging selection stays within source storage");
    }
    toolsModel.SetActiveTool(TOOL_FREESEL);registrySettings.ReplaceDeletedWithAlpha=FALSE;
    SetBitmapAndInfo(CreateColorDIB(8,4,RGB(200,0,0)),NULL,NULL,FALSE);
    HBITMAP mask=CreateMonoBitmap(4,2,FALSE);HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,mask);
    SetPixelV(dc,0,0,RGB(255,255,255));SelectObject(dc,old);DeleteDC(dc);
    selectionModel.setMask(CRect(1,1,5,3),mask);selectionModel.m_bShow=TRUE;selectionModel.TakeOff();selectionModel.FlipHorizontally();
    TestBitmap preview(8,4,RGB(200,0,0));selectionModel.DrawBackground(preview.dc,RGB(255,255,255));
    AuditCheck(GetPixel(preview.dc,1,1)==RGB(255,255,255) && GetPixel(preview.dc,4,1)==RGB(200,0,0),"freehand transform clears original mask, not transformed mask");
    selectionModel.HideSelection();toolsModel.SetActiveTool(TOOL_RECTSEL);
    DWORD handles=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    for(int i=0;i<32;++i)
    {
        SelectionModel model;model.InsertFromHBITMAP(MakeRasterBitmap(source));model.m_bShow=TRUE;
        model.StretchSkew(100,100,20,15);
    }
    DWORD leaked=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)-handles;
    printf("audit skew GDI handle delta=%ld\n",LONG(leaked));
    AuditCheck(LONG(leaked)<=2,"repeated selection skew releases GDI bitmaps");
}
static int auditSaveStage=0,auditSaveTicks=0;static bool auditAccept=false,auditFilterOk=false;
static WCHAR auditSavePath[MAX_PATH];
static VOID CALLBACK AuditDriveSave(HWND,UINT,UINT_PTR timer,DWORD)
{
    RequireCanvas(++auditSaveTicks<150,"audit Save As completes");
    HWND hook=FindSaveHook();
    if(!hook) { AuditDismissError(NULL,0,timer,0);return; }
    if(auditSaveStage) return;
    HWND dialog=GetParent(hook),combo=GetDlgItem(dialog,cmb1);WCHAR label[256]={};
    SendMessageW(combo,CB_GETLBTEXT,0,LPARAM(label));CStringW expected;expected.LoadString(g_hinstExe,IDS_BMP_24+1);
    auditFilterOk=wcsstr(label,expected)==label;
    if(auditAccept) SendMessageW(dialog,CDM_SETCONTROLTEXT,edt1,LPARAM(auditSavePath));
    auditSaveStage=1;PostMessage(dialog,WM_COMMAND,auditAccept?IDOK:IDCANCEL,0);
}
static void AuditSaveDialogs()
{
    WORD language=registrySettings.UILanguage;
    for(WORD next:{WORD(0x0409),WORD(0x0419)})
    {
        registrySettings.UILanguage=next;ApplyPreferredLanguage();mainWindow.ReloadLanguage();
        WCHAR file[MAX_PATH]=L"audit.png";auditSaveStage=auditSaveTicks=0;auditAccept=false;
        UINT_PTR timer=SetTimer(NULL,0,40,AuditDriveSave);mainWindow.GetSaveFileName(file,_countof(file));KillTimer(NULL,timer);
        AuditCheck(auditFilterOk,"Save As filter captions follow live UI language");
    }
    registrySettings.UILanguage=language;ApplyPreferredLanguage();mainWindow.ReloadLanguage();
    WCHAR root[MAX_PATH];GetCurrentDirectoryW(_countof(root),root);
    swprintf_s(auditSavePath,L"%s\\general-audit-tests\\too-wide.webp",root);DeleteFileW(auditSavePath);
    SetBitmapAndInfo(CreateColorDIB(17000,1,RGB(10,30,60)),L"general-audit-tests/original.png",NULL,TRUE);
    CStringW previous=g_szFileName;imageModel.InvertColors();
    auditSaveStage=auditSaveTicks=0;auditAccept=true;
    UINT_PTR timer=SetTimer(NULL,0,40,AuditDriveSave);mainWindow.SendMessage(WM_COMMAND,IDM_FILESAVEAS);KillTimer(NULL,timer);
    AuditCheck(previous==g_szFileName && !imageModel.IsImageSaved() && GetFileAttributesW(auditSavePath)==INVALID_FILE_ATTRIBUTES,
        "failed Save As keeps original document path and dirty state");
}
static int TestGeneralAudit()
{
    CreateDirectoryW(L"general-audit-tests",NULL);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    auditUiThread=GetCurrentThreadId();LoadStringW(g_hinstExe,IDS_PROGRAMNAME,auditAppTitle,_countof(auditAppTitle));
    auditStopDialogs=CreateEvent(NULL,TRUE,FALSE,NULL);HANDLE dialogs=CreateThread(NULL,0,AuditMessageBoxes,NULL,0,NULL);
    AuditLegacyCodecs();AuditHistory();AuditCutFailure();AuditSelectionTransforms();AuditSaveDialogs();AuditPrintToPdf();
    SetEvent(auditStopDialogs);WaitForSingleObject(dialogs,5000);CloseHandle(dialogs);CloseHandle(auditStopDialogs);
    printf("General audit failures: %d\n",auditFailures);return auditFailures?80:0;
}
