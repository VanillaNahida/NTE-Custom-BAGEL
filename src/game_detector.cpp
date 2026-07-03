#include "game_detector.h"
#include "globals.h"

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
                    if (!found) paths.push_back(path);
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
