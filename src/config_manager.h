#pragma once
#include <vector>
#include <string>

void LoadConfig();
void SaveConfig();

// Announcement config, stored inside AppConfig.json (same file as game config)
struct NoticeConfigData {
    std::string hashZh;      // short hash of last-seen Chinese announcement
    std::string hashEn;      // short hash of last-seen English announcement
    long long dismissUntil;  // unix seconds; skip popup while now < dismissUntil
};

void LoadNoticeConfig(NoticeConfigData& out);
void SaveNoticeConfig(const NoticeConfigData& data);
