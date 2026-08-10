#pragma once
#include <windows.h>
#include <string>

// Payload posted to the main window when the announcement is ready to show
struct NoticePayload {
    std::wstring content;
    std::wstring timeText;
};

// Startup entry: fetch the bilingual announcement and show a dialog if needed (async).
void StartNoticeCheck();

// Show the announcement window (must be called on the main thread).
void ShowNoticeWindow(const std::wstring& content, const std::wstring& timeText);
