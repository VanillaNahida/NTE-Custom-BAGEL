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
#define IDC_ABOUT_COPY_DEVICE  2014
#define IDC_NOTICE_EDIT        2015
#define IDC_NOTICE_DISMISS     2016
#define IDC_NOTICE_OK          2017
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
#define WM_NOTICE_AVAILABLE    (WM_USER + 102)

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

// Region identifier for game paths and UIDs
enum GameRegion {
    REGION_CN = 0,
    REGION_GLOBAL = 1,
    REGION_UNKNOWN = -1
};

// Config
extern std::vector<std::wstring> g_gamePaths;
extern std::vector<int> g_gamePathRegions;
extern std::vector<std::wstring> g_uids;
extern std::vector<int> g_uidRegions;
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
    char machineId[65];
    char errorMessage[512];
    bool imageExists;
    bool approved;
    char reviewStatus[16];
    bool deviceBanned;
    bool imageBanned;
};
// Bilingual announcement from GET /api/notice (must match libs/cloud_upload.h)
struct CloudUploadNoticeResult {
    bool ok;
    wchar_t zh[4096];
    wchar_t en[4096];
    wchar_t updatedAt[64];
};
typedef CloudUploadResult (*CloudUpload_UploadImage_t)(const wchar_t*);
typedef void (*CloudUpload_GetMachineId_t)(wchar_t*, int);
typedef CloudUploadNoticeResult (*CloudUpload_GetNotice_t)();
typedef void (*CloudUpload_CheckForUpdate_t)(HWND, const wchar_t*, const wchar_t*, const wchar_t*);

extern HMODULE g_hCloudUploadDll;
extern CloudUpload_UploadImage_t g_pfnUpload;
extern CloudUpload_GetMachineId_t g_pfnGetMachineId;
extern CloudUpload_GetNotice_t g_pfnGetNotice;
extern CloudUpload_CheckForUpdate_t g_pfnCheckUpdate;
