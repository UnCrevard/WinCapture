/*
 * ui.c - Barre flottante, cadre troué, icônes PNG/GDI, tray, préréglages et réglages
 */
#include "wincapture.h"

static const GUID UI_CLSID_WICImagingFactory      = { 0xCACAF262, 0x9370, 0x4615, { 0xA1, 0x3B, 0x9F, 0x55, 0x39, 0xDA, 0x4C, 0x0A } };
static const GUID UI_IID_IWICImagingFactory       = { 0xEC5EC8A9, 0xC395, 0x4314, { 0x9C, 0x77, 0x54, 0xD7, 0xA9, 0x35, 0xFF, 0x70 } };
static const GUID UI_GUID_WICPixelFormat32bppBGRA = { 0x6FDDC324, 0x4E03, 0x4BFE, { 0xB1, 0x85, 0x3D, 0x77, 0x76, 0x8D, 0xC9, 0x0F } };


static const GUID MY_CLSID_FileOpenDialog = { 0xDC1C5A9C, 0xE88A, 0x4DDE, { 0xA5, 0xA1, 0x60, 0xF8, 0x2A, 0x20, 0xAE, 0xF7 } };
static const GUID MY_IID_IFileOpenDialog  = { 0xD57C7288, 0xD4AD, 0x4768, { 0xBE, 0x02, 0x9D, 0x96, 0x95, 0x32, 0xD9, 0x60 } };
static const GUID MY_IID_IShellItem       = { 0x43826D1E, 0xE718, 0x42EE, { 0xBC, 0x55, 0xA1, 0xE2, 0x61, 0xC3, 0x7B, 0xFE } };

/* ------------------------------------------------------------------ */
/* Icônes PNG (ressources/fichiers) + icônes vectorielles GDI          */
/* ------------------------------------------------------------------ */
enum { ICO_SHUTTER, ICO_TARGET, ICO_GEAR, ICO_N };
static const int g_icoResId[ICO_N] = { IDR_ICO_SHUTTER, IDR_ICO_TARGET, IDR_ICO_GEAR };
static const WCHAR *const g_icoFile[ICO_N] = { L"ico_shutter.png", L"ico_target.png", L"ico_gear.png" };
static unsigned char *g_icoMask[ICO_N];
static int g_icoPx;

void icons_free(void)
{
    for (int k = 0; k < ICO_N; k++) {
        free(g_icoMask[k]);
        g_icoMask[k] = NULL;
    }
}

static unsigned char *load_png_mask(IWICImagingFactory *f, int resId, const WCHAR *fileName, int n)
{
    IWICStream *st = NULL;
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *fr = NULL;
    IWICFormatConverter *fc = NULL;
    HRESULT hr = E_FAIL;
    HRSRC hrsrc = FindResourceW(g_inst, MAKEINTRESOURCEW(resId), RT_RCDATA);
    unsigned char *mask = NULL;

    if (hrsrc) {
        HGLOBAL hglob = LoadResource(g_inst, hrsrc);
        DWORD sz = SizeofResource(g_inst, hrsrc);
        BYTE *ptr = hglob ? (BYTE *)LockResource(hglob) : NULL;
        if (ptr && sz > 0 && SUCCEEDED(IWICImagingFactory_CreateStream(f, &st)) &&
            SUCCEEDED(IWICStream_InitializeFromMemory(st, ptr, sz))) {
            hr = IWICImagingFactory_CreateDecoderFromStream(f, (IStream *)st, NULL, WICDecodeMetadataCacheOnDemand, &dec);
        }
    }
    if (FAILED(hr) && fileName) {
        WCHAR dir[MAX_PATH], path[MAX_PATH];
        GetModuleFileNameW(NULL, dir, MAX_PATH);
        PathRemoveFileSpecW(dir);
        PathCombineW(path, dir, fileName);
        hr = IWICImagingFactory_CreateDecoderFromFilename(f, path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec);
        if (FAILED(hr)) {
            WCHAR sub[MAX_PATH];
            PathCombineW(sub, dir, L"C");
            PathCombineW(path, sub, fileName);
            hr = IWICImagingFactory_CreateDecoderFromFilename(f, path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec);
        }
    }
    if (SUCCEEDED(hr) &&
        SUCCEEDED(IWICBitmapDecoder_GetFrame(dec, 0, &fr)) &&
        SUCCEEDED(IWICImagingFactory_CreateFormatConverter(f, &fc)) &&
        SUCCEEDED(IWICFormatConverter_Initialize(fc, (IWICBitmapSource *)fr, &UI_GUID_WICPixelFormat32bppBGRA,
                                                 WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom))) {
        UINT w = 0, h = 0;
        if (SUCCEEDED(IWICBitmapSource_GetSize((IWICBitmapSource *)fc, &w, &h)) && w > 0 && h > 0) {
            BYTE *bgra = (BYTE *)malloc((size_t)w * h * 4);
            if (bgra && SUCCEEDED(IWICBitmapSource_CopyPixels((IWICBitmapSource *)fc, NULL, w * 4, w * h * 4, bgra))) {
                BOOL useAlpha = FALSE;
                for (UINT i = 0; i < w * h; i++) {
                    if (bgra[i * 4 + 3] < 255) { useAlpha = TRUE; break; }
                }
                mask = (unsigned char *)malloc((size_t)n * n);
                if (mask) {
                    for (int y = 0; y < n; y++)
                        for (int x = 0; x < n; x++) {
                            int x0 = (int)((UINT64)x * w / (UINT64)n), x1 = (int)((UINT64)(x + 1) * w / (UINT64)n);
                            int y0 = (int)((UINT64)y * h / (UINT64)n), y1 = (int)((UINT64)(y + 1) * h / (UINT64)n);
                            int sum = 0, sx, sy;
                            if (x1 <= x0) x1 = x0 + 1;
                            if (y1 <= y0) y1 = y0 + 1;
                            if (x1 > (int)w) x1 = (int)w;
                            if (y1 > (int)h) y1 = (int)h;
                            for (sy = y0; sy < y1; sy++)
                                for (sx = x0; sx < x1; sx++) {
                                    const BYTE *p = &bgra[((size_t)sy * w + (size_t)sx) * 4];
                                    sum += useAlpha ? (int)p[3] : (((int)p[0] + (int)p[1] + (int)p[2]) / 3);
                                }
                            mask[y * n + x] = (unsigned char)(sum / ((x1 - x0) * (y1 - y0)));
                        }
                }
            }
            free(bgra);
        }
    }
    REL(fc); REL(fr); REL(dec); REL(st);
    return mask;
}

void icons_init(void)
{
    IWICImagingFactory *f = NULL;
    int n = BTN_H - 2 * S(2);
    if (n < 8) n = 8;
    g_icoPx = n;
    icons_free();
    if (SUCCEEDED(CoCreateInstance(&UI_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                   &UI_IID_IWICImagingFactory, (void **)&f))) {
        for (int k = 0; k < ICO_N; k++)
            g_icoMask[k] = load_png_mask(f, g_icoResId[k], g_icoFile[k], n);
        REL(f);
    }
}

static void draw_mask(HDC dc, int cx, int cy, const unsigned char *m, int n, COLORREF fg, COLORREF bg)
{
    BITMAPINFO bi; DWORD *px; int x, y;
    if (!m) return;
    px = (DWORD *)malloc((size_t)n * n * sizeof(DWORD));
    if (!px) return;
    for (y = 0; y < n; y++)
        for (x = 0; x < n; x++) {
            int a = m[y * n + x];
            int r = (GetRValue(fg) * a + GetRValue(bg) * (255 - a)) / 255;
            int g = (GetGValue(fg) * a + GetGValue(bg) * (255 - a)) / 255;
            int b = (GetBValue(fg) * a + GetBValue(bg) * (255 - a)) / 255;
            px[(n - 1 - y) * n + x] = ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
        }
    ZeroMemory(&bi, sizeof bi);
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = n;
    bi.bmiHeader.biHeight = n;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, cx - n / 2, cy - n / 2, n, n, 0, 0, 0, n, px, &bi, DIB_RGB_COLORS);
    free(px);
}

static void draw_icon(HDC dc, const RECT *br, int id, COLORREF fg, COLORREF bg, BOOL rec)
{
    int cx = (br->left + br->right) / 2, cy = (br->top + br->bottom) / 2;
    int ico = id == ID_SHOT ? ICO_SHUTTER : id == ID_PICK ? ICO_TARGET : id == ID_CFG ? ICO_GEAR : -1;
    if (id == ID_SHOT && g_shotWait > 0) {
        WCHAR num[8]; RECT tr = *br;
        swprintf_s(num, ARRAYSIZE(num), L"%d", g_shotWait);
        SelectObject(dc, g_font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 190, 40));
        DrawTextW(dc, num, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    if (ico >= 0) {
        draw_mask(dc, cx, cy, g_icoMask[ico], g_icoPx, fg, bg);
        if (ico == ICO_TARGET) {
            int rad = g_icoPx * 9 / 100;
            HPEN rp = CreatePen(PS_SOLID, 1, RGB(235, 60, 40));
            HBRUSH rb = CreateSolidBrush(RGB(235, 60, 40));
            HGDIOBJ op = SelectObject(dc, rp), ob = SelectObject(dc, rb);
            if (rad < 1) rad = 1;
            Ellipse(dc, cx - rad, cy - rad, cx + rad + 1, cy + rad + 1);
            SelectObject(dc, op); SelectObject(dc, ob);
            DeleteObject(rp); DeleteObject(rb);
        }
        return;
    }
    {
        int u = S(1) > 0 ? S(1) : 1;
        int r = (br->bottom - br->top) / 2 - 3 * u;
        COLORREF col = (id == ID_REC) ? RGB(225, 40, 40) : fg;
        HPEN pen = CreatePen(PS_SOLID, u, col), pen2 = CreatePen(PS_SOLID, 2 * u, col);
        HBRUSH solid = CreateSolidBrush(col);
        HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        if (r < 4) r = 4;
        if (id == ID_REC) {
            SelectObject(dc, solid);
            if (rec) Rectangle(dc, cx - r * 3 / 4, cy - r * 3 / 4, cx + r * 3 / 4 + 1, cy + r * 3 / 4 + 1);
            else     Ellipse(dc, cx - r * 4 / 5, cy - r * 4 / 5, cx + r * 4 / 5 + 1, cy + r * 4 / 5 + 1);
        } else if (id == ID_PAUSE) {
            SelectObject(dc, solid);
            if (rc.paused) {
                POINT tri[3];
                tri[0].x = cx - r * 2 / 3; tri[0].y = cy - r * 3 / 4;
                tri[1].x = cx - r * 2 / 3; tri[1].y = cy + r * 3 / 4;
                tri[2].x = cx + r * 3 / 4; tri[2].y = cy;
                Polygon(dc, tri, 3);
            } else {
                Rectangle(dc, cx - r * 3 / 5, cy - r * 3 / 4, cx - r / 5 + 1, cy + r * 3 / 4 + 1);
                Rectangle(dc, cx + r / 5, cy - r * 3 / 4, cx + r * 3 / 5 + 1, cy + r * 3 / 4 + 1);
            }
        } else if (id == ID_SIZE) {
            int L = r * 2 / 3 + 1, sx, sy;
            SelectObject(dc, pen2);
            for (sy = -1; sy <= 1; sy += 2)
                for (sx = -1; sx <= 1; sx += 2) {
                    MoveToEx(dc, cx + sx * r, cy + sy * (r - L), NULL);
                    LineTo(dc, cx + sx * r, cy + sy * r);
                    LineTo(dc, cx + sx * (r - L), cy + sy * r);
                }
        } else if (id == ID_BARE) {
            int rx = r, ry = r * 3 / 4;
            if (ry < 4) ry = 4;
            if (g_frameOn) {
                SelectObject(dc, pen2);
                Rectangle(dc, cx - rx, cy - ry, cx + rx + 1, cy + ry + 1);
            } else {
                LOGBRUSH lb = { BS_SOLID, col, 0 };
                HPEN dpen = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_SQUARE | PS_JOIN_MITER,
                                         (DWORD)(2 * u), &lb, 0, NULL);
                int x0 = cx - rx, x1 = cx + rx, y0 = cy - ry, y1 = cy + ry;
                int hx = rx / 3, cx_d = rx / 5, vy = ry / 2;
                if (hx < 2) hx = 2;
                if (cx_d < 1) cx_d = 1;
                if (vy < 2) vy = 2;
                SelectObject(dc, dpen ? dpen : pen2);
                MoveToEx(dc, x0, y0, NULL);           LineTo(dc, x0 + hx, y0);
                MoveToEx(dc, cx - cx_d, y0, NULL);    LineTo(dc, cx + cx_d, y0);
                MoveToEx(dc, x1 - hx, y0, NULL);      LineTo(dc, x1, y0);
                MoveToEx(dc, x0, y1, NULL);           LineTo(dc, x0 + hx, y1);
                MoveToEx(dc, cx - cx_d, y1, NULL);    LineTo(dc, cx + cx_d, y1);
                MoveToEx(dc, x1 - hx, y1, NULL);      LineTo(dc, x1, y1);
                MoveToEx(dc, x0, y0, NULL);           LineTo(dc, x0, y0 + vy);
                MoveToEx(dc, x0, y1 - vy, NULL);      LineTo(dc, x0, y1);
                MoveToEx(dc, x1, y0, NULL);           LineTo(dc, x1, y0 + vy);
                MoveToEx(dc, x1, y1 - vy, NULL);      LineTo(dc, x1, y1);
                if (dpen) { SelectObject(dc, op); DeleteObject(dpen); }
            }
        } else if (id == ID_CLOSE) {
            int half = r * 3 / 4;
            if (half < 4) half = 4;
            SelectObject(dc, pen2);
            MoveToEx(dc, cx - half, cy + r / 2, NULL);
            LineTo(dc, cx + half + 1, cy + r / 2);
        }
        SelectObject(dc, op);
        SelectObject(dc, ob);
        DeleteObject(pen); DeleteObject(pen2); DeleteObject(solid);
    }
}

/* ------------------------------------------------------------------ */
/* Tray                                                                */
/* ------------------------------------------------------------------ */
void tray_add(void)
{
    ZeroMemory(&g_nid, sizeof g_nid);
    g_nid.cbSize = sizeof g_nid;
    g_nid.hWnd = g_main;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = g_icoIdle;
    wcsncpy_s(g_nid.szTip, ARRAYSIZE(g_nid.szTip), T(S_STATUS_READY), _TRUNCATE);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void tray_set(HICON ic, const WCHAR *tip)
{
    g_nid.uFlags = NIF_ICON | NIF_TIP;
    g_nid.hIcon = ic;
    wcsncpy_s(g_nid.szTip, ARRAYSIZE(g_nid.szTip), tip, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void balloon(const WCHAR *title, const WCHAR *msg)
{
    NOTIFYICONDATAW n = g_nid;
    n.uFlags = NIF_INFO;
    wcsncpy_s(n.szInfoTitle, ARRAYSIZE(n.szInfoTitle), title, _TRUNCATE);
    wcsncpy_s(n.szInfo, ARRAYSIZE(n.szInfo), msg, _TRUNCATE);
    n.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

/* ------------------------------------------------------------------ */
/* Raccourcis & menu tailles                                           */
/* ------------------------------------------------------------------ */
static UINT hk_mods(int hk)
{
    BYTE f = HIBYTE((WORD)hk); UINT m = MOD_NOREPEAT;
    if (f & HOTKEYF_SHIFT)   m |= MOD_SHIFT;
    if (f & HOTKEYF_CONTROL) m |= MOD_CONTROL;
    if (f & HOTKEYF_ALT)     m |= MOD_ALT;
    return m;
}

void hk_text(int hk, WCHAR *out, int n)
{
    WCHAR k[32] = L""; BYTE f = HIBYTE((WORD)hk), vk = LOBYTE((WORD)hk);
    LONG sc;
    out[0] = 0;
    if (!vk) return;
    sc = (LONG)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC) << 16;
    if (f & HOTKEYF_EXT) sc |= 1L << 24;
    GetKeyNameTextW(sc, k, ARRAYSIZE(k));
    swprintf_s(out, n, L"%s%s%s%s", (f & HOTKEYF_CONTROL) ? T(S_MOD_CTRL) : L"",
               (f & HOTKEYF_ALT) ? T(S_MOD_ALT) : L"", (f & HOTKEYF_SHIFT) ? T(S_MOD_SHIFT) : L"", k);
}

void hotkeys_unregister(void)
{
    UnregisterHotKey(g_main, HK_SHOT);
    UnregisterHotKey(g_main, HK_REC);
    UnregisterHotKey(g_main, HK_FULL);
    UnregisterHotKey(g_main, HK_PAUSE);
}

void hotkeys_register(void)
{
    hotkeys_unregister();
    if (LOBYTE((WORD)g_cfg.hkShot) &&
        !RegisterHotKey(g_main, HK_SHOT, hk_mods(g_cfg.hkShot), LOBYTE((WORD)g_cfg.hkShot)))
        balloon(T(S_HK_T), T(S_HK_IMG));
    if (LOBYTE((WORD)g_cfg.hkRec) &&
        !RegisterHotKey(g_main, HK_REC, hk_mods(g_cfg.hkRec), LOBYTE((WORD)g_cfg.hkRec)))
        balloon(T(S_HK_T), T(S_HK_VID));
    if (LOBYTE((WORD)g_cfg.hkFull) &&
        !RegisterHotKey(g_main, HK_FULL, hk_mods(g_cfg.hkFull), LOBYTE((WORD)g_cfg.hkFull)))
        balloon(T(S_HK_T), T(S_HK_FULL));
    if (LOBYTE((WORD)g_cfg.hkPause) &&
        !RegisterHotKey(g_main, HK_PAUSE, hk_mods(g_cfg.hkPause), LOBYTE((WORD)g_cfg.hkPause)))
        balloon(T(S_HK_T), T(S_HK_PAUSE));
}

static HMENU size_menu(void)
{
    HMENU m = CreatePopupMenu(); WCHAR b[128], nm[70]; int cw, ch, cx, cy;
    cur_size(&cw, &ch);
    cur_pos(&cx, &cy);
    for (int i = 0; i < g_sizeN; i++) {
        SIZEP *p = &g_sizes[i];
        BOOL same = p->w == cw && p->h == ch && (!p->hasPos || (p->x == cx && p->y == cy));
        WCHAR *o = nm;
        for (const WCHAR *q = p->name; *q && o < nm + 64; q++) {
            if (*q == L'&') *o++ = L'&';
            *o++ = *q;
        }
        *o = 0;
        if (p->hasPos) swprintf_s(b, ARRAYSIZE(b), L"%s\t%d × %d  @ %d, %d", nm, p->w, p->h, p->x, p->y);
        else           swprintf_s(b, ARRAYSIZE(b), L"%s\t%d × %d", nm, p->w, p->h);
        AppendMenuW(m, MF_STRING | (g_rec ? MF_GRAYED : 0) | (same ? MF_CHECKED : 0), IDM_SIZE0 + i, b);
    }
    if (g_sizeN) AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    {
        WCHAR t[128], hk[48];
        hk_text(g_cfg.hkFull, hk, ARRAYSIZE(hk));
        swprintf_s(t, ARRAYSIZE(t), g_full ? T(S_MENU_FULL_OFF) : T(S_MENU_FULL_ON), hk);
        AppendMenuW(m, MF_STRING | (g_rec ? MF_GRAYED : 0) | (g_full ? MF_CHECKED : 0), IDM_FULL, t);
    }
    AppendMenuW(m, MF_STRING, IDM_SIZE_ADD, T(S_SIZE_ADD));
    AppendMenuW(m, MF_STRING, IDM_SIZE_EDIT, T(S_SIZE_EDIT));
    return m;
}

void tray_menu(void)
{
    POINT pt; HMENU m = CreatePopupMenu();
    WCHAR t1[96], t2[96], t3[96], hk[48];
    GetCursorPos(&pt);
    hk_text(g_cfg.hkShot, hk, ARRAYSIZE(hk));
    swprintf_s(t1, ARRAYSIZE(t1), T(S_MENU_SHOT), hk);
    hk_text(g_cfg.hkRec, hk, ARRAYSIZE(hk));
    swprintf_s(t2, ARRAYSIZE(t2), g_rec ? T(S_MENU_STOP) : T(S_MENU_START), hk);
    AppendMenuW(m, MF_STRING, IDM_SHOT, t1);
    AppendMenuW(m, MF_STRING, IDM_REC, t2);
    if (g_rec) {
        hk_text(g_cfg.hkPause, hk, ARRAYSIZE(hk));
        swprintf_s(t3, ARRAYSIZE(t3), rc.paused ? T(S_MENU_RESUME) : T(S_MENU_PAUSE), hk);
        AppendMenuW(m, MF_STRING, ID_PAUSE, t3);
    }
    AppendMenuW(m, MF_POPUP, (UINT_PTR)size_menu(), T(S_MENU_SIZE));
    {
        WCHAR tf[128];
        hk_text(g_cfg.hkFull, hk, ARRAYSIZE(hk));
        swprintf_s(tf, ARRAYSIZE(tf), g_full ? T(S_MENU_FULL_OFF) : T(S_MENU_FULL_ON), hk);
        AppendMenuW(m, MF_STRING | (g_rec ? MF_GRAYED : 0) | (g_full ? MF_CHECKED : 0), IDM_FULL, tf);
    }
    AppendMenuW(m, MF_STRING | (g_rec || g_full ? MF_GRAYED : 0), IDM_PICK, T(S_MENU_PICK));
    AppendMenuW(m, MF_STRING | (g_full ? MF_GRAYED : 0), IDM_BARE, g_frameOn ? T(S_MENU_FRAME_HIDE) : T(S_MENU_FRAME_SHOW));
    AppendMenuW(m, MF_STRING, IDM_SHOWHIDE, g_uiHidden ? T(S_MENU_WIN_SHOW) : T(S_MENU_WIN_HIDE));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_OPENDIR, T(S_MENU_OPENDIR));
    if (g_lastFile[0]) AppendMenuW(m, MF_STRING, IDM_OPENLAST, T(S_MENU_OPENLAST));
    if (g_lastShot[0]) AppendMenuW(m, MF_STRING, IDM_COPYLAST, T(S_MENU_COPYLAST));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_CFG, T(S_MENU_SETTINGS));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_QUIT, T(S_MENU_QUIT));
    SetForegroundWindow(g_main);
    TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, g_main, NULL);
    PostMessageW(g_main, WM_NULL, 0, 0);
    DestroyMenu(m);
}

/* ------------------------------------------------------------------ */
/* État de l'interface, taille, position, transparence et exclusion    */
/* ------------------------------------------------------------------ */
static const int g_barIds[8] = { ID_SHOT, ID_REC, ID_PAUSE, ID_SIZE, ID_PICK, ID_CFG, ID_BARE, ID_CLOSE };

void cur_size(int *w, int *h)
{
    RECT r;
    get_region(&r);
    *w = r.right - r.left;
    *h = r.bottom - r.top;
}

void cur_pos(int *x, int *y)
{
    RECT r;
    get_region(&r);
    *x = r.left;
    *y = r.top;
}

void update_title(void)
{
    WCHAR t[96]; int cw, ch, cx, cy;
    if (!g_main || !g_frame) return;
    cur_pos(&cx, &cy);
    if (g_picking) {
        wcscpy_s(t, ARRAYSIZE(t), T(S_PICK_HINT));
    } else if (g_shotWait > 0) {
        swprintf_s(t, ARRAYSIZE(t), T(S_COUNTDOWN), g_shotWait);
    } else if (g_rec) {
        UINT64 s = now100() / 10000000ULL;
        swprintf_s(t, ARRAYSIZE(t), L"%s %02u:%02u  %d × %d  @ %d, %d",
                   rc.paused ? L"II" : L"●", (UINT)(s / 60), (UINT)(s % 60), rc.w, rc.h, cx, cy);
    } else {
        cur_size(&cw, &ch);
        swprintf_s(t, ARRAYSIZE(t), L"%d × %d  @ %d, %d", cw, ch, cx, cy);
    }
    wcscpy_s(g_status, ARRAYSIZE(g_status), t);
    SetWindowTextW(g_main, t);
    InvalidateRect(g_main, &g_textRc, TRUE);
    if (g_shotWait >= 0) InvalidateRect(GetDlgItem(g_main, ID_SHOT), NULL, FALSE);
}

void ui_repaint(void)
{
    InvalidateRect(g_main, NULL, TRUE);
    if (g_frame) InvalidateRect(g_frame, NULL, TRUE);
}

void ui_update(void)
{
    SetDlgItemTextW(g_main, ID_REC, g_rec ? L"■ Stop" : L"● Rec");
    tray_set(g_rec ? g_icoRec : g_icoIdle, g_rec ? T(S_STATUS_REC) : T(S_STATUS_READY));
    SendMessageW(g_main, WM_SETICON, ICON_SMALL, (LPARAM)(g_rec ? g_icoRec : g_icoIdle));
    if (g_rec) SetTimer(g_main, TIMER_UI, 500, NULL); else KillTimer(g_main, TIMER_UI);
    bar_apply();
    ui_repaint();
    update_title();
}

void apply_wh(int w, int h)
{
    if (g_rec) return;
    if (g_full) toggle_full();
    SetWindowPos(g_frame, NULL, 0, 0, w + 2 * BORDER, h + FRAME_TOP + BORDER,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void apply_size(int i)
{
    SIZEP *p; RECT r;
    if (g_rec || i < 0 || i >= g_sizeN) return;
    if (g_full) toggle_full();
    p = &g_sizes[i];
    r.left = p->x; r.top = p->y; r.right = p->x + p->w; r.bottom = p->y + p->h;
    if (!p->hasPos || !MonitorFromRect(&r, MONITOR_DEFAULTTONULL)) {
        apply_wh(p->w, p->h);
        return;
    }
    pick_commit(&r);
}

static BOOL bar_shows(int id)
{
    switch (id) {
    case ID_SHOT: case ID_REC: return TRUE;
    case ID_PAUSE: return g_rec;
    default: return g_barExpanded;
    }
}

static int bar_w(BOOL expanded, BOOL rec)
{
    int n = 2 + (rec ? 1 : 0) + (expanded ? 5 : 0);
    int w = 2 * S(10) + S(8) + n * S(28) + (n - 1) * S(3);
    if (expanded) w += S(8) + S(8) + S(200);
    return w;
}

int bar_width(void) { return bar_w(TRUE, FALSE); }

void layout_bar(HWND h)
{
    RECT c; int x = S(10), y = S(3), W;
    GetClientRect(h, &c);
    W = c.right;
    for (int i = 0; i < 8; i++) {
        HWND b = GetDlgItem(h, g_barIds[i]);
        if (!bar_shows(g_barIds[i])) { ShowWindow(b, SW_HIDE); continue; }
        MoveWindow(b, x, y, S(28), BTN_H, TRUE);
        ShowWindow(b, SW_SHOWNA);
        x += S(28) + S(3);
        if (g_barIds[i] == ID_CFG) x += S(8);
    }
    if (g_barExpanded) {
        g_textRc.left = x + S(6);
        g_textRc.right = W - S(14);
        g_textRc.top = 0;
        g_textRc.bottom = BAR_H;
    } else {
        SetRectEmpty(&g_textRc);
    }
}

void bar_apply(void)
{
    RECT r; int w = bar_w(g_barExpanded, g_rec);
    if (!g_main) return;
    GetWindowRect(g_main, &r);
    if (r.right - r.left != w || r.bottom - r.top != BAR_H)
        SetWindowPos(g_main, NULL, 0, 0, w, BAR_H, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    layout_bar(g_main);
    RedrawWindow(g_main, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

static void hover_check(void)
{
    RECT r; POINT p; BOOL want;
    if (!g_main || !IsWindowVisible(g_main)) return;
    GetCursorPos(&p);
    GetWindowRect(g_main, &r);
    want = PtInRect(&r, p) || g_menuOpen || g_picking;
    if (want != g_barExpanded) { g_barExpanded = want; bar_apply(); }
}

void frame_hole(HWND h)
{
    RECT c; HRGN all, hole;
    GetClientRect(h, &c);
    all  = CreateRectRgn(0, 0, c.right, c.bottom);
    hole = CreateRectRgn(BORDER, FRAME_TOP, c.right - BORDER, c.bottom - BORDER);
    CombineRgn(all, all, hole, RGN_DIFF);
    DeleteObject(hole);
    SetWindowRgn(h, all, TRUE);
}

void ui_apply(void)
{
    BOOL bar = !g_uiHidden;
    BOOL frm = bar && g_frameOn && !g_full;
    if (!g_main) return;
    ShowWindow(g_main, bar ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (g_frame) ShowWindow(g_frame, frm ? SW_SHOWNOACTIVATE : SW_HIDE);
    update_title();
}

void toggle_frame(void)
{
    if (g_full) return;
    g_frameOn = !g_frameOn;
    ui_apply();
    if (g_main) InvalidateRect(GetDlgItem(g_main, ID_BARE), NULL, FALSE);
    geo_save();
}

void toggle_full(void)
{
    if (g_rec || g_picking) return;
    if (!g_full) {
        RECT hole; MONITORINFO mi;
        mi.cbSize = sizeof mi;
        get_region(&hole);
        if (!GetMonitorInfoW(MonitorFromRect(&hole, MONITOR_DEFAULTTONEAREST), &mi)) return;
        g_fullRect = mi.rcMonitor;
        g_full = TRUE;
    } else {
        g_full = FALSE;
    }
    ui_apply();
}

void toggle_visible(void)
{
    g_uiHidden = !g_uiHidden;
    ui_apply();
    if (!g_uiHidden) SetForegroundWindow(g_main);
}

void apply_opacity(void)
{
    int t = g_cfg.transp < 0 ? 0 : g_cfg.transp > 80 ? 80 : g_cfg.transp;
    BYTE a = (BYTE)(255 * (100 - t) / 100);
    SetLayeredWindowAttributes(g_main, 0, a, LWA_ALPHA);
    if (g_frame) SetLayeredWindowAttributes(g_frame, 0, a, LWA_ALPHA);
}

void apply_exclude_capture(void)
{
    DWORD aff = WDA_EXCLUDEFROMCAPTURE;
    if (g_main)   SetWindowDisplayAffinity(g_main, aff);
    if (g_frame)  SetWindowDisplayAffinity(g_frame, aff);
    if (g_cfgwnd) SetWindowDisplayAffinity(g_cfgwnd, aff);
    if (g_hl)     SetWindowDisplayAffinity(g_hl, aff);
    if (g_mag)    SetWindowDisplayAffinity(g_mag, WDA_EXCLUDEFROMCAPTURE);
    if (g_szwnd) {
        SetWindowDisplayAffinity(g_szwnd, aff);
        RedrawWindow(g_szwnd, NULL, NULL,
                     RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
    }
    DwmFlush();
}

static BOOL CALLBACK set_child_font(HWND c, LPARAM l)
{
    SendMessageW(c, WM_SETFONT, (WPARAM)l, FALSE);
    return TRUE;
}

void dpi_refresh(HWND src, int newDpi, const RECT *suggested)
{
    NONCLIENTMETRICSW ncm;
    typedef BOOL (WINAPI *SPI_DPI)(UINT, UINT, PVOID, UINT, UINT);
    SPI_DPI pSpiDpi;
    HFONT oldFont = g_font;
    int cw = 0, ch = 0;
    if (newDpi <= 0 || newDpi == g_dpi) return;
    if (src == g_frame && g_frame) cur_size(&cw, &ch);
    g_dpi = newDpi;
    ZeroMemory(&ncm, sizeof ncm);
    ncm.cbSize = sizeof ncm;
    pSpiDpi = (SPI_DPI)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SystemParametersInfoForDpi");
    if (!(pSpiDpi && pSpiDpi(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0, (UINT)g_dpi)))
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    if (g_main)   EnumChildWindows(g_main, set_child_font, (LPARAM)g_font);
    if (g_cfgwnd) EnumChildWindows(g_cfgwnd, set_child_font, (LPARAM)g_font);
    if (g_szwnd)  EnumChildWindows(g_szwnd, set_child_font, (LPARAM)g_font);
    if (oldFont) DeleteObject(oldFont);
    icons_init();
    if (suggested && src) {
        if (src == g_frame && cw > 0 && ch > 0)
            SetWindowPos(src, NULL, suggested->left, suggested->top,
                         cw + 2 * BORDER, ch + FRAME_TOP + BORDER,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        else
            SetWindowPos(src, NULL, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
    }
    bar_apply();
    if (g_frame) frame_hole(g_frame);
    update_title();
}

static void nudge_frame(int dx, int dy, BOOL resize)
{
    RECT r; int w, h;
    if (g_rec || g_full || !g_frame) return;
    GetWindowRect(g_frame, &r);
    if (resize) {
        w = (r.right - r.left) + dx;
        h = (r.bottom - r.top) + dy;
        if (w < 2 * BORDER + 64) w = 2 * BORDER + 64;
        if (h < FRAME_TOP + BORDER + 36) h = FRAME_TOP + BORDER + 36;
        SetWindowPos(g_frame, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    } else {
        SetWindowPos(g_frame, NULL, r.left + dx, r.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    geo_save();
    update_title();
}

/* ------------------------------------------------------------------ */
/* Éditeur de préréglages                                              */
/* ------------------------------------------------------------------ */
static void sz_fill_list(HWND h, int sel)
{
    HWND lb = GetDlgItem(h, ID_SZ_LIST); WCHAR t[128];
    SendMessageW(lb, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < g_sizeN; i++) {
        SIZEP *p = &g_sizes[i];
        if (p->hasPos) swprintf_s(t, ARRAYSIZE(t), L"%s — %d × %d @ %d, %d", p->name, p->w, p->h, p->x, p->y);
        else           swprintf_s(t, ARRAYSIZE(t), L"%s — %d × %d", p->name, p->w, p->h);
        SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)t);
    }
    if (sel >= g_sizeN) sel = g_sizeN - 1;
    if (sel >= 0) SendMessageW(lb, LB_SETCURSEL, (WPARAM)sel, 0);
}

static void sz_pos_enable(HWND h)
{
    BOOL on = IsDlgButtonChecked(h, ID_SZ_USEPOS) == BST_CHECKED;
    EnableWindow(GetDlgItem(h, ID_SZ_X), on);
    EnableWindow(GetDlgItem(h, ID_SZ_Y), on);
}

static void sz_to_edits(HWND h, int i)
{
    SIZEP *p;
    if (i < 0 || i >= g_sizeN) return;
    p = &g_sizes[i];
    SetDlgItemTextW(h, ID_SZ_NAME, p->name);
    SetDlgItemInt(h, ID_SZ_W, (UINT)p->w, FALSE);
    SetDlgItemInt(h, ID_SZ_H, (UINT)p->h, FALSE);
    if (p->hasPos) {
        SetDlgItemInt(h, ID_SZ_X, (UINT)p->x, TRUE);
        SetDlgItemInt(h, ID_SZ_Y, (UINT)p->y, TRUE);
    } else {
        SetDlgItemTextW(h, ID_SZ_X, L"");
        SetDlgItemTextW(h, ID_SZ_Y, L"");
    }
    CheckDlgButton(h, ID_SZ_USEPOS, p->hasPos ? BST_CHECKED : BST_UNCHECKED);
    sz_pos_enable(h);
}

static void sz_current_to_edits(HWND h)
{
    int cw, ch, cx, cy;
    cur_size(&cw, &ch);
    cur_pos(&cx, &cy);
    SetDlgItemInt(h, ID_SZ_W, (UINT)cw, FALSE);
    SetDlgItemInt(h, ID_SZ_H, (UINT)ch, FALSE);
    SetDlgItemInt(h, ID_SZ_X, (UINT)cx, TRUE);
    SetDlgItemInt(h, ID_SZ_Y, (UINT)cy, TRUE);
    CheckDlgButton(h, ID_SZ_USEPOS, BST_CHECKED);
    sz_pos_enable(h);
}

static BOOL sz_from_edits(HWND h, SIZEP *p)
{
    BOOL ok1, ok2, ok3 = TRUE, ok4 = TRUE;
    BOOL usepos = IsDlgButtonChecked(h, ID_SZ_USEPOS) == BST_CHECKED;
    int w = (int)GetDlgItemInt(h, ID_SZ_W, &ok1, FALSE), hh = (int)GetDlgItemInt(h, ID_SZ_H, &ok2, FALSE);
    int x = 0, y = 0;
    if (usepos) {
        x = (int)GetDlgItemInt(h, ID_SZ_X, &ok3, TRUE);
        y = (int)GetDlgItemInt(h, ID_SZ_Y, &ok4, TRUE);
    }
    if (!ok1 || !ok2 || !ok3 || !ok4 || w < 64 || hh < 36 || w > 16000 || hh > 16000) {
        MessageBoxW(h, T(S_SZ_INVALID), T(S_PRESETS), MB_ICONWARNING);
        return FALSE;
    }
    GetDlgItemTextW(h, ID_SZ_NAME, p->name, ARRAYSIZE(p->name));
    for (WCHAR *c = p->name; *c; c++) if (*c == L'|') *c = L'/';
    if (!p->name[0]) swprintf_s(p->name, ARRAYSIZE(p->name), L"%d × %d", w, hh);
    p->w = w; p->h = hh; p->hasPos = usepos; p->x = x; p->y = y;
    return TRUE;
}

LRESULT CALLBACK SzProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        mk(h, L"LISTBOX", NULL, WS_TABSTOP | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
           12, 12, 210, 326, ID_SZ_LIST);
        mk(h, L"STATIC", T(S_SZ_NAME), SS_LEFT, 232, 12, 196, 18, 0);
        mk(h, L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 232, 30, 196, 22, ID_SZ_NAME);
        SendDlgItemMessageW(h, ID_SZ_NAME, EM_LIMITTEXT, 31, 0);
        mk(h, L"STATIC", T(S_SZ_W), SS_LEFT, 232, 60, 90, 18, 0);
        mk(h, L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 232, 78, 90, 22, ID_SZ_W);
        mk(h, L"STATIC", T(S_SZ_H), SS_LEFT, 338, 60, 90, 18, 0);
        mk(h, L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 338, 78, 90, 22, ID_SZ_H);
        mk(h, L"STATIC", T(S_SZ_X), SS_LEFT, 232, 108, 90, 18, 0);
        mk(h, L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 232, 126, 90, 22, ID_SZ_X);
        mk(h, L"STATIC", T(S_SZ_Y), SS_LEFT, 338, 108, 90, 18, 0);
        mk(h, L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 338, 126, 90, 22, ID_SZ_Y);
        mk(h, L"BUTTON", T(S_SZ_USEPOS), BS_AUTOCHECKBOX | WS_TABSTOP, 232, 154, 196, 20, ID_SZ_USEPOS);
        mk(h, L"BUTTON", T(S_SZ_CUR), BS_PUSHBUTTON | WS_TABSTOP, 232, 180, 196, 26, ID_SZ_CUR);
        mk(h, L"BUTTON", T(S_SZ_ADD),    BS_PUSHBUTTON | WS_TABSTOP, 232, 214, 96, 26, ID_SZ_ADD);
        mk(h, L"BUTTON", T(S_SZ_SET),   BS_PUSHBUTTON | WS_TABSTOP, 332, 214, 96, 26, ID_SZ_SET);
        mk(h, L"BUTTON", T(S_SZ_DEL),  BS_PUSHBUTTON | WS_TABSTOP, 232, 246, 96, 26, ID_SZ_DEL);
        mk(h, L"BUTTON", T(S_SZ_APPLY),  BS_PUSHBUTTON | WS_TABSTOP, 332, 246, 96, 26, ID_SZ_APPLY);
        mk(h, L"BUTTON", T(S_SZ_UP),     BS_PUSHBUTTON | WS_TABSTOP, 232, 278, 96, 26, ID_SZ_UP);
        mk(h, L"BUTTON", T(S_SZ_DOWN),  BS_PUSHBUTTON | WS_TABSTOP, 332, 278, 96, 26, ID_SZ_DOWN);
        mk(h, L"BUTTON", T(S_SZ_RESET), BS_PUSHBUTTON | WS_TABSTOP, 232, 314, 196, 26, ID_SZ_RESET);
        sz_fill_list(h, 0);
        sz_to_edits(h, 0);
        dark_apply(h);
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        int sel = (int)SendDlgItemMessageW(h, ID_SZ_LIST, LB_GETCURSEL, 0, 0);
        SIZEP p;
        switch (id) {
        case ID_SZ_LIST:
            if (code == LBN_SELCHANGE) sz_to_edits(h, sel);
            break;
        case ID_SZ_USEPOS:
            if (code == BN_CLICKED) sz_pos_enable(h);
            break;
        case ID_SZ_CUR:
            sz_current_to_edits(h);
            break;
        case ID_SZ_ADD:
            if (g_sizeN >= MAXS) { MessageBoxW(h, T(S_SZ_MAX), T(S_PRESETS), MB_ICONWARNING); break; }
            if (sz_from_edits(h, &p)) {
                g_sizes[g_sizeN++] = p;
                sizes_save();
                sz_fill_list(h, g_sizeN - 1);
                sz_to_edits(h, g_sizeN - 1);
            }
            break;
        case ID_SZ_SET:
            if (sel >= 0 && sz_from_edits(h, &p)) { g_sizes[sel] = p; sizes_save(); sz_fill_list(h, sel); }
            break;
        case ID_SZ_DEL:
            if (sel >= 0) {
                memmove(&g_sizes[sel], &g_sizes[sel + 1], (size_t)(g_sizeN - sel - 1) * sizeof(SIZEP));
                g_sizeN--;
                sizes_save();
                sz_fill_list(h, sel);
                sz_to_edits(h, sel < g_sizeN ? sel : g_sizeN - 1);
            }
            break;
        case ID_SZ_UP: case ID_SZ_DOWN: {
            int to = sel + (id == ID_SZ_UP ? -1 : 1);
            if (sel >= 0 && to >= 0 && to < g_sizeN) {
                SIZEP t = g_sizes[sel]; g_sizes[sel] = g_sizes[to]; g_sizes[to] = t;
                sizes_save();
                sz_fill_list(h, to);
            }
            break;
        }
        case ID_SZ_APPLY:
            if (sel >= 0) apply_size(sel);
            break;
        case ID_SZ_RESET:
            if (MessageBoxW(h, T(S_SZ_RESET_Q), T(S_PRESETS), MB_YESNO | MB_ICONQUESTION) == IDYES) {
                sizes_default(); sizes_save(); sz_fill_list(h, 0); sz_to_edits(h, 0);
            }
            break;
        }
        return 0;
    }
    case WM_ERASEBKGND:
        if (dark_erase(h, (HDC)w)) return 1;
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        if (g_dark) return dark_ctl((HWND)l, (HDC)w, m);
        break;
    case WM_CLOSE:
        ShowWindow(h, SW_HIDE);
        return 0;
    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProcW(h, m, w, l);
        return hit == HTCLIENT ? HTCAPTION : hit;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

void sizes_editor(BOOL addCurrent)
{
    RECT r, wa;
    sync_theme();
    if (!IsWindowVisible(g_szwnd)) {
        GetWindowRect(g_szwnd, &r);
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        SetWindowPos(g_szwnd, HWND_TOPMOST,
                     wa.left + ((wa.right - wa.left) - (r.right - r.left)) / 2,
                     wa.top + ((wa.bottom - wa.top) - (r.bottom - r.top)) / 2, 0, 0, SWP_NOSIZE);
    }
    if (addCurrent) {
        SetDlgItemTextW(g_szwnd, ID_SZ_NAME, L"");
        sz_current_to_edits(g_szwnd);
        SendDlgItemMessageW(g_szwnd, ID_SZ_LIST, LB_SETCURSEL, (WPARAM)-1, 0);
    }
    ShowWindow(g_szwnd, SW_SHOW);
    SetForegroundWindow(g_szwnd);
    if (addCurrent) SetFocus(GetDlgItem(g_szwnd, ID_SZ_NAME));
}

/* ------------------------------------------------------------------ */
/* Aimantation et procédures des fenêtres principales                  */
/* ------------------------------------------------------------------ */
static int snap_axis(int lo, int hi, int m0, int m1, int w0, int w1, int th, BOOL ok0, BOOL ok1)
{
    int cand[4] = { m0, w0, m1, w1 }, best = th + 1, bd = 0;
    BOOL use[4] = { ok0, ok0 || w0 != m0, ok1, ok1 || w1 != m1 };
    for (int i = 0; i < 4; i++) {
        int d;
        if (!use[i]) continue;
        d = cand[i] - (i < 2 ? lo : hi);
        if (abs(d) < best) { best = abs(d); bd = d; }
    }
    return best <= th ? bd : 0;
}

static BOOL snap_rect(RECT *r)
{
    MONITORINFO mi; int th = S(6), dx, dy, mx, my;
    POINT p; BOOL okL, okR, okT, okB;
    if (GetKeyState(VK_CONTROL) & 0x8000) return FALSE;
    mi.cbSize = sizeof mi;
    if (!GetMonitorInfoW(MonitorFromRect(r, MONITOR_DEFAULTTONEAREST), &mi)) return FALSE;
    mx = (r->left + r->right) / 2; my = (r->top + r->bottom) / 2;
    p.x = mi.rcMonitor.left - 1;  p.y = my; okL = MonitorFromPoint(p, MONITOR_DEFAULTTONULL) == NULL;
    p.x = mi.rcMonitor.right;     p.y = my; okR = MonitorFromPoint(p, MONITOR_DEFAULTTONULL) == NULL;
    p.x = mx; p.y = mi.rcMonitor.top - 1;   okT = MonitorFromPoint(p, MONITOR_DEFAULTTONULL) == NULL;
    p.x = mx; p.y = mi.rcMonitor.bottom;    okB = MonitorFromPoint(p, MONITOR_DEFAULTTONULL) == NULL;
    dx = snap_axis(r->left, r->right, mi.rcMonitor.left, mi.rcMonitor.right, mi.rcWork.left, mi.rcWork.right, th, okL, okR);
    dy = snap_axis(r->top, r->bottom, mi.rcMonitor.top, mi.rcMonitor.bottom, mi.rcWork.top, mi.rcWork.bottom, th, okT, okB);
    OffsetRect(r, dx, dy);
    return TRUE;
}

void create_aux_windows(void)
{
    RECT sr = { 0, 0, 440, 350 };
    DWORD sst = WS_POPUP | WS_CAPTION | WS_SYSMENU, sex = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
    g_dark = sys_dark();
    allow_dark_app(g_dark);
    cfg_scale_update();
    g_cfgwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"WinCapCfg", T(S_SETTINGS_T),
                               WS_POPUP | WS_BORDER, 0, 0, cfg_scale(880), cfg_scale(514), g_main, NULL, g_inst, NULL);
    AdjustWindowRectEx(&sr, sst, FALSE, sex);
    g_szwnd = CreateWindowExW(sex, L"WinCapSizes", T(S_SZ_TITLE), sst, 0, 0,
                              sr.right - sr.left, sr.bottom - sr.top, g_main, NULL, g_inst, NULL);
    apply_exclude_capture();
}

void cfg_toggle(void)
{
    RECT w, wa; POINT c; int ww, hh, x, y;
    if (IsWindowVisible(g_cfgwnd)) { ShowWindow(g_cfgwnd, SW_HIDE); return; }
    if (GetTickCount() - g_cfgHid < 300) return;
    if (g_szwnd) ShowWindow(g_szwnd, SW_HIDE);
    sync_theme();
    GetWindowRect(g_cfgwnd, &w);
    ww = w.right - w.left; hh = w.bottom - w.top;
    GetCursorPos(&c);
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    x = c.x - ww / 2; y = c.y - hh - 12;
    if (x + ww > wa.right) x = wa.right - ww;
    if (x < wa.left) x = wa.left;
    if (y < wa.top) y = c.y + 12;
    if (y + hh > wa.bottom) y = wa.bottom - hh;
    SetWindowPos(g_cfgwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(g_cfgwnd);
}

LRESULT CALLBACK FrameProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        g_frame = h;
        return 0;
    case WM_NCCALCSIZE:
        return 0;
    case WM_NCPAINT:
        return 0;
    case WM_NCACTIVATE:
        return DefWindowProcW(h, m, w, -1);
    case WM_NCLBUTTONDBLCLK:
        return 0;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_NCHITTEST: {
        RECT r; int x, y, W, H, G = S(12);
        GetWindowRect(h, &r);
        x = GET_X_LPARAM(l) - r.left; y = GET_Y_LPARAM(l) - r.top;
        W = r.right - r.left; H = r.bottom - r.top;
        if (x < G && y < G) return HTTOPLEFT;
        if (x >= W - G && y < G) return HTTOPRIGHT;
        if (x < G && y >= H - G) return HTBOTTOMLEFT;
        if (x >= W - G && y >= H - G) return HTBOTTOMRIGHT;
        if (y < FRAME_TOP) return HTCAPTION;
        if (y >= H - BORDER) return HTBOTTOM;
        if (x < BORDER) return HTLEFT;
        if (x >= W - BORDER) return HTRIGHT;
        return HTCLIENT;
    }
    case WM_ERASEBKGND: {
        RECT c; HBRUSH b = CreateSolidBrush(frame_color());
        GetClientRect(h, &c);
        FillRect((HDC)w, &c, b);
        DeleteObject(b);
        return 1;
    }
    case WM_MOVING:
        if (snap_rect((RECT *)l)) return TRUE;
        break;
    case WM_SIZING:
        if (g_rec) { GetWindowRect(h, (RECT *)l); return TRUE; }
        break;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)l)->ptMinTrackSize.x = 2 * BORDER + S(128);
        ((MINMAXINFO *)l)->ptMinTrackSize.y = FRAME_TOP + BORDER + S(72);
        return 0;
    case WM_SIZE:
        frame_hole(h);
        update_title();
        return 0;
    case WM_MOVE:
        update_title();
        return 0;
    case WM_DPICHANGED:
        dpi_refresh(h, HIWORD(w), (const RECT *)l);
        return 0;
    case WM_EXITSIZEMOVE:
        geo_save();
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == g_wmTaskbar && m) { tray_add(); return 0; }
    if (m == g_wmShowMe && m) {
        g_uiHidden = FALSE; ui_apply(); SetForegroundWindow(h);
        return 0;
    }
    switch (m) {
    case WM_CREATE:
        g_main = h;
        mk(h, L"BUTTON", L"Image",   BS_OWNERDRAW, 0, 0, 10, 10, ID_SHOT);
        mk(h, L"BUTTON", L"● Rec",   BS_OWNERDRAW, 0, 0, 10, 10, ID_REC);
        mk(h, L"BUTTON", L"Taille",  BS_OWNERDRAW, 0, 0, 10, 10, ID_SIZE);
        mk(h, L"BUTTON", L"Cibler",  BS_OWNERDRAW, 0, 0, 10, 10, ID_PICK);
        mk(h, L"BUTTON", L"Config",  BS_OWNERDRAW, 0, 0, 10, 10, ID_CFG);
        mk(h, L"BUTTON", L"×",       BS_OWNERDRAW, 0, 0, 10, 10, ID_CLOSE);
        mk(h, L"BUTTON", L"-",       BS_OWNERDRAW, 0, 0, 10, 10, ID_BARE);
        mk(h, L"BUTTON", L"II",      BS_OWNERDRAW, 0, 0, 10, 10, ID_PAUSE);
        SetTimer(h, TIMER_HOVER, 120, NULL);
        {
            static const int tids[8] = { ID_SHOT, ID_REC, ID_PAUSE, ID_SIZE, ID_PICK, ID_CFG, ID_BARE, ID_CLOSE };
            HWND tt = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL,
                                      WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                                      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                      h, NULL, g_inst, NULL);
            for (int i = 0; i < 8; i++) {
                TOOLINFOW ti;
                ZeroMemory(&ti, sizeof ti);
                ti.cbSize = sizeof ti;
                ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
                ti.hwnd = h;
                ti.uId = (UINT_PTR)GetDlgItem(h, tids[i]);
                ti.lpszText = LPSTR_TEXTCALLBACKW;
                SendMessageW(tt, TTM_ADDTOOLW, 0, (LPARAM)&ti);
            }
        }
        tray_add();
        hotkeys_register();
        return 0;

    case WM_NCCALCSIZE:
        return 0;
    case WM_NOTIFY: {
        NMHDR *nh = (NMHDR *)l;
        if (nh->code == TTN_GETDISPINFOW) {
            NMTTDISPINFOW *di = (NMTTDISPINFOW *)l;
            const WCHAR *base = L""; int hkv = 0; WCHAR hk[48];
            switch (GetDlgCtrlID((HWND)nh->idFrom)) {
            case ID_SHOT:  base = T(S_TIP_SHOT); hkv = g_cfg.hkShot; break;
            case ID_REC:   base = g_rec ? T(S_TIP_STOP) : T(S_TIP_REC); hkv = g_cfg.hkRec; break;
            case ID_SIZE:  base = T(S_TIP_SIZE); break;
            case ID_PICK:  base = T(S_TIP_PICK); break;
            case ID_CFG:   base = T(S_TIP_CFG); break;
            case ID_PAUSE: base = rc.paused ? T(S_TIP_RESUME) : T(S_TIP_PAUSE); hkv = g_cfg.hkPause; break;
            case ID_BARE:  base = T(S_TIP_FRAME); break;
            case ID_CLOSE: base = T(S_TIP_HIDE); break;
            }
            hk_text(hkv, hk, ARRAYSIZE(hk));
            if (hk[0]) swprintf_s(di->szText, ARRAYSIZE(di->szText), L"%s  (%s)", base, hk);
            else       wcsncpy_s(di->szText, ARRAYSIZE(di->szText), base, _TRUNCATE);
            di->lpszText = di->szText;
            return 0;
        }
        break;
    }
    case WM_NCPAINT:
        return 0;
    case WM_NCACTIVATE:
        return DefWindowProcW(h, m, w, -1);
    case WM_NCLBUTTONDBLCLK:
        if (w == HTCAPTION) toggle_frame();
        return 0;
    case WM_NCHITTEST:
        return HTCAPTION;
    case WM_ERASEBKGND: {
        RECT c; HBRUSH b = CreateSolidBrush(title_color());
        GetClientRect(h, &c);
        FillRect((HDC)w, &c, b);
        DeleteObject(b);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT c; HBRUSH grip;
        GetClientRect(h, &c);
        if (g_textRc.right - g_textRc.left > S(20)) {
            RECT tr = g_textRc;
            SelectObject(dc, g_font);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, text_for(title_color()));
            DrawTextW(dc, g_status, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        grip = CreateSolidBrush(text_for(title_color()));
        if (grip) {
            int d = S(2), gap = S(2), cy = BAR_H / 2;
            for (int col = 0; col < 2; col++) {
                int lx = S(3) + col * (d + gap);
                int rx = c.right - S(3) - d - col * (d + gap);
                for (int row = -1; row <= 1; row++) {
                    RECT dot = { lx, cy + row * S(4), lx + d, cy + row * S(4) + d };
                    FillRect(dc, &dot, grip);
                    dot.left = rx; dot.right = rx + d;
                    FillRect(dc, &dot, grip);
                }
            }
            DeleteObject(grip);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *d = (DRAWITEMSTRUCT *)l;
        HBRUSH b;
        COLORREF tc = title_color(), bg;
        BOOL rec = (d->CtlID == ID_REC && g_rec), dark = lum(tc) < 140;
        bg = rec ? RGB(255, 255, 255) : shade(tc, dark ? 32 : -32);
        if (d->itemState & ODS_SELECTED) bg = rec ? RGB(210, 210, 210) : shade(tc, dark ? 70 : -70);
        b = CreateSolidBrush(bg);
        FillRect(d->hDC, &d->rcItem, b);
        DeleteObject(b);
        draw_icon(d->hDC, &d->rcItem, (int)d->CtlID, text_for(bg), bg, rec);
        return TRUE;
    }
    case WM_KEYDOWN: {
        int step = (GetKeyState(VK_SHIFT) & 0x8000) ? 10 : 1;
        BOOL resize = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        switch (w) {
        case VK_LEFT:  nudge_frame(-step, 0, resize); return 0;
        case VK_RIGHT: nudge_frame(step, 0, resize);  return 0;
        case VK_UP:    nudge_frame(0, -step, resize); return 0;
        case VK_DOWN:  nudge_frame(0, step, resize);  return 0;
        }
        break;
    }
    case WM_MOVING:
        if (snap_rect((RECT *)l)) return TRUE;
        break;
    case WM_SIZE:
        layout_bar(h);
        RedrawWindow(h, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        return 0;
    case WM_DPICHANGED:
        dpi_refresh(h, HIWORD(w), (const RECT *)l);
        return 0;
    case WM_SETCURSOR:
        if (g_picking) { SetCursor(LoadCursorW(NULL, IDC_CROSS)); return TRUE; }
        break;
    case WM_SETTINGCHANGE:
        if (l && !_wcsicmp((const WCHAR *)l, L"ImmersiveColorSet")) sync_theme();
        return 0;
    case WM_RELANG: {
        RECT pr; BOOL vis = IsWindowVisible(g_cfgwnd);
        GetWindowRect(g_cfgwnd, &pr);
        DestroyWindow(g_cfgwnd);
        DestroyWindow(g_szwnd);
        create_aux_windows();
        hotkeys_register();
        ui_update();
        update_title();
        if (vis) {
            SetWindowPos(g_cfgwnd, HWND_TOPMOST, pr.left, pr.top, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
            SetForegroundWindow(g_cfgwnd);
        }
        return 0;
    }
    case WM_PICKMSG:
        if (!g_picking) return 0;
        switch (w) {
        case 1: pick_mouse_down(); break;
        case 2: pick_mouse_up(); break;
        case 3: pick_wheel((int)l); break;
        case 4: pick_end(); break;
        }
        return 0;
    case WM_TIMER:
        if (w == TIMER_PICK)      { if (g_picking) pick_poll(); return 0; }
        if (w == TIMER_HOVER)     { hover_check(); return 0; }
        if (w == TIMER_COUNTDOWN) { shot_countdown_tick(); return 0; }
        update_title();
        return 0;
    case WM_HOTKEY:
        hotkey_handle(w);
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(w);
        if (id >= IDM_SIZE0 && id < IDM_SIZE0 + g_sizeN) { apply_size(id - IDM_SIZE0); return 0; }
        switch (id) {
        case ID_SHOT: case IDM_SHOT: shot_trigger(); break;
        case ID_REC:  case IDM_REC:  toggle_rec(); break;
        case ID_CFG:  case IDM_CFG:  cfg_toggle(); break;
        case ID_PICK: case IDM_PICK: pick_start(); break;
        case ID_CLOSE: toggle_visible(); break;
        case ID_PAUSE: rec_pause_toggle(); break;
        case ID_SIZE: {
            RECT br; HMENU sm = size_menu();
            GetWindowRect(GetDlgItem(h, ID_SIZE), &br);
            g_menuOpen = TRUE;
            TrackPopupMenu(sm, TPM_BOTTOMALIGN, br.left, br.top, 0, h, NULL);
            g_menuOpen = FALSE;
            DestroyMenu(sm);
            break;
        }
        case IDM_SIZE_ADD:  sizes_editor(TRUE); break;
        case IDM_SIZE_EDIT: sizes_editor(FALSE); break;
        case ID_BARE: case IDM_BARE: toggle_frame(); break;
        case IDM_SHOWHIDE: toggle_visible(); break;
        case IDM_FULL: toggle_full(); break;
        case IDM_OPENDIR: open_capture_dir(); break;
        case IDM_OPENLAST: open_last_capture(); break;
        case IDM_COPYLAST: copy_last_shot(); break;
        case IDM_QUIT: DestroyWindow(h); break;
        }
        return 0;
    }
    case WM_TRAY:
        if (l == WM_LBUTTONUP) toggle_visible();
        else if (l == WM_RBUTTONUP) tray_menu();
        else if (l == NIN_BALLOONUSERCLICK) {
            if (g_lastFile[0]) open_last_capture();
            else open_capture_dir();
        }
        return 0;
    case WM_CLOSE:
        g_uiHidden = TRUE;
        ui_apply();
        return 0;
    case WM_EXITSIZEMOVE:
        geo_save();
        return 0;
    case WM_DESTROY:
        geo_save();
        rec_stop();
        pick_end();
        uia_release();
        icons_free();
        hotkeys_unregister();
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* ------------------------------------------------------------------ */
/* Popup de réglages (tray)                                            */
/* ------------------------------------------------------------------ */
typedef struct { int label; int *tgt; int n; const WCHAR *txt[6]; int tid[6]; int val[6]; int x, y; } ROW;
static ROW g_rows[] = {
    { S_ROW_VCODEC, &g_cfg.vcodec, 2, { L"H.264 (AVC)", L"H.265 (HEVC)" }, { 0 }, { 0, 1 }, 18, 24 },
    { S_ROW_VBR,    &g_cfg.vkbps,  5, { L"2 Mb/s", L"4 Mb/s", L"8 Mb/s", L"16 Mb/s", L"32 Mb/s" }, { 0 },
                                      { 2000, 4000, 8000, 16000, 32000 }, 18, 84 },
    { S_ROW_FPS,    &g_cfg.fps,    4, { L"15", L"24", L"30", L"60" }, { 0 }, { 15, 24, 30, 60 }, 18, 114 },
    { S_ROW_ABR,    &g_cfg.akbps,  4, { L"96 kb/s", L"128 kb/s", L"160 kb/s", L"192 kb/s" }, { 0 },
                                      { 96, 128, 160, 192 }, 306, 24 },
    { S_ROW_IMGFMT, &g_cfg.imgfmt, 3, { L"PNG", L"JPEG", L"BMP" }, { 0 }, { 0, 1, 2 }, 606, 24 },
    { S_ROW_JPGQ,   &g_cfg.jpgq,   5, { L"60 %", L"75 %", L"85 %", L"95 %", L"100 %" }, { 0 },
                                      { 60, 75, 85, 95, 100 }, 606, 54 },
    { S_ROW_SOUND,  &g_cfg.sound,  4, { NULL }, { S_SND_DEF, S_SND_SHUT, S_SND_FILE, S_SND_NONE },
                                      { 0, 1, 2, 3 }, 606, 84 },
};

static BOOL pick_sound_file(WCHAR *dst)
{
    OPENFILENAMEW o; WCHAR f[MAX_PATH] = L"", flt[96];
    int fl; BOOL ok;
    ZeroMemory(&o, sizeof o);
    o.lStructSize = sizeof o;
    o.hwndOwner = g_cfgwnd;
    fl = swprintf_s(flt, ARRAYSIZE(flt) - 8, L"%s (*.wav)", T(S_WAV_FILES));
    wcscpy_s(flt + fl + 1, 8, L"*.wav");
    flt[fl + 7] = 0;
    o.lpstrFilter = flt;
    o.lpstrFile = f;
    o.nMaxFile = MAX_PATH;
    o.lpstrTitle = T(S_PICK_SND);
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    g_cfgModal = TRUE;
    ok = GetOpenFileNameW(&o);
    g_cfgModal = FALSE;
    if (ok) wcsncpy_s(dst, MAX_PATH, f, _TRUNCATE);
    return ok;
}

typedef struct { WCHAR path[MAX_PATH]; HWND owner; BOOL ok, failed; } FOLDERDLG;

static DWORD WINAPI folder_thread(LPVOID p)
{
    FOLDERDLG *d = (FOLDERDLG *)p;
    IFileOpenDialog *fd = NULL; IShellItem *it = NULL; PWSTR name = NULL; DWORD opts = 0;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(CoCreateInstance(&MY_CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                   &MY_IID_IFileOpenDialog, (void **)&fd))) {
        IFileOpenDialog_GetOptions(fd, &opts);
        IFileOpenDialog_SetOptions(fd, opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        IFileOpenDialog_SetTitle(fd, T(S_PICK_DIR));
        if (d->path[0]) {
            IShellItem *cur = NULL;
            if (SUCCEEDED(SHCreateItemFromParsingName(d->path, NULL, &MY_IID_IShellItem, (void **)&cur))) {
                IFileOpenDialog_SetFolder(fd, cur);
                REL(cur);
            }
        }
        if (SUCCEEDED(IFileOpenDialog_Show(fd, d->owner)) &&
            SUCCEEDED(IFileOpenDialog_GetResult(fd, &it)) &&
            SUCCEEDED(IShellItem_GetDisplayName(it, SIGDN_FILESYSPATH, &name))) {
            wcsncpy_s(d->path, MAX_PATH, name, _TRUNCATE);
            CoTaskMemFree(name);
            d->ok = TRUE;
        }
        REL(it); REL(fd);
    } else {
        d->failed = TRUE;
    }
    CoUninitialize();
    return 0;
}

static BOOL browse_folder(HWND owner, WCHAR *path)
{
    FOLDERDLG d; HANDLE th; MSG m;
    ZeroMemory(&d, sizeof d);
    d.owner = owner;
    wcsncpy_s(d.path, MAX_PATH, path, _TRUNCATE);
    th = CreateThread(NULL, 0, folder_thread, &d, 0, NULL);
    if (!th) { balloon(T(S_DIR_T), T(S_DIR_ERR)); return FALSE; }
    g_cfgModal = TRUE;
    while (MsgWaitForMultipleObjects(1, &th, FALSE, INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0 + 1)
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
    g_cfgModal = FALSE;
    CloseHandle(th);
    if (d.failed) balloon(T(S_DIR_T), T(S_DIR_ERR));
    if (d.ok) wcsncpy_s(path, MAX_PATH, d.path, _TRUNCATE);
    return d.ok;
}

static void dir_refresh(HWND h)
{
    WCHAR d[MAX_PATH];
    dir_resolve(d);
    SetDlgItemTextW(h, ID_TXT_DIR, d);
}

static void qp_enable(HWND h)
{
    static const int ids[6] = { ID_EDT_QPI, ID_EDT_QPP, ID_EDT_QPB, ID_UD_QPI, ID_UD_QPP, ID_UD_QPB };
    for (int i = 0; i < 6; i++) EnableWindow(GetDlgItem(h, ids[i]), g_cfg.rcmode == 1);
}

static void transp_label(HWND h)
{
    WCHAR t[64];
    swprintf_s(t, ARRAYSIZE(t), T(S_TRANSP), g_cfg.transp);
    SetDlgItemTextW(h, ID_TXT_TRANSP, t);
}

static BOOL choose_color(int *col)
{
    CHOOSECOLORW cc; static COLORREF cust[16]; BOOL ok;
    ZeroMemory(&cc, sizeof cc);
    cc.lStructSize = sizeof cc;
    cc.hwndOwner = g_cfgwnd;
    cc.rgbResult = (COLORREF)*col;
    cc.lpCustColors = cust;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    g_cfgModal = TRUE;
    ok = ChooseColorW(&cc);
    g_cfgModal = FALSE;
    if (ok) *col = (int)cc.rgbResult;
    return ok;
}

LRESULT CALLBACK CfgProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        mk(h, L"BUTTON", T(S_GROUP_VIDEO), BS_GROUPBOX, 8, 8, 570, 350, 0);
        mk(h, L"BUTTON", T(S_GROUP_IMAGE), BS_GROUPBOX, 590, 8, 282, 258, 0);
        mk(h, L"BUTTON", T(S_GROUP_FILENAME), BS_GROUPBOX, 590, 272, 282, 232, 0);
        mk(h, L"BUTTON", T(S_GROUP_GENERAL), BS_GROUPBOX, 8, 360, 570, 144, 0);

        for (int i = 0; i < (int)ARRAYSIZE(g_rows); i++) {
            ROW *r = &g_rows[i];
            HWND cb;
            int sel = 0;
            mk(h, L"STATIC", T(r->label), SS_LEFT, r->x, r->y + 4, 130, 20, 0);
            cb = mk(h, L"COMBOBOX", NULL, WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, r->x + 138, r->y, 130, 200, ID_COMBO0 + i);
            for (int k = 0; k < r->n; k++) {
                SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)(r->tid[k] ? T(r->tid[k]) : r->txt[k]));
                if (r->val[k] == *r->tgt) sel = k;
            }
            SendMessageW(cb, CB_SETCURSEL, sel, 0);
        }
        mk(h, L"STATIC", T(S_ROW_CONTAINER), SS_LEFT, 18, 58, 130, 20, 0);
        {
            HWND cb = mk(h, L"COMBOBOX", NULL, WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 156, 54, 130, 200, ID_CMB_CONTAINER);
            SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)L"MP4");
            SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)L"MPEG-TS");
            SendMessageW(cb, CB_SETCURSEL, g_cfg.container == 1 ? 1 : 0, 0);
        }
        mk(h, L"STATIC", T(S_RC_LBL), SS_LEFT, 18, 148, 130, 20, 0);
        {
            HWND cb = mk(h, L"COMBOBOX", NULL, WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 156, 144, 130, 200, ID_CMB_RC);
            SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)T(S_RC_BR));
            SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)T(S_RC_QP));
            SendMessageW(cb, CB_SETCURSEL, g_cfg.rcmode == 1 ? 1 : 0, 0);
        }
        {
            static const int lbl[3] = { S_QP_I, S_QP_P, S_QP_B };
            static const int eid[3] = { ID_EDT_QPI, ID_EDT_QPP, ID_EDT_QPB };
            static const int uid[3] = { ID_UD_QPI, ID_UD_QPP, ID_UD_QPB };
            const int *val[3] = { &g_cfg.qpI, &g_cfg.qpP, &g_cfg.qpB };
            for (int k = 0; k < 3; k++) {
                int y = 174 + k * 28; HWND ed, ud;
                mk(h, L"STATIC", T(lbl[k]), SS_LEFT, 18, y + 4, 136, 20, 0);
                ed = mk(h, L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 156, y, 90, 22, eid[k]);
                ud = CreateWindowExW(0, UPDOWN_CLASSW, NULL,
                                     WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS,
                                     0, 0, 0, 0, h, (HMENU)(INT_PTR)uid[k], g_inst, NULL);
                SendMessageW(ud, UDM_SETRANGE32, 0, 51);
                SendMessageW(ud, UDM_SETPOS32, 0, *val[k]);
                SendMessageW(ud, UDM_SETBUDDY, (WPARAM)ed, 0);
            }
        }
        mk(h, L"STATIC", T(S_QP_HINT), SS_LEFT, 18, 282, 556, 68, 0);
        qp_enable(h);

        mk(h, L"BUTTON", T(S_CHK_AUDIO), BS_AUTOCHECKBOX | WS_TABSTOP, 306, 54, 268, 22, ID_CHK_AUDIO);
        CheckDlgButton(h, ID_CHK_AUDIO, g_cfg.audio ? BST_CHECKED : BST_UNCHECKED);
        mk(h, L"BUTTON", T(S_CHK_MIC), BS_AUTOCHECKBOX | WS_TABSTOP, 306, 78, 268, 22, ID_CHK_MIC);
        CheckDlgButton(h, ID_CHK_MIC, g_cfg.mic ? BST_CHECKED : BST_UNCHECKED);

        mk(h, L"STATIC", T(S_ROW_RSOUND), SS_LEFT, 306, 110, 130, 20, 0);
        {
            static const int sid[4] = { S_SND_DEF, S_SND_SHUT, S_SND_FILE, S_SND_NONE };
            HWND cs = mk(h, L"COMBOBOX", NULL, WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 444, 106, 130, 200, ID_CMB_RSND);
            for (int k = 0; k < 4; k++) SendMessageW(cs, CB_ADDSTRING, 0, (LPARAM)T(sid[k]));
            SendMessageW(cs, CB_SETCURSEL, (g_cfg.soundRec >= 0 && g_cfg.soundRec < 4) ? g_cfg.soundRec : 0, 0);
        }
        mk(h, L"BUTTON", T(S_CHK_CURSOR), BS_AUTOCHECKBOX | WS_TABSTOP, 306, 136, 268, 22, ID_CHK_CURSOR);
        CheckDlgButton(h, ID_CHK_CURSOR, g_cfg.cursor ? BST_CHECKED : BST_UNCHECKED);
        mk(h, L"BUTTON", T(S_CHK_CLICKS), BS_AUTOCHECKBOX | WS_TABSTOP, 306, 160, 268, 22, ID_CHK_CLICKS);
        CheckDlgButton(h, ID_CHK_CLICKS, g_cfg.clicks ? BST_CHECKED : BST_UNCHECKED);
        mk(h, L"BUTTON", T(S_CHK_VFR), BS_AUTOCHECKBOX | WS_TABSTOP, 306, 184, 268, 22, ID_CHK_VFR);
        CheckDlgButton(h, ID_CHK_VFR, g_cfg.vfr ? BST_CHECKED : BST_UNCHECKED);
        mk(h, L"BUTTON", T(S_CHK_GPU), BS_AUTOCHECKBOX | WS_TABSTOP, 306, 208, 268, 22, ID_CHK_GPU);
        CheckDlgButton(h, ID_CHK_GPU, g_cfg.gpu ? BST_CHECKED : BST_UNCHECKED);
        mk(h, L"STATIC", T(S_GPU_HINT), SS_LEFT, 306, 232, 268, 42, 0);
        mk(h, L"STATIC", T(S_ROW_DELAY), SS_LEFT, 606, 118, 130, 20, 0);
        {
            static const int dids[4] = { S_DLY_0, S_DLY_3, S_DLY_5, S_DLY_10 };
            static const int dvals[4] = { 0, 3, 5, 10 };
            HWND cd = mk(h, L"COMBOBOX", NULL, WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 744, 114, 112, 200, ID_CMB_DELAY);
            int dsel = 0;
            for (int k = 0; k < 4; k++) {
                SendMessageW(cd, CB_ADDSTRING, 0, (LPARAM)T(dids[k]));
                if (dvals[k] == g_cfg.delay) dsel = k;
            }
            SendMessageW(cd, CB_SETCURSEL, dsel, 0);
        }
        mk(h, L"BUTTON", T(S_CHK_CLIP), BS_AUTOCHECKBOX | WS_TABSTOP, 606, 148, 250, 22, ID_CHK_CLIP);
        CheckDlgButton(h, ID_CHK_CLIP, g_cfg.clip ? BST_CHECKED : BST_UNCHECKED);
        mk(h, L"STATIC", T(S_HK_LBL_IMG), SS_LEFT, 606, 178, 130, 20, 0);
        mk(h, HOTKEY_CLASSW, L"", WS_TABSTOP | WS_BORDER, 744, 174, 112, 22, ID_HK_SHOT);
        SendMessageW(GetDlgItem(h, ID_HK_SHOT), HKM_SETRULES, HKCOMB_S, MAKELPARAM(HOTKEYF_CONTROL | HOTKEYF_ALT, 0));
        SendMessageW(GetDlgItem(h, ID_HK_SHOT), HKM_SETHOTKEY, (WPARAM)g_cfg.hkShot, 0);

        mk(h, L"STATIC", T(S_NAME_LBL), SS_LEFT, 602, 292, 260, 18, 0);
        mk(h, L"EDIT", g_nameFmt, WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 602, 312, 260, 22, ID_EDT_NAME);
        SendDlgItemMessageW(h, ID_EDT_NAME, EM_LIMITTEXT, 120, 0);
        mk(h, L"STATIC", T(S_NAME_HINT), SS_LEFT, 602, 340, 260, 78, 0);
        mk(h, L"STATIC", T(S_DIR_T), SS_LEFT, 602, 440, 60, 20, 0);
        mk(h, L"STATIC", L"", SS_LEFT | SS_PATHELLIPSIS, 664, 440, 148, 20, ID_TXT_DIR);
        mk(h, L"BUTTON", L"…", BS_PUSHBUTTON | WS_TABSTOP, 816, 436, 34, 24, ID_BTN_DIR);
        dir_refresh(h);

        mk(h, L"STATIC", T(S_COL_FRAME), SS_LEFT, 18, 380, 130, 20, 0);
        mk(h, L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 156, 376, 130, 24, ID_COL_FRAME);
        mk(h, L"STATIC", T(S_COL_BAR), SS_LEFT, 18, 410, 130, 20, 0);
        mk(h, L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 156, 406, 130, 24, ID_COL_TITLE);
        mk(h, L"STATIC", L"", SS_LEFT, 18, 438, 268, 18, ID_TXT_TRANSP);
        mk(h, TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_HORZ | TBS_AUTOTICKS, 18, 458, 268, 30, ID_TRK_TRANSP);
        SendDlgItemMessageW(h, ID_TRK_TRANSP, TBM_SETRANGE, TRUE, MAKELONG(0, 80));
        SendDlgItemMessageW(h, ID_TRK_TRANSP, TBM_SETTICFREQ, 10, 0);
        SendDlgItemMessageW(h, ID_TRK_TRANSP, TBM_SETPAGESIZE, 0, 10);
        SendDlgItemMessageW(h, ID_TRK_TRANSP, TBM_SETPOS, TRUE, g_cfg.transp);
        transp_label(h);
        mk(h, L"STATIC", T(S_LANGUAGE), SS_LEFT, 306, 374, 130, 20, 0);
        {
            static const WCHAR *const lnames[4] = { L"English", L"Français", L"Español", L"Deutsch" };
            HWND cl = mk(h, L"COMBOBOX", NULL, WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 444, 370, 130, 200, ID_CMB_LANG);
            for (int k = 0; k < 4; k++) SendMessageW(cl, CB_ADDSTRING, 0, (LPARAM)lnames[k]);
            SendMessageW(cl, CB_SETCURSEL, (g_cfg.lang >= 0 && g_cfg.lang < 4) ? g_cfg.lang : 0, 0);
        }
        mk(h, L"STATIC", T(S_HK_LBL_VID), SS_LEFT, 306, 400, 130, 20, 0);
        mk(h, HOTKEY_CLASSW, L"", WS_TABSTOP | WS_BORDER, 444, 396, 130, 22, ID_HK_REC);
        mk(h, L"STATIC", T(S_HK_LBL_PAUSE), SS_LEFT, 306, 428, 130, 20, 0);
        mk(h, HOTKEY_CLASSW, L"", WS_TABSTOP | WS_BORDER, 444, 424, 130, 22, ID_HK_PAUSE);
        mk(h, L"STATIC", T(S_HK_LBL_FULL), SS_LEFT, 306, 456, 130, 20, 0);
        mk(h, HOTKEY_CLASSW, L"", WS_TABSTOP | WS_BORDER, 444, 452, 130, 22, ID_HK_FULL);
        {
            static const int hkIds[3] = { ID_HK_REC, ID_HK_PAUSE, ID_HK_FULL };
            const int hkVals[3] = { g_cfg.hkRec, g_cfg.hkPause, g_cfg.hkFull };
            for (int k = 0; k < 3; k++) {
                HWND hkc = GetDlgItem(h, hkIds[k]);
                SendMessageW(hkc, HKM_SETRULES, HKCOMB_S, MAKELPARAM(HOTKEYF_CONTROL | HOTKEYF_ALT, 0));
                SendMessageW(hkc, HKM_SETHOTKEY, (WPARAM)hkVals[k], 0);
            }
        }
        dark_apply(h);
        return 0;
    case WM_ERASEBKGND:
        if (dark_erase(h, (HDC)w)) return 1;
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        if (g_dark) return dark_ctl((HWND)l, (HDC)w, m);
        break;
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        if (id >= ID_COMBO0 && id < ID_COMBO0 + (int)ARRAYSIZE(g_rows) && code == CBN_SELCHANGE) {
            ROW *r = &g_rows[id - ID_COMBO0];
            int sel = (int)SendMessageW((HWND)l, CB_GETCURSEL, 0, 0);
            int prev = *r->tgt;
            if (sel < 0 || sel >= r->n) return 0;
            if (r->tgt == &g_cfg.sound && r->val[sel] == 2 && !pick_sound_file(g_soundPath)) {
                for (int k = 0; k < r->n; k++)
                    if (r->val[k] == prev) SendMessageW((HWND)l, CB_SETCURSEL, k, 0);
                return 0;
            }
            *r->tgt = r->val[sel];
            cfg_save();
            if (r->tgt == &g_cfg.sound) play_shutter();
        } else if (id == ID_CHK_AUDIO || id == ID_CHK_CURSOR || id == ID_CHK_VFR || id == ID_CHK_GPU ||
                   id == ID_CHK_MIC || id == ID_CHK_CLIP || id == ID_CHK_CLICKS) {
            int on = IsDlgButtonChecked(h, id) == BST_CHECKED;
            if (id == ID_CHK_AUDIO)       g_cfg.audio = on;
            else if (id == ID_CHK_CURSOR) g_cfg.cursor = on;
            else if (id == ID_CHK_VFR)    g_cfg.vfr = on;
            else if (id == ID_CHK_GPU)    g_cfg.gpu = on;
            else if (id == ID_CHK_MIC)    g_cfg.mic = on;
            else if (id == ID_CHK_CLIP)   g_cfg.clip = on;
            else if (id == ID_CHK_CLICKS) g_cfg.clicks = on;
            cfg_save();
        } else if (id == ID_BTN_DIR && code == BN_CLICKED) {
            WCHAR cur[MAX_PATH];
            dir_resolve(cur);
            if (browse_folder(h, cur)) {
                wcsncpy_s(g_outDir, MAX_PATH, cur, _TRUNCATE);
                cfg_save();
                dir_refresh(h);
            }
        } else if ((id == ID_HK_SHOT || id == ID_HK_REC || id == ID_HK_FULL || id == ID_HK_PAUSE) && code == EN_CHANGE) {
            int v = (int)LOWORD(SendMessageW((HWND)l, HKM_GETHOTKEY, 0, 0));
            if (!LOBYTE((WORD)v)) v = 0;
            if (id == ID_HK_SHOT)      g_cfg.hkShot = v;
            else if (id == ID_HK_REC)  g_cfg.hkRec = v;
            else if (id == ID_HK_FULL) g_cfg.hkFull = v;
            else                       g_cfg.hkPause = v;
            cfg_save();
        } else if ((id == ID_COL_FRAME || id == ID_COL_TITLE) && code == BN_CLICKED) {
            int *col = (id == ID_COL_FRAME) ? &g_cfg.colFrame : &g_cfg.colTitle;
            if (choose_color(col)) {
                cfg_save();
                InvalidateRect((HWND)l, NULL, TRUE);
                RedrawWindow(g_main, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
                if (g_frame) RedrawWindow(g_frame, NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
            }
        } else if (id == ID_CMB_RSND && code == CBN_SELCHANGE) {
            int sel = (int)SendMessageW((HWND)l, CB_GETCURSEL, 0, 0), prev = g_cfg.soundRec;
            if (sel >= 0 && sel < 4) {
                if (sel == 2 && !pick_sound_file(g_soundRecPath)) {
                    SendMessageW((HWND)l, CB_SETCURSEL, (WPARAM)prev, 0);
                } else {
                    g_cfg.soundRec = sel;
                    cfg_save();
                    play_rec_sound();
                }
            }
        } else if (id == ID_CMB_DELAY && code == CBN_SELCHANGE) {
            static const int dvals[4] = { 0, 3, 5, 10 };
            int sel = (int)SendMessageW((HWND)l, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < 4) { g_cfg.delay = dvals[sel]; cfg_save(); }
        } else if (id == ID_CMB_RC && code == CBN_SELCHANGE) {
            g_cfg.rcmode = ((int)SendMessageW((HWND)l, CB_GETCURSEL, 0, 0) == 1) ? 1 : 0;
            cfg_save();
            qp_enable(h);
        } else if (id == ID_CMB_CONTAINER && code == CBN_SELCHANGE) {
            int sel = (int)SendMessageW((HWND)l, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel <= 1) {
                g_cfg.container = sel;
                cfg_save();
            }
        } else if ((id == ID_EDT_QPI || id == ID_EDT_QPP || id == ID_EDT_QPB) && code == EN_CHANGE) {
            BOOL okv; int v = (int)GetDlgItemInt(h, id, &okv, FALSE);
            if (okv) {
                if (v > 51) v = 51;
                if (id == ID_EDT_QPI) g_cfg.qpI = v; else if (id == ID_EDT_QPP) g_cfg.qpP = v; else g_cfg.qpB = v;
                cfg_save();
            }
        } else if (id == ID_CMB_LANG && code == CBN_SELCHANGE) {
            int sel = (int)SendMessageW((HWND)l, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < 4 && sel != g_cfg.lang) {
                g_cfg.lang = sel;
                cfg_save();
                PostMessageW(g_main, WM_RELANG, 0, 0);
            }
        } else if (id == ID_EDT_NAME && code == EN_CHANGE) {
            GetDlgItemTextW(h, ID_EDT_NAME, g_nameFmt, ARRAYSIZE(g_nameFmt));
            cfg_save();
        }
        return 0;
    }
    case WM_HSCROLL:
        if ((HWND)l == GetDlgItem(h, ID_TRK_TRANSP)) {
            g_cfg.transp = (int)SendMessageW((HWND)l, TBM_GETPOS, 0, 0);
            transp_label(h);
            apply_opacity();
            if (LOWORD(w) == TB_ENDTRACK) cfg_save();
        }
        return 0;
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *d = (DRAWITEMSTRUCT *)l;
        COLORREF c = (COLORREF)(d->CtlID == ID_COL_FRAME ? g_cfg.colFrame : g_cfg.colTitle);
        HBRUSH b = CreateSolidBrush(c);
        FillRect(d->hDC, &d->rcItem, b);
        DeleteObject(b);
        FrameRect(d->hDC, &d->rcItem, (HBRUSH)GetStockObject(GRAY_BRUSH));
        SelectObject(d->hDC, g_font);
        SetBkMode(d->hDC, TRANSPARENT);
        SetTextColor(d->hDC, text_for(c));
        DrawTextW(d->hDC, T(S_CHOOSE), -1, &d->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return TRUE;
    }
    case WM_DPICHANGED:
        dpi_refresh(h, HIWORD(w), (const RECT *)l);
        PostMessageW(g_main, WM_RELANG, 0, 0);
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(w) == WA_INACTIVE) {
            if (!g_cfgModal) { ShowWindow(h, SW_HIDE); g_cfgHid = GetTickCount(); hotkeys_register(); }
        } else {
            hotkeys_unregister();
        }
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
