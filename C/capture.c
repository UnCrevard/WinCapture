/*
 * capture.c - Capture d'image (WIC + presse-papiers), capture vidéo GDI / DXGI-D3D11,
 *             effets de clic et ciblage de fenêtre / zone avec loupe de précision
 */
#include "wincapture.h"

static const GUID MY_CLSID_WICImagingFactory     = { 0xCACAF262, 0x9370, 0x4615, { 0xA1, 0x3B, 0x9F, 0x55, 0x39, 0xDA, 0x4C, 0x0A } };
static const GUID MY_IID_IWICImagingFactory      = { 0xEC5EC8A9, 0xC395, 0x4314, { 0x9C, 0x77, 0x54, 0xD7, 0xA9, 0x35, 0xFF, 0x70 } };
static const GUID MY_GUID_ContainerFormatBmp     = { 0x0AF1D87E, 0xFCFE, 0x4188, { 0xBD, 0xEB, 0xA7, 0x90, 0x64, 0x71, 0xCB, 0xE3 } };
static const GUID MY_GUID_ContainerFormatPng     = { 0x1B7CFAF4, 0x713F, 0x473C, { 0xBB, 0xCD, 0x61, 0x37, 0x42, 0x5F, 0xAE, 0xAF } };
static const GUID MY_GUID_ContainerFormatJpeg    = { 0x19E4A5AA, 0x5662, 0x4FC5, { 0xA0, 0xC0, 0x17, 0x58, 0x02, 0x8E, 0x10, 0x57 } };
static const GUID MY_GUID_WICPixelFormat24bppBGR = { 0x6FDDC324, 0x4E03, 0x4BFE, { 0xB1, 0x85, 0x3D, 0x77, 0x76, 0x8D, 0xC9, 0x0C } };
static const GUID MY_CLSID_CUIAutomation         = { 0xFF48DBA4, 0x60EF, 0x4201, { 0xAA, 0x87, 0x54, 0x10, 0x3E, 0xEF, 0x59, 0x4E } };
static const GUID MY_IID_IUIAutomation         = { 0x30CBE57D, 0xD9D0, 0x452A, { 0xAB, 0x13, 0x7A, 0xC5, 0xAC, 0x48, 0x25, 0xEE } };
static const GUID MY_MFVideoFormat_HEVC          = { 0x43564548, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };

static const GUID MY_IID_IDXGIFactory1          = { 0x770AAE78, 0xF26F, 0x4DBA, { 0xA8, 0x29, 0x25, 0x3C, 0x83, 0xD1, 0xB3, 0x87 } };
static const GUID MY_IID_IDXGIOutput1           = { 0x00CDDEA8, 0x939B, 0x4B83, { 0xA3, 0x40, 0xA6, 0x85, 0x22, 0x66, 0x66, 0xCC } };
static const GUID MY_IID_ID3D11Texture2D        = { 0x6F15AAF2, 0xD208, 0x4E89, { 0x9A, 0xB4, 0x48, 0x95, 0x35, 0xD3, 0x4F, 0x9C } };
static const GUID MY_IID_ID3D11VideoDevice      = { 0x10EC4D5B, 0x975A, 0x4689, { 0xB9, 0xE4, 0xD0, 0xAA, 0xC3, 0x0F, 0xE3, 0x33 } };
static const GUID MY_IID_ID3D11VideoContext     = { 0x61F21C45, 0x3C0E, 0x4A74, { 0x9C, 0xEA, 0x67, 0x10, 0x0D, 0x9A, 0xD5, 0xE4 } };
static const GUID MY_IID_ID3D10Multithread      = { 0x9B7E4E00, 0x342C, 0x4106, { 0xA1, 0x9F, 0x4F, 0x27, 0x04, 0xF6, 0x89, 0xF0 } };
static const GUID MY_IID_IMFDXGIBuffer          = { 0xE7174CFA, 0x1C9E, 0x48B1, { 0x88, 0x66, 0x62, 0x62, 0x26, 0xBF, 0xC2, 0x58 } };
static const GUID MY_IID_IMF2DBuffer            = { 0x7DC9D5F9, 0x9ED9, 0x44EC, { 0x9B, 0xBF, 0x06, 0x00, 0xBB, 0x58, 0x9F, 0xBB } };
static const GUID MY_IID_IMFVideoSampleAllocatorEx = { 0x545B3A48, 0x3283, 0x4F62, { 0x86, 0x6F, 0xA6, 0x2D, 0x8F, 0x59, 0x8F, 0x9F } };

static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

/* ------------------------------------------------------------------ */
/* Capture GDI & région                                                */
/* ------------------------------------------------------------------ */
BOOL grab_init(GRAB *g, int w, int h)
{
    BITMAPINFO bi;
    HDC s = GetDC(NULL);
    ZeroMemory(&bi, sizeof bi);
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = h;          /* positif = bottom-up (convention MF RGB32 & CF_DIB) */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    g->dc = CreateCompatibleDC(s);
    g->bmp = CreateDIBSection(s, &bi, DIB_RGB_COLORS, &g->bits, NULL, 0);
    ReleaseDC(NULL, s);
    if (!g->bmp) { DeleteDC(g->dc); return FALSE; }
    g->old = SelectObject(g->dc, g->bmp);
    g->w = w; g->h = h;
    return TRUE;
}

void grab_free(GRAB *g)
{
    if (g->old) SelectObject(g->dc, g->old);
    DeleteObject(g->bmp);
    DeleteDC(g->dc);
}

void grab_frame(GRAB *g, int x, int y, BOOL cursor)
{
    HDC s = GetDC(NULL);
    BitBlt(g->dc, 0, 0, g->w, g->h, s, x, y, SRCCOPY | CAPTUREBLT);
    ReleaseDC(NULL, s);
    if (cursor) {
        CURSORINFO ci; ICONINFO ii;
        ci.cbSize = sizeof ci;
        if (GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING) && GetIconInfo(ci.hCursor, &ii)) {
            DrawIconEx(g->dc, ci.ptScreenPos.x - x - (int)ii.xHotspot,
                       ci.ptScreenPos.y - y - (int)ii.yHotspot, ci.hCursor, 0, 0, 0, NULL, DI_NORMAL);
            if (ii.hbmMask) DeleteObject(ii.hbmMask);
            if (ii.hbmColor) DeleteObject(ii.hbmColor);
        }
    }
}

void get_region(RECT *r)
{
    RECT w;
    if (g_full) { *r = g_fullRect; return; }
    GetWindowRect(g_frame, &w);
    r->left = w.left + BORDER;
    r->top = w.top + FRAME_TOP;
    r->right = w.right - BORDER;
    r->bottom = w.bottom - BORDER;
}

/* ------------------------------------------------------------------ */
/* Effets de clic souris (vidéo)                                       */
/* ------------------------------------------------------------------ */
static void click_add(int x, int y, int btn, UINT64 now)
{
    if (rc.clickCount < MAX_CLICKS) {
        rc.clickRings[rc.clickCount].x = x;
        rc.clickRings[rc.clickCount].y = y;
        rc.clickRings[rc.clickCount].btn = btn;
        rc.clickRings[rc.clickCount].t0 = now;
        rc.clickCount++;
    } else {
        memmove(&rc.clickRings[0], &rc.clickRings[1], sizeof(CLICKRING) * (MAX_CLICKS - 1));
        rc.clickRings[MAX_CLICKS - 1].x = x;
        rc.clickRings[MAX_CLICKS - 1].y = y;
        rc.clickRings[MAX_CLICKS - 1].btn = btn;
        rc.clickRings[MAX_CLICKS - 1].t0 = now;
    }
}

void click_poll(UINT64 now)
{
    BOOL curL, curR;
    int i = 0;
    while (i < rc.clickCount) {
        if (now >= rc.clickRings[i].t0 && now - rc.clickRings[i].t0 >= CLICK_DUR100) {
            memmove(&rc.clickRings[i], &rc.clickRings[i + 1], (size_t)(rc.clickCount - i - 1) * sizeof(CLICKRING));
            rc.clickCount--;
        } else {
            i++;
        }
    }
    if (!rc.clicks || rc.paused) { rc.prevL = rc.prevR = FALSE; return; }
    curL = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    curR = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    if ((curL && !rc.prevL) || (curR && !rc.prevR)) {
        POINT pt;
        GetCursorPos(&pt);
        if (curL && !rc.prevL) click_add(pt.x, pt.y, 1, now);
        if (curR && !rc.prevR) click_add(pt.x, pt.y, 2, now);
    }
    rc.prevL = curL;
    rc.prevR = curR;
}

BOOL click_any_active(UINT64 now)
{
    for (int i = 0; i < rc.clickCount; i++)
        if (now >= rc.clickRings[i].t0 && now - rc.clickRings[i].t0 < CLICK_DUR100) return TRUE;
    return FALSE;
}

void draw_click_rings_gdi(HDC dc, int rx, int ry, UINT64 now)
{
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    for (int i = 0; i < rc.clickCount; i++) {
        UINT64 age; double p; int rad, cx, cy; COLORREF col; HPEN pen, op;
        if (now < rc.clickRings[i].t0) continue;
        age = now - rc.clickRings[i].t0;
        if (age >= CLICK_DUR100) continue;
        p = (double)age / (double)CLICK_DUR100;
        rad = 6 + (int)(p * 18.0);
        cx = rc.clickRings[i].x - rx;
        cy = rc.clickRings[i].y - ry;
        col = rc.clickRings[i].btn == 2 ? RGB(255, 140, 0) : RGB(0, 165, 255);
        pen = CreatePen(PS_SOLID, 2, col);
        op = (HPEN)SelectObject(dc, pen);
        Ellipse(dc, cx - rad, cy - rad, cx + rad + 1, cy + rad + 1);
        SelectObject(dc, op);
        DeleteObject(pen);
    }
    SelectObject(dc, ob);
}

/* ------------------------------------------------------------------ */
/* Capture d'image (WIC + retardateur + presse-papiers)                */
/* ------------------------------------------------------------------ */
void take_screenshot(void)
{
    RECT r; GRAB g; WCHAR path[MAX_PATH];
    const GUID *fmts[3] = { &MY_GUID_ContainerFormatPng, &MY_GUID_ContainerFormatJpeg, &MY_GUID_ContainerFormatBmp };
    const WCHAR *ext[3] = { L"png", L"jpg", L"bmp" };
    IWICImagingFactory *f = NULL; IWICBitmap *bm = NULL; IWICStream *st = NULL;
    IWICBitmapEncoder *enc = NULL; IWICBitmapFrameEncode *fr = NULL; IPropertyBag2 *bag = NULL;
    WICPixelFormatGUID pf = MY_GUID_WICPixelFormat24bppBGR;
    HRESULT hr;
    int w, h, fi = g_cfg.imgfmt;

    if (fi < 0 || fi > 2) fi = 0;
    get_region(&r);
    w = r.right - r.left; h = r.bottom - r.top;
    if (w < 2 || h < 2 || !grab_init(&g, w, h)) return;
    grab_frame(&g, r.left, r.top, g_cfg.cursor);
    play_shutter();
    if (g_cfg.clip) copy_dib_clipboard(&g);
    SelectObject(g.dc, g.old); g.old = NULL;

    make_path(FALSE, ext[fi], path);
    hr = CoCreateInstance(&MY_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                          &MY_IID_IWICImagingFactory, (void **)&f);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateBitmapFromHBITMAP(f, g.bmp, NULL, WICBitmapIgnoreAlpha, &bm);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateStream(f, &st);
    if (SUCCEEDED(hr)) hr = IWICStream_InitializeFromFilename(st, path, GENERIC_WRITE);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateEncoder(f, fmts[fi], NULL, &enc);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_Initialize(enc, (IStream *)st, WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_CreateNewFrame(enc, &fr, &bag);
    if (SUCCEEDED(hr) && fi == 1 && bag) {
        PROPBAG2 o; VARIANT v;
        ZeroMemory(&o, sizeof o);
        o.pstrName = (LPOLESTR)L"ImageQuality";
        VariantInit(&v); v.vt = VT_R4; v.fltVal = g_cfg.jpgq / 100.0f;
        IPropertyBag2_Write(bag, 1, &o, &v);
    }
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_Initialize(fr, bag);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetSize(fr, w, h);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetPixelFormat(fr, &pf);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_WriteSource(fr, (IWICBitmapSource *)bm, NULL);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_Commit(fr);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_Commit(enc);

    REL(bag); REL(fr); REL(enc); REL(st); REL(bm); REL(f);
    grab_free(&g);
    if (FAILED(hr)) {
        DeleteFileW(path);
        balloon(T(S_SHOT_FAIL_T), T(S_SHOT_FAIL_M));
    } else {
        wcsncpy_s(g_lastFile, ARRAYSIZE(g_lastFile), path, _TRUNCATE);
        wcsncpy_s(g_lastShot, ARRAYSIZE(g_lastShot), path, _TRUNCATE);
        if (g_uiHidden || g_full) balloon(T(S_SAVED_T), PathFindFileNameW(path));
    }
}

void shot_trigger(void)
{
    if (g_shotWait > 0) {
        g_shotWait = 0;
        KillTimer(g_main, TIMER_COUNTDOWN);
        update_title();
        return;
    }
    if (g_cfg.delay > 0) {
        g_shotWait = g_cfg.delay;
        SetTimer(g_main, TIMER_COUNTDOWN, 1000, NULL);
        update_title();
    } else {
        take_screenshot();
    }
}

void shot_countdown_tick(void)
{
    if (g_shotWait <= 0) { KillTimer(g_main, TIMER_COUNTDOWN); return; }
    g_shotWait--;
    if (g_shotWait <= 0) {
        KillTimer(g_main, TIMER_COUNTDOWN);
        update_title();
        take_screenshot();
    } else {
        update_title();
    }
}

/* ------------------------------------------------------------------ */
/* Fil vidéo GDI                                                       */
/* ------------------------------------------------------------------ */
static IMFSample *make_video_sample(const void *bits, DWORD size, UINT64 ts, UINT64 dur)
{
    IMFMediaBuffer *b = NULL; IMFSample *s = NULL; BYTE *d = NULL;
    if (FAILED(MFCreateMemoryBuffer(size, &b))) return NULL;
    if (SUCCEEDED(IMFMediaBuffer_Lock(b, &d, NULL, NULL))) {
        memcpy(d, bits, size);
        IMFMediaBuffer_Unlock(b);
        IMFMediaBuffer_SetCurrentLength(b, size);
        if (SUCCEEDED(MFCreateSample(&s))) {
            IMFSample_AddBuffer(s, b);
            IMFSample_SetSampleTime(s, (LONGLONG)ts);
            IMFSample_SetSampleDuration(s, (LONGLONG)dur);
        }
    }
    REL(b);
    return s;
}

DWORD WINAPI video_thread(LPVOID unused)
{
    GRAB g; BYTE *last; DWORD size = (DWORD)rc.w * rc.h * 4;
    UINT64 dur = 10000000ULL / rc.fps, next = 0, pendTs = 0;
    IMFSample *pend = NULL; BOOL vfr = rc.vfr;
    (void)unused;
    if (!grab_init(&g, rc.w, rc.h)) return 0;
    last = (BYTE *)malloc(size);

    for (;;) {
        UINT64 now;
        if (WaitForSingleObject(rc.stopEv, 0) == WAIT_OBJECT_0) break;
        if (rc.paused) {
            if (WaitForSingleObject(rc.stopEv, 20) == WAIT_OBJECT_0) break;
            continue;
        }
        now = now100();
        click_poll(now);

        if (now < next) {
            if (WaitForSingleObject(rc.stopEv, (DWORD)((next - now) / 10000)) == WAIT_OBJECT_0) break;
            continue;
        }
        next += dur;
        if (now > next + dur * 2) next = now + dur;

        {
            RECT rr;
            get_region(&rr);
            grab_frame(&g, rr.left, rr.top, rc.cursor);
            if (rc.clicks) draw_click_rings_gdi(g.dc, rr.left, rr.top, now);
        }

        if (!vfr || !pend || now - pendTs >= 10000000ULL || click_any_active(now) || memcmp(last, g.bits, size) != 0) {
            IMFSample *s = make_video_sample(g.bits, size, now, dur);
            if (s) {
                if (vfr) {
                    if (pend) {
                        IMFSample_SetSampleDuration(pend, (LONGLONG)(now - pendTs));
                        IMFSinkWriter_WriteSample(rc.sw, rc.vidx, pend);
                        REL(pend);
                    }
                    pend = s; pendTs = now;
                    memcpy(last, g.bits, size);
                } else {
                    IMFSinkWriter_WriteSample(rc.sw, rc.vidx, s);
                    REL(s);
                }
            }
        }
    }
    if (pend) {
        UINT64 end = now100();
        IMFSample_SetSampleDuration(pend, (LONGLONG)(end > pendTs + dur ? end - pendTs : dur));
        IMFSinkWriter_WriteSample(rc.sw, rc.vidx, pend);
        REL(pend);
    }
    free(last);
    grab_free(&g);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Capture GPU : DXGI Desktop Duplication -> D3D11 VideoProcessor     */
/* ------------------------------------------------------------------ */
typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void *, REFIID, void **);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void *);
    ULONG   (STDMETHODCALLTYPE *Release)(void *);
    void    (STDMETHODCALLTYPE *Enter)(void *);
    void    (STDMETHODCALLTYPE *Leave)(void *);
    BOOL    (STDMETHODCALLTYPE *SetMultithreadProtected)(void *, BOOL);
    BOOL    (STDMETHODCALLTYPE *GetMultithreadProtected)(void *);
} MTVTBL;

typedef struct {
    ID3D11Device *dev; ID3D11DeviceContext *ctx;
    ID3D11VideoDevice *vdev; ID3D11VideoContext *vctx;
    ID3D11VideoProcessorEnumerator *venum; ID3D11VideoProcessor *vp;
    IDXGIOutput1 *out1; IDXGIOutputDuplication *dupl;
    IMFDXGIDeviceManager *mgr; UINT mgrToken;
    IMFVideoSampleAllocatorEx *alloc;
    ID3D11Texture2D *full, *desk, *comp, *stage;
    ID3D11VideoProcessorInputView *vDesk, *vComp;
    RECT mon; int monW, monH;
    int w, h, rx, ry;
    BOOL haveImage, ptrVisible; POINT ptrPos;
    BYTE *meta; UINT metaCap;
    BYTE *shape; UINT shapeCap;
    int cw, ch; BYTE *cbgra, *cmode;
} GPUCAP;
static GPUCAP gpu;

void gpu_free(void)
{
    GPUCAP *g = &gpu;
    encoder_set_d3d_manager(NULL);
    if (g->alloc) { IMFVideoSampleAllocatorEx_UninitializeSampleAllocator(g->alloc); REL(g->alloc); }
    REL(g->vDesk); REL(g->vComp);
    REL(g->full); REL(g->desk); REL(g->comp); REL(g->stage);
    REL(g->vp); REL(g->venum); REL(g->vctx); REL(g->vdev);
    REL(g->dupl); REL(g->out1);
    REL(g->mgr); REL(g->ctx); REL(g->dev);
    free(g->meta); free(g->shape); free(g->cbgra); free(g->cmode);
    ZeroMemory(g, sizeof *g);
}

static BOOL gpu_open_dup(GPUCAP *g)
{
    DXGI_OUTDUPL_DESC dd;
    REL(g->dupl);
    if (FAILED(IDXGIOutput1_DuplicateOutput(g->out1, (IUnknown *)g->dev, &g->dupl))) return FALSE;
    IDXGIOutputDuplication_GetDesc(g->dupl, &dd);
    if (dd.ModeDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM || dd.DesktopImageInSystemMemory) { REL(g->dupl); return FALSE; }
    return TRUE;
}

static BOOL gpu_tex(GPUCAP *g, ID3D11Texture2D **pt, int w, int h, D3D11_USAGE usage, UINT bind, UINT cpu)
{
    D3D11_TEXTURE2D_DESC td;
    ZeroMemory(&td, sizeof td);
    td.Width = (UINT)w; td.Height = (UINT)h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = usage; td.BindFlags = bind; td.CPUAccessFlags = cpu;
    return SUCCEEDED(ID3D11Device_CreateTexture2D(g->dev, &td, NULL, pt));
}

BOOL gpu_init(int *pw, int *ph, int fps)
{
    GPUCAP *g = &gpu;
    RECT r; POINT c; HMONITOR hm;
    IDXGIFactory1 *fac = NULL; IDXGIAdapter *ad = NULL; IDXGIOutput *out = NULL, *fout = NULL;
    DXGI_OUTPUT_DESC od; DXGI_OUTDUPL_DESC dd;
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC cd;
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE csIn, csOut;
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC iv;
    IMFActivate **acts = NULL; UINT32 nact = 0;
    IMFMediaType *nv = NULL; IMFAttributes *aa = NULL;
    UINT fmt = 0; UINT i, j; int w, h;
    void *mt = NULL;

    ZeroMemory(g, sizeof *g);
    get_region(&r);
    c.x = (r.left + r.right) / 2; c.y = (r.top + r.bottom) / 2;
    hm = MonitorFromPoint(c, MONITOR_DEFAULTTONEAREST);

    if (FAILED(CreateDXGIFactory1(&MY_IID_IDXGIFactory1, (void **)&fac))) return FALSE;
    for (i = 0; !fout; i++) {
        if (IDXGIFactory1_EnumAdapters(fac, i, &ad) == DXGI_ERROR_NOT_FOUND) break;
        for (j = 0; ; j++) {
            if (IDXGIAdapter_EnumOutputs(ad, j, &out) == DXGI_ERROR_NOT_FOUND) break;
            if (SUCCEEDED(IDXGIOutput_GetDesc(out, &od)) && od.Monitor == hm) { fout = out; break; }
            REL(out);
        }
        if (!fout) REL(ad);
    }
    REL(fac);
    if (!fout) goto fail;
    if (od.Rotation != DXGI_MODE_ROTATION_IDENTITY && od.Rotation != DXGI_MODE_ROTATION_UNSPECIFIED) goto fail;
    g->mon = od.DesktopCoordinates;

    if (FAILED(D3D11CreateDevice(ad, D3D_DRIVER_TYPE_UNKNOWN, NULL,
                                 D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                                 &fl, 1, D3D11_SDK_VERSION, &g->dev, NULL, &g->ctx))) goto fail;
    REL(ad);
    if (FAILED(IDXGIOutput_QueryInterface(fout, &MY_IID_IDXGIOutput1, (void **)&g->out1))) goto fail;
    REL(fout);

    if (SUCCEEDED(ID3D11Device_QueryInterface(g->dev, &MY_IID_ID3D10Multithread, &mt)) && mt) {
        MTVTBL *vt = *(MTVTBL **)mt;
        vt->SetMultithreadProtected(mt, TRUE);
        vt->Release(mt);
    }
    if (FAILED(ID3D11Device_QueryInterface(g->dev, &MY_IID_ID3D11VideoDevice, (void **)&g->vdev))) goto fail;
    if (FAILED(ID3D11DeviceContext_QueryInterface(g->ctx, &MY_IID_ID3D11VideoContext, (void **)&g->vctx))) goto fail;

    {
        MFT_REGISTER_TYPE_INFO in, out2;
        in.guidMajorType = MFMediaType_Video;  in.guidSubtype = MFVideoFormat_NV12;
        out2.guidMajorType = MFMediaType_Video;
        out2.guidSubtype = g_cfg.vcodec ? MY_MFVideoFormat_HEVC : MFVideoFormat_H264;
        if (FAILED(MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
                             &in, &out2, &acts, &nact)) || nact == 0) goto fail;
        for (i = 0; i < nact; i++) IUnknown_Release((IUnknown *)acts[i]);
        CoTaskMemFree(acts);
    }

    if (!gpu_open_dup(g)) goto fail;
    IDXGIOutputDuplication_GetDesc(g->dupl, &dd);
    g->monW = (int)dd.ModeDesc.Width; g->monH = (int)dd.ModeDesc.Height;
    w = *pw; h = *ph;
    if (w > g->monW) w = g->monW;
    if (h > g->monH) h = g->monH;
    w &= ~1; h &= ~1;
    if (w < 128 || h < 72) goto fail;
    g->w = w; g->h = h;

    if (!gpu_tex(g, &g->full, g->monW, g->monH, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0) ||
        !gpu_tex(g, &g->desk, w, h, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0) ||
        !gpu_tex(g, &g->comp, w, h, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0) ||
        !gpu_tex(g, &g->stage, 256, 256, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ)) goto fail;

    ZeroMemory(&cd, sizeof cd);
    cd.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    cd.InputFrameRate.Numerator = (UINT)fps; cd.InputFrameRate.Denominator = 1;
    cd.InputWidth = (UINT)w; cd.InputHeight = (UINT)h;
    cd.OutputFrameRate = cd.InputFrameRate;
    cd.OutputWidth = (UINT)w; cd.OutputHeight = (UINT)h;
    cd.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
    if (FAILED(ID3D11VideoDevice_CreateVideoProcessorEnumerator(g->vdev, &cd, &g->venum))) goto fail;
    if (FAILED(ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(g->venum, DXGI_FORMAT_NV12, &fmt)) ||
        !(fmt & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT)) goto fail;
    fmt = 0;
    if (FAILED(ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(g->venum, DXGI_FORMAT_B8G8R8A8_UNORM, &fmt)) ||
        !(fmt & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT)) goto fail;
    if (FAILED(ID3D11VideoDevice_CreateVideoProcessor(g->vdev, g->venum, 0, &g->vp))) goto fail;
    ZeroMemory(&csIn, sizeof csIn);  csIn.RGB_Range = 0;
    ZeroMemory(&csOut, sizeof csOut); csOut.YCbCr_Matrix = 1;
    csOut.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;
    ID3D11VideoContext_VideoProcessorSetStreamColorSpace(g->vctx, g->vp, 0, &csIn);
    ID3D11VideoContext_VideoProcessorSetOutputColorSpace(g->vctx, g->vp, &csOut);
    ID3D11VideoContext_VideoProcessorSetStreamFrameFormat(g->vctx, g->vp, 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
    ID3D11VideoContext_VideoProcessorSetStreamAutoProcessingMode(g->vctx, g->vp, 0, FALSE);

    ZeroMemory(&iv, sizeof iv);
    iv.FourCC = 0; iv.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    iv.Texture2D.MipSlice = 0; iv.Texture2D.ArraySlice = 0;
    if (FAILED(ID3D11VideoDevice_CreateVideoProcessorInputView(g->vdev, (ID3D11Resource *)g->desk, g->venum, &iv, &g->vDesk)) ||
        FAILED(ID3D11VideoDevice_CreateVideoProcessorInputView(g->vdev, (ID3D11Resource *)g->comp, g->venum, &iv, &g->vComp))) goto fail;

    if (FAILED(MFCreateDXGIDeviceManager(&g->mgrToken, &g->mgr))) goto fail;
    if (FAILED(IMFDXGIDeviceManager_ResetDevice(g->mgr, (IUnknown *)g->dev, g->mgrToken))) goto fail;
    encoder_set_d3d_manager(g->mgr);
    if (FAILED(MFCreateVideoSampleAllocatorEx(&MY_IID_IMFVideoSampleAllocatorEx, (void **)&g->alloc))) goto fail;
    if (FAILED(IMFVideoSampleAllocatorEx_SetDirectXManager(g->alloc, (IUnknown *)g->mgr))) goto fail;
    nv = vtype(&MFVideoFormat_NV12, w, h, fps, 0);
    if (!nv || FAILED(MFCreateAttributes(&aa, 1))) goto fail;
    IMFAttributes_SetUINT32(aa, &MF_SA_D3D11_BINDFLAGS, D3D11_BIND_RENDER_TARGET);
    if (FAILED(IMFVideoSampleAllocatorEx_InitializeSampleAllocatorEx(g->alloc, 3, 8, aa, nv))) goto fail;
    REL(aa); REL(nv);

    *pw = w; *ph = h;
    return TRUE;

fail:
    REL(aa); REL(nv); REL(ad); REL(fout);
    gpu_free();
    return FALSE;
}

static void gpu_update_region(GPUCAP *g)
{
    RECT r; int x, y;
    get_region(&r);
    x = r.left - g->mon.left;
    y = r.top - g->mon.top;
    if (x > g->monW - g->w) x = g->monW - g->w;
    if (y > g->monH - g->h) y = g->monH - g->h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    g->rx = x; g->ry = y;
}

static BOOL gpu_region_dirty(GPUCAP *g, const DXGI_OUTDUPL_FRAME_INFO *fi)
{
    UINT n1 = 0, n2 = 0, k;
    RECT reg, t;
    DXGI_OUTDUPL_MOVE_RECT *mr; RECT *dr;
    reg.left = g->rx; reg.top = g->ry; reg.right = g->rx + g->w; reg.bottom = g->ry + g->h;
    if (fi->TotalMetadataBufferSize == 0) return TRUE;
    if (fi->TotalMetadataBufferSize > g->metaCap) {
        free(g->meta);
        g->metaCap = fi->TotalMetadataBufferSize;
        g->meta = (BYTE *)malloc(g->metaCap);
        if (!g->meta) { g->metaCap = 0; return TRUE; }
    }
    mr = (DXGI_OUTDUPL_MOVE_RECT *)g->meta;
    if (FAILED(IDXGIOutputDuplication_GetFrameMoveRects(g->dupl, g->metaCap, mr, &n1))) return TRUE;
    for (k = 0; k < n1 / sizeof(DXGI_OUTDUPL_MOVE_RECT); k++)
        if (IntersectRect(&t, &reg, &mr[k].DestinationRect)) return TRUE;
    dr = (RECT *)(g->meta + n1);
    if (FAILED(IDXGIOutputDuplication_GetFrameDirtyRects(g->dupl, g->metaCap - n1, dr, &n2))) return TRUE;
    for (k = 0; k < n2 / sizeof(RECT); k++)
        if (IntersectRect(&t, &reg, &dr[k])) return TRUE;
    return FALSE;
}

static void gpu_build_cursor(GPUCAP *g, const DXGI_OUTDUPL_POINTER_SHAPE_INFO *si, const BYTE *src)
{
    int w = (int)si->Width, h = (int)si->Height, x, y;
    free(g->cbgra); free(g->cmode);
    g->cbgra = NULL; g->cmode = NULL; g->cw = g->ch = 0;
    if (si->Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME) h /= 2;
    if (w <= 0 || h <= 0 || w > 256 || h > 256) return;
    g->cbgra = (BYTE *)calloc((size_t)w * h, 4);
    g->cmode = (BYTE *)calloc((size_t)w * h, 1);
    if (!g->cbgra || !g->cmode) { free(g->cbgra); free(g->cmode); g->cbgra = g->cmode = NULL; return; }
    g->cw = w; g->ch = h;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            BYTE *d = g->cbgra + ((size_t)y * w + x) * 4, *md = g->cmode + (size_t)y * w + x;
            if (si->Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME) {
                int bit = 7 - (x & 7);
                int a = (src[(size_t)y * si->Pitch + x / 8] >> bit) & 1;
                int xr = (src[(size_t)(y + h) * si->Pitch + x / 8] >> bit) & 1;
                if (!a)       { d[0] = d[1] = d[2] = xr ? 255 : 0; d[3] = 255; *md = 1; }
                else if (xr)  { d[0] = d[1] = d[2] = 255; d[3] = 255; *md = 3; }
                else          *md = 0;
            } else {
                memcpy(d, src + (size_t)y * si->Pitch + (size_t)x * 4, 4);
                if (si->Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) *md = d[3] ? 2 : 0;
                else *md = d[3] ? 3 : 1;
            }
        }
}

static BOOL gpu_cursor_in_region(GPUCAP *g)
{
    int cx = g->ptrPos.x - g->rx, cy = g->ptrPos.y - g->ry;
    return g->ptrVisible && g->cbgra && cx < g->w && cy < g->h && cx + g->cw > 0 && cy + g->ch > 0;
}

static void gpu_on_frame(GPUCAP *g, const DXGI_OUTDUPL_FRAME_INFO *fi, IDXGIResource *res, BOOL cursorOn, BOOL *dirty)
{
    if (fi->LastPresentTime.QuadPart != 0 && res) {
        ID3D11Texture2D *tex = NULL;
        if (SUCCEEDED(IDXGIResource_QueryInterface(res, &MY_IID_ID3D11Texture2D, (void **)&tex))) {
            ID3D11DeviceContext_CopyResource(g->ctx, (ID3D11Resource *)g->full, (ID3D11Resource *)tex);
            REL(tex);
            if (!g->haveImage || gpu_region_dirty(g, fi)) *dirty = TRUE;
            g->haveImage = TRUE;
        }
    }
    if (cursorOn && fi->LastMouseUpdateTime.QuadPart != 0) {
        BOOL was = gpu_cursor_in_region(g);
        g->ptrVisible = fi->PointerPosition.Visible != 0;
        g->ptrPos = fi->PointerPosition.Position;
        if (fi->PointerShapeBufferSize > 0) {
            DXGI_OUTDUPL_POINTER_SHAPE_INFO si; UINT need = 0;
            if (fi->PointerShapeBufferSize > g->shapeCap) {
                free(g->shape);
                g->shapeCap = fi->PointerShapeBufferSize;
                g->shape = (BYTE *)malloc(g->shapeCap);
                if (!g->shape) g->shapeCap = 0;
            }
            if (g->shape && SUCCEEDED(IDXGIOutputDuplication_GetFramePointerShape(g->dupl, g->shapeCap, g->shape, &need, &si)))
                gpu_build_cursor(g, &si, g->shape);
        }
        if (was || gpu_cursor_in_region(g)) *dirty = TRUE;
    }
}

static BOOL gpu_draw_cursor(GPUCAP *g)
{
    int cx = g->ptrPos.x - g->rx, cy = g->ptrPos.y - g->ry;
    int x0 = cx < 0 ? 0 : cx, y0 = cy < 0 ? 0 : cy;
    int x1 = cx + g->cw > g->w ? g->w : cx + g->cw, y1 = cy + g->ch > g->h ? g->h : cy + g->ch;
    int iw = x1 - x0, ih = y1 - y0, x, y, k;
    D3D11_BOX box; D3D11_MAPPED_SUBRESOURCE ms; BYTE *tmp;
    if (!gpu_cursor_in_region(g) || iw <= 0 || ih <= 0 || iw > 256 || ih > 256) return FALSE;
    ID3D11DeviceContext_CopyResource(g->ctx, (ID3D11Resource *)g->comp, (ID3D11Resource *)g->desk);
    box.left = (UINT)x0; box.top = (UINT)y0; box.front = 0; box.right = (UINT)x1; box.bottom = (UINT)y1; box.back = 1;
    ID3D11DeviceContext_CopySubresourceRegion(g->ctx, (ID3D11Resource *)g->stage, 0, 0, 0, 0, (ID3D11Resource *)g->comp, 0, &box);
    if (FAILED(ID3D11DeviceContext_Map(g->ctx, (ID3D11Resource *)g->stage, 0, D3D11_MAP_READ, 0, &ms))) return TRUE;
    tmp = (BYTE *)malloc((size_t)iw * ih * 4);
    if (!tmp) { ID3D11DeviceContext_Unmap(g->ctx, (ID3D11Resource *)g->stage, 0); return TRUE; }
    for (y = 0; y < ih; y++) memcpy(tmp + (size_t)y * iw * 4, (BYTE *)ms.pData + (size_t)y * ms.RowPitch, (size_t)iw * 4);
    ID3D11DeviceContext_Unmap(g->ctx, (ID3D11Resource *)g->stage, 0);
    for (y = 0; y < ih; y++)
        for (x = 0; x < iw; x++) {
            int s = (y0 + y - cy) * g->cw + (x0 + x - cx);
            const BYTE *c = g->cbgra + (size_t)s * 4;
            BYTE *d = tmp + ((size_t)y * iw + x) * 4;
            switch (g->cmode[s]) {
            case 1: d[0] = c[0]; d[1] = c[1]; d[2] = c[2]; break;
            case 2: for (k = 0; k < 3; k++) d[k] = (BYTE)((c[k] * c[3] + d[k] * (255 - c[3])) / 255); break;
            case 3: d[0] ^= c[0]; d[1] ^= c[1]; d[2] ^= c[2]; break;
            default: break;
            }
        }
    ID3D11DeviceContext_UpdateSubresource(g->ctx, (ID3D11Resource *)g->comp, 0, &box, tmp, (UINT)iw * 4, 0);
    free(tmp);
    return TRUE;
}

static BOOL gpu_draw_clicks(GPUCAP *g, BOOL compReady, UINT64 now)
{
    for (int i = 0; i < rc.clickCount; i++) {
        UINT64 age; double p; int rad, rcx, rcy, x0, y0, x1, y1, iw, ih, x, y;
        BYTE rb, gb, bb; D3D11_BOX box; D3D11_MAPPED_SUBRESOURCE ms; BYTE *tmp;
        if (now < rc.clickRings[i].t0) continue;
        age = now - rc.clickRings[i].t0;
        if (age >= CLICK_DUR100) continue;
        p = (double)age / (double)CLICK_DUR100;
        rad = 6 + (int)(p * 18.0);
        rcx = rc.clickRings[i].x - g->mon.left - g->rx;
        rcy = rc.clickRings[i].y - g->mon.top - g->ry;
        x0 = imax(0, rcx - rad - 3); y0 = imax(0, rcy - rad - 3);
        x1 = imin(g->w, rcx + rad + 4); y1 = imin(g->h, rcy + rad + 4);
        iw = x1 - x0; ih = y1 - y0;
        if (iw <= 0 || ih <= 0 || iw > 256 || ih > 256) continue;
        if (!compReady) {
            ID3D11DeviceContext_CopyResource(g->ctx, (ID3D11Resource *)g->comp, (ID3D11Resource *)g->desk);
            compReady = TRUE;
        }
        box.left = (UINT)x0; box.top = (UINT)y0; box.front = 0;
        box.right = (UINT)x1; box.bottom = (UINT)y1; box.back = 1;
        ID3D11DeviceContext_CopySubresourceRegion(g->ctx, (ID3D11Resource *)g->stage, 0, 0, 0, 0,
                                                  (ID3D11Resource *)g->comp, 0, &box);
        if (FAILED(ID3D11DeviceContext_Map(g->ctx, (ID3D11Resource *)g->stage, 0, D3D11_MAP_READ, 0, &ms))) continue;
        tmp = (BYTE *)malloc((size_t)iw * ih * 4);
        if (!tmp) { ID3D11DeviceContext_Unmap(g->ctx, (ID3D11Resource *)g->stage, 0); continue; }
        for (y = 0; y < ih; y++)
            memcpy(tmp + (size_t)y * iw * 4, (BYTE *)ms.pData + (size_t)y * ms.RowPitch, (size_t)iw * 4);
        ID3D11DeviceContext_Unmap(g->ctx, (ID3D11Resource *)g->stage, 0);
        if (rc.clickRings[i].btn == 2) { rb = 255; gb = 140; bb = 0; }
        else                           { rb = 0;   gb = 165; bb = 255; }
        for (y = 0; y < ih; y++)
            for (x = 0; x < iw; x++) {
                double dx = (x0 + x) - rcx, dy = (y0 + y) - rcy;
                double dist = fabs(sqrt(dx * dx + dy * dy) - (double)rad);
                if (dist <= 1.6) {
                    double a = (1.0 - dist / 1.6) * (1.0 - 0.55 * p);
                    BYTE *d = tmp + ((size_t)y * iw + x) * 4;
                    d[0] = (BYTE)(bb * a + d[0] * (1.0 - a));
                    d[1] = (BYTE)(gb * a + d[1] * (1.0 - a));
                    d[2] = (BYTE)(rb * a + d[2] * (1.0 - a));
                }
            }
        ID3D11DeviceContext_UpdateSubresource(g->ctx, (ID3D11Resource *)g->comp, 0, &box, tmp, (UINT)iw * 4, 0);
        free(tmp);
    }
    return compReady;
}

static IMFSample *gpu_frame(GPUCAP *g, BOOL withCursor, UINT64 now)
{
    IMFSample *s = NULL; IMFMediaBuffer *mb = NULL; IMFDXGIBuffer *db = NULL; ID3D11Texture2D *tex = NULL;
    ID3D11VideoProcessorOutputView *ov = NULL; ID3D11VideoProcessorInputView *in;
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC od; D3D11_VIDEO_PROCESSOR_STREAM st; D3D11_BOX box;
    BOOL ok = FALSE, compReady; int tries;

    for (tries = 0; tries < 25; tries++) {
        if (SUCCEEDED(IMFVideoSampleAllocatorEx_AllocateSample(g->alloc, &s)) && s) break;
        s = NULL;
        Sleep(2);
    }
    if (!s) return NULL;
    if (FAILED(IMFSample_GetBufferByIndex(s, 0, &mb))) goto done;
    if (FAILED(IMFMediaBuffer_QueryInterface(mb, &MY_IID_IMFDXGIBuffer, (void **)&db))) goto done;
    if (FAILED(IMFDXGIBuffer_GetResource(db, &MY_IID_ID3D11Texture2D, (void **)&tex))) goto done;

    box.left = (UINT)g->rx; box.top = (UINT)g->ry; box.front = 0;
    box.right = (UINT)(g->rx + g->w); box.bottom = (UINT)(g->ry + g->h); box.back = 1;
    ID3D11DeviceContext_CopySubresourceRegion(g->ctx, (ID3D11Resource *)g->desk, 0, 0, 0, 0, (ID3D11Resource *)g->full, 0, &box);
    compReady = withCursor ? gpu_draw_cursor(g) : FALSE;
    if (rc.clicks) compReady = gpu_draw_clicks(g, compReady, now);
    in = compReady ? g->vComp : g->vDesk;

    ZeroMemory(&od, sizeof od);
    od.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    od.Texture2D.MipSlice = 0;
    if (FAILED(ID3D11VideoDevice_CreateVideoProcessorOutputView(g->vdev, (ID3D11Resource *)tex, g->venum, &od, &ov))) goto done;
    ZeroMemory(&st, sizeof st);
    st.Enable = TRUE;
    st.pInputSurface = in;
    if (FAILED(ID3D11VideoContext_VideoProcessorBlt(g->vctx, g->vp, ov, 0, 1, &st))) goto done;
    ID3D11DeviceContext_Flush(g->ctx);

    {
        IMF2DBuffer *b2 = NULL; DWORD len = 0;
        if (SUCCEEDED(IMFMediaBuffer_QueryInterface(mb, &MY_IID_IMF2DBuffer, (void **)&b2)) && b2) {
            IMF2DBuffer_GetContiguousLength(b2, &len);
            REL(b2);
        }
        if (!len) len = (DWORD)(g->w * g->h * 3 / 2);
        IMFMediaBuffer_SetCurrentLength(mb, len);
    }
    ok = TRUE;
done:
    REL(ov); REL(tex); REL(db); REL(mb);
    if (!ok) REL(s);
    return s;
}

DWORD WINAPI video_thread_gpu(LPVOID unused)
{
    GPUCAP *g = &gpu;
    UINT64 dur = 10000000ULL / rc.fps, next = 0, pendTs = 0;
    BOOL dirty = TRUE, vfr = rc.vfr, wasPaused = FALSE;
    IMFSample *pend = NULL;
    int lrx = -1, lry = -1;
    (void)unused;

    while (WaitForSingleObject(rc.stopEv, 0) != WAIT_OBJECT_0) {
        UINT64 now;
        DWORD tmo;
        DXGI_OUTDUPL_FRAME_INFO fi; IDXGIResource *res = NULL; HRESULT hr;
        if (rc.paused) {
            wasPaused = TRUE;
            if (WaitForSingleObject(rc.stopEv, 20) == WAIT_OBJECT_0) break;
            continue;
        }
        if (wasPaused) { wasPaused = FALSE; dirty = TRUE; }
        now = now100();
        click_poll(now);
        tmo = now < next ? (DWORD)((next - now) / 10000) : 0;

        hr = IDXGIOutputDuplication_AcquireNextFrame(g->dupl, tmo, &fi, &res);
        if (SUCCEEDED(hr)) {
            gpu_on_frame(g, &fi, res, rc.cursor, &dirty);
            IDXGIOutputDuplication_ReleaseFrame(g->dupl);
            REL(res);
        } else if (hr == DXGI_ERROR_ACCESS_LOST) {
            if (!gpu_open_dup(g)) Sleep(100);
            continue;
        } else if (hr != DXGI_ERROR_WAIT_TIMEOUT) {
            Sleep(5);
        }

        now = now100();
        click_poll(now);
        if (now < next) continue;
        next += dur;
        if (now > next + dur * 2) next = now + dur;

        gpu_update_region(g);
        if (g->rx != lrx || g->ry != lry) { lrx = g->rx; lry = g->ry; dirty = TRUE; }
        if (click_any_active(now)) dirty = TRUE;
        if (!g->haveImage) continue;

        if (!vfr || !pend || dirty || now - pendTs >= 10000000ULL) {
            IMFSample *s = gpu_frame(g, rc.cursor, now);
            if (s) {
                IMFSample_SetSampleTime(s, (LONGLONG)now);
                IMFSample_SetSampleDuration(s, (LONGLONG)dur);
                if (vfr) {
                    if (pend) {
                        IMFSample_SetSampleDuration(pend, (LONGLONG)(now - pendTs));
                        IMFSinkWriter_WriteSample(rc.sw, rc.vidx, pend);
                        REL(pend);
                    }
                    pend = s; pendTs = now;
                } else {
                    IMFSinkWriter_WriteSample(rc.sw, rc.vidx, s);
                    REL(s);
                }
            }
            dirty = FALSE;
        }
    }
    if (pend) {
        UINT64 end = now100();
        IMFSample_SetSampleDuration(pend, (LONGLONG)(end > pendTs + dur ? end - pendTs : dur));
        IMFSinkWriter_WriteSample(rc.sw, rc.vidx, pend);
        REL(pend);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Ciblage fenêtre / UI Automation + loupe de précision x4             */
/* ------------------------------------------------------------------ */
#define MAXC 64
static IUIAutomation *g_uia;
static IUIAutomationTreeWalker *g_walker;
static RECT g_cand[MAXC];
static int  g_candN;
static BOOL g_candFull;
static int  g_pickLevel;
static POINT g_pickPt = { -1, -1 };
static RECT g_pickRc;
static BOOL g_pickHave;
static POINT g_pollPt = { -1, -1 };
static int  g_pollLvl = -1;
static BOOL g_drag, g_dragMoved;
static POINT g_dragA;
static HHOOK g_mhook;

void uia_release(void)
{
    REL(g_walker);
    REL(g_uia);
}

static void uia_init(void)
{
    if (g_uia) return;
    if (FAILED(CoCreateInstance(&MY_CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER,
                                &MY_IID_IUIAutomation, (void **)&g_uia))) { g_uia = NULL; return; }
    if (FAILED(IUIAutomation_get_ControlViewWalker(g_uia, &g_walker))) { g_walker = NULL; REL(g_uia); }
}

static BOOL is_desktop(HWND t)
{
    HWND r = GetAncestor(t, GA_ROOT);
    WCHAR cls[32];
    if (!r || r == GetDesktopWindow() || r == GetShellWindow()) return TRUE;
    GetClassNameW(r, cls, ARRAYSIZE(cls));
    return !wcscmp(cls, L"WorkerW") || !wcscmp(cls, L"Progman");
}

static BOOL hwnd_rect(HWND w, RECT *b)
{
    if (!IsWindowVisible(w)) return FALSE;
    if (GetAncestor(w, GA_PARENT) == GetDesktopWindow()) {
        if (FAILED(DwmGetWindowAttribute(w, DWMWA_EXTENDED_FRAME_BOUNDS, b, sizeof *b)))
            GetWindowRect(w, b);
    } else {
        GetWindowRect(w, b);
    }
    return b->right > b->left && b->bottom > b->top;
}

static void cand_add(const RECT *r, POINT pt)
{
    RECT t = *r; LONGLONG a; int i, j;
    if (r->right <= r->left || r->bottom <= r->top || g_candN >= MAXC) return;
    InflateRect(&t, 2, 2);
    if (!PtInRect(&t, pt)) return;
    for (i = 0; i < g_candN; i++) if (EqualRect(&g_cand[i], r)) return;
    a = (LONGLONG)(r->right - r->left) * (r->bottom - r->top);
    for (i = 0; i < g_candN; i++)
        if ((LONGLONG)(g_cand[i].right - g_cand[i].left) * (g_cand[i].bottom - g_cand[i].top) > a) break;
    for (j = g_candN; j > i; j--) g_cand[j] = g_cand[j - 1];
    g_cand[i] = *r;
    g_candN++;
}

static int cand_build(POINT pt, BOOL full)
{
    HWND t, top, c, w;
    t = WindowFromPoint(pt);
    if (t == g_hl || t == g_mag) return 2;
    g_candN = 0;
    if (!t) return 0;
    top = GetAncestor(t, GA_ROOT);
    if (top == g_main || top == g_frame || top == g_cfgwnd || top == g_szwnd) return 0;

    if (!is_desktop(t) && !IsIconic(top)) {
        c = t;
        for (;;) {
            POINT cp = pt; HWND ch;
            ScreenToClient(c, &cp);
            ch = ChildWindowFromPointEx(c, cp, CWP_SKIPINVISIBLE);
            if (!ch || ch == c) break;
            c = ch;
        }
        for (w = c; w; ) {
            RECT r;
            if (hwnd_rect(w, &r)) cand_add(&r, pt);
            if (w == top) break;
            w = GetAncestor(w, GA_PARENT);
            if (w == GetDesktopWindow()) break;
        }
    }

    if (g_uia && g_walker) {
        IUIAutomationElement *e = NULL; int pid = 0, n;
        if (SUCCEEDED(IUIAutomation_ElementFromPoint(g_uia, pt, &e)) && e) {
            IUIAutomationElement_get_CurrentProcessId(e, &pid);
            if (pid == (int)GetCurrentProcessId()) { REL(e); }
            for (n = 0; e && n < 40; n++) {
                RECT r; IUIAutomationElement *par = NULL;
                BOOL okr = SUCCEEDED(IUIAutomationElement_get_CurrentBoundingRectangle(e, &r));
                IUIAutomationTreeWalker_GetParentElement(g_walker, e, &par);
                if (!par) { REL(e); break; }
                if (okr) cand_add(&r, pt);
                REL(e);
                if (!full) { REL(par); break; }
                e = par;
            }
            REL(e);
        }
    }
    return g_candN > 0 ? 1 : 0;
}

static void pick_highlight(const RECT *b)
{
    HRGN a, in; int w, h, th = S(3);
    if (!b) { ShowWindow(g_hl, SW_HIDE); return; }
    w = b->right - b->left; h = b->bottom - b->top;
    SetWindowPos(g_hl, HWND_TOPMOST, b->left, b->top, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    a  = CreateRectRgn(0, 0, w, h);
    in = CreateRectRgn(th, th, imax(w - th, th), imax(h - th, th));
    CombineRgn(a, a, in, RGN_DIFF);
    DeleteObject(in);
    SetWindowRgn(g_hl, a, TRUE);
}

static void mag_update(POINT pt)
{
    MONITORINFO mi; int w = S(136), h = S(156), mx, my;
    if (!g_mag) return;
    mi.cbSize = sizeof mi;
    mx = pt.x + S(22); my = pt.y + S(22);
    if (GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi)) {
        if (mx + w > mi.rcMonitor.right)  mx = pt.x - w - S(18);
        if (my + h > mi.rcMonitor.bottom) my = pt.y - h - S(18);
    }
    SetWindowPos(g_mag, HWND_TOPMOST, mx, my, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g_mag, NULL, FALSE);
}

LRESULT CALLBACK MagProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps), sdc, mdc;
        RECT rc, bar; HBITMAP bmp; HGDIOBJ ob;
        POINT pt; COLORREF px; HPEN pen, op; HBRUSH bg, bd;
        int zoomH, cx, cy, half = 15, srcN = 31;
        WCHAR txt[48];

        GetClientRect(h, &rc);
        GetCursorPos(&pt);
        zoomH = rc.bottom - S(22);
        sdc = GetDC(NULL);
        mdc = CreateCompatibleDC(sdc);
        bmp = CreateCompatibleBitmap(sdc, srcN, srcN);
        ob = SelectObject(mdc, bmp);
        BitBlt(mdc, 0, 0, srcN, srcN, sdc, pt.x - half, pt.y - half, SRCCOPY);
        px = GetPixel(mdc, half, half);
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchBlt(dc, 1, 1, rc.right - 2, zoomH - 1, mdc, 0, 0, srcN, srcN, SRCCOPY);
        SelectObject(mdc, ob);
        DeleteObject(bmp);
        DeleteDC(mdc);
        ReleaseDC(NULL, sdc);

        /* réticule central sur le pixel visé */
        cx = rc.right / 2; cy = zoomH / 2;
        pen = CreatePen(PS_SOLID, 1, RGB(0, 180, 255));
        op = (HPEN)SelectObject(dc, pen);
        MoveToEx(dc, cx, 1, NULL); LineTo(dc, cx, zoomH);
        MoveToEx(dc, 1, cy, NULL); LineTo(dc, rc.right - 1, cy);
        SelectObject(dc, op);
        DeleteObject(pen);

        /* bandeau coordonnées + couleur */
        bar.left = 0; bar.top = zoomH; bar.right = rc.right; bar.bottom = rc.bottom;
        bg = CreateSolidBrush(RGB(28, 28, 30));
        FillRect(dc, &bar, bg);
        DeleteObject(bg);
        bd = CreateSolidBrush(RGB(90, 90, 95));
        FrameRect(dc, &rc, bd);
        DeleteObject(bd);

        swprintf_s(txt, ARRAYSIZE(txt), L"%d,%d  #%02X%02X%02X",
                   (int)pt.x, (int)pt.y, GetRValue(px), GetGValue(px), GetBValue(px));
        SelectObject(dc, g_font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(240, 240, 240));
        DrawTextW(dc, txt, -1, &bar, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(h, &ps);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void pick_status(const RECT *b)
{
    swprintf_s(g_status, ARRAYSIZE(g_status), T(S_PICK_STATUS),
               (int)(b->right - b->left), (int)(b->bottom - b->top));
    InvalidateRect(g_main, &g_textRc, TRUE);
}

static void pick_move(void)
{
    POINT pt; int r = 1;
    GetCursorPos(&pt);
    mag_update(pt);
    if (pt.x != g_pickPt.x || pt.y != g_pickPt.y) {
        g_pickPt = pt; g_candFull = FALSE;
        g_pickLevel = 0;
        r = cand_build(pt, FALSE);
    } else if (g_pickLevel > 0 && !g_candFull) {
        r = cand_build(pt, TRUE);
        g_candFull = TRUE;
    }
    if (r == 2) return;
    if (g_pickLevel >= g_candN) g_pickLevel = g_candN - 1;
    if (g_pickLevel < 0) g_pickLevel = 0;
    g_pickHave = g_candN > 0;
    if (g_pickHave) { g_pickRc = g_cand[g_pickLevel]; pick_status(&g_pickRc); }
    pick_highlight(g_pickHave ? &g_pickRc : NULL);
}

static LRESULT CALLBACK MouseHook(int code, WPARAM w, LPARAM l)
{
    if (code == HC_ACTION && g_picking) {
        MSLLHOOKSTRUCT *ms = (MSLLHOOKSTRUCT *)l;
        switch (w) {
        case WM_LBUTTONDOWN: PostMessageW(g_main, WM_PICKMSG, 1, 0); return 1;
        case WM_LBUTTONUP:   PostMessageW(g_main, WM_PICKMSG, 2, 0); return 1;
        case WM_MOUSEWHEEL:  PostMessageW(g_main, WM_PICKMSG, 3, (LPARAM)(short)HIWORD(ms->mouseData)); return 1;
        case WM_RBUTTONDOWN: PostMessageW(g_main, WM_PICKMSG, 4, 0); return 1;
        case WM_RBUTTONUP: case WM_MBUTTONDOWN: case WM_MBUTTONUP: return 1;
        }
    }
    return CallNextHookEx(NULL, code, w, l);
}

void pick_poll(void)
{
    POINT pt;
    if (g_drag) { pick_mouse_move(); return; }
    GetCursorPos(&pt);
    if (pt.x == g_pollPt.x && pt.y == g_pollPt.y && g_pickLevel == g_pollLvl) return;
    g_pollPt = pt;
    pick_move();
    g_pollLvl = g_pickLevel;
}

void pick_end(void)
{
    if (!g_picking) return;
    g_picking = FALSE;
    KillTimer(g_main, TIMER_PICK);
    ShowWindow(g_hl, SW_HIDE);
    if (g_mag) ShowWindow(g_mag, SW_HIDE);
    g_candN = 0; g_pickLevel = 0; g_pickHave = FALSE; g_drag = g_dragMoved = FALSE;
    g_pickPt.x = g_pickPt.y = -1;
    if (g_mhook) { UnhookWindowsHookEx(g_mhook); g_mhook = NULL; }
    UnregisterHotKey(g_main, HK_ESC);
    ui_repaint();
    update_title();
}

void pick_start(void)
{
    if (g_rec || g_picking || g_full) return;
    if (g_uiHidden) { g_uiHidden = FALSE; ui_apply(); }
    uia_init();
    g_picking = TRUE;
    g_candN = 0; g_pickLevel = 0; g_pickHave = FALSE; g_drag = g_dragMoved = FALSE;
    g_pickPt.x = g_pickPt.y = -1; g_pollPt.x = g_pollPt.y = -1; g_pollLvl = -1;
    g_mhook = SetWindowsHookExW(WH_MOUSE_LL, MouseHook, GetModuleHandleW(NULL), 0);
    if (!g_mhook) {
        g_picking = FALSE;
        balloon(T(S_PICK_T), T(S_PICK_HOOK));
        return;
    }
    RegisterHotKey(g_main, HK_ESC, 0, VK_ESCAPE);
    SetTimer(g_main, TIMER_PICK, 30, NULL);
    ui_repaint();
    update_title();
    pick_poll();
}

void pick_commit(const RECT *b)
{
    MONITORINFO mi; int top = b->top - FRAME_TOP;
    mi.cbSize = sizeof mi;
    if (GetMonitorInfoW(MonitorFromRect(b, MONITOR_DEFAULTTONEAREST), &mi) && top < mi.rcMonitor.top)
        top = mi.rcMonitor.top;
    SetWindowPos(g_frame, HWND_TOPMOST, b->left - BORDER, top,
                 (b->right - b->left) + 2 * BORDER, (b->bottom + BORDER) - top, SWP_NOACTIVATE);
}

static void pick_apply(void)
{
    RECT b; BOOL ok;
    pick_move();
    ok = g_pickHave; b = g_pickRc;
    pick_end();
    if (ok) pick_commit(&b);
    else balloon(T(S_PICK_T), T(S_PICK_NONE));
}

void pick_mouse_down(void)
{
    GetCursorPos(&g_dragA);
    g_drag = TRUE; g_dragMoved = FALSE;
}

void pick_mouse_move(void)
{
    POINT c; RECT d;
    if (!g_drag) return;
    GetCursorPos(&c);
    mag_update(c);
    if (!g_dragMoved && (abs(c.x - g_dragA.x) > S(5) || abs(c.y - g_dragA.y) > S(5))) g_dragMoved = TRUE;
    if (!g_dragMoved) return;
    d.left = imin(c.x, g_dragA.x); d.right = imax(c.x, g_dragA.x);
    d.top = imin(c.y, g_dragA.y);  d.bottom = imax(c.y, g_dragA.y);
    pick_status(&d);
    pick_highlight(&d);
}

void pick_mouse_up(void)
{
    POINT c; RECT d;
    if (!g_drag) return;
    g_drag = FALSE;
    if (!g_dragMoved) { pick_apply(); return; }
    GetCursorPos(&c);
    d.left = imin(c.x, g_dragA.x); d.right = imax(c.x, g_dragA.x);
    d.top = imin(c.y, g_dragA.y);  d.bottom = imax(c.y, g_dragA.y);
    pick_end();
    if (d.right - d.left >= S(16) && d.bottom - d.top >= S(16)) pick_commit(&d);
}

void pick_wheel(int delta)
{
    if (delta > 0) g_pickLevel++;
    else if (g_pickLevel > 0) g_pickLevel--;
}
