static void TestParameterFlags()
{
    TestParameterBar();
    VectorReset(800,600);ActivateFigureChoice(FIG_STAR5);toolsModel.ResetObjectSize(false);
    const LPCWSTR initial[]={L"160",L"90",L"25",L"12",L"-8",L"35",L"-40"};
    for(int p=0;p<PARAM_COUNT;++p)
    {
        EnterObjectValue(ParamEdit(p),initial[p]);
        RequireCanvas(ParameterHasValue(p) && !ParameterIsFixed(p),"entering an empty field sets its value without fixing it");
    }
    for(int p=PARAM_ROUNDING;p<PARAM_COUNT;++p)
    {
        WCHAR type[32];HWND label=ParameterControl(IDC_PARAMETER_FIXED_FIRST+p);GetClassNameW(label,type,_countof(type));
        RequireCanvas(!lstrcmpiW(type,L"STATIC") && !(GetWindowLongPtr(label,GWL_STYLE)&WS_TABSTOP),"rounding and star bend have plain labels without checkboxes or checkbox tab stops");
    }
    ParamCheck(PARAM_HEIGHT,true);ParamCheck(PARAM_SKEW_X,true);
    ParamCheck(PARAM_WIDTH,true);ParamCheck(PARAM_WIDTH,false);
    RequireCanvas(WindowLabel(ParamEdit(PARAM_WIDTH))==L"160 px" && toolsModel.ObjectSizePreset(false).cx==160,"unchecking an idle dimension preserves the starting value");
    ParamCheck(PARAM_ANGLE,true);ParamCheck(PARAM_ANGLE,false);
    RequireCanvas(WindowLabel(ParamEdit(PARAM_ANGLE))==L"25°" && toolsModel.FigureAngle()==25,"unchecking an idle angle preserves the starting value");
    SendMessage(ParameterControl(IDC_SAVE_OBJECT_SIZE),BM_CLICK,0,0);
    NewParameterFigure();
    auto snapshot=[]()
    {
        CRect r;RequireCanvas(toolsModel.EditableFigureBounds(r) && r.Width()==160 && r.Height()==90,"saved unlocked width and locked height are both applied to the new shape");
        double x,y;toolsModel.FigureSkew(x,y);
        RequireCanvas(toolsModel.FigureAngle()==25 && fabs(x-tan(12*M_PI/180))<1e-12 && fabs(y-tan(-8*M_PI/180))<1e-12,"S preserves angle and both skews regardless of checkbox state");
        for(int p=0;p<PARAM_ROUNDING;++p)RequireCanvas(ParameterIsFixed(p)==(p==PARAM_HEIGHT || p==PARAM_SKEW_X),"S restores the exact mixed fixation mask");
        RequireCanvas(FigureRounding()==35 && toolsModel.StarBend()==-40,"S still preserves rounding and signed bend without checkboxes");
    };
    snapshot();
    const LPCWSTR changed[]={L"180",L"110",L"45",L"8",L"-4"};
    for(int p=0;p<PARAM_ROUNDING;++p)
    {
        bool fixed=ParameterIsFixed(p);EnterObjectValue(ParamEdit(p),changed[p]);
        RequireCanvas(ParameterIsFixed(p)==fixed,"editing a populated field preserves its checked or unchecked state");
    }
    ParamCheck(PARAM_HEIGHT,false);ParamCheck(PARAM_ANGLE,true);EnterObjectValue(ParamEdit(PARAM_ROUNDING),L"60");EnterObjectValue(ParamEdit(PARAM_STAR_BEND),L"70");
    toolsModel.OnEndDraw(TRUE);NewParameterFigure();snapshot();
    // Save the same live numbers with a different checkbox state, then omit
    // one field. Only another S press may replace the previous snapshot.
    ParamCheck(PARAM_HEIGHT,false);ParamCheck(PARAM_ANGLE,true);EnterObjectValue(ParamEdit(PARAM_WIDTH),L"");
    SendMessage(ParameterControl(IDC_SAVE_OBJECT_SIZE),BM_CLICK,0,0);toolsModel.OnEndDraw(TRUE);NewParameterFigure();
    RequireCanvas(!ParameterIsFixed(PARAM_WIDTH) && !ParameterIsFixed(PARAM_HEIGHT) && ParameterIsFixed(PARAM_ANGLE),"S saves unchecked values and deliberately empty fields independently");
    toolsModel.OnEndDraw(TRUE);toolsModel.ResetObjectSize(false);
    EnterObjectValue(ParamEdit(PARAM_HEIGHT),L"90");ParamCheck(PARAM_HEIGHT,true);EnterObjectValue(ParamEdit(PARAM_HEIGHT),L"");
    RequireCanvas(!ParameterHasValue(PARAM_HEIGHT) && !ParameterIsFixed(PARAM_HEIGHT),"clearing a fixed prepared dimension clears fixation");
    EnterObjectValue(ParamEdit(PARAM_HEIGHT),L"95");RequireCanvas(!ParameterIsFixed(PARAM_HEIGHT),"re-entering a cleared dimension does not restore an old checkmark");
    ActivateFigureChoice(FIG_TRIANGLE);toolsModel.ResetObjectSize(false);RequireCanvas(!ParameterHasValue(PARAM_HEIGHT) && !ParameterIsFixed(PARAM_HEIGHT),"parameter memory remains separate for each figure kind");
    puts("PASS independent values/fixation: empty/populated input, unchecking before/after creation, mixed S snapshots, per-kind memory, R, rounding and bend without checkboxes");

    DWORD remember=registrySettings.RememberObjectSizes;registrySettings.RememberObjectSizes=1;
    ActivateFigureChoice(FIG_DIAMOND);toolsModel.ResetObjectSize(false);
    EnterObjectValue(ParamEdit(PARAM_WIDTH),L"144");EnterObjectValue(ParamEdit(PARAM_HEIGHT),L"88");ParamCheck(PARAM_HEIGHT,true);
    SendMessage(ParameterControl(IDC_SAVE_OBJECT_SIZE),BM_CLICK,0,0);
    RegistrySettings loaded;loaded.Load(SW_HIDE);
    RequireCanvas(loaded.SavedObjectSizes[FIG_DIAMOND].cx==144 && loaded.SavedObjectSizes[FIG_DIAMOND].cy==88 && loaded.SavedObjectFixed[FIG_DIAMOND]==(1u<<PARAM_HEIGHT),"INI preserves positive unlocked width and independently fixed height");
    PrepareTextRegression();EnterObjectValue(ParamEdit(PARAM_WIDTH),L"180");EnterObjectValue(ParamEdit(PARAM_HEIGHT),L"90");EnterObjectValue(ParamEdit(PARAM_ANGLE),L"0");ParamCheck(PARAM_WIDTH,true);
    SendMessage(ParameterControl(IDC_SAVE_OBJECT_SIZE),BM_CLICK,0,0);NewRegressionText();
    RequireCanvas(ParameterIsFixed(PARAM_WIDTH) && !ParameterIsFixed(PARAM_HEIGHT) && !ParameterIsFixed(PARAM_ANGLE),"new text restores saved checkbox states without automatically fixing its angle or height");
    EnterObjectValue(ParamEdit(PARAM_ANGLE),L"22");RequireCanvas(!ParameterIsFixed(PARAM_ANGLE),"live text angle entry does not enable fixation");
    ParamCheck(PARAM_ANGLE,true);SendMessage(ParameterControl(IDC_SAVE_OBJECT_SIZE),BM_CLICK,0,0);toolsModel.OnEndDraw(TRUE);NewRegressionText();
    RequireCanvas(textEditWindow.Angle()==22 && ParameterIsFixed(PARAM_ANGLE) && !ParameterIsFixed(PARAM_HEIGHT),"text S captures angle value and checkbox together");
    toolsModel.OnEndDraw(TRUE);ParamCheck(PARAM_ANGLE,false);RequireCanvas(textEditWindow.Angle()==22 && WindowLabel(ParamEdit(PARAM_ANGLE))==L"22°","unchecking prepared text angle retains its number");
    EnterObjectValue(ParamEdit(PARAM_ANGLE),L"0");SendMessage(ParameterControl(IDC_SAVE_OBJECT_SIZE),BM_CLICK,0,0);loaded.Load(SW_HIDE);
    RequireCanvas(loaded.SavedTextAngle==0 && loaded.SavedTextAnglePresent && !(loaded.SavedObjectFixed[FIG_COUNT]&(1u<<PARAM_ANGLE)),"INI distinguishes an explicit unchecked zero angle from an empty field");
    registrySettings.RememberObjectSizes=0;registrySettings.StorePreferences();loaded.Load(SW_HIDE);
    RequireCanvas(!loaded.SavedObjectFixed[FIG_DIAMOND] && !loaded.SavedObjectFixed[FIG_COUNT] && !loaded.SavedTextAnglePresent,"disabled persistence clears values and fixation from next-run settings");
    registrySettings.RememberObjectSizes=remember;toolsModel.ResetObjectSize(true);ActivateFigureChoice(FIG_DIAMOND);toolsModel.ResetObjectSize(false);
    puts("PASS fixation persistence: per-kind INI masks, mixed text state, explicit zero text angle and remember-sizes opt-out");
    puts("PASS all independent parameter checks");
}
