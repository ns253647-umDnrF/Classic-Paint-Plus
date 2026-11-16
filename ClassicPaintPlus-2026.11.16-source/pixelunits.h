#pragma once
CStringW PixelText(int value);
bool ParsePixelValue(LPCWSTR text,int& value,int minimum,int maximum);
void InstallPixelUnits(HWND edit,bool enabled=true);
void InstallNumericSuffix(HWND edit,LPCWSTR suffix);
bool IsEditKeyMessage(const MSG& message);
