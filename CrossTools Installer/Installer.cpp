// Installer.cpp - CrossTools Setup Wizard
//
// Supports two installation pathways:
//   1. CrossFire Adjacent: Installed alongside CrossFire.exe.
//      Uses <CrossFireFolder>\dictionaries by default (or custom).
//   2. Standalone Installation: Installed in Program Files (or custom).
//      Creates <InstallPath>\dictionaries by default.
//
// Embeds pre-compiled binaries:
//   RT_RCDATA  IDR_APP_EXE     (101) -- CrossTools.exe
//   RT_RCDATA  IDR_UNINSTALLER (102) -- Uninstall CrossTools.exe

#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <string>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")

// ── Resource IDs ─────────────────────────────────────────────────────────────
#define IDR_APP_EXE     101
#define IDR_UNINSTALLER 102
#define IDI_APP           1

// ── Control IDs ──────────────────────────────────────────────────────────────
#define IDC_BACK              206
#define IDC_NEXT              207
#define IDC_CANCEL            208
#define IDC_TITLE             209
#define IDC_SUBTITLE          210
#define IDC_BODY              211

// Page 1: Pathway selection
#define IDC_MODE_CF           220
#define IDC_MODE_STANDALONE   221
#define IDC_MODE_CF_DESC      222
#define IDC_MODE_SA_DESC      223

// Page 2: Location & Dictionaries
#define IDC_LBL_INSTALL       230
#define IDC_EDIT_INSTALL      231
#define IDC_BTN_INSTALL       232

#define IDC_LBL_DICT          233
#define IDC_RADIO_DICT_CREATE 234
#define IDC_RADIO_DICT_EXIST  235
#define IDC_EDIT_DICT         236
#define IDC_BTN_DICT          237

#define IDC_LBL_CDB           238
#define IDC_EDIT_CDB          239
#define IDC_BTN_CDB           240

// Page 3: Progress & Status
#define IDC_PROGRESS          250
#define IDC_STATUS            251

// Page 4: Launch
#define IDC_LAUNCH            260

// ── Pages ─────────────────────────────────────────────────────────────────────
#define WIZ_WELCOME  0
#define WIZ_MODE     1
#define WIZ_CONFIG   2
#define WIZ_INSTALL  3
#define WIZ_FINISH   4

// ── Custom Messages ───────────────────────────────────────────────────────────
#define WM_INSTALL_DONE     (WM_APP + 1)
#define WM_INSTALL_STATUS   (WM_APP + 2)
#define WM_INSTALL_PROGRESS (WM_APP + 3)

// ── Layout (client area pixels) ──────────────────────────────────────────────
#define CL_W      540
#define CL_H      480
#define BANNER_H   88
#define MARGIN     22
#define CONTENT_Y (BANNER_H + 14)
#define BTN_H      28
#define BTN_Y     (CL_H - BTN_H - 14)

// ════════════════════════════════════════════════════════════════════════════
//  Globals
// ════════════════════════════════════════════════════════════════════════════

static HWND g_hwnd          = nullptr;
static HWND g_hwndTitle     = nullptr;
static HWND g_hwndSubtitle  = nullptr;
static HWND g_hwndBody      = nullptr;
static HWND g_hwndBack      = nullptr;
static HWND g_hwndNext      = nullptr;
static HWND g_hwndCancel    = nullptr;

// Page 1: Pathway
static HWND g_hwndModeCf      = nullptr;
static HWND g_hwndModeCfDesc  = nullptr;
static HWND g_hwndModeSa      = nullptr;
static HWND g_hwndModeSaDesc  = nullptr;

// Page 2: Config
static HWND g_hwndLblInstall   = nullptr;
static HWND g_hwndEditInstall  = nullptr;
static HWND g_hwndBtnInstall   = nullptr;

static HWND g_hwndLblDict      = nullptr;
static HWND g_hwndRadioDictCr  = nullptr;
static HWND g_hwndRadioDictEx  = nullptr;
static HWND g_hwndEditDict     = nullptr;
static HWND g_hwndBtnDict      = nullptr;

static HWND g_hwndLblCdb       = nullptr;
static HWND g_hwndEditCdb      = nullptr;
static HWND g_hwndBtnCdb       = nullptr;

// Page 3: Progress
static HWND g_hwndProgress     = nullptr;
static HWND g_hwndStatus       = nullptr;

// Page 4: Launch
static HWND g_hwndLaunch       = nullptr;

static HFONT g_fTitle    = nullptr;
static HFONT g_fSub      = nullptr;
static HFONT g_fBody     = nullptr;
static HFONT g_fBold     = nullptr;
static HICON g_appIcon   = nullptr;

static int          g_page        = WIZ_WELCOME;
static bool         g_ok          = false;
static bool         g_modeCf      = true; // default to CrossFire adjacent
static std::wstring g_installPath;
static std::wstring g_dictPath;
static std::wstring g_cdbPath;

// ════════════════════════════════════════════════════════════════════════════
//  Registry helper
// ════════════════════════════════════════════════════════════════════════════
static const wchar_t* kRegKey = L"Software\\CrossTools";

static void RegWrite(const wchar_t* name, const std::wstring& value) {
    HKEY hk = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hk, nullptr) != ERROR_SUCCESS)
        return;
    RegSetValueExW(hk, name, 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(value.c_str()),
                   (DWORD)((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hk);
}

// ════════════════════════════════════════════════════════════════════════════
//  Path detection helpers
// ════════════════════════════════════════════════════════════════════════════

static std::wstring DetectCrossFirePath() {
    wchar_t userProfile[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH)) {
        std::wstring u = std::wstring(userProfile) + L"\\CrossFire";
        if (GetFileAttributesW(u.c_str()) != INVALID_FILE_ATTRIBUTES) return u;
    }
    const wchar_t* candidates[] = {
        L"C:\\CrossFire",
        L"C:\\Program Files\\CrossFire",
        L"C:\\Program Files (x86)\\CrossFire",
    };
    for (const wchar_t* p : candidates) {
        if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) return p;
    }
    return L"C:\\CrossFire";
}

static std::wstring DefaultStandalonePath() {
    wchar_t buf[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, buf) == S_OK)
        return std::wstring(buf) + L"\\CrossTools";
    return L"C:\\CrossTools";
}

static bool MakeDirRecursive(const std::wstring& path) {
    if (path.empty()) return false;
    if (CreateDirectoryW(path.c_str(), nullptr)) return true;
    DWORD err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS) return true;
    if (err == ERROR_PATH_NOT_FOUND) {
        size_t pos = path.rfind(L'\\');
        if (pos == std::wstring::npos || pos == 0) return false;
        if (!MakeDirRecursive(path.substr(0, pos))) return false;
        return CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
    }
    return false;
}

static bool WriteResource(WORD id, const std::wstring& path) {
    HRSRC   rs = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!rs) return false;
    HGLOBAL hg = LoadResource(nullptr, rs);
    void*   pd = LockResource(hg);
    DWORD   sz = SizeofResource(nullptr, rs);
    if (!pd || !sz) return false;
    HANDLE hf = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0;
    bool ok = WriteFile(hf, pd, sz, &wr, nullptr) && wr == sz;
    CloseHandle(hf);
    return ok;
}

// ════════════════════════════════════════════════════════════════════════════
//  Thread-safe status & progress posts
// ════════════════════════════════════════════════════════════════════════════
static void PostStatus(const std::wstring& msg) {
    wchar_t* p = new wchar_t[msg.size() + 1];
    wcscpy_s(p, msg.size() + 1, msg.c_str());
    PostMessageW(g_hwnd, WM_INSTALL_STATUS, 0, (LPARAM)p);
}

static void PostProgress(int percent) {
    PostMessageW(g_hwnd, WM_INSTALL_PROGRESS, (WPARAM)percent, 0);
}

// ════════════════════════════════════════════════════════════════════════════
//  Installation thread
// ════════════════════════════════════════════════════════════════════════════
struct InstallArgs {
    bool isCrossFireAdjacent;
    std::wstring installPath;
    std::wstring dictPath;
    std::wstring cdbPath;
};

static DWORD WINAPI InstallThread(LPVOID p) {
    auto* a = reinterpret_cast<InstallArgs*>(p);
    bool isCf        = a->isCrossFireAdjacent;
    std::wstring ip  = a->installPath;
    std::wstring dict= a->dictPath;
    std::wstring cdb = a->cdbPath;
    delete a;

    PostProgress(10);
    PostStatus(L"Preparing destination folder\u2026");
    Sleep(100);

    // 1. Ensure install folder exists
    if (!MakeDirRecursive(ip)) {
        PostStatus(L"Could not access the installation folder.");
        PostMessageW(g_hwnd, WM_INSTALL_DONE, 0, 0);
        return 1;
    }

    PostProgress(30);

    // 2. Dictionaries setup: if standalone and dictPath is empty, create /dictionaries
    if (!isCf && dict.empty()) {
        dict = ip + L"\\dictionaries";
        MakeDirRecursive(dict);
    } else if (isCf && dict.empty()) {
        dict = ip + L"\\dictionaries";
    }

    PostProgress(50);
    PostStatus(L"Installing CrossTools\u2026");
    Sleep(100);

    // 3. Extract CrossTools.exe
    if (!WriteResource(IDR_APP_EXE, ip + L"\\CrossTools.exe")) {
        PostStatus(L"Could not install CrossTools executable.");
        PostMessageW(g_hwnd, WM_INSTALL_DONE, 0, 0);
        return 1;
    }

    PostProgress(75);
    PostStatus(L"Installing uninstaller\u2026");
    Sleep(100);

    // 4. Extract Uninstaller
    if (!WriteResource(IDR_UNINSTALLER, ip + L"\\Uninstall CrossTools.exe")) {
        PostStatus(L"Could not install uninstaller.");
        PostMessageW(g_hwnd, WM_INSTALL_DONE, 0, 0);
        return 1;
    }

    PostProgress(90);
    PostStatus(L"Configuring settings\u2026");

    // 5. Store configuration in registry
    RegWrite(L"InstallMode", isCf ? L"CrossFireAdjacent" : L"Standalone");
    RegWrite(L"InstallPath", ip);
    RegWrite(L"DictFolder",  dict);
    if (isCf) RegWrite(L"CrossFirePath", ip);
    if (!cdb.empty()) RegWrite(L"ClueDatabasePath", cdb);

    PostProgress(100);
    PostStatus(L"Installation complete.");
    Sleep(80);

    PostMessageW(g_hwnd, WM_INSTALL_DONE, 1, 0);
    return 0;
}

// ════════════════════════════════════════════════════════════════════════════
//  UI helpers
// ════════════════════════════════════════════════════════════════════════════
static void Show(HWND h)           { if (h) ShowWindow(h, SW_SHOW); }
static void Hide(HWND h)           { if (h) ShowWindow(h, SW_HIDE); }
static void Enable(HWND h, bool e) { if (h) EnableWindow(h, e ? TRUE : FALSE); }

static void HidePageControls() {
    // Page 1: Mode
    Hide(g_hwndModeCf); Hide(g_hwndModeCfDesc);
    Hide(g_hwndModeSa); Hide(g_hwndModeSaDesc);

    // Page 2: Config
    Hide(g_hwndLblInstall);  Hide(g_hwndEditInstall); Hide(g_hwndBtnInstall);
    Hide(g_hwndLblDict);     Hide(g_hwndRadioDictCr); Hide(g_hwndRadioDictEx);
    Hide(g_hwndEditDict);    Hide(g_hwndBtnDict);
    Hide(g_hwndLblCdb);      Hide(g_hwndEditCdb);     Hide(g_hwndBtnCdb);

    // Page 3: Progress
    Hide(g_hwndProgress); Hide(g_hwndStatus);

    // Page 4: Launch
    Hide(g_hwndLaunch);
}

static void UpdateConfigPageLayout() {
    if (g_modeCf) {
        // CrossFire Adjacent layout
        SetWindowTextW(g_hwndTitle,    L"CrossFire Folder & Dictionaries");
        SetWindowTextW(g_hwndSubtitle, L"Select your CrossFire folder and confirm dictionary location.");

        SetWindowTextW(g_hwndLblInstall, L"CrossFire folder (where CrossFire.exe is located):");
        SetWindowTextW(g_hwndLblDict,    L"Dictionaries folder:");

        // Hide standalone dictionary radio buttons
        Hide(g_hwndRadioDictCr);
        Hide(g_hwndRadioDictEx);

        // Position Edit & Browse for Dict directly under the label
        SetWindowPos(g_hwndEditDict, nullptr, MARGIN, CONTENT_Y + 118, CL_W - MARGIN*2 - 82, 22, SWP_NOZORDER);
        SetWindowPos(g_hwndBtnDict,  nullptr, CL_W - MARGIN - 76, CONTENT_Y + 117, 76, 24, SWP_NOZORDER);

        Show(g_hwndLblInstall);  Show(g_hwndEditInstall); Show(g_hwndBtnInstall);
        Show(g_hwndLblDict);     Show(g_hwndEditDict);    Show(g_hwndBtnDict);
        Show(g_hwndLblCdb);      Show(g_hwndEditCdb);     Show(g_hwndBtnCdb);

        Enable(g_hwndEditDict, true);
        Enable(g_hwndBtnDict,  true);
    } else {
        // Standalone layout
        SetWindowTextW(g_hwndTitle,    L"Choose Install Location");
        SetWindowTextW(g_hwndSubtitle, L"Choose where to install CrossTools and configure dictionaries.");

        SetWindowTextW(g_hwndLblInstall, L"Installation folder:");
        SetWindowTextW(g_hwndLblDict,    L"Dictionaries folder:");

        // Show radio buttons for standalone
        Show(g_hwndRadioDictCr);
        Show(g_hwndRadioDictEx);

        // Position Edit & Browse indented under the second radio
        SetWindowPos(g_hwndEditDict, nullptr, MARGIN + 18, CONTENT_Y + 144, CL_W - MARGIN*2 - 100, 22, SWP_NOZORDER);
        SetWindowPos(g_hwndBtnDict,  nullptr, CL_W - MARGIN - 76, CONTENT_Y + 143, 76, 24, SWP_NOZORDER);

        Show(g_hwndLblInstall);  Show(g_hwndEditInstall); Show(g_hwndBtnInstall);
        Show(g_hwndLblDict);     Show(g_hwndEditDict);    Show(g_hwndBtnDict);
        Show(g_hwndLblCdb);      Show(g_hwndEditCdb);     Show(g_hwndBtnCdb);

        bool useExisting = (SendMessageW(g_hwndRadioDictEx, BM_GETCHECK, 0, 0) == BST_CHECKED);
        Enable(g_hwndEditDict, useExisting);
        Enable(g_hwndBtnDict,  useExisting);
    }
}

static void SetPageUI(int page) {
    HidePageControls();
    Enable(g_hwndBack,   false);
    Enable(g_hwndNext,   true);
    Enable(g_hwndCancel, true);

    switch (page) {
    case WIZ_WELCOME:
        SetWindowTextW(g_hwndTitle,   L"Welcome to CrossTools Setup");
        SetWindowTextW(g_hwndSubtitle,L"This wizard will install CrossTools on your computer.");
        SetWindowTextW(g_hwndBody,
            L"CrossTools is a crossword helper: count letters, fill blanks, "
            L"find random words, and manage custom word lists.\r\n\r\n"
            L"This wizard will guide you through setting up CrossTools on your system.\r\n\r\n"
            L"Click Next to continue.");
        Show(g_hwndBody);
        SetWindowTextW(g_hwndNext, L"Next >");
        break;

    case WIZ_MODE:
        SetWindowTextW(g_hwndTitle,   L"Select Installation Type");
        SetWindowTextW(g_hwndSubtitle,L"Choose how you want to install CrossTools.");
        Hide(g_hwndBody);
        Show(g_hwndModeCf);     Show(g_hwndModeCfDesc);
        Show(g_hwndModeSa);     Show(g_hwndModeSaDesc);
        Enable(g_hwndBack, true);
        SetWindowTextW(g_hwndNext, L"Next >");
        break;

    case WIZ_CONFIG:
        Hide(g_hwndBody);
        UpdateConfigPageLayout();
        Enable(g_hwndBack, true);
        SetWindowTextW(g_hwndNext, L"Install");
        break;

    case WIZ_INSTALL:
        SetWindowTextW(g_hwndTitle,   L"Installing CrossTools");
        SetWindowTextW(g_hwndSubtitle,L"Please wait while CrossTools is being installed\u2026");
        Hide(g_hwndBody);
        SendMessageW(g_hwndProgress, PBM_SETRANGE32, 0, 100);
        SendMessageW(g_hwndProgress, PBM_SETPOS, 0, 0);
        Show(g_hwndProgress);
        Show(g_hwndStatus);
        Enable(g_hwndBack,   false);
        Enable(g_hwndNext,   false);
        Enable(g_hwndCancel, false);
        break;

    case WIZ_FINISH:
        SetWindowTextW(g_hwndTitle,
            g_ok ? L"Installation Complete" : L"Installation Failed");
        SetWindowTextW(g_hwndSubtitle,
            g_ok ? L"CrossTools has been installed successfully."
                 : L"CrossTools could not be installed. See details below.");
        SetWindowTextW(g_hwndBody,
            g_ok ? L"Click Finish to close Setup."
                 : L"Click Close to dismiss Setup.");
        Show(g_hwndBody);
        if (g_ok) Show(g_hwndLaunch);
        Enable(g_hwndBack,   false);
        Enable(g_hwndCancel, false);
        SetWindowTextW(g_hwndNext, g_ok ? L"Finish" : L"Close");
        break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  Folder browse helper
// ════════════════════════════════════════════════════════════════════════════
static int CALLBACK BrowseCB(HWND hw, UINT msg, LPARAM, LPARAM lpData) {
    if (msg == BFFM_INITIALIZED && lpData) SendMessageW(hw, BFFM_SETSELECTIONW, TRUE, lpData);
    return 0;
}

static void BrowseIntoEdit(HWND edit, const wchar_t* title) {
    wchar_t cur[MAX_PATH] = {}; GetWindowTextW(edit, cur, MAX_PATH);
    BROWSEINFOW bi = {};
    bi.hwndOwner = g_hwnd; bi.lpszTitle = title;
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
    bi.lpfn = BrowseCB; bi.lParam = (LPARAM)cur;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    wchar_t chosen[MAX_PATH] = {};
    if (SHGetPathFromIDListW(pidl, chosen) && chosen[0]) {
        std::wstring p = chosen;
        while (!p.empty() && p.back() == L'\\') p.pop_back();
        SetWindowTextW(edit, p.c_str());
    }
    CoTaskMemFree(pidl);
}

// ════════════════════════════════════════════════════════════════════════════
//  Window procedure
// ════════════════════════════════════════════════════════════════════════════
static LRESULT CALLBACK WizardProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CTLCOLORSTATIC: {
        HWND hc = (HWND)lp;
        if (hc == g_hwndTitle) {
            SetBkColor((HDC)wp,   RGB(255,255,255));
            SetTextColor((HDC)wp, RGB(0, 70, 127));
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        }
        if (hc == g_hwndSubtitle) {
            SetBkColor((HDC)wp,   RGB(255,255,255));
            SetTextColor((HDC)wp, RGB(70, 70, 70));
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcW; GetClientRect(hwnd, &rcW);
        RECT rcBanner = { 0, 0, rcW.right, BANNER_H };
        FillRect(hdc, &rcBanner, (HBRUSH)GetStockObject(WHITE_BRUSH));

        if (g_appIcon)
            DrawIconEx(hdc, rcW.right - 68, (BANNER_H - 48) / 2,
                       g_appIcon, 48, 48, 0, nullptr, DI_NORMAL);

        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
        HPEN hOld = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, 0,          BANNER_H,   nullptr);
        LineTo  (hdc, rcW.right,  BANNER_H);
        MoveToEx(hdc, 0,          BTN_Y - 10, nullptr);
        LineTo  (hdc, rcW.right,  BTN_Y - 10);
        SelectObject(hdc, hOld);
        DeleteObject(hPen);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_CANCEL:
            if (g_page != WIZ_INSTALL) PostQuitMessage(0);
            break;

        case IDC_BACK:
            if (g_page > WIZ_WELCOME) { --g_page; SetPageUI(g_page); }
            break;

        case IDC_NEXT:
            if (g_page == WIZ_FINISH) { PostQuitMessage(0); break; }

            if (g_page == WIZ_WELCOME) {
                g_page = WIZ_MODE; SetPageUI(g_page); break;
            }

            if (g_page == WIZ_MODE) {
                g_modeCf = (SendMessageW(g_hwndModeCf, BM_GETCHECK, 0, 0) == BST_CHECKED);
                if (g_modeCf) {
                    std::wstring cfPath = DetectCrossFirePath();
                    SetWindowTextW(g_hwndEditInstall, cfPath.c_str());
                    SetWindowTextW(g_hwndEditDict, (cfPath + L"\\dictionaries").c_str());
                } else {
                    std::wstring saPath = DefaultStandalonePath();
                    SetWindowTextW(g_hwndEditInstall, saPath.c_str());
                    SetWindowTextW(g_hwndEditDict, (saPath + L"\\dictionaries").c_str());
                }
                g_page = WIZ_CONFIG; SetPageUI(g_page); break;
            }

            if (g_page == WIZ_CONFIG) {
                wchar_t buf[MAX_PATH] = {};
                GetWindowTextW(g_hwndEditInstall, buf, MAX_PATH);
                g_installPath = buf;
                if (g_installPath.empty()) {
                    MessageBoxW(hwnd, L"Please enter a valid installation folder.",
                                L"CrossTools Setup", MB_OK | MB_ICONWARNING);
                    break;
                }

                if (g_modeCf) {
                    GetWindowTextW(g_hwndEditDict, buf, MAX_PATH);
                    g_dictPath = buf;
                } else {
                    bool useExisting = (SendMessageW(g_hwndRadioDictEx, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    if (useExisting) {
                        GetWindowTextW(g_hwndEditDict, buf, MAX_PATH);
                        g_dictPath = buf;
                        if (g_dictPath.empty()) {
                            MessageBoxW(hwnd, L"Please enter a dictionaries folder.",
                                        L"CrossTools Setup", MB_OK | MB_ICONWARNING);
                            break;
                        }
                    } else {
                        g_dictPath.clear();
                    }
                }

                GetWindowTextW(g_hwndEditCdb, buf, MAX_PATH);
                g_cdbPath = buf;

                g_page = WIZ_INSTALL; SetPageUI(g_page);
                auto* a = new InstallArgs{ g_modeCf, g_installPath, g_dictPath, g_cdbPath };
                HANDLE ht = CreateThread(nullptr, 0, InstallThread, a, 0, nullptr);
                if (ht) CloseHandle(ht);
                break;
            }
            break;

        case IDC_MODE_CF:
        case IDC_MODE_STANDALONE:
            g_modeCf = (SendMessageW(g_hwndModeCf, BM_GETCHECK, 0, 0) == BST_CHECKED);
            break;

        case IDC_BTN_INSTALL:
            BrowseIntoEdit(g_hwndEditInstall, g_modeCf ? L"Select CrossFire folder:" : L"Select installation folder:");
            // If CrossFire mode, auto-update dictionary field to <ChosenPath>\dictionaries
            if (g_modeCf) {
                wchar_t b[MAX_PATH] = {};
                GetWindowTextW(g_hwndEditInstall, b, MAX_PATH);
                if (b[0]) {
                    std::wstring d = std::wstring(b) + L"\\dictionaries";
                    SetWindowTextW(g_hwndEditDict, d.c_str());
                }
            }
            break;

        case IDC_BTN_DICT:
            BrowseIntoEdit(g_hwndEditDict, L"Select dictionaries folder:");
            break;

        case IDC_BTN_CDB:
            BrowseIntoEdit(g_hwndEditCdb, L"Select ClueDatabase folder:");
            break;

        case IDC_RADIO_DICT_CREATE:
        case IDC_RADIO_DICT_EXIST:
            if (!g_modeCf) {
                bool useExisting = (SendMessageW(g_hwndRadioDictEx, BM_GETCHECK, 0, 0) == BST_CHECKED);
                Enable(g_hwndEditDict, useExisting);
                Enable(g_hwndBtnDict,  useExisting);
            }
            break;

        case IDC_LAUNCH: {
            std::wstring exe = g_installPath + L"\\CrossTools.exe";
            ShellExecuteW(hwnd, L"open", exe.c_str(), nullptr, g_installPath.c_str(), SW_SHOWNORMAL);
            PostQuitMessage(0);
            break;
        }
        }
        break;

    case WM_INSTALL_STATUS: {
        wchar_t* txt = reinterpret_cast<wchar_t*>(lp);
        if (txt) { SetWindowTextW(g_hwndStatus, txt); delete[] txt; }
        break;
    }

    case WM_INSTALL_PROGRESS:
        SendMessageW(g_hwndProgress, PBM_SETPOS, wp, 0);
        break;

    case WM_INSTALL_DONE:
        g_ok = (wp == 1);
        g_page = WIZ_FINISH;
        SetPageUI(g_page);
        break;

    case WM_CLOSE:
        if (g_page != WIZ_INSTALL) PostQuitMessage(0);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ════════════════════════════════════════════════════════════════════════════
//  Entry point
// ════════════════════════════════════════════════════════════════════════════
int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    g_fTitle = CreateFontW(-22, 0, 0, 0, FW_BOLD,   0, 0, 0, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_fSub   = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_fBody  = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_fBold  = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD,0, 0, 0, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_appIcon = LoadIconW(hi, MAKEINTRESOURCEW(IDI_APP));

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.hInstance     = hi;
    wc.lpfnWndProc   = WizardProc;
    wc.lpszClassName = L"CrossToolsSetup";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = wc.hIconSm = g_appIcon;
    RegisterClassExW(&wc);

    RECT rcClient = { 0, 0, CL_W, CL_H };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&rcClient, style, FALSE);
    int wW = rcClient.right  - rcClient.left;
    int wH = rcClient.bottom - rcClient.top;
    int wX = (GetSystemMetrics(SM_CXSCREEN) - wW) / 2;
    int wY = (GetSystemMetrics(SM_CYSCREEN) - wH) / 2;

    g_hwnd = CreateWindowExW(0, L"CrossToolsSetup", L"CrossTools Setup",
                              style, wX, wY, wW, wH,
                              nullptr, nullptr, hi, nullptr);

    // ── Banner (always visible) ───────────────────────────────────────────────
    g_hwndTitle = CreateWindowExW(0, L"STATIC", L"",
                                   WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   MARGIN, 14, CL_W - 90, 30,
                                   g_hwnd, (HMENU)IDC_TITLE, hi, nullptr);
    SendMessageW(g_hwndTitle, WM_SETFONT, (WPARAM)g_fTitle, TRUE);

    g_hwndSubtitle = CreateWindowExW(0, L"STATIC", L"",
                                      WS_CHILD | WS_VISIBLE | SS_LEFT,
                                      MARGIN + 10, 50, CL_W - 110, 20,
                                      g_hwnd, (HMENU)IDC_SUBTITLE, hi, nullptr);
    SendMessageW(g_hwndSubtitle, WM_SETFONT, (WPARAM)g_fSub, TRUE);

    // ── Body text (Welcome / Finish) ──────────────────────────────────────────
    g_hwndBody = CreateWindowExW(0, L"STATIC", L"",
                                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  MARGIN, CONTENT_Y + 10, CL_W - MARGIN*2, 200,
                                  g_hwnd, (HMENU)IDC_BODY, hi, nullptr);
    SendMessageW(g_hwndBody, WM_SETFONT, (WPARAM)g_fBody, TRUE);

    // ── Page 1: Pathway selection ─────────────────────────────────────────────
    {
        int cy = CONTENT_Y + 16;
        g_hwndModeCf = CreateWindowExW(0, L"BUTTON",
            L"CrossFire adjacent installation (Recommended for CrossFire users)",
            WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
            MARGIN, cy, CL_W - MARGIN*2, 20,
            g_hwnd, (HMENU)IDC_MODE_CF, hi, nullptr);
        SendMessageW(g_hwndModeCf, WM_SETFONT, (WPARAM)g_fBold, TRUE);
        SendMessageW(g_hwndModeCf, BM_SETCHECK, BST_CHECKED, 0); // default
        cy += 24;

        g_hwndModeCfDesc = CreateWindowExW(0, L"STATIC",
            L"Installs CrossTools directly inside your CrossFire folder alongside CrossFire.exe.\r\n"
            L"Automatically links to your existing CrossFire dictionaries.",
            WS_CHILD | SS_LEFT,
            MARGIN + 20, cy, CL_W - MARGIN*2 - 20, 38,
            g_hwnd, (HMENU)IDC_MODE_CF_DESC, hi, nullptr);
        SendMessageW(g_hwndModeCfDesc, WM_SETFONT, (WPARAM)g_fBody, TRUE);
        cy += 50;

        g_hwndModeSa = CreateWindowExW(0, L"BUTTON",
            L"Standalone installation",
            WS_CHILD | BS_AUTORADIOBUTTON,
            MARGIN, cy, CL_W - MARGIN*2, 20,
            g_hwnd, (HMENU)IDC_MODE_STANDALONE, hi, nullptr);
        SendMessageW(g_hwndModeSa, WM_SETFONT, (WPARAM)g_fBold, TRUE);
        cy += 24;

        g_hwndModeSaDesc = CreateWindowExW(0, L"STATIC",
            L"Installs CrossTools as a separate application in Program Files.\r\n"
            L"Creates a dedicated dictionaries folder inside the installation directory.",
            WS_CHILD | SS_LEFT,
            MARGIN + 20, cy, CL_W - MARGIN*2 - 20, 38,
            g_hwnd, (HMENU)IDC_MODE_SA_DESC, hi, nullptr);
        SendMessageW(g_hwndModeSaDesc, WM_SETFONT, (WPARAM)g_fBody, TRUE);
    }

    // ── Page 2: Location & Config ─────────────────────────────────────────────
    {
        int cy = CONTENT_Y + 8;
        const int M  = MARGIN;
        const int EW = CL_W - M*2 - 82;
        const int BX = CL_W - M - 76;
        const int BW = 76;

        // Install / CrossFire folder row
        g_hwndLblInstall = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT,
                                           M, cy, CL_W - M*2, 18, g_hwnd, (HMENU)IDC_LBL_INSTALL, hi, nullptr);
        SendMessageW(g_hwndLblInstall, WM_SETFONT, (WPARAM)g_fBold, TRUE);
        cy += 22;

        g_hwndEditInstall = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL,
                                            M, cy, EW, 22, g_hwnd, (HMENU)IDC_EDIT_INSTALL, hi, nullptr);
        SendMessageW(g_hwndEditInstall, WM_SETFONT, (WPARAM)g_fBody, TRUE);

        g_hwndBtnInstall = CreateWindowExW(0, L"BUTTON", L"Browse\u2026", WS_CHILD | BS_PUSHBUTTON,
                                           BX, cy - 1, BW, 24, g_hwnd, (HMENU)IDC_BTN_INSTALL, hi, nullptr);
        SendMessageW(g_hwndBtnInstall, WM_SETFONT, (WPARAM)g_fBody, TRUE);
        cy += 36;

        // Dictionaries row
        g_hwndLblDict = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT,
                                        M, cy, CL_W - M*2, 18, g_hwnd, (HMENU)IDC_LBL_DICT, hi, nullptr);
        SendMessageW(g_hwndLblDict, WM_SETFONT, (WPARAM)g_fBold, TRUE);
        cy += 22;

        g_hwndRadioDictCr = CreateWindowExW(0, L"BUTTON",
            L"Create a \"dictionaries\" folder inside the installation folder",
            WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
            M, cy, CL_W - M*2, 18, g_hwnd, (HMENU)IDC_RADIO_DICT_CREATE, hi, nullptr);
        SendMessageW(g_hwndRadioDictCr, WM_SETFONT, (WPARAM)g_fBody, TRUE);
        SendMessageW(g_hwndRadioDictCr, BM_SETCHECK, BST_CHECKED, 0);
        cy += 22;

        g_hwndRadioDictEx = CreateWindowExW(0, L"BUTTON",
            L"Use an existing dictionaries folder:",
            WS_CHILD | BS_AUTORADIOBUTTON,
            M, cy, 260, 18, g_hwnd, (HMENU)IDC_RADIO_DICT_EXIST, hi, nullptr);
        SendMessageW(g_hwndRadioDictEx, WM_SETFONT, (WPARAM)g_fBody, TRUE);
        cy += 22;

        g_hwndEditDict = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL,
                                         M, cy, EW, 22, g_hwnd, (HMENU)IDC_EDIT_DICT, hi, nullptr);
        SendMessageW(g_hwndEditDict, WM_SETFONT, (WPARAM)g_fBody, TRUE);

        g_hwndBtnDict = CreateWindowExW(0, L"BUTTON", L"Browse\u2026", WS_CHILD | BS_PUSHBUTTON,
                                        BX, cy - 1, BW, 24, g_hwnd, (HMENU)IDC_BTN_DICT, hi, nullptr);
        SendMessageW(g_hwndBtnDict, WM_SETFONT, (WPARAM)g_fBody, TRUE);
        cy += 40;

        // ClueDatabase row
        g_hwndLblCdb = CreateWindowExW(0, L"STATIC",
            L"ClueDatabase folder:  (optional \u2014 leave blank to skip)",
            WS_CHILD | SS_LEFT, M, cy, CL_W - M*2, 18, g_hwnd, (HMENU)IDC_LBL_CDB, hi, nullptr);
        SendMessageW(g_hwndLblCdb, WM_SETFONT, (WPARAM)g_fBold, TRUE);
        cy += 22;

        g_hwndEditCdb = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL,
                                        M, cy, EW, 22, g_hwnd, (HMENU)IDC_EDIT_CDB, hi, nullptr);
        SendMessageW(g_hwndEditCdb, WM_SETFONT, (WPARAM)g_fBody, TRUE);

        g_hwndBtnCdb = CreateWindowExW(0, L"BUTTON", L"Browse\u2026", WS_CHILD | BS_PUSHBUTTON,
                                       BX, cy - 1, BW, 24, g_hwnd, (HMENU)IDC_BTN_CDB, hi, nullptr);
        SendMessageW(g_hwndBtnCdb, WM_SETFONT, (WPARAM)g_fBody, TRUE);
    }

    // ── Page 3: Progress ─────────────────────────────────────────────────────
    {
        int cy = CONTENT_Y + 60;
        g_hwndProgress = CreateWindowExW(0, PROGRESS_CLASS, L"",
                                          WS_CHILD | PBS_SMOOTH,
                                          MARGIN, cy, CL_W - MARGIN*2, 22,
                                          g_hwnd, (HMENU)IDC_PROGRESS, hi, nullptr);
        cy += 36;
        g_hwndStatus = CreateWindowExW(0, L"STATIC", L"",
                                        WS_CHILD | SS_LEFT,
                                        MARGIN, cy, CL_W - MARGIN*2, 80,
                                        g_hwnd, (HMENU)IDC_STATUS, hi, nullptr);
        SendMessageW(g_hwndStatus, WM_SETFONT, (WPARAM)g_fBody, TRUE);
    }

    // ── Page 4: Finish ────────────────────────────────────────────────────────
    g_hwndLaunch = CreateWindowExW(0, L"BUTTON", L"Launch CrossTools now",
                                  WS_CHILD | BS_PUSHBUTTON,
                                  MARGIN, CONTENT_Y + 80, 180, 26,
                                  g_hwnd, (HMENU)IDC_LAUNCH, hi, nullptr);
    SendMessageW(g_hwndLaunch, WM_SETFONT, (WPARAM)g_fBody, TRUE);

    // ── Navigation buttons ────────────────────────────────────────────────────
    g_hwndBack = CreateWindowExW(0, L"BUTTON", L"< Back",
                                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  CL_W - 296, BTN_Y, 86, BTN_H,
                                  g_hwnd, (HMENU)IDC_BACK, hi, nullptr);
    SendMessageW(g_hwndBack, WM_SETFONT, (WPARAM)g_fBody, TRUE);

    g_hwndNext = CreateWindowExW(0, L"BUTTON", L"Next >",
                                  WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                  CL_W - 202, BTN_Y, 90, BTN_H,
                                  g_hwnd, (HMENU)IDC_NEXT, hi, nullptr);
    SendMessageW(g_hwndNext, WM_SETFONT, (WPARAM)g_fBody, TRUE);

    g_hwndCancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
                                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    CL_W - 100, BTN_Y, 86, BTN_H,
                                    g_hwnd, (HMENU)IDC_CANCEL, hi, nullptr);
    SendMessageW(g_hwndCancel, WM_SETFONT, (WPARAM)g_fBody, TRUE);

    // ── Start on Welcome page ─────────────────────────────────────────────────
    SetPageUI(WIZ_WELCOME);
    ShowWindow(g_hwnd, SW_SHOWNORMAL);
    UpdateWindow(g_hwnd);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }

    DeleteObject(g_fTitle);
    DeleteObject(g_fSub);
    DeleteObject(g_fBody);
    DeleteObject(g_fBold);
    CoUninitialize();
    return 0;
}
