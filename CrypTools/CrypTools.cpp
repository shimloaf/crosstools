// CrypTools.cpp - Native Win32 Cryptic Crossword Clue Creation & Analysis Tool
// Modeled on CrossTools architecture: plain Win32/C++, zero external dependencies,
// persistent top bar, multi-tab layout, compiled with MSVC cl.exe.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <cwctype>
#include <cstdio>

#include <shlobj.h>

#ifdef _MSC_VER
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#endif

#define IDI_APP 1
#define APP_TITLE L"CrypTools"
#define APP_CLASS L"CrypToolsWnd"
#define CHIP_CLASS L"CrypChipWnd"
#define CLUES_FILE  L"clues.dat"
#define IND_FILE    L"indicators.dat"
#define SUBS_FILE   L"substitutions.dat"

#define IDT_ANIM   101
#define IDT_RELOAD 102

#define WM_APP_CHIP_CLICK  (WM_APP + 1)
#define WM_APP_CHIP_CHANGE (WM_APP + 2)

enum {
    // Top persistent clue bar (visible on all tabs, like CrossTools's word bar)
    IDC_CLUE_LBL = 50,
    IDC_CLUE_EDIT,

    // Tab control
    IDC_TAB,

    // Tab 0: Clue Builder controls
    IDC_CHIP_WND = 100,
    IDC_ANSWER_LBL, IDC_ANSWER, IDC_COUNT_LBL,
    IDC_VISUALIZE, IDC_SAVE_DB, IDC_STATUS_BAR,
    IDC_SUB_INPLACE_EDIT,

    // Tab 1: Indicator Bank controls
    IDC_IND_FILTER_LBL, IDC_IND_FILTER,
    IDC_IND_SEARCH_LBL, IDC_IND_SEARCH,
    IDC_IND_COUNT_LBL, IDC_IND_LIST,
    IDC_IND_EX_TITLE, IDC_IND_EX_CLUE, IDC_IND_EX_EXPLAIN, IDC_IND_EX_TRY,
    IDC_IND_ADD_LBL, IDC_IND_ADD_EDIT, IDC_IND_ADD_CAT, IDC_IND_ADD_BTN, IDC_IND_REMOVE_BTN,

    // Tab 2: Substitutions controls (String scanner)
    IDC_SUB_INPUT_LBL, IDC_SUB_INPUT, IDC_SUB_USE_CLUE_BTN, IDC_SUB_DIR_BTN,
    IDC_SUB_MATCHES_LBL, IDC_SUB_MATCHES_LIST,
    IDC_SUB_FROM_LBL, IDC_SUB_FROM, IDC_SUB_TO_LBL, IDC_SUB_TO,
    IDC_SUB_ADD_BTN, IDC_SUB_REMOVE_BTN,

    // Tab 3: Tools controls
    IDC_TOOL_TAB_ANA = 200, IDC_TOOL_TAB_HID, IDC_TOOL_TAB_DIFF,

    // Tool 1: Anagrams
    IDC_TOOL_ANA_INPUT_LBL, IDC_TOOL_ANA_INPUT,
    IDC_TOOL_ANA_USE_FODDER, IDC_TOOL_ANA_USE_ANS, IDC_TOOL_ANA_COUNT,
    IDC_TOOL_ANA_LIST, IDC_TOOL_ANA_SET_ANS,

    // Tool 2: Hidden Words
    IDC_TOOL_HID_PHRASE_LBL, IDC_TOOL_HID_PHRASE, IDC_TOOL_HID_USE_CLUE,
    IDC_TOOL_HID_MINLEN_LBL, IDC_TOOL_HID_MINLEN,
    IDC_TOOL_HID_SCOPE_LBL, IDC_TOOL_HID_SCOPE,
    IDC_TOOL_HID_REV_LBL, IDC_TOOL_HID_REV,
    IDC_TOOL_HID_COUNT, IDC_TOOL_HID_LIST, IDC_TOOL_HID_SET_ANS,

    // Tool 3: Addition & Removal
    IDC_TOOL_DIFF_MAIN_LBL, IDC_TOOL_DIFF_MAIN,
    IDC_TOOL_DIFF_GOAL_LBL, IDC_TOOL_DIFF_GOAL,
    IDC_TOOL_DIFF_SWAP, IDC_TOOL_DIFF_USE_CLUE,
    IDC_TOOL_DIFF_RESULT,
    IDC_TOOL_DIFF_DEL_LBL, IDC_TOOL_DIFF_DEL_LIST,
    IDC_TOOL_DIFF_ADD_LBL, IDC_TOOL_DIFF_ADD_LIST,

    // Tab 4: Database controls
    IDC_DB_SEARCH_LBL = 300, IDC_DB_SEARCH, IDC_DB_COUNT,
    IDC_DB_LIST, IDC_DB_DETAIL,
    IDC_DB_DELETE_BTN, IDC_DB_COPY_BTN, IDC_DB_LOAD_BTN,

    // Tab 5: Config controls
    IDC_CFG_IGNORE_WS = 400, IDC_CFG_IGNORE_NUMS, IDC_CFG_IGNORE_PUNCT, IDC_CFG_IGNORE_SPECIAL,
    IDC_CFG_IGNORED_LBL, IDC_CFG_IGNORED_DISP, IDC_CFG_CHAR_NOTE,
    IDC_CFG_DICT_LBL, IDC_CFG_DICT_DIR, IDC_CFG_DICT_BROWSE, IDC_CFG_DICT_RELOAD, IDC_CFG_DICT_STATUS,
    IDC_CFG_DEF_SCOPE_LBL, IDC_CFG_DEF_SCOPE,
    IDC_CFG_DEF_MINLEN_LBL, IDC_CFG_DEF_MINLEN,
    IDC_CFG_DEF_DIR_LBL, IDC_CFG_DEF_DIR,
    IDC_CFG_REG_NOTE,
};

enum { TAB_BUILDER = 0, TAB_INDICATORS = 1, TAB_SUBS = 2, TAB_TOOLS = 3, TAB_DATABASE = 4, TAB_CONFIG = 5 };
enum { TOOL_ANAGRAM = 0, TOOL_HIDDEN = 1, TOOL_DIFF = 2 };

enum ChipLabel {
    LABEL_NONE = 0,        // Untagged
    LABEL_DEFINITION,      // Primary definition (Blue)
    LABEL_FODDER,          // Raw wordplay fodder (Green)
    LABEL_INDICATOR,       // Manipulation indicator (Orange)
    LABEL_SUBSTITUTION,    // Letter abbreviation/substitution (Purple)
    LABEL_CONNECTOR,       // Connector / link word (Gray)
    LABEL_DOUBLEDEF2,      // Second definition in double-def (Light Blue)
    LABEL_OTHER_FODDER,    // Generic Other Fodder (Teal)
    LABEL_COUNT
};

struct WordChip {
    std::wstring text;
    ChipLabel    label;
    std::wstring subText;
    std::wstring indType;
    int          x, y, w, h;
    bool         selected;
};

struct Indicator {
    std::wstring word;
    std::wstring category;
    std::wstring example;
    std::wstring explanation;
};

struct Substitution {
    std::wstring from;
    std::wstring to;
};

struct ClueEntry {
    std::wstring answer;
    std::wstring clue;
    std::wstring enumeration;
    std::wstring notes;
    std::wstring labels;
};

// ---------------------------------------------------------------------------
// Global Application State
// ---------------------------------------------------------------------------

static HINSTANCE g_hInst;
static HWND      g_hWnd;

// Fonts
static HFONT g_hFont;          // Standard UI font (Segoe UI 9pt)
static HFONT g_hBoldFont;      // Bold UI font (Segoe UI 9pt bold)
static HFONT g_hInputFont;     // Persistent clue input bar font (Segoe UI 14pt)
static HFONT g_hChipFont;      // Large word chip font (Segoe UI 22pt bold)
static HFONT g_hBadgeFont;     // Small badge pill font (Segoe UI 9pt bold)
static HFONT g_hAnimLargeFont; // Big resolution font (Segoe UI 18pt bold)

// Navigation
static HWND g_hTab;
static int  g_activeTab = TAB_BUILDER;
static int  g_activeTool = TOOL_ANAGRAM;

// Persistent Top Bar (Clue & Answer)
static HWND g_hClueLbl, g_hClueEdit;
static HWND g_hAnswerLbl, g_hAnswer, g_hCountLbl;

// Builder tab controls
static HWND g_hChipWnd;
static HWND g_hVisualize, g_hSaveDb, g_hStatusBar;
static HWND g_hInplaceEdit = nullptr;
static int  g_editingChipIdx = -1;

// Indicator Bank tab controls
static HWND g_hIndFilterLbl, g_hIndFilter;
static HWND g_hIndSearchLbl, g_hIndSearch;
static HWND g_hIndCountLbl, g_hIndList;
static HWND g_hIndExTitle, g_hIndExClue, g_hIndExExplain, g_hIndExTry;
static HWND g_hIndAddLbl, g_hIndAddEdit, g_hIndAddCat, g_hIndAddBtn, g_hIndRemoveBtn;

// Substitutions tab controls (String Scanner)
static HWND g_hSubInputLbl, g_hSubInput, g_hSubUseClueBtn, g_hSubDirBtn;
static HWND g_hSubMatchesLbl, g_hSubMatchesList;
static HWND g_hSubFromLbl, g_hSubFrom, g_hSubToLbl, g_hSubTo;
static HWND g_hSubAddBtn, g_hSubRemoveBtn;
static bool g_subDirectionReverse = false;

// Tools tab controls
static HWND g_hToolTabAna, g_hToolTabHid, g_hToolTabDiff;

// Tool 1: Anagrams
static HWND g_hToolAnaInputLbl, g_hToolAnaInput;
static HWND g_hToolAnaUseFodder, g_hToolAnaUseAns, g_hToolAnaCount;
static HWND g_hToolAnaList, g_hToolAnaSetAns;

// Tool 2: Hidden Words
static HWND g_hToolHidPhraseLbl, g_hToolHidPhrase, g_hToolHidUseClue;
static HWND g_hToolHidMinLenLbl, g_hToolHidMinLen;
static HWND g_hToolHidScopeLbl, g_hToolHidScope;
static HWND g_hToolHidRevLbl, g_hToolHidRev;
static HWND g_hToolHidCount, g_hToolHidList, g_hToolHidSetAns;

// Tool 3: Addition & Removal
static HWND g_hToolDiffMainLbl, g_hToolDiffMain;
static HWND g_hToolDiffGoalLbl, g_hToolDiffGoal;
static HWND g_hToolDiffSwap, g_hToolDiffUseClue;
static HWND g_hToolDiffResult;
static HWND g_hToolDiffDelLbl, g_hToolDiffDelList;
static HWND g_hToolDiffAddLbl, g_hToolDiffAddList;

// Tab 4: Database controls
static HWND g_hDbSearchLbl, g_hDbSearch, g_hDbCount;
static HWND g_hDbList, g_hDbDetail;
static HWND g_hDbDeleteBtn, g_hDbCopyBtn, g_hDbLoadBtn;

// Tab 5: Config controls (Matching CrossTools Config page)
static HWND g_hCfgIgnoredLbl, g_hCfgIgnoreWs, g_hCfgIgnoreNums, g_hCfgIgnorePunct, g_hCfgIgnoreSpecial;
static HWND g_hCfgIgnoredDisp, g_hCfgCharNote;
static HWND g_hCfgDictLbl, g_hCfgDictDir, g_hCfgDictBrowse, g_hCfgDictReload, g_hCfgDictStatus;
static HWND g_hCfgDefScopeLbl, g_hCfgDefScope;
static HWND g_hCfgDefMinLenLbl, g_hCfgDefMinLen;
static HWND g_hCfgDefDirLbl, g_hCfgDefDir;
static HWND g_hCfgRegNote;

static std::wstring g_cfgDictDir = L"dictionaries";
static int  g_cfgIgnoreWs = 1;
static int  g_cfgIgnoreNums = 1;
static int  g_cfgIgnorePunct = 1;
static int  g_cfgIgnoreSpecial = 1;
static int  g_cfgHiddenMinLen = 3;
static int  g_cfgHiddenScope = 0; // 0 = Boundary Spanning (Default)

// Character groups
static const wchar_t* kWhitespace = L" \t\u00A0\u3000";
static const wchar_t* kNumbers = L"0123456789";
static const wchar_t* kPunct = L"!\"%&'()*,-./:;?@[\\]_`{}~#$^+=|<>\u2010\u2013\u2014\u2018\u2019\u201C\u201D\u00AB\u00BB\u2026";
static const wchar_t* kSpecial = L"/\u00A7\u00A9\u00AE\u00B0\u00B1\u00B2\u00B3\u00B6\u00BC\u00BD\u00BE\u00D7\u00F7\u20AC\u00A3\u00A5\u00A2";

// Interactive canvas state
static std::vector<WordChip> g_chips;
static int  g_animStep = 0;         // 0 = idle, 1..N = collapse sequence
static int  g_dragStart = -1;
static int  g_dragEnd   = -1;
static bool g_dragging  = false;
static bool g_visMode   = false;

// App Data
static std::vector<Indicator>    g_indicators;
static std::vector<Substitution> g_substitutions;
static std::vector<ClueEntry>    g_clues;

// Lexicon for Anagrams, Hidden Words, Additions & Removals
static std::vector<std::wstring> g_dictWords;
static std::unordered_map<std::wstring, std::vector<std::wstring>> g_anagramLookup;
static std::unordered_set<std::wstring> g_dictSet;

// File Paths
static wchar_t g_exeDir[MAX_PATH];
static wchar_t g_cluesPath[MAX_PATH];
static wchar_t g_indPath[MAX_PATH];
static wchar_t g_subsPath[MAX_PATH];

// Forward declarations
static void SetStatus(const wchar_t* text);
static void RebuildChips(const std::wstring& clueText);
static void RefreshIndicatorList();
static void ShowIndicatorDetail();
static void RefreshSubstitutionList();
static void ScanSubstitutionsInPhrase();
static void RefreshDatabaseList();
static void StopAnimation();
static void ComputeAnagrams();
static void ComputeHiddenWords();
static void ComputeLetterDiff();
struct LabelChoice {
    ChipLabel label;
    std::wstring indType;
    bool editNow;
};
static LabelChoice ShowLabelMenu(HWND hWnd, POINT screenPt, int chipIdx = -1);
static void ShowToolView(int toolIndex);
static void LayoutControls(HWND hWnd);

// ---------------------------------------------------------------------------
// String & Utility Helpers
// ---------------------------------------------------------------------------

static std::wstring Trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos) return L"";
    size_t b = s.find_last_not_of(L" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::wstring ToUpper(const std::wstring& s) {
    std::wstring r = s;
    for (wchar_t& c : r) c = (wchar_t)towupper(c);
    return r;
}

static std::wstring ToLower(const std::wstring& s) {
    std::wstring r = s;
    for (wchar_t& c : r) c = (wchar_t)towlower(c);
    return r;
}

static std::wstring StripPunct(const std::wstring& s) {
    std::wstring r;
    for (wchar_t c : s) if (iswalpha(c)) r += (wchar_t)towupper(c);
    return r;
}

static std::vector<std::wstring> Split(const std::wstring& s, wchar_t delim) {
    std::vector<std::wstring> tokens;
    size_t start = 0;
    while (start < s.size()) {
        size_t p = s.find(delim, start);
        if (p == std::wstring::npos) {
            tokens.push_back(s.substr(start));
            break;
        }
        tokens.push_back(s.substr(start, p - start));
        start = p + 1;
    }
    return tokens;
}

static std::vector<std::wstring> SplitBySpace(const std::wstring& s) {
    std::vector<std::wstring> tokens;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && iswspace(s[i])) ++i;
        if (i >= s.size()) break;
        size_t start = i;
        while (i < s.size() && !iswspace(s[i])) ++i;
        tokens.push_back(s.substr(start, i - start));
    }
    return tokens;
}

static std::wstring GetWindowTextW_s(HWND hWnd) {
    if (!hWnd) return L"";
    int len = GetWindowTextLengthW(hWnd);
    if (len <= 0) return L"";
    std::wstring buf(len + 1, L'\0');
    GetWindowTextW(hWnd, &buf[0], len + 1);
    buf.resize(len);
    return buf;
}

static std::wstring ComputeEnumeration(const std::wstring& answer) {
    int count = 0;
    for (wchar_t c : answer) if (iswalpha(c)) ++count;
    if (count == 0) return L"(0)";
    return L"(" + std::to_wstring(count) + L")";
}

static COLORREF ChipBgColor(ChipLabel lbl) {
    switch (lbl) {
    case LABEL_DEFINITION: return RGB(173, 216, 255); // Light Blue
    case LABEL_FODDER:     return RGB(198, 239, 206); // Light Green
    case LABEL_INDICATOR:  return RGB(255, 220, 153); // Light Orange
    case LABEL_SUBSTITUTION: return RGB(220, 190, 255); // Light Purple
    case LABEL_CONNECTOR:  return RGB(220, 220, 220); // Light Gray
    case LABEL_DOUBLEDEF2: return RGB(220, 235, 255); // Very Light Blue
    case LABEL_OTHER_FODDER: return RGB(195, 245, 235); // Soft Teal
    default:               return RGB(245, 247, 250); // Untagged
    }
}

static COLORREF ChipBorderColor(ChipLabel lbl, bool selected) {
    if (selected) return RGB(0, 80, 220);
    switch (lbl) {
    case LABEL_DEFINITION: return RGB(70, 130, 230);
    case LABEL_FODDER:     return RGB(46, 139, 87);
    case LABEL_INDICATOR:  return RGB(210, 105, 30);
    case LABEL_SUBSTITUTION: return RGB(138, 43, 226);
    case LABEL_CONNECTOR:  return RGB(140, 140, 140);
    case LABEL_DOUBLEDEF2: return RGB(100, 160, 230);
    case LABEL_OTHER_FODDER: return RGB(20, 160, 140);
    default:               return RGB(180, 190, 205);
    }
}

static const wchar_t* LabelShortTag(ChipLabel lbl) {
    switch (lbl) {
    case LABEL_DEFINITION: return L"DEF";
    case LABEL_FODDER:     return L"FODDER";
    case LABEL_INDICATOR:  return L"IND";
    case LABEL_SUBSTITUTION: return L"SUB";
    case LABEL_CONNECTOR:  return L"LINK";
    case LABEL_DOUBLEDEF2: return L"DEF 2";
    case LABEL_OTHER_FODDER: return L"OTHER";
    default:               return L"";
    }
}

static std::wstring LookupSubstitution(const std::wstring& word) {
    std::wstring clean = ToLower(StripPunct(word));
    for (auto& s : g_substitutions) {
        if (ToLower(s.from) == clean) return s.to;
    }
    return L"";
}

// ---------------------------------------------------------------------------
// File & Settings Management
// ---------------------------------------------------------------------------

static void BuildPaths() {
    GetModuleFileNameW(nullptr, g_exeDir, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(g_exeDir, L'\\');
    if (lastSlash) *lastSlash = L'\0';

    swprintf_s(g_cluesPath, L"%s\\%s", g_exeDir, CLUES_FILE);
    swprintf_s(g_indPath,   L"%s\\%s", g_exeDir, IND_FILE);
    swprintf_s(g_subsPath,  L"%s\\%s", g_exeDir, SUBS_FILE);
}

// Windows Registry settings (per-user: HKEY_CURRENT_USER\Software\CrypTools)
static const wchar_t* kRegKey = L"Software\\CrypTools";

static std::wstring ResolveFolder(std::wstring s) {
    size_t a = s.find_first_not_of(L" \t"), b = s.find_last_not_of(L" \t");
    if (a == std::wstring::npos) return L"";
    s = s.substr(a, b - a + 1);
    if (s.size() >= 2 && (s[1] == L':' || (s[0] == L'\\' && s[1] == L'\\'))) return s;
    return std::wstring(g_exeDir) + L"\\" + s;
}

static std::wstring BrowseFolder(HWND parent, const wchar_t* title) {
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = parent;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return L"";
    wchar_t path[MAX_PATH];
    if (SHGetPathFromIDListW(pidl, path)) {
        CoTaskMemFree(pidl);
        return path;
    }
    CoTaskMemFree(pidl);
    return L"";
}

static std::wstring GetIgnoredChars() {
    std::wstring s;
    if (g_cfgIgnoreWs)      s += kWhitespace;
    if (g_cfgIgnoreNums)    s += kNumbers;
    if (g_cfgIgnorePunct)   s += kPunct;
    if (g_cfgIgnoreSpecial) s += kSpecial;
    return s;
}

static void RefreshIgnoredDisplay() {
    if (!g_hCfgIgnoredDisp) return;
    std::wstring s = GetIgnoredChars();
    SetWindowTextW(g_hCfgIgnoredDisp, s.empty() ? L"(nothing is ignored)" : s.c_str());
}

static void Settings(bool save) {
    HKEY k = nullptr;
    if (save) {
        if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    } else if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegKey, 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS) {
        return;
    }

    if (save) {
        wchar_t b[MAX_PATH + 1] = { 0 };
        if (g_hCfgDictDir) GetWindowTextW(g_hCfgDictDir, b, MAX_PATH);
        else wcscpy_s(b, g_cfgDictDir.c_str());
        RegSetValueExW(k, L"DictFolder", 0, REG_SZ, (const BYTE*)b, (DWORD)((wcslen(b) + 1) * sizeof(wchar_t)));
    } else {
        DWORD type = 0, size = MAX_PATH * sizeof(wchar_t);
        wchar_t b[MAX_PATH + 1] = { 0 };
        if (RegQueryValueExW(k, L"DictFolder", nullptr, &type, (BYTE*)b, &size) == ERROR_SUCCESS && type == REG_SZ) {
            b[size / sizeof(wchar_t)] = 0;
            g_cfgDictDir = b;
            if (g_hCfgDictDir) SetWindowTextW(g_hCfgDictDir, b);
        }
    }

    struct DwordSetting { const wchar_t* name; int* val; HWND h; bool isCheck; };
    DwordSetting dwSettings[] = {
        { L"ActiveTool",          &g_activeTool,          nullptr,              false },
        { L"IgnoreWhitespace",    &g_cfgIgnoreWs,         g_hCfgIgnoreWs,       true  },
        { L"IgnoreNumbers",       &g_cfgIgnoreNums,       g_hCfgIgnoreNums,     true  },
        { L"IgnorePunctuation",   &g_cfgIgnorePunct,      g_hCfgIgnorePunct,    true  },
        { L"IgnoreSpecial",       &g_cfgIgnoreSpecial,    g_hCfgIgnoreSpecial,  true  },
        { L"SubDirectionReverse", (int*)&g_subDirectionReverse, nullptr,        false },
        { L"HiddenMinLen",        &g_cfgHiddenMinLen,     g_hToolHidMinLen,     false },
        { L"HiddenScope",         &g_cfgHiddenScope,      g_hToolHidScope,      false }
    };

    for (auto& d : dwSettings) {
        DWORD v = 0;
        if (save) {
            if (d.isCheck && d.h) v = (SendMessageW(d.h, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
            else if (!d.isCheck && d.h) v = (DWORD)SendMessageW(d.h, CB_GETCURSEL, 0, 0);
            else if (d.val) v = (DWORD)*d.val;
            RegSetValueExW(k, d.name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        } else {
            DWORD type = 0, size = sizeof(v);
            if (RegQueryValueExW(k, d.name, nullptr, &type, (BYTE*)&v, &size) == ERROR_SUCCESS && type == REG_DWORD) {
                if (d.val) *d.val = (int)v;
                if (d.isCheck && d.h) SendMessageW(d.h, BM_SETCHECK, v ? BST_CHECKED : BST_UNCHECKED, 0);
                else if (!d.isCheck && d.h) SendMessageW(d.h, CB_SETCURSEL, v, 0);
            }
        }
    }

    RegCloseKey(k);
}

static void LoadSettings() { Settings(false); }
static void SaveSettings() { Settings(true); }

static void LoadIndicators() {
    g_indicators.clear();
    FILE* f = nullptr;
    if (_wfopen_s(&f, g_indPath, L"r, ccs=UTF-8") != 0 || !f) {
        // Fallback relative path
        std::wstring alt = std::wstring(g_exeDir) + L"\\..\\CrypTools\\indicators.dat";
        _wfopen_s(&f, alt.c_str(), L"r, ccs=UTF-8");
    }
    if (!f) return;

    wchar_t line[1024];
    std::wstring curCat = L"anagram";
    Indicator curInd;
    bool hasInd = false;

    while (fgetws(line, 1024, f)) {
        std::wstring s = Trim(line);
        if (s.empty()) continue;

        if (s[0] == L'#') {
            if (hasInd) { g_indicators.push_back(curInd); hasInd = false; }
            curCat = Trim(s.substr(1));
            continue;
        }

        if (s.find(L"EX:") == 0) {
            if (hasInd) curInd.example = Trim(s.substr(3));
            continue;
        }
        if (s.find(L"EXPL:") == 0) {
            if (hasInd) curInd.explanation = Trim(s.substr(5));
            continue;
        }

        if (s.find(L'|') != std::wstring::npos) {
            if (hasInd) { g_indicators.push_back(curInd); hasInd = false; }
            auto parts = Split(s, L'|');
            if (!parts.empty()) {
                Indicator ind;
                ind.word = Trim(parts[0]);
                ind.category = curCat.empty() ? L"anagram" : curCat;
                if (parts.size() > 1) ind.example = Trim(parts[1]);
                if (parts.size() > 2) ind.explanation = Trim(parts[2]);
                if (!ind.word.empty()) g_indicators.push_back(ind);
            }
            continue;
        }

        if (hasInd) { g_indicators.push_back(curInd); }
        curInd.word = s;
        curInd.category = curCat.empty() ? L"anagram" : curCat;
        curInd.example.clear();
        curInd.explanation.clear();
        hasInd = true;
    }
    if (hasInd && !curInd.word.empty()) g_indicators.push_back(curInd);
    fclose(f);
}

static void SaveIndicators() {
    FILE* f = nullptr;
    if (_wfopen_s(&f, g_indPath, L"w, ccs=UTF-8") != 0 || !f) return;

    std::wstring curCat;
    for (auto& ind : g_indicators) {
        if (ind.category != curCat) {
            curCat = ind.category;
            fwprintf(f, L"\n# %s\n", curCat.c_str());
        }
        fwprintf(f, L"%s\n", ind.word.c_str());
        if (!ind.example.empty())     fwprintf(f, L"EX: %s\n", ind.example.c_str());
        if (!ind.explanation.empty()) fwprintf(f, L"EXPL: %s\n", ind.explanation.c_str());
    }
    fclose(f);
}

static void LoadSubstitutions() {
    g_substitutions.clear();
    FILE* f = nullptr;
    if (_wfopen_s(&f, g_subsPath, L"r, ccs=UTF-8") != 0 || !f) {
        std::wstring alt = std::wstring(g_exeDir) + L"\\..\\CrypTools\\substitutions.dat";
        _wfopen_s(&f, alt.c_str(), L"r, ccs=UTF-8");
    }
    if (!f) return;

    wchar_t line[512];
    while (fgetws(line, 512, f)) {
        std::wstring s = Trim(line);
        if (s.empty() || s[0] == L'#') continue;
        size_t eq = s.find(L'=');
        if (eq != std::wstring::npos) {
            Substitution sub;
            sub.from = Trim(s.substr(0, eq));
            sub.to   = Trim(s.substr(eq + 1));
            if (!sub.from.empty() && !sub.to.empty()) g_substitutions.push_back(sub);
        }
    }
    fclose(f);
}

static void SaveSubstitutions() {
    FILE* f = nullptr;
    if (_wfopen_s(&f, g_subsPath, L"w, ccs=UTF-8") != 0 || !f) return;
    fwprintf(f, L"# Common abbreviations and substitutions\n");
    for (auto& s : g_substitutions) {
        fwprintf(f, L"%s=%s\n", s.from.c_str(), s.to.c_str());
    }
    fclose(f);
}

static void LoadClues() {
    g_clues.clear();
    FILE* f = nullptr;
    if (_wfopen_s(&f, g_cluesPath, L"r, ccs=UTF-8") != 0 || !f) return;

    wchar_t line[1024];
    ClueEntry cur;
    bool inClue = false;

    while (fgetws(line, 1024, f)) {
        std::wstring s = Trim(line);
        if (s.empty()) {
            if (inClue) { g_clues.push_back(cur); cur = ClueEntry(); inClue = false; }
            continue;
        }
        if (s.find(L"ANSWER:") == 0)      { cur.answer = Trim(s.substr(7)); inClue = true; }
        else if (s.find(L"CLUE:") == 0)   { cur.clue = Trim(s.substr(5)); inClue = true; }
        else if (s.find(L"TYPE:") == 0)   { /* legacy clue type ignored */ }
        else if (s.find(L"ENUMERATION:") == 0) { cur.enumeration = Trim(s.substr(12)); }
        else if (s.find(L"NOTES:") == 0)  { cur.notes = Trim(s.substr(6)); }
        else if (s.find(L"LABELS:") == 0) { cur.labels = Trim(s.substr(7)); }
    }
    if (inClue) g_clues.push_back(cur);
    fclose(f);
}

static void SaveClues() {
    FILE* f = nullptr;
    if (_wfopen_s(&f, g_cluesPath, L"w, ccs=UTF-8") != 0 || !f) return;
    for (auto& c : g_clues) {
        fwprintf(f, L"ANSWER: %s\n", c.answer.c_str());
        fwprintf(f, L"CLUE: %s\n", c.clue.c_str());
        fwprintf(f, L"ENUMERATION: %s\n", c.enumeration.c_str());
        fwprintf(f, L"NOTES: %s\n", c.notes.c_str());
        fwprintf(f, L"LABELS: %s\n\n", c.labels.c_str());
    }
    fclose(f);
}

static std::wstring SerializeLabels() {
    std::wstring r;
    static const wchar_t* kNames[] = { L"none", L"definition", L"fodder", L"indicator", L"substitution", L"connector", L"doubledef2", L"other_fodder" };
    for (size_t i = 0; i < g_chips.size(); ++i) {
        if (!r.empty()) r += L" ";
        r += std::to_wstring(i) + L":" + kNames[(int)g_chips[i].label];
        if (g_chips[i].label == LABEL_INDICATOR && !g_chips[i].indType.empty()) {
            r += L"[" + g_chips[i].indType + (g_chips[i].subText.empty() ? L"" : L"=" + g_chips[i].subText) + L"]";
        } else if (g_chips[i].label == LABEL_SUBSTITUTION && !g_chips[i].subText.empty()) {
            r += L"[" + g_chips[i].subText + L"]";
        } else if (g_chips[i].label == LABEL_OTHER_FODDER && !g_chips[i].subText.empty()) {
            r += L"[" + g_chips[i].subText + L"]";
        }
    }
    return r;
}

// ---------------------------------------------------------------------------
// Lexicon Loader (Fast multi-file loader for *.dict in dictionaries folder)
// ---------------------------------------------------------------------------

static void LoadFallbackWords() {
    static const wchar_t* kFallback[] = {
        L"STARE", L"TEARS", L"RATES", L"ASTER", L"TARES", L"ALERT", L"ALTER", L"LATER",
        L"HEART", L"EARTH", L"HATER", L"SHARE", L"SHEAR", L"HORSE", L"SHORE", L"STONE",
        L"NOTES", L"TONES", L"ONSET", L"SNAKE", L"SNEAK", L"LEMON", L"MELON", L"SMILE",
        L"SLIME", L"MILES", L"LIMES", L"BREAD", L"BEARD", L"DEBAR", L"PEACH", L"CHEAP",
        L"TRAIN", L"PAINT", L"POINT", L"GREAT", L"GRATE", L"STORM", L"NIGHT", L"THING",
        L"LIGHT", L"STAND", L"PLANT", L"WATER", L"RIVER", L"OCEAN", L"CANOE", L"CROSS",
        L"CRYPT", L"CLUES", L"SOLVE", L"WORDS", L"SWORD", L"BOARD", L"BROAD", L"CLEAN",
        L"CLOUD", L"CROWD", L"DANCE", L"DREAM", L"DRIVE", L"EARLY", L"EMPTY", L"EQUAL",
        L"FAITH", L"FIELD", L"FIGHT", L"FINAL", L"FIRST", L"FLASH", L"FLOOR", L"FOCUS",
        L"FORCE", L"FRESH", L"FRONT", L"FRUIT", L"GLASS", L"GRACE", L"GREEN", L"GROUP",
        L"GUARD", L"GUESS", L"GUIDE", L"HAPPY", L"HEAVY", L"HONOR", L"HOUSE", L"HUMAN",
        L"IMAGE", L"INDEX", L"INNER", L"INPUT", L"ISSUE", L"JUDGE", L"KNIFE", L"LAUGH",
        L"LEARN", L"LEAST", L"LEAVE", L"LEGAL", L"LEVEL", L"LOCAL", L"LOGIC", L"LOOSE",
        L"LOVER", L"LUCKY", L"LUNCH", L"MAJOR", L"MAKER", L"MARCH", L"MATCH", L"MAYOR",
        L"MEDIA", L"METAL", L"MIGHT", L"MINOR", L"MODEL", L"MONEY", L"MONTH", L"MORAL",
        L"MOTOR", L"MOUNT", L"MOUSE", L"MOUTH", L"MOVIE", L"MUSIC", L"NEEDS", L"NEVER",
        L"NOISE", L"NORTH", L"NOVEL", L"NURSE", L"OCCUR", L"OFFER", L"ORDER", L"OTHER",
        L"OUGHT", L"PANEL", L"PAPER", L"PARTY", L"PEACE", L"PHASE", L"PHONE", L"PHOTO",
        L"PIECE", L"PILOT", L"PITCH", L"PLACE", L"PLAIN", L"PLANE", L"PLATE", L"PLAZA",
        L"POETRY", L"POWER", L"PRESS", L"PRICE", L"PRIDE", L"PRIME", L"PRIZE", L"PROOF",
        L"PROUD", L"PROVE", L"QUEEN", L"QUICK", L"QUIET", L"QUITE", L"RADIO", L"RAISE",
        L"RANGE", L"RAPID", L"RATIO", L"REACH", L"READY", L"RIGHT", L"ROBOT", L"ROUGH",
        L"ROUND", L"ROUTE", L"ROYAL", L"RULER", L"SCALE", L"SCENE", L"SCOPE", L"SCORE",
        L"SENSE", L"SERVE", L"SHAPE", L"SHARE", L"SHARP", L"SHEET", L"SHELF", L"SHELL",
        L"SHIFT", L"SHIRT", L"SHOCK", L"SHOOT", L"SHORT", L"SHOWN", L"SIGHT", L"SINCE",
        L"SKILL", L"SMART", L"SMOKE", L"SOLID", L"SOLVE", L"SORRY", L"SOUND", L"SOUTH",
        L"SPACE", L"SPARE", L"SPEAK", L"SPEED", L"SPEND", L"SPENT", L"SPLIT", L"SPOKE",
        L"SPORT", L"STAFF", L"STAGE", L"STAKE", L"STAMP", L"STAND", L"START", L"STATE",
        L"STEAM", L"STEEL", L"STICK", L"STILL", L"STOCK", L"STORE", L"STORY", L"STRIP",
        L"STUDY", L"STUFF", L"STYLE", L"SUGAR", L"SUITE", L"SUPER", L"SWEET", L"TABLE",
        L"TAKEN", L"TASTE", L"TAXES", L"TEACH", L"TEETH", L"TERMS", L"THANK", L"THEFT",
        L"THEIR", L"THEME", L"THERE", L"THESE", L"THICK", L"THING", L"THINK", L"THIRD",
        L"THOSE", L"THREE", L"THREW", L"THROW", L"TIGHT", L"TIMES", L"TIRED", L"TITLE"
    };
    for (auto w : kFallback) {
        std::wstring sw(w);
        if (g_dictSet.insert(sw).second) {
            g_dictWords.push_back(sw);
            std::wstring s = sw;
            std::sort(s.begin(), s.end());
            g_anagramLookup[s].push_back(sw);
        }
    }
}

static bool LoadSingleDictFile(const std::wstring& path) {
    HANDLE hF = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hF == INVALID_HANDLE_VALUE) return false;

    DWORD sz = GetFileSize(hF, nullptr);
    if (sz == 0 || sz == INVALID_FILE_SIZE) {
        CloseHandle(hF);
        return false;
    }

    std::string content(sz, '\0');
    DWORD read = 0;
    ReadFile(hF, &content[0], sz, &read, nullptr);
    CloseHandle(hF);
    if (read == 0) return false;

    const char* ptr = content.data();
    size_t len = read;
    size_t pos = 0;
    if (len >= 3 && (unsigned char)ptr[0] == 0xEF && (unsigned char)ptr[1] == 0xBB && (unsigned char)ptr[2] == 0xBF) pos = 3;

    std::wstring word;
    word.reserve(32);
    int added = 0;
    std::wstring ignored = GetIgnoredChars();

    while (pos < len) {
        size_t nextNl = pos;
        while (nextNl < len && ptr[nextNl] != '\n' && ptr[nextNl] != '\r') ++nextNl;

        size_t semi = pos;
        while (semi < nextNl && ptr[semi] != ';') ++semi;

        word.clear();
        bool ok = true;
        for (size_t k = pos; k < semi; ++k) {
            unsigned char c = (unsigned char)ptr[k];
            wchar_t wc = (wchar_t)c;
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                word += (wchar_t)towupper(wc);
            } else if (ignored.find(wc) != std::wstring::npos || c == ' ' || c == '-' || c == '\'') {
                // skip ignored
            } else {
                ok = false;
                break;
            }
        }

        if (ok && word.size() >= 2 && word.size() <= 28) {
            if (g_dictSet.insert(word).second) {
                g_dictWords.push_back(word);
                std::wstring sorted = word;
                std::sort(sorted.begin(), sorted.end());
                g_anagramLookup[sorted].push_back(word);
                ++added;
            }
        }
        pos = nextNl;
        while (pos < len && (ptr[pos] == '\n' || ptr[pos] == '\r')) ++pos;
    }
    return added > 0;
}

static void LoadDictionary() {
    g_dictWords.clear();
    g_anagramLookup.clear();
    g_dictSet.clear();

    std::wstring folder = ResolveFolder(g_cfgDictDir.empty() ? L"dictionaries" : g_cfgDictDir);
    int filesLoaded = 0;

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW((folder + L"\\*.dict").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::wstring filePath = folder + L"\\" + fd.cFileName;
                if (LoadSingleDictFile(filePath)) filesLoaded++;
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    if (filesLoaded == 0) {
        std::wstring fallbacks[] = {
            std::wstring(g_exeDir) + L"\\dictionaries\\default.dict",
            std::wstring(g_exeDir) + L"\\..\\Installation\\dictionaries\\default.dict",
            std::wstring(g_exeDir) + L"\\..\\CrossTools\\dictionaries\\default.dict"
        };
        for (auto& fb : fallbacks) {
            if (LoadSingleDictFile(fb)) {
                filesLoaded++;
                break;
            }
        }
    }

    if (g_dictWords.empty()) {
        LoadFallbackWords();
    }

    if (g_hCfgDictStatus) {
        wchar_t buf[256];
        swprintf_s(buf, L"Loaded %d words from %d dictionary file(s) (*.dict).", (int)g_dictWords.size(), filesLoaded);
        SetWindowTextW(g_hCfgDictStatus, buf);
    }
}

// ---------------------------------------------------------------------------
// Word Chip Geometry & Invalidation (Much Bigger Words)
// ---------------------------------------------------------------------------

#define CHIP_PAD_HORZ 18
#define CHIP_HEIGHT   76
#define CHIP_GAP_X    10
#define CHIP_GAP_Y    12

static void RebuildChips(const std::wstring& clueText) {
    std::vector<std::tuple<std::wstring, ChipLabel, std::wstring, std::wstring>> oldState;
    for (auto& c : g_chips) oldState.push_back({ c.text, c.label, c.subText, c.indType });

    g_chips.clear();
    auto tokens = SplitBySpace(clueText);
    if (tokens.empty() || !g_hChipWnd) return;

    HDC hdc = GetDC(g_hChipWnd);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_hChipFont);

    RECT clientRect;
    GetClientRect(g_hChipWnd, &clientRect);
    int maxW = clientRect.right - 24;
    if (maxW < 120) maxW = 700;

    int curX = 16;
    int curY = 16;

    for (size_t i = 0; i < tokens.size(); ++i) {
        SIZE sz;
        GetTextExtentPoint32W(hdc, tokens[i].c_str(), (int)tokens[i].size(), &sz);
        int chipW = sz.cx + CHIP_PAD_HORZ * 2;
        if (chipW < 75) chipW = 75;

        if (curX + chipW > maxW && curX > 16) {
            curX = 16;
            curY += CHIP_HEIGHT + CHIP_GAP_Y;
        }

        WordChip chip;
        chip.text = tokens[i];
        chip.label = LABEL_NONE;
        chip.x = curX;
        chip.y = curY;
        chip.w = chipW;
        chip.h = CHIP_HEIGHT;
        chip.selected = false;

        // Restore state if tokens match
        if (i < oldState.size()) {
            if (std::get<0>(oldState[i]) == chip.text) {
                chip.label   = std::get<1>(oldState[i]);
                chip.subText = std::get<2>(oldState[i]);
                chip.indType = std::get<3>(oldState[i]);
            }
        }

        g_chips.push_back(chip);
        curX += chipW + CHIP_GAP_X;
    }

    SelectObject(hdc, oldFont);
    ReleaseDC(g_hChipWnd, hdc);
}

static int HitTestChip(int px, int py) {
    for (int i = 0; i < (int)g_chips.size(); ++i) {
        auto& c = g_chips[i];
        if (px >= c.x && px <= c.x + c.w && py >= c.y && py <= c.y + c.h)
            return i;
    }
    return -1;
}

static LabelChoice ShowLabelMenu(HWND hWnd, POINT screenPt, int chipIdx) {
    LabelChoice res = { (ChipLabel)-1, L"", false };

    HMENU hMenu = CreatePopupMenu();
    HMENU hIndSub = CreatePopupMenu();
    AppendMenuW(hIndSub, MF_STRING, 101, L"Anagram");
    AppendMenuW(hIndSub, MF_STRING, 102, L"Reversal");
    AppendMenuW(hIndSub, MF_STRING, 103, L"Hidden Word");
    AppendMenuW(hIndSub, MF_STRING, 104, L"Container");
    AppendMenuW(hIndSub, MF_STRING, 105, L"Homophone");
    AppendMenuW(hIndSub, MF_STRING, 106, L"Deletion");
    AppendMenuW(hIndSub, MF_STRING, 107, L"Acrostic");
    AppendMenuW(hIndSub, MF_STRING, 108, L"Charade");
    AppendMenuW(hIndSub, MF_STRING, 109, L"Other");

    bool isAlreadyInd = (chipIdx >= 0 && chipIdx < (int)g_chips.size() && g_chips[chipIdx].label == LABEL_INDICATOR);

    if (isAlreadyInd) {
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hIndSub, L"Change Indicator Type \u25B6");
        AppendMenuW(hMenu, MF_STRING, 110, L"Define Result Letters...");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    }

    AppendMenuW(hMenu, MF_STRING, 1, L"Definition (Blue)");
    AppendMenuW(hMenu, MF_STRING, 2, L"Fodder (Green)");
    AppendMenuW(hMenu, MF_STRING, 8, L"Other Fodder (Teal)");
    if (!isAlreadyInd) {
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hIndSub, L"Indicator (Orange) \u25B6");
    }
    AppendMenuW(hMenu, MF_STRING, 4, L"Substitution (Purple)...");
    AppendMenuW(hMenu, MF_STRING, 5, L"Connector / Link (Gray)");
    AppendMenuW(hMenu, MF_STRING, 6, L"Double Def 2 (Light Blue)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 7, L"Clear Tag");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTBUTTON, screenPt.x, screenPt.y, 0, hWnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd == 1) { res.label = LABEL_DEFINITION; }
    else if (cmd == 2) { res.label = LABEL_FODDER; }
    else if (cmd == 8) { res.label = LABEL_OTHER_FODDER; }
    else if (cmd >= 101 && cmd <= 109) {
        res.label = LABEL_INDICATOR;
        static const wchar_t* kIndTypes[] = {
            L"Anagram", L"Reversal", L"Hidden Word", L"Container",
            L"Homophone", L"Deletion", L"Acrostic", L"Charade", L"Other"
        };
        res.indType = kIndTypes[cmd - 101];
    }
    else if (cmd == 110) {
        res.label = LABEL_INDICATOR;
        if (chipIdx >= 0 && chipIdx < (int)g_chips.size()) res.indType = g_chips[chipIdx].indType;
        res.editNow = true;
    }
    else if (cmd == 3) {
        // Tagged as general Indicator; now prompt a second time for its type!
        HMENU hPrompt = CreatePopupMenu();
        AppendMenuW(hPrompt, MF_STRING, 101, L"Anagram");
        AppendMenuW(hPrompt, MF_STRING, 102, L"Reversal");
        AppendMenuW(hPrompt, MF_STRING, 103, L"Hidden Word");
        AppendMenuW(hPrompt, MF_STRING, 104, L"Container");
        AppendMenuW(hPrompt, MF_STRING, 105, L"Homophone");
        AppendMenuW(hPrompt, MF_STRING, 106, L"Deletion");
        AppendMenuW(hPrompt, MF_STRING, 107, L"Acrostic");
        AppendMenuW(hPrompt, MF_STRING, 108, L"Charade");
        AppendMenuW(hPrompt, MF_STRING, 109, L"Other");
        int subCmd = TrackPopupMenu(hPrompt, TPM_RETURNCMD | TPM_LEFTBUTTON, screenPt.x, screenPt.y, 0, hWnd, nullptr);
        DestroyMenu(hPrompt);
        res.label = LABEL_INDICATOR;
        if (subCmd >= 101 && subCmd <= 109) {
            static const wchar_t* kIndTypes[] = {
                L"Anagram", L"Reversal", L"Hidden Word", L"Container",
                L"Homophone", L"Deletion", L"Acrostic", L"Charade", L"Other"
            };
            res.indType = kIndTypes[subCmd - 101];
        } else {
            res.indType = L"Anagram";
        }
    }
    else if (cmd == 4) { res.label = LABEL_SUBSTITUTION; res.editNow = true; }
    else if (cmd == 5) { res.label = LABEL_CONNECTOR; }
    else if (cmd == 6) { res.label = LABEL_DOUBLEDEF2; }
    else if (cmd == 7) { res.label = LABEL_NONE; }

    return res;
}

// ---------------------------------------------------------------------------
// In-place Substitution Editor
// ---------------------------------------------------------------------------

static void StartInplaceEdit(int chipIdx) {
    if (chipIdx < 0 || chipIdx >= (int)g_chips.size() || !g_hChipWnd) return;
    g_editingChipIdx = chipIdx;
    auto& chip = g_chips[chipIdx];

    RECT r = { chip.x + 4, chip.y + chip.h - 26, chip.x + chip.w - 4, chip.y + chip.h - 4 };

    if (!g_hInplaceEdit) {
        g_hInplaceEdit = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_UPPERCASE,
            r.left, r.top, r.right - r.left, r.bottom - r.top,
            g_hChipWnd, (HMENU)IDC_SUB_INPLACE_EDIT, g_hInst, nullptr
        );
        SendMessageW(g_hInplaceEdit, WM_SETFONT, (WPARAM)g_hBadgeFont, FALSE);
    } else {
        MoveWindow(g_hInplaceEdit, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
        ShowWindow(g_hInplaceEdit, SW_SHOW);
    }

    std::wstring initVal;
    if (chip.label == LABEL_SUBSTITUTION) {
        initVal = chip.subText.empty() ? LookupSubstitution(chip.text) : chip.subText;
    } else {
        initVal = chip.subText;
    }
    SetWindowTextW(g_hInplaceEdit, initVal.c_str());
    SetFocus(g_hInplaceEdit);
    SendMessageW(g_hInplaceEdit, EM_SETSEL, 0, -1);
}

static void StopInplaceEdit(bool save) {
    if (!g_hInplaceEdit || g_editingChipIdx < 0 || g_editingChipIdx >= (int)g_chips.size()) return;
    if (save) {
        wchar_t buf[64];
        GetWindowTextW(g_hInplaceEdit, buf, 64);
        g_chips[g_editingChipIdx].subText = Trim(std::wstring(buf));
    }
    ShowWindow(g_hInplaceEdit, SW_HIDE);
    g_editingChipIdx = -1;
    InvalidateRect(g_hChipWnd, nullptr, TRUE);
}

// ---------------------------------------------------------------------------
// Custom Clue Canvas Procedure (CrypChipWnd) with Double Buffering
// ---------------------------------------------------------------------------

static void StopAnimation() {
    KillTimer(g_hChipWnd, IDT_ANIM);
    g_animStep = 0;
    g_visMode = false;
    if (g_hVisualize) SetWindowTextW(g_hVisualize, L"Visualize \u25B6");
    if (g_hChipWnd) InvalidateRect(g_hChipWnd, nullptr, TRUE);
    SetStatus(L"Ready. Click word cards to select role; drag to label a span.");
}

static LRESULT CALLBACK ChipWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT client;
        GetClientRect(hWnd, &client);

        // Double-buffering to ensure smooth, flicker-free rendering
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, client.right, client.bottom);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

        HBRUSH hBg = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(memDC, &client, hBg);
        DeleteObject(hBg);

        // Calculate breakdown for stages
        std::wstring defWords, fodderWords, indicatorWords, subSummary, allLetters;
        for (auto& c : g_chips) {
            std::wstring clean = StripPunct(c.text);
            if (c.label == LABEL_DEFINITION || c.label == LABEL_DOUBLEDEF2) {
                if (!defWords.empty()) defWords += L" ";
                defWords += c.text;
            } else if (c.label == LABEL_FODDER || c.label == LABEL_OTHER_FODDER) {
                std::wstring part = c.subText.empty() ? c.text : c.subText;
                if (!fodderWords.empty()) fodderWords += L" ";
                fodderWords += part;
                allLetters += StripPunct(part);
            } else if (c.label == LABEL_INDICATOR) {
                if (!indicatorWords.empty()) indicatorWords += L" ";
                indicatorWords += c.text;
            } else if (c.label == LABEL_SUBSTITUTION) {
                std::wstring rep = c.subText.empty() ? LookupSubstitution(c.text) : c.subText;
                if (rep.empty()) rep = clean;
                if (!subSummary.empty()) subSummary += L" + ";
                subSummary += c.text + L"\u2192[" + rep + L"]";
                allLetters += rep;
            }
        }
        std::wstring targetAnswer = ToUpper(Trim(GetWindowTextW_s(g_hAnswer)));

        // -------------------------------------------------------------
        // Render word cards (Big Prominent Design)
        // -------------------------------------------------------------
        for (int i = 0; i < (int)g_chips.size(); ++i) {
            auto& chip = g_chips[i];

            COLORREF bg = ChipBgColor(chip.label);
            COLORREF border = ChipBorderColor(chip.label, chip.selected);
            int borderThickness = chip.selected ? 3 : 2;

            if (g_visMode || g_animStep > 0) {
                if (chip.label == LABEL_DEFINITION) {
                    border = RGB(0, 110, 240);
                    borderThickness = 3;
                } else if (chip.label == LABEL_FODDER) {
                    border = RGB(20, 160, 60);
                    borderThickness = 3;
                } else if (chip.label == LABEL_OTHER_FODDER) {
                    border = RGB(20, 160, 140);
                    borderThickness = 3;
                } else if (chip.label == LABEL_INDICATOR) {
                    border = RGB(230, 120, 0);
                    borderThickness = 3;
                }
            }

            RECT r = { chip.x, chip.y, chip.x + chip.w, chip.y + chip.h };

            HBRUSH hCardBrush = CreateSolidBrush(bg);
            HPEN   hCardPen   = CreatePen(PS_SOLID, borderThickness, border);
            HPEN   oldP       = (HPEN)SelectObject(memDC, hCardPen);
            HBRUSH oldB       = (HBRUSH)SelectObject(memDC, hCardBrush);

            RoundRect(memDC, r.left, r.top, r.right, r.bottom, 12, 12);

            SelectObject(memDC, oldP);
            SelectObject(memDC, oldB);
            DeleteObject(hCardBrush);
            DeleteObject(hCardPen);

            // Draw large word text
            HFONT oldF = (HFONT)SelectObject(memDC, g_hChipFont);
            SetTextColor(memDC, RGB(25, 28, 35));

            std::wstring displayText = chip.text;
            if ((g_visMode || g_animStep >= 2) && chip.label == LABEL_SUBSTITUTION) {
                std::wstring rep = chip.subText.empty() ? LookupSubstitution(chip.text) : chip.subText;
                if (!rep.empty()) displayText = rep;
            }

            RECT textRect = { r.left, r.top + 8, r.right, r.top + 46 };
            DrawTextW(memDC, displayText.c_str(), -1, &textRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

            // Role badge pill underneath word
            if (chip.label != LABEL_NONE) {
                SelectObject(memDC, g_hBadgeFont);
                std::wstring bTag = LabelShortTag(chip.label);
                if (chip.label == LABEL_INDICATOR) {
                    bTag = chip.indType.empty() ? L"Anagram" : chip.indType;
                    if (!chip.subText.empty()) {
                        bTag += L" \u2192 " + chip.subText;
                    }
                } else if (chip.label == LABEL_SUBSTITUTION) {
                    std::wstring val = chip.subText.empty() ? LookupSubstitution(chip.text) : chip.subText;
                    if (!val.empty()) bTag += L": " + val;
                } else if (chip.label == LABEL_OTHER_FODDER) {
                    if (!chip.subText.empty()) bTag += L": " + chip.subText;
                }

                SIZE bsz;
                GetTextExtentPoint32W(memDC, bTag.c_str(), (int)bTag.size(), &bsz);
                int bw = bsz.cx + 14;
                int bh = 18;
                int bx = r.left + (chip.w - bw) / 2;
                int by = r.bottom - bh - 6;
                RECT badgeRect = { bx, by, bx + bw, by + bh };

                HBRUSH hPillBrush = CreateSolidBrush(RGB(255, 255, 255));
                HPEN   hPillPen   = CreatePen(PS_SOLID, 1, border);
                HPEN   oldPP      = (HPEN)SelectObject(memDC, hPillPen);
                HBRUSH oldPB      = (HBRUSH)SelectObject(memDC, hPillBrush);
                RoundRect(memDC, badgeRect.left, badgeRect.top, badgeRect.right, badgeRect.bottom, 6, 6);
                SelectObject(memDC, oldPP);
                SelectObject(memDC, oldPB);
                DeleteObject(hPillBrush);
                DeleteObject(hPillPen);

                SetTextColor(memDC, border);
                DrawTextW(memDC, bTag.c_str(), -1, &badgeRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            }

            SelectObject(memDC, oldF);
        }

        // -------------------------------------------------------------
        // Step-by-Step Collapse Visualizer: Direct Letter Transitions
        // -------------------------------------------------------------
        if (g_visMode || g_animStep > 0) {
            int maxChipBottom = 80;
            for (auto& c : g_chips) {
                if (c.y + c.h > maxChipBottom) maxChipBottom = c.y + c.h;
            }

            int stagesTop = maxChipBottom + 12;
            int stagesBottom = client.bottom - 8;
            int availH = stagesBottom - stagesTop;
            if (availH < 140) availH = 140;

            struct IndGroup {
                std::wstring words;
                std::wstring type;
                std::wstring manualResult;
            };
            std::vector<IndGroup> indStages;
            for (size_t i = 0; i < g_chips.size(); ++i) {
                if (g_chips[i].label == LABEL_INDICATOR) {
                    std::wstring t = g_chips[i].indType.empty() ? L"Anagram" : g_chips[i].indType;
                    if (!indStages.empty() && indStages.back().type == t) {
                        indStages.back().words += L" " + g_chips[i].text;
                        if (!g_chips[i].subText.empty()) indStages.back().manualResult = g_chips[i].subText;
                    } else {
                        indStages.push_back({ g_chips[i].text, t, g_chips[i].subText });
                    }
                }
            }

            struct VisualStage {
                std::wstring tag;
                std::wstring transition;
                std::wstring subNote;
                COLORREF bg;
                COLORREF border;
                COLORREF tagColor;
            };
            std::vector<VisualStage> stages;

            // 1. Initial State: Fodder & Substitutions
            std::wstring currentLetters = ToUpper(StripPunct(fodderWords));
            std::wstring s0Display;
            if (!currentLetters.empty()) {
                s0Display = currentLetters;
                if (!subSummary.empty()) s0Display += L"  +  " + subSummary;
            } else if (!subSummary.empty()) {
                s0Display = subSummary;
            } else if (!allLetters.empty()) {
                s0Display = ToUpper(allLetters);
                currentLetters = s0Display;
            } else {
                s0Display = L"(No wordplay components tagged)";
            }

            VisualStage st0;
            st0.tag = (fodderWords.empty() && !subSummary.empty()) ? L"SUBSTITUTIONS" : L"FODDER";
            st0.transition = s0Display;
            st0.bg = RGB(242, 247, 255);
            st0.border = RGB(70, 130, 240);
            st0.tagColor = RGB(25, 75, 200);
            stages.push_back(st0);

            // 2. Intermediate Stages: Each Indicator transforms the working letters
            if (indStages.empty()) {
                // No indicators tagged (direct assembly / charade)
                VisualStage stInd;
                stInd.tag = L"WORDPLAY ASSEMBLY";
                stInd.transition = currentLetters.empty() ? (targetAnswer.empty() ? L"..." : targetAnswer)
                                                         : (targetAnswer.empty() ? currentLetters : currentLetters + L" \u2192 " + targetAnswer);
                stInd.bg = RGB(255, 251, 235);
                stInd.border = RGB(245, 158, 11);
                stInd.tagColor = RGB(180, 83, 9);
                stages.push_back(stInd);
                currentLetters = targetAnswer.empty() ? currentLetters : targetAnswer;
            } else {
                for (size_t k = 0; k < indStages.size(); ++k) {
                    auto& ind = indStages[k];
                    VisualStage st;
                    st.tag = ToUpper(ind.type) + L" (\"" + ind.words + L"\")";
                    st.bg = RGB(255, 251, 235);
                    st.border = RGB(245, 158, 11);
                    st.tagColor = RGB(180, 83, 9);

                    // Check if manual result is defined
                    if (!ind.manualResult.empty()) {
                        std::wstring manualRes = ToUpper(StripPunct(ind.manualResult));
                        st.transition = currentLetters.empty() ? manualRes : (currentLetters + L" \u2192 " + manualRes);
                        currentLetters = manualRes;
                    } else if (ind.type == L"Deletion") {
                        if (!targetAnswer.empty()) {
                            std::unordered_map<wchar_t, int> targetCounts;
                            for (wchar_t c : targetAnswer) targetCounts[c]++;
                            std::wstring nextLetters;
                            for (wchar_t c : currentLetters) {
                                if (targetCounts[c] > 0) {
                                    nextLetters += c;
                                    targetCounts[c]--;
                                }
                            }
                            if (nextLetters.empty() || nextLetters == currentLetters) {
                                nextLetters = (currentLetters.size() > 1) ? currentLetters.substr(1) : currentLetters;
                            }
                            st.transition = currentLetters + L" \u2192 " + nextLetters;
                            currentLetters = nextLetters;
                        } else {
                            std::wstring nextLetters = (currentLetters.size() > 1) ? currentLetters.substr(1) : currentLetters;
                            st.transition = currentLetters + L" \u2192 " + nextLetters;
                            currentLetters = nextLetters;
                        }
                    } else if (ind.type == L"Anagram") {
                        if (!targetAnswer.empty()) {
                            std::unordered_map<wchar_t, int> curCounts;
                            for (wchar_t c : currentLetters) curCounts[c]++;
                            std::wstring nextLetters;
                            for (wchar_t c : targetAnswer) {
                                if (curCounts[c] > 0) {
                                    nextLetters += c;
                                    curCounts[c]--;
                                }
                            }
                            for (wchar_t c : currentLetters) {
                                if (curCounts[c] > 0) {
                                    nextLetters += c;
                                    curCounts[c]--;
                                }
                            }
                            st.transition = currentLetters + L" \u2192 " + nextLetters;
                            currentLetters = nextLetters;
                        } else {
                            std::wstring sorted = currentLetters;
                            std::sort(sorted.begin(), sorted.end());
                            st.transition = currentLetters + L" \u2192 " + sorted;
                            currentLetters = sorted;
                        }
                    } else if (ind.type == L"Reversal") {
                        std::wstring nextLetters = currentLetters;
                        std::reverse(nextLetters.begin(), nextLetters.end());
                        st.transition = currentLetters + L" \u2192 " + nextLetters;
                        currentLetters = nextLetters;
                    } else if (ind.type == L"Container") {
                        if (!targetAnswer.empty() && targetAnswer.find(currentLetters) != std::wstring::npos) {
                            size_t pos = targetAnswer.find(currentLetters);
                            std::wstring outL = targetAnswer.substr(0, pos);
                            std::wstring outR = targetAnswer.substr(pos + currentLetters.size());
                            st.transition = (outL.empty() ? L"" : outL) + L"[" + currentLetters + L"]" + outR + L" \u2192 " + targetAnswer;
                            currentLetters = targetAnswer;
                        } else {
                            st.transition = currentLetters + L" [inserted] \u2192 " + (targetAnswer.empty() ? currentLetters : targetAnswer);
                            currentLetters = targetAnswer.empty() ? currentLetters : targetAnswer;
                        }
                    } else if (ind.type == L"Hidden Word") {
                        st.transition = (fodderWords.empty() ? currentLetters : fodderWords) +
                                        L" \u2192 " + (targetAnswer.empty() ? currentLetters : targetAnswer);
                        currentLetters = targetAnswer.empty() ? currentLetters : targetAnswer;
                    } else if (ind.type == L"Acrostic") {
                        std::wstring nextLetters;
                        auto words = Split(fodderWords.empty() ? GetWindowTextW_s(g_hClueEdit) : fodderWords, L' ');
                        for (auto& w : words) {
                            std::wstring c = StripPunct(w);
                            if (!c.empty()) nextLetters += towupper(c[0]);
                        }
                        st.transition = currentLetters + L" \u2192 " + (nextLetters.empty() ? targetAnswer : nextLetters);
                        currentLetters = nextLetters.empty() ? targetAnswer : nextLetters;
                    } else if (ind.type == L"Homophone") {
                        st.transition = currentLetters + L" \u2192 " + (targetAnswer.empty() ? currentLetters : targetAnswer);
                        currentLetters = targetAnswer.empty() ? currentLetters : targetAnswer;
                    } else {
                        st.transition = currentLetters + (targetAnswer.empty() ? L"" : L" \u2192 " + targetAnswer);
                        currentLetters = targetAnswer.empty() ? currentLetters : targetAnswer;
                    }

                    stages.push_back(st);
                }
            }

            // 3. Final Stage: Solution Confirmed
            VisualStage stSol;
            stSol.tag = L"SOLUTION";
            stSol.transition = targetAnswer.empty() ? (currentLetters.empty() ? L"(ENTER TARGET ANSWER)" : currentLetters) : targetAnswer;
            if (!defWords.empty() || !targetAnswer.empty()) {
                stSol.subNote = L"\u2713 Definition: \"" + (defWords.empty() ? targetAnswer : defWords) + L"\" " + ComputeEnumeration(targetAnswer);
            }
            stSol.bg = RGB(236, 253, 245);
            stSol.border = RGB(16, 185, 129);
            stSol.tagColor = RGB(4, 120, 87);
            stages.push_back(stSol);

            int totalStages = (int)stages.size();
            int cardGap = (totalStages > 4) ? 4 : 6;
            int cardH = (availH - (totalStages - 1) * cardGap) / totalStages;
            if (cardH > 75) cardH = 75;
            if (cardH < 40) cardH = 40;

            int maxVisibleStep = g_animStep;

            for (int step = 1; step <= totalStages; ++step) {
                if (step > maxVisibleStep) break;
                auto& st = stages[step - 1];

                int cy = stagesTop + (step - 1) * (cardH + cardGap);
                RECT rCard = { 10, cy, client.right - 10, cy + cardH };

                HBRUSH hBr = CreateSolidBrush(st.bg);
                HPEN   hPen = CreatePen(PS_SOLID, 1, st.border);
                HPEN   oldP = (HPEN)SelectObject(memDC, hPen);
                HBRUSH oldB = (HBRUSH)SelectObject(memDC, hBr);
                RoundRect(memDC, rCard.left, rCard.top, rCard.right, rCard.bottom, 8, 8);
                SelectObject(memDC, oldP);
                SelectObject(memDC, oldB);
                DeleteObject(hBr);
                DeleteObject(hPen);

                // Top tag badge (No "STAGE N")
                HFONT oldF = (HFONT)SelectObject(memDC, g_hBoldFont);
                SetTextColor(memDC, st.tagColor);
                RECT rTag = { rCard.left + 14, rCard.top + 4, rCard.right - 14, rCard.top + 20 };
                DrawTextW(memDC, st.tag.c_str(), -1, &rTag, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

                // Prominent Transition Letters
                if (step == totalStages) {
                    SelectObject(memDC, g_hAnimLargeFont);
                    SetTextColor(memDC, RGB(16, 130, 60));
                } else {
                    SelectObject(memDC, g_hInputFont);
                    SetTextColor(memDC, RGB(25, 30, 45));
                }
                int transBottom = st.subNote.empty() ? (rCard.bottom - 4) : (rCard.bottom - 18);
                RECT rTrans = { rCard.left + 14, rCard.top + 18, rCard.right - 14, transBottom };
                DrawTextW(memDC, st.transition.c_str(), -1, &rTrans, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

                // Subnote for definition on solution card
                if (!st.subNote.empty()) {
                    SelectObject(memDC, g_hFont);
                    SetTextColor(memDC, RGB(80, 100, 80));
                    RECT rSub = { rCard.left + 14, rCard.bottom - 18, rCard.right - 14, rCard.bottom - 2 };
                    DrawTextW(memDC, st.subNote.c_str(), -1, &rSub, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                }

                SelectObject(memDC, oldF);
            }
        }

        BitBlt(hdc, 0, 0, client.right, client.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (g_visMode) return 0;
        StopInplaceEdit(true);

        int px = LOWORD(lParam), py = HIWORD(lParam);
        int idx = HitTestChip(px, py);
        g_dragStart = idx;
        g_dragEnd   = idx;
        g_dragging  = false;

        for (auto& c : g_chips) c.selected = false;
        if (idx >= 0) {
            g_chips[idx].selected = true;
            SetCapture(hWnd);
        }
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!(wParam & MK_LBUTTON) || g_dragStart < 0) return 0;
        int px = LOWORD(lParam), py = HIWORD(lParam);
        int idx = HitTestChip(px, py);
        if (idx < 0) return 0;

        if (idx != g_dragEnd) {
            g_dragging = true;
            g_dragEnd = idx;
            int lo = std::min(g_dragStart, g_dragEnd);
            int hi = std::max(g_dragStart, g_dragEnd);
            for (int i = 0; i < (int)g_chips.size(); ++i)
                g_chips[i].selected = (i >= lo && i <= hi);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK: {
        if (g_visMode) return 0;
        int px = LOWORD(lParam), py = HIWORD(lParam);
        int idx = HitTestChip(px, py);
        if (idx >= 0 && (g_chips[idx].label == LABEL_INDICATOR || g_chips[idx].label == LABEL_SUBSTITUTION || g_chips[idx].label == LABEL_OTHER_FODDER)) {
            StartInplaceEdit(idx);
            return 0;
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        ReleaseCapture();
        if (g_dragStart < 0) return 0;
        int px = LOWORD(lParam), py = HIWORD(lParam);

        POINT pt = { px, py };
        ClientToScreen(hWnd, &pt);

        int lo = (g_dragging && g_dragStart != g_dragEnd) ? std::min(g_dragStart, g_dragEnd) : g_dragStart;
        int hi = (g_dragging && g_dragStart != g_dragEnd) ? std::max(g_dragStart, g_dragEnd) : g_dragStart;

        LabelChoice choice = ShowLabelMenu(hWnd, pt, lo);
        if ((int)choice.label >= 0) {
            for (int i = lo; i <= hi; ++i) {
                if (i >= 0 && i < (int)g_chips.size()) {
                    g_chips[i].label = choice.label;
                    if (choice.label == LABEL_INDICATOR) {
                        g_chips[i].indType = choice.indType;
                    } else if (choice.label == LABEL_SUBSTITUTION && g_chips[i].subText.empty()) {
                        g_chips[i].subText = LookupSubstitution(g_chips[i].text);
                    } else if (choice.label == LABEL_NONE) {
                        g_chips[i].subText.clear();
                        g_chips[i].indType.clear();
                    }
                }
            }
            if (choice.editNow && lo >= 0 && lo < (int)g_chips.size()) {
                StartInplaceEdit(lo);
            } else if (choice.label == LABEL_SUBSTITUTION && lo == hi && lo >= 0 && lo < (int)g_chips.size()) {
                StartInplaceEdit(lo);
            }
        }

        for (auto& c : g_chips) c.selected = false;
        g_dragStart = g_dragEnd = -1;
        g_dragging = false;

        InvalidateRect(hWnd, nullptr, TRUE);
        PostMessageW(GetParent(hWnd), WM_APP_CHIP_CLICK, 0, 0);
        return 0;
    }

    case WM_RBUTTONUP: {
        if (g_visMode) return 0;
        int px = LOWORD(lParam), py = HIWORD(lParam);
        int idx = HitTestChip(px, py);
        if (idx >= 0) {
            POINT pt = { px, py };
            ClientToScreen(hWnd, &pt);
            LabelChoice choice = ShowLabelMenu(hWnd, pt, idx);
            if ((int)choice.label >= 0) {
                g_chips[idx].label = choice.label;
                if (choice.label == LABEL_INDICATOR) {
                    g_chips[idx].indType = choice.indType;
                } else if (choice.label == LABEL_SUBSTITUTION && g_chips[idx].subText.empty()) {
                    g_chips[idx].subText = LookupSubstitution(g_chips[idx].text);
                } else if (choice.label == LABEL_NONE) {
                    g_chips[idx].subText.clear();
                    g_chips[idx].indType.clear();
                }
                if (choice.editNow) {
                    StartInplaceEdit(idx);
                } else if (choice.label == LABEL_SUBSTITUTION && g_chips[idx].subText.empty()) {
                    StartInplaceEdit(idx);
                }
                InvalidateRect(hWnd, nullptr, TRUE);
            }
        }
        return 0;
    }

    case WM_TIMER: {
        if (wParam == IDT_ANIM) {
            int indCount = 0;
            std::wstring lastType;
            for (auto& c : g_chips) {
                if (c.label == LABEL_INDICATOR) {
                    std::wstring t = c.indType.empty() ? L"Anagram" : c.indType;
                    if (lastType != t) {
                        indCount++;
                        lastType = t;
                    }
                }
            }
            int totalStages = (indCount == 0) ? 3 : (indCount + 2);

            if (g_animStep < totalStages) {
                g_animStep++;
                InvalidateRect(hWnd, nullptr, TRUE);
            } else {
                KillTimer(hWnd, IDT_ANIM);
                g_animStep = totalStages;
                g_visMode = true;
                if (g_hVisualize) SetWindowTextW(g_hVisualize, L"Edit Clue \u270E");
                wchar_t sbuf[128];
                swprintf_s(sbuf, L"Visualization complete: All %d collapse stages displayed on screen. Click 'Edit Clue' to return.", totalStages);
                SetStatus(sbuf);
            }
        }
        return 0;
    }

    case WM_KILLFOCUS: {
        StopInplaceEdit(true);
        return 0;
    }

    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            StopInplaceEdit(false);
            if (g_visMode) StopAnimation();
            return 0;
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Main UI Operations & Database Export
// ---------------------------------------------------------------------------

static void SetStatus(const wchar_t* text) {
    if (g_hStatusBar) SetWindowTextW(g_hStatusBar, text);
}

static void UpdateCountLabel() {
    if (!g_hAnswer || !g_hCountLbl) return;
    std::wstring ans = GetWindowTextW_s(g_hAnswer);
    int cnt = 0;
    for (wchar_t c : ans) if (iswalpha(c)) ++cnt;
    wchar_t buf[32];
    swprintf_s(buf, L"(%d)", cnt);
    SetWindowTextW(g_hCountLbl, buf);
}

static void SaveCurrentClue() {
    std::wstring answer = ToUpper(Trim(GetWindowTextW_s(g_hAnswer)));
    std::wstring clue   = Trim(GetWindowTextW_s(g_hClueEdit));

    if (answer.empty() || clue.empty()) {
        MessageBoxW(g_hWnd, L"Please enter both a Clue and an Answer before saving.", L"CrypTools", MB_OK | MB_ICONWARNING);
        return;
    }

    ClueEntry ce;
    ce.answer = answer;
    ce.clue   = clue;
    ce.enumeration = ComputeEnumeration(answer);
    ce.labels = SerializeLabels();
    ce.notes  = L"";

    // Update if already exists with same clue text, else append
    bool updated = false;
    for (auto& c : g_clues) {
        if (c.clue == ce.clue) {
            c = ce;
            updated = true;
            break;
        }
    }
    if (!updated) g_clues.push_back(ce);

    SaveClues();
    RefreshDatabaseList();
    SetStatus(L"Clue exported to clues.dat database successfully.");
}

// ---------------------------------------------------------------------------
// Indicator Bank UI Handlers (Prominent Filters Inspired by CrossTools)
// ---------------------------------------------------------------------------

static void RefreshIndicatorList() {
    if (!g_hIndFilter || !g_hIndList) return;
    int sel = (int)SendMessageW(g_hIndFilter, CB_GETCURSEL, 0, 0);
    static const wchar_t* kCats[] = {
        L"", L"anagram", L"reversal", L"hidden", L"container",
        L"homophone", L"deletion", L"acrostic"
    };
    std::wstring filterCat = (sel > 0 && sel < 8) ? kCats[sel] : L"";
    std::wstring searchTxt = ToLower(Trim(g_hIndSearch ? GetWindowTextW_s(g_hIndSearch) : L""));

    SendMessageW(g_hIndList, LB_RESETCONTENT, 0, 0);
    int shown = 0;

    for (int i = 0; i < (int)g_indicators.size(); ++i) {
        auto& ind = g_indicators[i];
        if (!filterCat.empty() && ind.category != filterCat) continue;
        if (!searchTxt.empty() && ToLower(ind.word).find(searchTxt) == std::wstring::npos) continue;

        std::wstring display = ind.word + L" [" + ind.category + L"]";
        if (!ind.example.empty()) display += L" \u2605";
        int lbIdx = (int)SendMessageW(g_hIndList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
        if (lbIdx != LB_ERR) {
            SendMessageW(g_hIndList, LB_SETITEMDATA, lbIdx, (LPARAM)i);
        }
        ++shown;
    }

    if (g_hIndCountLbl) {
        wchar_t buf[128];
        swprintf_s(buf, L"Showing %d indicator(s)%s", shown, searchTxt.empty() ? L"" : L" (filtered)");
        SetWindowTextW(g_hIndCountLbl, buf);
    }

    if (SendMessageW(g_hIndList, LB_GETCOUNT, 0, 0) > 0) {
        SendMessageW(g_hIndList, LB_SETCURSEL, 0, 0);
        ShowIndicatorDetail();
    } else {
        SetWindowTextW(g_hIndExTitle, L"No indicators match criteria");
        SetWindowTextW(g_hIndExClue, L"");
        SetWindowTextW(g_hIndExExplain, L"");
        EnableWindow(g_hIndExTry, FALSE);
    }
}

static void ShowIndicatorDetail() {
    int sel = (int)SendMessageW(g_hIndList, LB_GETCURSEL, 0, 0);
    if (sel < 0) return;

    int origIdx = (int)SendMessageW(g_hIndList, LB_GETITEMDATA, sel, 0);
    if (origIdx >= 0 && origIdx < (int)g_indicators.size()) {
        auto& ind = g_indicators[origIdx];
        std::wstring title = L"Indicator: \"" + ind.word + L"\" (" + ind.category + L")";
        SetWindowTextW(g_hIndExTitle, title.c_str());

        if (!ind.example.empty()) {
            SetWindowTextW(g_hIndExClue, ind.example.c_str());
            SetWindowTextW(g_hIndExExplain, ind.explanation.c_str());
            EnableWindow(g_hIndExTry, TRUE);
        } else {
            SetWindowTextW(g_hIndExClue, L"(No example clue stored yet for this indicator)");
            SetWindowTextW(g_hIndExExplain, L"Type an example clue in Clue Builder and save it.");
            EnableWindow(g_hIndExTry, FALSE);
        }
    }
}

// ---------------------------------------------------------------------------
// Substitutions UI Handlers (Phrase Input Scanner)
// ---------------------------------------------------------------------------

static void ScanSubstitutionsInPhrase() {
    if (!g_hSubMatchesList) return;
    SendMessageW(g_hSubMatchesList, LB_RESETCONTENT, 0, 0);

    std::wstring phrase = Trim(g_hSubInput ? GetWindowTextW_s(g_hSubInput) : L"");
    std::wstring phraseLower = ToLower(phrase);
    bool reverse = g_subDirectionReverse;
    int matchCount = 0;

    for (int k = 0; k < (int)g_substitutions.size(); ++k) {
        auto& s = g_substitutions[k];
        // Standard (Word -> Abbr): src = s.from, dst = s.to
        // Reversed (Abbr -> Word): src = s.to,   dst = s.from
        std::wstring src = reverse ? ToLower(s.to) : ToLower(s.from);
        std::wstring dst = reverse ? ToLower(s.from) : ToLower(s.to);
        std::wstring srcDisp = reverse ? s.to : s.from;
        std::wstring dstDisp = reverse ? s.from : s.to;

        bool match = false;
        if (phraseLower.empty()) {
            match = true; // Blank input shows all substitutions in bank
        } else {
            // Show all substitutions that could be in target phrase, not exact string matching
            if (phraseLower.find(src) != std::wstring::npos ||
                src.find(phraseLower) != std::wstring::npos ||
                dst.find(phraseLower) != std::wstring::npos) {
                match = true;
            }
        }

        if (match) {
            wchar_t buf[256];
            swprintf_s(buf, L"\"%s\"  \u2192  %s", srcDisp.c_str(), dstDisp.c_str());
            int lbIdx = (int)SendMessageW(g_hSubMatchesList, LB_ADDSTRING, 0, (LPARAM)buf);
            if (lbIdx != LB_ERR) {
                SendMessageW(g_hSubMatchesList, LB_SETITEMDATA, lbIdx, (LPARAM)k);
            }
            ++matchCount;
        }
    }

    if (g_hSubMatchesLbl) {
        wchar_t buf[128];
        if (phraseLower.empty()) {
            swprintf_s(buf, L"Substitution Bank: %d total pair(s) (all shown):", matchCount);
        } else {
            swprintf_s(buf, L"Found %d recognized substitution(s) for input:", matchCount);
        }
        SetWindowTextW(g_hSubMatchesLbl, buf);
    }
}

static void RefreshSubstitutionList() {
    ScanSubstitutionsInPhrase();
}

// ---------------------------------------------------------------------------
// Database Tab UI Handlers
// ---------------------------------------------------------------------------

static void RefreshDatabaseList() {
    if (!g_hDbList) return;
    std::wstring search = ToLower(Trim(g_hDbSearch ? GetWindowTextW_s(g_hDbSearch) : L""));

    SendMessageW(g_hDbList, LB_RESETCONTENT, 0, 0);
    int shown = 0;
    for (int i = 0; i < (int)g_clues.size(); ++i) {
        auto& c = g_clues[i];
        if (!search.empty() && ToLower(c.answer).find(search) == std::wstring::npos &&
            ToLower(c.clue).find(search) == std::wstring::npos) continue;

        std::wstring display = c.answer + L": " + c.clue;
        int lbIdx = (int)SendMessageW(g_hDbList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
        if (lbIdx != LB_ERR) {
            SendMessageW(g_hDbList, LB_SETITEMDATA, lbIdx, (LPARAM)i);
        }
        ++shown;
    }

    if (g_hDbCount) {
        wchar_t buf[128];
        swprintf_s(buf, L"%d clue(s) in database%s", shown, search.empty() ? L"" : L" (filtered)");
        SetWindowTextW(g_hDbCount, buf);
    }

    if (!g_clues.empty() && SendMessageW(g_hDbList, LB_GETCOUNT, 0, 0) > 0) {
        SendMessageW(g_hDbList, LB_SETCURSEL, 0, 0);
        int origIdx = (int)SendMessageW(g_hDbList, LB_GETITEMDATA, 0, 0);
        if (origIdx >= 0 && origIdx < (int)g_clues.size()) {
            auto& c = g_clues[origIdx];
            wchar_t buf[2048];
            swprintf_s(buf,
                L"ANSWER:      %s\r\n"
                L"CLUE:        %s\r\n"
                L"ENUMERATION: %s\r\n"
                L"LABELS:      %s\r\n"
                L"NOTES:       %s",
                c.answer.c_str(), c.clue.c_str(),
                c.enumeration.c_str(), c.labels.c_str(), c.notes.c_str());
            if (g_hDbDetail) SetWindowTextW(g_hDbDetail, buf);
        }
    }
}

// ---------------------------------------------------------------------------
// Tools Tab Algorithms: Anagrams, Inner Substrings, Addition & Removal
// ---------------------------------------------------------------------------

struct AnagramItem {
    std::wstring text;
    int colorState; // 1 = Green (Valid Anagram), 2 = Yellow (Contains valid substring), 0 = Plain
    int subWordLen;
};

static void ComputeAnagrams() {
    std::wstring input = ToUpper(Trim(GetWindowTextW_s(g_hToolAnaInput)));
    SendMessageW(g_hToolAnaList, LB_RESETCONTENT, 0, 0);
    if (input.empty()) {
        SetWindowTextW(g_hToolAnaCount, L"Enter a word above to generate anagrams and candidate permutations.");
        return;
    }

    std::wstring lettersOnly;
    for (wchar_t c : input) if (iswalpha(c)) lettersOnly += c;
    if (lettersOnly.empty()) {
        SetWindowTextW(g_hToolAnaCount, L"Please enter letters to anagram.");
        return;
    }

    std::wstring sorted = lettersOnly;
    std::sort(sorted.begin(), sorted.end());

    // 1. Collect valid exact anagrams from dictionary (Green)
    std::unordered_set<std::wstring> seen;
    std::vector<AnagramItem> validAnagrams;
    auto it = g_anagramLookup.find(sorted);
    if (it != g_anagramLookup.end()) {
        for (const auto& w : it->second) {
            if (seen.insert(w).second) {
                validAnagrams.push_back({ w, 1, (int)w.size() });
            }
        }
    }
    std::sort(validAnagrams.begin(), validAnagrams.end(), [](const AnagramItem& a, const AnagramItem& b) {
        return a.text < b.text;
    });

    // 2. Generate permutations / candidates
    // Non-valid anagrams are fine to display, but highlight valid as green, and anagrams containing valid substrings as yellow.
    std::vector<AnagramItem> subwordAnagrams; // Yellow (colorState = 2)
    std::vector<AnagramItem> otherAnagrams;   // Plain (colorState = 0)

    int n = (int)lettersOnly.size();
    if (n >= 2) {
        std::wstring perm = sorted;
        int maxPerms = (n <= 7) ? 5040 : 250;
        int permCount = 0;

        do {
            if (seen.find(perm) == seen.end()) {
                seen.insert(perm);
                // Check if the entire permutation is in dictionary
                if (g_dictSet.find(perm) != g_dictSet.end()) {
                    validAnagrams.push_back({ perm, 1, n });
                } else {
                    // Check if contains a valid substring of length >= 3
                    std::wstring bestSub;
                    for (int subLen = std::min(n - 1, 8); subLen >= 3; --subLen) {
                        for (int pos = 0; pos <= n - subLen; ++pos) {
                            std::wstring sub = perm.substr(pos, subLen);
                            if (g_dictSet.find(sub) != g_dictSet.end()) {
                                bestSub = sub;
                                break;
                            }
                        }
                        if (!bestSub.empty()) break;
                    }

                    if (!bestSub.empty()) {
                        subwordAnagrams.push_back({ perm + L"  [contains: " + bestSub + L"]", 2, (int)bestSub.size() });
                    } else {
                        if (otherAnagrams.size() < 100) {
                            otherAnagrams.push_back({ perm, 0, 0 });
                        }
                    }
                }
            }
            ++permCount;
            if (permCount >= maxPerms) break;
        } while (std::next_permutation(perm.begin(), perm.end()));
    }

    // Sort subword anagrams by best subword length descending, then alphabetical
    std::sort(subwordAnagrams.begin(), subwordAnagrams.end(), [](const AnagramItem& a, const AnagramItem& b) {
        if (a.subWordLen != b.subWordLen) return a.subWordLen > b.subWordLen;
        return a.text < b.text;
    });

    if (subwordAnagrams.size() > 300) subwordAnagrams.resize(300);

    // Combine: Green first, Yellow second, Plain last
    std::vector<AnagramItem> allItems;
    allItems.reserve(validAnagrams.size() + subwordAnagrams.size() + otherAnagrams.size());
    for (auto& a : validAnagrams)   allItems.push_back(a);
    for (auto& a : subwordAnagrams) allItems.push_back(a);
    for (auto& a : otherAnagrams)   allItems.push_back(a);

    wchar_t cbuf[160];
    swprintf_s(cbuf, L"Results for \"%s\": %d valid anagram(s) (green), %d with valid substrings (yellow), %d total shown:",
        lettersOnly.c_str(), (int)validAnagrams.size(), (int)subwordAnagrams.size(), (int)allItems.size());
    SetWindowTextW(g_hToolAnaCount, cbuf);

    for (const auto& item : allItems) {
        std::wstring line = item.text;
        if (item.colorState == 1 && line == lettersOnly) {
            line += L"  (exact input)";
        }
        int lbIdx = (int)SendMessageW(g_hToolAnaList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
        if (lbIdx != LB_ERR) {
            SendMessageW(g_hToolAnaList, LB_SETITEMDATA, lbIdx, (LPARAM)item.colorState);
        }
    }
}

static void ComputeHiddenWords() {
    std::wstring phrase = GetWindowTextW_s(g_hToolHidPhrase);
    SendMessageW(g_hToolHidList, LB_RESETCONTENT, 0, 0);
    if (phrase.empty()) {
        SetWindowTextW(g_hToolHidCount, L"Enter a phrase above to scan for hidden inner words.");
        return;
    }

    int minLen = 3;
    if (g_hToolHidMinLen) {
        int sel = (int)SendMessageW(g_hToolHidMinLen, CB_GETCURSEL, 0, 0);
        if (sel == 1) minLen = 4;
        else if (sel == 2) minLen = 5;
        else if (sel == 3) minLen = 6;
    }

    int scopeSel = g_hToolHidScope ? (int)SendMessageW(g_hToolHidScope, CB_GETCURSEL, 0, 0) : 0;
    // 0 = Boundary Spanning Only (Default, Green)
    // 1 = All Inner Substrings (Green boundary + Yellow inner)
    // 2 = Single Word Interiors Only (Yellow)

    bool includeRev = true;
    if (g_hToolHidRev) {
        includeRev = (SendMessageW(g_hToolHidRev, CB_GETCURSEL, 0, 0) == 0);
    }

    std::wstring pUp = ToUpper(phrase);
    std::wstring noSpace;
    std::vector<int> origIdx;
    std::vector<bool> isSpaceBefore;

    bool prevWasSpace = false;
    for (int i = 0; i < (int)pUp.size(); ++i) {
        if (iswalpha(pUp[i])) {
            noSpace += pUp[i];
            origIdx.push_back(i);
            isSpaceBefore.push_back(prevWasSpace);
            prevWasSpace = false;
        } else if (iswspace(pUp[i])) {
            prevWasSpace = true;
        }
    }

    struct HiddenResult {
        std::wstring word;
        int len;
        std::wstring snippet;
        bool crosses;
        bool reversed;
    };
    std::vector<HiddenResult> results;
    int n = (int)noSpace.size();

    // 1. Scan forward substrings
    for (int len = minLen; len <= std::min(14, n); ++len) {
        for (int i = 0; i <= n - len; ++i) {
            bool crosses = false;
            for (int k = i + 1; k < i + len; ++k) {
                if (isSpaceBefore[k]) { crosses = true; break; }
            }

            if (scopeSel == 0 && !crosses) continue; // Boundary spanning only
            if (scopeSel == 2 && crosses) continue;  // Single word interiors only

            std::wstring sub = noSpace.substr(i, len);
            if (g_dictSet.find(sub) != g_dictSet.end()) {
                int sPos = origIdx[i];
                int ePos = origIdx[i + len - 1];
                int snipStart = std::max(0, sPos - 3);
                int snipEnd = std::min((int)pUp.size(), ePos + 4);
                std::wstring snip = pUp.substr(snipStart, snipEnd - snipStart);
                results.push_back({ sub, len, snip, crosses, false });
            }
        }
    }

    // 2. Scan reversed substrings (cryptic reversals)
    if (includeRev) {
        for (int len = minLen; len <= std::min(14, n); ++len) {
            for (int i = 0; i <= n - len; ++i) {
                bool crosses = false;
                for (int k = i + 1; k < i + len; ++k) {
                    if (isSpaceBefore[k]) { crosses = true; break; }
                }

                if (scopeSel == 0 && !crosses) continue;
                if (scopeSel == 2 && crosses) continue;

                std::wstring sub = noSpace.substr(i, len);
                std::wstring subRev = sub;
                std::reverse(subRev.begin(), subRev.end());
                if (g_dictSet.find(subRev) != g_dictSet.end()) {
                    int sPos = origIdx[i];
                    int ePos = origIdx[i + len - 1];
                    int snipStart = std::max(0, sPos - 3);
                    int snipEnd = std::min((int)pUp.size(), ePos + 4);
                    std::wstring snip = pUp.substr(snipStart, snipEnd - snipStart);
                    results.push_back({ subRev, len, snip, crosses, true });
                }
            }
        }
    }

    // Sort: Boundary-spanning (crosses == true) first, then by length descending, then alphabetical
    std::sort(results.begin(), results.end(), [](const HiddenResult& a, const HiddenResult& b) {
        if (a.crosses != b.crosses) return a.crosses > b.crosses;
        if (a.len != b.len) return a.len > b.len;
        return a.word < b.word;
    });

    int greenCount = 0, yellowCount = 0;
    for (auto& r : results) {
        if (r.crosses) ++greenCount;
        else ++yellowCount;
    }

    wchar_t cbuf[160];
    swprintf_s(cbuf, L"Discovered %d hidden word(s) [%d boundary-spanning (green)%s]:",
        (int)results.size(), greenCount,
        (yellowCount > 0) ? (std::wstring(L", ") + std::to_wstring(yellowCount) + L" inner (yellow)").c_str() : L"");
    SetWindowTextW(g_hToolHidCount, cbuf);

    for (auto& r : results) {
        wchar_t buf[512];
        swprintf_s(buf, L"%-12s (%d)  [%s%s] in: \"...%s...\"",
            r.word.c_str(), r.len,
            r.crosses ? L"BOUNDARY SPANNING" : L"INNER SUBSTRING",
            r.reversed ? L", REVERSED" : L"",
            r.snippet.c_str());
        int lbIdx = (int)SendMessageW(g_hToolHidList, LB_ADDSTRING, 0, (LPARAM)buf);
        if (lbIdx != LB_ERR) {
            int itemData = r.crosses ? 1 : 2; // 1 = Green, 2 = Yellow
            SendMessageW(g_hToolHidList, LB_SETITEMDATA, lbIdx, (LPARAM)itemData);
        }
    }
}

static void ComputeLetterDiff() {
    std::wstring mainW = ToUpper(Trim(GetWindowTextW_s(g_hToolDiffMain)));
    std::wstring goalW = ToUpper(Trim(GetWindowTextW_s(g_hToolDiffGoal)));

    SendMessageW(g_hToolDiffDelList, LB_RESETCONTENT, 0, 0);
    SendMessageW(g_hToolDiffAddList, LB_RESETCONTENT, 0, 0);

    if (mainW.empty() && goalW.empty()) {
        SetWindowTextW(g_hToolDiffResult, L"Enter a Main Word above to explore letter additions and deletions towards a goal word.");
        return;
    }

    // 1. Interactive Transformation Analysis (Main -> Goal)
    std::wstring res;
    if (!mainW.empty() && !goalW.empty()) {
        int freq1[26] = {0}, freq2[26] = {0};
        for (wchar_t c : mainW) if (c >= L'A' && c <= L'Z') freq1[c - L'A']++;
        for (wchar_t c : goalW) if (c >= L'A' && c <= L'Z') freq2[c - L'A']++;

        std::wstring added, removed;
        for (int i = 0; i < 26; ++i) {
            int d = freq2[i] - freq1[i];
            if (d > 0) for (int j = 0; j < d; ++j) added += (wchar_t)(L'A' + i);
            if (d < 0) for (int j = 0; j < -d; ++j) removed += (wchar_t)(L'A' + i);
        }

        res += L"TRANSFORMATION: " + mainW + L" (" + std::to_wstring(mainW.size()) + L")  \u2192  " + goalW + L" (" + std::to_wstring(goalW.size()) + L")\r\n";
        res += L"Letter Changes: ";
        if (!added.empty())   res += L"Add [+" + added + L"]  ";
        if (!removed.empty()) res += L"Remove [-" + removed + L"]  ";
        if (added.empty() && removed.empty()) res += L"Exact Anagram (rearrange letters only!)";
        res += L"\r\n\r\nCryptic Mechanics & Indicator Ideas:\r\n";

        if (mainW.size() > goalW.size()) {
            if (mainW.find(goalW) == 0) {
                std::wstring tail = mainW.substr(goalW.size());
                res += L"\u2022 Curtailment (tail removed): Drop final letter(s) \"" + tail + L"\"\r\n"
                       L"  Indicators: tailless, shortened, endless, curtailed, unfinished, missing end, docked.\r\n";
            } else if (mainW.rfind(goalW) == mainW.size() - goalW.size()) {
                std::wstring head = mainW.substr(0, mainW.size() - goalW.size());
                res += L"\u2022 Beheading (head decapitated): Drop first letter(s) \"" + head + L"\"\r\n"
                       L"  Indicators: headless, decapitated, missing start, leaderless, without beginning, unstarted.\r\n";
            } else if (removed.size() == 1) {
                res += L"\u2022 Internal Deletion: Remove '" + removed + L"' from inside word\r\n"
                       L"  Indicators: gutted, hollow, empty, heartless, stripped, without core.\r\n";
            }
        } else if (goalW.size() > mainW.size()) {
            if (goalW.find(mainW) == 0) {
                std::wstring tail = goalW.substr(mainW.size());
                res += L"\u2022 Suffix addition: Append \"" + tail + L"\" to end\r\n"
                       L"  Indicators: followed by, attached to, after, ending with.\r\n";
            } else if (goalW.rfind(mainW) == goalW.size() - mainW.size()) {
                std::wstring head = goalW.substr(0, goalW.size() - mainW.size());
                res += L"\u2022 Prefix addition: Prepend \"" + head + L"\" to start\r\n"
                       L"  Indicators: preceded by, leading with, headed by, before.\r\n";
            } else if (added.size() == 1) {
                res += L"\u2022 Infix insertion (Container): Enclose '" + added + L"' inside word\r\n"
                       L"  Indicators: holding, containing, around, embracing, swallowing, inside, enters.\r\n";
            }
        } else if (mainW.size() == goalW.size() && !added.empty()) {
            res += L"\u2022 Mutation / Substitution: Replace [-" + removed + L"] with [+" + added + L"]\r\n";
        }
    } else {
        res += L"Main Word: " + mainW + L" (" + std::to_wstring(mainW.size()) + L" letters)\r\n"
               L"Explore valid single-letter additions and deletions below (click any to set as Goal Word):";
    }
    SetWindowTextW(g_hToolDiffResult, res.c_str());

    // 2. Compute valid 1-letter deletions from Main Word
    if (mainW.size() >= 3) {
        std::vector<std::pair<std::wstring, std::wstring>> delWords;
        for (size_t i = 0; i < mainW.size(); ++i) {
            std::wstring cand = mainW.substr(0, i) + mainW.substr(i + 1);
            if (g_dictSet.find(cand) != g_dictSet.end()) {
                std::wstring note;
                if (i == 0) note = L"Headless (-" + std::wstring(1, mainW[i]) + L")";
                else if (i == mainW.size() - 1) note = L"Tailless (-" + std::wstring(1, mainW[i]) + L")";
                else note = L"Internal deletion (-" + std::wstring(1, mainW[i]) + L")";
                delWords.push_back({ cand, note });
            }
        }
        std::sort(delWords.begin(), delWords.end());
        delWords.erase(std::unique(delWords.begin(), delWords.end()), delWords.end());

        for (auto& p : delWords) {
            std::wstring line = p.first + L"  [" + p.second + L"]";
            SendMessageW(g_hToolDiffDelList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
        }
    }

    // 3. Compute valid 1-letter additions to Main Word
    if (!mainW.empty() && mainW.size() <= 20) {
        std::vector<std::pair<std::wstring, std::wstring>> addWords;
        for (wchar_t ch = L'A'; ch <= L'Z'; ++ch) {
            // Prefix
            std::wstring pfx = ch + mainW;
            if (g_dictSet.find(pfx) != g_dictSet.end()) {
                addWords.push_back({ pfx, L"Prefix (+" + std::wstring(1, ch) + L")" });
            }
            // Suffix
            std::wstring sfx = mainW + ch;
            if (g_dictSet.find(sfx) != g_dictSet.end()) {
                addWords.push_back({ sfx, L"Suffix (+" + std::wstring(1, ch) + L")" });
            }
            // Infix insertions
            for (size_t i = 1; i < mainW.size(); ++i) {
                std::wstring ifx = mainW.substr(0, i) + ch + mainW.substr(i);
                if (g_dictSet.find(ifx) != g_dictSet.end()) {
                    addWords.push_back({ ifx, L"Container (+" + std::wstring(1, ch) + L")" });
                }
            }
        }
        std::sort(addWords.begin(), addWords.end());
        addWords.erase(std::unique(addWords.begin(), addWords.end()), addWords.end());

        for (auto& p : addWords) {
            std::wstring line = p.first + L"  [" + p.second + L"]";
            SendMessageW(g_hToolDiffAddList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
        }
    }
}

// ---------------------------------------------------------------------------
// Window Layout & Resizing
// ---------------------------------------------------------------------------

static void ShowControls(bool show, std::initializer_list<HWND> hwnds) {
    for (HWND h : hwnds) {
        if (h) {
            ShowWindow(h, show ? SW_SHOW : SW_HIDE);
            if (show) BringWindowToTop(h);
        }
    }
}

static void ShowToolView(int toolIndex) {
    g_activeTool = toolIndex;

    // Highlight active tool button font
    SendMessageW(g_hToolTabAna,  WM_SETFONT, (WPARAM)(toolIndex == TOOL_ANAGRAM ? g_hBoldFont : g_hFont), TRUE);
    SendMessageW(g_hToolTabHid,  WM_SETFONT, (WPARAM)(toolIndex == TOOL_HIDDEN ? g_hBoldFont : g_hFont), TRUE);
    SendMessageW(g_hToolTabDiff, WM_SETFONT, (WPARAM)(toolIndex == TOOL_DIFF ? g_hBoldFont : g_hFont), TRUE);

    bool isTools = (g_activeTab == TAB_TOOLS);

    // Common tools header
    ShowControls(isTools, { g_hToolTabAna, g_hToolTabHid, g_hToolTabDiff });

    // Anagram controls
    ShowControls(isTools && toolIndex == TOOL_ANAGRAM, {
        g_hToolAnaInputLbl, g_hToolAnaInput,
        g_hToolAnaUseFodder, g_hToolAnaUseAns, g_hToolAnaCount,
        g_hToolAnaList, g_hToolAnaSetAns
    });

    // Hidden word controls
    ShowControls(isTools && toolIndex == TOOL_HIDDEN, {
        g_hToolHidPhraseLbl, g_hToolHidPhrase, g_hToolHidUseClue,
        g_hToolHidMinLenLbl, g_hToolHidMinLen,
        g_hToolHidScopeLbl, g_hToolHidScope,
        g_hToolHidRevLbl, g_hToolHidRev,
        g_hToolHidCount, g_hToolHidList, g_hToolHidSetAns
    });

    // Addition & Removal controls
    ShowControls(isTools && toolIndex == TOOL_DIFF, {
        g_hToolDiffMainLbl, g_hToolDiffMain,
        g_hToolDiffGoalLbl, g_hToolDiffGoal,
        g_hToolDiffSwap, g_hToolDiffUseClue,
        g_hToolDiffResult,
        g_hToolDiffDelLbl, g_hToolDiffDelList,
        g_hToolDiffAddLbl, g_hToolDiffAddList
    });
}

static void ShowTab(int tab) {
    // Builder tab controls
    ShowControls(tab == TAB_BUILDER, {
        g_hChipWnd,
        g_hVisualize, g_hSaveDb,
        g_hStatusBar,
    });

    // Indicator bank controls
    ShowControls(tab == TAB_INDICATORS, {
        g_hIndFilterLbl, g_hIndFilter,
        g_hIndSearchLbl, g_hIndSearch,
        g_hIndCountLbl, g_hIndList,
        g_hIndExTitle, g_hIndExClue, g_hIndExExplain, g_hIndExTry,
        g_hIndAddLbl, g_hIndAddEdit, g_hIndAddCat, g_hIndAddBtn, g_hIndRemoveBtn,
    });

    // Substitutions controls (String scanner)
    ShowControls(tab == TAB_SUBS, {
        g_hSubInputLbl, g_hSubInput, g_hSubUseClueBtn, g_hSubDirBtn,
        g_hSubMatchesLbl, g_hSubMatchesList,
        g_hSubFromLbl, g_hSubFrom, g_hSubToLbl, g_hSubTo,
        g_hSubAddBtn, g_hSubRemoveBtn,
    });

    // Tools tab controls
    ShowToolView(g_activeTool);

    // Database tab controls
    ShowControls(tab == TAB_DATABASE, {
        g_hDbSearchLbl, g_hDbSearch, g_hDbCount,
        g_hDbList, g_hDbDetail,
        g_hDbDeleteBtn, g_hDbCopyBtn, g_hDbLoadBtn,
    });

    // Config tab controls
    ShowControls(tab == TAB_CONFIG, {
        g_hCfgIgnoredLbl, g_hCfgIgnoreWs, g_hCfgIgnoreNums, g_hCfgIgnorePunct, g_hCfgIgnoreSpecial,
        g_hCfgIgnoredDisp, g_hCfgCharNote,
        g_hCfgDictLbl, g_hCfgDictDir, g_hCfgDictBrowse, g_hCfgDictReload, g_hCfgDictStatus,
        g_hCfgDefScopeLbl, g_hCfgDefScope,
        g_hCfgDefMinLenLbl, g_hCfgDefMinLen,
        g_hCfgDefDirLbl, g_hCfgDefDir,
        g_hCfgRegNote,
    });
}

static void LayoutControls(HWND hWnd) {
    RECT cr;
    GetClientRect(hWnd, &cr);
    int cw = cr.right, ch = cr.bottom;

    // 1. Persistent Top Clue & Answer Bar (Visible across all tabs, matching CrossTools's top bar)
    int topH = 34;
    int ansAreaW = 55 + 130 + 38 + 12; // 235px
    int clueEditW = cw - 58 - ansAreaW - 12;
    if (clueEditW < 120) clueEditW = 120;

    MoveWindow(g_hClueLbl, 12, 14, 42, 26, TRUE);
    MoveWindow(g_hClueEdit, 56, 10, clueEditW, topH, TRUE);

    int ax = cw - ansAreaW;
    MoveWindow(g_hAnswerLbl, ax, 14, 55, 26, TRUE); ax += 58;
    MoveWindow(g_hAnswer, ax, 10, 130, topH, TRUE); ax += 134;
    MoveWindow(g_hCountLbl, ax, 14, 38, 26, TRUE);

    // 2. Tab Control directly beneath top clue bar
    int tabTop = 54;
    MoveWindow(g_hTab, 10, tabTop, cw - 20, ch - tabTop - 6, TRUE);

    RECT tr = { 10, tabTop, cw - 10, ch - 6 };
    TabCtrl_AdjustRect(g_hTab, FALSE, &tr);
    int tx = tr.left + 6, ty = tr.top + 6, tw = tr.right - tr.left - 12, th = tr.bottom - tr.top - 12;

    // -------------------------------------------------------------
    // Tab 0: Clue Builder Layout
    // -------------------------------------------------------------
    {
        int btmBarH = 68;
        int canvasH = th - btmBarH - 8;
        if (canvasH < 120) canvasH = 120;

        // Big Clue Canvas at TOP of stack
        MoveWindow(g_hChipWnd, tx, ty, tw, canvasH, TRUE);

        // Buttons at BOTTOM of stack
        int curY = ty + canvasH + 6;
        int curX = tx;

        int btnW1    = 130;
        int btnW2    = 140;
        int lineH    = 26;

        MoveWindow(g_hVisualize, curX, curY - 1, btnW1, lineH + 2, TRUE); curX += btnW1 + 10;
        MoveWindow(g_hSaveDb, curX, curY - 1, btnW2, lineH + 2, TRUE);

        // Status bar
        MoveWindow(g_hStatusBar, tx, curY + 30, tw, 22, TRUE);
    }

    // -------------------------------------------------------------
    // Tab 1: Indicator Bank Layout (Prominent Filters)
    // -------------------------------------------------------------
    {
        int curY = ty;
        int lineH = 26;

        // Filter Bar (Prominent at top)
        MoveWindow(g_hIndFilterLbl, tx, curY + 3, 56, lineH, TRUE);
        MoveWindow(g_hIndFilter, tx + 60, curY, 150, lineH * 10, TRUE);
        MoveWindow(g_hIndSearchLbl, tx + 220, curY + 3, 50, lineH, TRUE);
        MoveWindow(g_hIndSearch, tx + 274, curY, 170, lineH, TRUE);
        MoveWindow(g_hIndCountLbl, tx + 456, curY + 4, tw - 460, lineH, TRUE);

        curY += lineH + 8;

        int listH = th - (curY - ty) - 40;
        if (listH < 120) listH = 120;
        int colW1 = (tw - 16) * 42 / 100;
        int colW2 = tw - colW1 - 16;

        MoveWindow(g_hIndList, tx, curY, colW1, listH, TRUE);

        int rX = tx + colW1 + 16;
        MoveWindow(g_hIndExTitle, rX, curY, colW2, 24, TRUE);
        MoveWindow(g_hIndExClue,  rX, curY + 28, colW2, 60, TRUE);
        MoveWindow(g_hIndExExplain, rX, curY + 94, colW2, 80, TRUE);
        MoveWindow(g_hIndExTry, rX, curY + 182, 200, 28, TRUE);

        int botY = curY + listH + 8;
        MoveWindow(g_hIndAddLbl, tx, botY + 3, 36, lineH, TRUE);
        MoveWindow(g_hIndAddEdit, tx + 40, botY, 140, lineH, TRUE);
        MoveWindow(g_hIndAddCat, tx + 186, botY, 120, lineH * 10, TRUE);
        MoveWindow(g_hIndAddBtn, tx + 312, botY, 95, lineH, TRUE);
        MoveWindow(g_hIndRemoveBtn, tx + 414, botY, 130, lineH, TRUE);
    }

    // -------------------------------------------------------------
    // Tab 2: Substitutions Layout (Focused String Scanner)
    // -------------------------------------------------------------
    {
        int curY = ty;
        int lineH = 26;

        // Top Scanner Input Bar with Direction Switcher
        MoveWindow(g_hSubInputLbl, tx, curY + 4, tw - 190, 20, TRUE);
        MoveWindow(g_hSubDirBtn, tx + tw - 180, curY, 180, lineH + 2, TRUE);
        curY += 32;

        int btnW = 140;
        MoveWindow(g_hSubInput, tx, curY, tw - btnW - 8, lineH + 2, TRUE);
        MoveWindow(g_hSubUseClueBtn, tx + tw - btnW, curY, btnW, lineH + 2, TRUE);

        curY += lineH + 10;
        MoveWindow(g_hSubMatchesLbl, tx, curY, tw, 22, TRUE);

        curY += 24;
        int listH = th - (curY - ty) - 40;
        if (listH < 120) listH = 120;
        MoveWindow(g_hSubMatchesList, tx, curY, tw, listH, TRUE);

        int subBotY = curY + listH + 8;
        MoveWindow(g_hSubFromLbl, tx, subBotY + 3, 44, lineH, TRUE);
        MoveWindow(g_hSubFrom, tx + 48, subBotY, 120, lineH, TRUE);
        MoveWindow(g_hSubToLbl, tx + 176, subBotY + 3, 36, lineH, TRUE);
        MoveWindow(g_hSubTo, tx + 216, subBotY, 100, lineH, TRUE);
        MoveWindow(g_hSubAddBtn, tx + 324, subBotY, 110, lineH, TRUE);
        MoveWindow(g_hSubRemoveBtn, tx + 442, subBotY, 130, lineH, TRUE);
    }

    // -------------------------------------------------------------
    // Tab 3: Tools Layout (Modern Full-View Tabs)
    // -------------------------------------------------------------
    {
        int curY = ty;
        int lineH = 26;

        // 1. Tool Mode Switcher Bar at Top
        int btnW = (tw - 16) / 3;
        MoveWindow(g_hToolTabAna,  tx, curY, btnW, 28, TRUE);
        MoveWindow(g_hToolTabHid,  tx + btnW + 8, curY, btnW, 28, TRUE);
        MoveWindow(g_hToolTabDiff, tx + (btnW + 8) * 2, curY, btnW, 28, TRUE);

        curY += 36;
        int toolAreaH = th - (curY - ty);

        // Tool 1: Anagrams Layout (Full View, matching CrossTools Fill tab)
        {
            MoveWindow(g_hToolAnaInputLbl, tx, curY + 4, 115, lineH, TRUE);
            MoveWindow(g_hToolAnaInput, tx + 120, curY, 200, lineH + 2, TRUE);
            MoveWindow(g_hToolAnaUseFodder, tx + 328, curY, 125, lineH + 2, TRUE);
            MoveWindow(g_hToolAnaUseAns, tx + 460, curY, 105, lineH + 2, TRUE);
            MoveWindow(g_hToolAnaSetAns, tx + 572, curY, 145, lineH + 2, TRUE);

            int aY = curY + lineH + 10;
            MoveWindow(g_hToolAnaCount, tx, aY, tw, 22, TRUE);
            aY += 24;
            int aListH = toolAreaH - (aY - curY);
            if (aListH < 100) aListH = 100;
            MoveWindow(g_hToolAnaList, tx, aY, tw, aListH, TRUE);
        }

        // Tool 2: Hidden Words Layout (Full View)
        {
            MoveWindow(g_hToolHidPhraseLbl, tx, curY + 4, 95, lineH, TRUE);
            int phrW = tw - 240;
            MoveWindow(g_hToolHidPhrase, tx + 100, curY, phrW, lineH + 2, TRUE);
            MoveWindow(g_hToolHidUseClue, tx + 108 + phrW, curY, 125, lineH + 2, TRUE);

            int hY = curY + lineH + 8;
            MoveWindow(g_hToolHidMinLenLbl, tx, hY + 3, 60, lineH, TRUE);
            MoveWindow(g_hToolHidMinLen, tx + 64, hY, 90, lineH * 5, TRUE);
            MoveWindow(g_hToolHidScopeLbl, tx + 164, hY + 3, 46, lineH, TRUE);
            MoveWindow(g_hToolHidScope, tx + 214, hY, 230, lineH * 5, TRUE);
            MoveWindow(g_hToolHidRevLbl, tx + 454, hY + 3, 65, lineH, TRUE);
            MoveWindow(g_hToolHidRev, tx + 524, hY, 140, lineH * 5, TRUE);
            MoveWindow(g_hToolHidSetAns, tx + 672, hY - 1, 145, lineH + 2, TRUE);

            hY += lineH + 8;
            MoveWindow(g_hToolHidCount, tx, hY, tw, 22, TRUE);
            hY += 24;
            int hListH = toolAreaH - (hY - curY);
            if (hListH < 100) hListH = 100;
            MoveWindow(g_hToolHidList, tx, hY, tw, hListH, TRUE);
        }

        // Tool 3: Addition & Removal Layout (Interactive Split View)
        {
            int dY = curY;
            MoveWindow(g_hToolDiffMainLbl, tx, dY + 3, 75, lineH, TRUE);
            MoveWindow(g_hToolDiffMain, tx + 80, dY, 150, lineH + 2, TRUE);
            MoveWindow(g_hToolDiffGoalLbl, tx + 240, dY + 3, 72, lineH, TRUE);
            MoveWindow(g_hToolDiffGoal, tx + 316, dY, 150, lineH + 2, TRUE);
            MoveWindow(g_hToolDiffSwap, tx + 474, dY, 70, lineH + 2, TRUE);
            MoveWindow(g_hToolDiffUseClue, tx + 550, dY, 160, lineH + 2, TRUE);

            dY += lineH + 8;
            int resH = 130;
            MoveWindow(g_hToolDiffResult, tx, dY, tw, resH, TRUE);

            dY += resH + 8;
            int splitW = (tw - 12) / 2;
            MoveWindow(g_hToolDiffDelLbl, tx, dY, splitW, 20, TRUE);
            MoveWindow(g_hToolDiffAddLbl, tx + splitW + 12, dY, splitW, 20, TRUE);

            dY += 22;
            int listH = toolAreaH - (dY - curY);
            if (listH < 100) listH = 100;
            MoveWindow(g_hToolDiffDelList, tx, dY, splitW, listH, TRUE);
            MoveWindow(g_hToolDiffAddList, tx + splitW + 12, dY, splitW, listH, TRUE);
        }
    }

    // -------------------------------------------------------------
    // Tab 4: Database Layout
    // -------------------------------------------------------------
    {
        int curY = ty;
        int lineH = 26;

        MoveWindow(g_hDbSearchLbl, tx, curY + 3, 50, lineH, TRUE);
        MoveWindow(g_hDbSearch, tx + 54, curY, 200, lineH, TRUE);
        MoveWindow(g_hDbCount, tx + 264, curY + 4, 200, lineH, TRUE);

        curY += lineH + 8;
        int dbListH = (th - (curY - ty) - 44) / 2;
        if (dbListH < 80) dbListH = 80;
        MoveWindow(g_hDbList, tx, curY, tw, dbListH, TRUE);

        curY += dbListH + 8;
        int dbDetH = th - (curY - ty) - 36;
        if (dbDetH < 80) dbDetH = 80;
        MoveWindow(g_hDbDetail, tx, curY, tw, dbDetH, TRUE);

        int dbBotY = curY + dbDetH + 8;
        MoveWindow(g_hDbDeleteBtn, tx, dbBotY, 110, lineH, TRUE);
        MoveWindow(g_hDbCopyBtn, tx + 120, dbBotY, 130, lineH, TRUE);
        MoveWindow(g_hDbLoadBtn, tx + 260, dbBotY, 150, lineH, TRUE);
    }

    // -------------------------------------------------------------
    // Tab 5: Config Layout (Two Clean Columns Matching CrossTools)
    // -------------------------------------------------------------
    {
        int curY = ty;
        int lineH = 26;
        int colW = (tw - 24) / 2;

        // Left Column: Ignored Characters & Stripping
        MoveWindow(g_hCfgIgnoredLbl, tx, curY + 2, colW, 20, TRUE);
        MoveWindow(g_hCfgIgnoreWs, tx + 8, curY + 26, colW - 8, 22, TRUE);
        MoveWindow(g_hCfgIgnoreNums, tx + 8, curY + 50, colW - 8, 22, TRUE);
        MoveWindow(g_hCfgIgnorePunct, tx + 8, curY + 74, colW - 8, 22, TRUE);
        MoveWindow(g_hCfgIgnoreSpecial, tx + 8, curY + 98, colW - 8, 22, TRUE);
        MoveWindow(g_hCfgIgnoredDisp, tx, curY + 126, colW, 76, TRUE);
        MoveWindow(g_hCfgCharNote, tx, curY + 208, colW, 36, TRUE);

        // Right Column: Dictionaries Folder & Cryptic Defaults
        int rX = tx + colW + 24;
        MoveWindow(g_hCfgDictLbl, rX, curY + 2, colW, 20, TRUE);
        MoveWindow(g_hCfgDictDir, rX, curY + 26, colW - 176, lineH + 2, TRUE);
        MoveWindow(g_hCfgDictBrowse, rX + colW - 170, curY + 26, 78, lineH + 2, TRUE);
        MoveWindow(g_hCfgDictReload, rX + colW - 86, curY + 26, 86, lineH + 2, TRUE);
        MoveWindow(g_hCfgDictStatus, rX, curY + 58, colW, 22, TRUE);

        int defY = curY + 90;
        int lblW = 165;

        MoveWindow(g_hCfgDefScopeLbl, rX, defY + 3, lblW, lineH, TRUE);
        MoveWindow(g_hCfgDefScope, rX + lblW + 6, defY, colW - lblW - 6, lineH * 5, TRUE);

        defY += 32;
        MoveWindow(g_hCfgDefMinLenLbl, rX, defY + 3, lblW, lineH, TRUE);
        MoveWindow(g_hCfgDefMinLen, rX + lblW + 6, defY, colW - lblW - 6, lineH * 5, TRUE);

        defY += 32;
        MoveWindow(g_hCfgDefDirLbl, rX, defY + 3, lblW, lineH, TRUE);
        MoveWindow(g_hCfgDefDir, rX + lblW + 6, defY, colW - lblW - 6, lineH * 5, TRUE);

        defY += 36;
        MoveWindow(g_hCfgRegNote, rX, defY, colW, 36, TRUE);
    }
}

// ---------------------------------------------------------------------------
// Control Construction in WM_CREATE
// ---------------------------------------------------------------------------

static void CreateAllControls(HWND hWnd) {
    auto SetF = [&](HWND h) { SendMessageW(h, WM_SETFONT, (WPARAM)g_hFont, FALSE); };

    // 1. Persistent Top Clue & Answer Bar (Above Tab Control, matching CrossTools's top bar)
    g_hClueLbl = CreateWindowExW(0, L"STATIC", L"Clue:", WS_CHILD | WS_VISIBLE | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_CLUE_LBL, g_hInst, nullptr);
    SendMessageW(g_hClueLbl, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hClueEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_CLUE_EDIT, g_hInst, nullptr);
    SendMessageW(g_hClueEdit, WM_SETFONT, (WPARAM)g_hInputFont, FALSE);

    g_hAnswerLbl = CreateWindowExW(0, L"STATIC", L"Answer:", WS_CHILD | WS_VISIBLE | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_ANSWER_LBL, g_hInst, nullptr);
    SendMessageW(g_hAnswerLbl, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hAnswer = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_UPPERCASE, 0,0,1,1, hWnd, (HMENU)IDC_ANSWER, g_hInst, nullptr);
    SendMessageW(g_hAnswer, WM_SETFONT, (WPARAM)g_hInputFont, FALSE);

    g_hCountLbl = CreateWindowExW(0, L"STATIC", L"(0)", WS_CHILD | WS_VISIBLE | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_COUNT_LBL, g_hInst, nullptr);
    SendMessageW(g_hCountLbl, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    // 2. Master Tab Control
    g_hTab = CreateWindowExW(0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_FOCUSNEVER,
        0, 0, 100, 100, hWnd, (HMENU)IDC_TAB, g_hInst, nullptr);
    SetF(g_hTab);

    auto AddTab = [&](const wchar_t* title) {
        TCITEMW ti = {};
        ti.mask = TCIF_TEXT;
        ti.pszText = (LPWSTR)title;
        int n = (int)SendMessageW(g_hTab, TCM_GETITEMCOUNT, 0, 0);
        SendMessageW(g_hTab, TCM_INSERTITEMW, n, (LPARAM)&ti);
    };
    AddTab(L"Clue Builder");
    AddTab(L"Indicator Bank");
    AddTab(L"Substitutions");
    AddTab(L"Tools");
    AddTab(L"Database");
    AddTab(L"Config");

    // -------------------------------------------------------------
    // Tab 0: Clue Builder Controls
    // -------------------------------------------------------------
    g_hChipWnd = CreateWindowExW(WS_EX_CLIENTEDGE, CHIP_CLASS, L"", WS_CHILD | WS_TABSTOP, 0,0,1,1, hWnd, (HMENU)IDC_CHIP_WND, g_hInst, nullptr);

    g_hVisualize = CreateWindowExW(0, L"BUTTON", L"Visualize \u25B6", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_VISUALIZE, g_hInst, nullptr);
    SendMessageW(g_hVisualize, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hSaveDb = CreateWindowExW(0, L"BUTTON", L"Save to Database", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_SAVE_DB, g_hInst, nullptr);
    SetF(g_hSaveDb);

    g_hStatusBar = CreateWindowExW(WS_EX_STATICEDGE, L"STATIC", L"Ready. Click word cards to assign roles (Definition, Fodder, Indicator, Substitution). Drag to select span.",
        WS_CHILD | SS_LEFT | SS_SUNKEN, 0,0,1,1, hWnd, (HMENU)IDC_STATUS_BAR, g_hInst, nullptr);
    SetF(g_hStatusBar);

    // -------------------------------------------------------------
    // Tab 1: Indicator Bank Controls (Prominent Filters)
    // -------------------------------------------------------------
    g_hIndFilterLbl = CreateWindowExW(0, L"STATIC", L"Category:", WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_IND_FILTER_LBL, g_hInst, nullptr);
    SetF(g_hIndFilterLbl);

    g_hIndFilter = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_IND_FILTER, g_hInst, nullptr);
    SetF(g_hIndFilter);
    {
        const wchar_t* cats[] = { L"All Categories", L"anagram", L"reversal", L"hidden", L"container", L"homophone", L"deletion", L"acrostic" };
        for (auto c : cats) SendMessageW(g_hIndFilter, CB_ADDSTRING, 0, (LPARAM)c);
        SendMessageW(g_hIndFilter, CB_SETCURSEL, 0, 0);
    }

    g_hIndSearchLbl = CreateWindowExW(0, L"STATIC", L"Search:", WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_IND_SEARCH_LBL, g_hInst, nullptr);
    SetF(g_hIndSearchLbl);

    g_hIndSearch = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_IND_SEARCH, g_hInst, nullptr);
    SetF(g_hIndSearch);

    g_hIndCountLbl = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_IND_COUNT_LBL, g_hInst, nullptr);
    SetF(g_hIndCountLbl);

    g_hIndList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | LBS_NOTIFY | LBS_SORT | WS_VSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_IND_LIST, g_hInst, nullptr);
    SetF(g_hIndList);

    g_hIndExTitle = CreateWindowExW(0, L"STATIC", L"Example Usage in Bank of Cryptics:", WS_CHILD | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_IND_EX_TITLE, g_hInst, nullptr);
    SendMessageW(g_hIndExTitle, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hIndExClue = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Select an indicator from the list to see real-world clue examples.",
        WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_IND_EX_CLUE, g_hInst, nullptr);
    SetF(g_hIndExClue);

    g_hIndExExplain = CreateWindowExW(WS_EX_STATICEDGE, L"STATIC", L"", WS_CHILD | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_IND_EX_EXPLAIN, g_hInst, nullptr);
    SetF(g_hIndExExplain);

    g_hIndExTry = CreateWindowExW(0, L"BUTTON", L"Load Example into Builder", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_IND_EX_TRY, g_hInst, nullptr);
    SetF(g_hIndExTry);

    g_hIndAddLbl = CreateWindowExW(0, L"STATIC", L"New:", WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_IND_ADD_LBL, g_hInst, nullptr);
    SetF(g_hIndAddLbl);

    g_hIndAddEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_IND_ADD_EDIT, g_hInst, nullptr);
    SetF(g_hIndAddEdit);

    g_hIndAddCat = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_IND_ADD_CAT, g_hInst, nullptr);
    SetF(g_hIndAddCat);
    {
        const wchar_t* cats[] = { L"anagram", L"reversal", L"hidden", L"container", L"homophone", L"deletion", L"acrostic" };
        for (auto c : cats) SendMessageW(g_hIndAddCat, CB_ADDSTRING, 0, (LPARAM)c);
        SendMessageW(g_hIndAddCat, CB_SETCURSEL, 0, 0);
    }

    g_hIndAddBtn = CreateWindowExW(0, L"BUTTON", L"Add Indicator", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_IND_ADD_BTN, g_hInst, nullptr);
    SetF(g_hIndAddBtn);

    g_hIndRemoveBtn = CreateWindowExW(0, L"BUTTON", L"Remove Selected", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_IND_REMOVE_BTN, g_hInst, nullptr);
    SetF(g_hIndRemoveBtn);

    // -------------------------------------------------------------
    // Tab 2: Substitutions Controls (Focused String Scanner)
    // -------------------------------------------------------------
    g_hSubInputLbl = CreateWindowExW(0, L"STATIC", L"Input word, phrase, or clue to scan for substitutions in bank:", WS_CHILD | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_SUB_INPUT_LBL, g_hInst, nullptr);
    SendMessageW(g_hSubInputLbl, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hSubInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_SUB_INPUT, g_hInst, nullptr);
    SendMessageW(g_hSubInput, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hSubUseClueBtn = CreateWindowExW(0, L"BUTTON", L"Use Current Clue", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_SUB_USE_CLUE_BTN, g_hInst, nullptr);
    SetF(g_hSubUseClueBtn);

    g_hSubDirBtn = CreateWindowExW(0, L"BUTTON",
        g_subDirectionReverse ? L"Direction: Abbr \u2192 Word" : L"Direction: Word \u2192 Abbr",
        WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_SUB_DIR_BTN, g_hInst, nullptr);
    SendMessageW(g_hSubDirBtn, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hSubMatchesLbl = CreateWindowExW(0, L"STATIC", L"Substitution Bank:", WS_CHILD | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_SUB_MATCHES_LBL, g_hInst, nullptr);
    SetF(g_hSubMatchesLbl);

    g_hSubMatchesList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | LBS_NOTIFY | WS_VSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_SUB_MATCHES_LIST, g_hInst, nullptr);
    SetF(g_hSubMatchesList);

    g_hSubFromLbl = CreateWindowExW(0, L"STATIC", g_subDirectionReverse ? L"Abbr:" : L"Word:", WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_SUB_FROM_LBL, g_hInst, nullptr);
    SetF(g_hSubFromLbl);

    g_hSubFrom = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_SUB_FROM, g_hInst, nullptr);
    SetF(g_hSubFrom);

    g_hSubToLbl = CreateWindowExW(0, L"STATIC", g_subDirectionReverse ? L"Word:" : L"Abbr:", WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_SUB_TO_LBL, g_hInst, nullptr);
    SetF(g_hSubToLbl);

    g_hSubTo = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_SUB_TO, g_hInst, nullptr);
    SetF(g_hSubTo);

    g_hSubAddBtn = CreateWindowExW(0, L"BUTTON", L"Add to Bank", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_SUB_ADD_BTN, g_hInst, nullptr);
    SetF(g_hSubAddBtn);

    g_hSubRemoveBtn = CreateWindowExW(0, L"BUTTON", L"Remove Selected", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_SUB_REMOVE_BTN, g_hInst, nullptr);
    SetF(g_hSubRemoveBtn);

    // -------------------------------------------------------------
    // Tab 3: Tools Controls (Full View Tabs: Anagrams, Hidden, Add/Remove)
    // -------------------------------------------------------------
    g_hToolTabAna = CreateWindowExW(0, L"BUTTON", L"\u21C4 Anagram Generator", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_TAB_ANA, g_hInst, nullptr);
    SendMessageW(g_hToolTabAna, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hToolTabHid = CreateWindowExW(0, L"BUTTON", L"\u25CE Hidden Word Finder", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_TAB_HID, g_hInst, nullptr);
    SetF(g_hToolTabHid);

    g_hToolTabDiff = CreateWindowExW(0, L"BUTTON", L"\u00B1 Addition & Removal", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_TAB_DIFF, g_hInst, nullptr);
    SetF(g_hToolTabDiff);

    // Tool 1: Anagrams (Owner-Drawn for Green / Yellow / Plain highlighting)
    g_hToolAnaInputLbl = CreateWindowExW(0, L"STATIC", L"Word to Anagram:", WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_ANA_INPUT_LBL, g_hInst, nullptr);
    SendMessageW(g_hToolAnaInputLbl, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hToolAnaInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL | ES_UPPERCASE, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_ANA_INPUT, g_hInst, nullptr);
    SendMessageW(g_hToolAnaInput, WM_SETFONT, (WPARAM)g_hInputFont, FALSE);

    g_hToolAnaUseFodder = CreateWindowExW(0, L"BUTTON", L"Use Clue Fodder", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_ANA_USE_FODDER, g_hInst, nullptr);
    SetF(g_hToolAnaUseFodder);

    g_hToolAnaUseAns = CreateWindowExW(0, L"BUTTON", L"Use Answer", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_ANA_USE_ANS, g_hInst, nullptr);
    SetF(g_hToolAnaUseAns);

    g_hToolAnaSetAns = CreateWindowExW(0, L"BUTTON", L"Set Selected as Answer", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_ANA_SET_ANS, g_hInst, nullptr);
    SetF(g_hToolAnaSetAns);

    g_hToolAnaCount = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_ANA_COUNT, g_hInst, nullptr);
    SetF(g_hToolAnaCount);

    g_hToolAnaList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | WS_VSCROLL,
        0,0,1,1, hWnd, (HMENU)IDC_TOOL_ANA_LIST, g_hInst, nullptr);
    SetF(g_hToolAnaList);

    // Tool 2: Hidden Words (Owner-Drawn with Boundary-Spanning Green & Inner Substring Yellow)
    g_hToolHidPhraseLbl = CreateWindowExW(0, L"STATIC", L"Phrase to scan:", WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_HID_PHRASE_LBL, g_hInst, nullptr);
    SendMessageW(g_hToolHidPhraseLbl, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hToolHidPhrase = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_HID_PHRASE, g_hInst, nullptr);
    SendMessageW(g_hToolHidPhrase, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hToolHidUseClue = CreateWindowExW(0, L"BUTTON", L"Use Current Clue", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_HID_USE_CLUE, g_hInst, nullptr);
    SetF(g_hToolHidUseClue);

    g_hToolHidMinLenLbl = CreateWindowExW(0, L"STATIC", L"Min Len:", WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_HID_MINLEN_LBL, g_hInst, nullptr);
    SetF(g_hToolHidMinLenLbl);

    g_hToolHidMinLen = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_HID_MINLEN, g_hInst, nullptr);
    SetF(g_hToolHidMinLen);
    SendMessageW(g_hToolHidMinLen, CB_ADDSTRING, 0, (LPARAM)L"3+ letters");
    SendMessageW(g_hToolHidMinLen, CB_ADDSTRING, 0, (LPARAM)L"4+ letters");
    SendMessageW(g_hToolHidMinLen, CB_ADDSTRING, 0, (LPARAM)L"5+ letters");
    SendMessageW(g_hToolHidMinLen, CB_ADDSTRING, 0, (LPARAM)L"6+ letters");
    SendMessageW(g_hToolHidMinLen, CB_SETCURSEL, 0, 0);

    g_hToolHidScopeLbl = CreateWindowExW(0, L"STATIC", L"Scope:", WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_HID_SCOPE_LBL, g_hInst, nullptr);
    SetF(g_hToolHidScopeLbl);

    g_hToolHidScope = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_HID_SCOPE, g_hInst, nullptr);
    SetF(g_hToolHidScope);
    SendMessageW(g_hToolHidScope, CB_ADDSTRING, 0, (LPARAM)L"Boundary Spanning Only (Default)");
    SendMessageW(g_hToolHidScope, CB_ADDSTRING, 0, (LPARAM)L"All Inner Substrings (Green + Yellow)");
    SendMessageW(g_hToolHidScope, CB_ADDSTRING, 0, (LPARAM)L"Single Word Interiors Only (Yellow)");
    SendMessageW(g_hToolHidScope, CB_SETCURSEL, g_cfgHiddenScope, 0);

    g_hToolHidRevLbl = CreateWindowExW(0, L"STATIC", L"Reversals:", WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_HID_REV_LBL, g_hInst, nullptr);
    SetF(g_hToolHidRevLbl);

    g_hToolHidRev = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_HID_REV, g_hInst, nullptr);
    SetF(g_hToolHidRev);
    SendMessageW(g_hToolHidRev, CB_ADDSTRING, 0, (LPARAM)L"Include Reversals");
    SendMessageW(g_hToolHidRev, CB_ADDSTRING, 0, (LPARAM)L"Forward Only");
    SendMessageW(g_hToolHidRev, CB_SETCURSEL, 0, 0);

    g_hToolHidSetAns = CreateWindowExW(0, L"BUTTON", L"Set Selected as Answer", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_HID_SET_ANS, g_hInst, nullptr);
    SetF(g_hToolHidSetAns);

    g_hToolHidCount = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_HID_COUNT, g_hInst, nullptr);
    SetF(g_hToolHidCount);

    g_hToolHidList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | WS_VSCROLL,
        0,0,1,1, hWnd, (HMENU)IDC_TOOL_HID_LIST, g_hInst, nullptr);
    SetF(g_hToolHidList);

    // Tool 3: Addition & Removal
    g_hToolDiffMainLbl = CreateWindowExW(0, L"STATIC", L"Main Word:", WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_DIFF_MAIN_LBL, g_hInst, nullptr);
    SendMessageW(g_hToolDiffMainLbl, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hToolDiffMain = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL | ES_UPPERCASE, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_DIFF_MAIN, g_hInst, nullptr);
    SendMessageW(g_hToolDiffMain, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hToolDiffGoalLbl = CreateWindowExW(0, L"STATIC", L"Goal Word:", WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_DIFF_GOAL_LBL, g_hInst, nullptr);
    SendMessageW(g_hToolDiffGoalLbl, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hToolDiffGoal = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL | ES_UPPERCASE, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_DIFF_GOAL, g_hInst, nullptr);
    SendMessageW(g_hToolDiffGoal, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hToolDiffSwap = CreateWindowExW(0, L"BUTTON", L"\u21C4 Swap", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_DIFF_SWAP, g_hInst, nullptr);
    SetF(g_hToolDiffSwap);

    g_hToolDiffUseClue = CreateWindowExW(0, L"BUTTON", L"Pull from Clue & Answer", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_DIFF_USE_CLUE, g_hInst, nullptr);
    SetF(g_hToolDiffUseClue);

    g_hToolDiffResult = CreateWindowExW(WS_EX_STATICEDGE, L"EDIT", L"", WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_DIFF_RESULT, g_hInst, nullptr);
    SetF(g_hToolDiffResult);

    g_hToolDiffDelLbl = CreateWindowExW(0, L"STATIC", L"Valid 1-Letter Deletions (Click to set as Goal):", WS_CHILD | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_DIFF_DEL_LBL, g_hInst, nullptr);
    SendMessageW(g_hToolDiffDelLbl, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hToolDiffDelList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | LBS_NOTIFY | WS_VSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_DIFF_DEL_LIST, g_hInst, nullptr);
    SetF(g_hToolDiffDelList);

    g_hToolDiffAddLbl = CreateWindowExW(0, L"STATIC", L"Valid 1-Letter Additions (Click to set as Goal):", WS_CHILD | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_DIFF_ADD_LBL, g_hInst, nullptr);
    SendMessageW(g_hToolDiffAddLbl, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hToolDiffAddList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | LBS_NOTIFY | WS_VSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_TOOL_DIFF_ADD_LIST, g_hInst, nullptr);
    SetF(g_hToolDiffAddList);

    // -------------------------------------------------------------
    // Tab 4: Database Controls
    // -------------------------------------------------------------
    g_hDbSearchLbl = CreateWindowExW(0, L"STATIC", L"Search:", WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_DB_SEARCH_LBL, g_hInst, nullptr);
    SetF(g_hDbSearchLbl);

    g_hDbSearch = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_DB_SEARCH, g_hInst, nullptr);
    SetF(g_hDbSearch);

    g_hDbCount = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_DB_COUNT, g_hInst, nullptr);
    SetF(g_hDbCount);

    g_hDbList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | LBS_NOTIFY | WS_VSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_DB_LIST, g_hInst, nullptr);
    SetF(g_hDbList);

    g_hDbDetail = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_DB_DETAIL, g_hInst, nullptr);
    SetF(g_hDbDetail);

    g_hDbDeleteBtn = CreateWindowExW(0, L"BUTTON", L"Delete Entry", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_DB_DELETE_BTN, g_hInst, nullptr);
    SetF(g_hDbDeleteBtn);

    g_hDbCopyBtn = CreateWindowExW(0, L"BUTTON", L"Copy Clue Text", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_DB_COPY_BTN, g_hInst, nullptr);
    SetF(g_hDbCopyBtn);

    g_hDbLoadBtn = CreateWindowExW(0, L"BUTTON", L"Load into Builder", WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_DB_LOAD_BTN, g_hInst, nullptr);
    SendMessageW(g_hDbLoadBtn, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    // -------------------------------------------------------------
    // Tab 5: Config Controls (Matching CrossTools Config Page)
    // -------------------------------------------------------------
    g_hCfgIgnoredLbl = CreateWindowExW(0, L"STATIC", L"Ignored Characters (Stripped from clues & dictionary words during matching):",
        WS_CHILD | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_CFG_IGNORED_LBL, g_hInst, nullptr);
    SendMessageW(g_hCfgIgnoredLbl, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hCfgIgnoreWs = CreateWindowExW(0, L"BUTTON", L"Whitespace (spaces, tabs, non-breaking spaces)",
        WS_CHILD | BS_AUTOCHECKBOX, 0,0,1,1, hWnd, (HMENU)IDC_CFG_IGNORE_WS, g_hInst, nullptr);
    SetF(g_hCfgIgnoreWs);

    g_hCfgIgnoreNums = CreateWindowExW(0, L"BUTTON", L"Numbers (0-9)",
        WS_CHILD | BS_AUTOCHECKBOX, 0,0,1,1, hWnd, (HMENU)IDC_CFG_IGNORE_NUMS, g_hInst, nullptr);
    SetF(g_hCfgIgnoreNums);

    g_hCfgIgnorePunct = CreateWindowExW(0, L"BUTTON", L"Punctuation (!\"'(),-./:;?@ etc.)",
        WS_CHILD | BS_AUTOCHECKBOX, 0,0,1,1, hWnd, (HMENU)IDC_CFG_IGNORE_PUNCT, g_hInst, nullptr);
    SetF(g_hCfgIgnorePunct);

    g_hCfgIgnoreSpecial = CreateWindowExW(0, L"BUTTON", L"Special Symbols (\u00A7\u00A9\u00AE\u00B0\u00B1 etc.)",
        WS_CHILD | BS_AUTOCHECKBOX, 0,0,1,1, hWnd, (HMENU)IDC_CFG_IGNORE_SPECIAL, g_hInst, nullptr);
    SetF(g_hCfgIgnoreSpecial);

    g_hCfgIgnoredDisp = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_CFG_IGNORED_DISP, g_hInst, nullptr);
    SetF(g_hCfgIgnoredDisp);

    g_hCfgCharNote = CreateWindowExW(0, L"STATIC",
        L"All characters in the box above are stripped when indexing dictionaries or parsing clues.",
        WS_CHILD | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_CFG_CHAR_NOTE, g_hInst, nullptr);
    SetF(g_hCfgCharNote);

    g_hCfgDictLbl = CreateWindowExW(0, L"STATIC", L"Dictionaries Folder (loads all *.dict files in directory):",
        WS_CHILD | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_CFG_DICT_LBL, g_hInst, nullptr);
    SendMessageW(g_hCfgDictLbl, WM_SETFONT, (WPARAM)g_hBoldFont, FALSE);

    g_hCfgDictDir = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_cfgDictDir.c_str(),
        WS_CHILD | ES_AUTOHSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_CFG_DICT_DIR, g_hInst, nullptr);
    SetF(g_hCfgDictDir);

    g_hCfgDictBrowse = CreateWindowExW(0, L"BUTTON", L"Browse...",
        WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_CFG_DICT_BROWSE, g_hInst, nullptr);
    SetF(g_hCfgDictBrowse);

    g_hCfgDictReload = CreateWindowExW(0, L"BUTTON", L"Reload",
        WS_CHILD | BS_PUSHBUTTON, 0,0,1,1, hWnd, (HMENU)IDC_CFG_DICT_RELOAD, g_hInst, nullptr);
    SetF(g_hCfgDictReload);

    g_hCfgDictStatus = CreateWindowExW(0, L"STATIC", L"Dictionaries loaded.",
        WS_CHILD | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_CFG_DICT_STATUS, g_hInst, nullptr);
    SetF(g_hCfgDictStatus);

    g_hCfgDefScopeLbl = CreateWindowExW(0, L"STATIC", L"Hidden Word Scope:",
        WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_CFG_DEF_SCOPE_LBL, g_hInst, nullptr);
    SetF(g_hCfgDefScopeLbl);

    g_hCfgDefScope = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_CFG_DEF_SCOPE, g_hInst, nullptr);
    SetF(g_hCfgDefScope);
    SendMessageW(g_hCfgDefScope, CB_ADDSTRING, 0, (LPARAM)L"Boundary Spanning Only (Default)");
    SendMessageW(g_hCfgDefScope, CB_ADDSTRING, 0, (LPARAM)L"All Inner Substrings (Green + Yellow)");
    SendMessageW(g_hCfgDefScope, CB_ADDSTRING, 0, (LPARAM)L"Single Word Interiors Only (Yellow)");
    SendMessageW(g_hCfgDefScope, CB_SETCURSEL, g_cfgHiddenScope, 0);

    g_hCfgDefMinLenLbl = CreateWindowExW(0, L"STATIC", L"Hidden Word Min Len:",
        WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_CFG_DEF_MINLEN_LBL, g_hInst, nullptr);
    SetF(g_hCfgDefMinLenLbl);

    g_hCfgDefMinLen = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_CFG_DEF_MINLEN, g_hInst, nullptr);
    SetF(g_hCfgDefMinLen);
    SendMessageW(g_hCfgDefMinLen, CB_ADDSTRING, 0, (LPARAM)L"3+ letters");
    SendMessageW(g_hCfgDefMinLen, CB_ADDSTRING, 0, (LPARAM)L"4+ letters");
    SendMessageW(g_hCfgDefMinLen, CB_ADDSTRING, 0, (LPARAM)L"5+ letters");
    SendMessageW(g_hCfgDefMinLen, CB_ADDSTRING, 0, (LPARAM)L"6+ letters");
    {
        int sel = 0;
        if (g_cfgHiddenMinLen == 4) sel = 1;
        else if (g_cfgHiddenMinLen == 5) sel = 2;
        else if (g_cfgHiddenMinLen >= 6) sel = 3;
        SendMessageW(g_hCfgDefMinLen, CB_SETCURSEL, sel, 0);
    }

    g_hCfgDefDirLbl = CreateWindowExW(0, L"STATIC", L"Substitutions Direction:",
        WS_CHILD | SS_RIGHT, 0,0,1,1, hWnd, (HMENU)IDC_CFG_DEF_DIR_LBL, g_hInst, nullptr);
    SetF(g_hCfgDefDirLbl);

    g_hCfgDefDir = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 0,0,1,1, hWnd, (HMENU)IDC_CFG_DEF_DIR, g_hInst, nullptr);
    SetF(g_hCfgDefDir);
    SendMessageW(g_hCfgDefDir, CB_ADDSTRING, 0, (LPARAM)L"Word \u2192 Abbr (e.g. Jack \u2192 J)");
    SendMessageW(g_hCfgDefDir, CB_ADDSTRING, 0, (LPARAM)L"Abbr \u2192 Word (e.g. J \u2192 Jack)");
    SendMessageW(g_hCfgDefDir, CB_SETCURSEL, g_subDirectionReverse ? 1 : 0, 0);

    g_hCfgRegNote = CreateWindowExW(0, L"STATIC",
        L"\u2714 Settings and preferences are automatically saved in the Windows Registry (HKCU\\Software\\CrypTools).",
        WS_CHILD | SS_LEFT, 0,0,1,1, hWnd, (HMENU)IDC_CFG_REG_NOTE, g_hInst, nullptr);
    SetF(g_hCfgRegNote);

    // Initialize checkbox states from loaded registry values
    SendMessageW(g_hCfgIgnoreWs,      BM_SETCHECK, g_cfgIgnoreWs ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_hCfgIgnoreNums,    BM_SETCHECK, g_cfgIgnoreNums ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_hCfgIgnorePunct,   BM_SETCHECK, g_cfgIgnorePunct ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_hCfgIgnoreSpecial, BM_SETCHECK, g_cfgIgnoreSpecial ? BST_CHECKED : BST_UNCHECKED, 0);
    RefreshIgnoredDisplay();
}

// ---------------------------------------------------------------------------
// Main Window Procedure
// ---------------------------------------------------------------------------

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE: {
        BuildPaths();
        LoadSettings();
        LoadIndicators();
        LoadSubstitutions();
        LoadClues();
        LoadDictionary();

        CreateAllControls(hWnd);

        ShowTab(TAB_BUILDER);
        LayoutControls(hWnd);

        RefreshIndicatorList();
        RefreshSubstitutionList();
        RefreshDatabaseList();

        // Default initial clue for immediate user engagement
        SetWindowTextW(g_hClueEdit, L"Strange tears shed in fixed gaze");
        SetWindowTextW(g_hAnswer, L"STARE");
        SetWindowTextW(g_hToolAnaInput, L"STARE");
        SetWindowTextW(g_hToolHidPhrase, L"Strange tears shed in fixed gaze");
        SetWindowTextW(g_hToolDiffMain, L"TEARS");
        SetWindowTextW(g_hToolDiffGoal, L"STARE");

        UpdateCountLabel();
        RebuildChips(L"Strange tears shed in fixed gaze");
        if (g_chips.size() >= 6) {
            g_chips[0].label = LABEL_INDICATOR;
            g_chips[1].label = LABEL_FODDER;
            g_chips[2].label = LABEL_CONNECTOR;
            g_chips[3].label = LABEL_CONNECTOR;
            g_chips[4].label = LABEL_DEFINITION;
            g_chips[5].label = LABEL_DEFINITION;
        }
        if (g_hChipWnd) InvalidateRect(g_hChipWnd, nullptr, TRUE);

        ComputeAnagrams();
        ComputeHiddenWords();
        ComputeLetterDiff();
        ScanSubstitutionsInPhrase();

        return 0;
    }

    case WM_SIZE: {
        LayoutControls(hWnd);
        if (g_hClueEdit) {
            std::wstring clue = GetWindowTextW_s(g_hClueEdit);
            RebuildChips(clue);
        }
        if (g_hChipWnd) InvalidateRect(g_hChipWnd, nullptr, TRUE);
        return 0;
    }

    case WM_APP_CHIP_CLICK: {
        int defC = 0, fodC = 0, indC = 0, subC = 0;
        for (auto& c : g_chips) {
            if (c.label == LABEL_DEFINITION || c.label == LABEL_DOUBLEDEF2) ++defC;
            else if (c.label == LABEL_FODDER) ++fodC;
            else if (c.label == LABEL_INDICATOR) ++indC;
            else if (c.label == LABEL_SUBSTITUTION) ++subC;
        }
        wchar_t buf[256];
        swprintf_s(buf,
            L"Tagged: %d definition, %d fodder, %d indicator, %d substitution word(s). Press 'Visualize' to view collapse breakdown.",
            defC, fodC, indC, subC);
        SetStatus(buf);
        return 0;
    }

    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* mi = (MEASUREITEMSTRUCT*)lParam;
        if (mi->CtlID == IDC_TOOL_ANA_LIST || mi->CtlID == IDC_TOOL_HID_LIST) {
            mi->itemHeight = 22;
            return TRUE;
        }
        break;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lParam;
        if (di->CtlID != IDC_TOOL_ANA_LIST && di->CtlID != IDC_TOOL_HID_LIST) break;
        if ((int)di->itemID < 0) return TRUE;
        int state = (int)SendMessageW(di->hwndItem, LB_GETITEMDATA, di->itemID, 0);
        wchar_t buf[512] = { 0 };
        SendMessageW(di->hwndItem, LB_GETTEXT, di->itemID, (LPARAM)buf);
        bool green = (state == 1);
        bool yellow = (state == 2);
        bool sel = (di->itemState & ODS_SELECTED);

        HBRUSH br = CreateSolidBrush(
            sel ? (green ? RGB(168, 220, 180) : yellow ? RGB(235, 215, 130) : RGB(200, 225, 250)) :
            green ? RGB(198, 239, 206) : yellow ? RGB(255, 235, 156) : GetSysColor(COLOR_WINDOW));
        FillRect(di->hDC, &di->rcItem, br);
        DeleteObject(br);

        HGDIOBJ old = SelectObject(di->hDC, g_hFont);
        SetBkMode(di->hDC, TRANSPARENT);
        SetTextColor(di->hDC, green ? RGB(0, 97, 0) : yellow ? RGB(156, 87, 0) : GetSysColor(COLOR_WINDOWTEXT));

        RECT tr = di->rcItem;
        tr.left += 6;
        DrawTextW(di->hDC, buf, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        if (sel) {
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 120, 215));
            HGDIOBJ oldPen = SelectObject(di->hDC, pen);
            HGDIOBJ oldBrush = SelectObject(di->hDC, GetStockObject(NULL_BRUSH));
            Rectangle(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom);
            SelectObject(di->hDC, oldPen);
            SelectObject(di->hDC, oldBrush);
            DeleteObject(pen);
        }
        SelectObject(di->hDC, old);
        return TRUE;
    }

    case WM_COMMAND: {
        int id   = LOWORD(wParam);
        int note = HIWORD(wParam);

        // Tab switch command
        if (id == IDC_TAB) {
            int newTab = (int)SendMessageW(g_hTab, TCM_GETCURSEL, 0, 0);
            if (newTab >= 0 && newTab <= 5) {
                StopAnimation();
                g_activeTab = newTab;
                ShowTab(newTab);
                LayoutControls(hWnd);

                if (g_activeTab == TAB_SUBS) {
                    if (GetWindowTextLengthW(g_hSubInput) == 0) {
                        SetWindowTextW(g_hSubInput, GetWindowTextW_s(g_hClueEdit).c_str());
                    }
                    ScanSubstitutionsInPhrase();
                }
            }
            return 0;
        }

        // Clue text edited in persistent top bar
        if (id == IDC_CLUE_EDIT && note == EN_CHANGE) {
            if (g_visMode) StopAnimation();
            std::wstring clue = GetWindowTextW_s(g_hClueEdit);
            RebuildChips(clue);
            if (g_hChipWnd) InvalidateRect(g_hChipWnd, nullptr, TRUE);
            if (g_hSubInput && GetWindowTextLengthW(g_hSubInput) == 0) {
                ScanSubstitutionsInPhrase();
            }
            return 0;
        }

        // Answer edited
        if (id == IDC_ANSWER && note == EN_CHANGE) {
            if (g_visMode && g_hChipWnd) InvalidateRect(g_hChipWnd, nullptr, TRUE);
            UpdateCountLabel();
            std::wstring ans = GetWindowTextW_s(g_hAnswer);
            if (g_hToolAnaInput && GetWindowTextLengthW(g_hToolAnaInput) == 0) {
                SetWindowTextW(g_hToolAnaInput, ans.c_str());
            }
            return 0;
        }

        // Visualize button clicked
        if (id == IDC_VISUALIZE) {
            if (g_chips.empty()) {
                SetStatus(L"Please enter a clue first.");
                return 0;
            }
            if (!g_visMode) {
                g_visMode = true;
                g_animStep = 1;
                SetWindowTextW(g_hVisualize, L"Edit Clue \u270E");
                SetTimer(g_hChipWnd, IDT_ANIM, 600, nullptr);
                SetStatus(L"Visualizing collapse breakdown... Stages remain on screen.");
            } else {
                StopAnimation();
            }
            InvalidateRect(g_hChipWnd, nullptr, TRUE);
            return 0;
        }

        // Save to Database
        if (id == IDC_SAVE_DB) {
            SaveCurrentClue();
            return 0;
        }

        // Indicator Bank: Category filter changed
        if (id == IDC_IND_FILTER && note == CBN_SELCHANGE) {
            RefreshIndicatorList();
            return 0;
        }

        // Indicator Bank: Search text changed
        if (id == IDC_IND_SEARCH && note == EN_CHANGE) {
            RefreshIndicatorList();
            return 0;
        }

        // Indicator Bank: Selection changed
        if (id == IDC_IND_LIST && (note == LBN_SELCHANGE || note == LBN_DBLCLK)) {
            ShowIndicatorDetail();
            return 0;
        }

        // Indicator Bank: Load Example into Builder
        if (id == IDC_IND_EX_TRY) {
            std::wstring exClue = GetWindowTextW_s(g_hIndExClue);
            if (!exClue.empty() && exClue[0] != L'(') {
                SendMessageW(g_hTab, TCM_SETCURSEL, TAB_BUILDER, 0);
                g_activeTab = TAB_BUILDER;
                ShowTab(TAB_BUILDER);
                LayoutControls(hWnd);

                std::wstring explain = GetWindowTextW_s(g_hIndExExplain);
                size_t eq = explain.find(L'=');
                if (eq != std::wstring::npos) {
                    std::wstring ansPart = Trim(explain.substr(eq + 1));
                    size_t sp = ansPart.find_first_of(L" (\"[");
                    if (sp != std::wstring::npos) ansPart = ansPart.substr(0, sp);
                    SetWindowTextW(g_hAnswer, ansPart.c_str());
                }

                std::wstring clueOnly = exClue;
                size_t p = clueOnly.rfind(L'(');
                if (p != std::wstring::npos) clueOnly = Trim(clueOnly.substr(0, p));
                SetWindowTextW(g_hClueEdit, clueOnly.c_str());

                UpdateCountLabel();
                RebuildChips(clueOnly);
                InvalidateRect(g_hChipWnd, nullptr, TRUE);
                SetStatus(L"Loaded indicator example clue into builder.");
            }
            return 0;
        }

        // Indicator Bank: Add Indicator
        if (id == IDC_IND_ADD_BTN) {
            std::wstring word = ToLower(Trim(GetWindowTextW_s(g_hIndAddEdit)));
            if (word.empty()) {
                MessageBoxW(hWnd, L"Please enter an indicator word to add.", L"CrypTools", MB_OK);
                return 0;
            }
            int catSel = (int)SendMessageW(g_hIndAddCat, CB_GETCURSEL, 0, 0);
            static const wchar_t* cats[] = { L"anagram", L"reversal", L"hidden", L"container", L"homophone", L"deletion", L"acrostic" };
            std::wstring cat = (catSel >= 0 && catSel < 7) ? cats[catSel] : L"anagram";

            Indicator ind;
            ind.word = word;
            ind.category = cat;
            g_indicators.push_back(ind);
            SaveIndicators();
            RefreshIndicatorList();
            SetWindowTextW(g_hIndAddEdit, L"");
            return 0;
        }

        // Indicator Bank: Remove Selected
        if (id == IDC_IND_REMOVE_BTN) {
            int sel = (int)SendMessageW(g_hIndList, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                int origIdx = (int)SendMessageW(g_hIndList, LB_GETITEMDATA, sel, 0);
                if (origIdx >= 0 && origIdx < (int)g_indicators.size()) {
                    g_indicators.erase(g_indicators.begin() + origIdx);
                    SaveIndicators();
                    RefreshIndicatorList();
                }
            }
            return 0;
        }

        // Substitutions: Live scan as user types
        if (id == IDC_SUB_INPUT && note == EN_CHANGE) {
            ScanSubstitutionsInPhrase();
            return 0;
        }

        // Substitutions: Use Current Clue
        if (id == IDC_SUB_USE_CLUE_BTN) {
            std::wstring c = GetWindowTextW_s(g_hClueEdit);
            SetWindowTextW(g_hSubInput, c.c_str());
            ScanSubstitutionsInPhrase();
            return 0;
        }

        // Substitutions: Direction Switch (Word -> Abbr vs Abbr -> Word)
        if (id == IDC_SUB_DIR_BTN) {
            g_subDirectionReverse = !g_subDirectionReverse;
            SetWindowTextW(g_hSubDirBtn, g_subDirectionReverse ? L"Direction: Abbr \u2192 Word" : L"Direction: Word \u2192 Abbr");
            if (g_hSubFromLbl) SetWindowTextW(g_hSubFromLbl, g_subDirectionReverse ? L"Abbr:" : L"Word:");
            if (g_hSubToLbl)   SetWindowTextW(g_hSubToLbl,   g_subDirectionReverse ? L"Word:" : L"Abbr:");
            if (g_hCfgDefDir)  SendMessageW(g_hCfgDefDir, CB_SETCURSEL, g_subDirectionReverse ? 1 : 0, 0);
            ScanSubstitutionsInPhrase();
            SaveSettings();
            return 0;
        }

        // Substitutions: Add Pair
        if (id == IDC_SUB_ADD_BTN) {
            std::wstring fText = Trim(GetWindowTextW_s(g_hSubFrom));
            std::wstring tText = Trim(GetWindowTextW_s(g_hSubTo));
            if (fText.empty() || tText.empty()) {
                MessageBoxW(hWnd, L"Please enter both fields before adding to bank.", L"CrypTools", MB_OK);
                return 0;
            }
            if (!g_subDirectionReverse) {
                g_substitutions.push_back({ ToLower(fText), ToUpper(tText) });
            } else {
                g_substitutions.push_back({ ToLower(tText), ToUpper(fText) });
            }
            SaveSubstitutions();
            ScanSubstitutionsInPhrase();
            SetWindowTextW(g_hSubFrom, L"");
            SetWindowTextW(g_hSubTo, L"");
            return 0;
        }

        // Substitutions: Remove Selected
        if (id == IDC_SUB_REMOVE_BTN) {
            int sel = (int)SendMessageW(g_hSubMatchesList, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                int origIdx = (int)SendMessageW(g_hSubMatchesList, LB_GETITEMDATA, sel, 0);
                if (origIdx >= 0 && origIdx < (int)g_substitutions.size()) {
                    g_substitutions.erase(g_substitutions.begin() + origIdx);
                    SaveSubstitutions();
                    ScanSubstitutionsInPhrase();
                }
            }
            return 0;
        }

        // Tools: Sub-mode switcher
        if (id == IDC_TOOL_TAB_ANA) {
            ShowToolView(TOOL_ANAGRAM);
            LayoutControls(hWnd);
            return 0;
        }
        if (id == IDC_TOOL_TAB_HID) {
            ShowToolView(TOOL_HIDDEN);
            LayoutControls(hWnd);
            return 0;
        }
        if (id == IDC_TOOL_TAB_DIFF) {
            ShowToolView(TOOL_DIFF);
            LayoutControls(hWnd);
            return 0;
        }

        // Tools: Anagram Live Update on typing
        if (id == IDC_TOOL_ANA_INPUT && note == EN_CHANGE) {
            ComputeAnagrams();
            return 0;
        }

        // Tools: Anagram Use Fodder
        if (id == IDC_TOOL_ANA_USE_FODDER) {
            std::wstring fod;
            for (auto& c : g_chips) {
                if (c.label == LABEL_FODDER) {
                    if (!fod.empty()) fod += L" ";
                    fod += c.text;
                }
            }
            if (!fod.empty()) {
                SetWindowTextW(g_hToolAnaInput, StripPunct(fod).c_str());
            } else {
                SetWindowTextW(g_hToolAnaInput, StripPunct(GetWindowTextW_s(g_hClueEdit)).c_str());
            }
            ComputeAnagrams();
            return 0;
        }

        // Tools: Anagram Use Answer
        if (id == IDC_TOOL_ANA_USE_ANS) {
            SetWindowTextW(g_hToolAnaInput, StripPunct(GetWindowTextW_s(g_hAnswer)).c_str());
            ComputeAnagrams();
            return 0;
        }

        // Tools: Anagram Set as Answer
        if (id == IDC_TOOL_ANA_SET_ANS || (id == IDC_TOOL_ANA_LIST && note == LBN_DBLCLK)) {
            int sel = (int)SendMessageW(g_hToolAnaList, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                wchar_t buf[256];
                SendMessageW(g_hToolAnaList, LB_GETTEXT, sel, (LPARAM)buf);
                std::wstring picked = StripPunct(std::wstring(buf));
                size_t sp = picked.find(L' ');
                if (sp != std::wstring::npos) picked = picked.substr(0, sp);

                SetWindowTextW(g_hAnswer, picked.c_str());
                UpdateCountLabel();

                SendMessageW(g_hTab, TCM_SETCURSEL, TAB_BUILDER, 0);
                g_activeTab = TAB_BUILDER;
                ShowTab(TAB_BUILDER);
                LayoutControls(hWnd);
                SetStatus(L"Selected anagram set as Answer in Clue Builder.");
            }
            return 0;
        }

        // Tools: Hidden Word Live Update on typing
        if (id == IDC_TOOL_HID_PHRASE && note == EN_CHANGE) {
            ComputeHiddenWords();
            return 0;
        }

        // Tools: Hidden Word Use Current Clue
        if (id == IDC_TOOL_HID_USE_CLUE) {
            SetWindowTextW(g_hToolHidPhrase, GetWindowTextW_s(g_hClueEdit).c_str());
            ComputeHiddenWords();
            return 0;
        }

        // Tools: Hidden Word options changed
        if ((id == IDC_TOOL_HID_MINLEN || id == IDC_TOOL_HID_SCOPE || id == IDC_TOOL_HID_REV) && note == CBN_SELCHANGE) {
            ComputeHiddenWords();
            return 0;
        }

        // Tools: Hidden Word Set as Answer
        if (id == IDC_TOOL_HID_SET_ANS || (id == IDC_TOOL_HID_LIST && note == LBN_DBLCLK)) {
            int sel = (int)SendMessageW(g_hToolHidList, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                wchar_t buf[512];
                SendMessageW(g_hToolHidList, LB_GETTEXT, sel, (LPARAM)buf);
                std::wstring line(buf);
                size_t sp = line.find(L' ');
                if (sp != std::wstring::npos) {
                    std::wstring word = Trim(line.substr(0, sp));
                    SetWindowTextW(g_hAnswer, word.c_str());
                    UpdateCountLabel();

                    SendMessageW(g_hTab, TCM_SETCURSEL, TAB_BUILDER, 0);
                    g_activeTab = TAB_BUILDER;
                    ShowTab(TAB_BUILDER);
                    LayoutControls(hWnd);
                    SetStatus(L"Selected hidden word set as Answer in Clue Builder.");
                }
            }
            return 0;
        }

        // Tools: Addition & Removal Live Update on typing
        if ((id == IDC_TOOL_DIFF_MAIN || id == IDC_TOOL_DIFF_GOAL) && note == EN_CHANGE) {
            ComputeLetterDiff();
            return 0;
        }

        // Tools: Addition & Removal Swap
        if (id == IDC_TOOL_DIFF_SWAP) {
            std::wstring m = GetWindowTextW_s(g_hToolDiffMain);
            std::wstring g = GetWindowTextW_s(g_hToolDiffGoal);
            SetWindowTextW(g_hToolDiffMain, g.c_str());
            SetWindowTextW(g_hToolDiffGoal, m.c_str());
            ComputeLetterDiff();
            return 0;
        }

        // Tools: Addition & Removal Pull from Clue & Answer
        if (id == IDC_TOOL_DIFF_USE_CLUE) {
            std::wstring ans = StripPunct(GetWindowTextW_s(g_hAnswer));
            std::wstring fod;
            for (auto& c : g_chips) {
                if (c.label == LABEL_FODDER) {
                    if (!fod.empty()) fod += L" ";
                    fod += c.text;
                }
            }
            if (fod.empty()) fod = GetWindowTextW_s(g_hClueEdit);
            SetWindowTextW(g_hToolDiffMain, StripPunct(fod).c_str());
            SetWindowTextW(g_hToolDiffGoal, ans.c_str());
            ComputeLetterDiff();
            return 0;
        }

        // Tools: Deletion list item clicked -> set as Goal Word
        if (id == IDC_TOOL_DIFF_DEL_LIST && (note == LBN_SELCHANGE || note == LBN_DBLCLK)) {
            int sel = (int)SendMessageW(g_hToolDiffDelList, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                wchar_t buf[256];
                SendMessageW(g_hToolDiffDelList, LB_GETTEXT, sel, (LPARAM)buf);
                std::wstring line(buf);
                size_t sp = line.find(L' ');
                if (sp != std::wstring::npos) {
                    std::wstring w = Trim(line.substr(0, sp));
                    SetWindowTextW(g_hToolDiffGoal, w.c_str());
                    ComputeLetterDiff();
                }
            }
            return 0;
        }

        // Tools: Addition list item clicked -> set as Goal Word
        if (id == IDC_TOOL_DIFF_ADD_LIST && (note == LBN_SELCHANGE || note == LBN_DBLCLK)) {
            int sel = (int)SendMessageW(g_hToolDiffAddList, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                wchar_t buf[256];
                SendMessageW(g_hToolDiffAddList, LB_GETTEXT, sel, (LPARAM)buf);
                std::wstring line(buf);
                size_t sp = line.find(L' ');
                if (sp != std::wstring::npos) {
                    std::wstring w = Trim(line.substr(0, sp));
                    SetWindowTextW(g_hToolDiffGoal, w.c_str());
                    ComputeLetterDiff();
                }
            }
            return 0;
        }

        // Database: Search text changed
        if (id == IDC_DB_SEARCH && note == EN_CHANGE) {
            RefreshDatabaseList();
            return 0;
        }

        // Database: Selection changed
        if (id == IDC_DB_LIST && (note == LBN_SELCHANGE || note == LBN_DBLCLK)) {
            int sel = (int)SendMessageW(g_hDbList, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                int origIdx = (int)SendMessageW(g_hDbList, LB_GETITEMDATA, sel, 0);
                if (origIdx >= 0 && origIdx < (int)g_clues.size()) {
                    auto& c = g_clues[origIdx];
                    wchar_t buf[2048];
                    swprintf_s(buf,
                        L"ANSWER:      %s\r\n"
                        L"CLUE:        %s\r\n"
                        L"ENUMERATION: %s\r\n"
                        L"LABELS:      %s\r\n"
                        L"NOTES:       %s",
                        c.answer.c_str(), c.clue.c_str(),
                        c.enumeration.c_str(), c.labels.c_str(), c.notes.c_str());
                    SetWindowTextW(g_hDbDetail, buf);
                }
            }
            return 0;
        }

        // Database: Delete Entry
        if (id == IDC_DB_DELETE_BTN) {
            int sel = (int)SendMessageW(g_hDbList, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                int origIdx = (int)SendMessageW(g_hDbList, LB_GETITEMDATA, sel, 0);
                if (origIdx >= 0 && origIdx < (int)g_clues.size()) {
                    if (MessageBoxW(hWnd, L"Are you sure you want to delete this clue from the database?", L"CrypTools",
                        MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        g_clues.erase(g_clues.begin() + origIdx);
                        SaveClues();
                        RefreshDatabaseList();
                        SetWindowTextW(g_hDbDetail, L"");
                    }
                }
            }
            return 0;
        }

        // Database: Copy Clue Text
        if (id == IDC_DB_COPY_BTN) {
            int sel = (int)SendMessageW(g_hDbList, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                int origIdx = (int)SendMessageW(g_hDbList, LB_GETITEMDATA, sel, 0);
                if (origIdx >= 0 && origIdx < (int)g_clues.size()) {
                    std::wstring text = g_clues[origIdx].clue;
                    if (OpenClipboard(hWnd)) {
                        EmptyClipboard();
                        size_t bytes = (text.size() + 1) * sizeof(wchar_t);
                        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
                        if (hMem) {
                            void* ptr = GlobalLock(hMem);
                            if (ptr) { memcpy(ptr, text.c_str(), bytes); GlobalUnlock(hMem); }
                            SetClipboardData(CF_UNICODETEXT, hMem);
                        }
                        CloseClipboard();
                        SetStatus(L"Clue text copied to clipboard.");
                    }
                }
            }
            return 0;
        }

        // Database: Load into Builder
        if (id == IDC_DB_LOAD_BTN) {
            int sel = (int)SendMessageW(g_hDbList, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                int origIdx = (int)SendMessageW(g_hDbList, LB_GETITEMDATA, sel, 0);
                if (origIdx >= 0 && origIdx < (int)g_clues.size()) {
                    auto& c = g_clues[origIdx];
                    SendMessageW(g_hTab, TCM_SETCURSEL, TAB_BUILDER, 0);
                    g_activeTab = TAB_BUILDER;
                    ShowTab(TAB_BUILDER);
                    LayoutControls(hWnd);

                    SetWindowTextW(g_hAnswer, c.answer.c_str());

                    std::wstring clueOnly = c.clue;
                    size_t p = clueOnly.rfind(L'(');
                    if (p != std::wstring::npos) clueOnly = Trim(clueOnly.substr(0, p));
                    SetWindowTextW(g_hClueEdit, clueOnly.c_str());

                    UpdateCountLabel();
                    RebuildChips(clueOnly);

                    // Restore serialized labels
                    auto pairs = Split(c.labels, L' ');
                    for (auto& pr : pairs) {
                        size_t col = pr.find(L':');
                        if (col != std::wstring::npos) {
                            int chipIdx = _wtoi(pr.substr(0, col).c_str());
                            std::wstring tagVal = pr.substr(col + 1);
                            std::wstring subVal;
                            size_t b1 = tagVal.find(L'[');
                            size_t b2 = tagVal.find(L']');
                            if (b1 != std::wstring::npos && b2 != std::wstring::npos) {
                                subVal = tagVal.substr(b1 + 1, b2 - b1 - 1);
                                tagVal = tagVal.substr(0, b1);
                            }

                            if (chipIdx >= 0 && chipIdx < (int)g_chips.size()) {
                                static const wchar_t* kNames[] = { L"none", L"definition", L"fodder", L"indicator", L"substitution", L"connector", L"doubledef2", L"other_fodder" };
                                for (int k = 0; k < LABEL_COUNT; ++k) {
                                    if (tagVal == kNames[k]) {
                                        g_chips[chipIdx].label = (ChipLabel)k;
                                        if ((ChipLabel)k == LABEL_INDICATOR) {
                                            size_t eqPos = subVal.find(L'=');
                                            if (eqPos != std::wstring::npos) {
                                                g_chips[chipIdx].indType = subVal.substr(0, eqPos);
                                                g_chips[chipIdx].subText = subVal.substr(eqPos + 1);
                                            } else {
                                                g_chips[chipIdx].indType = subVal;
                                                g_chips[chipIdx].subText.clear();
                                            }
                                        } else if ((ChipLabel)k == LABEL_SUBSTITUTION || (ChipLabel)k == LABEL_OTHER_FODDER) {
                                            g_chips[chipIdx].subText = subVal;
                                            g_chips[chipIdx].indType.clear();
                                        } else {
                                            g_chips[chipIdx].subText.clear();
                                            g_chips[chipIdx].indType.clear();
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    InvalidateRect(g_hChipWnd, nullptr, TRUE);
                    SetStatus(L"Loaded clue from database into builder.");
                }
            }
            return 0;
        }

        // Config: Ignored character checkboxes
        if ((id == IDC_CFG_IGNORE_WS || id == IDC_CFG_IGNORE_NUMS ||
             id == IDC_CFG_IGNORE_PUNCT || id == IDC_CFG_IGNORE_SPECIAL) && note == BN_CLICKED) {
            g_cfgIgnoreWs      = (SendMessageW(g_hCfgIgnoreWs, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_cfgIgnoreNums    = (SendMessageW(g_hCfgIgnoreNums, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_cfgIgnorePunct   = (SendMessageW(g_hCfgIgnorePunct, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_cfgIgnoreSpecial = (SendMessageW(g_hCfgIgnoreSpecial, BM_GETCHECK, 0, 0) == BST_CHECKED);
            RefreshIgnoredDisplay();
            SaveSettings();
            LoadDictionary();
            ComputeAnagrams();
            ComputeHiddenWords();
            return 0;
        }

        // Config: Browse Dictionaries Folder
        if (id == IDC_CFG_DICT_BROWSE) {
            std::wstring folder = BrowseFolder(hWnd, L"Select Dictionaries Folder (containing *.dict files)");
            if (!folder.empty()) {
                g_cfgDictDir = folder;
                SetWindowTextW(g_hCfgDictDir, folder.c_str());
                SaveSettings();
                LoadDictionary();
                ComputeAnagrams();
                ComputeHiddenWords();
            }
            return 0;
        }

        // Config: Reload Dictionaries
        if (id == IDC_CFG_DICT_RELOAD) {
            wchar_t db[MAX_PATH] = { 0 };
            GetWindowTextW(g_hCfgDictDir, db, MAX_PATH);
            g_cfgDictDir = db;
            SaveSettings();
            LoadDictionary();
            ComputeAnagrams();
            ComputeHiddenWords();
            return 0;
        }

        // Config: Hidden Word Scope Changed
        if (id == IDC_CFG_DEF_SCOPE && note == CBN_SELCHANGE) {
            int s = (int)SendMessageW(g_hCfgDefScope, CB_GETCURSEL, 0, 0);
            if (s >= 0) {
                g_cfgHiddenScope = s;
                if (g_hToolHidScope) SendMessageW(g_hToolHidScope, CB_SETCURSEL, s, 0);
                SaveSettings();
                ComputeHiddenWords();
            }
            return 0;
        }

        // Config: Hidden Word Min Len Changed
        if (id == IDC_CFG_DEF_MINLEN && note == CBN_SELCHANGE) {
            int sel = (int)SendMessageW(g_hCfgDefMinLen, CB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                g_cfgHiddenMinLen = 3 + sel;
                if (g_hToolHidMinLen) SendMessageW(g_hToolHidMinLen, CB_SETCURSEL, sel, 0);
                SaveSettings();
                ComputeHiddenWords();
            }
            return 0;
        }

        // Config: Substitutions Direction Changed
        if (id == IDC_CFG_DEF_DIR && note == CBN_SELCHANGE) {
            int d = (int)SendMessageW(g_hCfgDefDir, CB_GETCURSEL, 0, 0);
            g_subDirectionReverse = (d == 1);
            if (g_hSubDirBtn) {
                SetWindowTextW(g_hSubDirBtn, g_subDirectionReverse ? L"Direction: Abbr \u2192 Word" : L"Direction: Word \u2192 Abbr");
            }
            if (g_hSubFromLbl) SetWindowTextW(g_hSubFromLbl, g_subDirectionReverse ? L"Abbr:" : L"Word:");
            if (g_hSubToLbl)   SetWindowTextW(g_hSubToLbl,   g_subDirectionReverse ? L"Word:" : L"Abbr:");
            SaveSettings();
            ScanSubstitutionsInPhrase();
            return 0;
        }

        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    case WM_NOTIFY: {
        NMHDR* pnm = (NMHDR*)lParam;
        if (pnm && (pnm->idFrom == IDC_TAB || pnm->hwndFrom == g_hTab) && pnm->code == (UINT)TCN_SELCHANGE) {
            int newTab = (int)SendMessageW(g_hTab, TCM_GETCURSEL, 0, 0);
            if (newTab >= 0 && newTab <= 5) {
                StopAnimation();
                g_activeTab = newTab;
                ShowTab(newTab);
                LayoutControls(hWnd);

                if (g_activeTab == TAB_SUBS) {
                    if (GetWindowTextLengthW(g_hSubInput) == 0) {
                        SetWindowTextW(g_hSubInput, GetWindowTextW_s(g_hClueEdit).c_str());
                    }
                    ScanSubstitutionsInPhrase();
                }
            }
        }
        return 0;
    }

    case WM_CLOSE: {
        SaveSettings();
        DestroyWindow(hWnd);
        return 0;
    }

    case WM_DESTROY: {
        DeleteObject(g_hFont);
        DeleteObject(g_hBoldFont);
        DeleteObject(g_hInputFont);
        DeleteObject(g_hChipFont);
        DeleteObject(g_hBadgeFont);
        DeleteObject(g_hAnimLargeFont);
        PostQuitMessage(0);
        return 0;
    }

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// WinMain Entry Point
// ---------------------------------------------------------------------------

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    g_hInst = hInst;

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);

    // Register Chip Canvas child window class
    WNDCLASSEXW wcChip = {};
    wcChip.cbSize        = sizeof(wcChip);
    wcChip.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcChip.lpfnWndProc   = ChipWndProc;
    wcChip.hInstance     = hInst;
    wcChip.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wcChip.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcChip.lpszClassName = CHIP_CLASS;
    RegisterClassExW(&wcChip);

    // Register Main Application window class
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = APP_CLASS;
    wc.hIcon         = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                         GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    wc.hIconSm       = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                         GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    RegisterClassExW(&wc);

    // Create Fonts
    g_hFont = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
    g_hBoldFont = CreateFontW(
        -13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
    g_hInputFont = CreateFontW(
        -18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
    g_hChipFont = CreateFontW(
        -25, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
    g_hBadgeFont = CreateFontW(
        -11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
    g_hAnimLargeFont = CreateFontW(
        -22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );

    HWND hWnd = CreateWindowExW(
        0, APP_CLASS, APP_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 940, 740,
        nullptr, nullptr, hInst, nullptr
    );
    if (!hWnd) return 1;

    SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)wc.hIcon);
    SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)wc.hIconSm);

    g_hWnd = hWnd;
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hWnd, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    return (int)m.wParam;
}
