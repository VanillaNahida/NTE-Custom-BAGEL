#include "notice_dialog.h"
#include "globals.h"
#include "config_manager.h"
#include "i18n.h"
#include "resource.h"
#include <ctime>
#include <string>

static HWND g_hNoticeWnd = NULL;
static HWND g_hNoticeDismissCheck = NULL;

// ===== Short hash (FNV-1a 32-bit, 8 lowercase hex chars) =====

std::string NoticeShortHash(const std::string& s)
{
    unsigned int hash = 2166136261u;
    for (unsigned char c : s)
    {
        hash ^= c;
        hash *= 16777619u;
    }
    char buf[16];
    wsprintfA(buf, "%08x", hash);
    return buf;
}

// ===== Helpers =====

static std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &out[0], len, nullptr, nullptr);
    out.pop_back();
    return out;
}

// Parse ISO 8601 UTC (e.g. "2026-08-10T09:40:11.000Z") and format in the user's local
// timezone. Chinese style: "2026-8-10 17:40:11"; US style: "8/10/2026 5:40:11 PM".
static bool FormatNoticeTime(const std::wstring& isoUtc, std::wstring& out)
{
    out.clear();
    if (isoUtc.size() < 19) return false;

    auto isDigit = [](wchar_t c) { return c >= L'0' && c <= L'9'; };
    auto num2 = [&isoUtc](size_t pos) -> int {
        return (isoUtc[pos] - L'0') * 10 + (isoUtc[pos + 1] - L'0');
    };

    if (!isDigit(isoUtc[0]) || isoUtc[4] != L'-' || isoUtc[7] != L'-' ||
        isoUtc[10] != L'T' || isoUtc[13] != L':' || isoUtc[16] != L':')
        return false;

    SYSTEMTIME utc = {};
    utc.wYear = (WORD)(num2(0) * 100 + num2(2));
    utc.wMonth = (WORD)num2(5);
    utc.wDay = (WORD)num2(8);
    utc.wHour = (WORD)num2(11);
    utc.wMinute = (WORD)num2(14);
    utc.wSecond = (WORD)num2(17);

    SYSTEMTIME local = {};
    if (!SystemTimeToTzSpecificLocalTime(NULL, &utc, &local))
        local = utc;

    wchar_t buf[64];
    if (GetEffectiveIsChinese())
    {
        wsprintfW(buf, L"%d-%d-%d %d:%02d:%02d",
            local.wYear, local.wMonth, local.wDay,
            local.wHour, local.wMinute, local.wSecond);
    }
    else
    {
        int h12 = local.wHour % 12;
        if (h12 == 0) h12 = 12;
        const wchar_t* ampm = (local.wHour < 12) ? L"AM" : L"PM";
        wsprintfW(buf, L"%d/%d/%d %d:%02d:%02d %s",
            local.wMonth, local.wDay, local.wYear,
            h12, local.wMinute, local.wSecond, ampm);
    }
    out = buf;
    return true;
}

// Convert LF / mixed line endings to CRLF so the Win32 multiline EDIT control
// displays line breaks correctly (it does not recognize a lone LF).
static std::wstring NormalizeNewlines(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == L'\n')
        {
            if (i == 0 || s[i - 1] != L'\r')
                out += L'\r';
            out += L'\n';
        }
        else
        {
            out += s[i];
        }
    }
    return out;
}

// ===== Startup check (worker thread) =====

DWORD WINAPI NoticeCheckThread(LPVOID)
{
    if (!g_pfnGetNotice) return 0;

    CloudUploadNoticeResult notice = g_pfnGetNotice();
    if (!notice.ok) return 0;

    std::wstring zh = notice.zh;
    std::wstring en = notice.en;

    // Pick the effective language; fall back to the other language if empty
    bool isChinese = GetEffectiveIsChinese();
    std::wstring content = isChinese ? zh : en;
    if (content.empty()) content = isChinese ? en : zh;
    if (content.empty()) return 0;

    // EDIT control requires CRLF line endings to render line breaks
    content = NormalizeNewlines(content);

    std::string hZh = NoticeShortHash(WideToUtf8(zh));
    std::string hEn = NoticeShortHash(WideToUtf8(en));

    NoticeConfigData cfg;
    LoadNoticeConfig(cfg);

    long long now = (long long)time(NULL);
    if (cfg.dismissUntil > now) return 0;              // "not this week" is still active
    if (hZh == cfg.hashZh && hEn == cfg.hashEn) return 0;  // no content change

    // Record current hashes so it won't pop again unless the content changes
    cfg.hashZh = hZh;
    cfg.hashEn = hEn;
    SaveNoticeConfig(cfg);

    std::wstring timeText;
    FormatNoticeTime(notice.updatedAt, timeText);

    NoticePayload* payload = new NoticePayload;
    payload->content = content;
    payload->timeText = timeText;
    PostMessageW(g_hWndMain, WM_NOTICE_AVAILABLE, 0, (LPARAM)payload);
    return 0;
}

void StartNoticeCheck()
{
    CreateThread(NULL, 0, NoticeCheckThread, NULL, 0, NULL);
}

// ===== Notice window =====

LRESULT CALLBACK NoticeDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        const auto& i18n = GetI18N();
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
        NoticePayload* payload = (NoticePayload*)((LPCREATESTRUCT)lParam)->lpCreateParams;

        HFONT hFont = NULL;
        NONCLIENTMETRICSW ncm = {0};
        ncm.cbSize = sizeof(ncm);
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
            hFont = CreateFontIndirectW(&ncm.lfMessageFont);
        if (!hFont)
            hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)hFont);

        const int margin = 16;
        const int editW = 440;
        const int editH = 260;

        HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            payload ? payload->content.c_str() : L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
            margin, margin, editW, editH,
            hWnd, (HMENU)IDC_NOTICE_EDIT, hInst, NULL);
        SendMessageW(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hCheck = CreateWindowW(L"BUTTON", i18n.noticeDismissCheck,
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            margin, margin + editH + 12, 280, 24,
            hWnd, (HMENU)IDC_NOTICE_DISMISS, hInst, NULL);
        SendMessageW(hCheck, WM_SETFONT, (WPARAM)hFont, TRUE);
        g_hNoticeDismissCheck = hCheck;

        HWND hOk = CreateWindowW(L"BUTTON", i18n.btnNoticeOk,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            margin + editW - 90, margin + editH + 10, 90, 30,
            hWnd, (HMENU)IDC_NOTICE_OK, hInst, NULL);
        SendMessageW(hOk, WM_SETFONT, (WPARAM)hFont, TRUE);

        int dlgW = editW + margin * 2;
        int dlgH = margin + editH + 12 + 30 + 14;
        RECT rcWin = {0, 0, dlgW, dlgH};
        AdjustWindowRect(&rcWin, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
        int winW = rcWin.right - rcWin.left;
        int winH = rcWin.bottom - rcWin.top;

        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(hWnd, NULL, (sw - winW) / 2, (sh - winH) / 2, winW, winH, SWP_NOZORDER);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_NOTICE_OK && HIWORD(wParam) == BN_CLICKED)
            DestroyWindow(hWnd);
        break;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
    {
        // If "don't remind for a week" is checked, write the timestamp to the config
        if (g_hNoticeDismissCheck &&
            SendMessageW(g_hNoticeDismissCheck, BM_GETCHECK, 0, 0) == BST_CHECKED)
        {
            NoticeConfigData cfg;
            LoadNoticeConfig(cfg);
            cfg.dismissUntil = (long long)time(NULL) + 7LL * 24 * 3600;
            SaveNoticeConfig(cfg);
        }
        g_hNoticeDismissCheck = NULL;
        g_hNoticeWnd = NULL;

        HFONT hFont = (HFONT)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        if (hFont && hFont != (HFONT)GetStockObject(DEFAULT_GUI_FONT))
            DeleteObject(hFont);
        break;
    }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void ShowNoticeWindow(const std::wstring& content, const std::wstring& timeText)
{
    if (g_hNoticeWnd && IsWindow(g_hNoticeWnd)) return;

    const auto& i18n = GetI18N();
    std::wstring title = std::wstring(i18n.noticeTitlePrefix) + L" | " + std::wstring(i18n.noticeUpdatedPrefix) + timeText;

    NoticePayload payload;
    payload.content = content;
    payload.timeText = timeText;

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = NoticeDlgProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"NoticeDialog";
    wc.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_APPICON));
    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowExW(0, L"NoticeDialog", title.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 480, 340,
        g_hWndMain, NULL, g_hInst, &payload);

    if (hWnd)
    {
        g_hNoticeWnd = hWnd;
        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);
    }
}
