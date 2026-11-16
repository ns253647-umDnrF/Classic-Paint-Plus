static double ThreePointRadius(FigureVertex a,FigureVertex b,FigureVertex c)
{
    double cross=fabs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x));
    return cross<1e-10?0:hypot(a.x-b.x,a.y-b.y)*hypot(b.x-c.x,b.y-c.y)*hypot(c.x-a.x,c.y-a.y)/(2*cross);
}
static void TestPixelRounding()
{
    CreateDirectoryW(L"shape-groups-tests",NULL);mainWindow.ShowWindow(SW_SHOWNOACTIVATE);
    FigureContour rectangle={{0,0},{1,0},{1,1},{0,1}};
    for(int width:{100,240,700})for(int height:{80,150})
    {
        auto p=RoundFigureContour(rectangle,12,width,height);
        RequireCanvas(p.size()%4==0,"rectangle has four equally sampled circular corners");
        size_t count=p.size()/4;
        FigureVertex centres[]={{12,12},{double(width-12),12},{double(width-12),double(height-12)},{12,double(height-12)}};
        for(size_t i=0;i<p.size();++i)
            RequireCanvas(fabs(hypot(p[i].x*width-centres[i/count].x,p[i].y*height-centres[i/count].y)-12)<1e-8,"12 px radius stays identical at all corners and aspect ratios");
    }
    for(FigureContour contour:{FigureContour{{0,0},{300,0},{0,170}},FigureContour{{0,0},{260,0},{310,160},{50,160}},
                              FigureContour{{0,0},{200,0},{200,200},{100,100},{0,200}}})
    {
        auto rounded=RoundFigureContour(contour,8,1,1);int arcs=0;
        for(size_t i=1;i+1<rounded.size();++i)
        {
            auto a=rounded[i-1],b=rounded[i],c=rounded[i+1];
            if(hypot(a.x-b.x,a.y-b.y)>8 || hypot(b.x-c.x,b.y-c.y)>8)continue;
            double radius=ThreePointRadius(a,b,c);
            if(radius>0){RequireCanvas(fabs(radius-8)<1e-7,"acute, obtuse, concave and skewed corners share the same circular radius");++arcs;}
        }
        RequireCanvas(arcs>=int(contour.size()),"all corner arcs checked geometrically");
    }
    auto limited=RoundFigureContour(rectangle,500,100,20);
    for(auto p:limited)RequireCanvas(std::isfinite(p.x) && std::isfinite(p.y) && p.x>=-1e-8 && p.x<=1+1e-8 && p.y>=-1e-8 && p.y<=1+1e-8,"oversized radius fits the shortest edge without reversing segments");
    auto open=RoundFigureContour({{0,0},{0,0},{100,0},{100,100},{100,100}},20,1,1,false);
    RequireCanvas(open.front().x==0 && open.front().y==0 && open.back().x==100 && open.back().y==100,"open polygons retain their endpoints and ignore construction duplicates");
    VectorReset();ActivateFigureChoice(FIG_RECT);toolsModel.ResetObjectSize(false);NewParameterFigure();
    EnterObjectValue(ParamEdit(PARAM_ROUNDING),L"24 px");
    RequireCanvas(FigureRounding()==24 && WindowLabel(ParamEdit(PARAM_ROUNDING))==L"24 px","radius field accepts and displays pixels");
    SetFocus(ParamEdit(PARAM_ROUNDING));RequireCanvas(WindowLabel(ParamEdit(PARAM_ROUNDING))==L"24","radius suffix disappears while editing");canvasWindow.SetFocus();
    toolsModel.SetFigureDimensions(240,130);
    RequireCanvas(FigureRounding()==24,"resizing the editable figure does not change radius");
    FigureClipboardData d={};RequireCanvas(toolsModel.ExportFigure(d) && d.version==3 && d.rounding==24,"clipboard version identifies the pixel radius");
    HGLOBAL packed=PackFigureClipboard(d);FigureClipboardData copy={};RequireCanvas(packed && UnpackFigureClipboard(packed,copy) && copy.rounding==24,"editable clipboard preserves pixel radius");GlobalFree(packed);
    d.rounding=501;RequireCanvas(!PackFigureClipboard(d),"clipboard rejects out-of-range radii");
    SendMessage(ParameterControl(IDC_SAVE_OBJECT_SIZE),BM_CLICK,0,0);SetFigureRounding(3);toolsModel.OnEndDraw(TRUE);NewParameterFigure();
    RequireCanvas(FigureRounding()==24,"S retains pixel radius snapshot for the next figure");toolsModel.OnEndDraw(TRUE);SendMessage(ParameterControl(IDC_RESET_OBJECT_SIZE),BM_CLICK,0,0);RequireCanvas(!FigureRounding(),"R restores zero radius");
    HWND slider=ParameterControl(IDC_FIGURE_ROUNDING);RequireCanvas(SendMessage(slider,TBM_GETRANGEMAX,0,0)==MAX_FIGURE_RADIUS,"radius slider range is in pixels");
    TestBitmap sheet(840,420,RGB(255,255,255));int slot=0;
    for(int kind:{FIG_RECT,FIG_TRIANGLE,FIG_DIAMOND,FIG_HEXAGON,FIG_STAR5,FIG_STAR16})for(int radius:{0,10,30})
    {
        int x=(slot/3)*140,y=(slot%3)*140;RECT r={x+15,y+15,x+125,y+125};
        DrawFigure(sheet.dc,kind,r,(HBRUSH)GetStockObject(BLACK_BRUSH),(HBRUSH)GetStockObject(LTGRAY_BRUSH),2,1,0,0,0,radius);++slot;
    }
    SaveShapeSheet(sheet.bitmap,L"shape-groups-tests/rounding.bmp");TestFreePolygonRounding();
    puts("PASS pixel radii: circular geometry, different sizes/angles, skew, concavity, short edges, native field/slider, S/R, clipboard and free polygon");
}
