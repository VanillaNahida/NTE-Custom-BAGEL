#include "settings_dialog.h"
#include "globals.h"
#include "config_manager.h"
#include "game_detector.h"
#include "resource.h"
#include "i18n.h"
#include <shlobj.h>

LRESULT CALLBACK SettingsDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static std::vector<std::wstring> snapPaths;
    static std::vector<std::wstring> snapUids;
    static std::wstring snapDefPath;
    static std::wstring snapDefUid;

    switch (msg)
    {
    case WM_CREATE:
    {
        const auto& i18n = GetI18N();
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        snapPaths = g_gamePaths;
        snapUids = g_uids;
        snapDefPath = g_defaultGamePath;
        snapDefUid = g_defaultUid;

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

        const int marginX = 16;
        const int textW = 440;
        int y = 14;

        CreateWindowW(L"STATIC", i18n.settingsGamePath,
            WS_CHILD | WS_VISIBLE,
            marginX, y, textW, 20,
            hWnd, NULL, hInst, NULL);
        y += 22;

        HWND hList = CreateWindowW(L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
            marginX, y, textW, 80,
            hWnd, (HMENU)IDC_SETTINGS_PATH_LIST, hInst, NULL);
        SendMessageW(hList, WM_SETFONT, (WPARAM)hFont, TRUE);
        for (const auto& p : g_gamePaths)
            SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)p.c_str());
        for (int idx = 0; idx < (int)g_gamePaths.size(); idx++)
        {
            if (g_gamePaths[idx] == g_defaultGamePath)
            {
                SendMessageW(hList, LB_SETCURSEL, idx, 0);
                break;
            }
        }
        y += 88;

        int smallBtnW = 75;
        int smallBtnH = 26;
        int smallGap = 6;

        HWND hAutoDetect = CreateWindowW(L"BUTTON", i18n.settingsAutoDetect,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            marginX, y, 140, smallBtnH,
            hWnd, (HMENU)IDC_SETTINGS_AUTO_DETECT, hInst, NULL);
        SendMessageW(hAutoDetect, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hAdd = CreateWindowW(L"BUTTON", i18n.btnAddPath,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            marginX + 140 + smallGap, y, smallBtnW, smallBtnH,
            hWnd, (HMENU)IDC_SETTINGS_ADD_PATH, hInst, NULL);
        SendMessageW(hAdd, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hRemove = CreateWindowW(L"BUTTON", i18n.btnRemovePath,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            marginX + 140 + smallBtnW + smallGap * 2, y, smallBtnW, smallBtnH,
            hWnd, (HMENU)IDC_SETTINGS_REMOVE_PATH, hInst, NULL);
        SendMessageW(hRemove, WM_SETFONT, (WPARAM)hFont, TRUE);
        y += smallBtnH + 12;

        CreateWindowW(L"STATIC", i18n.settingsUid,
            WS_CHILD | WS_VISIBLE,
            marginX, y, textW, 20,
            hWnd, NULL, hInst, NULL);
        y += 22;

        HWND hUidList = CreateWindowW(L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
            marginX, y, textW, 70,
            hWnd, (HMENU)IDC_SETTINGS_UID_LIST, hInst, NULL);
        SendMessageW(hUidList, WM_SETFONT, (WPARAM)hFont, TRUE);
        for (const auto& uid : g_uids)
            SendMessageW(hUidList, LB_ADDSTRING, 0, (LPARAM)uid.c_str());
        for (int idx = 0; idx < (int)g_uids.size(); idx++)
        {
            if (g_uids[idx] == g_defaultUid)
            {
                SendMessageW(hUidList, LB_SETCURSEL, idx, 0);
                break;
            }
        }
        y += 78;

        int uidInputW = 120;
        HWND hUidInput = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            marginX, y, uidInputW, smallBtnH,
            hWnd, (HMENU)IDC_SETTINGS_UID_INPUT, hInst, NULL);
        SendMessageW(hUidInput, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hAddUid = CreateWindowW(L"BUTTON", i18n.btnAddPath,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            marginX + uidInputW + smallGap, y, smallBtnW, smallBtnH,
            hWnd, (HMENU)IDC_SETTINGS_ADD_UID, hInst, NULL);
        SendMessageW(hAddUid, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hRemoveUid = CreateWindowW(L"BUTTON", i18n.btnRemovePath,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            marginX + uidInputW + smallBtnW + smallGap * 2, y, smallBtnW, smallBtnH,
            hWnd, (HMENU)IDC_SETTINGS_REMOVE_UID, hInst, NULL);
        SendMessageW(hRemoveUid, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hRescan = CreateWindowW(L"BUTTON", i18n.btnRescanUID,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            marginX + uidInputW + smallBtnW * 2 + smallGap * 3, y, smallBtnW, smallBtnH,
            hWnd, (HMENU)IDC_SETTINGS_RESCAN_UID, hInst, NULL);
        SendMessageW(hRescan, WM_SETFONT, (WPARAM)hFont, TRUE);
        y += smallBtnH + 10;

        int btnW = 90;
        int btnH = 30;
        int btnGap = 12;
        int totalBtnW = btnW * 2 + btnGap;
        int btnStartX = marginX + (textW - totalBtnW) / 2;

        HWND hSave = CreateWindowW(L"BUTTON", i18n.settingsSave,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            btnStartX, y, btnW, btnH,
            hWnd, (HMENU)IDC_SETTINGS_SAVE, hInst, NULL);
        SendMessageW(hSave, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hCancel = CreateWindowW(L"BUTTON", i18n.settingsCancel,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            btnStartX + btnW + btnGap, y, btnW, btnH,
            hWnd, (HMENU)IDC_SETTINGS_CANCEL, hInst, NULL);
        SendMessageW(hCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

        RECT rcWindow = {0, 0, textW + marginX * 2, y + btnH + 16};
        AdjustWindowRect(&rcWindow, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
        int dlgW = rcWindow.right - rcWindow.left;
        int dlgH = rcWindow.bottom - rcWindow.top;

        RECT rcParent;
        GetWindowRect(((LPCREATESTRUCT)lParam)->hwndParent, &rcParent);
        int x = rcParent.left + ((rcParent.right - rcParent.left) - dlgW) / 2;
        int yPos = rcParent.top + ((rcParent.bottom - rcParent.top) - dlgH) / 2;

        SetWindowPos(hWnd, NULL, x, yPos, dlgW, dlgH, SWP_NOZORDER);

        {
            auto scanned = ScanUIDs();
            if (!scanned.empty())
            {
                SendMessageW(hUidList, LB_RESETCONTENT, 0, 0);
                for (const auto& uid : scanned)
                    SendMessageW(hUidList, LB_ADDSTRING, 0, (LPARAM)uid.c_str());
                for (int idx = 0; idx < (int)scanned.size(); idx++)
                {
                    if (scanned[idx] == g_defaultUid)
                    {
                        SendMessageW(hUidList, LB_SETCURSEL, idx, 0);
                        break;
                    }
                }
            }
        }

        return 0;
    }

    case WM_COMMAND:
    {
        WORD id = LOWORD(wParam);
        if (HIWORD(wParam) == BN_CLICKED)
        {
            const auto& i18n = GetI18N();
            HWND hList = GetDlgItem(hWnd, IDC_SETTINGS_PATH_LIST);
            HWND hUidList = GetDlgItem(hWnd, IDC_SETTINGS_UID_LIST);

            switch (id)
            {
            case IDC_SETTINGS_AUTO_DETECT:
            {
                auto detected = DetectGamePaths();
                SendMessageW(hList, LB_RESETCONTENT, 0, 0);
                for (const auto& p : detected)
                    SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)p.c_str());
                if (!detected.empty())
                    MessageBoxW(hWnd, i18n.settingsDetected, i18n.msgBoxHint, MB_OK | MB_ICONINFORMATION);
                else
                    MessageBoxW(hWnd, i18n.settingsNoPath, i18n.msgBoxHint, MB_OK | MB_ICONWARNING);
                break;
            }
            case IDC_SETTINGS_ADD_PATH:
            {
                BROWSEINFOW bi = {0};
                bi.hwndOwner = hWnd;
                bi.lpszTitle = L"";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
                if (pidl)
                {
                    WCHAR folderPath[MAX_PATH];
                    if (SHGetPathFromIDListW(pidl, folderPath))
                    {
                        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)folderPath);
                    }
                    CoTaskMemFree(pidl);
                }
                break;
            }
            case IDC_SETTINGS_REMOVE_PATH:
            {
                int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR)
                    SendMessageW(hList, LB_DELETESTRING, sel, 0);
                break;
            }
            case IDC_SETTINGS_ADD_UID:
            {
                HWND hInput = GetDlgItem(hWnd, IDC_SETTINGS_UID_INPUT);
                WCHAR buf[256] = {0};
                GetWindowTextW(hInput, buf, 256);
                if (buf[0])
                {
                    SendMessageW(hUidList, LB_ADDSTRING, 0, (LPARAM)buf);
                    SetWindowTextW(hInput, L"");
                }
                break;
            }
            case IDC_SETTINGS_REMOVE_UID:
            {
                int sel = (int)SendMessageW(hUidList, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR)
                    SendMessageW(hUidList, LB_DELETESTRING, sel, 0);
                break;
            }
            case IDC_SETTINGS_RESCAN_UID:
            {
                std::vector<std::wstring> tempPaths;
                int count = (int)SendMessageW(hList, LB_GETCOUNT, 0, 0);
                for (int i = 0; i < count; i++)
                {
                    int len = (int)SendMessageW(hList, LB_GETTEXTLEN, i, 0);
                    std::wstring s(len + 1, L'\0');
                    SendMessageW(hList, LB_GETTEXT, i, (LPARAM)&s[0]);
                    s.resize(len);
                    tempPaths.push_back(s);
                }
                std::swap(g_gamePaths, tempPaths);
                auto scanned = ScanUIDs();
                std::swap(g_gamePaths, tempPaths);

                SendMessageW(hUidList, LB_RESETCONTENT, 0, 0);
                for (const auto& uid : scanned)
                    SendMessageW(hUidList, LB_ADDSTRING, 0, (LPARAM)uid.c_str());
                break;
            }
            case IDC_SETTINGS_SAVE:
            {
                g_gamePaths.clear();
                int count = (int)SendMessageW(hList, LB_GETCOUNT, 0, 0);
                for (int i = 0; i < count; i++)
                {
                    int len = (int)SendMessageW(hList, LB_GETTEXTLEN, i, 0);
                    std::wstring s(len + 1, L'\0');
                    SendMessageW(hList, LB_GETTEXT, i, (LPARAM)&s[0]);
                    s.resize(len);
                    if (!s.empty()) g_gamePaths.push_back(s);
                }

                g_uids.clear();
                int uidCount = (int)SendMessageW(hUidList, LB_GETCOUNT, 0, 0);
                for (int i = 0; i < uidCount; i++)
                {
                    int len = (int)SendMessageW(hUidList, LB_GETTEXTLEN, i, 0);
                    std::wstring s(len + 1, L'\0');
                    SendMessageW(hUidList, LB_GETTEXT, i, (LPARAM)&s[0]);
                    s.resize(len);
                    if (!s.empty()) g_uids.push_back(s);
                }

                int selPath = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
                if (selPath != LB_ERR)
                {
                    int len = (int)SendMessageW(hList, LB_GETTEXTLEN, selPath, 0);
                    g_defaultGamePath.resize(len + 1);
                    SendMessageW(hList, LB_GETTEXT, selPath, (LPARAM)&g_defaultGamePath[0]);
                    g_defaultGamePath.resize(len);
                }
                else
                    g_defaultGamePath.clear();

                int selUid = (int)SendMessageW(hUidList, LB_GETCURSEL, 0, 0);
                if (selUid != LB_ERR)
                {
                    int len = (int)SendMessageW(hUidList, LB_GETTEXTLEN, selUid, 0);
                    g_defaultUid.resize(len + 1);
                    SendMessageW(hUidList, LB_GETTEXT, selUid, (LPARAM)&g_defaultUid[0]);
                    g_defaultUid.resize(len);
                }
                else
                    g_defaultUid.clear();

                SaveConfig();
                snapPaths = g_gamePaths;
                snapUids = g_uids;
                snapDefPath = g_defaultGamePath;
                snapDefUid = g_defaultUid;
                DestroyWindow(hWnd);
                break;
            }
            case IDC_SETTINGS_CANCEL:
                DestroyWindow(hWnd);
                break;
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

        if (snapPaths != g_gamePaths || snapUids != g_uids
            || snapDefPath != g_defaultGamePath || snapDefUid != g_defaultUid)
        {
            g_gamePaths = snapPaths;
            g_uids = snapUids;
            g_defaultGamePath = snapDefPath;
            g_defaultUid = snapDefUid;
        }
        EnableWindow(g_hWndMain, TRUE);
        SetForegroundWindow(g_hWndMain);
        break;
    }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void OnSettings()
{
    LoadConfig();

    const auto& i18n = GetI18N();
    WNDCLASSEXW wcSettings = {0};
    wcSettings.cbSize = sizeof(wcSettings);
    wcSettings.style = CS_HREDRAW | CS_VREDRAW;
    wcSettings.lpfnWndProc = SettingsDlgProc;
    wcSettings.hInstance = g_hInst;
    wcSettings.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcSettings.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcSettings.lpszClassName = L"SettingsDialog";
    wcSettings.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_APPICON));
    RegisterClassExW(&wcSettings);

    HWND hDlg = CreateWindowExW(0, L"SettingsDialog", i18n.settingsTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 480, CW_USEDEFAULT,
        g_hWndMain, NULL, g_hInst, NULL);

    if (hDlg)
    {
        EnableWindow(g_hWndMain, FALSE);
        ShowWindow(hDlg, SW_SHOW);
        UpdateWindow(hDlg);
    }
}
