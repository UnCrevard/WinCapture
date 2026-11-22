/*
 * encoder.c - Encodage Media Foundation (H.264 / HEVC, AAC) + capture audio
 *             système (WASAPI loopback) + microphone optionnel + rééchantillonnage
 */
#include "wincapture.h"

static const GUID MY_CLSID_MMDeviceEnumerator = { 0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
static const GUID MY_IID_IMMDeviceEnumerator  = { 0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };
static const GUID MY_IID_IAudioClient         = { 0x1CB9AD4C, 0xDBFA, 0x4C32, { 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2 } };
static const GUID MY_IID_IAudioCaptureClient  = { 0xC8ADBD64, 0xE71E, 0x48A0, { 0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17 } };
static const GUID MY_MFVideoFormat_HEVC       = { 0x43564548, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };

REC rc;
static WCHAR g_recPath[MAX_PATH];

UINT64 raw100(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (UINT64)(t.QuadPart - rc.t0.QuadPart) * 10000000ULL / rc.freq.QuadPart;
}

UINT64 now100(void)
{
    UINT64 r = raw100();
    return rc.paused ? rc.pausedAt - rc.pausedSum : r - rc.pausedSum;
}

static void write_pcm(const int16_t *pcm, UINT32 frames)
{
    DWORD bytes = frames * (DWORD)rc.ach * 2u;
    IMFMediaBuffer *b = NULL; IMFSample *s = NULL; BYTE *d = NULL;
    if (!frames) return;
    if (SUCCEEDED(MFCreateMemoryBuffer(bytes, &b)) && SUCCEEDED(IMFMediaBuffer_Lock(b, &d, NULL, NULL))) {
        if (pcm) memcpy(d, pcm, bytes); else memset(d, 0, bytes);
        IMFMediaBuffer_Unlock(b);
        IMFMediaBuffer_SetCurrentLength(b, bytes);
        if (SUCCEEDED(MFCreateSample(&s))) {
            IMFSample_AddBuffer(s, b);
            IMFSample_SetSampleTime(s, (LONGLONG)(rc.aframes * 10000000ULL / (UINT64)rc.arate));
            IMFSample_SetSampleDuration(s, (LONGLONG)((UINT64)frames * 10000000ULL / (UINT64)rc.arate));
            IMFSinkWriter_WriteSample(rc.sw, rc.aidx, s);
        }
    }
    REL(s); REL(b);
    rc.aframes += frames;
}

static float read_sample(const BYTE *data, UINT32 idx, int inCh, int ch, BOOL isFloat, BOOL silent)
{
    int c;
    if (silent) return 0.f;
    c = ch < inCh ? ch : 0;   /* mono -> dupliqué sur droite ; multicanal -> 2 premiers canaux */
    return isFloat ? ((const float *)data)[idx * inCh + c]
                   : ((const int16_t *)data)[idx * inCh + c] / 32768.0f;
}

/* Convertit et rééchantillonne un paquet WASAPI vers float entrelacé (outCh, outRate) */
static UINT32 convert_resample(const BYTE *data, UINT32 inFr, int inCh, int inRate, BOOL isFloat,
                               BOOL silent, float *out, UINT32 outCap, int outCh, int outRate, double *pos)
{
    UINT32 outFr = 0;
    double step;
    if (!inFr || !outCap) return 0;
    if (inRate == outRate) {
        UINT32 n = inFr < outCap ? inFr : outCap;
        for (UINT32 i = 0; i < n; i++)
            for (int c = 0; c < outCh; c++)
                out[i * outCh + c] = read_sample(data, i, inCh, c, isFloat, silent);
        return n;
    }
    step = (double)inRate / (double)outRate;
    while (*pos < (double)inFr && outFr < outCap) {
        UINT32 i0 = (UINT32)(*pos);
        UINT32 i1 = i0 + 1 < inFr ? i0 + 1 : i0;
        float frac = (float)(*pos - (double)i0);
        for (int c = 0; c < outCh; c++) {
            float a = read_sample(data, i0, inCh, c, isFloat, silent);
            float b = read_sample(data, i1, inCh, c, isFloat, silent);
            out[outFr * outCh + c] = a + (b - a) * frac;
        }
        outFr++;
        *pos += step;
    }
    *pos -= (double)inFr;
    if (*pos < 0.0) *pos = 0.0;
    return outFr;
}

DWORD WINAPI audio_thread(LPVOID unused)
{
    float *sysF = NULL, *micF = NULL, *micRing = NULL;
    int16_t *pcm = NULL;
    UINT32 tmpCap = 0, pcmCap = 0;
    UINT32 ringCap = (UINT32)rc.arate, ringHead = 0, ringTail = 0, ringLen = 0;
    double sysPos = 0.0, micPos = 0.0;
    (void)unused;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (rc.micCap) {
        micRing = (float *)calloc((size_t)ringCap * rc.ach, sizeof(float));
        IAudioClient_Start(rc.micAc);
    }
    if (rc.cap) IAudioClient_Start(rc.ac);

    do {
        UINT32 n = 0;
        /* 1) Drainer le microphone dans l'anneau float (ou directement en sortie si pas de loopback) */
        if (rc.micCap) {
            while (SUCCEEDED(IAudioCaptureClient_GetNextPacketSize(rc.micCap, &n)) && n) {
                BYTE *data; UINT32 fr, outFr; DWORD fl;
                if (FAILED(IAudioCaptureClient_GetBuffer(rc.micCap, &data, &fr, &fl, NULL, NULL))) break;
                UINT32 needOut = (UINT32)((UINT64)fr * (UINT64)rc.arate / (UINT64)rc.micRate) + 16;
                if (needOut > tmpCap) {
                    free(sysF); free(micF);
                    tmpCap = needOut;
                    sysF = (float *)malloc((size_t)tmpCap * rc.ach * sizeof(float));
                    micF = (float *)malloc((size_t)tmpCap * rc.ach * sizeof(float));
                }
                if (micF && !rc.paused && now100() >= rc.muteUntil) {
                    outFr = convert_resample(data, fr, rc.micCh, rc.micRate, rc.micFloat,
                                             (fl & AUDCLNT_BUFFERFLAGS_SILENT) != 0,
                                             micF, tmpCap, rc.ach, rc.arate, &micPos);
                    if (rc.cap && micRing) {
                        for (UINT32 i = 0; i < outFr; i++) {
                            if (ringLen == ringCap) {
                                ringHead = (ringHead + 1) % ringCap;
                                ringLen--;
                            }
                            for (int c = 0; c < rc.ach; c++)
                                micRing[ringTail * rc.ach + c] = micF[i * rc.ach + c];
                            ringTail = (ringTail + 1) % ringCap;
                            ringLen++;
                        }
                    } else if (outFr) {
                        if (outFr > pcmCap) { free(pcm); pcmCap = outFr; pcm = (int16_t *)malloc((size_t)pcmCap * rc.ach * 2); }
                        if (pcm) {
                            for (UINT32 i = 0; i < outFr * (UINT32)rc.ach; i++) {
                                float v = micF[i];
                                if (v > 1.f) v = 1.f; else if (v < -1.f) v = -1.f;
                                pcm[i] = (int16_t)(v * 32767.0f);
                            }
                            write_pcm(pcm, outFr);
                        }
                    }
                }
                IAudioCaptureClient_ReleaseBuffer(rc.micCap, fr);
            }
        }

        /* 2) Drainer l'audio système (loopback) + mixer avec le micro */
        if (rc.cap) {
            while (SUCCEEDED(IAudioCaptureClient_GetNextPacketSize(rc.cap, &n)) && n) {
                BYTE *data; UINT32 fr, outFr; DWORD fl;
                if (FAILED(IAudioCaptureClient_GetBuffer(rc.cap, &data, &fr, &fl, NULL, NULL))) break;
                UINT32 needOut = (UINT32)((UINT64)fr * (UINT64)rc.arate / (UINT64)rc.sysRate) + 16;
                if (needOut > tmpCap) {
                    free(sysF); free(micF);
                    tmpCap = needOut;
                    sysF = (float *)malloc((size_t)tmpCap * rc.ach * sizeof(float));
                    micF = (float *)malloc((size_t)tmpCap * rc.ach * sizeof(float));
                }
                if (sysF && !rc.paused && now100() >= rc.muteUntil) {
                    outFr = convert_resample(data, fr, rc.sysCh, rc.sysRate, rc.sysFloat,
                                             (fl & AUDCLNT_BUFFERFLAGS_SILENT) != 0,
                                             sysF, tmpCap, rc.ach, rc.arate, &sysPos);
                    if (outFr > pcmCap) { free(pcm); pcmCap = outFr; pcm = (int16_t *)malloc((size_t)pcmCap * rc.ach * 2); }
                    if (pcm && outFr) {
                        for (UINT32 i = 0; i < outFr; i++) {
                            BOOL haveMic = micRing && ringLen > 0;
                            for (int c = 0; c < rc.ach; c++) {
                                float v = sysF[i * rc.ach + c];
                                if (haveMic) v += micRing[ringHead * rc.ach + c];
                                if (v > 1.f) v = 1.f; else if (v < -1.f) v = -1.f;
                                pcm[i * rc.ach + c] = (int16_t)(v * 32767.0f);
                            }
                            if (haveMic) { ringHead = (ringHead + 1) % ringCap; ringLen--; }
                        }
                        write_pcm(pcm, outFr);
                    }
                }
                IAudioCaptureClient_ReleaseBuffer(rc.cap, fr);
            }
        }

        /* 3) Combler les périodes de silence WASAPI (ou utiliser le micro en attente) */
        for (;;) {
            UINT64 expected = now100() * (UINT64)rc.arate / 10000000ULL;
            UINT32 chunk;
            if (expected <= rc.aframes + (UINT64)(rc.arate / 10)) break;
            chunk = (UINT32)(expected - rc.aframes);
            if (chunk > 4800) chunk = 4800;
            if (micRing && ringLen > 0) {
                if (chunk > pcmCap) { free(pcm); pcmCap = chunk; pcm = (int16_t *)malloc((size_t)pcmCap * rc.ach * 2); }
                if (pcm) {
                    for (UINT32 i = 0; i < chunk; i++) {
                        BOOL haveMic = ringLen > 0;
                        for (int c = 0; c < rc.ach; c++) {
                            float v = haveMic ? micRing[ringHead * rc.ach + c] : 0.f;
                            if (v > 1.f) v = 1.f; else if (v < -1.f) v = -1.f;
                            pcm[i * rc.ach + c] = (int16_t)(v * 32767.0f);
                        }
                        if (haveMic) { ringHead = (ringHead + 1) % ringCap; ringLen--; }
                    }
                    write_pcm(pcm, chunk);
                } else {
                    write_pcm(NULL, chunk);
                }
            } else {
                write_pcm(NULL, chunk);
            }
        }
    } while (WaitForSingleObject(rc.stopEv, 10) == WAIT_TIMEOUT);

    if (rc.ac)    IAudioClient_Stop(rc.ac);
    if (rc.micAc) IAudioClient_Stop(rc.micAc);
    free(sysF); free(micF); free(micRing); free(pcm);
    CoUninitialize();
    return 0;
}

void audio_close(void)
{
    REL(rc.cap); REL(rc.ac);
    if (rc.wfx) { CoTaskMemFree(rc.wfx); rc.wfx = NULL; }
    REL(rc.micCap); REL(rc.micAc);
    if (rc.micWfx) { CoTaskMemFree(rc.micWfx); rc.micWfx = NULL; }
}

static BOOL open_endpoint(IMMDeviceEnumerator *en, EDataFlow flow, DWORD flags,
                          IAudioClient **pac, IAudioCaptureClient **pcap,
                          WAVEFORMATEX **pwfx, int *rate, int *ch, BOOL *isFloat)
{
    IMMDevice *dev = NULL; BOOL ok = FALSE;
    if (SUCCEEDED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(en, flow, eConsole, &dev)) &&
        SUCCEEDED(IMMDevice_Activate(dev, &MY_IID_IAudioClient, CLSCTX_ALL, NULL, (void **)pac)) &&
        SUCCEEDED(IAudioClient_GetMixFormat(*pac, pwfx)) &&
        ((*pwfx)->wBitsPerSample == 32 || (*pwfx)->wBitsPerSample == 16) &&
        SUCCEEDED(IAudioClient_Initialize(*pac, AUDCLNT_SHAREMODE_SHARED, flags, 10000000, 0, *pwfx, NULL)) &&
        SUCCEEDED(IAudioClient_GetService(*pac, &MY_IID_IAudioCaptureClient, (void **)pcap))) {
        *rate = (int)(*pwfx)->nSamplesPerSec;
        *ch = (*pwfx)->nChannels > 2 ? 2 : (*pwfx)->nChannels;
        *isFloat = (*pwfx)->wBitsPerSample == 32;
        ok = *rate >= 8000 && *ch >= 1;
    }
    REL(dev);
    if (!ok) {
        REL(*pcap); REL(*pac);
        if (*pwfx) { CoTaskMemFree(*pwfx); *pwfx = NULL; }
    }
    return ok;
}

BOOL audio_open(void)
{
    IMMDeviceEnumerator *en = NULL;
    BOOL sysOk = FALSE, micOk = FALSE;
    if (FAILED(CoCreateInstance(&MY_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                                &MY_IID_IMMDeviceEnumerator, (void **)&en))) return FALSE;
    if (g_cfg.audio)
        sysOk = open_endpoint(en, eRender, AUDCLNT_STREAMFLAGS_LOOPBACK,
                              &rc.ac, &rc.cap, &rc.wfx, &rc.sysRate, &rc.sysCh, &rc.sysFloat);
    if (g_cfg.mic)
        micOk = open_endpoint(en, eCapture, 0,
                              &rc.micAc, &rc.micCap, &rc.micWfx, &rc.micRate, &rc.micCh, &rc.micFloat);
    REL(en);
    if (!sysOk && !micOk) { audio_close(); return FALSE; }
    {
        int baseRate = sysOk ? rc.sysRate : rc.micRate;
        int baseCh   = sysOk ? rc.sysCh   : rc.micCh;
        /* Media Foundation AAC n'accepte que 44.1 ou 48 kHz : tout autre taux est rééchantillonné en 48 kHz */
        rc.arate = (baseRate == 44100 || baseRate == 48000) ? baseRate : 48000;
        rc.ach   = baseCh;
        if (sysOk && micOk && rc.micCh > rc.ach) rc.ach = 2;
    }
    return TRUE;
}

IMFMediaType *vtype(const GUID *sub, int w, int h, int fps, int kbps)
{
    IMFMediaType *t = NULL;
    if (FAILED(MFCreateMediaType(&t))) return NULL;
    IMFMediaType_SetGUID(t, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    IMFMediaType_SetGUID(t, &MF_MT_SUBTYPE, sub);
    if (kbps) IMFMediaType_SetUINT32(t, &MF_MT_AVG_BITRATE, kbps * 1000);
    IMFMediaType_SetUINT32(t, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    IMFMediaType_SetUINT64(t, &MF_MT_FRAME_SIZE, PACK(w, h));
    IMFMediaType_SetUINT64(t, &MF_MT_FRAME_RATE, PACK(fps, 1));
    IMFMediaType_SetUINT64(t, &MF_MT_PIXEL_ASPECT_RATIO, PACK(1, 1));
    return t;
}

static IMFMediaType *atype(BOOL aac)
{
    IMFMediaType *t = NULL;
    int bps = aac ? g_cfg.akbps * 125 : rc.arate * rc.ach * 2;
    if (aac && rc.ach == 1 && bps > 12000) bps = 12000;
    if (FAILED(MFCreateMediaType(&t))) return NULL;
    IMFMediaType_SetGUID(t, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(t, &MF_MT_SUBTYPE, aac ? &MFAudioFormat_AAC : &MFAudioFormat_PCM);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_SAMPLES_PER_SECOND, rc.arate);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, rc.ach);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, bps);
    if (aac) {
        IMFMediaType_SetUINT32(t, &MF_MT_AAC_PAYLOAD_TYPE, 0);
        IMFMediaType_SetUINT32(t, &MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29);
    } else {
        IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_BLOCK_ALIGNMENT, rc.ach * 2);
    }
    return t;
}

/* ICodecAPI : déclarée à la main (en-tête variable selon les SDK) */
typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void *, REFIID, void **);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void *);
    ULONG   (STDMETHODCALLTYPE *Release)(void *);
    HRESULT (STDMETHODCALLTYPE *IsSupported)(void *, const GUID *);
    HRESULT (STDMETHODCALLTYPE *IsModifiable)(void *, const GUID *);
    HRESULT (STDMETHODCALLTYPE *GetParameterRange)(void *, const GUID *, VARIANT *, VARIANT *, VARIANT *);
    HRESULT (STDMETHODCALLTYPE *GetParameterValues)(void *, const GUID *, VARIANT **, ULONG *);
    HRESULT (STDMETHODCALLTYPE *GetDefaultValue)(void *, const GUID *, VARIANT *);
    HRESULT (STDMETHODCALLTYPE *GetValue)(void *, const GUID *, VARIANT *);
    HRESULT (STDMETHODCALLTYPE *SetValue)(void *, const GUID *, VARIANT *);
} CAPIVTBL;

static const GUID MY_IID_ICodecAPI    = { 0x901DB4C7, 0x31CE, 0x41A2, { 0x85, 0xDC, 0x8F, 0xA0, 0xBF, 0x41, 0xB8, 0xDA } };
static const GUID MY_CAPI_RATECONTROL = { 0x1C0608E9, 0x370C, 0x4710, { 0x8A, 0x58, 0xCB, 0x61, 0x81, 0xC4, 0x24, 0x23 } };
static const GUID MY_CAPI_QUALITY     = { 0xFCBF57A3, 0x7EA5, 0x4B0C, { 0x96, 0x44, 0x69, 0xB4, 0x0C, 0x39, 0xC3, 0x91 } };
static const GUID MY_CAPI_ENCODEQP    = { 0x2CB5696B, 0x23FB, 0x4CE1, { 0xA0, 0xF9, 0xEF, 0x5B, 0x90, 0xFD, 0x55, 0xCA } };
static const GUID MY_CAPI_FRAMETYPEQP = { 0xAA70B610, 0xE03F, 0x450C, { 0xAD, 0x07, 0x07, 0x31, 0x4E, 0x63, 0x9C, 0xE7 } };
static const GUID MY_CAPI_MINQP       = { 0x0EE22C6A, 0xA37C, 0x4568, { 0xB5, 0xF1, 0x9D, 0x4C, 0x2B, 0x3A, 0xB8, 0x86 } };
static const GUID MY_CAPI_MAXQP       = { 0x3DAF6F66, 0xA6A7, 0x45E0, { 0xA8, 0xE5, 0xF2, 0x74, 0x3F, 0x46, 0xA3, 0xA2 } };

static BOOL capi_u4(void *ca, const GUID *key, ULONG val)
{
    VARIANT v; CAPIVTBL *vt = *(CAPIVTBL **)ca;
    VariantInit(&v); v.vt = VT_UI4; v.ulVal = val;
    return vt->SetValue(ca, key, &v) == S_OK;
}

static BOOL capi_u8(void *ca, const GUID *key, ULONGLONG val)
{
    VARIANT v; CAPIVTBL *vt = *(CAPIVTBL **)ca;
    VariantInit(&v); v.vt = VT_UI8; v.ullVal = val;
    return vt->SetValue(ca, key, &v) == S_OK;
}

static void apply_qp(void)
{
    void *ca = NULL; CAPIVTBL *vt; BOOL ok = FALSE;
    int qi = g_cfg.qpI, qp = g_cfg.qpP, qb = g_cfg.qpB, lo, hi, avg;
    if (g_cfg.rcmode != 1) return;
    if (qi < 0) qi = 0; if (qi > 51) qi = 51;
    if (qp < 0) qp = 0; if (qp > 51) qp = 51;
    if (qb < 0) qb = 0; if (qb > 51) qb = 51;
    lo = qi < qp ? qi : qp; if (qb < lo) lo = qb;
    hi = qi > qp ? qi : qp; if (qb > hi) hi = qb;
    avg = (qi + qp + qb) / 3;

    if (FAILED(IMFSinkWriter_GetServiceForStream(rc.sw, rc.vidx, &GUID_NULL, &MY_IID_ICodecAPI, &ca)) || !ca) {
        balloon(T(S_QP_UNSUP_T), T(S_QP_UNSUP_M));
        return;
    }
    vt = *(CAPIVTBL **)ca;
    capi_u4(ca, &MY_CAPI_RATECONTROL, 3);
    ok = capi_u8(ca, &MY_CAPI_ENCODEQP, (ULONGLONG)qi | ((ULONGLONG)qp << 16) | ((ULONGLONG)qb << 32));
    if (!ok)
        ok = capi_u8(ca, &MY_CAPI_FRAMETYPEQP, (ULONGLONG)qi | ((ULONGLONG)qp << 16) | ((ULONGLONG)qb << 32));
    if (!ok) {
        BOOL a = capi_u4(ca, &MY_CAPI_MINQP, (ULONG)lo);
        BOOL b = capi_u4(ca, &MY_CAPI_MAXQP, (ULONG)hi);
        ok = a || b;
    }
    if (!ok)
        ok = capi_u4(ca, &MY_CAPI_QUALITY, (ULONG)(100 - avg * 100 / 51));
    if (!ok) {
        capi_u4(ca, &MY_CAPI_RATECONTROL, 2);
        balloon(T(S_QP_UNSUP_T), T(S_QP_UNSUP_M));
    }
    vt->Release(ca);
}

/* Note : gpu_mgr est accédé via extern si nécessaire ? gpu_init est dans capture.c -> passons D3D manager via sink_open param ou getter */
static void *g_sink_d3dmgr = NULL;
void encoder_set_d3d_manager(void *mgr) { g_sink_d3dmgr = mgr; }

HRESULT sink_open(const WCHAR *path, int w, int h, BOOL gpuMode)
{
    IMFAttributes *at = NULL; IMFMediaType *t = NULL; HRESULT hr;
    MFCreateAttributes(&at, 3);
    IMFAttributes_SetUINT32(at, &MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1);
    IMFAttributes_SetUINT32(at, &MF_SINK_WRITER_DISABLE_THROTTLING, 1);
    if (gpuMode && g_sink_d3dmgr)
        IMFAttributes_SetUnknown(at, &MF_SINK_WRITER_D3D_MANAGER, (IUnknown *)g_sink_d3dmgr);
    hr = MFCreateSinkWriterFromURL(path, NULL, at, &rc.sw);
    REL(at);
    if (FAILED(hr)) return hr;

    t = vtype(g_cfg.vcodec ? &MY_MFVideoFormat_HEVC : &MFVideoFormat_H264, w, h, rc.fps, g_cfg.vkbps);
    hr = t ? IMFSinkWriter_AddStream(rc.sw, t, &rc.vidx) : E_FAIL;
    REL(t);
    if (FAILED(hr)) return hr;

    t = vtype(gpuMode ? &MFVideoFormat_NV12 : &MFVideoFormat_RGB32, w, h, rc.fps, 0);
    if (t && gpuMode) {
        IMFMediaType_SetUINT32(t, &MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);
        IMFMediaType_SetUINT32(t, &MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_16_235);
    }
    hr = t ? IMFSinkWriter_SetInputMediaType(rc.sw, rc.vidx, t, NULL) : E_FAIL;
    REL(t);
    if (SUCCEEDED(hr)) apply_qp();
    return hr;
}

void rec_start(void)
{
    RECT r; IMFMediaType *t = NULL; HRESULT hr; int w, h, w0, h0; BOOL useGpu = FALSE;

    if (g_rec) return;
    get_region(&r);
    w0 = w = (r.right - r.left) & ~1; h0 = h = (r.bottom - r.top) & ~1;
    if (w < 128 || h < 72) {
        MessageBoxW(g_main, T(S_TOO_SMALL), T(S_APP), MB_ICONWARNING);
        return;
    }
    ZeroMemory(&rc, sizeof rc);
    rc.fps = g_cfg.fps; rc.cursor = g_cfg.cursor; rc.vfr = g_cfg.vfr; rc.clicks = g_cfg.clicks;

    if (g_cfg.gpu) {
        if (gpu_init(&w, &h, rc.fps)) useGpu = TRUE;
        else balloon(T(S_GPU_FB_T), T(S_GPU_FB_M));
    }
    rc.w = w; rc.h = h; rc.gpu = useGpu;

    if (g_cfg.audio || g_cfg.mic) {
        if (audio_open()) rc.hasAudio = TRUE;
        else balloon(T(S_AUDIO_T), T(S_AUDIO_M));
    }

    make_path(TRUE, L"mp4", g_recPath);

    hr = sink_open(g_recPath, w, h, useGpu);
    if (FAILED(hr) && useGpu) {
        REL(rc.sw);
        gpu_free();
        useGpu = FALSE;
        w = w0; h = h0;
        rc.w = w; rc.h = h; rc.gpu = FALSE;
        balloon(T(S_GPU_FB_T), T(S_GPU_FB_M));
        hr = sink_open(g_recPath, w, h, FALSE);
    }
    if (FAILED(hr)) goto fail;

    if (rc.hasAudio) {
        t = atype(TRUE);
        hr = t ? IMFSinkWriter_AddStream(rc.sw, t, &rc.aidx) : E_FAIL;
        REL(t);
        if (FAILED(hr)) goto fail;
        t = atype(FALSE);
        hr = t ? IMFSinkWriter_SetInputMediaType(rc.sw, rc.aidx, t, NULL) : E_FAIL;
        REL(t);
        if (FAILED(hr)) goto fail;
    }

    hr = IMFSinkWriter_BeginWriting(rc.sw);
    if (FAILED(hr)) goto fail;

    QueryPerformanceFrequency(&rc.freq);
    QueryPerformanceCounter(&rc.t0);
    rc.muteUntil = sound_len100(g_cfg.soundRec, g_soundRecPath);
    play_rec_sound();
    rc.stopEv = CreateEventW(NULL, TRUE, FALSE, NULL);
    rc.vth = CreateThread(NULL, 0, rc.gpu ? video_thread_gpu : video_thread, NULL, 0, NULL);
    if (rc.hasAudio) rc.ath = CreateThread(NULL, 0, audio_thread, NULL, 0, NULL);
    g_rec = TRUE;
    ui_update();
    return;

fail:
    REL(rc.sw);
    gpu_free();
    audio_close();
    DeleteFileW(g_recPath);
    MessageBoxW(g_main, T(S_ENC_FAIL), T(S_APP), MB_ICONERROR);
}

void rec_stop(void)
{
    HANDLE h[2]; int n = 0;
    if (!g_rec) return;
    SetEvent(rc.stopEv);
    if (rc.vth) h[n++] = rc.vth;
    if (rc.ath) h[n++] = rc.ath;
    if (n > 0) WaitForMultipleObjects(n, h, TRUE, 10000);
    for (int i = 0; i < n; i++) CloseHandle(h[i]);
    play_rec_sound();
    IMFSinkWriter_Finalize(rc.sw);
    REL(rc.sw);
    gpu_free();
    audio_close();
    CloseHandle(rc.stopEv);
    g_rec = FALSE;
    wcsncpy_s(g_lastFile, ARRAYSIZE(g_lastFile), g_recPath, _TRUNCATE);
    if (g_uiHidden || g_full) balloon(T(S_SAVED_T), PathFindFileNameW(g_recPath));
    ui_update();
}

void rec_pause_toggle(void)
{
    if (!g_rec) return;
    if (!rc.paused) { rc.pausedAt = raw100(); rc.paused = TRUE; }
    else            { rc.pausedSum += raw100() - rc.pausedAt; rc.paused = FALSE; }
    ui_repaint();
    InvalidateRect(GetDlgItem(g_main, ID_PAUSE), NULL, TRUE);
    update_title();
}

void toggle_rec(void)
{
    if (g_rec) rec_stop();
    else rec_start();
}
