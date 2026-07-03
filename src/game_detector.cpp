#include "game_detector.h"
#include "globals.h"

int DetectGameRegion(const std::wstring& gamePath)
{
    // Check for international server launcher first
    std::wstring globalExe = gamePath + L"\\NTEGlobalLauncher.exe";
    if (GetFileAttributesW(globalExe.c_str()) != INVALID_FILE_ATTRIBUTES)
        return REGION_GLOBAL;

    // Check for Chinese server launcher
    std::wstring cnExe = gamePath + L"\\NTELauncher.exe";
    if (GetFileAttributesW(cnExe.c_str()) != INVALID_FILE_ATTRIBUTES)
        return REGION_CN;

    return REGION_UNKNOWN;
}

std::vector<std::wstring> DetectGamePaths()
{
    std::vector<std::wstring> paths;
    g_gamePathRegions.clear();

    struct RegEntry {
        const wchar_t* path;
        int region;
    };
    const RegEntry regEntries[] = {
        { L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\YH", REGION_CN },
        { L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\NTEGlobal", REGION_GLOBAL }
    };

    for (const auto& entry : regEntries)
    {
        HKEY hKey = nullptr;
        LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, entry.path, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
        if (result != ERROR_SUCCESS)
        {
            result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, entry.path, 0, KEY_READ, &hKey);
        }
        if (result == ERROR_SUCCESS)
        {
            WCHAR installPath[MAX_PATH] = {0};
            DWORD size = sizeof(installPath);
            DWORD type = REG_SZ;
            if (RegQueryValueExW(hKey, L"InstallLocation", nullptr, &type, (LPBYTE)installPath, &size) == ERROR_SUCCESS)
            {
                std::wstring path(installPath);
                while (!path.empty() && path.back() == L'\\')
                    path.pop_back();
                if (!path.empty())
                {
                    bool found = false;
                    for (const auto& p : paths)
                    {
                        if (_wcsicmp(p.c_str(), path.c_str()) == 0)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        paths.push_back(path);
                        g_gamePathRegions.push_back(entry.region);
                    }
                }
            }
            RegCloseKey(hKey);
        }
    }
    return paths;
}

std::vector<std::wstring> ScanUIDs()
{
    std::vector<std::wstring> uids;
    g_uidRegions.clear();
    if (g_gamePaths.empty()) return uids;

    for (size_t pi = 0; pi < g_gamePaths.size(); pi++)
    {
        const auto& gamePath = g_gamePaths[pi];
        int region = (pi < g_gamePathRegions.size()) ? g_gamePathRegions[pi] : REGION_UNKNOWN;

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
                bool found = false;
                for (const auto& existing : uids)
                {
                    if (existing == fd.cFileName) { found = true; break; }
                }
                if (!found)
                {
                    uids.push_back(fd.cFileName);
                    g_uidRegions.push_back(region);
                }
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    return uids;
}
