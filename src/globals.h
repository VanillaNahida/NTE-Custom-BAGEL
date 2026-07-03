#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>

// Control IDs
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
#define IDC_SETTINGS_AUTO_DETECT 2003
#define IDC_SETTINGS_SAVE      2004
#define IDC_SETTINGS_CANCEL    2005
#define IDC_SETTINGS_PATH_LIST  2006
#define IDC_SETTINGS_ADD_PATH   2007
#define IDC_SETTINGS_REMOVE_PATH 2008
#define IDC_SETTINGS_RESCAN_UID 2009
#define IDC_SETTINGS_UID_LIST   2010
#define IDC_SETTINGS_ADD_UID    2011
#define IDC_SETTINGS_REMOVE_UID 2012
#define IDC_SETTINGS_UID_INPUT  2013

#define WM_LAUNCHER_EXITED     (WM_USER + 101)

// Application instance
extern HINSTANCE g_hInst;

// Main window
extern HWND g_hWndMain;
extern HFONT g_hFont;

// Bin directory
extern WCHAR g_szBinDir[MAX_PATH];

// Image state
extern WCHAR g_szSelectedImage[MAX_PATH];
extern bool g_bImageLoaded;
extern Gdiplus::Bitmap* g_pPreviewBitmap;

// Resolution
struct Resolution {
    const WCHAR* label;
    int width;
    int height;
};
extern const Resolution g_resolutions[];
extern const int g_resolutionCount;
extern int g_selectedResolution;

// Config
extern std::vector<std::wstring> g_gamePaths;
extern std::vector<std::wstring> g_uids;
extern std::wstring g_defaultGamePath;
extern std::wstring g_defaultUid;
extern WCHAR g_szConfigPath[MAX_PATH];

// Control handles
extern HWND g_hComboResolution;
extern HWND g_hBtnSelect;
extern HWND g_hBtnLaunch;
extern HWND g_hBtnAbout;
extern HWND g_hBtnOpenFolder;
extern HWND g_hBtnSettings;
extern HWND g_hStaticStatus;
extern HWND g_hStaticPath;
extern HWND g_hWndPreview;

// Launcher
extern HANDLE g_hLauncherProcess;

// GDI+ token
extern ULONG_PTR g_gdiplusToken;

// Cloud upload DLL interface (云控)
struct CloudUploadResult {
    bool success;
    char imageHash[65];
    char errorMessage[512];
};
typedef CloudUploadResult (*CloudUpload_UploadImage_t)(const wchar_t*);
typedef void (*CloudUpload_CheckForUpdate_t)(HWND, const wchar_t*, const wchar_t*, const wchar_t*);

extern HMODULE g_hCloudUploadDll;
extern CloudUpload_UploadImage_t g_pfnUpload;
extern CloudUpload_CheckForUpdate_t g_pfnCheckUpdate;
