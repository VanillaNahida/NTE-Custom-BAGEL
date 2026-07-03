#define _WIN32_IE 0x0600
#include "version.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <string>
#include <fstream>
#include <vector>
#include "resource.h"
#include "i18n.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")

#pragma comment(linker, "/manifestdependency:\"type='win32' \
    name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
    processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' \
    language='*'\"")

#define IDC_COMBO_RESOLUTION   1001
#define IDC_BTN_SELECT_IMAGE   1002
#define IDC_BTN_LAUNCH         1003
#define IDC_STATIC_RESOLUTION  1004
#define IDC_STATIC_STATUS      1005
#define IDC_STATIC_PATH        1006
#define IDC_BTN_ABOUT          1007
#define IDC_BTN_OPEN_FOLDER    1008
#define IDC_BTN_SETTINGS       1009

#define IDC_ABOUT_CLOSE        2001
#define IDC_SETTINGS_UID_EDIT  2002
#define IDC_SETTINGS_UID_LIST  2010
#define IDC_SETTINGS_AUTO_DETECT 2003
#define IDC_SETTINGS_SAVE      2004
#define IDC_SETTINGS_CANCEL    2005
#define IDC_SETTINGS_PATH_LIST  2006
#define IDC_SETTINGS_ADD_PATH   2007
#define IDC_SETTINGS_REMOVE_PATH 2008
#define IDC_SETTINGS_RESCAN_UID 2009
#define IDC_SETTINGS_ADD_UID    2011
#define IDC_SETTINGS_REMOVE_UID 2012
#define IDC_SETTINGS_UID_INPUT  2013

#define WM_USER_UPDATE_STATUS  (WM_USER + 100)
#define WM_LAUNCHER_EXITED     (WM_USER + 101)

WCHAR g_szBinDir[MAX_PATH];

// ====== AppConfig ======
std::vector<std::wstring> g_gamePaths;
std::vector<std::wstring> g_uids;
std::wstring g_defaultGamePath;
std::wstring g_defaultUid;
WCHAR g_szConfigPath[MAX_PATH];

// ====== CloudUpload DLL interface ======
struct CloudUploadResult {
    bool success;
    char imageHash[65];
    char errorMessage[512];
};
typedef CloudUploadResult (*CloudUpload_UploadImage_t)(const wchar_t*);
typedef void (*CloudUpload_CheckForUpdate_t)(HWND, const wchar_t*, const wchar_t*, const wchar_t*);

HMODULE g_hCloudUploadDll = nullptr;
CloudUpload_UploadImage_t g_pfnUpload = nullptr;
CloudUpload_CheckForUpdate_t g_pfnCheckUpdate = nullptr;

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

HINSTANCE g_hInst;
HWND g_hWndMain;
HWND g_hComboResolution;
HWND g_hBtnSelect;
HWND g_hBtnLaunch;
HWND g_hBtnAbout;
HWND g_hBtnOpenFolder;
HWND g_hBtnSettings;
HWND g_hStaticStatus;
HWND g_hStaticPath;
HWND g_hWndPreview;

HANDLE g_hLauncherProcess = NULL;

HFONT g_hFont = NULL;
Gdiplus::Bitmap* g_pPreviewBitmap = NULL;

WCHAR g_szSelectedImage[MAX_PATH] = {0};
bool g_bImageLoaded = false;

void UpdatePreview();
DWORD WINAPI LauncherWatchThread(LPVOID lpParam);
LRESULT CALLBACK AboutDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingsDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void LoadConfig();
void SaveConfig();
std::vector<std::wstring> DetectGamePaths();
void OnOpenFolder();
void OnSettings();

ULONG_PTR g_gdiplusToken;

struct Resolution {
    const WCHAR* label;
    int width;
    int height;
};

const Resolution g_resolutions[] = {
    { L"720p  (1280 x 720)",   1280, 720  },
    { L"1080p (1920 x 1080)",  1920, 1080 },
    { L"1440p (2560 x 1440)",  2560, 1440 },
};
const int g_resolutionCount = 3;
int g_selectedResolution = 1;

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT num = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    Gdiplus::ImageCodecInfo* pCodecInfo = (Gdiplus::ImageCodecInfo*)malloc(size);
    if (!pCodecInfo) return -1;

    Gdiplus::GetImageEncoders(num, size, pCodecInfo);
    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pCodecInfo[j].Clsid;
            free(pCodecInfo);
            return j;
        }
    }
    free(pCodecInfo);
    return -1;
}

bool EnsureTargetDirectory()
{
    DWORD attr = GetFileAttributesW(g_szBinDir);
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        return CreateDirectoryW(g_szBinDir, NULL) != 0;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool ProcessAndSaveImage(const WCHAR* srcPath, int targetWidth, int targetHeight)
{
    Gdiplus::Bitmap srcBitmap(srcPath);
    if (srcBitmap.GetLastStatus() != Gdiplus::Ok)
        return false;

    Gdiplus::Bitmap destBitmap(targetWidth, targetHeight, PixelFormat32bppARGB);
    Gdiplus::Graphics graphics(&destBitmap);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    graphics.DrawImage(&srcBitmap, 0, 0, targetWidth, targetHeight);

    CLSID pngClsid;
    if (GetEncoderClsid(L"image/png", &pngClsid) < 0)
        return false;

    if (!EnsureTargetDirectory())
        return false;

    WCHAR targetPath[MAX_PATH];
    wsprintfW(targetPath, L"%sreplace.png", g_szBinDir);

    Gdiplus::Status status = destBitmap.Save(targetPath, &pngClsid, NULL);
    return status == Gdiplus::Ok;
}

void UpdateStatusText(const WCHAR* text)
{
    SetWindowTextW(g_hStaticStatus, text);
}

void UpdatePathText()
{
    const auto& i18n = GetI18N();
    if (g_bImageLoaded)
    {
        WCHAR display[MAX_PATH + 64];
        wsprintfW(display, i18n.pathSelected, g_szSelectedImage);
        SetWindowTextW(g_hStaticPath, display);
    }
    else
    {
        SetWindowTextW(g_hStaticPath, i18n.pathNoImage);
    }
}

bool IsLauncherRunning()
{
    WCHAR launcherName[] = L"launcher.exe";
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W pe = {0};
    pe.dwSize = sizeof(pe);

    bool found = false;
    if (Process32FirstW(hSnapshot, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, launcherName) == 0)
            {
                found = true;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return found;
}

std::wstring GetAppVersion()
{
    return L"v" + std::wstring(APP_VERSION_W);
}

DWORD WINAPI LauncherWatchThread(LPVOID lpParam)
{
    HANDLE hProcess = (HANDLE)lpParam;
    WaitForSingleObject(hProcess, INFINITE);
    CloseHandle(hProcess);
    PostMessageW(g_hWndMain, WM_LAUNCHER_EXITED, 0, 0);
    return 0;
}

void LoadImageFromPath(const WCHAR* szPath)
{
    const auto& i18n = GetI18N();
    if (IsLauncherRunning())
    {
        MessageBoxW(g_hWndMain, i18n.msgLauncherRunning, i18n.msgBoxHint, MB_OK | MB_ICONWARNING);
        return;
    }

    wcscpy_s(g_szSelectedImage, MAX_PATH, szPath);
    g_bImageLoaded = true;
    UpdatePathText();

    if (g_pPreviewBitmap)
    {
        delete g_pPreviewBitmap;
        g_pPreviewBitmap = NULL;
    }

    const Resolution& res = g_resolutions[g_selectedResolution];

    if (ProcessAndSaveImage(g_szSelectedImage, res.width, res.height))
    {
        WCHAR msg[256];
        wsprintfW(msg, i18n.statusImageProcessed, res.width, res.height);
        UpdateStatusText(msg);
        UpdatePreview();

        // Check for update when user loads an image
        if (g_pfnCheckUpdate) {
            CreateThread(NULL, 0, [](LPVOID) -> DWORD {
                const auto& i = GetI18N();
                g_pfnCheckUpdate(g_hWndMain, APP_VERSION_W, i.updateTitle, i.updateMsg);
                return 0;
            }, NULL, 0, NULL);
        }
    }
    else
    {
        UpdateStatusText(i18n.statusProcessFailed);
    }
}

void OnSelectImage()
{
    WCHAR szFile[MAX_PATH] = {0};

    const auto& i18n = GetI18N();
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = GetFileDialogFilter();
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = i18n.fileDialogTitle;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileNameW(&ofn))
    {
        LoadImageFromPath(szFile);
    }
}

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

    // Upload succeeded
    char hashShort[13] = {};
    strncpy_s(hashShort, result.imageHash, 12);
    WCHAR statusMsg[256];
    wsprintfW(statusMsg, i18n.statusUploadSuccess, hashShort);
    UpdateStatusText(statusMsg);

    // Now launch the injector
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
    // Always upload before launching
    DoUploadAndLaunch();
}

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
            std::wstring verText = i18n.aboutVersionPrefix + GetAppVersion();
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

// ====== JSON helpers (simple manual serialization for AppConfig.json) ======
static std::wstring JsonEscape(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t ch : s)
    {
        if (ch == L'\\') out += L"\\\\";
        else if (ch == L'"') out += L"\\\"";
        else out += ch;
    }
    return out;
}

static std::wstring JsonUnescape(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++)
    {
        if (s[i] == L'\\' && i + 1 < s.size())
        {
            if (s[i + 1] == L'"') { out += L'"'; i++; }
            else if (s[i + 1] == L'\\') { out += L'\\'; i++; }
            else out += s[i];
        }
        else out += s[i];
    }
    return out;
}

static std::wstring VectorToJsonArray(const std::vector<std::wstring>& vec)
{
    std::wstring out = L"[";
    for (size_t i = 0; i < vec.size(); i++)
    {
        if (i > 0) out += L",";
        out += L"\"" + JsonEscape(vec[i]) + L"\"";
    }
    out += L"]";
    return out;
}

void SaveConfig()
{
    std::wstring json = L"{\"gamePaths\":";
    json += VectorToJsonArray(g_gamePaths);
    json += L",\"uids\":";
    json += VectorToJsonArray(g_uids);
    json += L",\"defaultGamePath\":\"";
    json += JsonEscape(g_defaultGamePath);
    json += L"\",\"defaultUid\":\"";
    json += JsonEscape(g_defaultUid);
    json += L"\"}";

    // Convert to UTF-8
    int len = WideCharToMultiByte(CP_UTF8, 0, json.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, json.c_str(), -1, &utf8[0], len, nullptr, nullptr);
    // Remove trailing null
    utf8.pop_back();

    std::ofstream f(g_szConfigPath, std::ios::binary);
    if (f.is_open())
    {
        f.write(utf8.c_str(), utf8.size());
    }
}

void LoadConfig()
{
    std::ifstream f(g_szConfigPath, std::ios::binary);
    if (!f.is_open()) return;

    std::string utf8((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    if (utf8.empty()) return;

    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wjson(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wjson[0], wlen);
    wjson.pop_back();

    auto extractArray = [&wjson](const std::wstring& key) -> std::vector<std::wstring>
    {
        std::vector<std::wstring> result;
        std::wstring searchKey = L"\"" + key + L"\"";

        size_t keyPos = wjson.find(searchKey);
        if (keyPos == std::wstring::npos) return result;

        size_t start = wjson.find(L'[', keyPos);
        if (start == std::wstring::npos) return result;

        size_t end = wjson.find(L']', start);
        if (end == std::wstring::npos) return result;

        // Parse items between [start+1, end)
        std::wstring content = wjson.substr(start + 1, end - start - 1);
        if (content.empty()) return result;

        size_t pos = 0;
        while (pos < content.size())
        {
            // Skip whitespace and commas
            while (pos < content.size() && (content[pos] == L' ' || content[pos] == L',' || content[pos] == L'\r' || content[pos] == L'\n'))
                pos++;

            if (pos >= content.size()) break;

            if (content[pos] == L'"')
            {
                pos++; // skip opening quote
                std::wstring item;
                while (pos < content.size())
                {
                    if (content[pos] == L'\\' && pos + 1 < content.size())
                    {
                        item += content[pos];
                        pos++;
                        item += content[pos];
                        pos++;
                    }
                    else if (content[pos] == L'"')
                    {
                        pos++;
                        break;
                    }
                    else
                    {
                        item += content[pos];
                        pos++;
                    }
                }
                result.push_back(JsonUnescape(item));
            }
        }
        return result;
    };

    g_gamePaths = extractArray(L"gamePaths");
    g_uids = extractArray(L"uids");

    auto extractString = [&wjson](const std::wstring& key) -> std::wstring
    {
        std::wstring searchKey = L"\"" + key + L"\"";
        size_t keyPos = wjson.find(searchKey);
        if (keyPos == std::wstring::npos) return L"";

        size_t valueStart = wjson.find(L'"', keyPos + searchKey.size());
        if (valueStart == std::wstring::npos) return L"";
        valueStart++;

        size_t valueEnd = wjson.find(L'"', valueStart);
        if (valueEnd == std::wstring::npos) return L"";

        std::wstring raw = wjson.substr(valueStart, valueEnd - valueStart);
        return JsonUnescape(raw);
    };

    g_defaultGamePath = extractString(L"defaultGamePath");
    g_defaultUid = extractString(L"defaultUid");
}

// ====== Registry: detect game paths ======
std::vector<std::wstring> DetectGamePaths()
{
    std::vector<std::wstring> paths;
    const wchar_t* regPaths[] = {
        L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\YH",
        L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\NTEGlobal"
    };

    for (const wchar_t* regPath : regPaths)
    {
        HKEY hKey = nullptr;
        LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
        if (result != ERROR_SUCCESS)
        {
            result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ, &hKey);
        }
        if (result == ERROR_SUCCESS)
        {
            WCHAR installPath[MAX_PATH] = {0};
            DWORD size = sizeof(installPath);
            DWORD type = REG_SZ;
            if (RegQueryValueExW(hKey, L"InstallLocation", nullptr, &type, (LPBYTE)installPath, &size) == ERROR_SUCCESS)
            {
                std::wstring path(installPath);
                // Remove trailing backslash if present
                while (!path.empty() && path.back() == L'\\')
                    path.pop_back();
                if (!path.empty())
                {
                    // Check if this path is already in the list
                    bool found = false;
                    for (const auto& p : paths)
                    {
                        if (_wcsicmp(p.c_str(), path.c_str()) == 0)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found) paths.push_back(path);
                }
            }
            RegCloseKey(hKey);
        }
    }
    return paths;
}

// ====== Open screenshots folder ======
void OnOpenFolder()
{
    const auto& i18n = GetI18N();

    if (g_gamePaths.empty() || g_uids.empty())
    {
        MessageBoxW(g_hWndMain, i18n.msgNoGamePath, i18n.msgBoxHint, MB_OK | MB_ICONWARNING);
        return;
    }

    // Use configured default path and UID, fallback to first entry
    std::wstring usePath = (!g_defaultGamePath.empty()) ? g_defaultGamePath : g_gamePaths[0];
    std::wstring useUid = (!g_defaultUid.empty()) ? g_defaultUid : g_uids[0];

    // Build Selfie path: gamePath\Client\WindowsNoEditor\Selfie\UID
    std::wstring selfiePath = usePath + L"\\Client\\WindowsNoEditor\\Selfie\\" + useUid;

    DWORD attr = GetFileAttributesW(selfiePath.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        ShellExecuteW(g_hWndMain, L"open", L"explorer", selfiePath.c_str(), NULL, SW_SHOWNORMAL);
    }
    else
    {
        // Fall back to parent Selfie folder
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

// ====== Scan UIDs from Selfie subfolders ======
std::vector<std::wstring> ScanUIDs()
{
    std::vector<std::wstring> uids;
    if (g_gamePaths.empty()) return uids;

    for (const auto& gamePath : g_gamePaths)
    {
        std::wstring selfieDir = gamePath + L"\\Client\\WindowsNoEditor\\Selfie\\*";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(selfieDir.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;

        do
        {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                wcscmp(fd.cFileName, L".") != 0 &&
                wcscmp(fd.cFileName, L"..") != 0)
            {
                // Deduplicate
                bool found = false;
                for (const auto& existing : uids)
                {
                    if (existing == fd.cFileName) { found = true; break; }
                }
                if (!found) uids.push_back(fd.cFileName);
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    return uids;
}

// ====== Settings dialog ======
LRESULT CALLBACK SettingsDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Per-dialog snapshot for cancel/rollback
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

        // Snapshot current config
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

        // Game path label
        CreateWindowW(L"STATIC", i18n.settingsGamePath,
            WS_CHILD | WS_VISIBLE,
            marginX, y, textW, 20,
            hWnd, NULL, hInst, NULL);
        y += 22;

        // Game path ListBox
        HWND hList = CreateWindowW(L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
            marginX, y, textW, 80,
            hWnd, (HMENU)IDC_SETTINGS_PATH_LIST, hInst, NULL);
        SendMessageW(hList, WM_SETFONT, (WPARAM)hFont, TRUE);
        for (const auto& p : g_gamePaths)
            SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)p.c_str());
        // Select default path
        for (int idx = 0; idx < (int)g_gamePaths.size(); idx++)
        {
            if (g_gamePaths[idx] == g_defaultGamePath)
            {
                SendMessageW(hList, LB_SETCURSEL, idx, 0);
                break;
            }
        }
        y += 88;

        // Add / Remove / Auto Detect buttons row
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

        // UID label
        CreateWindowW(L"STATIC", i18n.settingsUid,
            WS_CHILD | WS_VISIBLE,
            marginX, y, textW, 20,
            hWnd, NULL, hInst, NULL);
        y += 22;

        // UID ListBox
        HWND hUidList = CreateWindowW(L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
            marginX, y, textW, 70,
            hWnd, (HMENU)IDC_SETTINGS_UID_LIST, hInst, NULL);
        SendMessageW(hUidList, WM_SETFONT, (WPARAM)hFont, TRUE);
        for (const auto& uid : g_uids)
            SendMessageW(hUidList, LB_ADDSTRING, 0, (LPARAM)uid.c_str());
        // Select default UID
        for (int idx = 0; idx < (int)g_uids.size(); idx++)
        {
            if (g_uids[idx] == g_defaultUid)
            {
                SendMessageW(hUidList, LB_SETCURSEL, idx, 0);
                break;
            }
        }
        y += 78;

        // UID input + Add / Remove / Rescan buttons row
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

        // Save / Cancel buttons
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

        // Size the dialog
        RECT rcWindow = {0, 0, textW + marginX * 2, y + btnH + 16};
        AdjustWindowRect(&rcWindow, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
        int dlgW = rcWindow.right - rcWindow.left;
        int dlgH = rcWindow.bottom - rcWindow.top;

        RECT rcParent;
        GetWindowRect(((LPCREATESTRUCT)lParam)->hwndParent, &rcParent);
        int x = rcParent.left + ((rcParent.right - rcParent.left) - dlgW) / 2;
        int yPos = rcParent.top + ((rcParent.bottom - rcParent.top) - dlgH) / 2;

        SetWindowPos(hWnd, NULL, x, yPos, dlgW, dlgH, SWP_NOZORDER);

        // Auto-scan UIDs on open
        {
            auto scanned = ScanUIDs();
            if (!scanned.empty())
            {
                SendMessageW(hUidList, LB_RESETCONTENT, 0, 0);
                for (const auto& uid : scanned)
                    SendMessageW(hUidList, LB_ADDSTRING, 0, (LPARAM)uid.c_str());
                // Re-select default UID if still present
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
                // Use current ListBox paths for scanning
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
                // Read paths from ListBox
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

                // Read UIDs from UID ListBox
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

                // Save selected items as defaults
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
                // Update snapshot so WM_DESTROY doesn't roll back
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

        // Restore snapshot on cancel/close
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

void OnResolutionChanged()
{
    const auto& i18n = GetI18N();
    int sel = (int)SendMessageW(g_hComboResolution, CB_GETCURSEL, 0, 0);
    if (sel >= 0 && sel < g_resolutionCount)
    {
        g_selectedResolution = sel;

        if (g_bImageLoaded)
        {
            if (g_pPreviewBitmap)
            {
                delete g_pPreviewBitmap;
                g_pPreviewBitmap = NULL;
            }

            const Resolution& res = g_resolutions[g_selectedResolution];
            if (ProcessAndSaveImage(g_szSelectedImage, res.width, res.height))
            {
                WCHAR msg[256];
                wsprintfW(msg, i18n.statusImageReprocessed, res.width, res.height);
                UpdateStatusText(msg);
                UpdatePreview();
            }
            else
            {
                UpdateStatusText(i18n.statusReprocessFailed);
            }
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

    // Dynamic button width based on text
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

void TryLoadExistingImage()
{
    WCHAR targetPath[MAX_PATH];
    wsprintfW(targetPath, L"%sreplace.png", g_szBinDir);

    if (GetFileAttributesW(targetPath) != INVALID_FILE_ATTRIBUTES)
    {
        UpdatePreview();
        UpdateStatusText(GetI18N().statusLoadedExisting);
    }
}

void UpdatePreview()
{
    if (g_pPreviewBitmap)
    {
        delete g_pPreviewBitmap;
        g_pPreviewBitmap = NULL;
    }

    WCHAR targetPath[MAX_PATH];
    wsprintfW(targetPath, L"%sreplace.png", g_szBinDir);
    g_pPreviewBitmap = Gdiplus::Bitmap::FromFile(targetPath);

    if (g_hWndPreview)
        InvalidateRect(g_hWndPreview, NULL, TRUE);
}

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

    // Load CloudUpload DLL (required)
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
        // Auto-detect game paths if none configured
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

    // Show default folder hint
    {
        const auto& i18n = GetI18N();
        UpdateStatusText(i18n.statusDefaultFolderHint);
    }

    TryLoadExistingImage();

    // Check for updates asynchronously on startup
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
