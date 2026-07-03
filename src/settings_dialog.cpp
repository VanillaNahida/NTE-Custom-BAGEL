#include "settings_dialog.h"
#include "globals.h"
#include "config_manager.h"
#include "game_detector.h"
#include "resource.h"
#include "i18n.h"
#include <shlobj.h>

static std::wstring MakePathDisplay(const std::wstring& path, int region)
{
    if (region == REGION_CN) return L"[CN] " + path;
    if (region == REGION_GLOBAL) return L"[Global] " + path;
    return path;
}

static std::wstring MakeUidDisplay(const std::wstring& uid, int region)
{
    if (region == REGION_CN) return L"[CN] " + uid;
    if (region == REGION_GLOBAL) return L"[Global] " + uid;
    return uid;
}

static std::wstring StripRegionPrefix(const std::wstring& display)
{
    if (display.find(L"[CN] ") == 0)
        return display.substr(5);
    if (display.find(L"[Global] ") == 0)
        return display.substr(9);
    return display;
}

LRESULT CALLBACK SettingsDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static std::vector<std::wstring> snapPaths;
    static std::vector<int> snapPathRegions;
    static std::vector<std::wstring> snapUids;
    static std::vector<int> snapUidRegions;
    static std::wstring snapDefPath;
    static std::wstring snapDefUid;

    switch (msg)
    {
    case WM_CREATE:
    {
        const auto& i18n = GetI18N();
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        snapPaths = g_gamePaths;
        snapPathRegions = g_gamePathRegions;
        snapUids = g_uids;
        snapUidRegions = g_uidRegions;
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

        HWND hPathLabel = CreateWindowW(L"STATIC", i18n.settingsGamePath,
            WS_CHILD | WS_VISIBLE,
            marginX, y, textW, 20,
            hWnd, NULL, hInst, NULL);
        SendMessageW(hPathLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        y += 22;

        HWND hList = CreateWindowW(L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
            marginX, y, textW, 80,
            hWnd, (HMENU)IDC_SETTINGS_PATH_LIST, hInst, NULL);
        SendMessageW(hList, WM_SETFONT, (WPARAM)hFont, TRUE);
        for (size_t i = 0; i < g_gamePaths.size(); i++)
        {
            std::wstring display = MakePathDisplay(g_gamePaths[i],
                i < g_gamePathRegions.size() ? g_gamePathRegions[i] : REGION_UNKNOWN);
            SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
        }
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

        HWND hUidLabel = CreateWindowW(L"STATIC", i18n.settingsUid,
            WS_CHILD | WS_VISIBLE,
            marginX, y, textW, 20,
            hWnd, NULL, hInst, NULL);
        SendMessageW(hUidLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        y += 22;

        HWND hUidList = CreateWindowW(L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
            marginX, y, textW, 70,
            hWnd, (HMENU)IDC_SETTINGS_UID_LIST, hInst, NULL);
        SendMessageW(hUidList, WM_SETFONT, (WPARAM)hFont, TRUE);
        for (size_t i = 0; i < g_uids.size(); i++)
        {
            std::wstring display = MakeUidDisplay(g_uids[i],
                i < g_uidRegions.size() ? g_uidRegions[i] : REGION_UNKNOWN);
            SendMessageW(hUidList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
        }
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
                for (size_t i = 0; i < scanned.size(); i++)
                {
                    std::wstring display = MakeUidDisplay(scanned[i],
                        i < g_uidRegions.size() ? g_uidRegions[i] : REGION_UNKNOWN);
                    SendMessageW(hUidList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
                }
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
                for (size_t i = 0; i < detected.size(); i++)
                {
                    std::wstring display = MakePathDisplay(detected[i],
                        i < g_gamePathRegions.size() ? g_gamePathRegions[i] : REGION_UNKNOWN);
                    SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
                }
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
                        int region = DetectGameRegion(folderPath);
                        std::wstring display = MakePathDisplay(folderPath, region);
                        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
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
                    // Try to detect region from existing game paths
                    int region = REGION_UNKNOWN;
                    for (size_t pi = 0; pi < g_gamePaths.size(); pi++)
                    {
                        std::wstring uidDir = g_gamePaths[pi] + L"\\Client\\WindowsNoEditor\\Selfie\\" + buf;
                        DWORD attr = GetFileAttributesW(uidDir.c_str());
                        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
                        {
                            region = (pi < g_gamePathRegions.size()) ? g_gamePathRegions[pi] : REGION_UNKNOWN;
                            break;
                        }
                    }
                    std::wstring display = MakeUidDisplay(buf, region);
                    SendMessageW(hUidList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
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
                std::vector<int> tempPathRegions;
                int count = (int)SendMessageW(hList, LB_GETCOUNT, 0, 0);
                for (int i = 0; i < count; i++)
                {
                    int len = (int)SendMessageW(hList, LB_GETTEXTLEN, i, 0);
                    std::wstring s(len + 1, L'\0');
                    SendMessageW(hList, LB_GETTEXT, i, (LPARAM)&s[0]);
                    s.resize(len);
                    std::wstring raw = StripRegionPrefix(s);
                    if (!raw.empty())
                    {
                        tempPaths.push_back(raw);
                        tempPathRegions.push_back(DetectGameRegion(raw));
                    }
                }
                std::swap(g_gamePaths, tempPaths);
                std::swap(g_gamePathRegions, tempPathRegions);
                auto scanned = ScanUIDs();
                std::swap(g_gamePaths, tempPaths);
                std::swap(g_gamePathRegions, tempPathRegions);

                SendMessageW(hUidList, LB_RESETCONTENT, 0, 0);
                for (size_t i = 0; i < scanned.size(); i++)
                {
                    std::wstring display = MakeUidDisplay(scanned[i],
                        i < g_uidRegions.size() ? g_uidRegions[i] : REGION_UNKNOWN);
                    SendMessageW(hUidList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
                }
                break;
            }
            case IDC_SETTINGS_SAVE:
            {
                g_gamePaths.clear();
                g_gamePathRegions.clear();
                int count = (int)SendMessageW(hList, LB_GETCOUNT, 0, 0);
                for (int i = 0; i < count; i++)
                {
                    int len = (int)SendMessageW(hList, LB_GETTEXTLEN, i, 0);
                    std::wstring s(len + 1, L'\0');
                    SendMessageW(hList, LB_GETTEXT, i, (LPARAM)&s[0]);
                    s.resize(len);
                    std::wstring raw = StripRegionPrefix(s);
                    if (!raw.empty())
                    {
                        g_gamePaths.push_back(raw);
                        g_gamePathRegions.push_back(DetectGameRegion(raw));
                    }
                }

                g_uids.clear();
                g_uidRegions.clear();
                int uidCount = (int)SendMessageW(hUidList, LB_GETCOUNT, 0, 0);
                for (int i = 0; i < uidCount; i++)
                {
                    int len = (int)SendMessageW(hUidList, LB_GETTEXTLEN, i, 0);
                    std::wstring s(len + 1, L'\0');
                    SendMessageW(hUidList, LB_GETTEXT, i, (LPARAM)&s[0]);
                    s.resize(len);
                    std::wstring raw = StripRegionPrefix(s);
                    if (!raw.empty())
                    {
                        g_uids.push_back(raw);
                        // Try to find which game path's Selfie dir contains this UID
                        int uidRegion = REGION_UNKNOWN;
                        for (size_t pi = 0; pi < g_gamePaths.size(); pi++)
                        {
                            std::wstring uidDir = g_gamePaths[pi] + L"\\Client\\WindowsNoEditor\\Selfie\\" + raw;
                            DWORD attr = GetFileAttributesW(uidDir.c_str());
                            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
                            {
                                uidRegion = g_gamePathRegions[pi];
                                break;
                            }
                        }
                        g_uidRegions.push_back(uidRegion);
                    }
                }

                int selPath = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
                if (selPath != LB_ERR)
                {
                    int len = (int)SendMessageW(hList, LB_GETTEXTLEN, selPath, 0);
                    std::wstring selStr(len + 1, L'\0');
                    SendMessageW(hList, LB_GETTEXT, selPath, (LPARAM)&selStr[0]);
                    selStr.resize(len);
                    g_defaultGamePath = StripRegionPrefix(selStr);
                }
                else
                    g_defaultGamePath.clear();

                int selUid = (int)SendMessageW(hUidList, LB_GETCURSEL, 0, 0);
                if (selUid != LB_ERR)
                {
                    int len = (int)SendMessageW(hUidList, LB_GETTEXTLEN, selUid, 0);
                    std::wstring selUidStr(len + 1, L'\0');
                    SendMessageW(hUidList, LB_GETTEXT, selUid, (LPARAM)&selUidStr[0]);
                    selUidStr.resize(len);
                    g_defaultUid = StripRegionPrefix(selUidStr);
                }
                else
                    g_defaultUid.clear();

                SaveConfig();
                snapPaths = g_gamePaths;
                snapPathRegions = g_gamePathRegions;
                snapUids = g_uids;
                snapUidRegions = g_uidRegions;
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
            g_gamePathRegions = snapPathRegions;
            g_uids = snapUids;
            g_uidRegions = snapUidRegions;
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
