#pragma once
#include <windows.h>

// --- Language override from --lang command-line argument ---
inline bool& GetLangOverride()
{
    static bool val = false;
    return val;
}
inline bool& GetLangOverrideSet()
{
    static bool val = false;
    return val;
}
inline void SetLanguageOverride(bool forceChinese)
{
    GetLangOverrideSet() = true;
    GetLangOverride() = forceChinese;
}

inline bool IsChineseSystem()
{
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;
}

inline bool GetEffectiveIsChinese()
{
    if (GetLangOverrideSet())
        return GetLangOverride();
    return IsChineseSystem();
}

struct LangStrings {
    // --- Main Window ---
    const wchar_t* windowTitle;
    const wchar_t* resolutionLabel;
    const wchar_t* btnSelectImage;
    const wchar_t* btnLaunch;
    const wchar_t* btnAbout;
    const wchar_t* statusReady;
    const wchar_t* pathNoImageHint;     // initial hint text in path static
    const wchar_t* pathNoImage;         // short text when no image
    const wchar_t* pathSelected;        // L"已选择: %s" / L"Selected: %s"
    const wchar_t* statusLoadedExisting;
    const wchar_t* statusUploading;
    const wchar_t* statusModuleNotLoaded;
    const wchar_t* statusUploadFailed;  // L"上传失败: %s" / L"Upload failed: %s"
    const wchar_t* statusUploadSuccess; // L"上传成功! 图片哈希: %hs..." / L"Upload success! Hash: %hs..."
    const wchar_t* statusLauncherStarted;
    const wchar_t* statusLauncherExited;
    const wchar_t* statusUacCancelled;
    const wchar_t* statusLaunchFailed;  // L"错误: 启动注入器失败 (错误代码: %lu)"
    const wchar_t* statusImageProcessed; // L"图片已自动处理并保存到 replace.png (%d x %d)"
    const wchar_t* statusImageReprocessed;
    const wchar_t* statusProcessFailed;
    const wchar_t* statusReprocessFailed;
    const wchar_t* statusUnsupportedFormat;
    // --- MessageBox ---
    const wchar_t* msgBoxError;
    const wchar_t* msgBoxHint;
    const wchar_t* msgLauncherRunning;
    const wchar_t* msgLauncherNotFound;
    const wchar_t* msgDllNotFound;
    const wchar_t* msgDllNoFunction;
    // --- File Dialog ---
    const wchar_t* fileDialogTitle;
    // --- About Dialog ---
    const wchar_t* aboutTitle;
    const wchar_t* aboutVersionPrefix;
    const wchar_t* aboutAuthor;
    const wchar_t* aboutDisclaimer;
    const wchar_t* aboutBtnOk;
    const wchar_t* aboutLinkGithub;
    const wchar_t* aboutLinkBilibili;
    const wchar_t* aboutLinkContact;
};

inline const LangStrings& GetI18N()
{
    static const LangStrings zhCN = {
        L"异环呗果图片上传器 | By: 香草味的纳西妲喵",
        L"分辨率:",
        L"选择图片",
        L"启动",
        L"关于",
        L"已准备就绪",
        L"未选择图片，可点击上方按钮选择或拖入图片到下方空白处。",
        L"未选择图片",
        L"已选择: %s",
        L"已加载现有图片",
        L"正在上传图片...",
        L"上传模块未加载",
        L"上传失败: %s",
        L"上传成功! 图片哈希: %hs...",
        L"上传成功，注入器已启动 (%hs...)",
        L"注入器已退出",
        L"管理员提权已被用户取消",
        L"错误: 启动注入器失败 (错误代码: %lu)",
        L"图片已自动处理并保存到 replace.png (%d x %d)",
        L"图片已自动重处理 (%d x %d)",
        L"错误: 图片处理失败",
        L"错误: 重新处理图片失败",
        L"不支持的文件格式，请拖入图片文件",
        L"错误",
        L"提示",
        L"注入器已启动，请关闭注入器后重新选择图片",
        L"未找到注入器，程序将无法继续运行，请确保下载并解压完整，且文件名未被修改。",
        L"无法加载 NTEUploadBase.dll，请确保该文件存在于程序目录中。",
        L"NTEUploadBase.dll 中未找到核心函数。",
        L"请选择图片文件，推荐比例: 16:9, 不符合比例的图片将会被自动拉伸处理",
        L"关于",
        L"程序版本：",
        L"作者：香草味的纳西妲喵",
        L"本程序开源，禁止用于商业用途，禁止上传违规图片，仅供学习交流和研究目的\n若程序被滥用或倒卖，作者将有权关闭使用权限。\n如对你有帮助，请给项目点一个Star!",
        L"确定",
        L"<a href=\"https://github.com/VanillaNahida/NTE-Custom-BAGEL\">程序代码开源GitHub</a>",
        L"<a href=\"https://space.bilibili.com/1347891621\">作者B站主页</a>",
        L"<a href=\"https://xcnahida.cn/contact\">问题反馈交流群</a>",
    };

    static const LangStrings enUS = {
        L"NTE BAGEL Image Uploader | By: VanillaNahida",
        L"Resolution:",
        L"Select Image",
        L"Launch",
        L"About",
        L"Ready",
        L"Select an image or drag & drop below.",
        L"No image selected",
        L"Selected: %s",
        L"Loaded existing image",
        L"Uploading image...",
        L"Upload module not loaded",
        L"Upload failed: %s",
        L"Upload success! Hash: %hs...",
        L"Upload success, launcher started (%hs...)",
        L"Launcher exited",
        L"UAC elevation cancelled by user",
        L"Error: Failed to launch injector (code: %lu)",
        L"Image processed and saved to replace.png (%d x %d)",
        L"Image reprocessed (%d x %d)",
        L"Error: Image processing failed",
        L"Error: Reprocessing failed",
        L"Unsupported file format. Please drop an image file.",
        L"Error",
        L"Hint",
        L"Injector is running. Please close it before selecting another image.",
        L"Injector not found. Please ensure the download is complete and file names are unchanged.",
        L"Cannot load NTEUploadBase.dll. Please ensure it exists in the program directory.",
        L"NTEUploadBase.dll does not contain the required function.",
        L"Select an image file (recommended ratio: 16:9). Non-16:9 images will be stretched.",
        L"About",
        L"Version: ",
        L"Author: VanillaNahida",
        L"This program is open source. Do not use for commercial purposes or upload inappropriate images.\nIt is intended for learning and research purposes only.\nIf the program is misused or resold, the author reserves the right to revoke access.\nIf you find this helpful, please give the project a Star!",
        L"OK",
        L"<a href=\"https://github.com/VanillaNahida/NTE-Custom-BAGEL\">GitHub Repository</a>",
        L"<a href=\"https://space.bilibili.com/1347891621\">Author's Bilibili Page</a>",
        L"<a href=\"https://xcnahida.cn/contact\">Contact / Support</a>",
    };

    static const bool isChinese = GetEffectiveIsChinese();
    return isChinese ? zhCN : enUS;
}

// File dialog filter with embedded null terminators (can't go in struct)
inline const wchar_t* GetFileDialogFilter()
{
    if (GetEffectiveIsChinese()) {
        return L"图片文件(*.png;*.jpg;*.jpeg;*.bmp;*.tiff)\0*.png;*.jpg;*.jpeg;*.bmp;*.tiff\0"
               L"PNG 文件\0*.png\0"
               L"JPEG 文件\0*.jpg;*.jpeg\0"
               L"BMP 文件\0*.bmp\0\0";
    } else {
        return L"Image Files (*.png;*.jpg;*.jpeg;*.bmp;*.tiff)\0*.png;*.jpg;*.jpeg;*.bmp;*.tiff\0"
               L"PNG Files (*.png)\0*.png\0"
               L"JPEG Files (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0"
               L"BMP Files (*.bmp)\0*.bmp\0\0";
    }
}
