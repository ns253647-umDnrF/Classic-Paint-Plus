#pragma once

// Standard Windows color dialog with read-only sampling from the canvas.
BOOL ChooseColorFromCanvas(HWND owner, COLORREF* color,BYTE* alpha=NULL);
