#include "preview_window.h"
#include "globals.h"
#include "image_processor.h"
#include "i18n.h"
#include <shellapi.h>

LRESULT CALLBACK PreviewWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        DragAcceptFiles(hWnd, TRUE);
        return 0;

    case WM_DROPFILES:
    {
        HDROP hDrop = (HDROP)wParam;
        WCHAR szFile[MAX_PATH] = {0};
        if (DragQueryFileW(hDrop, 0, szFile, MAX_PATH) > 0)
        {
            const WCHAR* ext = wcsrchr(szFile, L'.');
            if (ext)
            {
                WCHAR extLower[16];
                int i = 0;
                for (; ext[i] && i < 15; i++)
                    extLower[i] = (ext[i] >= L'A' && ext[i] <= L'Z') ? ext[i] + 32 : ext[i];
                extLower[i] = L'\0';

                if (wcscmp(extLower, L".png") == 0 || wcscmp(extLower, L".jpg") == 0 ||
                    wcscmp(extLower, L".jpeg") == 0 || wcscmp(extLower, L".bmp") == 0 ||
                    wcscmp(extLower, L".gif") == 0 || wcscmp(extLower, L".tiff") == 0)
                {
                    LoadImageFromPath(szFile);
                }
                else
                {
                    UpdateStatusText(GetI18N().statusUnsupportedFormat);
                }
            }
        }
        DragFinish(hDrop);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));

        if (g_pPreviewBitmap && g_pPreviewBitmap->GetLastStatus() == Gdiplus::Ok)
        {
            Gdiplus::Graphics graphics(hdc);
            graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

            int pw = rc.right - rc.left;
            int ph = rc.bottom - rc.top;
            if (pw <= 0 || ph <= 0) { EndPaint(hWnd, &ps); return 0; }

            int iw = g_pPreviewBitmap->GetWidth();
            int ih = g_pPreviewBitmap->GetHeight();
            if (iw <= 0 || ih <= 0) { EndPaint(hWnd, &ps); return 0; }

            float scaleX = (float)pw / (float)iw;
            float scaleY = (float)ph / (float)ih;
            float scale = (scaleX < scaleY) ? scaleX : scaleY;

            int dw = (int)(iw * scale);
            int dh = (int)(ih * scale);
            int dx = (pw - dw) / 2;
            int dy = (ph - dh) / 2;

            graphics.DrawImage(g_pPreviewBitmap, dx, dy, dw, dh);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
