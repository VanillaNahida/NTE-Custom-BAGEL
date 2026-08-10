#include "config_manager.h"
#include "globals.h"
#include "game_detector.h"
#include <fstream>

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

static std::wstring ReadConfigFile()
{
    std::ifstream f(g_szConfigPath, std::ios::binary);
    if (!f.is_open()) return L"";
    std::string utf8((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    if (utf8.empty()) return L"";

    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return L"";
    std::wstring wjson(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wjson[0], wlen);
    wjson.pop_back();
    return wjson;
}

static void WriteConfigFile(const std::wstring& json)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, json.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return;
    std::string utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, json.c_str(), -1, &utf8[0], len, nullptr, nullptr);
    utf8.pop_back();

    std::ofstream f(g_szConfigPath, std::ios::binary);
    if (f.is_open())
    {
        f.write(utf8.c_str(), utf8.size());
    }
}

static std::wstring WideFromUtf8(const std::string& s)
{
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], len);
    out.pop_back();
    return out;
}

static std::string Utf8FromWide(const std::wstring& w)
{
    if (w.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &out[0], len, nullptr, nullptr);
    out.pop_back();
    return out;
}

static std::wstring JsonExtractString(const std::wstring& wjson, const std::wstring& key)
{
    std::wstring searchKey = L"\"" + key + L"\"";
    size_t keyPos = wjson.find(searchKey);
    if (keyPos == std::wstring::npos) return L"";

    size_t valueStart = wjson.find(L'"', keyPos + searchKey.size());
    if (valueStart == std::wstring::npos) return L"";
    valueStart++;

    size_t valueEnd = wjson.find(L'"', valueStart);
    if (valueEnd == std::wstring::npos) return L"";

    return JsonUnescape(wjson.substr(valueStart, valueEnd - valueStart));
}

static long long JsonExtractNumber(const std::wstring& wjson, const std::wstring& key)
{
    std::wstring searchKey = L"\"" + key + L"\"";
    size_t keyPos = wjson.find(searchKey);
    if (keyPos == std::wstring::npos) return 0;

    size_t colon = wjson.find(L':', keyPos);
    if (colon == std::wstring::npos) return 0;

    size_t start = colon + 1;
    while (start < wjson.size() && wjson[start] == L' ') start++;

    size_t end = start;
    while (end < wjson.size() &&
           (wjson[end] == L'-' || (wjson[end] >= L'0' && wjson[end] <= L'9')))
        end++;

    if (end == start) return 0;
    try
    {
        return std::stoll(wjson.substr(start, end - start));
    }
    catch (...)
    {
        return 0;
    }
}

// Build the complete config JSON: game config + announcement config
static std::wstring BuildFullJson(const NoticeConfigData& notice)
{
    std::wstring json = L"{\"gamePaths\":";
    json += VectorToJsonArray(g_gamePaths);
    json += L",\"uids\":";
    json += VectorToJsonArray(g_uids);
    json += L",\"defaultGamePath\":\"";
    json += JsonEscape(g_defaultGamePath);
    json += L"\",\"defaultUid\":\"";
    json += JsonEscape(g_defaultUid);
    json += L"\",\"hash_zh\":\"";
    json += JsonEscape(WideFromUtf8(notice.hashZh));
    json += L"\",\"hash_en\":\"";
    json += JsonEscape(WideFromUtf8(notice.hashEn));
    json += L"\",\"dismiss_until\":";
    json += std::to_wstring(notice.dismissUntil);
    json += L"}";
    return json;
}

void SaveConfig()
{
    // Preserve the current announcement state while writing the full config
    NoticeConfigData notice;
    LoadNoticeConfig(notice);
    WriteConfigFile(BuildFullJson(notice));
}

void LoadNoticeConfig(NoticeConfigData& out)
{
    out = NoticeConfigData();
    std::wstring wjson = ReadConfigFile();
    if (wjson.empty()) return;

    out.hashZh = Utf8FromWide(JsonExtractString(wjson, L"hash_zh"));
    out.hashEn = Utf8FromWide(JsonExtractString(wjson, L"hash_en"));
    out.dismissUntil = JsonExtractNumber(wjson, L"dismiss_until");
}

void SaveNoticeConfig(const NoticeConfigData& notice)
{
    WriteConfigFile(BuildFullJson(notice));
}

void LoadConfig()
{
    std::wstring wjson = ReadConfigFile();
    if (wjson.empty()) return;

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

        std::wstring content = wjson.substr(start + 1, end - start - 1);
        if (content.empty()) return result;

        size_t pos = 0;
        while (pos < content.size())
        {
            while (pos < content.size() && (content[pos] == L' ' || content[pos] == L',' || content[pos] == L'\r' || content[pos] == L'\n'))
                pos++;

            if (pos >= content.size()) break;

            if (content[pos] == L'"')
            {
                pos++;
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

    g_defaultGamePath = JsonExtractString(wjson, L"defaultGamePath");
    g_defaultUid = JsonExtractString(wjson, L"defaultUid");

    // Detect regions for loaded paths from file system
    g_gamePathRegions.clear();
    g_gamePathRegions.resize(g_gamePaths.size(), REGION_UNKNOWN);
    for (size_t i = 0; i < g_gamePaths.size(); i++)
    {
        g_gamePathRegions[i] = DetectGameRegion(g_gamePaths[i]);
    }

    // Detect regions for loaded UIDs by matching against game paths
    g_uidRegions.clear();
    g_uidRegions.resize(g_uids.size(), REGION_UNKNOWN);
    for (size_t ui = 0; ui < g_uids.size(); ui++)
    {
        for (size_t pi = 0; pi < g_gamePaths.size(); pi++)
        {
            std::wstring uidDir = g_gamePaths[pi] + L"\\Client\\WindowsNoEditor\\Selfie\\" + g_uids[ui];
            DWORD attr = GetFileAttributesW(uidDir.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
            {
                g_uidRegions[ui] = (pi < g_gamePathRegions.size()) ? g_gamePathRegions[pi] : REGION_UNKNOWN;
                break;
            }
        }
    }
}
