/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Window procedure of the size boxes
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 *             Copyright 2017-2023 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
 */

#pragma once

BOOL setCursorOnSizeBox(HITTEST hit);
BOOL getSizeBoxRect(LPRECT prc, HITTEST hit, LPCRECT prcBase, int gripSize = GRIP_SIZE);
HITTEST getSizeBoxHitTest(POINT pt, LPCRECT prcBase, int gripSize = GRIP_SIZE);
VOID drawSizeBoxes(HDC hdc, LPCRECT prcBase, BOOL bDrawFrame = FALSE, LPCRECT prcPaint = NULL,
                   int gripSize = GRIP_SIZE, COLORREF color = CLR_INVALID);
