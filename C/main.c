/*
 * main.c - Point d'entrée, configuration (.ini), chemins, presse-papiers et sons
 */
#include "wincapture.h"

static const GUID MY_FOLDERID_Pictures       = { 0x33E28130, 0x4E1E, 0x4676, { 0x83, 0x5A, 0x98, 0x39, 0x5C, 0x3B, 0xC3, 0xBB } };
static const GUID MY_CLSID_WICImagingFactory = { 0xCACAF262, 0x9370, 0x4615, { 0xA1, 0x3B, 0x9F, 0x55, 0x39, 0xDA, 0x4C, 0x0A } };
static const GUID MY_IID_IWICImagingFactory  = { 0xEC5EC8A9, 0xC395, 0x4314, { 0x9C, 0x77, 0x54, 0xD7, 0xA9, 0x35, 0xFF, 0x70 } };
static const GUID MY_GUID_WICPixelFormat32bppBGRA = { 0x6FDDC324, 0x4E03, 0x4BFE, { 0xB1, 0x85, 0x3D, 0x77, 0x76, 0x8D, 0xC9, 0x0F } };

int g_dpi = 96;

CFG g_cfg = {
    0, 8000, 30, 1, 160, 0, 90, 1, 0, 1,
    0x0653, 0x0652, RGB(45, 45, 48), RGB(45, 45, 48),
    0, -1, 1, 0, 18, 20, 22, 0x0646, 0,
    0, 0, 1, 0, 0, 0x0650
};

SIZEP g_sizes[MAXS];
int g_sizeN;
WCHAR g_ini[MAX_PATH];
WCHAR g_soundPath[MAX_PATH], g_soundRecPath[MAX_PATH];
WCHAR g_nameFmt[128] = L"capture_%Y%m%d_%H%M%S";
WCHAR g_outDir[MAX_PATH];
WCHAR g_lastFile[MAX_PATH], g_lastShot[MAX_PATH];

HINSTANCE g_inst;
HWND g_main, g_cfgwnd, g_szwnd, g_frame, g_hl, g_mag;
HFONT g_font;
HICON g_icoIdle, g_icoRec;
NOTIFYICONDATAW g_nid;
UINT g_wmTaskbar, g_wmShowMe;
DWORD g_cfgHid;
BOOL g_cfgModal;
volatile BOOL g_rec;
BOOL g_picking;
BOOL g_frameOn = TRUE;
BOOL g_uiHidden;
BOOL g_full;
RECT g_fullRect;
WCHAR g_status[96];
RECT g_textRc;
BOOL g_barExpanded, g_menuOpen;
int g_shotWait;

static const WCHAR *const g_str[S_COUNT][4] = {
#define X(id, en, fr, es, de) { en, fr, es, de },
    STRINGS(X)
#undef X
};

const WCHAR *T(int id)
{
    int l = (g_cfg.lang >= 0 && g_cfg.lang < 4) ? g_cfg.lang : 0;
    const WCHAR *t = (id >= 0 && id < S_COUNT) ? g_str[id][l] : NULL;
    if (!t || !t[0]) t = (id >= 0 && id < S_COUNT) ? g_str[id][0] : L"";
    return t;
}

static int detect_lang(void)
{
    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
    case LANG_FRENCH:  return 1;
    case LANG_SPANISH: return 2;
    case LANG_GERMAN:  return 3;
    default:           return 0;
    }
}

static const struct { const WCHAR *k; int *v; } g_keys[] = {
    { L"vcodec", &g_cfg.vcodec }, { L"vkbps", &g_cfg.vkbps }, { L"fps", &g_cfg.fps },
    { L"audio", &g_cfg.audio },   { L"akbps", &g_cfg.akbps }, { L"imgfmt", &g_cfg.imgfmt },
    { L"jpgq", &g_cfg.jpgq },     { L"cursor", &g_cfg.cursor }, { L"sound", &g_cfg.sound }, { L"vfr", &g_cfg.vfr },
    { L"hk_image", &g_cfg.hkShot }, { L"hk_video", &g_cfg.hkRec },
    { L"col_cadre", &g_cfg.colFrame }, { L"col_barre", &g_cfg.colTitle },
    { L"transparence", &g_cfg.transp }, { L"lang", &g_cfg.lang }, { L"gpu", &g_cfg.gpu },
    { L"rc_mode", &g_cfg.rcmode }, { L"qp_i", &g_cfg.qpI }, { L"qp_p", &g_cfg.qpP }, { L"qp_b", &g_cfg.qpB },
    { L"hk_full", &g_cfg.hkFull }, { L"sound_rec", &g_cfg.soundRec },
    { L"mic", &g_cfg.mic }, { L"clipboard", &g_cfg.clip }, { L"exclude_ui", &g_cfg.excl },
    { L"delay", &g_cfg.delay }, { L"clicks", &g_cfg.clicks }, { L"hk_pause", &g_cfg.hkPause },
};

void cfg_load(void)
{
    for (int i = 0; i < (int)ARRAYSIZE(g_keys); i++)
        *g_keys[i].v = GetPrivateProfileIntW(L"Encodage", g_keys[i].k, *g_keys[i].v, g_ini);
    GetPrivateProfileStringW(L"Son", L"fichier", L"", g_soundPath, MAX_PATH, g_ini);
    GetPrivateProfileStringW(L"Son", L"fichier_video", L"", g_soundRecPath, MAX_PATH, g_ini);
    GetPrivateProfileStringW(L"Dossiers", L"dossier", L"", g_outDir, MAX_PATH, g_ini);
    if (!g_outDir[0]) {
        GetPrivateProfileStringW(L"Dossiers", L"videos", L"", g_outDir, MAX_PATH, g_ini);
        if (!g_outDir[0]) GetPrivateProfileStringW(L"Dossiers", L"images", L"", g_outDir, MAX_PATH, g_ini);
    }
    GetPrivateProfileStringW(L"Fichiers", L"format", L"capture_%Y%m%d_%H%M%S", g_nameFmt, ARRAYSIZE(g_nameFmt), g_ini);
}

void cfg_save(void)
{
    WCHAR b[16];
    for (int i = 0; i < (int)ARRAYSIZE(g_keys); i++) {
        swprintf_s(b, ARRAYSIZE(b), L"%d", *g_keys[i].v);
        WritePrivateProfileStringW(L"Encodage", g_keys[i].k, b, g_ini);
    }
    WritePrivateProfileStringW(L"Son", L"fichier", g_soundPath, g_ini);
    WritePrivateProfileStringW(L"Son", L"fichier_video", g_soundRecPath, g_ini);
    WritePrivateProfileStringW(L"Dossiers", L"dossier", g_outDir, g_ini);
    WritePrivateProfileStringW(L"Dossiers", L"images", NULL, g_ini);
    WritePrivateProfileStringW(L"Dossiers", L"videos", NULL, g_ini);
    WritePrivateProfileStringW(L"Fichiers", L"format", g_nameFmt, g_ini);
}

static const struct { const WCHAR *n; int w, h; } g_defSizes[] = {
    { L"360p", 640, 360 }, { L"480p", 854, 480 }, { L"720p (HD)", 1280, 720 },
    { L"1080p (Full HD)", 1920, 1080 }, { L"1440p (2K)", 2560, 1440 }, { L"2160p (4K)", 3840, 2160 }
};

void sizes_default(void)
{
    g_sizeN = (int)ARRAYSIZE(g_defSizes);
    for (int i = 0; i < g_sizeN; i++) {
        wcsncpy_s(g_sizes[i].name, ARRAYSIZE(g_sizes[i].name), g_defSizes[i].n, _TRUNCATE);
        g_sizes[i].w = g_defSizes[i].w; g_sizes[i].h = g_defSizes[i].h;
        g_sizes[i].x = g_sizes[i].y = 0; g_sizes[i].hasPos = FALSE;
    }
}

void sizes_load(void)
{
    int n = (int)GetPrivateProfileIntW(L"Tailles", L"n", -1, g_ini);
    WCHAR key[16], v[128];
    sizes_default();
    if (n < 0) return;
    if (n > MAXS) n = MAXS;
    g_sizeN = 0;
    for (int i = 0; i < n; i++) {
        WCHAR *f[5], *q = v; int nf = 0, w, h;
        swprintf_s(key, ARRAYSIZE(key), L"t%d", i);
        GetPrivateProfileStringW(L"Tailles", key, L"", v, ARRAYSIZE(v), g_ini);
        f[nf++] = q;
        while (nf < 5 && (q = wcschr(q, L'|')) != NULL) { *q++ = 0; f[nf++] = q; }
        if (nf < 3) continue;
        w = _wtoi(f[1]); h = _wtoi(f[2]);
        if (w < 64 || h < 36 || w > 16000 || h > 16000) continue;
        wcsncpy_s(g_sizes[g_sizeN].name, ARRAYSIZE(g_sizes[g_sizeN].name), f[0], _TRUNCATE);
        g_sizes[g_sizeN].w = w; g_sizes[g_sizeN].h = h;
        g_sizes[g_sizeN].hasPos = (nf >= 5);
        g_sizes[g_sizeN].x = g_sizes[g_sizeN].hasPos ? _wtoi(f[3]) : 0;
        g_sizes[g_sizeN].y = g_sizes[g_sizeN].hasPos ? _wtoi(f[4]) : 0;
        g_sizeN++;
    }
}

void sizes_save(void)
{
    WCHAR key[16], v[128];
    WritePrivateProfileStringW(L"Tailles", NULL, NULL, g_ini);
    swprintf_s(v, ARRAYSIZE(v), L"%d", g_sizeN);
    WritePrivateProfileStringW(L"Tailles", L"n", v, g_ini);
    for (int i = 0; i < g_sizeN; i++) {
        swprintf_s(key, ARRAYSIZE(key), L"t%d", i);
        if (g_sizes[i].hasPos)
            swprintf_s(v, ARRAYSIZE(v), L"%s|%d|%d|%d|%d", g_sizes[i].name, g_sizes[i].w, g_sizes[i].h,
                       g_sizes[i].x, g_sizes[i].y);
        else
            swprintf_s(v, ARRAYSIZE(v), L"%s|%d|%d", g_sizes[i].name, g_sizes[i].w, g_sizes[i].h);
        WritePrivateProfileStringW(L"Tailles", key, v, g_ini);
    }
}

/* ------------------------------------------------------------------ */
/* Chemins, noms de fichiers, presse-papiers et ouverture              */
/* ------------------------------------------------------------------ */
static void known_dir(REFKNOWNFOLDERID id, WCHAR *out)
{
    PWSTR p = NULL;
    out[0] = 0;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, NULL, &p))) {
        wcsncpy_s(out, MAX_PATH, p, _TRUNCATE);
        CoTaskMemFree(p);
    }
}

void dir_resolve(WCHAR *dir)
{
    if (g_outDir[0]) {
        wcsncpy_s(dir, MAX_PATH, g_outDir, _TRUNCATE);
    } else {
        known_dir(&MY_FOLDERID_Pictures, dir);
        PathAppendW(dir, L"Captures");
    }
}

static void name_clean(WCHAR *s)
{
    int n;
    for (WCHAR *p = s; *p; p++)
        if (*p < 32 || wcschr(L"\\/:*?\"<>|", *p)) *p = L'_';
    n = (int)wcslen(s);
    while (n > 0 && (s[n - 1] == L' ' || s[n - 1] == L'.')) s[--n] = 0;
}

static void file_stem(WCHAR *out, int n)
{
    SYSTEMTIME t; RECT r; POINT c; HWND w;
    WCHAR title[80] = L"", proc[64] = L"", tok[16], one[2] = L"";
    const WCHAR *p, *add;
    WCHAR *o = out; int left = n - 1;

    GetLocalTime(&t);
    get_region(&r);
    c.x = (r.left + r.right) / 2; c.y = (r.top + r.bottom) / 2;
    w = WindowFromPoint(c);
    if (w) w = GetAncestor(w, GA_ROOT);
    if (w && w != g_main) {
        DWORD pid = 0; HANDLE hp; DWORD_PTR res = 0;
        if (!SendMessageTimeoutW(w, WM_GETTEXT, ARRAYSIZE(title), (LPARAM)title,
                                 SMTO_ABORTIFHUNG | SMTO_BLOCK, 200, &res)) title[0] = 0;
        GetWindowThreadProcessId(w, &pid);
        hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hp) {
            WCHAR full[MAX_PATH]; DWORD sz = ARRAYSIZE(full);
            if (QueryFullProcessImageNameW(hp, 0, full, &sz)) {
                wcsncpy_s(proc, ARRAYSIZE(proc), PathFindFileNameW(full), _TRUNCATE);
                PathRemoveExtensionW(proc);
            }
            CloseHandle(hp);
        }
    }
    for (p = g_nameFmt[0] ? g_nameFmt : L"capture_%Y%m%d_%H%M%S"; *p && left > 0; p++) {
        add = NULL;
        if (*p == L'%' && p[1]) {
            p++;
            switch (*p) {
            case L'Y': swprintf_s(tok, ARRAYSIZE(tok), L"%04d", t.wYear); add = tok; break;
            case L'y': swprintf_s(tok, ARRAYSIZE(tok), L"%02d", t.wYear % 100); add = tok; break;
            case L'm': swprintf_s(tok, ARRAYSIZE(tok), L"%02d", t.wMonth); add = tok; break;
            case L'd': swprintf_s(tok, ARRAYSIZE(tok), L"%02d", t.wDay); add = tok; break;
            case L'H': swprintf_s(tok, ARRAYSIZE(tok), L"%02d", t.wHour); add = tok; break;
            case L'M': swprintf_s(tok, ARRAYSIZE(tok), L"%02d", t.wMinute); add = tok; break;
            case L'S': swprintf_s(tok, ARRAYSIZE(tok), L"%02d", t.wSecond); add = tok; break;
            case L'f': swprintf_s(tok, ARRAYSIZE(tok), L"%03d", t.wMilliseconds); add = tok; break;
            case L't': add = title[0] ? title : L"sans-titre"; break;
            case L'p': add = proc[0] ? proc : L"programme"; break;
            case L'%': add = L"%"; break;
            default: tok[0] = L'%'; tok[1] = *p; tok[2] = 0; add = tok; break;
            }
        } else {
            one[0] = *p; one[1] = 0; add = one;
        }
        while (*add && left > 0) { *o++ = *add++; left--; }
    }
    *o = 0;
    name_clean(out);
    if (!out[0]) wcsncpy_s(out, n, L"capture", _TRUNCATE);
}

void make_path(BOOL video, const WCHAR *ext, WCHAR *out)
{
    WCHAR dir[MAX_PATH], stem[160]; int room, i;
    dir_resolve(dir);
    (void)video;
    SHCreateDirectoryExW(NULL, dir, NULL);
    file_stem(stem, ARRAYSIZE(stem));
    room = MAX_PATH - (int)wcslen(dir) - (int)wcslen(ext) - 16;
    if (room < 8) room = 8;
    if ((int)wcslen(stem) > room) stem[room] = 0;
    swprintf_s(out, MAX_PATH, L"%s\\%s.%s", dir, stem, ext);
    for (i = 2; PathFileExistsW(out) && i < 1000; i++)
        swprintf_s(out, MAX_PATH, L"%s\\%s_%d.%s", dir, stem, i, ext);
}

BOOL copy_dib_clipboard(const GRAB *g)
{
    DWORD sz; HGLOBAL hm; BYTE *dst; BITMAPINFOHEADER bih; BOOL ok = FALSE;
    if (!g || !g->bits || g->w <= 0 || g->h <= 0) return FALSE;
    sz = (DWORD)g->w * (DWORD)g->h * 4u;
    hm = GlobalAlloc(GMEM_MOVEABLE, sizeof bih + sz);
    if (!hm) return FALSE;
    dst = (BYTE *)GlobalLock(hm);
    if (!dst) { GlobalFree(hm); return FALSE; }
    ZeroMemory(&bih, sizeof bih);
    bih.biSize = sizeof bih;
    bih.biWidth = g->w;
    bih.biHeight = g->h;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = sz;
    memcpy(dst, &bih, sizeof bih);
    memcpy(dst + sizeof bih, g->bits, sz);
    GlobalUnlock(hm);
    if (OpenClipboard(g_main)) {
        EmptyClipboard();
        ok = SetClipboardData(CF_DIB, hm) != NULL;
        CloseClipboard();
    }
    if (!ok) GlobalFree(hm);
    return ok;
}

void open_capture_dir(void)
{
    WCHAR dir[MAX_PATH];
    dir_resolve(dir);
    SHCreateDirectoryExW(NULL, dir, NULL);
    ShellExecuteW(NULL, L"open", dir, NULL, NULL, SW_SHOWNORMAL);
}

void open_last_capture(void)
{
    if (g_lastFile[0] && PathFileExistsW(g_lastFile))
        ShellExecuteW(NULL, L"open", g_lastFile, NULL, NULL, SW_SHOWNORMAL);
    else
        open_capture_dir();
}

void copy_last_shot(void)
{
    IWICImagingFactory *f = NULL; IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *fr = NULL; IWICFormatConverter *fc = NULL;
    UINT w = 0, h = 0;
    if (!g_lastShot[0] || !PathFileExistsW(g_lastShot)) return;
    if (SUCCEEDED(CoCreateInstance(&MY_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                 &MY_IID_IWICImagingFactory, (void **)&f)) &&
        SUCCEEDED(IWICImagingFactory_CreateDecoderFromFilename(f, g_lastShot, NULL, GENERIC_READ,
                                                               WICDecodeMetadataCacheOnDemand, &dec)) &&
        SUCCEEDED(IWICBitmapDecoder_GetFrame(dec, 0, &fr)) &&
        SUCCEEDED(IWICImagingFactory_CreateFormatConverter(f, &fc)) &&
        SUCCEEDED(IWICFormatConverter_Initialize(fc, (IWICBitmapSource *)fr, &MY_GUID_WICPixelFormat32bppBGRA,
                                                 WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom)) &&
        SUCCEEDED(IWICBitmapSource_GetSize((IWICBitmapSource *)fc, &w, &h)) && w > 0 && h > 0) {
        DWORD stride = w * 4u, sz = stride * h;
        HGLOBAL hm = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + sz);
        if (hm) {
            BYTE *dst = (BYTE *)GlobalLock(hm);
            if (dst) {
                BITMAPINFOHEADER *bih = (BITMAPINFOHEADER *)dst;
                BYTE *topdown = (BYTE *)malloc(sz);
                ZeroMemory(bih, sizeof *bih);
                bih->biSize = sizeof *bih; bih->biWidth = (LONG)w; bih->biHeight = (LONG)h;
                bih->biPlanes = 1; bih->biBitCount = 32; bih->biCompression = BI_RGB; bih->biSizeImage = sz;
                if (topdown && SUCCEEDED(IWICBitmapSource_CopyPixels((IWICBitmapSource *)fc, NULL, stride, sz, topdown))) {
                    BYTE *px = dst + sizeof *bih;
                    for (UINT y = 0; y < h; y++)
                        memcpy(px + (size_t)y * stride, topdown + (size_t)(h - 1 - y) * stride, stride);
                    GlobalUnlock(hm);
                    if (OpenClipboard(g_main)) {
                        EmptyClipboard();
                        if (!SetClipboardData(CF_DIB, hm)) GlobalFree(hm);
                        CloseClipboard();
                    } else GlobalFree(hm);
                } else {
                    GlobalUnlock(hm); GlobalFree(hm);
                }
                free(topdown);
            } else GlobalFree(hm);
        }
    }
    REL(fc); REL(fr); REL(dec); REL(f);
}

int ini_int(const WCHAR *sec, const WCHAR *key, int def)
{
    WCHAR b[24];
    GetPrivateProfileStringW(sec, key, L"", b, ARRAYSIZE(b), g_ini);
    return b[0] ? (int)wcstol(b, NULL, 10) : def;
}

void geo_save(void)
{
    RECT r, br; WCHAR b[16];
    const WCHAR *k[4] = { L"x", L"y", L"w", L"h" };
    int v[4];
    if (!g_main || !g_frame) return;
    GetWindowRect(g_frame, &r);
    v[0] = r.left; v[1] = r.top; v[2] = r.right - r.left; v[3] = r.bottom - r.top;
    for (int i = 0; i < 4; i++) {
        swprintf_s(b, ARRAYSIZE(b), L"%d", v[i]);
        WritePrivateProfileStringW(L"Fenetre", k[i], b, g_ini);
    }
    WritePrivateProfileStringW(L"Fenetre", L"frame", g_frameOn ? L"1" : L"0", g_ini);
    GetWindowRect(g_main, &br);
    swprintf_s(b, ARRAYSIZE(b), L"%d", (int)br.left);  WritePrivateProfileStringW(L"Fenetre", L"bx", b, g_ini);
    swprintf_s(b, ARRAYSIZE(b), L"%d", (int)br.top);   WritePrivateProfileStringW(L"Fenetre", L"by", b, g_ini);
}

BOOL geo_load(RECT *r)
{
    int x = ini_int(L"Fenetre", L"x", 0), y = ini_int(L"Fenetre", L"y", 0);
    int w = ini_int(L"Fenetre", L"w", 0), h = ini_int(L"Fenetre", L"h", 0);
    if (w <= 0 || h <= 0) return FALSE;
    r->left = x; r->top = y; r->right = x + w; r->bottom = y + h;
    return MonitorFromRect(r, MONITOR_DEFAULTTONULL) != NULL;
}

/* ------------------------------------------------------------------ */
/* Sons & icônes tray                                                  */
/* ------------------------------------------------------------------ */
#define SH_RATE 22050
#define SH_N    (SH_RATE * 18 / 100)
static BYTE g_wav[44 + SH_N * 2];
static BOOL g_wavOk;

static void put32(BYTE *p, UINT32 v) { p[0] = (BYTE)v; p[1] = (BYTE)(v >> 8); p[2] = (BYTE)(v >> 16); p[3] = (BYTE)(v >> 24); }
static void put16(BYTE *p, UINT16 v) { p[0] = (BYTE)v; p[1] = (BYTE)(v >> 8); }

static void make_shutter(void)
{
    UINT32 seed = 12345u;
    memcpy(g_wav, "RIFF", 4);          put32(g_wav + 4, 36 + SH_N * 2);
    memcpy(g_wav + 8, "WAVEfmt ", 8);  put32(g_wav + 16, 16);
    put16(g_wav + 20, 1);              put16(g_wav + 22, 1);
    put32(g_wav + 24, SH_RATE);        put32(g_wav + 28, SH_RATE * 2);
    put16(g_wav + 32, 2);              put16(g_wav + 34, 16);
    memcpy(g_wav + 36, "data", 4);     put32(g_wav + 40, SH_N * 2);
    for (int i = 0; i < SH_N; i++) {
        double t = (double)i / SH_RATE, env = 0.0, nz, v;
        if (t < 0.07) env = exp(-t * 80.0);
        if (t >= 0.09) env += 0.8 * exp(-(t - 0.09) * 60.0);
        seed = seed * 1664525u + 1013904223u;
        nz = ((seed >> 16) & 0x7FFF) / 16384.0 - 1.0;
        v = env * (0.7 * nz + 0.3 * sin(6.283185307 * (t < 0.09 ? 2200.0 : 1500.0) * t));
        if (v > 1.0) v = 1.0; else if (v < -1.0) v = -1.0;
        put16(g_wav + 44 + i * 2, (UINT16)(INT16)(v * 26000.0));
    }
    g_wavOk = TRUE;
}

static void play_sound(int mode, const WCHAR *path)
{
    switch (mode) {
    case 1:
        if (!g_wavOk) make_shutter();
        PlaySoundW((LPCWSTR)g_wav, NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
        break;
    case 2:
        if (path[0]) {
            PlaySoundW(path, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
            break;
        }
        /* fall through */
    case 0:
        MessageBeep(MB_OK);
        break;
    default:
        break;
    }
}

void play_shutter(void)   { play_sound(g_cfg.sound, g_soundPath); }
void play_rec_sound(void) { play_sound(g_cfg.soundRec, g_soundRecPath); }

UINT64 sound_len100(int mode, const WCHAR *path)
{
    UINT64 ms = 700;
    if (mode == 3) return 0;
    if (mode == 1) {
        ms = 250;
    } else if (mode == 2 && path[0]) {
        HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        ms = 1000;
        if (f != INVALID_HANDLE_VALUE) {
            BYTE hd[4096]; DWORD n = 0;
            if (ReadFile(f, hd, sizeof hd, &n, NULL) && n > 44 && !memcmp(hd, "RIFF", 4)) {
                DWORD pos = 12, avg = 0, data = 0, sz;
                while (pos + 8 <= n) {
                    memcpy(&sz, hd + pos + 4, 4);
                    if (!memcmp(hd + pos, "fmt ", 4) && pos + 8 + 12 <= n) memcpy(&avg, hd + pos + 16, 4);
                    if (!memcmp(hd + pos, "data", 4)) { data = sz; break; }
                    pos += 8 + sz + (sz & 1);
                }
                if (avg && data) ms = (UINT64)data * 1000 / avg;
            }
            CloseHandle(f);
        }
        if (ms > 3000) ms = 3000;
    }
    return (ms + 100) * 10000ULL;
}

HWND mk(HWND p, const WCHAR *cls, const WCHAR *txt, DWORD st, int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowExW(0, cls, txt, WS_CHILD | WS_VISIBLE | st, x, y, w, h,
                             p, (HMENU)(INT_PTR)id, g_inst, NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}

void hotkey_handle(WPARAM id)
{
    if (id == HK_SHOT)       shot_trigger();
    else if (id == HK_REC)   toggle_rec();
    else if (id == HK_ESC)   pick_end();
    else if (id == HK_FULL)  toggle_full();
    else if (id == HK_PAUSE) rec_pause_toggle();
}

/* ------------------------------------------------------------------ */
/* Point d'entrée                                                      */
/* ------------------------------------------------------------------ */
int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hprev, PWSTR cmd, int show)
{
    WNDCLASSEXW wc; MSG msg; WCHAR dir[MAX_PATH]; NONCLIENTMETRICSW ncm;
    (void)hprev; (void)cmd; (void)show;
    g_inst = hi;

    g_wmShowMe = RegisterWindowMessageW(L"WinCapture.ShowMe");
    CreateMutexW(NULL, TRUE, L"Local\\WinCapture.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = FindWindowW(L"WinCapMain", NULL);
        if (other) PostMessageW(other, g_wmShowMe, 0, 0);
        return 0;
    }

    {
        BOOL (WINAPI *setCtx)(HANDLE) =
            (BOOL (WINAPI *)(HANDLE))GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext");
        if (!(setCtx && setCtx((HANDLE)-4))) SetProcessDPIAware();
    }
    { HDC sdc = GetDC(NULL); g_dpi = GetDeviceCaps(sdc, LOGPIXELSX); ReleaseDC(NULL, sdc); }
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    icons_init();
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    {
        INITCOMMONCONTROLSEX icc;
        icc.dwSize = sizeof icc; icc.dwICC = ICC_HOTKEY_CLASS | ICC_BAR_CLASSES | ICC_WIN95_CLASSES;
        InitCommonControlsEx(&icc);
    }
    timeBeginPeriod(1);

    GetModuleFileNameW(NULL, dir, MAX_PATH);
    PathRemoveFileSpecW(dir);
    PathCombineW(g_ini, dir, L"capture.ini");
    cfg_load();
    if (g_cfg.lang < 0 || g_cfg.lang > 3) g_cfg.lang = detect_lang();
    sizes_load();
    cfg_save();
    sizes_save();

    ZeroMemory(&ncm, sizeof ncm);
    ncm.cbSize = sizeof ncm;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    g_icoIdle = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(IDI_APP_IDLE), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    if (!g_icoIdle) g_icoIdle = LoadIconW(NULL, IDI_APPLICATION);
    g_icoRec = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(IDI_APP_REC), IMAGE_ICON,
                                 GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    if (!g_icoRec) g_icoRec = g_icoIdle;
    g_wmTaskbar = RegisterWindowMessageW(L"TaskbarCreated");

    ZeroMemory(&wc, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = MainProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(IDI_APP_IDLE));
    if (!wc.hIcon) wc.hIcon = g_icoIdle;
    wc.lpszClassName = L"WinCapMain";
    RegisterClassExW(&wc);

    wc.lpfnWndProc = CfgProc;
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.hIcon = NULL;
    wc.lpszClassName = L"WinCapCfg";
    RegisterClassExW(&wc);

    wc.lpfnWndProc = SzProc;
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.lpszClassName = L"WinCapSizes";
    RegisterClassExW(&wc);

    wc.lpfnWndProc = DefWindowProcW;
    wc.hbrBackground = CreateSolidBrush(RGB(255, 60, 60));
    wc.lpszClassName = L"WinCapHl";
    RegisterClassExW(&wc);
    g_hl = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"WinCapHl", NULL,
                           WS_POPUP, 0, 0, 10, 10, NULL, NULL, hi, NULL);

    wc.lpfnWndProc = MagProc;
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"WinCapMag";
    RegisterClassExW(&wc);
    g_mag = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                            L"WinCapMag", NULL, WS_POPUP, 0, 0, S(136), S(156), NULL, NULL, hi, NULL);

    wc.lpfnWndProc = FrameProc;
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"WinCapFrame";
    RegisterClassExW(&wc);

    g_main = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, L"WinCapMain", T(S_APP),
                             WS_POPUP | WS_CLIPCHILDREN, 100, 100, bar_width(), BAR_H, NULL, NULL, hi, NULL);
    SendMessageW(g_main, WM_SETICON, ICON_BIG, (LPARAM)wc.hIcon);
    SendMessageW(g_main, WM_SETICON, ICON_SMALL, (LPARAM)g_icoIdle);
    g_frame = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
                              L"WinCapFrame", NULL, WS_POPUP | WS_THICKFRAME, 100, 140, 800, 500,
                              NULL, NULL, hi, NULL);
    apply_opacity();
    create_aux_windows();
    apply_exclude_capture();

    {
        RECT gr, fr, br; int bx, by;
        g_frameOn = ini_int(L"Fenetre", L"frame", 1) != 0;
        if (geo_load(&gr))
            SetWindowPos(g_frame, NULL, gr.left, gr.top, gr.right - gr.left, gr.bottom - gr.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        else
            apply_wh(1280, 720);
        GetWindowRect(g_frame, &fr);
        bx = ini_int(L"Fenetre", L"bx", 0x7fffffff);
        by = ini_int(L"Fenetre", L"by", 0x7fffffff);
        br.left = bx; br.top = by; br.right = bx + bar_width(); br.bottom = by + BAR_H;
        if (bx == 0x7fffffff || by == 0x7fffffff || !MonitorFromRect(&br, MONITOR_DEFAULTTONULL)) {
            MONITORINFO mi;
            mi.cbSize = sizeof mi;
            bx = fr.left + ((fr.right - fr.left) - bar_width()) / 2;
            by = fr.top - BAR_H - S(4);
            if (GetMonitorInfoW(MonitorFromRect(&fr, MONITOR_DEFAULTTONEAREST), &mi) && by < mi.rcMonitor.top)
                by = mi.rcMonitor.top;
        }
        SetWindowPos(g_main, HWND_TOPMOST, bx, by, bar_width(), BAR_H, SWP_NOACTIVATE);
    }
    bar_apply();
    frame_hole(g_frame);
    ui_apply();
    UpdateWindow(g_main);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && GetAncestor(msg.hwnd, GA_ROOT) == g_main &&
            (msg.wParam == VK_LEFT || msg.wParam == VK_RIGHT || msg.wParam == VK_UP || msg.wParam == VK_DOWN)) {
            SendMessageW(g_main, WM_KEYDOWN, msg.wParam, msg.lParam);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    timeEndPeriod(1);
    MFShutdown();
    CoUninitialize();
    return (int)msg.wParam;
}
