// CrossTools.cpp (v1.2) - crossword helper. The word bar stays at the top; tabs sit below it.
//   Letter count:    counts characters, skipping ignored ones (whitespace / punctuation, see Config).
//                    "Blank" characters (default _) always count. Also reports which dictionary
//                    files (*.dict in the dictionaries folder) contain the word.
//   Fill the blank:  C_T  ->  CAT, CBT, CCT ... CZT   (spaces and punctuation are kept as typed)
//                    Any number of blanks; # = any vowel and @ = any consonant (Config). With 2+ gaps the dictionary
//                    words that fit are listed first, best score first.
//                    A phrase (COP C_R) also gets yellow "potential" words: each word is in a dictionary even if the
//                    whole phrase is not. The Show drop-down picks green, yellow, both, or everything.
//                    The two icon buttons beside Max rows shuffle the rows into a random selection and sort them A-Z / Z-A.
//                    Split tries adding 1, 2 and 3 spaces in every place and leaves the word bar split where the phrase
//                    has the most potential words (every word it makes has at least the Config minimum length, 3).
//   Random:          a random word from a random dictionary. The word bar's pattern (C__T) a length and a score narrow it.
//   Dictionaries:    rescales or clamps the scores in chosen .dict files (e.g. max 100 -> max 50, rounded to 5), merges
//                    dictionaries (into the default one or a new one), and dumps the words added from Letter count / Fill
//                    (CrossTools.dict) into the default dictionary. Each changed original is kept as "name.dict (old)".
//   Config:          blank characters, what to ignore (checkbox groups), dictionaries folder,
//                    and the default dictionary that "Add to ..." appends to.
//
//   Settings:        saved in the registry (HKEY_CURRENT_USER\Software\CrossTools) when you leave the Config tab and when the window closes.
//
// .dict format: UTF-8, one "entry;score" per line. Entries may contain spaces/punctuation/capitals.
// A word matches an entry when they are equal after upper-casing and dropping ignored characters,
// so ICEAGE, "ice age" and "Ice-Age" all find the entry "ice age".
//
// Plain Win32, no dependencies, classic Windows look. See build.bat.
#ifndef NOMINMAX
#define NOMINMAX                  // keep windows.h from turning std::min / std::max into macros (breaks cl)
#endif
#include <windows.h>
#include <commctrl.h>
#include <process.h>
#include <algorithm>
#include <atomic>
#include <climits>
#include <queue>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <unordered_map>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

enum { IDC_INPUT = 1, IDC_TAB, IDC_LIST, IDC_STATUS, IDC_COUNT, IDC_NOTE, IDC_RESULT,
       IDC_LBL_BLANKS, IDC_BLANKS, IDC_LBL_IGNORED, IDC_IGNORED, IDC_WS, IDC_CFG_NOTE,
       IDC_LBL_DICT, IDC_DICTDIR, IDC_RELOAD, IDC_DICT_STATUS,
       IDC_LBL_OLD, IDC_OLDMAX, IDC_LBL_NEW, IDC_NEWMAX, IDC_LBL_ROUND, IDC_ROUNDTO, IDC_INFO,
       IDC_DICTLIST, IDC_APPLY, IDC_LOG,
       IDC_NUMS, IDC_PUNCT, IDC_SPECIAL, IDC_LBL_ADD, IDC_ADDDICT, IDC_ADDSCORE, IDC_ADD,
       IDC_LBL_CLAMP, IDC_CLAMPMAX, IDC_MAKEDEF, IDC_SHOWSCORE, IDC_LBL_SCORE, IDC_SCORE,
       IDC_LBL_SHOW, IDC_FILLMODE, IDC_LBL_CAP, IDC_FILLCAP, IDC_LBL_VOWEL, IDC_VOWELS, IDC_LBL_CONS, IDC_CONS,
       IDC_RANDOM, IDC_LBL_RANDLEN, IDC_RANDLEN, IDC_RANDWORD, IDC_RANDFROM, IDC_RANDSTATUS, IDC_LBL_RANDSCORE, IDC_RANDSCORE, IDC_LBL_SEP, IDC_SEP,
       IDC_FILLSHUF, IDC_FILLSORT, IDC_DUMP, IDC_MERGE, IDC_FILLSPLIT, IDC_FILLSCORE, IDC_LBL_MINSPLIT, IDC_MINSPLIT };
enum { TAB_COUNT = 0, TAB_FILL = 1, TAB_RANDOM = 2, TAB_SCORES = 3, TAB_CONFIG = 4 };
#define IDI_APP 1
#define WM_APP_LOADED (WM_APP + 1)   // background load finished (lParam = LoadResult*)
#define WM_APP_FILLDBL (WM_APP + 2) // double-click on a Fill row (wParam = row)
#define IDT_RELOAD 1                // debounce timer for settings edits

// Character groups the Config checkboxes switch on and off (together they cover all ASCII punctuation).
static const wchar_t* kWhitespace = L" \t\u00A0\u3000";
static const wchar_t* kNumbers = L"0123456789";
static const wchar_t* kPunct = L".,;:!?'\"-()[]{}/\u201C\u201D\u2018\u2019\u2013\u2014\u2026";
static const wchar_t* kSpecial = L"@#$%^&*+=<>|~\\_`";
static const wchar_t* kDefaultDictDir = L"dictionaries";
static const wchar_t* kDefaultAddDict = L"CrossTools.dict";

static HWND hInput, hTab, hList, hStatus, hCount, hNote, hResult, hAdd, hLblScore, hScore;
static HWND hLblBlanks, hBlanks, hLblIgnored, hIgnored, hWs, hNums, hPunct, hSpecial, hCfgNote, hShowScore;
static HWND hLblDict, hDictDir, hReload, hDictStatus, hLblAdd, hAddDict, hAddScore;
static HWND hLblOld, hOldMax, hLblNew, hNewMax, hLblRound, hRound, hLblClamp, hClampMax, hInfo, hTip;
static HWND hDictList, hApply, hMakeDef, hLog;
static HWND hLblShow, hFillMode, hLblCap, hFillCap, hLblVowel, hVowels, hLblCons, hConsonants;
static HWND hLblSep, hSepEdit;
static HWND hLblMinSplit, hMinSplit;
static HWND hFillShuf, hFillSort, hFillSplit;   // Fill: icon buttons (shuffle, sort A-Z / Z-A) and the Split button
static HBITMAP hBmpShuf, hBmpAZ, hBmpZA;
static HWND hDump, hMerge, hFillScore;
static HWND hRandWord, hRandFrom, hRandStatus, hLblRandLen, hRandLen, hLblRandScore, hRandScore, hRandBtn;
static HFONT hFont, hBigFont, hSmallFont, hRandFont, hBoldGui;
static COLORREF g_resultColor = RGB(128, 128, 128);   // colour of the dictionary result line
static int g_scoreMode = 0;            // score box on Letter count: 0 hidden, 1 score for a new word, 2 change a found word's score
static std::wstring g_scoreKey;        // what the score box currently describes (it is only refilled when this changes)
static std::wstring g_foundKey;        // normalized form of the found word (for changing its score)
static bool g_canAdd = false;          // the word could be added to the default dictionary
static bool g_fillByScore = false;     // Fill: potential words best total score first (the Sort by score button)
static bool g_fillShuffled = false;    // the Fill rows are a random selection (shuffle button)
static uint64_t g_fillSeed = 0;        // which random order (a new one per click)
static int g_fillSort = 0;             // sort button: 0 natural order, 1 strictly A-Z, 2 strictly Z-A
static std::wstring g_defaultDict;     // the default dictionary (Dictionaries tab): where merges and the dump go
static int g_colW = 100;              // Fill list column width
static WNDPROC g_listProc;             // original LISTBOX procedure (we subclass it for double-clicks)
static int g_itemH = 30;               // Fill list row height
static WNDPROC g_editProc;             // original EDIT window procedure (we subclass it for shortcuts)

// ---- character rules -------------------------------------------------------------------------
struct Cfg {
    std::wstring P;                                           // blank characters: any letter (upper-cased)
    std::wstring V;                                           // wildcard characters: any vowel
    std::wstring K;                                           // wildcard characters: any consonant
    std::wstring sig;                                         // identifies these rules: blanks|vowels|consonants|groups
    std::vector<unsigned char> ign = std::vector<unsigned char>(65536, 0);
    int Gap(wchar_t c) const {                                // 0 ordinary, 1 any letter, 2 vowel, 3 consonant
        if (P.find(c) != std::wstring::npos) return 1;
        if (V.find(c) != std::wstring::npos) return 2;
        if (K.find(c) != std::wstring::npos) return 3;
        return 0;
    }
    bool Ignored(wchar_t c) const { return ign[(unsigned short)c] != 0; }
};

// Upper-case one character (same mapping as CharUpperBuffW, with an ASCII fast path).
static inline wchar_t Up(wchar_t c) {
    if (c < 128) return (c >= L'a' && c <= L'z') ? (wchar_t)(c - 32) : c;
    return (wchar_t)(UINT_PTR)CharUpperW((LPWSTR)(UINT_PTR)c);
}

// ---- dictionaries ----------------------------------------------------------------------------
// A dictionary is a sorted array of 64-bit fingerprints of its normalized entries (upper-case, ignored
// characters dropped). No per-entry allocation, so loading takes tens of ms, and a lookup is a binary
// search. A false match needs a 64-bit collision (about 1 in 10^14 per lookup).
static const uint64_t kFnvBasis = 14695981039346656037ULL, kFnvPrime = 1099511628211ULL;
static inline uint64_t Mix(uint64_t h) {
    h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL; h ^= h >> 33;
    return h;
}
static uint64_t HashChars(const wchar_t* s, size_t n) {
    uint64_t h = kFnvBasis;
    for (size_t i = 0; i < n; ++i) h = (h ^ (unsigned short)s[i]) * kFnvPrime;
    return Mix(h);
}
static uint64_t HashKey(const std::wstring& key) { return HashChars(key.data(), key.size()); }

// Fingerprint of an entry's normalized form (upper-cased, ignored characters dropped). False if nothing is left.
static inline bool EntryHash(const wchar_t* s, size_t n, const Cfg& cfg, uint64_t& out) {
    uint64_t h = kFnvBasis;
    bool any = false;
    for (size_t i = 0; i < n; ++i) {
        wchar_t c = Up(s[i]);
        if (cfg.Ignored(c)) continue;
        h = (h ^ (unsigned short)c) * kFnvPrime;
        any = true;
    }
    out = Mix(h);
    return any;
}

static const size_t kMaxKey = 40;      // longest word the word bar takes; longer entries can't match a pattern

// An entry's normalized form: upper-cased, ignored characters dropped.
static inline void NormEntry(const wchar_t* s, size_t n, const Cfg& cfg, std::wstring& out) {
    out.clear();
    for (size_t i = 0; i < n; ++i) {
        wchar_t c = Up(s[i]);
        if (!cfg.Ignored(c)) out += c;
    }
}

struct Dict {
    std::wstring name, path;
    std::vector<uint64_t> hashes;
    std::vector<int> scores;          // score of each entry in hashes (same order); -1 = none
    struct Bucket { std::vector<wchar_t> chars; std::vector<int> scores; std::vector<unsigned char> dup; };   // normalized entries of one length
    std::vector<Bucket> byLen;        // index = length: lets the Fill tab find words that fit a pattern
    long long maxScore = 0;           // highest score in the file
};

static Cfg g_cfg;                      // current rules; rebuilt only when they change
static std::vector<Dict> g_dicts;      // UI thread only; replaced when a background load lands
static std::wstring g_loadedSig;       // rules the loaded fingerprints were built with
static std::wstring g_requestedDir;    // folder setting of the latest reload request
static std::vector<std::wstring> g_untick;   // dictionaries to untick when the next load lands
static bool g_loading = false;
static bool g_ready = false;           // false while the window is still being built
static HWND g_hwnd;
static std::atomic<int> g_gen{0};      // bumped per reload request; older background loads give up

static std::wstring ExeDir() {
    wchar_t p[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::wstring s(p, n);
    size_t k = s.find_last_of(L"\\/");
    return k == std::wstring::npos ? L"." : s.substr(0, k);
}

// Relative folder settings are relative to the exe (not the current directory).
static std::wstring ResolveFolder(std::wstring s) {
    size_t a = s.find_first_not_of(L" \t"), b = s.find_last_not_of(L" \t");
    s = (a == std::wstring::npos) ? std::wstring(kDefaultDictDir) : s.substr(a, b - a + 1);
    while (s.size() > 1 && (s.back() == L'\\' || s.back() == L'/')) s.pop_back();
    bool absolute = (s.size() >= 2 && s[1] == L':') || s[0] == L'\\' || s[0] == L'/';
    return absolute ? s : ExeDir() + L"\\" + s;
}

static bool ReadBytes(const std::wstring& path, std::string& bytes) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    bytes.clear();
    LARGE_INTEGER sz;
    if (GetFileSizeEx(f, &sz) && sz.QuadPart < (1LL << 30)) {
        bytes.resize((size_t)sz.QuadPart);
        DWORD got = 0;
        if (!ReadFile(f, bytes.empty() ? nullptr : &bytes[0], (DWORD)bytes.size(), &got, nullptr)) got = 0;
        bytes.resize(got);
    }
    CloseHandle(f);
    return true;
}

static bool ReadText(const std::wstring& path, std::wstring& out) {
    std::string bytes;
    if (!ReadBytes(path, bytes)) return false;

    size_t skip = (bytes.size() >= 3 && (unsigned char)bytes[0] == 0xEF &&
                   (unsigned char)bytes[1] == 0xBB && (unsigned char)bytes[2] == 0xBF) ? 3 : 0;
    const char* p = bytes.data() + skip;
    int n = (int)(bytes.size() - skip);
    out.clear();
    if (n <= 0) return true;

    UINT cp = CP_UTF8;                                        // fall back to the ANSI code page
    int wn = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, p, n, nullptr, 0);
    if (wn == 0) { cp = CP_ACP; wn = MultiByteToWideChar(cp, 0, p, n, nullptr, 0); }
    if (wn <= 0) return true;
    out.resize(wn);
    MultiByteToWideChar(cp, cp == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0, p, n, &out[0], wn);
    return true;
}

struct LoadJob { int gen; std::wstring dirSetting; Cfg cfg; };
struct LoadResult { int gen; std::wstring sig, status; std::vector<Dict> dicts; };

static bool Stale(int gen) { return g_gen.load() != gen; }
enum { LOAD_OK, LOAD_UNREADABLE, LOAD_STALE };

// Whole-number score in [a, b) (spaces/tabs trimmed, at most 9 digits), or -1 if there isn't one.
template <class C> static int ParseScore(const C* s, size_t a, size_t b) {
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
    if (b == a || b - a > 9) return -1;
    int v = 0;
    for (size_t i = a; i < b; ++i) {
        if (s[i] < '0' || s[i] > '9') return -1;
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

static int LoadDictFile(const std::wstring& path, const Cfg& cfg, int gen, Dict& d) {
    std::wstring t;
    if (!ReadText(path, t)) return LOAD_UNREADABLE;
    std::vector<std::pair<uint64_t, int>> hs;                 // (fingerprint, score) per entry
    hs.reserve((size_t)std::count(t.begin(), t.end(), L'\n') + 1);
    d.byLen.resize(kMaxKey + 1);
    std::wstring nk;                                          // normalized entry (reused)

    size_t pos = 0, lines = 0;
    while (pos < t.size()) {
        if ((++lines & 0x3FFF) == 0 && Stale(gen)) return LOAD_STALE;
        size_t nl = t.find(L'\n', pos);
        if (nl == std::wstring::npos) nl = t.size();
        size_t end = nl;
        if (end > pos && t[end - 1] == L'\r') --end;
        if (end > pos) {
            size_t semi = std::wstring::npos;                 // "entry;score": last ';' on THIS line
            for (size_t i = end; i > pos; --i)
                if (t[i - 1] == L';') { semi = i - 1; break; }
            size_t stop = (semi != std::wstring::npos) ? semi : end;
            if (stop < end) {                                 // note the highest score in the file
                long long v = 0;
                bool digits = false;
                for (size_t i = stop + 1; i < end; ++i)
                    if (t[i] >= L'0' && t[i] <= L'9') { if (v < 100000000) v = v * 10 + (t[i] - L'0'); digits = true; }
                if (digits && v > d.maxScore) d.maxScore = v;
            }
            NormEntry(t.data() + pos, stop - pos, cfg, nk);       // upper-cased, ignored characters dropped
            if (!nk.empty()) {
                int sc = stop < end ? ParseScore(t.data(), stop + 1, end) : -1;
                hs.push_back({ HashKey(nk), sc });
                if (nk.size() <= kMaxKey) {                       // also kept as text, for pattern searches (Fill)
                    Dict::Bucket& bk = d.byLen[nk.size()];
                    bk.chars.insert(bk.chars.end(), nk.begin(), nk.end());
                    bk.scores.push_back(sc);
                }
            }
        }
        pos = nl + 1;
    }
    std::sort(hs.begin(), hs.end(), [](const std::pair<uint64_t, int>& a, const std::pair<uint64_t, int>& b) {
        return a.first != b.first ? a.first < b.first : a.second > b.second;   // same entry twice: highest score first
    });
    hs.erase(std::unique(hs.begin(), hs.end(), [](const std::pair<uint64_t, int>& a, const std::pair<uint64_t, int>& b) {
        return a.first == b.first;
    }), hs.end());
    d.hashes.reserve(hs.size());
    d.scores.reserve(hs.size());
    for (const auto& e : hs) { d.hashes.push_back(e.first); d.scores.push_back(e.second); }
    return LOAD_OK;
}

// A word listed more than once (in one dictionary or in several) is one word, as on the Fill tab. Its best copy
// (highest score, then the earliest dictionary) stays; the other copies are marked in `dup`.
static void MarkDuplicates(std::vector<Dict>& dicts) {
    struct Best { int score; uint64_t where; };
    auto place = [](size_t di, size_t L, size_t e) { return ((uint64_t)di << 56) | ((uint64_t)L << 48) | (uint64_t)e; };
    size_t n = 0;
    for (const Dict& d : dicts)
        for (const Dict::Bucket& b : d.byLen) n += b.scores.size();
    std::unordered_map<uint64_t, Best> best;
    best.reserve(n);
    for (size_t di = 0; di < dicts.size(); ++di)
        for (size_t L = 0; L < dicts[di].byLen.size(); ++L) {
            const Dict::Bucket& b = dicts[di].byLen[L];
            for (size_t e = 0; e < b.scores.size(); ++e) {
                uint64_t h = HashChars(&b.chars[e * L], L);
                auto it = best.find(h);
                if (it == best.end()) best.emplace(h, Best{ b.scores[e], place(di, L, e) });
                else if (b.scores[e] > it->second.score) it->second = Best{ b.scores[e], place(di, L, e) };
            }
        }
    for (size_t di = 0; di < dicts.size(); ++di)
        for (size_t L = 0; L < dicts[di].byLen.size(); ++L) {
            Dict::Bucket& b = dicts[di].byLen[L];
            b.dup.assign(b.scores.size(), 0);
            for (size_t e = 0; e < b.scores.size(); ++e)
                b.dup[e] = best[HashChars(&b.chars[e * L], L)].where != place(di, L, e) ? 1 : 0;
        }
}

// Worker thread. Returns false if a newer request superseded this one.
static bool ScanDicts(const LoadJob& job, LoadResult& r) {
    std::wstring folder = ResolveFolder(job.dirSetting);
    DWORD attr = GetFileAttributesW(folder.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        r.status = L"Folder not found: " + folder;
        return true;
    }

    WIN32_FIND_DATAW fd;
    HANDLE f = FindFirstFileW((folder + L"\\*.dict").c_str(), &fd);
    if (f != INVALID_HANDLE_VALUE) {
        do {
            std::wstring n = fd.cFileName;
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || n.size() <= 5 ||
                lstrcmpiW(n.c_str() + n.size() - 5, L".dict") != 0) continue;
            if (Stale(job.gen)) { FindClose(f); return false; }
            Dict d;
            d.name = n.substr(0, n.size() - 5);
            d.path = folder + L"\\" + n;
            int rc = LoadDictFile(d.path, job.cfg, job.gen, d);
            if (rc == LOAD_STALE) { FindClose(f); return false; }
            if (rc == LOAD_OK) r.dicts.push_back(std::move(d));
        } while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    std::sort(r.dicts.begin(), r.dicts.end(), [](const Dict& a, const Dict& b) {
        return lstrcmpiW(a.name.c_str(), b.name.c_str()) < 0;
    });
    MarkDuplicates(r.dicts);

    size_t entries = 0;
    for (const Dict& d : r.dicts) entries += d.hashes.size();
    if (r.dicts.empty())
        r.status = L"No .dict files found in " + folder;
    else
        r.status = L"Loaded " + std::to_wstring(r.dicts.size()) +
                   (r.dicts.size() == 1 ? L" dictionary (" : L" dictionaries (") +
                   std::to_wstring(entries) + L" entries)";
    return true;
}

static unsigned __stdcall LoadThread(void* p) {
    LoadJob* job = (LoadJob*)p;
    LoadResult* r = new LoadResult;
    r->gen = job->gen;
    r->sig = job->cfg.sig;
    if (!ScanDicts(*job, *r) || !PostMessageW(g_hwnd, WM_APP_LOADED, 0, (LPARAM)r)) delete r;
    delete job;
    return 0;
}

// ---- dictionary list and score editing (Score edit tab) --------------------------------------
static void Update();

static void PopulateDictList(const std::vector<std::wstring>& checked) {
    SendMessageW(hDictList, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(hDictList);
    for (size_t i = 0; i < g_dicts.size(); ++i) {
        const Dict& d = g_dicts[i];
        std::wstring cols[3] = { d.name, std::to_wstring(d.hashes.size()), std::to_wstring(d.maxScore) };
        LVITEMW it = {};
        it.mask = LVIF_TEXT;
        it.iItem = (int)i;
        it.pszText = (LPWSTR)cols[0].c_str();
        SendMessageW(hDictList, LVM_INSERTITEMW, 0, (LPARAM)&it);
        for (int c = 1; c < 3; ++c) {
            it.iSubItem = c;
            it.pszText = (LPWSTR)cols[c].c_str();
            SendMessageW(hDictList, LVM_SETITEMTEXTW, i, (LPARAM)&it);
        }
        if (std::find(checked.begin(), checked.end(), d.name) != checked.end())
            ListView_SetCheckState(hDictList, (int)i, TRUE);
    }
    SendMessageW(hDictList, WM_SETREDRAW, TRUE, 0);
}

static std::vector<std::wstring> CheckedNames() {
    std::vector<std::wstring> v;
    for (size_t i = 0; i < g_dicts.size(); ++i)
        if (ListView_GetCheckState(hDictList, (int)i)) v.push_back(g_dicts[i].name);
    return v;
}

// Current rules from the Config tab. The 64 KB table is only rebuilt when a rule actually changed.
static bool Checked(HWND c) { return SendMessageW(c, BM_GETCHECK, 0, 0) == BST_CHECKED; }

static const Cfg& CurCfg() {
    wchar_t pb[16], vb[16], kb[16];
    GetWindowTextW(hBlanks, pb, 16);
    GetWindowTextW(hVowels, vb, 16);
    GetWindowTextW(hConsonants, kb, 16);
    bool ws = Checked(hWs), num = Checked(hNums), pun = Checked(hPunct), spc = Checked(hSpecial);
    std::wstring sig = std::wstring(pb) + L'\x1F' + vb + L'\x1F' + kb + L'\x1F' + (ws ? L'1' : L'0') + (num ? L'1' : L'0') +
                       (pun ? L'1' : L'0') + (spc ? L'1' : L'0');
    if (sig != g_cfg.sig) {
        g_cfg.P = pb;
        g_cfg.V = vb;
        g_cfg.K = kb;
        for (std::wstring* t : { &g_cfg.P, &g_cfg.V, &g_cfg.K }) CharUpperBuffW(&(*t)[0], (DWORD)t->size());
        std::fill(g_cfg.ign.begin(), g_cfg.ign.end(), (unsigned char)0);
        auto add = [](const wchar_t* chars) { for (; *chars; ++chars) g_cfg.ign[(unsigned short)*chars] = 1; };
        if (ws) add(kWhitespace);
        if (num) add(kNumbers);
        if (pun) add(kPunct);
        if (spc) add(kSpecial);
        for (const std::wstring* t : { &g_cfg.P, &g_cfg.V, &g_cfg.K }) for (wchar_t c : *t) g_cfg.ign[(unsigned short)c] = 0;   // blank characters always count
        g_cfg.sig = sig;
    }
    return g_cfg;
}

// Starts a background (re)load with the current rules and folder. Never blocks the window.
static void StartReload() {
    KillTimer(g_hwnd, IDT_RELOAD);
    LoadJob* job = new LoadJob;
    job->gen = ++g_gen;
    job->cfg = CurCfg();                                      // a copy: the worker owns its rules
    wchar_t db[MAX_PATH];
    GetWindowTextW(hDictDir, db, MAX_PATH);
    job->dirSetting = g_requestedDir = db;
    g_loading = true;
    SetWindowTextW(hDictStatus, L"Loading dictionaries...");

    uintptr_t th = _beginthreadex(nullptr, 0, LoadThread, job, 0, nullptr);
    if (th) {
        CloseHandle((HANDLE)th);
    } else {
        delete job;
        g_loading = false;
        SetWindowTextW(hDictStatus, L"Could not start the dictionary loader");
    }
    Update();
}

// Settings typing: wait for a pause before reloading, so each keystroke doesn't trigger a load.
static void ScheduleReload(UINT ms) { SetTimer(g_hwnd, IDT_RELOAD, ms, nullptr); }

static void OnLoaded(LoadResult* r) {
    if (r->gen == g_gen.load()) {                             // results of superseded loads are dropped
        std::vector<std::wstring> checked = CheckedNames();   // keep the user's ticks across the reload
        checked.erase(std::remove_if(checked.begin(), checked.end(), [](const std::wstring& n) {
            return std::find(g_untick.begin(), g_untick.end(), n) != g_untick.end();
        }), checked.end());
        g_untick.clear();
        g_dicts = std::move(r->dicts);
        g_loadedSig = r->sig;
        g_loading = false;
        SetWindowTextW(hDictStatus, r->status.c_str());
        PopulateDictList(checked);
        Update();
    }
    delete r;
}

// new = old * dst / src, rounded half-up to a multiple of step (integer maths, no float error).
static long long Rescale(long long v, long long src, long long dst, long long step) {
    return ((2 * v * dst + src * step) / (2 * src * step)) * step;
}

// What to do to each score: rescale and/or round it first (ratio = Old max and New max were given), then clamp anything
// above a maximum down to that maximum. Boxes left blank are skipped.
struct ScoreOp { bool rescale, ratio; long long src, dst, step; bool clamp; long long max; };
static long long NewScore(const ScoreOp& op, long long v) {
    if (op.rescale) v = Rescale(v, op.src, op.dst, op.step);
    if (op.clamp) v = std::min(v, op.max);
    return v;
}

// Rewrites the whole-number score after the last ';' on each line. Everything else (entries,
// line endings, BOM, other spacing) is copied byte-for-byte, and so is any score that comes out the same.
static std::string ConvertBytes(const std::string& in, const ScoreOp& op, size_t& changed, size_t& skipped) {
    std::string out;
    out.reserve(in.size());
    changed = skipped = 0;
    size_t pos = 0;
    while (pos < in.size()) {
        size_t nl = in.find('\n', pos);
        size_t lineEnd = (nl == std::string::npos) ? in.size() : nl;
        size_t end = lineEnd;
        if (end > pos && in[end - 1] == '\r') --end;
        bool done = false;
        size_t semi = std::string::npos;                      // last ';' on THIS line
        for (size_t i = end; i > pos; --i)
            if (in[i - 1] == ';') { semi = i - 1; break; }
        if (semi != std::string::npos) {
            size_t a = semi + 1, b = end;
            while (a < b && (in[a] == ' ' || in[a] == '\t')) ++a;
            while (b > a && (in[b - 1] == ' ' || in[b - 1] == '\t')) --b;
            bool digits = b > a && b - a <= 9;
            long long v = 0;
            for (size_t i = a; digits && i < b; ++i) {
                if (in[i] < '0' || in[i] > '9') digits = false;
                else v = v * 10 + (in[i] - '0');
            }
            if (digits) {
                long long nv = NewScore(op, v);
                if (nv == v) {
                    out.append(in, pos, lineEnd - pos);       // comes out the same: untouched
                } else {
                    out.append(in, pos, a - pos);             // entry, ';' and any space before the score
                    out += std::to_string(nv);
                    out.append(in, b, lineEnd - b);           // any space after the score, and the '\r'
                    ++changed;
                }
                done = true;
            }
        }
        if (!done) {
            out.append(in, pos, lineEnd - pos);
            if (end > pos) ++skipped;
        }
        if (nl != std::string::npos) out += '\n';
        pos = (nl == std::string::npos) ? in.size() : nl + 1;
    }
    return out;
}

// Sets the score on every line whose normalized entry has fingerprint hk (same rules as the loader).
// Everything else is copied byte-for-byte. Lines with no whole-number score, or already at that score, are left alone.
static std::string SetWordScoreBytes(const std::string& in, const Cfg& cfg, uint64_t hk, int score, size_t& changed) {
    std::string out;
    out.reserve(in.size() + 16);
    changed = 0;
    UINT cp = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, in.data(), (int)in.size(), nullptr, 0) > 0 ? CP_UTF8 : CP_ACP;
    size_t pos = (in.size() >= 3 && (unsigned char)in[0] == 0xEF && (unsigned char)in[1] == 0xBB &&
                  (unsigned char)in[2] == 0xBF) ? 3 : 0;
    out.append(in, 0, pos);
    std::wstring w;
    while (pos < in.size()) {
        size_t nl = in.find('\n', pos);
        size_t lineEnd = (nl == std::string::npos) ? in.size() : nl;
        size_t end = lineEnd;
        if (end > pos && in[end - 1] == '\r') --end;
        size_t semi = std::string::npos;                      // last ';' on THIS line
        for (size_t i = end; i > pos; --i)
            if (in[i - 1] == ';') { semi = i - 1; break; }
        bool done = false;
        if (semi != std::string::npos) {
            bool ascii = true;
            for (size_t i = pos; i < semi && ascii; ++i) ascii = (unsigned char)in[i] < 0x80;
            w.clear();
            if (ascii) {
                w.assign(in.begin() + (std::ptrdiff_t)pos, in.begin() + (std::ptrdiff_t)semi);
            } else {
                int n = MultiByteToWideChar(cp, 0, in.data() + pos, (int)(semi - pos), nullptr, 0);
                if (n > 0) {
                    w.resize((size_t)n);
                    MultiByteToWideChar(cp, 0, in.data() + pos, (int)(semi - pos), &w[0], n);
                }
            }
            uint64_t h;
            if (EntryHash(w.data(), w.size(), cfg, h) && h == hk) {
                size_t a = semi + 1, b = end;
                while (a < b && (in[a] == ' ' || in[a] == '\t')) ++a;
                while (b > a && (in[b - 1] == ' ' || in[b - 1] == '\t')) --b;
                int old = ParseScore(in.data(), a, b);
                if (old >= 0 && old != score) {
                    out.append(in, pos, a - pos);             // entry, ';' and any space before the score
                    out += std::to_string(score);
                    out.append(in, b, lineEnd - b);           // any space after the score, and the '\r'
                    ++changed;
                    done = true;
                }
            }
        }
        if (!done) out.append(in, pos, lineEnd - pos);
        if (nl != std::string::npos) out += '\n';
        pos = (nl == std::string::npos) ? in.size() : nl + 1;
    }
    return out;
}

static bool WriteBytes(const std::wstring& path, const std::string& data) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    size_t off = 0;
    while (ok && off < data.size()) {
        DWORD n = 0, chunk = (DWORD)std::min<size_t>(data.size() - off, 1u << 24);
        ok = WriteFile(f, data.data() + off, chunk, &n, nullptr) && n > 0;
        off += n;
    }
    return CloseHandle(f) && ok;
}

// Keeps the original as "name.dict (old)" (then "(old 2)"... never overwriting a backup), then replaces the file with
// `data` (temp file + rename). Returns "" on success, otherwise the reason it didn't.
static std::wstring BackupAndWrite(const std::wstring& path, const std::string& data, std::wstring& backup) {
    backup = path + L" (old)";                                // not *.dict, so it is never loaded
    for (int n = 2; GetFileAttributesW(backup.c_str()) != INVALID_FILE_ATTRIBUTES; ++n)
        backup = path + L" (old " + std::to_wstring(n) + L")";     // never overwrite an earlier backup
    if (!CopyFileW(path.c_str(), backup.c_str(), TRUE)) return L"could not create the backup, file left alone";

    std::wstring tmp = path + L".tmp";
    if (!WriteBytes(tmp, data) || !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmp.c_str());
        return L"could not write the file (the backup was kept)";
    }
    return L"";
}

// Backs up, then rewrites one dictionary. Returns "" on success, otherwise the reason it didn't.
static std::wstring ConvertFile(const Dict& d, const ScoreOp& op, size_t& changed, size_t& skipped,
                                std::wstring& backup) {
    std::string in;
    if (!ReadBytes(d.path, in)) return L"could not read the file";
    std::string out = ConvertBytes(in, op, changed, skipped);
    if (changed == 0) return L"no scores would change, file left alone";

    return BackupAndWrite(d.path, out, backup);
}

// ---- Merging dictionaries ----------------------------------------------------------------------
// Two entries are the same when they match ignoring capitals (not the Config ignore rules, so a merge never drops a
// spelling because of a setting). Where both have an entry the higher score is kept; the first spelling stays.
struct Merger {
    std::vector<std::wstring> lines;              // the merged file, one line each (no line ends)
    std::vector<int> scores;                      // each line's whole-number score, -1 if it has none
    std::unordered_map<std::wstring, size_t> at;  // upper-cased entry -> its line
    size_t added = 0, raised = 0, same = 0;       // what MergeText did: new entries, higher scores, entries already there
};

// Splits one line "entry;score" at its last ';' (spaces/tabs around the entry trimmed). False for a line with no entry.
static bool SplitDictLine(const std::wstring& t, size_t pos, size_t end, std::wstring& entry, std::wstring& field,
                          bool& hasField, int& score) {
    size_t semi = std::wstring::npos;
    for (size_t i = end; i > pos; --i)
        if (t[i - 1] == L';') { semi = i - 1; break; }
    size_t a = pos, b = semi != std::wstring::npos ? semi : end;
    while (a < b && (t[a] == L' ' || t[a] == L'\t')) ++a;
    while (b > a && (t[b - 1] == L' ' || t[b - 1] == L'\t')) --b;
    entry.assign(t, a, b - a);
    hasField = semi != std::wstring::npos;
    field.clear();
    score = -1;
    if (hasField) {
        size_t fa = semi + 1, fb = end;
        while (fa < fb && (t[fa] == L' ' || t[fa] == L'\t')) ++fa;
        while (fb > fa && (t[fb - 1] == L' ' || t[fb - 1] == L'\t')) --fb;
        field.assign(t, fa, fb - fa);
        score = ParseScore(t.data(), fa, fb);
    }
    return !entry.empty();
}

template <class F> static void ForEachLine(const std::wstring& t, F fn) {   // fn(pos, end): end is before any '\r'
    size_t pos = 0;
    while (pos < t.size()) {
        size_t nl = t.find(L'\n', pos);
        if (nl == std::wstring::npos) nl = t.size();
        size_t end = nl;
        if (end > pos && t[end - 1] == L'\r') --end;
        fn(pos, end);
        pos = nl + 1;
    }
}

static void MergeKey(const std::wstring& entry, std::wstring& key) {
    key.clear();
    for (wchar_t c : entry) key += Up(c);
}

// The starting point when merging into an existing file: every line is kept exactly as it is.
static void MergeBase(Merger& m, const std::wstring& t) {
    std::wstring entry, field, key;
    bool has = false;
    int sc = -1;
    ForEachLine(t, [&](size_t pos, size_t end) {
        size_t idx = m.lines.size();
        m.lines.emplace_back(t, pos, end - pos);
        if (SplitDictLine(t, pos, end, entry, field, has, sc)) {
            m.scores.push_back(sc);
            MergeKey(entry, key);
            m.at.emplace(key, idx);                           // the first copy of an entry is the one later merges compare with
        } else {
            m.scores.push_back(-1);
        }
    });
}

// Adds another dictionary's entries: new ones at the end, higher scores raised in place.
static void MergeText(Merger& m, const std::wstring& t) {
    std::wstring entry, field, key;
    bool has = false;
    int sc = -1;
    ForEachLine(t, [&](size_t pos, size_t end) {
        if (!SplitDictLine(t, pos, end, entry, field, has, sc)) return;
        MergeKey(entry, key);
        auto it = m.at.find(key);
        if (it == m.at.end()) {
            m.at.emplace(key, m.lines.size());
            m.lines.push_back(sc >= 0 ? entry + L";" + std::to_wstring(sc) : has ? entry + L";" + field : entry);
            m.scores.push_back(sc);
            ++m.added;
        } else if (sc > m.scores[it->second]) {               // the higher score wins; the rest of the line stays as it was
            std::wstring& line = m.lines[it->second];
            size_t semi = line.rfind(L';');
            if (semi == std::wstring::npos) {
                line += L";";
            } else {
                size_t a = semi + 1;
                while (a < line.size() && (line[a] == L' ' || line[a] == L'\t')) ++a;
                line.resize(a);
            }
            line += std::to_wstring(sc);
            m.scores[it->second] = sc;
            ++m.raised;
        } else {
            ++m.same;
        }
    });
}

// The merged file as bytes: UTF-8 (with a BOM if the original had one), one line each, the file's own line ending.
static std::string MergedBytes(const Merger& m, bool bom, bool crlf) {
    std::wstring all;
    for (const std::wstring& l : m.lines) { all += l; all += crlf ? L"\r\n" : L"\n"; }
    std::string out = bom ? std::string("\xEF\xBB\xBF") : std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, all.data(), (int)all.size(), nullptr, 0, nullptr, nullptr);
    if (n > 0) {
        size_t at = out.size();
        out.resize(at + (size_t)n);
        WideCharToMultiByte(CP_UTF8, 0, all.data(), (int)all.size(), &out[at], n, nullptr, nullptr);
    }
    return out;
}
// ---- end of merging (pure) ----

struct MergeResult { size_t added = 0, raised = 0, same = 0; bool wrote = false; std::wstring backup; };

// Merges the source dictionaries into the file `target`: an existing dictionary (rewritten, its original kept as "(old)",
// only if something changes) or, with createNew, a new file. Returns "" on success, otherwise the reason it didn't.
static std::wstring MergeFiles(const std::wstring& target, bool createNew, const std::vector<std::wstring>& sources,
                               MergeResult& r) {
    Merger m;
    bool bom = false, crlf = false;
    if (!createNew) {
        std::string raw;
        std::wstring t;
        if (!ReadBytes(target, raw) || !ReadText(target, t)) return L"could not read the default dictionary";
        bom = raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF;
        size_t nl = raw.find('\n');
        crlf = nl != std::string::npos && nl > 0 && raw[nl - 1] == '\r';
        MergeBase(m, t);
    }
    for (const std::wstring& src : sources) {                 // read everything before writing anything
        std::wstring t;
        if (!ReadText(src, t)) return L"could not read " + src.substr(src.find_last_of(L'\\') + 1);
        MergeText(m, t);
    }
    r.added = m.added;
    r.raised = m.raised;
    r.same = m.same;
    if (createNew) {
        if (m.lines.empty()) return L"there are no entries to merge";
        std::wstring tmp = target + L".tmp";
        if (!WriteBytes(tmp, MergedBytes(m, false, false)) || !MoveFileExW(tmp.c_str(), target.c_str(), 0)) {
            DeleteFileW(tmp.c_str());
            return L"could not write the new dictionary";
        }
        r.wrote = true;
        return L"";
    }
    if (m.added + m.raised == 0) return L"";                  // everything is already there: the file is left alone
    std::wstring err = BackupAndWrite(target, MergedBytes(m, bom, crlf), r.backup);
    r.wrote = err.empty();
    return err;
}

static long long GetNum(HWND e) {
    wchar_t b[16];
    GetWindowTextW(e, b, 16);
    return wcstoll(b, nullptr, 10);
}

static std::wstring g_previewText;      // the info icon's tooltip, rebuilt on every hover

// The score change the four boxes ask for. Blank boxes are skipped: Old max and New max rescale (both are needed),
// Round to rounds (to whole numbers when Old/New are given without it), Clamp max caps what is left.
static bool ReadScoreOp(ScoreOp& op, std::wstring& err) {
    long long o = GetNum(hOldMax), n = GetNum(hNewMax), r = GetNum(hRound), c = GetNum(hClampMax);
    op = ScoreOp{ false, false, 1, 1, 1, false, 0 };
    if ((o > 0) != (n > 0)) { err = L"Enter both Old max and New max to rescale, or leave both blank."; return false; }
    if (o > 0) { op.rescale = op.ratio = true; op.src = o; op.dst = n; }
    if (r > 1) { op.rescale = true; op.step = r; }
    if (c > 0) { op.clamp = true; op.max = c; }
    if (!op.rescale && !op.clamp) {
        err = L"Fill in Old max and New max (rescale), Round to (round) and/or Clamp max (cap), then try again.";
        return false;
    }
    return true;
}

// The info icon's tooltip: what the boxes do, and what they would do to a few example scores.
static std::wstring PreviewText() {
    std::wstring s = L"Scores are rescaled and rounded first (Old max -> New max, rounded to Round to), then anything\n"
                     L"above Clamp max is capped. Boxes left blank are skipped.\n\n";
    ScoreOp op;
    std::wstring err;
    if (!ReadScoreOp(op, err)) return s + err;
    long long base = op.ratio ? op.src : op.clamp ? op.max * 2 : op.step * 20 + op.step / 2 + 1;
    s += L"Preview (old -> new):";
    for (int pct : { 100, 75, 50, 25 }) {
        long long v = base * pct / 100;
        s += L"  " + std::to_wstring(v) + L" -> " + std::to_wstring(NewScore(op, v)) + (pct > 25 ? L"," : L"");
    }
    return s;
}

static void ApplyScores(HWND parent) {
    ScoreOp op;
    std::wstring err0;
    if (!ReadScoreOp(op, err0)) { SetWindowTextW(hLog, err0.c_str()); return; }
    std::vector<size_t> sel;
    for (size_t i = 0; i < g_dicts.size(); ++i)
        if (ListView_GetCheckState(hDictList, (int)i)) sel.push_back(i);
    if (sel.empty()) { SetWindowTextW(hLog, L"Tick at least one dictionary first."); return; }

    std::wstring n = std::to_wstring(sel.size()) + (sel.size() == 1 ? L" dictionary" : L" dictionaries");
    std::wstring q = L"In " + n + L":";
    if (op.ratio)
        q += L"\n- rescale the scores from a max of " + std::to_wstring(op.src) + L" to a max of " + std::to_wstring(op.dst) +
             (op.step > 1 ? L", rounded to the nearest " + std::to_wstring(op.step) : std::wstring(L", to whole numbers"));
    else if (op.rescale)
        q += L"\n- round the scores to the nearest " + std::to_wstring(op.step);
    if (op.clamp)
        q += std::wstring(L"\n- ") + (op.rescale ? L"then clamp" : L"clamp") + L" every score above " + std::to_wstring(op.max) +
             L" down to " + std::to_wstring(op.max);
    q += L"\n\nContinue? The original of each changed file is kept beside it as \"name.dict (old)\".";
    if (MessageBoxW(parent, q.c_str(), L"CrossTools", MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    std::wstring log;
    std::vector<std::wstring> done;
    for (size_t i : sel) {
        const Dict& d = g_dicts[i];
        size_t changed = 0, skipped = 0;
        std::wstring backup, err = ConvertFile(d, op, changed, skipped, backup);
        if (!log.empty()) log += L"\r\n";
        if (err.empty()) {
            log += d.name + L": " + std::to_wstring(changed) + L" scores changed. Original kept as " +
                   backup.substr(backup.find_last_of(L'\\') + 1);
            if (skipped) log += L" (" + std::to_wstring(skipped) + L" lines had no whole-number score and were left as is)";
            done.push_back(d.name);
        } else {
            log += d.name + L": " + err;
        }
    }
    SetWindowTextW(hLog, log.c_str());
    g_untick = done;                                          // untick what was done: no accidental repeat
    StartReload();                                            // reload so the new top scores show
}

// "mine" / "mine.dict" / "sub\MINE.DICT" -> "mine.dict" / "MINE.dict": a file name only, always .dict.
static std::wstring DictFileName(const wchar_t* typed) {
    std::wstring s(typed);
    size_t a = s.find_first_not_of(L" \t"), b = s.find_last_not_of(L" \t");
    s = (a == std::wstring::npos) ? std::wstring() : s.substr(a, b - a + 1);
    size_t k = s.find_last_of(L"\\/");
    if (k != std::wstring::npos) s = s.substr(k + 1);
    if (s.size() >= 5 && lstrcmpiW(s.c_str() + s.size() - 5, L".dict") == 0) s.resize(s.size() - 5);
    if (s.empty()) s = L"CrossTools";
    return s + L".dict";
}

// Appends "WORD;score" to the default dictionary (created if missing), then reloads the dictionaries.
static void AddEntry(std::wstring entry, long long score) {
    wchar_t nb[MAX_PATH], db[MAX_PATH];
    GetWindowTextW(hAddDict, nb, MAX_PATH);
    GetWindowTextW(hDictDir, db, MAX_PATH);

    size_t a = entry.find_first_not_of(L" \t"), b = entry.find_last_not_of(L" \t");
    if (a == std::wstring::npos) return;
    entry = entry.substr(a, b - a + 1);

    std::wstring folder = ResolveFolder(db);
    CreateDirectoryW(folder.c_str(), nullptr);                // no-op if it already exists
    std::wstring path = folder + L"\\" + DictFileName(nb);

    std::wstring line = entry + L";" + std::to_wstring(score);
    int n = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), (int)line.size(), nullptr, 0, nullptr, nullptr);
    std::string bytes((size_t)(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, line.c_str(), (int)line.size(), &bytes[0], n, nullptr, nullptr);
    bytes += '\n';

    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ | FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    bool ok = f != INVALID_HANDLE_VALUE;
    if (ok) {
        LARGE_INTEGER sz;
        if (GetFileSizeEx(f, &sz) && sz.QuadPart > 0) {       // start on a fresh line if the file doesn't end with one
            char last = 0;
            DWORD got = 0;
            LARGE_INTEGER at;
            at.QuadPart = sz.QuadPart - 1;
            if (SetFilePointerEx(f, at, nullptr, FILE_BEGIN) && ReadFile(f, &last, 1, &got, nullptr) &&
                got == 1 && last != '\n')
                bytes.insert(bytes.begin(), '\n');
        }
        DWORD wrote = 0;                                      // FILE_APPEND_DATA: always written at the end
        ok = WriteFile(f, bytes.data(), (DWORD)bytes.size(), &wrote, nullptr) && wrote == bytes.size();
        CloseHandle(f);
    }
    if (!ok) {
        MessageBoxW(g_hwnd, (L"Could not write to " + path).c_str(), L"CrossTools", MB_OK | MB_ICONWARNING);
        return;
    }
    StartReload();                                            // the new entry is picked up and shown as found
}

static void AddToDefaultDict() {
    wchar_t wb[64], sb[16];
    GetWindowTextW(hInput, wb, 64);
    GetWindowTextW(hScore, sb, 16);
    AddEntry(wb, sb[0] ? wcstoll(sb, nullptr, 10) : 50);
}

// ---- Dictionaries tab: merge, make default, and dump the added words into the default dictionary ----------
static std::wstring FileOf(const std::wstring& path) { return path.substr(path.find_last_of(L'\\') + 1); }

static int DefaultDictIndex() {                               // the default dictionary among the loaded ones, or -1
    for (size_t i = 0; i < g_dicts.size(); ++i)
        if (lstrcmpiW(g_dicts[i].name.c_str(), g_defaultDict.c_str()) == 0) return (int)i;
    return -1;
}

// ---- merge window: built in memory (no .rc dialog), so the exe stays one self-contained file ----
enum { IDC_MD_TEXT = 100, IDC_MD_NEW, IDC_MD_KEEP, IDC_MD_NAME, IDC_MD_LIST, IDC_MD_HINT };

static void PutDword(std::vector<WORD>& v, DWORD d) { v.push_back((WORD)(d & 0xFFFF)); v.push_back((WORD)(d >> 16)); }
static void PutString(std::vector<WORD>& v, const wchar_t* s) { do v.push_back((WORD)*s); while (*s++); }
static void PutItem(std::vector<WORD>& v, DWORD style, int x, int y, int cx, int cy, WORD id, WORD cls, const wchar_t* text) {
    if (v.size() & 1) v.push_back(0);                         // every item starts on a DWORD boundary
    PutDword(v, style | WS_CHILD | WS_VISIBLE);
    PutDword(v, 0);
    v.push_back((WORD)x);
    v.push_back((WORD)y);
    v.push_back((WORD)cx);
    v.push_back((WORD)cy);
    v.push_back(id);
    v.push_back(0xFFFF);                                      // the class is a predefined atom: 0x80 button, 0x81 edit,
    v.push_back(cls);                                         // 0x82 static, 0x85 combo box
    PutString(v, text);
    v.push_back(0);                                           // no creation data
}
static void BuildMergeTemplate(std::vector<WORD>& v) {
    v.clear();
    PutDword(v, DS_MODALFRAME | DS_SETFONT | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU);
    PutDword(v, 0);
    v.push_back(8);                                           // number of items
    v.push_back(0);
    v.push_back(0);
    v.push_back(282);                                         // width and height, in dialog units
    v.push_back(112);
    v.push_back(0);                                           // no menu
    v.push_back(0);                                           // the default dialog class
    PutString(v, L"Merge dictionaries");
    v.push_back(8);                                           // font: 8 pt MS Shell Dlg, like the classic dialogs
    PutString(v, L"MS Shell Dlg");
    PutItem(v, SS_LEFT, 8, 8, 266, 20, IDC_MD_TEXT, 0x0082, L"");
    PutItem(v, BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 8, 34, 108, 12, IDC_MD_NEW, 0x0080, L"A new dictionary named:");
    PutItem(v, BS_AUTORADIOBUTTON | WS_TABSTOP, 8, 54, 108, 12, IDC_MD_KEEP, 0x0080, L"Keep one of them:");
    PutItem(v, ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 120, 32, 150, 14, IDC_MD_NAME, 0x0081, L"");
    PutItem(v, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 120, 52, 150, 60, IDC_MD_LIST, 0x0085, L"");
    PutItem(v, SS_LEFT, 8, 72, 266, 20, IDC_MD_HINT, 0x0082, L"");
    PutItem(v, BS_DEFPUSHBUTTON | WS_TABSTOP, 168, 94, 50, 14, IDOK, 0x0080, L"Merge");
    PutItem(v, BS_PUSHBUTTON | WS_TABSTOP, 224, 94, 50, 14, IDCANCEL, 0x0080, L"Cancel");
}
// ---- end of merge window template ----

struct MergeAsk {
    std::vector<std::wstring> names;                          // in: the checked dictionaries
    int preselect = 0;                                        // in: which one "keep one of them" starts on
    std::wstring folder;                                      // in: the dictionaries folder (a new name must not exist there)
    bool toNew = true;                                        // out: a new dictionary, or keep one of the checked ones
    std::wstring name;                                        // out: the new dictionary's file name
    int keep = 0;                                             // out: which of the checked ones is kept
};

static INT_PTR CALLBACK MergeDlgProc(HWND d, UINT m, WPARAM w, LPARAM l) {
    MergeAsk* a = (MergeAsk*)GetWindowLongPtrW(d, DWLP_USER);
    switch (m) {
    case WM_INITDIALOG: {
        a = (MergeAsk*)l;
        SetWindowLongPtrW(d, DWLP_USER, (LONG_PTR)a);
        SetDlgItemTextW(d, IDC_MD_TEXT, (L"Merge the " + std::to_wstring(a->names.size()) +
                                         L" checked dictionaries into a new dictionary, or into one of them?").c_str());
        SetDlgItemTextW(d, IDC_MD_HINT, L"The others are merged into the one you keep. Where an entry is in more than one, "
                                        L"the higher score is kept.");
        SetDlgItemTextW(d, IDC_MD_NAME, L"merged");
        SendDlgItemMessageW(d, IDC_MD_NAME, EM_LIMITTEXT, 60, 0);
        for (const std::wstring& n : a->names) SendDlgItemMessageW(d, IDC_MD_LIST, CB_ADDSTRING, 0, (LPARAM)n.c_str());
        SendDlgItemMessageW(d, IDC_MD_LIST, CB_SETCURSEL, (WPARAM)a->preselect, 0);
        CheckRadioButton(d, IDC_MD_NEW, IDC_MD_KEEP, IDC_MD_NEW);
        EnableWindow(GetDlgItem(d, IDC_MD_LIST), FALSE);
        SendDlgItemMessageW(d, IDC_MD_NAME, EM_SETSEL, 0, -1);
        SetFocus(GetDlgItem(d, IDC_MD_NAME));
        return FALSE;                                         // the focus is set above
    }
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDC_MD_NEW:
        case IDC_MD_KEEP: {
            bool toNew = IsDlgButtonChecked(d, IDC_MD_NEW) == BST_CHECKED;
            EnableWindow(GetDlgItem(d, IDC_MD_NAME), toNew);
            EnableWindow(GetDlgItem(d, IDC_MD_LIST), !toNew);
            return TRUE;
        }
        case IDOK: {
            a->toNew = IsDlgButtonChecked(d, IDC_MD_NEW) == BST_CHECKED;
            if (a->toNew) {
                wchar_t nb[MAX_PATH];
                GetDlgItemTextW(d, IDC_MD_NAME, nb, MAX_PATH);
                if (std::wstring(nb).find_first_not_of(L" \t") == std::wstring::npos) {
                    MessageBoxW(d, L"Type a name for the new dictionary.", L"CrossTools", MB_OK | MB_ICONINFORMATION);
                    return TRUE;
                }
                a->name = DictFileName(nb);
                if (GetFileAttributesW((a->folder + L"\\" + a->name).c_str()) != INVALID_FILE_ATTRIBUTES) {
                    MessageBoxW(d, (a->name + L" already exists. Pick another name.").c_str(), L"CrossTools",
                                MB_OK | MB_ICONINFORMATION);
                    return TRUE;
                }
            } else {
                a->keep = (int)SendDlgItemMessageW(d, IDC_MD_LIST, CB_GETCURSEL, 0, 0);
                if (a->keep < 0) a->keep = 0;
            }
            EndDialog(d, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(d, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// The Merge dictionaries button: asks whether the checked dictionaries go into a new dictionary or into one of them.
static void ApplyMerge(HWND parent) {
    if (g_loading) { SetWindowTextW(hLog, L"Dictionaries are still loading. Try again in a moment."); return; }
    std::vector<size_t> sel;
    for (size_t i = 0; i < g_dicts.size(); ++i)
        if (ListView_GetCheckState(hDictList, (int)i)) sel.push_back(i);
    if (sel.size() < 2) { SetWindowTextW(hLog, L"Tick at least two dictionaries to merge."); return; }

    wchar_t db[MAX_PATH];
    GetWindowTextW(hDictDir, db, MAX_PATH);
    MergeAsk ask;
    ask.folder = ResolveFolder(db);
    int def = DefaultDictIndex();
    for (size_t k = 0; k < sel.size(); ++k) {
        ask.names.push_back(g_dicts[sel[k]].name);
        if ((int)sel[k] == def) ask.preselect = (int)k;       // the default dictionary is the likely one to keep
    }
    std::vector<WORD> tpl;
    BuildMergeTemplate(tpl);
    INT_PTR res = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), (LPCDLGTEMPLATEW)tpl.data(), parent, MergeDlgProc,
                                          (LPARAM)&ask);
    if (res == -1) { SetWindowTextW(hLog, L"Could not open the merge window."); return; }
    if (res != IDOK) return;

    std::vector<std::wstring> paths, names;
    for (size_t k = 0; k < sel.size(); ++k) {
        if (!ask.toNew && (int)k == ask.keep) continue;       // the one kept is not merged into itself
        paths.push_back(g_dicts[sel[k]].path);
        names.push_back(g_dicts[sel[k]].name);
    }
    std::wstring target = ask.toNew ? ask.folder + L"\\" + ask.name : g_dicts[sel[(size_t)ask.keep]].path;
    std::wstring list, tname = FileOf(target);
    for (const std::wstring& nm : names) list += (list.empty() ? L"" : L", ") + nm;

    MergeResult r;
    std::wstring err = MergeFiles(target, ask.toNew, paths, r), log;
    if (!err.empty()) {
        log = L"Merge into " + tname + L" failed: " + err + L".";
    } else if (ask.toNew) {
        log = L"Merged " + list + L" into the new dictionary " + tname + L": " + std::to_wstring(r.added) + L" entries (" +
              std::to_wstring(r.same + r.raised) + L" duplicates merged).";
    } else if (!r.wrote) {
        log = L"Nothing to merge: every entry of " + list + L" is already in " + tname + L" with the same or a higher score.";
    } else {
        log = L"Merged " + list + L" into " + tname + L": " + std::to_wstring(r.added) + L" added, " +
              std::to_wstring(r.raised) + L" scores raised, " + std::to_wstring(r.same) + L" already there. Original kept as " +
              FileOf(r.backup) + L".";
    }
    SetWindowTextW(hLog, log.c_str());
    if (err.empty()) {
        if (r.wrote) g_untick = names;                        // untick what was merged: no accidental repeat
        StartReload();
    }
}

// The Make default button: the one ticked dictionary becomes the default (shown in bold); the dump goes into it.
static void MakeDefault() {
    std::vector<size_t> sel;
    for (size_t i = 0; i < g_dicts.size(); ++i)
        if (ListView_GetCheckState(hDictList, (int)i)) sel.push_back(i);
    if (sel.size() != 1) { SetWindowTextW(hLog, L"Tick exactly one dictionary, then click Make default."); return; }
    g_defaultDict = g_dicts[sel[0]].name;
    InvalidateRect(hDictList, nullptr, TRUE);
    SetWindowTextW(hLog, (g_defaultDict + L" is now the default dictionary (shown in bold).").c_str());
}

// The dump button: merges the file Add word writes to (Config) into the default dictionary, then empties that file
// (its original is kept as "name.dict (old)", like every changed file).
static void DumpToDefault() {
    if (g_loading) { SetWindowTextW(hLog, L"Dictionaries are still loading. Try again in a moment."); return; }
    int ti = DefaultDictIndex();
    if (ti < 0) { SetWindowTextW(hLog, L"Make a dictionary the default first: tick one and click Make default."); return; }
    wchar_t nb[MAX_PATH], db[MAX_PATH];
    GetWindowTextW(hAddDict, nb, MAX_PATH);
    GetWindowTextW(hDictDir, db, MAX_PATH);
    std::wstring staging = ResolveFolder(db) + L"\\" + DictFileName(nb);
    const std::wstring target = g_dicts[(size_t)ti].path;
    std::wstring sname = FileOf(staging), tname = FileOf(target);
    if (lstrcmpiW(staging.c_str(), target.c_str()) == 0) {
        SetWindowTextW(hLog, (sname + L" is the default dictionary. Make another one the default to dump into.").c_str());
        return;
    }
    if (GetFileAttributesW(staging.c_str()) == INVALID_FILE_ATTRIBUTES) {
        SetWindowTextW(hLog, (sname + L" does not exist yet, so there is nothing to dump.").c_str());
        return;
    }
    std::wstring q = L"Dump " + sname + L" into " + tname + L"?\n\nIts entries are merged into " + tname +
                     L" (where an entry is in both, the higher score is kept), then " + sname +
                     L" is emptied.\nThe originals of both files are kept beside them as \"name.dict (old)\".";
    if (MessageBoxW(g_hwnd, q.c_str(), L"CrossTools", MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    MergeResult r;
    std::wstring err = MergeFiles(target, false, std::vector<std::wstring>(1, staging), r);
    if (!err.empty()) { SetWindowTextW(hLog, (L"Dump failed: " + err + L".").c_str()); return; }
    if (r.added + r.raised + r.same == 0) {
        SetWindowTextW(hLog, (sname + L" has no entries, so there is nothing to dump.").c_str());
        return;
    }
    std::wstring log = sname + L": " + std::to_wstring(r.added) + L" added to " + tname + L", " + std::to_wstring(r.raised) +
                       L" scores raised, " + std::to_wstring(r.same) + L" already there.";
    if (r.wrote) log += L" Original of " + tname + L" kept as " + FileOf(r.backup) + L".";
    std::wstring emptied, clearErr = BackupAndWrite(staging, std::string(), emptied);
    if (clearErr.empty()) log += L" " + sname + L" is now empty (original kept as " + FileOf(emptied) + L").";
    else log += L" But " + sname + L" could not be emptied: " + clearErr + L".";
    SetWindowTextW(hLog, log.c_str());
    StartReload();
}

// ---- Fill the blank: the list box only knows how many rows there are; each row is made when it is drawn ----
static const uint64_t kScoreMax = 100000;                      // most potential words ranked by score (they are all made at once)
static const uint64_t kBig = 1ULL << 62;                      // "too many to count" ceiling for combination counts
static const wchar_t* kVowels = L"AEIOU";
static const wchar_t* kConsonants = L"BCDFGHJKLMNPQRSTVWXYZ";

static const wchar_t* GapLetters(int kind) { return kind == 2 ? kVowels : kind == 3 ? kConsonants : L"ABCDEFGHIJKLMNOPQRSTUVWXYZ"; }
static int GapRadix(int kind) { return kind == 2 ? 5 : kind == 3 ? 21 : 26; }
static uint64_t SatMul(uint64_t a, uint64_t b) { return a >= kBig / b ? kBig : a * b; }
static uint64_t SatAdd(uint64_t a, uint64_t b) { return a + b >= kBig ? kBig : a + b; }

// Does dictionary character c fit a pattern position? (kind 0 = a fixed character, else a gap of that kind)
static bool FitsGap(int kind, wchar_t c, wchar_t fixedChar) {
    switch (kind) {
    case 0: return c == fixedChar;
    case 1: return c >= L'A' && c <= L'Z';
    case 2: return c != 0 && wcschr(kVowels, c) != nullptr;
    default: return c >= L'B' && c <= L'Z' && wcschr(kVowels, c) == nullptr;
    }
}

// Does a whole dictionary entry (L characters) fit the pattern?
static bool FitsEntry(const wchar_t* t, size_t L, const std::wstring& key, const std::vector<int>& kindAt) {
    for (size_t p = 0; p < L; ++p)
        if (!FitsGap(kindAt[p], t[p], key[p])) return false;
    return true;
}

enum { FILL_ALL = 0, FILL_DICT = 1, FILL_POTENTIAL = 2, FILL_BOTH = 3 };   // the Show drop-down
enum { ROW_PLAIN = 0, ROW_DICT = 1, ROW_POTENTIAL = 2 };                   // what a row is: plain, green, yellow

struct FillHit { std::wstring letters; int score; uint64_t rank; };

// One word of a phrase ("COP C_R" has two): its pattern, and which of the phrase's gaps are its own.
struct FillSeg { std::wstring key; std::vector<int> kindAt; size_t g0 = 0, g1 = 0; };

// Splits the typed phrase at the separator characters (Config). Words that are nothing but ignored characters vanish.
static void MakeSegments(const std::wstring& word, const std::wstring& seps, const Cfg& cfg, std::vector<FillSeg>& segs) {
    segs.clear();
    FillSeg cur;
    size_t g = 0;
    for (wchar_t c : word) {
        int kind = cfg.Gap(c);
        if (kind) {
            cur.key += c;
            cur.kindAt.push_back(kind);
            ++g;
        } else if (seps.find(c) != std::wstring::npos) {
            if (!cur.key.empty()) { cur.g1 = g; segs.push_back(cur); }
            cur = FillSeg();
            cur.g0 = g;
        } else if (!cfg.Ignored(c)) {
            cur.key += c;
            cur.kindAt.push_back(0);
        }
    }
    if (!cur.key.empty()) { cur.g1 = g; segs.push_back(cur); }
}

struct FillModel {
    std::wstring word;                 // the typed word (upper-case) with the gap characters still in place
    std::vector<int> pos;              // where each gap is in `word`
    std::vector<int> kind;             // 1 any letter, 2 vowel, 3 consonant
    std::vector<FillHit> hits;         // dictionary words that fit (green); letters for the gaps only
    std::vector<uint64_t> ranks;       // the same words' places in the alphabetical list of all combinations, ascending
    std::vector<FillSeg> segs;         // the words of a phrase (empty unless there are 2 or more)
    std::vector<std::vector<std::wstring>> segHits;   // per word: dictionary words that fit it, alphabetical
    std::vector<uint64_t> yskip;       // green phrases that are also in the product below, ascending
    const std::vector<Dict>* dicts = nullptr;
    uint64_t total = 0;                // all combinations (stops counting at kBig)
    uint64_t product = 0;              // phrase: every way of choosing one dictionary word per word
    uint64_t potential = 0;            // yellow: the product, less the phrases that are green
    long long rows = 0;                // rows in the list
    int mode = FILL_ALL;
    bool hitsFirst = false;            // all combinations with 2+ gaps: green rows are listed before the alphabetical rest
    bool valid = false;
    std::vector<std::wstring> alpha;   // the dictionary words that fit, alphabetical (only built for the strict A-Z / Z-A order)
    uint64_t n = 0;                    // rows in the natural order, before the Max rows cap (stops counting at kBig)
    bool shuffle = false, sorted = false, desc = false;   // the shuffle and sort buttons
    uint64_t seed = 0;                 // which random order
    bool byScore = false;              // Sort by score: `block` lists the dictionary and potential words, best score first
    std::vector<std::wstring> block;   // their letters for the gaps, in order
    std::vector<unsigned char> blockState;   // ROW_DICT or ROW_POTENTIAL for each
    std::vector<uint64_t> blockRanks;  // all combinations: their places in the alphabetical list, ascending (to skip them)
};
static FillModel g_fill;

// What the order buttons ask for. sort: 0 = natural order (as before), 1 = strictly A-Z, 2 = strictly Z-A. byScore: the
// Sort by score button (dictionary words, then potential words, best total score first).
struct FillOrder { bool shuffle = false; uint64_t seed = 0; int sort = 0; bool byScore = false; };

// The k-th combination in alphabetical order (with desc, in reverse alphabetical order), as the letters for the gaps.
static std::wstring ComboLetters(uint64_t k, const std::vector<int>& kind, bool desc = false) {
    std::wstring s(kind.size(), L'A');
    for (size_t g = kind.size(); g-- > 0;) {
        uint64_t r = (uint64_t)GapRadix(kind[g]), d = k % r;
        s[g] = GapLetters(kind[g])[desc ? r - 1 - d : d];     // reverse order = each digit counted from the other end
        k /= r;
    }
    return s;
}

// The k-th way of choosing one dictionary word per word of the phrase, as the letters for all the gaps.
static std::wstring ProductLetters(uint64_t k, const FillModel& m) {
    std::wstring s(m.kind.size(), L'A');
    for (size_t i = m.segs.size(); i-- > 0;) {
        const std::vector<std::wstring>& hs = m.segHits[i];
        const std::wstring& w = hs[k % hs.size()];
        k /= hs.size();
        for (size_t g = 0; g < w.size(); ++g) s[m.segs[i].g0 + g] = w[g];
    }
    return s;
}

// Yellow: every word of the phrase is in some dictionary (the phrase as a whole may not be).
static bool IsPotential(const FillModel& m, const std::wstring& letters) {
    if (m.segs.size() < 2 || !m.dicts) return false;
    for (const FillSeg& sg : m.segs) {
        std::wstring t = sg.key;
        size_t g = sg.g0;
        for (size_t p = 0; p < t.size(); ++p)
            if (sg.kindAt[p]) t[p] = letters[g++];
        uint64_t h = HashKey(t);
        bool in = false;
        for (const Dict& d : *m.dicts)
            if (std::binary_search(d.hashes.begin(), d.hashes.end(), h)) { in = true; break; }
        if (!in) return false;
    }
    return true;
}

// Row i of the natural order: the typed word with the gaps filled in, and what the row is (plain, green or yellow).
static std::wstring NaturalRow(long long i, int& state) {
    const FillModel& m = g_fill;
    std::wstring letters;
    state = ROW_PLAIN;
    auto green = [&](size_t h) { letters = m.hits[h].letters; state = ROW_DICT; };
    if (m.mode == FILL_DICT) {
        green((size_t)i);
    } else if (m.mode == FILL_POTENTIAL || m.mode == FILL_BOTH) {   // [green rows], then the yellow ones
        size_t g = m.mode == FILL_BOTH ? m.hits.size() : 0;
        if ((size_t)i < g) {
            green((size_t)i);
        } else {
            uint64_t k = (uint64_t)i - g;
            for (uint64_t r : m.yskip) { if (r <= k) ++k; else break; }   // skip phrases already listed as green
            letters = ProductLetters(k, m);
            state = ROW_POTENTIAL;
        }
    } else if (m.hitsFirst) {
        if ((size_t)i < m.hits.size()) {
            green((size_t)i);
        } else {                                              // the alphabetical rest, skipping the words already listed
            uint64_t k = (uint64_t)(i - (long long)m.hits.size());
            for (uint64_t r : m.ranks) { if (r <= k) ++k; else break; }
            letters = ComboLetters(k, m.kind);
            if (IsPotential(m, letters)) state = ROW_POTENTIAL;
        }
    } else {                                                  // one gap: plain alphabetical, green and yellow shown in place
        letters = ComboLetters((uint64_t)i, m.kind);
        if (std::binary_search(m.ranks.begin(), m.ranks.end(), (uint64_t)i)) state = ROW_DICT;
        else if (IsPotential(m, letters)) state = ROW_POTENTIAL;
    }
    std::wstring w = m.word;
    for (size_t g = 0; g < m.pos.size(); ++g) w[(size_t)m.pos[g]] = letters[g];
    return w;
}

// A pseudo-random permutation of 0..n-1 that needs no memory: a 4-round Feistel network over the next even number of
// bits, walked back into range. Every row of even a huge list gets its own, scattered place.
static uint64_t PermIndex(uint64_t i, uint64_t n, uint64_t seed) {
    if (n <= 1) return 0;
    int bits = 2;
    while (bits < 62 && (1ULL << bits) < n) bits += 2;
    int hb = bits / 2;
    uint64_t mask = (1ULL << hb) - 1, x = i;
    do {
        uint64_t l = x >> hb, r = x & mask;
        for (uint64_t round = 1; round <= 4; ++round) {
            uint64_t t = l ^ (Mix(r + seed + round * 0x9E3779B97F4A7C15ULL) & mask);
            l = r;
            r = t;
        }
        x = (l << hb) | r;
    } while (x >= n);
    return x;
}

// Row i once the sort button has been used: strictly alphabetical (A-Z or Z-A), even where the natural order was best
// score first. "All combinations" lists every combination in place (green and yellow rows tinted where they fall);
// the other views list their green words, then their yellow ones, each block alphabetical.
static std::wstring SortedRow(long long i, int& state) {
    const FillModel& m = g_fill;
    std::wstring letters;
    state = ROW_PLAIN;
    auto green = [&](uint64_t k) {
        letters = m.alpha[(size_t)(m.desc ? m.alpha.size() - 1 - k : k)];
        state = ROW_DICT;
    };
    auto yellow = [&](uint64_t k) {                           // the k-th potential word, alphabetical
        if (m.desc && m.potential < kBig) k = m.potential - 1 - k;
        for (uint64_t r : m.yskip) { if (r <= k) ++k; else break; }   // skip phrases already listed as green
        letters = ProductLetters(k, m);
        state = ROW_POTENTIAL;
    };
    if (m.mode == FILL_ALL) {
        letters = ComboLetters((uint64_t)i, m.kind, m.desc);
        if (std::binary_search(m.alpha.begin(), m.alpha.end(), letters)) state = ROW_DICT;
        else if (IsPotential(m, letters)) state = ROW_POTENTIAL;
    } else if (m.mode == FILL_DICT) {
        green((uint64_t)i);
    } else if (m.mode == FILL_POTENTIAL) {
        yellow((uint64_t)i);
    } else if ((size_t)i < m.alpha.size()) {                  // dictionary + potential: the green block, then the yellow block
        green((uint64_t)i);
    } else {
        yellow((uint64_t)i - m.alpha.size());
    }
    std::wstring w = m.word;
    for (size_t g = 0; g < m.pos.size(); ++g) w[(size_t)m.pos[g]] = letters[g];
    return w;
}

// Row i when Sort by score is on: the ranked dictionary and potential words, then (all combinations) the alphabetical rest.
static std::wstring ScoreRow(long long i, int& state) {
    const FillModel& m = g_fill;
    std::wstring letters;
    if ((size_t)i < m.block.size()) {
        letters = m.block[(size_t)i];
        state = m.blockState[(size_t)i];
    } else {                                                  // skip the combinations already listed above
        uint64_t k = (uint64_t)i - m.block.size();
        for (uint64_t r : m.blockRanks) { if (r <= k) ++k; else break; }
        letters = ComboLetters(k, m.kind);
        state = ROW_PLAIN;
    }
    std::wstring w = m.word;
    for (size_t g = 0; g < m.pos.size(); ++g) w[(size_t)m.pos[g]] = letters[g];
    return w;
}

// Row i of the list, in the order the shuffle, sort and Sort by score buttons ask for.
static std::wstring FillRow(long long i, int& state) {
    const FillModel& m = g_fill;
    if (m.shuffle) return NaturalRow((long long)PermIndex((uint64_t)i, m.n, m.seed), state);
    if (m.byScore) return ScoreRow(i, state);
    if (m.sorted) return SortedRow(i, state);
    return NaturalRow(i, state);
}

// The dictionary words that fit one word of a phrase (letters for its own gaps), alphabetical, no repeats.
static void FindSegHits(const FillSeg& sg, const std::vector<Dict>& dicts, std::vector<std::wstring>& out) {
    out.clear();
    size_t L = sg.key.size(), ng = sg.g1 - sg.g0;
    if (ng == 0) {                                            // a word typed in full: is it in a dictionary?
        uint64_t h = HashKey(sg.key);
        for (const Dict& d : dicts)
            if (std::binary_search(d.hashes.begin(), d.hashes.end(), h)) { out.push_back(std::wstring()); break; }
        return;
    }
    std::vector<size_t> at;
    for (size_t p = 0; p < L; ++p)
        if (sg.kindAt[p]) at.push_back(p);
    for (const Dict& d : dicts) {
        if (L >= d.byLen.size()) continue;
        const Dict::Bucket& b = d.byLen[L];
        for (size_t e = 0; e < b.scores.size(); ++e) {
            const wchar_t* t = &b.chars[e * L];
            if (!FitsEntry(t, L, sg.key, sg.kindAt)) continue;
            std::wstring w(ng, L'A');
            for (size_t k = 0; k < ng; ++k) w[k] = t[at[k]];
            out.push_back(std::move(w));
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
}

struct ScoredWord { std::wstring letters; int score; };

// The dictionary words that fit one word of a phrase, each with its best score (a word in several dictionaries, or twice,
// counts once; no score counts as 0), best score first, ties alphabetical.
static void FindSegHitsScored(const FillSeg& sg, const std::vector<Dict>& dicts, std::vector<ScoredWord>& out) {
    out.clear();
    size_t L = sg.key.size(), ng = sg.g1 - sg.g0;
    if (ng == 0) {                                            // a word typed in full: its best score, if it is a word
        uint64_t h = HashKey(sg.key);
        bool found = false;
        int best = 0;
        for (const Dict& d : dicts) {
            auto it = std::lower_bound(d.hashes.begin(), d.hashes.end(), h);
            if (it == d.hashes.end() || *it != h) continue;
            found = true;
            best = std::max(best, d.scores[(size_t)(it - d.hashes.begin())]);
        }
        if (found) out.push_back(ScoredWord{ std::wstring(), best });
        return;
    }
    std::vector<size_t> at;
    for (size_t p = 0; p < L; ++p)
        if (sg.kindAt[p]) at.push_back(p);
    for (const Dict& d : dicts) {
        if (L >= d.byLen.size()) continue;
        const Dict::Bucket& b = d.byLen[L];
        for (size_t e = 0; e < b.scores.size(); ++e) {
            const wchar_t* t = &b.chars[e * L];
            if (!FitsEntry(t, L, sg.key, sg.kindAt)) continue;
            std::wstring w(ng, L'A');
            for (size_t k = 0; k < ng; ++k) w[k] = t[at[k]];
            out.push_back(ScoredWord{ std::move(w), std::max(b.scores[e], 0) });
        }
    }
    std::sort(out.begin(), out.end(), [](const ScoredWord& a, const ScoredWord& b) {
        return a.letters != b.letters ? a.letters < b.letters : a.score > b.score;
    });
    out.erase(std::unique(out.begin(), out.end(), [](const ScoredWord& a, const ScoredWord& b) { return a.letters == b.letters; }),
              out.end());
    std::sort(out.begin(), out.end(), [](const ScoredWord& a, const ScoredWord& b) {
        return a.score != b.score ? a.score > b.score : a.letters < b.letters;
    });
}

// The place of a combination (its letters for the gaps) in the alphabetical list of all combinations.
static uint64_t RankOf(const std::wstring& letters, const std::vector<int>& kind) {
    uint64_t r = 0;
    for (size_t g = 0; g < letters.size(); ++g) {
        const wchar_t* set = GapLetters(kind[g]);
        r = SatAdd(SatMul(r, (uint64_t)GapRadix(kind[g])), (uint64_t)(wcschr(set, letters[g]) - set));
    }
    return r;
}

// Appends the `want` best potential words to m.block. A potential word is one dictionary word per word of the phrase; its
// score is the sum of those words' own best scores. The best are found one at a time (a priority queue over the words'
// score-ordered lists), so billions of combinations are never made. Equal totals go to the word that is earlier in each
// word's own list (best score, then alphabetical). A phrase that is also a whole dictionary word is green, not yellow.
static void TopPotential(FillModel& m, uint64_t want) {
    size_t ns = m.segs.size();
    std::vector<std::vector<ScoredWord>> lists(ns);
    for (size_t i = 0; i < ns; ++i) {
        FindSegHitsScored(m.segs[i], *m.dicts, lists[i]);
        if (lists[i].empty()) return;
    }
    struct Node { long long sum; std::vector<uint32_t> idx; size_t pos; };
    auto later = [](const Node& a, const Node& b) { return a.sum != b.sum ? a.sum < b.sum : a.idx > b.idx; };
    std::priority_queue<Node, std::vector<Node>, decltype(later)> heap(later);
    Node root{ 0, std::vector<uint32_t>(ns, 0), 0 };
    for (size_t i = 0; i < ns; ++i) root.sum += lists[i][0].score;
    heap.push(root);
    uint64_t got = 0;
    while (got < want && !heap.empty()) {
        Node nd = heap.top();
        heap.pop();
        std::wstring letters(m.kind.size(), L'A');
        for (size_t i = 0; i < ns; ++i) {
            const std::wstring& w = lists[i][nd.idx[i]].letters;
            for (size_t g = 0; g < w.size(); ++g) letters[m.segs[i].g0 + g] = w[g];
        }
        if (!std::binary_search(m.alpha.begin(), m.alpha.end(), letters)) {
            m.block.push_back(letters);
            m.blockState.push_back(ROW_POTENTIAL);
            ++got;
        }
        for (size_t i = nd.pos; i < ns; ++i) {                // each combination is reached once, from one parent
            if (nd.idx[i] + 1 >= lists[i].size()) continue;
            Node ch = nd;
            ++ch.idx[i];
            ch.sum += lists[i][ch.idx[i]].score - lists[i][nd.idx[i]].score;
            ch.pos = i;
            heap.push(ch);
        }
    }
}

// Works out the list for a pattern: dictionary words that fit (green), for a phrase the yellow ones too,
// how many combinations there are, and how many rows to show.
static void BuildFill(const std::wstring& word, const std::vector<int>& gapWord, const std::vector<int>& gapKey,
                      const std::vector<int>& gapKind, const std::wstring& key, const std::vector<FillSeg>& segs,
                      const std::vector<Dict>& dicts, int mode, long long cap, const FillOrder& ord) {
    FillModel& m = g_fill;
    m = FillModel();
    m.word = word;
    m.pos = gapWord;
    m.kind = gapKind;
    m.mode = mode;
    m.dicts = &dicts;
    m.hitsFirst = mode == FILL_ALL && gapWord.size() >= 2;

    size_t L = key.size();
    std::vector<int> kindAt(L, 0);
    for (size_t g = 0; g < gapKey.size(); ++g) kindAt[(size_t)gapKey[g]] = gapKind[g];
    std::vector<FillHit> found;
    for (const Dict& d : dicts) {                             // only entries of the same length can fit
        if (L >= d.byLen.size()) continue;
        const Dict::Bucket& b = d.byLen[L];
        for (size_t e = 0; e < b.scores.size(); ++e) {
            const wchar_t* t = &b.chars[e * L];
            if (!FitsEntry(t, L, key, kindAt)) continue;
            FillHit f;
            f.letters.resize(gapKey.size());
            for (size_t g = 0; g < gapKey.size(); ++g) f.letters[g] = t[(size_t)gapKey[g]];
            f.score = b.scores[e];
            f.rank = 0;
            found.push_back(std::move(f));
        }
    }
    std::sort(found.begin(), found.end(), [](const FillHit& a, const FillHit& b) {   // same word in several dictionaries:
        return a.letters != b.letters ? a.letters < b.letters : a.score > b.score;   // keep the best score
    });
    found.erase(std::unique(found.begin(), found.end(), [](const FillHit& a, const FillHit& b) {
        return a.letters == b.letters;
    }), found.end());
    if (ord.sort || ord.byScore)                              // strict A-Z / Z-A and Sort by score need the words alphabetical too
        for (const FillHit& f : found) m.alpha.push_back(f.letters);
    for (FillHit& f : found) {                                // place in the alphabetical list of all combinations
        uint64_t r = 0;
        for (size_t g = 0; g < f.letters.size(); ++g) {
            const wchar_t* set = GapLetters(gapKind[g]);
            r = SatAdd(SatMul(r, (uint64_t)GapRadix(gapKind[g])), (uint64_t)(wcschr(set, f.letters[g]) - set));
        }
        f.rank = r;
        m.ranks.push_back(r);
    }
    if (gapWord.size() >= 2)                                  // several gaps: best scores first
        std::sort(found.begin(), found.end(), [](const FillHit& a, const FillHit& b) {
            return a.score != b.score ? a.score > b.score : a.letters < b.letters;
        });
    m.hits = std::move(found);

    if (segs.size() >= 2) {                                   // a phrase: yellow = every word is a dictionary word
        m.segs = segs;
        m.segHits.resize(segs.size());
        m.product = 1;
        for (size_t i = 0; i < segs.size() && m.product; ++i) {
            FindSegHits(segs[i], dicts, m.segHits[i]);
            m.product = m.segHits[i].empty() ? 0 : SatMul(m.product, (uint64_t)m.segHits[i].size());
        }
        if (m.product) {
            for (const FillHit& f : m.hits) {                 // green phrases also in the product are listed once, as green
                uint64_t r = 0;
                bool in = true;
                for (size_t i = 0; i < segs.size() && in; ++i) {
                    const std::vector<std::wstring>& hs = m.segHits[i];
                    std::wstring w = f.letters.substr(segs[i].g0, segs[i].g1 - segs[i].g0);
                    auto it = std::lower_bound(hs.begin(), hs.end(), w);
                    if (it == hs.end() || *it != w) in = false;
                    else r = SatAdd(SatMul(r, (uint64_t)hs.size()), (uint64_t)(it - hs.begin()));
                }
                if (in) m.yskip.push_back(r);
            }
            std::sort(m.yskip.begin(), m.yskip.end());
            m.yskip.erase(std::unique(m.yskip.begin(), m.yskip.end()), m.yskip.end());
        }
        m.potential = m.product >= kBig ? kBig : m.product - (uint64_t)m.yskip.size();
    }

    m.total = 1;
    for (int k : gapKind) m.total = SatMul(m.total, (uint64_t)GapRadix(k));
    uint64_t n = mode == FILL_ALL ? m.total
               : mode == FILL_DICT ? (uint64_t)m.hits.size()
               : mode == FILL_POTENTIAL ? m.potential
               : SatAdd((uint64_t)m.hits.size(), m.potential);
    m.n = n;
    m.shuffle = ord.shuffle;
    m.seed = ord.seed;
    m.sorted = ord.sort != 0;
    m.desc = ord.sort == 2;
    m.rows = (long long)std::min<uint64_t>(n, (uint64_t)(cap < 1 ? 1 : cap));
    if (ord.byScore && !ord.shuffle) {                        // Sort by score: dictionary words, then potential words
        m.byScore = true;
        uint64_t P = (mode == FILL_DICT || m.segs.size() < 2) ? 0 : m.potential;
        uint64_t want = std::min<uint64_t>(P, std::min<uint64_t>((uint64_t)(cap < 1 ? 1 : cap), kScoreMax));
        if (mode != FILL_POTENTIAL) {
            std::vector<const FillHit*> g;
            for (const FillHit& f : m.hits) g.push_back(&f);
            std::sort(g.begin(), g.end(), [](const FillHit* a, const FillHit* b) {
                return a->score != b->score ? a->score > b->score : a->letters < b->letters;
            });
            for (const FillHit* f : g) {
                m.block.push_back(f->letters);
                m.blockState.push_back(ROW_DICT);
            }
        }
        if (want) TopPotential(m, want);
        if (mode == FILL_ALL) {                               // the alphabetical rest skips what is already listed
            for (const std::wstring& l : m.block) m.blockRanks.push_back(RankOf(l, gapKind));
            std::sort(m.blockRanks.begin(), m.blockRanks.end());
        }
        if (want < P) m.rows = std::min<long long>(m.rows, (long long)m.block.size());   // the list ends where the ranking does
    }
    m.valid = true;
}

// ---- Split: where do 1, 2 or 3 extra spaces make the most potential words? -----------------------------
// How many dictionary words fit one word of a phrase, each distinct word once (counting only, so it is cheap).
static uint64_t SegCount(const FillSeg& sg, const std::vector<Dict>& dicts) {
    size_t L = sg.key.size();
    if (sg.g1 == sg.g0) {                                     // typed in full: is it a dictionary word?
        uint64_t h = HashKey(sg.key);
        for (const Dict& d : dicts)
            if (std::binary_search(d.hashes.begin(), d.hashes.end(), h)) return 1;
        return 0;
    }
    uint64_t n = 0;
    for (const Dict& d : dicts) {
        if (L >= d.byLen.size()) continue;
        const Dict::Bucket& b = d.byLen[L];
        for (size_t e = 0; e < b.scores.size(); ++e)
            if (!(e < b.dup.size() && b.dup[e]) && FitsEntry(&b.chars[e * L], L, sg.key, sg.kindAt)) ++n;
    }
    return n;
}

// The key and gap tables for a typed word, as Update() makes them.
static void ParseFillWord(const std::wstring& word, const Cfg& cfg, std::wstring& key, std::vector<int>& gapWord,
                          std::vector<int>& gapKey, std::vector<int>& gapKind) {
    key.clear();
    gapWord.clear();
    gapKey.clear();
    gapKind.clear();
    for (size_t i = 0; i < word.size(); ++i) {
        wchar_t c = word[i];
        if (int g = cfg.Gap(c)) {
            gapWord.push_back((int)i);
            gapKey.push_back((int)key.size());
            gapKind.push_back(g);
            key += c;
        } else if (!cfg.Ignored(c)) {
            key += c;
        }
    }
}

// The exact numbers the Fill tab would show for a phrase: dictionary words that fit, and potential (yellow) words.
// This builds g_fill, so callers rebuild the Fill list (Update) afterwards.
static void FillCounts(const std::wstring& phrase, const std::wstring& seps, const Cfg& cfg, const std::vector<Dict>& dicts,
                       uint64_t& greens, uint64_t& potential) {
    std::wstring key;
    std::vector<int> gapWord, gapKey, gapKind;
    std::vector<FillSeg> segs;
    ParseFillWord(phrase, cfg, key, gapWord, gapKey, gapKind);
    MakeSegments(phrase, seps, cfg, segs);
    BuildFill(phrase, gapWord, gapKey, gapKind, key, segs, dicts, FILL_POTENTIAL, 1, FillOrder());
    greens = g_fill.hits.size();
    potential = g_fill.potential;
}

struct SplitBest { std::wstring word; uint64_t potential = 0; size_t tried = 0; int added = 0; };

// Adds the first separator character in every place, 1, then 2, then 3 at a time (up to maxAdd), and returns the phrase
// with the most potential words. The word as typed is a candidate too, so it is never made worse. A tie goes to the
// fewest added separators, then to the leftmost. A separator goes just before a character that counts (a gap or a letter),
// and never next to an existing separator. Every word an added separator makes (on either side of it) must have at least
// minLen letters; words already separated in the typed phrase are left alone. potential 0 means no split gives any.
static SplitBest BestSplit(const std::wstring& word, const std::wstring& seps, const Cfg& cfg, const std::vector<Dict>& dicts,
                           int maxAdd, size_t minLen) {
    SplitBest best;
    best.word = word;
    if (seps.empty()) return best;
    auto counted = [&](wchar_t c) { return cfg.Gap(c) != 0 || (!cfg.Ignored(c) && seps.find(c) == std::wstring::npos); };
    std::vector<size_t> places;
    bool seen = false;
    for (size_t p = 0; p < word.size(); ++p) {
        if (seen && counted(word[p]) && seps.find(word[p - 1]) == std::wstring::npos) places.push_back(p);
        if (counted(word[p])) seen = true;
    }

    struct Cand { std::wstring text; uint64_t product; };
    std::vector<Cand> cands;                                  // [0] is the word as typed
    std::vector<size_t> pick;
    auto longEnough = [&](size_t k) {                         // do the k chosen separators leave words of minLen letters?
        if (minLen <= 1) return true;
        size_t used = 0, n = 0;
        bool touched = false;                                 // the word being counted starts at an added separator
        for (size_t p = 0; p <= word.size(); ++p) {
            bool cut = used < k && p < word.size() && places[pick[used]] == p;
            bool sep = p < word.size() && seps.find(word[p]) != std::wstring::npos;
            if (p == word.size() || cut || sep) {             // a word ends here
                if ((touched || cut) && n < minLen) return false;
                n = 0;
                touched = cut;
                if (cut) ++used;
            }
            if (p < word.size() && !sep && counted(word[p])) ++n;
        }
        return true;
    };
    for (size_t k = 0; k <= (size_t)maxAdd && k <= places.size(); ++k) {
        pick.resize(k);
        for (size_t i = 0; i < k; ++i) pick[i] = i;
        for (;;) {
            std::wstring t;
            size_t used = 0;
            for (size_t p = 0; p < word.size(); ++p) {
                if (used < k && places[pick[used]] == p) { t += seps[0]; ++used; }
                t += word[p];
            }
            if (longEnough(k)) cands.push_back(Cand{ t, 0 });
            size_t i = k;                                     // the next combination, in order
            while (i > 0 && pick[i - 1] == places.size() - k + i - 1) --i;
            if (i == 0) break;
            ++pick[i - 1];
            for (size_t j = i; j < k; ++j) pick[j] = pick[j - 1] + 1;
        }
    }
    best.tried = cands.size() - 1;

    // Cheap score for every candidate: the ways of choosing one dictionary word per word (the potential words plus any
    // that are also whole dictionary words).
    std::unordered_map<std::wstring, uint64_t> counts;
    std::vector<FillSeg> segs;
    uint64_t top = 0;
    for (Cand& c : cands) {
        MakeSegments(c.text, seps, cfg, segs);
        if (segs.size() < 2) continue;
        uint64_t prod = 1;
        for (const FillSeg& sg : segs) {
            auto it = counts.find(sg.key);
            if (it == counts.end()) it = counts.emplace(sg.key, SegCount(sg, dicts)).first;
            if (it->second == 0) { prod = 0; break; }
            prod = SatMul(prod, it->second);
        }
        c.product = prod;
        top = std::max(top, prod);
    }
    if (top == 0) return best;

    // The exact count takes off the phrases that are also dictionary words (the same few for every split), so only
    // candidates within that many of the best score can win. Those are counted exactly, as the Fill tab will show them.
    uint64_t greens = 0, base = 0;
    FillCounts(word, seps, cfg, dicts, greens, base);
    uint64_t floor = top > greens ? top - greens : 1;
    std::vector<size_t> finalists;
    for (size_t i = 0; i < cands.size(); ++i)
        if (cands[i].product && cands[i].product >= floor) finalists.push_back(i);
    if (finalists.size() > 64) {                              // keep the 64 highest scores (ties: leftmost first)
        std::stable_sort(finalists.begin(), finalists.end(), [&](size_t a, size_t b) { return cands[a].product > cands[b].product; });
        finalists.resize(64);
    }
    size_t bestIdx = (size_t)-1;
    uint64_t bestPot = 0;
    for (size_t i : finalists) {
        uint64_t g = 0, pot = base;
        if (i != 0) FillCounts(cands[i].text, seps, cfg, dicts, g, pot);
        if (pot > bestPot || (pot == bestPot && pot && i < bestIdx)) { bestPot = pot; bestIdx = i; }
    }
    if (bestIdx == (size_t)-1) return best;
    best.word = cands[bestIdx].text;
    best.potential = bestPot;
    best.added = (int)(best.word.size() - word.size());
    return best;
}

// ---- Random: a random dictionary word, any length, one length, or fitting a pattern ------------------
struct RandSpec {
    size_t len = 0;                    // letters wanted (0 = any length)
    int score = -1;                    // only words with this score (-1 = any score)
    bool pattern = false;              // fit `key`, where kindAt marks the gaps (as on the Fill tab)
    std::wstring key;
    std::vector<int> kindAt;
};
struct RandPick { bool ok = false; size_t dict = 0, len = 0, entry = 0; };

// A pattern (a word with gaps) decides the length itself; without one, the Length box does.
static RandSpec MakeRandSpec(const std::wstring& key, const std::vector<int>& gapKey, const std::vector<int>& gapKind,
                             size_t userLen, int score) {
    RandSpec sp;
    sp.score = score;
    if (gapKey.empty()) {
        sp.len = userLen;
    } else {
        sp.pattern = true;
        sp.key = key;
        sp.len = key.size();
        sp.kindAt.assign(key.size(), 0);
        for (size_t g = 0; g < gapKey.size(); ++g) sp.kindAt[(size_t)gapKey[g]] = gapKind[g];
    }
    return sp;
}

// Does entry e of a bucket (L characters each) fit the spec? Only the best copy of a word counts: a word listed
// in two dictionaries (or twice in one) is one word, exactly as in Fill's green list.
static bool EntryFitsSpec(const Dict::Bucket& b, size_t L, size_t e, const RandSpec& sp) {
    if (e < b.dup.size() && b.dup[e]) return false;
    if (sp.score >= 0 && b.scores[e] != sp.score) return false;
    return !sp.pattern || FitsEntry(&b.chars[e * L], L, sp.key, sp.kindAt);
}

static uint64_t BucketFits(const Dict::Bucket& b, size_t L, const RandSpec& sp) {
    uint64_t n = 0;
    for (size_t e = 0; e < b.scores.size(); ++e)
        if (EntryFitsSpec(b, L, e, sp)) ++n;
    return n;
}

static void RandRange(const Dict& d, const RandSpec& sp, size_t& lo, size_t& hi) {   // lengths to look at
    lo = sp.len ? sp.len : 1;
    hi = sp.len ? sp.len : kMaxKey;
    if (d.byLen.empty()) { lo = 1; hi = 0; return; }
    if (hi >= d.byLen.size()) hi = d.byLen.size() - 1;
}

static uint64_t DictFits(const Dict& d, const RandSpec& sp) {
    size_t lo, hi;
    RandRange(d, sp, lo, hi);
    uint64_t n = 0;
    for (size_t L = lo; L <= hi; ++L) n += BucketFits(d.byLen[L], L, sp);
    return n;
}

// A random fitting word: every distinct word that fits is equally likely, so a dictionary turns up in
// proportion to how many words it contributes.
static RandPick PickRandom(const std::vector<Dict>& dicts, const RandSpec& sp, std::mt19937_64& rng) {
    RandPick r;
    std::vector<uint64_t> cnt(dicts.size());
    uint64_t total = 0;
    for (size_t i = 0; i < dicts.size(); ++i) {
        cnt[i] = DictFits(dicts[i], sp);
        total += cnt[i];
    }
    if (total == 0) return r;
    uint64_t k = std::uniform_int_distribution<uint64_t>(0, total - 1)(rng);
    size_t di = 0;
    while (k >= cnt[di]) k -= cnt[di++];                      // k is now the place within dictionary di
    size_t lo, hi;
    RandRange(dicts[di], sp, lo, hi);
    for (size_t L = lo; L <= hi; ++L) {
        const Dict::Bucket& b = dicts[di].byLen[L];
        for (size_t e = 0; e < b.scores.size(); ++e) {
            if (!EntryFitsSpec(b, L, e, sp)) continue;
            if (k == 0) { r.ok = true; r.dict = di; r.len = L; r.entry = e; return r; }
            --k;
        }
    }
    return r;
}

// Every dictionary that has this word (normalized), with the score it gives it.
static std::vector<std::pair<size_t, int>> DictsWithWord(const std::vector<Dict>& dicts, const std::wstring& norm) {
    std::vector<std::pair<size_t, int>> out;
    uint64_t h = HashKey(norm);
    for (size_t i = 0; i < dicts.size(); ++i) {
        auto it = std::lower_bound(dicts[i].hashes.begin(), dicts[i].hashes.end(), h);
        if (it != dicts[i].hashes.end() && *it == h)
            out.push_back({ i, dicts[i].scores[(size_t)(it - dicts[i].hashes.begin())] });
    }
    return out;
}

// The entry as written in the dictionary file (spaces, punctuation, capitals), found by its normalized form.
static bool OriginalEntry(const std::wstring& path, const std::wstring& norm, const Cfg& cfg, std::wstring& out) {
    std::wstring t, nk;
    if (!ReadText(path, t)) return false;
    size_t pos = 0;
    while (pos < t.size()) {
        size_t nl = t.find(L'\n', pos);
        if (nl == std::wstring::npos) nl = t.size();
        size_t end = nl;
        if (end > pos && t[end - 1] == L'\r') --end;
        size_t stop = end;                                    // the entry is everything before the last ';'
        for (size_t i = end; i > pos; --i)
            if (t[i - 1] == L';') { stop = i - 1; break; }
        NormEntry(t.data() + pos, stop - pos, cfg, nk);
        if (nk == norm) {
            size_t a = pos, b = stop;
            while (a < b && (t[a] == L' ' || t[a] == L'\t')) ++a;
            while (b > a && (t[b - 1] == L' ' || t[b - 1] == L'\t')) --b;
            out = t.substr(a, b - a);
            return true;
        }
        pos = nl + 1;
    }
    return false;
}

// "Max rows" box: default 2000, never more than the largest int (the most a list box can hold).
static long long FillCap() {
    wchar_t b[16];
    GetWindowTextW(hFillCap, b, 16);
    long long v = b[0] ? wcstoll(b, nullptr, 10) : 2000;
    return v < 1 ? 2000 : std::min<long long>(v, INT_MAX);
}

static std::wstring Num(unsigned long long v) {             // 1234567 -> 1,234,567
    std::wstring s = std::to_wstring(v);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, 1, L',');
    return s;
}

// Double-click on a Fill row that isn't in a dictionary: offer to add it to the default dictionary.
static void AddFromFill(long long i) {
    const Cfg& cfg = CurCfg();
    if (g_loading || g_loadedSig != cfg.sig || !g_fill.valid || i < 0 || i >= g_fill.rows) return;
    int state = ROW_PLAIN;
    std::wstring w = FillRow(i, state);
    if (state == ROW_DICT) return;                            // already in a dictionary (yellow and plain rows are not)
    wchar_t nb[MAX_PATH], sb[16];
    GetWindowTextW(hAddDict, nb, MAX_PATH);
    GetWindowTextW(hAddScore, sb, 16);                        // the Config default score
    std::wstring name = DictFileName(nb);
    long long score = sb[0] ? wcstoll(sb, nullptr, 10) : 50;
    std::wstring q = L"Add \"" + w + L"\" to " + name.substr(0, name.size() - 5) + L" with a score of " +
                     std::to_wstring(score) + L"?";
    if (MessageBoxW(g_hwnd, q.c_str(), L"CrossTools", MB_YESNO | MB_ICONQUESTION) == IDYES) AddEntry(w, score);
    SetFocus(hInput);
}

// The Split button: leaves the word bar split where 1, 2 or 3 added spaces make the most potential words.
static void SplitWord() {
    const Cfg& cfg = CurCfg();
    wchar_t buf[64], spb[8];
    GetWindowTextW(hInput, buf, 64);
    GetWindowTextW(hSepEdit, spb, 8);                         // word separator characters (Config)
    std::wstring word(buf);
    CharUpperBuffW(&word[0], (DWORD)word.size());
    bool gap = false;
    for (wchar_t c : word) gap = gap || cfg.Gap(c) != 0;
    const wchar_t* msg = nullptr;
    if (!spb[0]) msg = L"No word separator is set (see Config)";
    else if (g_loading || g_loadedSig != cfg.sig) msg = L"Dictionaries loading...";
    else if (g_dicts.empty()) msg = L"No dictionaries loaded (see Config)";
    else if (!gap) msg = L"Type a word with a blank to split, e.g. ______";
    if (msg) { SetWindowTextW(hStatus, msg); return; }

    wchar_t mb[8];
    GetWindowTextW(hMinSplit, mb, 8);                         // Config: minimum word length for Split (empty or 0 = 1)
    long minLen = wcstol(mb, nullptr, 10);
    if (minLen < 1) minLen = 1;
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    SplitBest r = BestSplit(word, spb, cfg, g_dicts, 3, (size_t)minLen);
    SetCursor(old);

    auto count = [](uint64_t v) { return v >= kBig ? std::wstring(L"far too many") : Num(v); };
    std::wstring st;
    bool changed = r.potential != 0 && r.word != word;
    if (changed) {
        SendMessageW(hInput, EM_LIMITTEXT, 64, 0);            // the added spaces may go past the word bar's own limit
        SendMessageW(hInput, EM_SETSEL, 0, -1);
        SendMessageW(hInput, EM_REPLACESEL, TRUE, (LPARAM)r.word.c_str());   // undoable with Ctrl+Z; the change runs Update
        SendMessageW(hInput, EM_LIMITTEXT, 40, 0);
        st = count(r.potential) + (r.potential == 1 ? L" potential word with " : L" potential words with ") +
             std::to_wstring(r.added) + (r.added == 1 ? L" space added (" : L" spaces added (") + Num(r.tried) + L" splits tried)";
    } else {
        Update();                                             // the search reused the Fill model: rebuild it for the word
        st = r.tried == 0 ? L"No way to split this word into words of at least " + std::to_wstring(minLen) + L" letters (see Config)"
          : r.potential == 0 ? L"No split of this word makes potential words (" + Num(r.tried) + L" tried)"
                              : L"Already the best split: " + count(r.potential) + L" potential words (" + Num(r.tried) +
                                    L" tried)";
    }
    SetWindowTextW(hStatus, st.c_str());
}

// ---- Random tab (screen side) ----
static std::mt19937_64 g_rng;                 // seeded at startup
static RandSpec g_randSpec;                   // what the Random tab picks from; kept current by UpdateRandom
static bool g_randOk = false;                 // dictionaries are loaded and match the current rules
static std::wstring g_randSig;                // identifies that spec: a word on show is cleared when it changes
static std::wstring g_userLen;                // what was typed in the Length box (it shows the pattern's length while there is one)
static bool g_settingLen = false;             // we are changing the Length box ourselves

static uint64_t RandomSeed() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return ((uint64_t)q.QuadPart * 6364136223846793005ULL) ^ (uint64_t)GetTickCount() ^
           ((uint64_t)GetCurrentProcessId() << 32);
}

// Keeps the Random tab in step with the word bar: the pattern (if any), the Length box, how many words fit.
static void UpdateRandom(const Cfg& cfg, const std::wstring& key, const std::vector<int>& gapKey,
                         const std::vector<int>& gapKind, bool hasText, bool dictsReady) {
    bool pattern = !gapKey.empty();
    g_settingLen = true;                                      // with a pattern the length is the pattern's own
    EnableWindow(hRandLen, pattern ? FALSE : TRUE);
    wchar_t cur[8];
    GetWindowTextW(hRandLen, cur, 8);
    std::wstring want = pattern ? std::to_wstring(key.size()) : g_userLen;
    if (want != cur) SetWindowTextW(hRandLen, want.c_str());
    g_settingLen = false;

    wchar_t scb[8];
    GetWindowTextW(hRandScore, scb, 8);                       // empty = any score
    g_randSpec = MakeRandSpec(key, gapKey, gapKind, (size_t)std::max(0L, wcstol(g_userLen.c_str(), nullptr, 10)),
                              scb[0] ? (int)wcstol(scb, nullptr, 10) : -1);
    g_randOk = dictsReady && !g_dicts.empty();
    std::wstring sig = (pattern ? key : std::wstring()) + L"|" + std::to_wstring(g_randSpec.len) + L"|" + std::to_wstring(g_randSpec.score) + L"|" + cfg.sig;
    if (sig != g_randSig) {                                   // the word on show no longer answers the question
        g_randSig = sig;
        SetWindowTextW(hRandWord, L"");
        SetWindowTextW(hRandFrom, L"");
        InvalidateRect(hRandWord, nullptr, TRUE);
        InvalidateRect(hRandFrom, nullptr, TRUE);
    }

    std::wstring st;
    if (!dictsReady) {
        st = L"Loading dictionaries...";
    } else if (g_dicts.empty()) {
        st = L"No dictionaries loaded (see Config)";
    } else {
        uint64_t total = 0;
        for (const Dict& d : g_dicts) total += DictFits(d, g_randSpec);
        std::wstring n = Num(total);
        std::wstring letters = std::to_wstring(g_randSpec.len) + (g_randSpec.len == 1 ? L" letter" : L" letters");
        bool hasScore = g_randSpec.score >= 0, hasLen = !pattern && g_randSpec.len > 0;
        if (pattern) st = total ? n + (total == 1 ? L" word fits this pattern" : L" words fit this pattern") : std::wstring(L"No words fit this pattern");
        else if (hasLen) st = total ? n + (total == 1 ? L" word" : L" words") + L" with " + letters : L"No words with " + letters;
        else st = total ? n + (total == 1 ? L" word" : L" words") : std::wstring(hasScore ? L"No words" : L"No words in the dictionaries");
        if (hasScore) st += (hasLen ? L" and a score of " : L" with a score of ") + std::to_wstring(g_randSpec.score);
        if (!pattern && total && (hasText || (!hasLen && !hasScore))) {   // how to narrow it down
            wchar_t g = !cfg.P.empty() ? cfg.P[0] : !cfg.V.empty() ? cfg.V[0] : !cfg.K.empty() ? cfg.K[0] : L'_';
            st += hasText ? std::wstring(L" (the word bar has no blank, so it isn't used)")
                          : std::wstring(L". Put a pattern like C") + g + L"T in the word bar to narrow it down.";
        }
    }
    SetWindowTextW(hRandStatus, st.c_str());
}

// The Pick button (and Enter in the word bar on this tab).
static void PickRandomWord() {
    const Cfg& cfg = CurCfg();
    if (!g_randOk || g_loading || g_loadedSig != cfg.sig) return;
    RandPick p = PickRandom(g_dicts, g_randSpec, g_rng);
    if (!p.ok) return;                                        // the status line already says nothing fits
    const Dict& d = g_dicts[p.dict];
    const Dict::Bucket& b = d.byLen[p.len];
    std::wstring norm(b.chars.data() + p.entry * p.len, p.len), shown;
    if (!OriginalEntry(d.path, norm, cfg, shown)) shown = norm;   // the dictionary's own spelling, else the letters
    CharUpperBuffW(&shown[0], (DWORD)shown.size());
    SendMessageW(hRandWord, WM_SETFONT, (WPARAM)(shown.size() <= 16 ? hRandFont : shown.size() <= 34 ? hFont : hSmallFont), TRUE);
    SetWindowTextW(hRandWord, shown.c_str());
    std::wstring from = L"From dictionary: ";                 // every dictionary that has the word
    bool first = true, showScore = Checked(hShowScore);
    for (const std::pair<size_t, int>& dw : DictsWithWord(g_dicts, norm)) {
        if (!first) from += L", ";
        first = false;
        from += g_dicts[dw.first].name;
        if (showScore && dw.second >= 0) from += L" (" + std::to_wstring(dw.second) + L")";
    }
    SetWindowTextW(hRandFrom, from.c_str());
    InvalidateRect(hRandWord, nullptr, TRUE);
    InvalidateRect(hRandFrom, nullptr, TRUE);
}

// Found-word mode: sets the word's score in every loaded dictionary that contains it, then reloads.
static void ChangeWordScore() {
    wchar_t sb[16];
    GetWindowTextW(hScore, sb, 16);
    if (!sb[0] || g_foundKey.empty()) return;
    int score = (int)wcstol(sb, nullptr, 10);
    const Cfg& cfg = CurCfg();
    uint64_t hk = HashKey(g_foundKey);
    size_t total = 0;
    std::wstring failed;
    for (const Dict& d : g_dicts) {
        if (!std::binary_search(d.hashes.begin(), d.hashes.end(), hk)) continue;
        std::string in;
        size_t n = 0;
        if (!ReadBytes(d.path, in)) { failed += L"\n" + d.name; continue; }
        std::string out = SetWordScoreBytes(in, cfg, hk, score, n);
        if (n == 0) continue;                                 // nothing to change in this file
        std::wstring tmp = d.path + L".tmp";
        if (!WriteBytes(tmp, out) || !MoveFileExW(tmp.c_str(), d.path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            DeleteFileW(tmp.c_str());
            failed += L"\n" + d.name;
            continue;
        }
        total += n;
    }
    if (!failed.empty())
        MessageBoxW(g_hwnd, (L"Could not update:" + failed).c_str(), L"CrossTools", MB_OK | MB_ICONWARNING);
    else if (total == 0)
        MessageBoxW(g_hwnd, L"Nothing to change: the score is already that, or the entry has no whole-number score.",
                    L"CrossTools", MB_OK | MB_ICONINFORMATION);
    StartReload();
}

// ---- main update -----------------------------------------------------------------------------
enum { RES_NONE, RES_FOUND, RES_MISSING, RES_INFO };

static void UpdateAddButton() {
    static std::wstring last;
    wchar_t nb[MAX_PATH];
    GetWindowTextW(hAddDict, nb, MAX_PATH);
    std::wstring name = DictFileName(nb);
    std::wstring label = g_scoreMode == 2 ? std::wstring(L"Update score") : L"Add to " + name.substr(0, name.size() - 5);
    if (label != last) { SetWindowTextW(hAdd, label.c_str()); last = label; }
    static std::wstring lastDump;                 // the Dictionaries tab's dump button names the same file
    std::wstring dump = L"Dump " + name.substr(0, name.size() - 5);
    if (dump != lastDump) { SetWindowTextW(hDump, dump.c_str()); lastDump = dump; }
    int sh = (g_scoreMode != 0 && TabCtrl_GetCurSel(hTab) == TAB_COUNT) ? SW_SHOW : SW_HIDE;
    for (HWND c : { hAdd, hLblScore, hScore }) ShowWindow(c, sh);
}

static void Update() {
    if (!g_ready) return;
    wchar_t buf[64];
    GetWindowTextW(hInput, buf, 64);
    const Cfg& cfg = CurCfg();

    std::wstring word(buf);                       // keeps spaces and punctuation as typed
    CharUpperBuffW(&word[0], (DWORD)word.size());
    int blanks = 0, counted = 0;                  // blanks = gap characters (blank or wildcard)
    std::wstring key;                             // normalized form used for dictionary lookup
    std::vector<int> gapWord, gapKey, gapKind;    // where each gap is in the word and in the key, and its kind
    for (size_t i = 0; i < word.size(); ++i) {
        wchar_t c = word[i];
        if (int g = cfg.Gap(c)) {
            gapWord.push_back((int)i);
            gapKey.push_back((int)key.size());
            gapKind.push_back(g);
            ++blanks;
            ++counted;
            key += c;
        } else if (!cfg.Ignored(c)) { ++counted; key += c; }
    }

    // Letter count tab.
    SetWindowTextW(hCount, std::to_wstring(counted).c_str());
    SetWindowTextW(hNote, counted == 1 ? L"letter" : L"letters");

    bool dictsReady = !g_loading && g_loadedSig == cfg.sig;
    int kind = RES_NONE;
    std::wstring res, boxKey, boxText;
    g_canAdd = false;
    g_scoreMode = 0;
    if (!key.empty()) {
        if (!dictsReady) {
            res = L"Loading dictionaries...";
            kind = RES_INFO;
        } else if (g_dicts.empty()) {
            res = L"No dictionaries loaded (see Config)";
            kind = RES_INFO;
            g_canAdd = blanks == 0;                   // adding a word will create the default dictionary
        } else {
            uint64_t hk = HashKey(key);
            bool showScore = Checked(hShowScore);
            int best = -1;
            for (const Dict& d : g_dicts) {
                auto it = std::lower_bound(d.hashes.begin(), d.hashes.end(), hk);
                if (it == d.hashes.end() || *it != hk) continue;
                int sc = d.scores[(size_t)(it - d.hashes.begin())];
                res += (res.empty() ? L"" : L", ") + d.name;
                if (showScore && sc >= 0) res += L" (" + std::to_wstring(sc) + L")";
                best = std::max(best, sc);
            }
            if (res.empty()) {
                res = L"Word not found in dictionary";
                kind = RES_MISSING;
                g_canAdd = blanks == 0;               // a word with a gap isn't a complete word to add
            } else {
                res = L"Found in dictionary: " + res;
                kind = RES_FOUND;
                if (showScore) {                      // the score box now changes this word's score
                    g_scoreMode = 2;
                    g_foundKey = key;
                    boxKey = L"F" + key + L"|" + res;
                    if (best >= 0) boxText = std::to_wstring(best);
                }
            }
        }
    }
    if (g_canAdd) {                                   // new word: the box holds the score it will be added with
        wchar_t sb[16];
        GetWindowTextW(hAddScore, sb, 16);            // starts from the Config default
        g_scoreMode = 1;
        boxKey = L"A";
        boxText = sb;
    }
    if (g_scoreMode == 0) g_scoreKey.clear();
    else if (boxKey != g_scoreKey) { SetWindowTextW(hScore, boxText.c_str()); g_scoreKey = boxKey; }
    g_resultColor = kind == RES_FOUND ? RGB(0, 128, 0) : kind == RES_MISSING ? RGB(200, 0, 0) : RGB(128, 128, 128);
    SetWindowTextW(hResult, res.c_str());
    InvalidateRect(hResult, nullptr, TRUE);
    UpdateAddButton();

    // Fill-the-blank tab (worked out only while that tab is showing; changing tabs calls Update).
    if (TabCtrl_GetCurSel(hTab) == TAB_RANDOM) { UpdateRandom(cfg, key, gapKey, gapKind, counted > 0, dictsReady); return; }
    if (TabCtrl_GetCurSel(hTab) != TAB_FILL) return;
    if (blanks == 0) {
        g_fill.valid = false;
        SendMessageW(hList, LB_SETCOUNT, 0, 0);
        std::wstring msg;
        if (cfg.P.empty() && cfg.V.empty() && cfg.K.empty()) {
            msg = L"No blank characters set (see Config)";
        } else {
            wchar_t g = !cfg.P.empty() ? cfg.P[0] : !cfg.V.empty() ? cfg.V[0] : cfg.K[0];
            msg = std::wstring(L"Type a word with ") + g + L" for the blank, e.g. C" + g + L"T";
        }
        SetWindowTextW(hStatus, msg.c_str());
        return;
    }

    int mode = (int)SendMessageW(hFillMode, CB_GETCURSEL, 0, 0);
    if (mode < FILL_ALL || mode > FILL_BOTH) mode = FILL_ALL;
    bool check = dictsReady && !g_dicts.empty();
    static const std::vector<Dict> noDicts;
    wchar_t spb[8];
    GetWindowTextW(hSepEdit, spb, 8);                         // word separator characters (Config)
    std::vector<FillSeg> segs;
    MakeSegments(word, spb, cfg, segs);
    FillOrder ord;
    ord.shuffle = g_fillShuffled;
    ord.seed = g_fillSeed;
    ord.sort = g_fillSort;
    ord.byScore = g_fillByScore;
    BuildFill(word, gapWord, gapKey, gapKind, key, segs, check ? g_dicts : noDicts, mode, FillCap(), ord);
    const FillModel& m = g_fill;

    HDC dc = GetDC(hList);
    HGDIOBJ old = SelectObject(dc, hFont);
    TEXTMETRICW tm;
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    ReleaseDC(hList, dc);
    g_colW = (int)((word.size() + 3) * (size_t)tm.tmAveCharWidth);
    SendMessageW(hList, LB_SETCOLUMNWIDTH, (WPARAM)g_colW, 0);
    SendMessageW(hList, LB_SETCOUNT, (WPARAM)m.rows, 0);
    SendMessageW(hList, LB_SETTOPINDEX, 0, 0);
    InvalidateRect(hList, nullptr, TRUE);

    std::wstring st;
    bool phrase = m.segs.size() >= 2;
    auto many = [](uint64_t v) { return v >= kBig ? std::wstring(L"far too many") : Num(v); };
    auto shown = [&](uint64_t of) { return (uint64_t)m.rows < of ? std::wstring(m.shuffle ? L"; showing a random " : L"; showing the first ") + Num((unsigned long long)m.rows) : std::wstring(); };
    if (mode != FILL_ALL && !check) {
        st = dictsReady ? L"No dictionaries loaded (see Config)" : L"Dictionaries loading...";
    } else if (mode == FILL_ALL) {
        st = m.total >= kBig ? std::wstring(L"Too many combinations to count")
                             : Num(m.total) + (m.total == 1 ? L" possibility" : L" possibilities");
        if (check) {
            if (m.hits.empty()) st += L", none in a dictionary";
            else st += L", " + Num(m.hits.size()) + L" in a dictionary" +
                       (m.hitsFirst ? L" (listed first, best scores first)" : L" (highlighted)");
            if (phrase && m.potential)
                st += L", " + many(m.potential) + (m.potential == 1 ? L" potential word (yellow)" : L" potential words (yellow)");
        } else if (!dictsReady) {
            st += L" (dictionaries loading...)";
        }
        st += shown(m.total);
    } else if (mode == FILL_POTENTIAL) {
        if (!phrase) st = spb[0] ? std::wstring(L"Separate the words to find potential words, e.g. COP") + spb[0] + L"C_R"
                                 : std::wstring(L"No word separator is set (see Config)");
        else if (m.potential == 0) st = L"No potential words fit";
        else st = many(m.potential) + (m.potential == 1 ? L" potential word" : L" potential words") + shown(m.potential);
    } else if (mode == FILL_DICT || !phrase) {
        if (m.hits.empty()) st = L"No dictionary words fit";
        else st = Num(m.hits.size()) + (m.hits.size() == 1 ? L" dictionary word" : L" dictionary words") + shown(m.hits.size());
    } else if (m.hits.empty() && m.potential == 0) {
        st = L"No dictionary or potential words fit";
    } else {                                                  // dictionary + potential
        st = Num(m.hits.size()) + (m.hits.size() == 1 ? L" dictionary word and " : L" dictionary words and ") +
             many(m.potential) + (m.potential == 1 ? L" potential word" : L" potential words") +
             shown(SatAdd((uint64_t)m.hits.size(), m.potential));
    }
    if (m.byScore && !m.block.empty()) st += L"; best score first";
    SetWindowTextW(hStatus, st.c_str());
}

// Shows what the Config checkboxes currently ignore (read-only: the boxes decide, not typing).
static void RefreshIgnoredDisplay() {
    std::wstring s;
    auto add = [&](const std::wstring& part) { if (!s.empty()) s += L"   "; s += part; };
    if (Checked(hWs)) add(L"[space] [tab]");
    if (Checked(hNums)) add(kNumbers);
    if (Checked(hPunct)) add(kPunct);
    if (Checked(hSpecial)) add(kSpecial);
    SetWindowTextW(hIgnored, s.empty() ? L"(nothing is ignored)" : s.c_str());
}

// ---- settings (per-user registry: HKEY_CURRENT_USER\Software\CrossTools) ---------------------
static const wchar_t* kRegKey = L"Software\\CrossTools";

struct SettingItem { const wchar_t* name; HWND h; };

// Reads (save = false) or writes (save = true) every setting. Text boxes are REG_SZ values, check boxes and the
// Fill "Show" choice are REG_DWORD. Nothing is read on a first run, so the defaults stand.
static void Settings(bool save) {
    HKEY k = nullptr;
    if (save) {
        if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    } else if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegKey, 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS) {
        return;
    }
    SettingItem texts[] = { { L"Blanks", hBlanks }, { L"DictFolder", hDictDir }, { L"AddDictionary", hAddDict },
                            { L"AddScore", hAddScore }, { L"Vowels", hVowels },
                            { L"Consonants", hConsonants }, { L"MaxRows", hFillCap }, { L"Separator", hSepEdit },
                            { L"MinSplit", hMinSplit } };
    for (const SettingItem& t : texts) {
        wchar_t b[MAX_PATH + 1];
        if (save) {
            GetWindowTextW(t.h, b, MAX_PATH);
            RegSetValueExW(k, t.name, 0, REG_SZ, (const BYTE*)b, (DWORD)((wcslen(b) + 1) * sizeof(wchar_t)));
        } else {
            DWORD type = 0, size = MAX_PATH * sizeof(wchar_t);
            if (RegQueryValueExW(k, t.name, nullptr, &type, (BYTE*)b, &size) == ERROR_SUCCESS && type == REG_SZ) {
                b[size / sizeof(wchar_t)] = 0;                // a stored string need not carry its own terminator
                SetWindowTextW(t.h, b);
            }
        }
    }
    SettingItem boxes[] = { { L"IgnoreWhitespace", hWs }, { L"IgnoreNumbers", hNums }, { L"IgnorePunctuation", hPunct },
                            { L"IgnoreSpecial", hSpecial }, { L"ShowScore", hShowScore } };
    for (const SettingItem& b : boxes) {
        DWORD v = Checked(b.h) ? 1 : 0, type = 0, size = sizeof(v);
        if (save) RegSetValueExW(k, b.name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        else if (RegQueryValueExW(k, b.name, nullptr, &type, (BYTE*)&v, &size) == ERROR_SUCCESS && type == REG_DWORD)
            SendMessageW(b.h, BM_SETCHECK, v ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    DWORD mode = (DWORD)SendMessageW(hFillMode, CB_GETCURSEL, 0, 0), type = 0, size = sizeof(mode);
    if (save) RegSetValueExW(k, L"FillMode", 0, REG_DWORD, (const BYTE*)&mode, sizeof(mode));
    else if (RegQueryValueExW(k, L"FillMode", nullptr, &type, (BYTE*)&mode, &size) == ERROR_SUCCESS && type == REG_DWORD)
        SendMessageW(hFillMode, CB_SETCURSEL, mode <= 3 ? mode : 0, 0);
    if (save) {                                               // the default dictionary (a name, not a text box)
        RegSetValueExW(k, L"DefaultDictionary", 0, REG_SZ, (const BYTE*)g_defaultDict.c_str(),
                       (DWORD)((g_defaultDict.size() + 1) * sizeof(wchar_t)));
    } else {
        wchar_t b[MAX_PATH + 1];
        DWORD dtype = 0, dsize = MAX_PATH * sizeof(wchar_t);
        if (RegQueryValueExW(k, L"DefaultDictionary", nullptr, &dtype, (BYTE*)b, &dsize) == ERROR_SUCCESS && dtype == REG_SZ) {
            b[dsize / sizeof(wchar_t)] = 0;
            g_defaultDict = b;
        }
    }
    RegCloseKey(k);
}

static void LoadSettings() { Settings(false); }
static void SaveSettings() { Settings(true); }

static void Show(HWND w, bool on) { ShowWindow(w, on ? SW_SHOW : SW_HIDE); }

static void ShowTab() {
    int t = TabCtrl_GetCurSel(hTab);
    bool c = t == TAB_COUNT, f = t == TAB_FILL, rn = t == TAB_RANDOM, sc = t == TAB_SCORES, g = t == TAB_CONFIG;
    for (HWND w : { hCount, hNote, hResult }) Show(w, c);
    for (HWND w : { hStatus, hList, hLblShow, hFillMode, hLblCap, hFillCap, hFillShuf, hFillSort, hFillSplit, hFillScore }) Show(w, f);
    for (HWND w : { hRandWord, hRandFrom, hRandStatus, hLblRandLen, hRandLen, hLblRandScore, hRandScore, hRandBtn }) Show(w, rn);
    for (HWND w : { hLblOld, hOldMax, hLblNew, hNewMax, hLblRound, hRound, hLblClamp, hClampMax, hInfo,
                    hDictList, hApply, hMerge, hMakeDef, hDump, hLog })
        Show(w, sc);
    for (HWND w : { hLblBlanks, hBlanks, hLblIgnored, hWs, hNums, hPunct, hSpecial, hIgnored, hCfgNote,
                    hLblDict, hDictDir, hReload, hDictStatus, hLblAdd, hAddDict, hAddScore, hShowScore,
                    hLblVowel, hVowels, hLblCons, hConsonants, hLblSep, hSepEdit, hLblMinSplit, hMinSplit })
        Show(w, g);
    UpdateAddButton();                            // the Add button also depends on the word
}

static HWND Make(DWORD ex, const wchar_t* cls, DWORD style, HWND parent, int id) {
    return CreateWindowExW(ex, cls, L"", WS_CHILD | style, 0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
}

// Icons for the Fill buttons, drawn once as bitmaps: 0 = shuffle (two crossing arrows); 1 = "A over Z" and 2 = "Z over A",
// each beside a down arrow. They sit on standard push buttons, so the classic look stays.
static HBITMAP MakeFillIcon(int kind) {
    const int W = 20, H = 18;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, W, H);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);
    RECT rc = { 0, 0, W, H };
    FillRect(dc, &rc, GetSysColorBrush(COLOR_BTNFACE));
    COLORREF ink = GetSysColor(COLOR_BTNTEXT);
    HPEN pen = CreatePen(PS_SOLID, 2, ink);
    HBRUSH brush = CreateSolidBrush(ink);
    HGDIOBJ oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, brush);
    if (kind == 0) {
        MoveToEx(dc, 1, 13, nullptr);
        LineTo(dc, 6, 13);
        LineTo(dc, 12, 5);
        LineTo(dc, 14, 5);
        MoveToEx(dc, 1, 5, nullptr);
        LineTo(dc, 6, 5);
        LineTo(dc, 12, 13);
        LineTo(dc, 14, 13);
        for (int y : { 5, 13 }) {
            POINT head[3] = { { 13, y - 3 }, { 13, y + 3 }, { 18, y } };
            Polygon(dc, head, 3);
        }
    } else {
        MoveToEx(dc, 4, 1, nullptr);
        LineTo(dc, 4, 11);
        POINT head[3] = { { 0, 10 }, { 8, 10 }, { 4, 16 } };
        Polygon(dc, head, 3);
        HFONT font = CreateFontW(-10, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_SWISS, L"Arial");
        HGDIOBJ oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, ink);
        TextOutW(dc, 11, -1, kind == 1 ? L"A" : L"Z", 1);
        TextOutW(dc, 11, 8, kind == 1 ? L"Z" : L"A", 1);
        SelectObject(dc, oldFont);
        DeleteObject(font);
    }
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldBmp);
    DeleteObject(pen);
    DeleteObject(brush);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
    return bmp;
}

// ---- text-box shortcuts (all edit boxes) -----------------------------------------------------
static void DeleteWordLeft(HWND e) {
    DWORD a = 0, b = 0;
    SendMessageW(e, EM_GETSEL, (WPARAM)&a, (LPARAM)&b);
    if (a == b) {                                             // nothing selected: select back to the word start
        wchar_t t[512];
        int n = GetWindowTextW(e, t, 512);
        size_t i = a > (DWORD)n ? (size_t)n : a;
        while (i > 0 && t[i - 1] == L' ') --i;
        while (i > 0 && t[i - 1] != L' ') --i;
        SendMessageW(e, EM_SETSEL, (WPARAM)i, (LPARAM)a);
    }
    SendMessageW(e, EM_REPLACESEL, TRUE, (LPARAM)L"");
}

static LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (h == hInput && (m == WM_KEYDOWN || m == WM_CHAR) && w == VK_RETURN) {   // Enter in the word bar picks a word on Random
        if (m == WM_KEYDOWN && TabCtrl_GetCurSel(hTab) == TAB_RANDOM)
            PostMessageW(g_hwnd, WM_COMMAND, MAKEWPARAM(IDC_RANDOM, BN_CLICKED), 0);
        return 0;
    }
    if (m == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000)) {
        switch (w) {
        case 'A': SendMessageW(h, EM_SETSEL, 0, -1); return 0;     // select all
        case 'C': SendMessageW(h, WM_COPY, 0, 0); return 0;
        case 'X': SendMessageW(h, WM_CUT, 0, 0); return 0;
        case 'V': SendMessageW(h, WM_PASTE, 0, 0); return 0;
        case 'Z': SendMessageW(h, EM_UNDO, 0, 0); return 0;
        case VK_BACK: DeleteWordLeft(h); return 0;                 // delete the previous word
        }
    }
    if (m == WM_CHAR && (w == 1 || w == 3 || w == 22 || w == 24 || w == 26 || w == 127))
        return 0;                                                  // the control codes those shortcuts produce
    return CallWindowProcW(g_editProc, h, m, w, l);
}

static LRESULT CALLBACK ListProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_LBUTTONDBLCLK) {                  // find the row ourselves: LB_ITEMFROMPOINT only has 16 bits
        RECT rc;
        GetClientRect(h, &rc);
        int perCol = std::max(1, (int)(rc.bottom / g_itemH));
        int x = (short)LOWORD(l), y = (short)HIWORD(l);
        long long top = (long long)SendMessageW(h, LB_GETTOPINDEX, 0, 0);
        if (x >= 0 && y >= 0 && y / g_itemH < perCol)
            PostMessageW(g_hwnd, WM_APP_FILLDBL, (WPARAM)(top + (long long)(x / g_colW) * perCol + y / g_itemH), 0);
        return 0;
    }
    return CallWindowProcW(g_listProc, h, m, w, l);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        g_hwnd = h;
        HGDIOBJ gui = GetStockObject(DEFAULT_GUI_FONT);
        {
            LOGFONTW lf = {};                                      // the default dictionary is drawn in the same font, bold
            GetObjectW(gui, sizeof(lf), &lf);
            lf.lfWeight = FW_BOLD;
            hBoldGui = CreateFontIndirectW(&lf);
        }
        hFont = CreateFontW(-24, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        hBigFont = CreateFontW(-96, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                               CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        hSmallFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                                 CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        {
            HDC dc = GetDC(h);                                     // row height for the owner-drawn Fill list
            HGDIOBJ old = SelectObject(dc, hFont);
            TEXTMETRICW tm;
            GetTextMetricsW(dc, &tm);
            SelectObject(dc, old);
            ReleaseDC(h, dc);
            g_itemH = tm.tmHeight + 2;
        }

        hRandFont = CreateFontW(-48, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                                CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        g_rng.seed(RandomSeed());
        hInput = Make(WS_EX_CLIENTEDGE, L"EDIT", WS_VISIBLE | ES_AUTOHSCROLL | ES_UPPERCASE, h, IDC_INPUT);
        // The tab control is created before the page controls so they sit on top of it.
        hTab = Make(0, WC_TABCONTROLW, WS_VISIBLE | WS_CLIPSIBLINGS | TCS_FOCUSNEVER, h, IDC_TAB);

        hCount = Make(0, L"STATIC", SS_CENTER, h, IDC_COUNT);
        hNote = Make(0, L"STATIC", SS_CENTER, h, IDC_NOTE);
        hResult = Make(0, L"STATIC", SS_CENTER, h, IDC_RESULT);
        hAdd = Make(0, L"BUTTON", BS_PUSHBUTTON, h, IDC_ADD);
        hStatus = Make(0, L"STATIC", 0, h, IDC_STATUS);
        hList = Make(WS_EX_CLIENTEDGE, L"LISTBOX",
                     WS_HSCROLL | LBS_MULTICOLUMN | LBS_NOSEL | LBS_OWNERDRAWFIXED | LBS_NODATA, h, IDC_LIST);

        hLblOld = Make(0, L"STATIC", 0, h, IDC_LBL_OLD);
        hOldMax = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL | ES_NUMBER, h, IDC_OLDMAX);
        hLblNew = Make(0, L"STATIC", 0, h, IDC_LBL_NEW);
        hNewMax = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL | ES_NUMBER, h, IDC_NEWMAX);
        hLblRound = Make(0, L"STATIC", 0, h, IDC_LBL_ROUND);
        hRound = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL | ES_NUMBER, h, IDC_ROUNDTO);
        hLblClamp = Make(0, L"STATIC", 0, h, IDC_LBL_CLAMP);
        hClampMax = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL | ES_NUMBER, h, IDC_CLAMPMAX);
        hInfo = Make(0, L"STATIC", SS_ICON | SS_NOTIFY, h, IDC_INFO);
        hDictList = Make(WS_EX_CLIENTEDGE, WC_LISTVIEWW, LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, h, IDC_DICTLIST);
        hApply = Make(0, L"BUTTON", BS_PUSHBUTTON, h, IDC_APPLY);
        hMakeDef = Make(0, L"BUTTON", BS_PUSHBUTTON, h, IDC_MAKEDEF);
        hLog = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, h, IDC_LOG);

        hLblBlanks = Make(0, L"STATIC", 0, h, IDC_LBL_BLANKS);
        hBlanks = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL, h, IDC_BLANKS);
        hLblIgnored = Make(0, L"STATIC", 0, h, IDC_LBL_IGNORED);
        hWs = Make(0, L"BUTTON", BS_AUTOCHECKBOX, h, IDC_WS);
        hNums = Make(0, L"BUTTON", BS_AUTOCHECKBOX, h, IDC_NUMS);
        hPunct = Make(0, L"BUTTON", BS_AUTOCHECKBOX, h, IDC_PUNCT);
        hSpecial = Make(0, L"BUTTON", BS_AUTOCHECKBOX, h, IDC_SPECIAL);
        hIgnored = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_MULTILINE | ES_READONLY, h, IDC_IGNORED);
        hCfgNote = Make(0, L"STATIC", 0, h, IDC_CFG_NOTE);
        hLblDict = Make(0, L"STATIC", 0, h, IDC_LBL_DICT);
        hDictDir = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL, h, IDC_DICTDIR);
        hReload = Make(0, L"BUTTON", BS_PUSHBUTTON, h, IDC_RELOAD);
        hDictStatus = Make(0, L"STATIC", 0, h, IDC_DICT_STATUS);
        hLblAdd = Make(0, L"STATIC", 0, h, IDC_LBL_ADD);
        hAddDict = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL, h, IDC_ADDDICT);
        hAddScore = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL | ES_NUMBER, h, IDC_ADDSCORE);
        hShowScore = Make(0, L"BUTTON", BS_AUTOCHECKBOX, h, IDC_SHOWSCORE);
        hLblScore = Make(0, L"STATIC", SS_RIGHT, h, IDC_LBL_SCORE);
        hScore = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL | ES_NUMBER, h, IDC_SCORE);
        hLblShow = Make(0, L"STATIC", 0, h, IDC_LBL_SHOW);
        hFillMode = Make(0, L"COMBOBOX", CBS_DROPDOWNLIST | WS_VSCROLL, h, IDC_FILLMODE);
        hLblCap = Make(0, L"STATIC", 0, h, IDC_LBL_CAP);
        hFillCap = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL | ES_NUMBER, h, IDC_FILLCAP);
        hLblVowel = Make(0, L"STATIC", 0, h, IDC_LBL_VOWEL);
        hVowels = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL, h, IDC_VOWELS);
        hLblCons = Make(0, L"STATIC", 0, h, IDC_LBL_CONS);
        hConsonants = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL, h, IDC_CONS);
        hLblSep = Make(0, L"STATIC", 0, h, IDC_LBL_SEP);
        hSepEdit = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL, h, IDC_SEP);
        hLblMinSplit = Make(0, L"STATIC", 0, h, IDC_LBL_MINSPLIT);
        hMinSplit = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL | ES_NUMBER, h, IDC_MINSPLIT);
        hFillShuf = Make(0, L"BUTTON", BS_PUSHBUTTON | BS_BITMAP, h, IDC_FILLSHUF);
        hFillSort = Make(0, L"BUTTON", BS_PUSHBUTTON | BS_BITMAP, h, IDC_FILLSORT);
        hFillSplit = Make(0, L"BUTTON", BS_PUSHBUTTON, h, IDC_FILLSPLIT);
        hFillScore = Make(0, L"BUTTON", BS_AUTOCHECKBOX | BS_PUSHLIKE, h, IDC_FILLSCORE);
        hDump = Make(0, L"BUTTON", BS_PUSHBUTTON, h, IDC_DUMP);
        hMerge = Make(0, L"BUTTON", BS_PUSHBUTTON, h, IDC_MERGE);
        hBmpShuf = MakeFillIcon(0);
        hBmpAZ = MakeFillIcon(1);
        hBmpZA = MakeFillIcon(2);
        SendMessageW(hFillShuf, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmpShuf);
        SendMessageW(hFillSort, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmpAZ);
        SendMessageW(hInfo, STM_SETICON, (WPARAM)LoadImageW(nullptr, IDI_INFORMATION, IMAGE_ICON, 16, 16, LR_SHARED), 0);
        hTip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, h, nullptr, nullptr, nullptr);
        {
            TOOLINFOW ti = {};                                     // the info icon's tooltip: its text is asked for on hover
            ti.cbSize = TTTOOLINFOW_V1_SIZE;
            ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            ti.hwnd = h;
            ti.uId = (UINT_PTR)hInfo;
            ti.lpszText = LPSTR_TEXTCALLBACKW;
            SendMessageW(hTip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
            SendMessageW(hTip, TTM_SETMAXTIPWIDTH, 0, 400);
            SendMessageW(hTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 30000);
        }
        hRandWord = Make(0, L"STATIC", SS_CENTER | SS_CENTERIMAGE | SS_NOPREFIX, h, IDC_RANDWORD);
        hRandFrom = Make(0, L"STATIC", SS_CENTER | SS_NOPREFIX, h, IDC_RANDFROM);
        hRandStatus = Make(0, L"STATIC", SS_CENTER, h, IDC_RANDSTATUS);
        hLblRandLen = Make(0, L"STATIC", SS_RIGHT, h, IDC_LBL_RANDLEN);
        hRandLen = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL | ES_NUMBER, h, IDC_RANDLEN);
        hRandBtn = Make(0, L"BUTTON", BS_PUSHBUTTON, h, IDC_RANDOM);
        hLblRandScore = Make(0, L"STATIC", SS_RIGHT, h, IDC_LBL_RANDSCORE);
        hRandScore = Make(WS_EX_CLIENTEDGE, L"EDIT", ES_AUTOHSCROLL | ES_NUMBER, h, IDC_RANDSCORE);

        SendMessageW(hInput, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hList, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hCount, WM_SETFONT, (WPARAM)hBigFont, TRUE);
        SendMessageW(hRandWord, WM_SETFONT, (WPARAM)hRandFont, TRUE);
        for (HWND c : { hBlanks, hDictDir, hResult, hIgnored, hAddDict, hAddScore, hScore, hVowels, hConsonants, hFillCap, hRandLen, hRandScore, hSepEdit, hOldMax, hNewMax, hRound, hClampMax, hMinSplit })
            SendMessageW(c, WM_SETFONT, (WPARAM)hSmallFont, TRUE);
        for (HWND c : { hTab, hStatus, hNote, hAdd, hLblBlanks, hLblIgnored, hWs, hNums, hPunct, hSpecial, hCfgNote,
                        hLblDict, hReload, hDictStatus, hLblAdd, hLblOld, hLblNew, hLblRound, hLblClamp,
                        hApply, hDictList, hLog, hShowScore, hLblScore, hLblShow, hFillMode, hLblCap, hLblVowel,
                        hLblCons, hRandFrom, hRandStatus, hLblRandLen, hLblRandScore, hRandBtn, hLblSep,
                        hDump, hMerge, hMakeDef, hFillSplit, hFillScore, hLblMinSplit })
            SendMessageW(c, WM_SETFONT, (WPARAM)gui, TRUE);
        SendMessageW(hInput, EM_LIMITTEXT, 40, 0);
        SendMessageW(hBlanks, EM_LIMITTEXT, 10, 0);
        SendMessageW(hDictDir, EM_LIMITTEXT, MAX_PATH - 1, 0);
        SendMessageW(hAddDict, EM_LIMITTEXT, 60, 0);
        for (HWND c : { hAddScore, hScore, hOldMax, hNewMax, hRound, hClampMax }) SendMessageW(c, EM_LIMITTEXT, 6, 0);
        for (HWND c : { hVowels, hConsonants, hFillCap }) SendMessageW(c, EM_LIMITTEXT, 10, 0);
        SendMessageW(hRandLen, EM_LIMITTEXT, 2, 0);
        SendMessageW(hRandScore, EM_LIMITTEXT, 6, 0);
        SendMessageW(hSepEdit, EM_LIMITTEXT, 4, 0);
        SendMessageW(hMinSplit, EM_LIMITTEXT, 2, 0);

        for (HWND c : { hInput, hBlanks, hIgnored, hDictDir, hAddDict, hAddScore, hOldMax, hNewMax, hRound,
                        hClampMax, hLog, hScore, hVowels, hConsonants, hFillCap, hRandLen, hRandScore, hSepEdit, hMinSplit }) {                   // Ctrl+A/C/X/V/Z/Backspace in every text box
            WNDPROC prev = (WNDPROC)SetWindowLongPtrW(c, GWLP_WNDPROC, (LONG_PTR)EditProc);
            if (!g_editProc) g_editProc = prev;
        }
        g_listProc = (WNDPROC)SetWindowLongPtrW(hList, GWLP_WNDPROC, (LONG_PTR)ListProc);   // double-click adds a word

        SetWindowTextW(hLblBlanks, L"Blank characters (always count as letters; mark the gap on the Fill tab):");
        SetWindowTextW(hLblIgnored, L"Ignored characters (not counted) - tick the groups to ignore:");
        SetWindowTextW(hWs, L"Whitespace (spaces, tabs)");
        SetWindowTextW(hNums, L"Numbers (0-9)");
        SetWindowTextW(hPunct, L"Punctuation");
        SetWindowTextW(hSpecial, L"Special characters");
        SetWindowTextW(hCfgNote, L"Blank and wildcard characters always count, even if their group is ignored.");
        SetWindowTextW(hLblDict, L"Dictionaries folder (.dict files; relative to the exe, or a full path):");
        SetWindowTextW(hReload, L"Reload");
        SetWindowTextW(hShowScore, L"Show a found word's score, and let me change it");
        SetWindowTextW(hLblScore, L"Score:");
        SetWindowTextW(hLblShow, L"Show:");
        SetWindowTextW(hLblCap, L"Max rows:");
        SetWindowTextW(hLblVowel, L"Any vowel:");
        SetWindowTextW(hLblCons, L"Any consonant:");
        SetWindowTextW(hLblRandLen, L"Length:");
        SetWindowTextW(hLblRandScore, L"Score:");
        SetWindowTextW(hLblSep, L"Word separator (splits a phrase into words on Fill):");
        SetWindowTextW(hFillSplit, L"Split");
        SetWindowTextW(hLblMinSplit, L"Minimum word length for Split (letters):");
        SetWindowTextW(hMerge, L"Merge dictionaries");
        SetWindowTextW(hMakeDef, L"Make default");
        SetWindowTextW(hFillScore, L"Sort by score");
        SetWindowTextW(hRandBtn, L"Pick a word");
        SetWindowTextW(hLblAdd, L"Default dictionary for added words (file in that folder), and the default score:");
        SetWindowTextW(hLblOld, L"Old max:");
        SetWindowTextW(hLblNew, L"New max:");
        SetWindowTextW(hLblRound, L"Round to:");
        SetWindowTextW(hLblClamp, L"Clamp max:");
        SetWindowTextW(hApply, L"Rescale / clamp");
        SendMessageW(hWs, BM_SETCHECK, BST_CHECKED, 0);        // defaults match the old behaviour:
        SendMessageW(hPunct, BM_SETCHECK, BST_CHECKED, 0);     // whitespace, punctuation and special
        SendMessageW(hSpecial, BM_SETCHECK, BST_CHECKED, 0);   // characters ignored; numbers counted

        ListView_SetExtendedListViewStyle(hDictList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
        const wchar_t* heads[3] = { L"Dictionary", L"Entries", L"Top score" };
        const int widths[3] = { 220, 90, 90 };
        for (int i = 0; i < 3; ++i) {
            LVCOLUMNW col = {};
            col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            col.pszText = (LPWSTR)heads[i];
            col.cx = widths[i];
            col.iSubItem = i;
            SendMessageW(hDictList, LVM_INSERTCOLUMNW, i, (LPARAM)&col);
        }

        TCITEMW item = {};
        item.mask = TCIF_TEXT;
        item.pszText = (LPWSTR)L"Letter count";
        SendMessageW(hTab, TCM_INSERTITEMW, TAB_COUNT, (LPARAM)&item);
        item.pszText = (LPWSTR)L"Fill the blank";
        SendMessageW(hTab, TCM_INSERTITEMW, TAB_FILL, (LPARAM)&item);
        item.pszText = (LPWSTR)L"Random";
        SendMessageW(hTab, TCM_INSERTITEMW, TAB_RANDOM, (LPARAM)&item);
        item.pszText = (LPWSTR)L"Dictionaries";
        SendMessageW(hTab, TCM_INSERTITEMW, TAB_SCORES, (LPARAM)&item);
        item.pszText = (LPWSTR)L"Config";
        SendMessageW(hTab, TCM_INSERTITEMW, TAB_CONFIG, (LPARAM)&item);
        TabCtrl_SetCurSel(hTab, TAB_COUNT);       // default tab

        SetWindowTextW(hBlanks, L"_");            // defaults (Update is skipped until g_ready)
        SetWindowTextW(hDictDir, kDefaultDictDir);
        SetWindowTextW(hAddDict, kDefaultAddDict);
        SetWindowTextW(hAddScore, L"50");
        SetWindowTextW(hVowels, L"#");
        SetWindowTextW(hConsonants, L"@");
        SetWindowTextW(hFillCap, L"2000");
        SetWindowTextW(hSepEdit, L" ");
        SetWindowTextW(hMinSplit, L"3");
        SendMessageW(hFillMode, CB_ADDSTRING, 0, (LPARAM)L"All combinations");
        SendMessageW(hFillMode, CB_ADDSTRING, 0, (LPARAM)L"Dictionary words only");
        SendMessageW(hFillMode, CB_ADDSTRING, 0, (LPARAM)L"Potential words only");
        SendMessageW(hFillMode, CB_ADDSTRING, 0, (LPARAM)L"Dictionary + potential");
        SendMessageW(hFillMode, CB_SETCURSEL, 0, 0);
        LoadSettings();                           // saved settings (if any) replace these defaults
        RefreshIgnoredDisplay();
        g_ready = true;
        ShowTab();
        StartReload();                            // dictionaries load in the background
        return 0;
    }
    case WM_SIZE: {
        int W = LOWORD(l), H = HIWORD(l);
        MoveWindow(hInput, 10, 10, W - 20, 40, TRUE);
        MoveWindow(hTab, 10, 58, W - 20, H - 68, TRUE);

        RECT r = { 10, 58, W - 10, H - 10 };      // tab window rect -> tab page rect
        TabCtrl_AdjustRect(hTab, FALSE, &r);
        int x = r.left + 6, y = r.top + 6, pw = r.right - r.left - 12, ph = r.bottom - r.top - 12;

        // Letter count tab
        MoveWindow(hCount, x, y + 10, pw, 120, TRUE);
        MoveWindow(hNote, x, y + 134, pw, 24, TRUE);
        MoveWindow(hResult, x, y + 170, pw, 44, TRUE);
        int rx = x + (pw - 324) / 2;              // score label, score box, Add / Update button
        MoveWindow(hLblScore, rx, y + 226, 46, 18, TRUE);
        MoveWindow(hScore, rx + 50, y + 221, 64, 26, TRUE);
        MoveWindow(hAdd, rx + 124, y + 220, 200, 28, TRUE);

        // Fill the blank tab
        MoveWindow(hLblShow, x, y + 4, 40, 18, TRUE);
        MoveWindow(hFillMode, x + 42, y, 190, 140, TRUE);       // the height includes the drop-down list
        MoveWindow(hLblCap, x + 242, y + 4, 60, 18, TRUE);
        MoveWindow(hFillCap, x + 304, y, 84, 26, TRUE);
        MoveWindow(hFillShuf, x + 400, y, 30, 26, TRUE);        // icon buttons: shuffle, sort A-Z / Z-A
        MoveWindow(hFillSort, x + 434, y, 30, 26, TRUE);
        MoveWindow(hFillSplit, x, y + 30, 60, 26, TRUE);        // second row: Split, Sort by score
        MoveWindow(hFillScore, x + 66, y + 30, 100, 26, TRUE);
        MoveWindow(hStatus, x, y + 62, pw, 20, TRUE);
        MoveWindow(hList, x, y + 86, pw, ph - 86, TRUE);

        // Random tab
        MoveWindow(hRandWord, x, y + 10, pw, 80, TRUE);
        MoveWindow(hRandFrom, x, y + 94, pw, 20, TRUE);
        MoveWindow(hRandStatus, x, y + 120, pw, 36, TRUE);          // two lines when pattern, length and score all apply
        int qx = x + (pw - 384) / 2;
        MoveWindow(hLblRandLen, qx, y + 176, 52, 18, TRUE);
        MoveWindow(hRandLen, qx + 56, y + 171, 46, 26, TRUE);
        MoveWindow(hLblRandScore, qx + 116, y + 176, 46, 18, TRUE);
        MoveWindow(hRandScore, qx + 166, y + 171, 64, 26, TRUE);
        MoveWindow(hRandBtn, qx + 244, y + 170, 140, 28, TRUE);

        // Dictionaries tab
        MoveWindow(hLblOld, x, y + 4, 46, 18, TRUE);
        MoveWindow(hOldMax, x + 48, y, 52, 26, TRUE);
        MoveWindow(hLblNew, x + 110, y + 4, 48, 18, TRUE);
        MoveWindow(hNewMax, x + 160, y, 52, 26, TRUE);
        MoveWindow(hLblRound, x + 222, y + 4, 50, 18, TRUE);
        MoveWindow(hRound, x + 274, y, 52, 26, TRUE);
        MoveWindow(hLblClamp, x + 336, y + 4, 58, 18, TRUE);
        MoveWindow(hClampMax, x + 396, y, 52, 26, TRUE);
        MoveWindow(hInfo, x + 458, y + 5, 16, 16, TRUE);          // hover for what the boxes do, with a preview
        int lh = std::max(60, ph - 162), by = y + 36 + lh + 6;
        MoveWindow(hDictList, x, y + 36, pw, lh, TRUE);
        MoveWindow(hApply, x, by, 150, 28, TRUE);
        MoveWindow(hMerge, x + 156, by, 150, 28, TRUE);
        MoveWindow(hMakeDef, x, by + 34, 150, 28, TRUE);
        MoveWindow(hDump, x + 156, by + 34, 150, 28, TRUE);
        MoveWindow(hLog, x, by + 68, pw, 52, TRUE);

        // Config tab
        MoveWindow(hLblBlanks, x, y, pw, 18, TRUE);
        MoveWindow(hBlanks, x, y + 20, 160, 26, TRUE);
        MoveWindow(hLblVowel, x + 172, y + 25, 62, 18, TRUE);
        MoveWindow(hVowels, x + 236, y + 20, 50, 26, TRUE);
        MoveWindow(hLblCons, x + 298, y + 25, 84, 18, TRUE);
        MoveWindow(hConsonants, x + 384, y + 20, 50, 26, TRUE);
        MoveWindow(hLblIgnored, x, y + 54, pw, 18, TRUE);
        MoveWindow(hWs, x, y + 74, pw, 20, TRUE);
        MoveWindow(hNums, x, y + 96, pw, 20, TRUE);
        MoveWindow(hPunct, x, y + 118, pw, 20, TRUE);
        MoveWindow(hSpecial, x, y + 140, pw, 20, TRUE);
        MoveWindow(hIgnored, x, y + 164, pw, 54, TRUE);
        MoveWindow(hCfgNote, x, y + 222, pw, 18, TRUE);
        MoveWindow(hLblDict, x, y + 248, pw, 18, TRUE);
        MoveWindow(hDictDir, x, y + 268, pw - 90, 26, TRUE);
        MoveWindow(hReload, x + pw - 84, y + 268, 84, 26, TRUE);
        MoveWindow(hDictStatus, x, y + 298, pw, 18, TRUE);
        MoveWindow(hLblAdd, x, y + 324, pw, 18, TRUE);
        MoveWindow(hAddDict, x, y + 344, 230, 26, TRUE);
        MoveWindow(hAddScore, x + 238, y + 344, 70, 26, TRUE);
        MoveWindow(hShowScore, x, y + 378, pw, 20, TRUE);
        MoveWindow(hLblSep, x, y + 409, 330, 18, TRUE);
        MoveWindow(hSepEdit, x + 336, y + 404, 60, 26, TRUE);
        MoveWindow(hLblMinSplit, x, y + 443, 330, 18, TRUE);
        MoveWindow(hMinSplit, x + 336, y + 438, 60, 26, TRUE);
        return 0;
    }
    case WM_MEASUREITEM: {                        // Fill list rows (owner-drawn so hits can be highlighted)
        MEASUREITEMSTRUCT* mi = (MEASUREITEMSTRUCT*)l;
        if (mi->CtlID == IDC_LIST) { mi->itemHeight = g_itemH; return TRUE; }
        break;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)l;
        if (di->CtlID != IDC_LIST) break;
        if ((int)di->itemID < 0) return TRUE;
        if (!g_fill.valid || (long long)di->itemID >= g_fill.rows) return TRUE;
        int state = ROW_PLAIN;
        std::wstring txt = FillRow((long long)di->itemID, state);
        bool tint = g_fill.mode != FILL_DICT;                 // all views are coloured, except "Dictionary words only"
        bool green = tint && state == ROW_DICT, yellow = tint && state == ROW_POTENTIAL;
        HBRUSH br = CreateSolidBrush(green ? RGB(198, 239, 206) : yellow ? RGB(255, 235, 156) : GetSysColor(COLOR_WINDOW));
        FillRect(di->hDC, &di->rcItem, br);
        DeleteObject(br);
        HGDIOBJ old = SelectObject(di->hDC, hFont);
        SetBkMode(di->hDC, TRANSPARENT);
        SetTextColor(di->hDC, green ? RGB(0, 97, 0) : yellow ? RGB(156, 87, 0) : GetSysColor(COLOR_WINDOWTEXT));
        RECT tr = di->rcItem;
        tr.left += 4;
        DrawTextW(di->hDC, txt.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(di->hDC, old);
        return TRUE;
    }
    case WM_CTLCOLORSTATIC: {                     // blue count; green / red / gray dictionary result
        HDC dc = (HDC)w;
        HWND c = (HWND)l;
        if (c == hCount || c == hNote) SetTextColor(dc, RGB(0, 70, 200));
        else if (c == hResult) SetTextColor(dc, g_resultColor);
        else if (c == hRandFrom) SetTextColor(dc, RGB(128, 128, 128));
        else break;
        SetBkMode(dc, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    }
    case WM_TIMER:
        if (w == IDT_RELOAD) StartReload();
        return 0;
    case WM_APP_LOADED:
        OnLoaded((LoadResult*)l);
        return 0;
    case WM_APP_FILLDBL:
        AddFromFill((long long)w);
        return 0;
    case WM_SETFOCUS:
        SetFocus(hInput);
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        if (id == IDC_INPUT && code == EN_CHANGE) {
            Update();
        } else if ((id == IDC_BLANKS || id == IDC_VOWELS || id == IDC_CONS) && code == EN_CHANGE) {
            ScheduleReload(350);                  // re-index once typing pauses
            Update();
        } else if ((id == IDC_WS || id == IDC_NUMS || id == IDC_PUNCT || id == IDC_SPECIAL) && code == BN_CLICKED) {
            RefreshIgnoredDisplay();
            ScheduleReload(200);
            Update();
        } else if (id == IDC_DICTDIR && code == EN_KILLFOCUS) {
            wchar_t db[MAX_PATH];                 // applies a changed folder when you leave the box
            GetWindowTextW(hDictDir, db, MAX_PATH);
            if (g_requestedDir != db) StartReload();
        } else if (id == IDC_RELOAD && code == BN_CLICKED) {
            StartReload();
            SetFocus(hInput);
        } else if (id == IDC_RANDOM && code == BN_CLICKED) {
            PickRandomWord();
            SetFocus(hInput);
        } else if (id == IDC_RANDSCORE && code == EN_CHANGE) {
            Update();
        } else if (id == IDC_RANDLEN && code == EN_CHANGE) {
            if (!g_settingLen) {                      // the user's own length (not the pattern's, which we put there)
                wchar_t lb[8];
                GetWindowTextW(hRandLen, lb, 8);
                g_userLen = lb;
            }
            Update();
        } else if (id == IDC_FILLMODE && code == CBN_SELCHANGE) {
            Update();
        } else if (id == IDC_FILLMODE && code == CBN_CLOSEUP) {
            SetFocus(hInput);
        } else if ((id == IDC_FILLCAP || id == IDC_SEP) && code == EN_CHANGE) {
            Update();
        } else if (id == IDC_SHOWSCORE && code == BN_CLICKED) {
            Update();
        } else if (id == IDC_ADDDICT && code == EN_CHANGE) {
            UpdateAddButton();
        } else if (id == IDC_ADD && code == BN_CLICKED) {
            if (g_scoreMode == 2) ChangeWordScore();
            else AddToDefaultDict();
            SetFocus(hInput);
        } else if (id == IDC_APPLY && code == BN_CLICKED) {
            ApplyScores(h);
        } else if (id == IDC_MERGE && code == BN_CLICKED) {
            ApplyMerge(h);
        } else if (id == IDC_MAKEDEF && code == BN_CLICKED) {
            MakeDefault();
        } else if (id == IDC_DUMP && code == BN_CLICKED) {
            DumpToDefault();
        } else if (id == IDC_FILLSCORE && code == BN_CLICKED) {
            g_fillByScore = Checked(hFillScore);  // potential words best total score first; it replaces any other order
            if (g_fillByScore) {
                g_fillShuffled = false;
                g_fillSort = 0;
                SendMessageW(hFillSort, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmpAZ);
            }
            Update();
            SetFocus(hInput);
        } else if (id == IDC_FILLSHUF && code == BN_CLICKED) {
            g_fillShuffled = true;                // a new random selection every click
            g_fillSeed = g_rng();
            g_fillByScore = false;
            SendMessageW(hFillScore, BM_SETCHECK, BST_UNCHECKED, 0);
            Update();
            SetFocus(hInput);
        } else if (id == IDC_FILLSPLIT && code == BN_CLICKED) {
            SplitWord();
            SetFocus(hInput);
        } else if (id == IDC_FILLSORT && code == BN_CLICKED) {
            if (g_fillShuffled || g_fillByScore) {            // back to a sorted list first, then it flips A-Z / Z-A
                g_fillShuffled = g_fillByScore = false;
                SendMessageW(hFillScore, BM_SETCHECK, BST_UNCHECKED, 0);
            } else {
                g_fillSort = g_fillSort == 2 ? 1 : 2;
            }
            SendMessageW(hFillSort, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)(g_fillSort == 2 ? hBmpZA : hBmpAZ));
            Update();
            SetFocus(hInput);
        }
        return 0;
    }
    case WM_NOTIFY:
        if (((NMHDR*)l)->hwndFrom == hDictList && ((NMHDR*)l)->code == NM_CUSTOMDRAW) {   // the default dictionary in bold
            NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)l;
            switch (cd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW;
            case CDDS_ITEMPREPAINT: return CDRF_NOTIFYSUBITEMDRAW;
            case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
                if ((int)cd->nmcd.dwItemSpec == DefaultDictIndex()) {
                    SelectObject(cd->nmcd.hdc, hBoldGui);
                    return CDRF_NEWFONT;
                }
                return CDRF_DODEFAULT;
            }
            return CDRF_DODEFAULT;
        }
        if (((NMHDR*)l)->hwndFrom == hTip && ((NMHDR*)l)->code == TTN_GETDISPINFOW) {     // the info icon's text, made on hover
            g_previewText = PreviewText();
            ((NMTTDISPINFOW*)l)->lpszText = (LPWSTR)g_previewText.c_str();
            return 0;
        }
        if (((NMHDR*)l)->hwndFrom == hTab && ((NMHDR*)l)->code == TCN_SELCHANGING && TabCtrl_GetCurSel(hTab) == TAB_CONFIG)
            SaveSettings();                           // leaving the Config tab (it is still the selected one here): keep the changes
        if (((NMHDR*)l)->hwndFrom == hTab && ((NMHDR*)l)->code == TCN_SELCHANGE) {
            ShowTab();
            Update();                                 // the Fill list is only built while its tab shows
            SetFocus(hInput);   // keep typing into the word bar after switching tabs
        }
        return 0;
    case WM_DESTROY:
        SaveSettings();                           // controls still exist here
        ++g_gen;                                  // tell any background load to stop
        DeleteObject(hFont);
        DeleteObject(hBigFont);
        DeleteObject(hSmallFont);
        DeleteObject(hRandFont);
        DeleteObject(hBoldGui);
        DeleteObject(hBmpShuf);
        DeleteObject(hBmpAZ);
        DeleteObject(hBmpZA);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hIcon = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                 GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    wc.hIconSm = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                   GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"CrossTools";
    RegisterClassExW(&wc);

    HWND h = CreateWindowW(L"CrossTools", L"CrossTools", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 540, 620, nullptr, nullptr, hi, nullptr);
    ShowWindow(h, show);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
