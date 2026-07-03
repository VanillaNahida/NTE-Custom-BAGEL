#pragma once
#include <windows.h>
#include <gdiplus.h>

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);
bool EnsureTargetDirectory();
bool ProcessAndSaveImage(const WCHAR* srcPath, int targetWidth, int targetHeight);

void UpdateStatusText(const WCHAR* text);
void UpdatePathText();
void UpdatePreview();
void LoadImageFromPath(const WCHAR* szPath);
void OnSelectImage();
void OnResolutionChanged();
void TryLoadExistingImage();
