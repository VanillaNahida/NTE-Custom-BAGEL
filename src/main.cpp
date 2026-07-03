#define _WIN32_IE 0x0600
#include "version.h"
#include "globals.h"
#include "config_manager.h"
#include "game_detector.h"
#include "image_processor.h"
#include "about_dialog.h"
#include "settings_dialog.h"
#include "preview_window.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include "resource.h"
#include "i18n.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")

#pragma comment(linker, "/manifestdependency:\"type='win32' \
    name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
    processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' \
    language='*'\"")

bool InitBinDir()
{
    WCHAR szExePath[MAX_PATH];
    GetModuleFileNameW(NULL, szExePath, MAX_PATH);
    WCHAR* pLastSlash = wcsrchr(szExePath, L'\\');
    if (pLastSlash)
        *(pLastSlash + 1) = L'\0';
    wsprintfW(g_szBinDir, L"%sbin\\", szExePath);

    WCHAR launcherPath[MAX_PATH];
    wsprintfW(launcherPath, L"%slauncher.exe", g_szBinDir);
    if (GetFileAttributesW(launcherPath) == INVALID_FILE_ATTRIBUTES)
    {
        const auto& i18n = GetI18N();
        MessageBoxW(NULL, i18n.msgLauncherNotFound, i18n.msgBoxError, MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

DWORD WINAPI LauncherWatchThread(LPVOID lpParam)
{
    HANDLE hProcess = (HANDLE)lpParam;
    WaitForSingleObject(hProcess, INFINITE);
    CloseHandle(hProcess);
    PostMessageW(g_hWndMain, WM_LAUNCHER_EXITED, 0, 0);
    return 0;
}

// ====== Cloud upload & launch (云控) ======
void DoUploadAndLaunch()
{
    const auto& i18n = GetI18N();
    if (!g_pfnUpload) {
        UpdateStatusText(i18n.statusModuleNotLoaded);
        return;
    }

    WCHAR targetPath[MAX_PATH];
    wsprintfW(targetPath, L"%sreplace.png", g_szBinDir);

    UpdateStatusText(i18n.statusUploading);
    EnableWindow(g_hBtnLaunch, FALSE);

    CloudUploadResult result = g_pfnUpload(targetPath);

    if (!result.success) {
        int len = MultiByteToWideChar(CP_UTF8, 0, result.errorMessage, -1, nullptr, 0);
        WCHAR* wideMsg = new WCHAR[len + 64];
        MultiByteToWideChar(CP_UTF8, 0, result.errorMessage, -1, wideMsg, len);
        WCHAR displayMsg[640];
        wsprintfW(displayMsg, i18n.statusUploadFailed, wideMsg);
        delete[] wideMsg;
        UpdateStatusText(displayMsg);
        EnableWindow(g_hBtnLaunch, TRUE);
        return;
    }

    char hashShort[13] = {};
    strncpy_s(hashShort, result.imageHash, 12);
    WCHAR statusMsg[256];
    wsprintfW(statusMsg, i18n.statusUploadSuccess, hashShort);
    UpdateStatusText(statusMsg);

    WCHAR launcherPath[MAX_PATH];
    wsprintfW(launcherPath, L"%slauncher.exe", g_szBinDir);

    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = g_hWndMain;
    sei.lpVerb = L"runas";
    sei.lpFile = launcherPath;
    sei.lpDirectory = g_szBinDir;
    sei.nShow = SW_SHOWNORMAL;

    if (ShellExecuteExW(&sei))
    {
        WCHAR msg[320];
        wsprintfW(msg, i18n.statusLauncherStarted, hashShort);
        UpdateStatusText(msg);
        EnableWindow(g_hBtnLaunch, FALSE);
        if (sei.hProcess)
        {
            g_hLauncherProcess = sei.hProcess;
            HANDLE hThread = CreateThread(NULL, 0, LauncherWatchThread, sei.hProcess, 0, NULL);
            if (hThread)
                CloseHandle(hThread);
        }
    }
    else
    {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED)
        {
            UpdateStatusText(i18n.statusUacCancelled);
        }
        else
        {
            WCHAR msg[256];
            wsprintfW(msg, i18n.statusLaunchFailed, err);
            UpdateStatusText(msg);
        }
        EnableWindow(g_hBtnLaunch, TRUE);
    }
}

void OnLaunchInjector()
{
    DoUploadAndLaunch();
}

void OnOpenFolder()
{
    const auto& i18n = GetI18N();

    if (g_gamePaths.empty() || g_uids.empty())
    {
        MessageBoxW(g_hWndMain, i18n.msgNoGamePath, i18n.msgBoxHint, MB_OK | MB_ICONWARNING);
        return;
    }

    std::wstring usePath = (!g_defaultGamePath.empty()) ? g_defaultGamePath : g_gamePaths[0];
    std::wstring useUid = (!g_defaultUid.empty()) ? g_defaultUid : g_uids[0];

    std::wstring selfiePath = usePath + L"\\Client\\WindowsNoEditor\\Selfie\\" + useUid;

    DWORD attr = GetFileAttributesW(selfiePath.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        ShellExecuteW(g_hWndMain, L"open", L"explorer", selfiePath.c_str(), NULL, SW_SHOWNORMAL);
    }
    else
    {
        std::wstring fallbackPath = g_gamePaths[0] + L"\\Client\\WindowsNoEditor\\Selfie";
        attr = GetFileAttributesW(fallbackPath.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
        {
            ShellExecuteW(g_hWndMain, L"open", L"explorer", fallbackPath.c_str(), NULL, SW_SHOWNORMAL);
        }
        else
        {
            MessageBoxW(g_hWndMain, i18n.msgFolderNotExist, i18n.msgBoxHint, MB_OK | MB_ICONWARNING);
        }
    }
}

void LayoutControls(int clientWidth, int clientHeight)
{
    const int margin = 15;
    const int rowHeight = 28;
    const int btnHeight = 32;
    const int comboWidth = 260;
    const int gap = 8;

    int y = margin;

    SetWindowPos(GetDlgItem(g_hWndMain, IDC_STATIC_RESOLUTION), NULL,
        margin, y + 4, 80, rowHeight, SWP_NOZORDER);
    SetWindowPos(g_hComboResolution, NULL,
        margin + 85, y, comboWidth, rowHeight * 8, SWP_NOZORDER);

    y += rowHeight + gap;

    const auto& i18n = GetI18N();
    const int btnSpacing = 6;
    const wchar_t* btnTexts[] = {
        i18n.btnSelectImage,
        i18n.btnLaunch,
        i18n.btnOpenFolder,
        i18n.btnSettings,
        i18n.btnAbout
    };
    const int btnTextCount = sizeof(btnTexts) / sizeof(btnTexts[0]);
    int btnWidth = 60;
    HDC hdc = GetDC(g_hWndMain);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_hFont);
    for (int i = 0; i < btnTextCount; i++)
    {
        SIZE sz;
        GetTextExtentPoint32W(hdc, btnTexts[i], (int)wcslen(btnTexts[i]), &sz);
        int w = sz.cx + 14;
        if (w > btnWidth) btnWidth = w;
    }
    SelectObject(hdc, oldFont);
    ReleaseDC(g_hWndMain, hdc);

    int totalButtonsWidth = btnWidth * 5 + btnSpacing * 4;
    int btnStartX = (clientWidth - totalButtonsWidth) / 2;
    if (btnStartX < margin) btnStartX = margin;

    SetWindowPos(g_hBtnSelect, NULL,
        btnStartX, y, btnWidth, btnHeight, SWP_NOZORDER);
    SetWindowPos(g_hBtnLaunch, NULL,
        btnStartX + btnWidth + btnSpacing, y, btnWidth, btnHeight, SWP_NOZORDER);
    SetWindowPos(g_hBtnOpenFolder, NULL,
        btnStartX + (btnWidth + btnSpacing) * 2, y, btnWidth, btnHeight, SWP_NOZORDER);
    SetWindowPos(g_hBtnSettings, NULL,
        btnStartX + (btnWidth + btnSpacing) * 3, y, btnWidth, btnHeight, SWP_NOZORDER);
    SetWindowPos(g_hBtnAbout, NULL,
        btnStartX + (btnWidth + btnSpacing) * 4, y, btnWidth, btnHeight, SWP_NOZORDER);

    y += btnHeight + gap;

    SetWindowPos(g_hStaticPath, NULL,
        margin, y, clientWidth - margin * 2, 20, SWP_NOZORDER);

    y += 22;

    int statusHeight = 20;
    int previewWidth = clientWidth - margin * 2;
    int previewHeight = previewWidth * 9 / 16;

    int availableTop = y;
    int availableBottom = clientHeight - margin - statusHeight - gap;
    int availableHeight = availableBottom - availableTop;
    if (previewHeight > availableHeight)
        previewHeight = availableHeight;

    int previewTop = availableTop + (availableHeight - previewHeight) / 2;

    SetWindowPos(g_hWndPreview, NULL,
        margin, previewTop, previewWidth, previewHeight, SWP_NOZORDER);

    SetWindowPos(g_hStaticStatus, NULL,
        margin, clientHeight - margin - statusHeight, clientWidth - margin * 2, statusHeight, SWP_NOZORDER);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        const auto& i18n = GetI18N();
        g_hWndMain = hWnd;

        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        NONCLIENTMETRICSW ncm = {0};
        ncm.cbSize = sizeof(ncm);
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
        {
            g_hFont = CreateFontIndirectW(&ncm.lfMessageFont);
        }
        if (!g_hFont)
            g_hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        CreateWindowW(L"STATIC", i18n.resolutionLabel,
            WS_CHILD | WS_VISIBLE,
            0, 0, 80, 24,
            hWnd, (HMENU)IDC_STATIC_RESOLUTION, hInst, NULL);

        g_hComboResolution = CreateWindowW(L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 200, 200,
            hWnd, (HMENU)IDC_COMBO_RESOLUTION, hInst, NULL);

        for (int i = 0; i < g_resolutionCount; i++)
        {
            SendMessageW(g_hComboResolution, CB_ADDSTRING, 0, (LPARAM)g_resolutions[i].label);
        }
        SendMessageW(g_hComboResolution, CB_SETCURSEL, g_selectedResolution, 0);

        g_hBtnSelect = CreateWindowW(L"BUTTON", i18n.btnSelectImage,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 140, 32,
            hWnd, (HMENU)IDC_BTN_SELECT_IMAGE, hInst, NULL);

        g_hBtnLaunch = CreateWindowW(L"BUTTON", i18n.btnLaunch,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 140, 32,
            hWnd, (HMENU)IDC_BTN_LAUNCH, hInst, NULL);

        g_hBtnAbout = CreateWindowW(L"BUTTON", i18n.btnAbout,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 140, 32,
            hWnd, (HMENU)IDC_BTN_ABOUT, hInst, NULL);

        g_hBtnOpenFolder = CreateWindowW(L"BUTTON", i18n.btnOpenFolder,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 140, 32,
            hWnd, (HMENU)IDC_BTN_OPEN_FOLDER, hInst, NULL);

        g_hBtnSettings = CreateWindowW(L"BUTTON", i18n.btnSettings,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 140, 32,
            hWnd, (HMENU)IDC_BTN_SETTINGS, hInst, NULL);

        g_hStaticPath = CreateWindowW(L"STATIC", i18n.pathNoImageHint,
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
            0, 0, 400, 20,
            hWnd, (HMENU)IDC_STATIC_PATH, hInst, NULL);

        g_hWndPreview = CreateWindowExW(WS_EX_CLIENTEDGE, L"PreviewWindow", NULL,
            WS_CHILD | WS_VISIBLE,
            0, 0, 400, 200,
            hWnd, NULL, hInst, NULL);

        g_hStaticStatus = CreateWindowW(L"STATIC", i18n.statusReady,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 400, 20,
            hWnd, (HMENU)IDC_STATIC_STATUS, hInst, NULL);

        SendMessageW(g_hComboResolution, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessageW(g_hBtnSelect, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessageW(g_hBtnLaunch, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessageW(g_hBtnAbout, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessageW(g_hBtnOpenFolder, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessageW(g_hBtnSettings, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessageW(g_hStaticPath, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessageW(g_hStaticStatus, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessageW(GetDlgItem(hWnd, IDC_STATIC_RESOLUTION), WM_SETFONT, (WPARAM)g_hFont, TRUE);

        return 0;
    }

    case WM_SIZE:
    {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        LayoutControls(width, height);
        return 0;
    }

    case WM_COMMAND:
    {
        WORD id = LOWORD(wParam);
        WORD code = HIWORD(wParam);

        switch (id)
        {
        case IDC_BTN_SELECT_IMAGE:
            if (code == BN_CLICKED)
                OnSelectImage();
            break;

        case IDC_BTN_LAUNCH:
            if (code == BN_CLICKED)
                OnLaunchInjector();
            break;

        case IDC_BTN_OPEN_FOLDER:
            if (code == BN_CLICKED)
                OnOpenFolder();
            break;

        case IDC_BTN_SETTINGS:
            if (code == BN_CLICKED)
                OnSettings();
            break;

        case IDC_BTN_ABOUT:
            if (code == BN_CLICKED)
                OnAbout();
            break;

        case IDC_COMBO_RESOLUTION:
            if (code == CBN_SELCHANGE)
                OnResolutionChanged();
            break;
        }
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetBkMode(hdcStatic, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }

    case WM_LAUNCHER_EXITED:
        g_hLauncherProcess = NULL;
        EnableWindow(g_hBtnLaunch, TRUE);
        UpdateStatusText(GetI18N().statusLauncherExited);
        break;

    case WM_DESTROY:
        if (g_pPreviewBitmap)
        {
            delete g_pPreviewBitmap;
            g_pPreviewBitmap = NULL;
        }
        if (g_hFont)
        {
            DeleteObject(g_hFont);
            g_hFont = NULL;
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    g_hInst = hInstance;

    // Parse --lang argument before any GetI18N() call
    {
        int argc;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv) {
            for (int i = 1; i < argc - 1; i++) {
                if (wcscmp(argv[i], L"--lang") == 0) {
                    if (_wcsicmp(argv[i + 1], L"zh-cn") == 0)
                        SetLanguageOverride(true);
                    else if (_wcsicmp(argv[i + 1], L"en-us") == 0)
                        SetLanguageOverride(false);
                    break;
                }
            }
            LocalFree(argv);
        }
    }

    // Load CloudUpload DLL (云控)
    WCHAR exeDir[MAX_PATH];
    GetModuleFileNameW(NULL, exeDir, MAX_PATH);
    WCHAR* pLastSlash = wcsrchr(exeDir, L'\\');
    if (pLastSlash) *(pLastSlash + 1) = L'\0';

    WCHAR dllPath[MAX_PATH];
    wsprintfW(dllPath, L"%sNTEUploadBase.dll", exeDir);

    g_hCloudUploadDll = LoadLibraryW(dllPath);
    if (!g_hCloudUploadDll) {
        const auto& i18n = GetI18N();
        MessageBoxW(NULL, i18n.msgDllNotFound, i18n.msgBoxError, MB_OK | MB_ICONERROR);
        return 1;
    }

    g_pfnUpload = (CloudUpload_UploadImage_t)GetProcAddress(g_hCloudUploadDll, "CloudUpload_UploadImage");
    if (!g_pfnUpload) {
        const auto& i18n = GetI18N();
        MessageBoxW(NULL, i18n.msgDllNoFunction, i18n.msgBoxError, MB_OK | MB_ICONERROR);
        FreeLibrary(g_hCloudUploadDll);
        return 1;
    }

    g_pfnCheckUpdate = (CloudUpload_CheckForUpdate_t)GetProcAddress(g_hCloudUploadDll, "CloudUpload_CheckForUpdate");

    if (!InitBinDir()) {
        FreeLibrary(g_hCloudUploadDll);
        return 1;
    }

    // Initialize config path and load
    {
        wsprintfW(g_szConfigPath, L"%sAppConfig.json", exeDir);
        LoadConfig();
        if (g_gamePaths.empty())
        {
            g_gamePaths = DetectGamePaths();
            if (!g_gamePaths.empty())
                SaveConfig();
        }
    }

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    INITCOMMONCONTROLSEX icc = {0};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_LINK_CLASS;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ImagePreprocessorWindow";

    RegisterClassExW(&wc);

    WNDCLASSEXW wcPrev = {0};
    wcPrev.cbSize = sizeof(wcPrev);
    wcPrev.style = CS_HREDRAW | CS_VREDRAW;
    wcPrev.lpfnWndProc = PreviewWndProc;
    wcPrev.hInstance = hInstance;
    wcPrev.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    wcPrev.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcPrev.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcPrev.lpszClassName = L"PreviewWindow";
    RegisterClassExW(&wcPrev);

    int windowW = 560;
    int windowH = 520;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - windowW) / 2;
    int posY = (screenH - windowH) / 2;

    HWND hWnd = CreateWindowExW(0, L"ImagePreprocessorWindow", GetI18N().windowTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY, windowW, windowH,
        NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        FreeLibrary(g_hCloudUploadDll);
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    {
        const auto& i18n = GetI18N();
        UpdateStatusText(i18n.statusDefaultFolderHint);
    }

    TryLoadExistingImage();

    // Check for updates asynchronously on startup (云控)
    if (g_pfnCheckUpdate) {
        CreateThread(NULL, 0, [](LPVOID) -> DWORD {
            const auto& i = GetI18N();
            g_pfnCheckUpdate(g_hWndMain, APP_VERSION_W, i.updateTitle, i.updateMsg);
            return 0;
        }, NULL, 0, NULL);
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Gdiplus::GdiplusShutdown(g_gdiplusToken);
    if (g_hCloudUploadDll)
        FreeLibrary(g_hCloudUploadDll);

    return (int)msg.wParam;
}
