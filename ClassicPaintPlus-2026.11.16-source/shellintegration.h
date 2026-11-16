#pragma once
// Register alternative image handlers without changing the user's default app.
bool RegisterPaintFileTypes(HKEY root,LPCWSTR classesPath,LPCWSTR executable,bool& changed);
void RegisterPaintWithWindows();
