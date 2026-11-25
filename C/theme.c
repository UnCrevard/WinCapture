/*
 * theme.c - Thème clair / sombre Windows, sous-classement des champs et couleurs
 */
#include "wincapture.h"

BOOL g_dark = FALSE;

BOOL sys_dark(void)
{
    DWORD v = 1, n = sizeof v;
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &v, &n) != ERROR_SUCCESS)
        return FALSE;
    return v == 0;
}

static HMODULE uxtheme_dll(void)
{
    static HMODULE m;
    if (!m) m = LoadLibraryW(L"uxtheme.dll");
    return m;
}

void allow_dark_app(BOOL on)
{
    typedef int (WINAPI *SPAM)(int);
    static SPAM f;
    static BOOL tried;
    if (!tried) {
        HMODULE m = uxtheme_dll();
        tried = TRUE;
        if (m) f = (SPAM)(void *)GetProcAddress(m, MAKEINTRESOURCEA(135));
    }
    if (f) f(on ? 1 : 3);   /* 1 = AllowDark, 3 = ForceLight */
}

void allow_dark_window(HWND w, BOOL on)
{
    typedef BOOL (WINAPI *ADMW)(HWND, BOOL);
    static ADMW f;
    static BOOL tried;
    if (!tried) {
        HMODULE m = uxtheme_dll();
        tried = TRUE;
        if (m) f = (ADMW)(void *)GetProcAddress(m, MAKEINTRESOURCEA(133));
    }
    if (f) f(w, on);
}

void set_theme(HWND w, const WCHAR *subapp, const WCHAR *idlist)
{
    typedef HRESULT (WINAPI *SWT)(HWND, LPCWSTR, LPCWSTR);
    static SWT f;
    static BOOL tried;
    if (!tried) {
        HMODULE m = uxtheme_dll();
        tried = TRUE;
        if (m) f = (SWT)(void *)GetProcAddress(m, "SetWindowTheme");
    }
    if (f) f(w, subapp, idlist);
}

static LRESULT CALLBACK InputBoxProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    WNDPROC orig = (WNDPROC)GetPropW(h, L"WCDarkOrig");
    WCHAR cls[32];

    if (!orig) return DefWindowProcW(h, m, w, l);
    if (m == WM_NCDESTROY) {
        RemovePropW(h, L"WCDarkOrig");
        SetWindowLongPtrW(h, GWLP_WNDPROC, (LONG_PTR)orig);
        return CallWindowProcW(orig, h, m, w, l);
    }
    if (!g_dark) return CallWindowProcW(orig, h, m, w, l);

    GetClassNameW(h, cls, ARRAYSIZE(cls));

    if (!_wcsicmp(cls, L"Edit") || !_wcsicmp(cls, L"ListBox")) {
        if (m == WM_NCPAINT) {
            LRESULT r = CallWindowProcW(orig, h, m, w, l);
            HDC dc = GetWindowDC(h);
            if (dc) {
                RECT rc;
                HBRUSH br = CreateSolidBrush(GetFocus() == h ? RGB(0, 120, 215) : RGB(88, 88, 88));
                GetWindowRect(h, &rc);
                OffsetRect(&rc, -rc.left, -rc.top);
                FrameRect(dc, &rc, br);
                DeleteObject(br);
                ReleaseDC(h, dc);
            }
            return r;
        }
        if (m == WM_SETFOCUS || m == WM_KILLFOCUS || m == WM_ENABLE) {
            LRESULT r = CallWindowProcW(orig, h, m, w, l);
            RedrawWindow(h, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
            return r;
        }
        return CallWindowProcW(orig, h, m, w, l);
    }

    if (!_wcsicmp(cls, L"msctls_hotkey32")) {
        if (m == WM_ERASEBKGND) return 1;
        if (m == WM_PAINT) {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc, tr;
            int hk = (int)LOWORD(CallWindowProcW(orig, h, HKM_GETHOTKEY, 0, 0));
            WCHAR txt[64];
            HBRUSH bg = CreateSolidBrush(DK_CTL);
            HBRUSH bd = CreateSolidBrush(GetFocus() == h ? RGB(0, 120, 215) : RGB(88, 88, 88));
            GetClientRect(h, &rc);
            FillRect(dc, &rc, bg);
            FrameRect(dc, &rc, bd);
            DeleteObject(bg); DeleteObject(bd);
            hk_text(hk, txt, ARRAYSIZE(txt));
            if (!txt[0]) wcscpy_s(txt, ARRAYSIZE(txt), L"—");
            tr = rc; tr.left += S(6); tr.right -= S(4);
            SelectObject(dc, g_font);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, IsWindowEnabled(h) ? DK_TXT : RGB(145, 145, 145));
            DrawTextW(dc, txt, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            EndPaint(h, &ps);
            return 0;
        }
        if (m == WM_SETFOCUS || m == WM_KILLFOCUS || m == WM_KEYDOWN || m == WM_SYSKEYDOWN ||
            m == WM_KEYUP || m == WM_SYSKEYUP || m == HKM_SETHOTKEY || m == WM_ENABLE) {
            LRESULT r = CallWindowProcW(orig, h, m, w, l);
            InvalidateRect(h, NULL, FALSE);
            return r;
        }
        return CallWindowProcW(orig, h, m, w, l);
    }

    if (!_wcsicmp(cls, L"ComboBox") && (GetWindowLongPtrW(h, GWL_STYLE) & CBS_DROPDOWNLIST) == CBS_DROPDOWNLIST) {
        if (m == WM_ERASEBKGND) return 1;
        if (m == WM_PAINT) {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc, tr;
            int sel = (int)CallWindowProcW(orig, h, CB_GETCURSEL, 0, 0);
            WCHAR txt[128] = L"";
            HBRUSH bg = CreateSolidBrush(DK_CTL);
            HBRUSH bd = CreateSolidBrush(GetFocus() == h ? RGB(0, 120, 215) : RGB(88, 88, 88));
            HPEN pen, oldp;
            POINT tri[3];
            int cx, cy;
            GetClientRect(h, &rc);
            FillRect(dc, &rc, bg);
            FrameRect(dc, &rc, bd);
            DeleteObject(bg); DeleteObject(bd);
            if (sel >= 0) {
                int len = (int)CallWindowProcW(orig, h, CB_GETLBTEXTLEN, (WPARAM)sel, 0);
                if (len > 0 && len < (int)ARRAYSIZE(txt))
                    CallWindowProcW(orig, h, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)txt);
            }
            tr = rc; tr.left += S(7); tr.right -= S(20);
            SelectObject(dc, g_font);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, IsWindowEnabled(h) ? DK_TXT : RGB(145, 145, 145));
            DrawTextW(dc, txt, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            cx = rc.right - S(11); cy = (rc.top + rc.bottom) / 2;
            tri[0].x = cx - S(3); tri[0].y = cy - S(1);
            tri[1].x = cx;        tri[1].y = cy + S(2);
            tri[2].x = cx + S(3); tri[2].y = cy - S(1);
            pen = CreatePen(PS_SOLID, 1, IsWindowEnabled(h) ? DK_TXT : RGB(145, 145, 145));
            oldp = (HPEN)SelectObject(dc, pen);
            Polyline(dc, tri, 3);
            SelectObject(dc, oldp);
            DeleteObject(pen);
            EndPaint(h, &ps);
            return 0;
        }
        if (m == WM_SETFOCUS || m == WM_KILLFOCUS || m == CB_SETCURSEL || m == WM_ENABLE) {
            LRESULT r = CallWindowProcW(orig, h, m, w, l);
            InvalidateRect(h, NULL, FALSE);
            return r;
        }
        return CallWindowProcW(orig, h, m, w, l);
    }

    if (!_wcsicmp(cls, L"msctls_updown32")) {
        if (m == WM_ERASEBKGND) return 1;
        if (m == WM_PAINT) {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc, topR, botR;
            HBRUSH bg = CreateSolidBrush(DK_CTL), bd = CreateSolidBrush(RGB(88, 88, 88));
            HBRUSH fg = CreateSolidBrush(IsWindowEnabled(h) ? DK_TXT : RGB(145, 145, 145));
            HPEN nul = (HPEN)GetStockObject(NULL_PEN), oldp;
            HGDIOBJ oldb;
            int mid, cx, cy;
            POINT p[3];
            GetClientRect(h, &rc);
            mid = (rc.top + rc.bottom) / 2;
            topR = rc; topR.bottom = mid;
            botR = rc; botR.top = mid;
            FillRect(dc, &rc, bg);
            FrameRect(dc, &topR, bd);
            FrameRect(dc, &botR, bd);
            oldp = (HPEN)SelectObject(dc, nul);
            oldb = SelectObject(dc, fg);
            cx = (rc.left + rc.right) / 2;
            cy = (topR.top + topR.bottom) / 2;
            p[0].x = cx;        p[0].y = cy - S(2);
            p[1].x = cx - S(3); p[1].y = cy + S(2);
            p[2].x = cx + S(3); p[2].y = cy + S(2);
            Polygon(dc, p, 3);
            cy = (botR.top + botR.bottom) / 2;
            p[0].x = cx - S(3); p[0].y = cy - S(1);
            p[1].x = cx + S(3); p[1].y = cy - S(1);
            p[2].x = cx;        p[2].y = cy + S(3);
            Polygon(dc, p, 3);
            SelectObject(dc, oldp);
            SelectObject(dc, oldb);
            DeleteObject(bg); DeleteObject(bd); DeleteObject(fg);
            EndPaint(h, &ps);
            return 0;
        }
        return CallWindowProcW(orig, h, m, w, l);
    }

    return CallWindowProcW(orig, h, m, w, l);
}

static BOOL CALLBACK dark_child(HWND c, LPARAM l)
{
    WCHAR cls[32];
    (void)l;
    GetClassNameW(c, cls, ARRAYSIZE(cls));
    if (!_wcsicmp(cls, L"Static")) return TRUE;
    if (!_wcsicmp(cls, L"Edit") || !_wcsicmp(cls, L"ListBox") ||
        !_wcsicmp(cls, L"ComboBox") || !_wcsicmp(cls, L"msctls_hotkey32") ||
        !_wcsicmp(cls, L"msctls_updown32")) {
        if (!GetPropW(c, L"WCDarkOrig")) {
            WNDPROC orig = (WNDPROC)SetWindowLongPtrW(c, GWLP_WNDPROC, (LONG_PTR)InputBoxProc);
            if (orig) SetPropW(c, L"WCDarkOrig", (HANDLE)orig);
        }
    }
    if (!g_dark) {
        set_theme(c, NULL, NULL);
        return TRUE;
    }
    if (!_wcsicmp(cls, L"Button")) {
        LONG_PTR st = GetWindowLongPtrW(c, GWL_STYLE) & BS_TYPEMASK;
        if (st == BS_GROUPBOX || st == BS_AUTOCHECKBOX || st == BS_CHECKBOX || st == BS_3STATE || st == BS_AUTO3STATE ||
            st == BS_RADIOBUTTON || st == BS_AUTORADIOBUTTON) {
            set_theme(c, L"", L"");         /* laisse le parent dessiner les libellés en thème sombre */
        } else {
            set_theme(c, L"DarkMode_Explorer", NULL);
        }
    } else if (!_wcsicmp(cls, L"ComboBox")) {
        set_theme(c, L"DarkMode_CFD", NULL);
    } else {
        set_theme(c, L"DarkMode_Explorer", NULL);
    }
    return TRUE;
}

void dark_apply(HWND h)
{
    BOOL on = g_dark;
    allow_dark_window(h, on);
    if (FAILED(DwmSetWindowAttribute(h, 20, &on, sizeof on)))
        DwmSetWindowAttribute(h, 19, &on, sizeof on);
    EnumChildWindows(h, dark_child, 0);
    RedrawWindow(h, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}

void sync_theme(void)
{
    BOOL d = sys_dark();
    if (d == g_dark) return;
    g_dark = d;
    allow_dark_app(g_dark);
    if (g_cfgwnd) dark_apply(g_cfgwnd);
    if (g_szwnd)  dark_apply(g_szwnd);
}

LRESULT dark_ctl(HWND ctlWnd, HDC dc, UINT msg)
{
    static HBRUSH bg, ctl;
    BOOL boxed = (msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX);
    if (!boxed && msg == WM_CTLCOLORSTATIC && ctlWnd) {
        WCHAR cls[16];
        GetClassNameW(ctlWnd, cls, ARRAYSIZE(cls));
        if (!_wcsicmp(cls, L"Edit")) boxed = TRUE;
    }
    if (!bg)  bg  = CreateSolidBrush(DK_BG);
    if (!ctl) ctl = CreateSolidBrush(DK_CTL);
    SetTextColor(dc, (ctlWnd && !IsWindowEnabled(ctlWnd)) ? RGB(145, 145, 145) : DK_TXT);
    SetBkColor(dc, boxed ? DK_CTL : DK_BG);
    return (LRESULT)(boxed ? ctl : bg);
}

BOOL dark_erase(HWND h, HDC dc)
{
    RECT c; HBRUSH b;
    if (!g_dark) return FALSE;
    GetClientRect(h, &c);
    b = CreateSolidBrush(DK_BG);
    FillRect(dc, &c, b);
    DeleteObject(b);
    return TRUE;
}

int lum(COLORREF c)
{
    return (299 * GetRValue(c) + 587 * GetGValue(c) + 114 * GetBValue(c)) / 1000;
}

int clamp255(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

COLORREF shade(COLORREF c, int d)
{
    return RGB(clamp255(GetRValue(c) + d), clamp255(GetGValue(c) + d), clamp255(GetBValue(c) + d));
}

COLORREF text_for(COLORREF bg)
{
    return lum(bg) > 140 ? RGB(20, 20, 20) : RGB(240, 240, 240);
}

COLORREF frame_color(void)
{
    return g_picking ? RGB(0, 120, 215)
         : g_rec     ? (rc.paused ? RGB(230, 140, 0) : RGB(200, 30, 30))
                     : (COLORREF)g_cfg.colFrame;
}

COLORREF title_color(void)
{
    return g_picking ? RGB(0, 120, 215)
         : g_rec     ? (rc.paused ? RGB(230, 140, 0) : RGB(200, 30, 30))
                     : (COLORREF)g_cfg.colTitle;
}
