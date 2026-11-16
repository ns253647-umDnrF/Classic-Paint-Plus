/*
 * PROJECT:    PAINT for ReactOS
 * LICENSE:    LGPL-2.0-or-later (https://spdx.org/licenses/LGPL-2.0-or-later)
 * PURPOSE:    Window procedure of the size boxes
 * COPYRIGHT:  Copyright 2015 Benedikt Freisen <b.freisen@gmx.net>
 *             Copyright 2017 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
 */

#include "precomp.h"

static LPCWSTR s_cursor_shapes[] =
{
    IDC_SIZENWSE, IDC_SIZENS, IDC_SIZENESW,
    IDC_SIZEWE,               IDC_SIZEWE,
    IDC_SIZENESW, IDC_SIZENS, IDC_SIZENWSE,
};

/* FUNCTIONS ********************************************************/

BOOL setCursorOnSizeBox(HITTEST hit)
{
    if (HIT_UPPER_LEFT <= hit && hit <= HIT_LOWER_RIGHT)
    {
        ::SetCursor(::LoadCursorW(NULL, s_cursor_shapes[hit - HIT_UPPER_LEFT]));
        return TRUE;
    }
    return FALSE;
}

BOOL getSizeBoxRect(LPRECT prc, HITTEST hit, LPCRECT prcBase, int gripSize)
{
    switch (hit)
    {
        case HIT_UPPER_LEFT:
            prc->left = prcBase->left;
            prc->top = prcBase->top;
            break;
        case HIT_UPPER_CENTER:
            prc->left = (prcBase->left + prcBase->right - gripSize) / 2;
            prc->top = prcBase->top;
            break;
        case HIT_UPPER_RIGHT:
            prc->left = prcBase->right - gripSize;
            prc->top = prcBase->top;
            break;
        case HIT_MIDDLE_LEFT:
            prc->left = prcBase->left;
            prc->top = (prcBase->top + prcBase->bottom - gripSize) / 2;
            break;
        case HIT_MIDDLE_RIGHT:
            prc->left = prcBase->right - gripSize;
            prc->top = (prcBase->top + prcBase->bottom - gripSize) / 2;
            break;
        case HIT_LOWER_LEFT:
            prc->left = prcBase->left;
            prc->top = prcBase->bottom - gripSize;
            break;
        case HIT_LOWER_CENTER:
            prc->left = (prcBase->left + prcBase->right - gripSize) / 2;
            prc->top = prcBase->bottom - gripSize;
            break;
        case HIT_LOWER_RIGHT:
            prc->left = prcBase->right - gripSize;
            prc->top = prcBase->bottom - gripSize;
            break;
        case HIT_INNER:
            *prc = *prcBase;
            ::InflateRect(prc, -gripSize, -gripSize);
            return TRUE;
        default:
            ::SetRectEmpty(prc);
            return FALSE;
    }

    prc->right = prc->left + gripSize;
    prc->bottom = prc->top + gripSize;
    return TRUE;
}

HITTEST getSizeBoxHitTest(POINT pt, LPCRECT prcBase, int gripSize)
{
    CRect rc;

    if (!::PtInRect(prcBase, pt))
        return HIT_NONE;

    rc = *prcBase;
    rc.InflateRect(-gripSize, -gripSize);
    if (rc.PtInRect(pt))
        return HIT_INNER;

    for (INT i = HIT_UPPER_LEFT; i <= HIT_LOWER_RIGHT; ++i)
    {
        HITTEST hit = (HITTEST)i;
        getSizeBoxRect(&rc, hit, prcBase, gripSize);
        if (rc.PtInRect(pt))
            return hit;
    }

    return HIT_BORDER;
}

VOID drawSizeBoxes(HDC hdc, LPCRECT prcBase, BOOL bDrawFrame, LPCRECT prcPaint, int gripSize, COLORREF color)
{
    CRect rc, rcIntersect;

    if (prcPaint && !::IntersectRect(&rcIntersect, prcPaint, prcBase))
        return;

    if (bDrawFrame)
    {
        rc = *prcBase;
        rc.InflateRect(-gripSize / 2, -gripSize / 2);

        DrawDashedRect(hdc,rc);
    }

    HBRUSH marker = CreateSolidBrush(color==CLR_INVALID?GetSysColor(COLOR_HIGHLIGHT):color);
    for (INT i = HIT_UPPER_LEFT; i <= HIT_LOWER_RIGHT; ++i)
    {
        getSizeBoxRect(&rc, (HITTEST)i, prcBase, gripSize);
        if (!prcPaint || ::IntersectRect(&rcIntersect, &rc, prcPaint))
            ::FillRect(hdc, &rc, marker);
    }
    DeleteObject(marker);
}
