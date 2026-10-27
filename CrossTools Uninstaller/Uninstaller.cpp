// Uninstaller.cpp - CrossTools Uninstaller
//
// Supports two uninstallation modes:
//   1. CrossFire Adjacent: Strips ONLY CrossTools files out of the CrossFire folder
//      without disturbing CrossFire or any of its files. The CrossFire folder is NEVER deleted.
//   2. Standalone: Removes CrossTools files and removes the installation folder if empty.
//
// In both modes, the dictionaries folder is NEVER deleted unless the user explicitly checks the box.

#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shlobj.h>
#include <commctrl.h>
#include <string>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comctl32.lib")

#define IDC_TITLE       101
#define IDC_INFO        103
#define IDC_DEL_DICTS   104
#define IDC_DICT_PATH   105
#define IDC_BTN_UNINST  106
#define IDC_BTN_CANCEL  107

#define WND_W 490
#define WND_H 290

static const wchar_t* kRegKey = L"Software\\CrossTools";

static HWND  g_hwnd         = nullptr;
static HWND  g_chkDicts     = nullptr;
static HWND  g_btnUninst    = nullptr;
static HWND  g_btnCancel    = nullptr;
static HFONT g_fTitle       = nullptr;
static HFONT g_fBody        = nullptr;
static HFONT g_fBold        = nullptr;
static HICON g_hIcon        = nullptr;

static std::wstring g_installPath;
static std::wstring g_installMode;
static std::wstring g_dictPath;
static std::wstring g_selfPath;

// ── Registry Helper ─────────────────────────────────────────────────────────
static std::wstring RegRead(HKEY root, const wchar_t* subkey, const wchar_t* value) {
    HKEY hk = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &hk) != ERROR_SUCCESS) return L"";
    wchar_t buf[MAX_PATH] = {};
    DWORD type = 0, size = sizeof(buf);
    RegQueryValueExW(hk, value, nullptr, &type, reinterpret_cast<BYTE*>(buf), &size);
    RegCloseKey(hk);
    return buf;
}

// ── Path Normalization & Containment Helper ─────────────────────────────────
static std::wstring NormalizePath(const std::wstring& path) {
    if (path.empty()) return L"";
    wchar_t full[MAX_PATH] = {};
    GetFullPathNameW(path.c_str(), MAX_PATH, full, nullptr);
    std::wstring s(full);
    while (s.size() > 3 && (s.back() == L'\\' || s.back() == L'/')) s.pop_back();
    return s;
}

static bool IsSameOrChildPath(const std::wstring& target, const std::wstring& potentialChild) {
    std::wstring t = NormalizePath(target);
    std::wstring c = NormalizePath(potentialChild);
    if (_wcsicmp(t.c_str(), c.c_str()) == 0) return true;
    if (c.size() > t.size() && (c[t.size()] == L'\\' || c[t.size()] == L'/')) {
        if (_wcsnicmp(t.c_str(), c.c_str(), t.size()) == 0) return true;
    }
    return false;
}

// ── Recursive Directory Deletion (with exclusion support) ───────────────────
static void RemoveDirRecursiveExcept(const std::wstring& dir, const std::wstring& excludePath) {
    std::wstring pattern = dir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            std::wstring child = dir + L"\\" + fd.cFileName;

            if (!excludePath.empty() && IsSameOrChildPath(child, excludePath)) {
                if (_wcsicmp(NormalizePath(child).c_str(), NormalizePath(excludePath).c_str()) == 0)
                    continue;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    RemoveDirRecursiveExcept(child, excludePath);
                    continue;
                }
            }

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                RemoveDirRecursiveExcept(child, excludePath);
            } else {
                SetFileAttributesW(child.c_str(), FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(child.c_str());
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    if (excludePath.empty() || !IsSameOrChildPath(dir, excludePath)) {
        RemoveDirectoryW(dir.c_str());
    }
}

static bool IsAppRunning() {
    std::wstring exe = g_installPath + L"\\CrossTools.exe";
    HANDLE hFile = CreateFileW(exe.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                               OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_SHARING_VIOLATION) return true;
    } else {
        CloseHandle(hFile);
    }
    return false;
}

// ── Perform Uninstallation ──────────────────────────────────────────────────
static void DoUninstall() {
    if (IsAppRunning()) {
        MessageBoxW(g_hwnd,
            L"CrossTools is currently running.\n\nPlease close CrossTools before uninstalling.",
            L"CrossTools Uninstaller", MB_OK | MB_ICONWARNING);
        return;
    }

    bool deleteDicts = (SendMessageW(g_chkDicts, BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool isCfAdjacent = (_wcsicmp(g_installMode.c_str(), L"CrossFireAdjacent") == 0);

    EnableWindow(g_btnUninst, FALSE);
    EnableWindow(g_btnCancel, FALSE);

    // Release directory lock immediately
    wchar_t tmp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tmp);
    SetCurrentDirectoryW(tmp);

    std::wstring dictNorm = NormalizePath(g_dictPath);
    std::wstring instNorm = NormalizePath(g_installPath);
    bool dictInsideInstall = (!dictNorm.empty() && IsSameOrChildPath(instNorm, dictNorm));

    // 1. Dictionaries deletion logic: NEVER deleted unless explicitly checked!
    if (deleteDicts && !dictNorm.empty()) {
        RemoveDirRecursiveExcept(dictNorm, L"");
    }

    // 2. File stripping logic based on mode:
    if (isCfAdjacent) {
        // CrossFire Adjacent mode:
        // Strip ONLY CrossTools-specific files out of the CrossFire folder!
        // NEVER delete CrossFire.exe, plugins, user puzzles, or the folder itself!
        std::wstring exeFile = g_installPath + L"\\CrossTools.exe";
        std::wstring iniFile = g_installPath + L"\\CrossTools.ini";
        SetFileAttributesW(exeFile.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(exeFile.c_str());
        SetFileAttributesW(iniFile.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(iniFile.c_str());
    } else {
        // Standalone mode:
        // Remove application files inside installPath (preserving dictionaries if unchecked)
        std::wstring excludeForInstall = (!deleteDicts && dictInsideInstall) ? dictNorm : L"";

        std::wstring pattern = g_installPath + L"\\*";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                std::wstring child = g_installPath + L"\\" + fd.cFileName;

                // Skip self (deleted by background command)
                if (_wcsicmp(NormalizePath(child).c_str(), NormalizePath(g_selfPath).c_str()) == 0)
                    continue;

                // Skip preserved dictionaries folder if requested
                if (!excludeForInstall.empty() && IsSameOrChildPath(child, excludeForInstall)) {
                    if (_wcsicmp(NormalizePath(child).c_str(), excludeForInstall.c_str()) == 0)
                        continue;
                }

                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    RemoveDirRecursiveExcept(child, excludeForInstall);
                } else {
                    SetFileAttributesW(child.c_str(), FILE_ATTRIBUTE_NORMAL);
                    DeleteFileW(child.c_str());
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }

    // 3. Remove registry key
    RegDeleteKeyW(HKEY_CURRENT_USER, kRegKey);

    // 4. Notify completion
    if (isCfAdjacent) {
        MessageBoxW(g_hwnd,
            L"CrossTools has been removed from your CrossFire folder.\n"
            L"Your CrossFire installation and files were not disturbed.",
            L"CrossTools Uninstaller", MB_OK | MB_ICONINFORMATION);
    } else {
        if (!deleteDicts && dictInsideInstall) {
            std::wstring msg = L"CrossTools has been uninstalled.\n\n"
                               L"Your dictionaries folder was preserved at:\n" + g_dictPath;
            MessageBoxW(g_hwnd, msg.c_str(), L"CrossTools Uninstaller", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(g_hwnd, L"CrossTools was successfully removed from your computer.",
                        L"CrossTools Uninstaller", MB_OK | MB_ICONINFORMATION);
        }
    }

    // 5. Spawn background self-delete command:
    // If CrossFire adjacent: Delete self ONLY! DO NOT run rmdir on the CrossFire directory!
    // If Standalone: Delete self and remove installPath if empty.
    std::wstring cmd;
    if (isCfAdjacent) {
        cmd = L"cmd.exe /d /c \"cd /d \"" + std::wstring(tmp) + L"\" & "
              L"ping 127.0.0.1 -n 2 >nul & "
              L"del /f /q \"" + g_selfPath + L"\" 2>nul & "
              L"ping 127.0.0.1 -n 1 >nul & "
              L"del /f /q \"" + g_selfPath + L"\" 2>nul\"";
    } else {
        cmd = L"cmd.exe /d /c \"cd /d \"" + std::wstring(tmp) + L"\" & "
              L"ping 127.0.0.1 -n 2 >nul & "
              L"del /f /q \"" + g_selfPath + L"\" 2>nul & "
              L"ping 127.0.0.1 -n 1 >nul & "
              L"del /f /q \"" + g_selfPath + L"\" 2>nul & "
              L"rmdir \"" + g_installPath + L"\" 2>nul\"";
    }

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, tmp, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    PostQuitMessage(0);
}

// ── Window Procedure ────────────────────────────────────────────────────────
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, GetSysColor(COLOR_BTNFACE));
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BTN_UNINST:
            DoUninstall();
            break;
        case IDC_BTN_CANCEL:
            PostQuitMessage(0);
            break;
        }
        break;
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ── Entry Point ─────────────────────────────────────────────────────────────
int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int) {
    wchar_t tmp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tmp);
    SetCurrentDirectoryW(tmp);

    wchar_t cur[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, cur, MAX_PATH);
    g_selfPath = cur;

    g_installPath = RegRead(HKEY_CURRENT_USER, kRegKey, L"InstallPath");
    g_installMode = RegRead(HKEY_CURRENT_USER, kRegKey, L"InstallMode");
    g_dictPath    = RegRead(HKEY_CURRENT_USER, kRegKey, L"DictFolder");

    if (g_installPath.empty()) {
        std::wstring s(cur);
        size_t pos = s.find_last_of(L"\\/");
        if (pos != std::wstring::npos) g_installPath = s.substr(0, pos);
    }

    if (g_installPath.empty()) {
        MessageBoxW(nullptr, L"Could not determine the CrossTools installation folder.",
                    L"CrossTools Uninstaller", MB_OK | MB_ICONERROR);
        return 1;
    }

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    g_fTitle = CreateFontW(-18, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_fBody  = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_fBold  = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    g_hIcon = LoadIconW(nullptr, IDI_APPLICATION);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.hInstance     = hi;
    wc.lpfnWndProc   = WndProc;
    wc.lpszClassName = L"CrossToolsUninstaller";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon         = g_hIcon;
    RegisterClassExW(&wc);

    RECT rcClient = { 0, 0, WND_W, WND_H };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&rcClient, style, FALSE);
    int wW = rcClient.right  - rcClient.left;
    int wH = rcClient.bottom - rcClient.top;
    int wX = (GetSystemMetrics(SM_CXSCREEN) - wW) / 2;
    int wY = (GetSystemMetrics(SM_CYSCREEN) - wH) / 2;

    g_hwnd = CreateWindowExW(0, L"CrossToolsUninstaller", L"CrossTools Uninstaller",
                              style, wX, wY, wW, wH, nullptr, nullptr, hi, nullptr);

    int cy = 16;
    HWND hTitle = CreateWindowExW(0, L"STATIC", L"Uninstall CrossTools",
                                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  20, cy, WND_W - 40, 24, g_hwnd, (HMENU)IDC_TITLE, hi, nullptr);
    SendMessageW(hTitle, WM_SETFONT, (WPARAM)g_fTitle, TRUE);
    cy += 28;

    bool isCf = (_wcsicmp(g_installMode.c_str(), L"CrossFireAdjacent") == 0);
    std::wstring infoText;
    if (isCf) {
        infoText = L"CrossTools will be removed from your CrossFire folder:\r\n" + g_installPath +
                   L"\r\n\r\nNote: CrossFire and your other files will not be disturbed.";
    } else {
        infoText = L"CrossTools will be removed from:\r\n" + g_installPath;
    }

    HWND hInfo = CreateWindowExW(0, L"STATIC", infoText.c_str(),
                                 WS_CHILD | WS_VISIBLE | SS_LEFT,
                                 20, cy, WND_W - 40, isCf ? 58 : 38, g_hwnd, (HMENU)IDC_INFO, hi, nullptr);
    SendMessageW(hInfo, WM_SETFONT, (WPARAM)g_fBody, TRUE);
    cy += isCf ? 66 : 48;

    // Checkbox: NEVER checked by default
    g_chkDicts = CreateWindowExW(0, L"BUTTON",
        L"Also delete the dictionaries folder",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        20, cy, WND_W - 40, 22, g_hwnd, (HMENU)IDC_DEL_DICTS, hi, nullptr);
    SendMessageW(g_chkDicts, WM_SETFONT, (WPARAM)g_fBold, TRUE);
    SendMessageW(g_chkDicts, BM_SETCHECK, BST_UNCHECKED, 0);
    cy += 24;

    std::wstring dictInfo = g_dictPath.empty()
        ? L"(No dictionaries folder configured)"
        : L"Location: " + g_dictPath;
    HWND hDictPath = CreateWindowExW(0, L"STATIC", dictInfo.c_str(),
                                    WS_CHILD | WS_VISIBLE | SS_LEFT,
                                    38, cy, WND_W - 58, 36, g_hwnd, (HMENU)IDC_DICT_PATH, hi, nullptr);
    SendMessageW(hDictPath, WM_SETFONT, (WPARAM)g_fBody, TRUE);

    // Buttons
    int btnY = WND_H - 44;
    g_btnUninst = CreateWindowExW(0, L"BUTTON", L"Uninstall",
                                  WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                  WND_W - 208, btnY, 96, 28, g_hwnd, (HMENU)IDC_BTN_UNINST, hi, nullptr);
    SendMessageW(g_btnUninst, WM_SETFONT, (WPARAM)g_fBody, TRUE);

    g_btnCancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
                                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  WND_W - 104, btnY, 84, 28, g_hwnd, (HMENU)IDC_BTN_CANCEL, hi, nullptr);
    SendMessageW(g_btnCancel, WM_SETFONT, (WPARAM)g_fBody, TRUE);

    ShowWindow(g_hwnd, SW_SHOWNORMAL);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_fTitle);
    DeleteObject(g_fBody);
    DeleteObject(g_fBold);
    return 0;
}
