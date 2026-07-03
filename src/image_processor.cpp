#include "image_processor.h"
#include "globals.h"
#include "version.h"
#include "i18n.h"
#include <commdlg.h>
#include <tlhelp32.h>

static bool IsLauncherRunning()
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
