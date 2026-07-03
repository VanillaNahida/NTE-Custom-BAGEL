#include "about_dialog.h"
#include "globals.h"
#include "version.h"
#include "resource.h"
#include "i18n.h"
#include <commctrl.h>
#include <shellapi.h>

LRESULT CALLBACK AboutDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        const auto& i18n = GetI18N();
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        HFONT hFont = NULL;
        NONCLIENTMETRICSW ncm = {0};
        ncm.cbSize = sizeof(ncm);
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
        {
            hFont = CreateFontIndirectW(&ncm.lfMessageFont);
        }
        if (!hFont)
            hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)hFont);

        const int marginX = 20;
        const int marginTop = 14;
        const int textMaxWidth = 380;
        int y = marginTop;

        const int infoLineHeight = 20;
        {
            std::wstring verText = i18n.aboutVersionPrefix + std::wstring(L"v") + std::wstring(APP_VERSION_W);
            HWND hVersion = CreateWindowW(L"STATIC", verText.c_str(),
                WS_CHILD | WS_VISIBLE,
                marginX, y, textMaxWidth, infoLineHeight,
                hWnd, NULL, hInst, NULL);
            SendMessageW(hVersion, WM_SETFONT, (WPARAM)hFont, TRUE);
        }
        y += infoLineHeight;

        HWND hAuthor = CreateWindowW(L"STATIC", i18n.aboutAuthor,
            WS_CHILD | WS_VISIBLE,
            marginX, y, textMaxWidth, infoLineHeight,
            hWnd, NULL, hInst, NULL);
        SendMessageW(hAuthor, WM_SETFONT, (WPARAM)hFont, TRUE);
        y += infoLineHeight + 6;

        const WCHAR* szDisclaimer = i18n.aboutDisclaimer;

        HDC hdc = GetDC(hWnd);
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        RECT rcText = {0, 0, textMaxWidth, 0};
        DrawTextW(hdc, szDisclaimer, -1, &rcText, DT_CALCRECT | DT_WORDBREAK);
        SelectObject(hdc, hOldFont);
        ReleaseDC(hWnd, hdc);

        int textHeight = rcText.bottom - rcText.top;

        HWND hStatic = CreateWindowW(L"STATIC", szDisclaimer,
            WS_CHILD | WS_VISIBLE,
            marginX, y, textMaxWidth, textHeight,
            hWnd, NULL, hInst, NULL);
        SendMessageW(hStatic, WM_SETFONT, (WPARAM)hFont, TRUE);

        y += textHeight;

        const int linkGap = 8;
        const int linkHeight = 28;
        y += linkGap;

        HWND hLink;

        hLink = CreateWindowW(L"SysLink",
            i18n.aboutLinkGithub,
            WS_CHILD | WS_VISIBLE,
            marginX, y, textMaxWidth, linkHeight,
            hWnd, NULL, hInst, NULL);
        SendMessageW(hLink, WM_SETFONT, (WPARAM)hFont, TRUE);

        y += linkHeight + 8;

        hLink = CreateWindowW(L"SysLink",
            i18n.aboutLinkBilibili,
            WS_CHILD | WS_VISIBLE,
            marginX, y, textMaxWidth, linkHeight,
            hWnd, NULL, hInst, NULL);
        SendMessageW(hLink, WM_SETFONT, (WPARAM)hFont, TRUE);

        y += linkHeight + 8;

        hLink = CreateWindowW(L"SysLink",
            i18n.aboutLinkContact,
            WS_CHILD | WS_VISIBLE,
            marginX, y, textMaxWidth, linkHeight,
            hWnd, NULL, hInst, NULL);
        SendMessageW(hLink, WM_SETFONT, (WPARAM)hFont, TRUE);

        y += linkHeight + 12;

        int btnWidth = 80;
        int btnHeight = 30;
        int btnX = (textMaxWidth - btnWidth) / 2 + marginX;
        HWND hBtn = CreateWindowW(L"BUTTON", i18n.aboutBtnOk,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            btnX, y, btnWidth, btnHeight,
            hWnd, (HMENU)IDC_ABOUT_CLOSE, hInst, NULL);
        SendMessageW(hBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

        RECT rcWindow = {0, 0, 440, y + btnHeight + 14};
        AdjustWindowRect(&rcWindow, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
        int dlgW = rcWindow.right - rcWindow.left;
        int dlgH = rcWindow.bottom - rcWindow.top;

        RECT rcParent;
        GetWindowRect(((LPCREATESTRUCT)lParam)->hwndParent, &rcParent);
        int x = rcParent.left + ((rcParent.right - rcParent.left) - dlgW) / 2;
        int yPos = rcParent.top + ((rcParent.bottom - rcParent.top) - dlgH) / 2;

        SetWindowPos(hWnd, NULL, x, yPos, dlgW, dlgH, SWP_NOZORDER);

        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_ABOUT_CLOSE && HIWORD(wParam) == BN_CLICKED)
            DestroyWindow(hWnd);
        break;

    case WM_NOTIFY:
    {
        LPNMHDR pnmh = (LPNMHDR)lParam;
        if (pnmh->code == NM_CLICK || pnmh->code == NM_RETURN)
        {
            PNMLINK pNMLink = (PNMLINK)lParam;
            if (pNMLink->item.szUrl[0] != L'\0')
            {
                ShellExecuteW(NULL, L"open", pNMLink->item.szUrl, NULL, NULL, SW_SHOWNORMAL);
            }
        }
        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetBkMode(hdcStatic, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }

    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
    {
        HFONT hFont = (HFONT)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        if (hFont && hFont != (HFONT)GetStockObject(DEFAULT_GUI_FONT))
            DeleteObject(hFont);
        EnableWindow(g_hWndMain, TRUE);
        SetForegroundWindow(g_hWndMain);
        break;
    }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void OnAbout()
{
    const auto& i18n = GetI18N();
    WNDCLASSEXW wcAbout = {0};
    wcAbout.cbSize = sizeof(wcAbout);
    wcAbout.style = CS_HREDRAW | CS_VREDRAW;
    wcAbout.lpfnWndProc = AboutDlgProc;
    wcAbout.hInstance = g_hInst;
    wcAbout.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcAbout.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcAbout.lpszClassName = L"AboutDialog";
    wcAbout.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_APPICON));
    RegisterClassExW(&wcAbout);

    HWND hDlg = CreateWindowExW(0, L"AboutDialog", i18n.aboutTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 440, CW_USEDEFAULT,
        g_hWndMain, NULL, g_hInst, NULL);

    if (hDlg)
    {
        EnableWindow(g_hWndMain, FALSE);
        ShowWindow(hDlg, SW_SHOW);
        UpdateWindow(hDlg);
    }
}
