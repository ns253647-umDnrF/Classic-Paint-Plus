#pragma once
struct FigureVertex { double x,y; };
using FigureContour=std::vector<FigureVertex>;
using FigureContours=std::vector<FigureContour>;
FigureContours FigureGeometry(int kind,int ellipseSteps=120,int starBend=0);
FigureContour RoundFigureContour(const FigureContour& vertices,int radius,double width,double height,bool closed=true);
int FigureStarPoints(int kind);
