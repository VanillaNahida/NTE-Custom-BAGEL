#pragma once
#include <vector>
#include <string>

std::vector<std::wstring> DetectGamePaths();
std::vector<std::wstring> ScanUIDs();
int DetectGameRegion(const std::wstring& gamePath);
