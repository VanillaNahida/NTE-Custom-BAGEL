#include "globals.h"

HINSTANCE g_hInst = NULL;
HWND g_hWndMain = NULL;
HFONT g_hFont = NULL;

WCHAR g_szBinDir[MAX_PATH] = {0};

WCHAR g_szSelectedImage[MAX_PATH] = {0};
bool g_bImageLoaded = false;
Gdiplus::Bitmap* g_pPreviewBitmap = NULL;

const Resolution g_resolutions[] = {
    { L"720p  (1280 x 720)",   1280, 720  },
    { L"1080p (1920 x 1080)",  1920, 1080 },
    { L"1440p (2560 x 1440)",  2560, 1440 },
};
const int g_resolutionCount = 3;
int g_selectedResolution = 1;

std::vector<std::wstring> g_gamePaths;
std::vector<int> g_gamePathRegions;
std::vector<std::wstring> g_uids;
std::vector<int> g_uidRegions;
std::wstring g_defaultGamePath;
std::wstring g_defaultUid;
WCHAR g_szConfigPath[MAX_PATH] = {0};

HWND g_hComboResolution = NULL;
HWND g_hBtnSelect = NULL;
HWND g_hBtnLaunch = NULL;
HWND g_hBtnAbout = NULL;
HWND g_hBtnOpenFolder = NULL;
HWND g_hBtnSettings = NULL;
HWND g_hStaticStatus = NULL;
HWND g_hStaticPath = NULL;
HWND g_hWndPreview = NULL;

HANDLE g_hLauncherProcess = NULL;

ULONG_PTR g_gdiplusToken = 0;

HMODULE g_hCloudUploadDll = NULL;
CloudUpload_UploadImage_t g_pfnUpload = NULL;
CloudUpload_GetMachineId_t g_pfnGetMachineId = NULL;
CloudUpload_GetNotice_t g_pfnGetNotice = NULL;
CloudUpload_CheckForUpdate_t g_pfnCheckUpdate = NULL;
