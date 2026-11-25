/*
 * wincapture.h - Déclarations partagées de WinCapture (Win32, Unicode, C)
 */
#ifndef WINCAPTURE_H
#define WINCAPTURE_H

#define UNICODE
#define _UNICODE
#define COBJMACROS
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifdef __MINGW32__
#define MINGW_HAS_SECURE_API 1      /* swprintf_s, wcsncpy_s, ... */
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <audioclient.h>
#include <wincodec.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dwmapi.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <commctrl.h>
#include <uiautomation.h>
#include "resource.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef WDA_NONE
#define WDA_NONE 0x00000000
#endif
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#ifndef NIN_BALLOONUSERCLICK
#define NIN_BALLOONUSERCLICK (WM_USER + 5)
#endif
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#define REL(p) do { if (p) { IUnknown_Release((IUnknown *)(p)); (p) = NULL; } } while (0)
#define PACK(a, b) ((((UINT64)(UINT32)(a)) << 32) | (UINT32)(b))

extern int g_dpi;
#define S(x)       MulDiv((x), g_dpi, 96)
#define BORDER     S(5)      /* épaisseur du cadre */
#define BAR_H      S(26)     /* hauteur de la barre (fenêtre indépendante du cadre) */
#define FRAME_TOP  S(10)     /* épaisseur du bord haut du cadre : poignée de déplacement */
#define BTN_H      S(20)

#define WM_TRAY    (WM_APP + 1)
#define WM_PICKMSG (WM_APP + 2)   /* évènements souris interceptés pendant le ciblage */
#define WM_RELANG  (WM_APP + 3)   /* langue changée : recréer les fenêtres auxiliaires */

enum {
    ID_SHOT = 1, ID_REC, ID_SIZE, ID_PICK, ID_CFG, ID_CLOSE, ID_BARE, ID_PAUSE,
    IDM_SHOT = 200, IDM_REC, IDM_SHOWHIDE, IDM_CFG, IDM_PICK, IDM_BARE, IDM_FULL,
    IDM_OPENDIR, IDM_OPENLAST, IDM_COPYLAST, IDM_QUIT,
    IDM_SIZE_ADD = 490, IDM_SIZE_EDIT,
    IDM_SIZE0 = 500,
    ID_COMBO0 = 100, ID_CHK_AUDIO = 150, ID_CHK_CURSOR = 151, ID_CHK_VFR = 152, ID_CHK_GPU = 153,
    ID_CHK_MIC = 154, ID_CHK_CLIP = 155, ID_CHK_EXCL = 156, ID_CHK_CLICKS = 157,
    ID_BTN_DIR = 160, ID_TXT_DIR,
    ID_HK_SHOT = 170, ID_HK_REC, ID_HK_FULL, ID_HK_PAUSE,
    ID_COL_FRAME = 180, ID_COL_TITLE, ID_EDT_NAME = 185,
    ID_TRK_TRANSP = 190, ID_TXT_TRANSP, ID_CMB_LANG = 195, ID_CMB_DELAY = 196,
    ID_CMB_RC = 320, ID_EDT_QPI, ID_EDT_QPP, ID_EDT_QPB, ID_UD_QPI, ID_UD_QPP, ID_UD_QPB, ID_CMB_RSND,
    ID_CMB_CONTAINER,
    ID_SZ_LIST = 300, ID_SZ_NAME, ID_SZ_W, ID_SZ_H, ID_SZ_CUR, ID_SZ_ADD, ID_SZ_SET,
    ID_SZ_DEL, ID_SZ_UP, ID_SZ_DOWN, ID_SZ_APPLY, ID_SZ_RESET,
    ID_SZ_X, ID_SZ_Y, ID_SZ_USEPOS,
    HK_SHOT = 1, HK_REC = 2, HK_ESC = 3, HK_FULL = 4, HK_PAUSE = 5,
    TIMER_UI = 1, TIMER_PICK = 2, TIMER_HOVER = 3, TIMER_COUNTDOWN = 4
};

typedef struct {
    int vcodec;   /* 0 = H.264, 1 = HEVC */
    int vkbps;    /* débit vidéo en kb/s */
    int fps;
    int audio;    /* 1 = enregistrer l'audio système */
    int akbps;    /* débit AAC en kb/s : 96 / 128 / 160 / 192 */
    int imgfmt;   /* 0 = PNG, 1 = JPEG, 2 = BMP */
    int jpgq;     /* qualité JPEG 0..100 */
    int cursor;   /* dessiner le curseur */
    int sound;    /* son de capture : 0 défaut Windows, 1 obturateur, 2 fichier .wav, 3 aucun */
    int vfr;      /* 1 = débit d'images variable : ignore les images identiques */
    int hkShot, hkRec;   /* raccourcis globaux : MAKEWORD(touche virtuelle, HOTKEYF_*), 0 = aucun */
    int colFrame, colTitle;   /* couleurs (COLORREF) du cadre et de la barre de titre */
    int transp;   /* transparence de l'interface en % (0 = opaque, max 80) */
    int lang;     /* langue : 0 English, 1 Français, 2 Español, 3 Deutsch ; -1 = à détecter au 1er démarrage */
    int gpu;      /* 1 = capture GPU (DXGI + encodeur matériel) quand c'est possible */
    int rcmode;   /* contrôle du débit : 0 = débit cible, 1 = qualité constante (QP) */
    int qpI, qpP, qpB;   /* QP (0-51) des images I, P, B ; plus bas = meilleure qualité */
    int hkFull;   /* raccourci global « plein écran » */
    int soundRec; /* son de début / fin d'enregistrement : mêmes choix que `sound` */
    int mic;      /* 1 = enregistrer aussi le microphone */
    int clip;     /* 1 = copier la capture d'image dans le presse-papiers */
    int excl;     /* 1 = exclure l'interface des captures (WDA_EXCLUDEFROMCAPTURE) */
    int delay;    /* retardateur image en secondes : 0 / 3 / 5 / 10 */
    int clicks;   /* 1 = halo visuel sur les clics souris en vidéo */
    int hkPause;  /* raccourci global « pause / reprendre » */
    int container; /* 0 = MP4, 1 = MPEG-TS */
} CFG;

#define MAXS 24
typedef struct { WCHAR name[32]; int w, h, x, y; BOOL hasPos; } SIZEP;

typedef struct { HDC dc; HBITMAP bmp; HGDIOBJ old; void *bits; int w, h; } GRAB;

#define MAX_CLICKS 8
#define CLICK_DUR100 3500000ULL  /* 350 ms */
typedef struct { int x, y, btn; UINT64 t0; } CLICKRING;

typedef struct {
    IMFSinkWriter *sw;
    DWORD vidx, aidx;
    BOOL hasAudio;
    int w, h, fps;
    BOOL cursor, vfr, gpu, clicks;
    HANDLE vth, ath, stopEv;
    LARGE_INTEGER freq, t0;
    volatile BOOL paused;
    UINT64 pausedAt, pausedSum;
    UINT64 muteUntil;
    /* audio : sortie système (loopback) + microphone optionnel */
    IAudioClient *ac;
    IAudioCaptureClient *cap;
    WAVEFORMATEX *wfx;
    int sysRate, sysCh;
    BOOL sysFloat;
    IAudioClient *micAc;
    IAudioCaptureClient *micCap;
    WAVEFORMATEX *micWfx;
    int micRate, micCh;
    BOOL micFloat;
    int arate, ach;
    UINT64 aframes;
    /* effets de clic */
    CLICKRING clickRings[MAX_CLICKS];
    int clickCount;
    BOOL prevL, prevR;
} REC;

#define STRINGS(X) \
    X(S_STATUS_READY, L"Capture — ready", L"Capture — prêt", L"Captura — listo", L"Aufnahme — bereit") \
    X(S_STATUS_REC, L"Capture — recording", L"Capture — enregistrement en cours", L"Captura — grabando", L"Aufnahme — läuft") \
    X(S_APP, L"Capture", L"Capture", L"Captura", L"Aufnahme") \
    X(S_SHOT_FAIL_T, L"Capture failed", L"Échec de la capture", L"Error de captura", L"Aufnahme fehlgeschlagen") \
    X(S_SHOT_FAIL_M, L"Unable to write the image.", L"Impossible d’écrire l’image.", L"No se pudo escribir la imagen.", L"Das Bild konnte nicht gespeichert werden.") \
    X(S_TOO_SMALL, L"The capture area is too small.", L"La zone de capture est trop petite.", L"El área de captura es demasiado pequeña.", L"Der Aufnahmebereich ist zu klein.") \
    X(S_AUDIO_T, L"Audio unavailable", L"Audio indisponible", L"Audio no disponible", L"Audio nicht verfügbar") \
    X(S_AUDIO_M, L"Video-only recording.", L"Enregistrement vidéo seul.", L"Grabación solo de vídeo.", L"Nur Videoaufzeichnung.") \
    X(S_ENC_FAIL, L"Unable to start encoding.\nSelected container/codec unavailable or audio initialization failed.", L"Impossible de démarrer l’encodage.\nConteneur/codec sélectionné indisponible ou échec d’initialisation audio.", L"No se puede iniciar la codificación.\nContenedor/códec no disponible o error de audio.", L"Die Kodierung kann nicht gestartet werden.\nAusgewählter Container/Codec nicht verfügbar oder Audiofehler.") \
    X(S_PICK_HINT, L"Click or drag · Wheel: level · Esc: cancel", L"Clic ou glisser · Molette : niveau · Échap : annuler", L"Clic o arrastrar · Rueda: nivel · Esc: cancelar", L"Klicken oder ziehen · Mausrad: Ebene · Esc: abbrechen") \
    X(S_PICK_STATUS, L"%d × %d · wheel: level · drag: free area", L"%d × %d · molette : niveau · glisser : zone libre", L"%d × %d · rueda: nivel · arrastrar: área libre", L"%d × %d · Mausrad: Ebene · Ziehen: freier Bereich") \
    X(S_PICK_T, L"Targeting", L"Ciblage", L"Selección", L"Zielauswahl") \
    X(S_PICK_HOOK, L"Unable to intercept the mouse.", L"Impossible d’intercepter la souris.", L"No se puede interceptar el ratón.", L"Die Maus kann nicht abgefangen werden.") \
    X(S_PICK_NONE, L"Nothing to target here: drag to draw a free area.", L"Rien de ciblable ici : glissez pour tracer une zone libre.", L"No hay nada que seleccionar aquí: arrastre para trazar un área libre.", L"Hier gibt es nichts auszuwählen: ziehen Sie, um einen freien Bereich zu zeichnen.") \
    X(S_SIZE_ADD, L"Add current size and position…", L"Ajouter la taille et la position actuelles…", L"Añadir tamaño y posición actuales…", L"Aktuelle Größe und Position hinzufügen…") \
    X(S_SIZE_EDIT, L"Edit presets…", L"Modifier les préréglages…", L"Editar ajustes preestablecidos…", L"Voreinstellungen bearbeiten…") \
    X(S_MOD_CTRL, L"Ctrl+", L"Ctrl+", L"Ctrl+", L"Strg+") \
    X(S_MOD_ALT, L"Alt+", L"Alt+", L"Alt+", L"Alt+") \
    X(S_MOD_SHIFT, L"Shift+", L"Maj+", L"Mayús+", L"Umschalt+") \
    X(S_HK_T, L"Shortcut unavailable", L"Raccourci indisponible", L"Atajo no disponible", L"Tastenkürzel nicht verfügbar") \
    X(S_HK_IMG, L"The “image” shortcut is already used by another application.", L"Le raccourci « image » est déjà utilisé par une autre application.", L"El atajo de «imagen» ya está en uso por otra aplicación.", L"Das Tastenkürzel „Bild“ wird bereits von einer anderen Anwendung verwendet.") \
    X(S_HK_VID, L"The “video” shortcut is already used by another application.", L"Le raccourci « vidéo » est déjà utilisé par une autre application.", L"El atajo de «vídeo» ya está en uso por otra aplicación.", L"Das Tastenkürzel „Video“ wird bereits von einer anderen Anwendung verwendet.") \
    X(S_MENU_SHOT, L"Screenshot\t%s", L"Capture d’écran\t%s", L"Captura de pantalla\t%s", L"Screenshot\t%s") \
    X(S_MENU_STOP, L"Stop recording\t%s", L"Arrêter l’enregistrement\t%s", L"Detener grabación\t%s", L"Aufnahme beenden\t%s") \
    X(S_MENU_START, L"Start recording\t%s", L"Démarrer l’enregistrement\t%s", L"Iniciar grabación\t%s", L"Aufnahme starten\t%s") \
    X(S_MENU_SIZE, L"Window size", L"Taille de la fenêtre", L"Tamaño de la ventana", L"Fenstergröße") \
    X(S_MENU_PICK, L"Target a window…", L"Cibler une fenêtre…", L"Seleccionar una ventana…", L"Fenster auswählen…") \
    X(S_MENU_FRAME_SHOW, L"Show frame", L"Afficher le cadre", L"Mostrar marco", L"Rahmen anzeigen") \
    X(S_MENU_FRAME_HIDE, L"Hide frame", L"Masquer le cadre", L"Ocultar marco", L"Rahmen ausblenden") \
    X(S_MENU_WIN_HIDE, L"Hide window", L"Masquer la fenêtre", L"Ocultar ventana", L"Fenster ausblenden") \
    X(S_MENU_WIN_SHOW, L"Show window", L"Afficher la fenêtre", L"Mostrar ventana", L"Fenster anzeigen") \
    X(S_MENU_SETTINGS, L"Encoding settings…", L"Réglages d’encodage…", L"Ajustes de codificación…", L"Kodierungseinstellungen…") \
    X(S_MENU_QUIT, L"Quit", L"Quitter", L"Salir", L"Beenden") \
    X(S_SZ_INVALID, L"Invalid values: minimum size 64 × 36, position must be whole numbers.", L"Valeurs invalides : taille minimale 64 × 36, position en nombres entiers.", L"Valores no válidos: tamaño mínimo 64 × 36, posición en números enteros.", L"Ungültige Werte: Mindestgröße 64 × 36, Position als ganze Zahlen.") \
    X(S_PRESETS, L"Presets", L"Préréglages", L"Ajustes preestablecidos", L"Voreinstellungen") \
    X(S_SZ_NAME, L"Name", L"Nom", L"Nombre", L"Name") \
    X(S_SZ_W, L"Width", L"Largeur", L"Ancho", L"Breite") \
    X(S_SZ_H, L"Height", L"Hauteur", L"Alto", L"Höhe") \
    X(S_SZ_X, L"Position X", L"Position X", L"Posición X", L"Position X") \
    X(S_SZ_Y, L"Position Y", L"Position Y", L"Posición Y", L"Position Y") \
    X(S_SZ_USEPOS, L"Remember position", L"Mémoriser la position", L"Recordar posición", L"Position merken") \
    X(S_SZ_CUR, L"Current size and position", L"Taille et position actuelles", L"Tamaño y posición actuales", L"Aktuelle Größe und Position") \
    X(S_SZ_ADD, L"Add", L"Ajouter", L"Añadir", L"Hinzufügen") \
    X(S_SZ_SET, L"Update", L"Modifier", L"Modificar", L"Ändern") \
    X(S_SZ_DEL, L"Delete", L"Supprimer", L"Eliminar", L"Löschen") \
    X(S_SZ_APPLY, L"Apply", L"Appliquer", L"Aplicar", L"Anwenden") \
    X(S_SZ_UP, L"Move up", L"Monter", L"Subir", L"Nach oben") \
    X(S_SZ_DOWN, L"Move down", L"Descendre", L"Bajar", L"Nach unten") \
    X(S_SZ_RESET, L"Restore default presets", L"Rétablir les préréglages par défaut", L"Restaurar los predeterminados", L"Standardwerte wiederherstellen") \
    X(S_SZ_MAX, L"Maximum number of presets reached.", L"Nombre maximum de préréglages atteint.", L"Se alcanzó el número máximo de ajustes.", L"Maximale Anzahl an Voreinstellungen erreicht.") \
    X(S_SZ_RESET_Q, L"Restore the default preset list?", L"Rétablir la liste des préréglages par défaut ?", L"¿Restaurar la lista de ajustes predeterminados?", L"Standardliste der Voreinstellungen wiederherstellen?") \
    X(S_SZ_TITLE, L"Size and position presets", L"Préréglages de taille et position", L"Ajustes de tamaño y posición", L"Voreinstellungen für Größe und Position") \
    X(S_TIP_SHOT, L"Take a screenshot", L"Capture d’image", L"Capturar imagen", L"Bildschirmfoto aufnehmen") \
    X(S_TIP_STOP, L"Stop recording", L"Arrêter l’enregistrement", L"Detener grabación", L"Aufnahme beenden") \
    X(S_TIP_REC, L"Record video", L"Enregistrer la vidéo", L"Grabar vídeo", L"Video aufnehmen") \
    X(S_TIP_SIZE, L"Sizes and presets", L"Tailles et préréglages", L"Tamaños y ajustes preestablecidos", L"Größen und Voreinstellungen") \
    X(S_TIP_PICK, L"Target a window or an area", L"Cibler une fenêtre ou une zone", L"Seleccionar una ventana o un área", L"Fenster oder Bereich auswählen") \
    X(S_TIP_CFG, L"Settings (encoding, sound, folders…)", L"Réglages (encodage, son, dossiers…)", L"Ajustes (codificación, sonido, carpetas…)", L"Einstellungen (Kodierung, Ton, Ordner…)") \
    X(S_TIP_HIDE, L"Hide (stays in the notification area)", L"Masquer (reste dans la zone de notification)", L"Ocultar (permanece en el área de notificación)", L"Ausblenden (bleibt im Infobereich)") \
    X(S_ROW_VCODEC, L"Video codec", L"Codec vidéo", L"Códec de vídeo", L"Videocodec") \
    X(S_ROW_CONTAINER, L"Recording format", L"Format d’enregistrement", L"Formato de grabación", L"Aufnahmeformat") \
    X(S_ROW_VBR, L"Video bitrate", L"Débit vidéo", L"Tasa de bits de vídeo", L"Video-Bitrate") \
    X(S_ROW_FPS, L"Frames per second", L"Images / seconde", L"Fotogramas por segundo", L"Bilder pro Sekunde") \
    X(S_ROW_ABR, L"Audio bitrate (AAC)", L"Débit audio (AAC)", L"Tasa de bits de audio (AAC)", L"Audio-Bitrate (AAC)") \
    X(S_ROW_IMGFMT, L"Image format", L"Format d’image", L"Formato de imagen", L"Bildformat") \
    X(S_ROW_JPGQ, L"JPEG quality", L"Qualité JPEG", L"Calidad JPEG", L"JPEG-Qualität") \
    X(S_ROW_SOUND, L"Image sound", L"Son d’image", L"Sonido de imagen", L"Bildton") \
    X(S_ROW_RSOUND, L"Video sound", L"Son de vidéo", L"Sonido de vídeo", L"Videoton") \
    X(S_SND_DEF, L"Windows default", L"Défaut Windows", L"Predeterminado de Windows", L"Windows-Standard") \
    X(S_SND_SHUT, L"Camera shutter", L"Obturateur photo", L"Obturador de cámara", L"Kameraverschluss") \
    X(S_SND_FILE, L"WAV file…", L"Fichier .wav…", L"Archivo .wav…", L"WAV-Datei…") \
    X(S_SND_NONE, L"None", L"Aucun", L"Ninguno", L"Kein Ton") \
    X(S_CHK_AUDIO, L"Record system audio", L"Enregistrer l’audio du système", L"Grabar el audio del sistema", L"Systemton aufnehmen") \
    X(S_CHK_CURSOR, L"Capture the mouse cursor", L"Capturer le curseur de la souris", L"Capturar el cursor del ratón", L"Mauszeiger aufnehmen") \
    X(S_CHK_VFR, L"Variable frame rate (skip duplicates)", L"Images variables (ignore les doublons)", L"Fotogramas variables (omite duplicados)", L"Variable Bildrate (Duplikate überspringen)") \
    X(S_HK_LBL_IMG, L"Image shortcut", L"Raccourci image", L"Atajo de imagen", L"Kürzel Bild") \
    X(S_HK_LBL_VID, L"Video shortcut", L"Raccourci vidéo", L"Atajo de vídeo", L"Kürzel Video") \
    X(S_COL_FRAME, L"Frame color", L"Couleur du cadre", L"Color del marco", L"Rahmenfarbe") \
    X(S_COL_BAR, L"Title bar color", L"Couleur de la barre", L"Color de la barra", L"Farbe der Titelleiste") \
    X(S_TRANSP, L"Interface transparency: %d %%", L"Transparence de l’interface : %d %%", L"Transparencia de la interfaz: %d %%", L"Transparenz der Oberfläche: %d %%") \
    X(S_NAME_LBL, L"File name (without extension)", L"Nom des fichiers (sans extension)", L"Nombre de archivo (sin extensión)", L"Dateiname (ohne Erweiterung)") \
    X(S_NAME_HINT, L"Variables: %Y %m %d %H %M %S (date and time), %f (milliseconds), %t (window title), %p (program). Prefix / suffix: type them as is, e.g. my_%t_%H%M%S%f", L"Variables : %Y %m %d %H %M %S (date et heure), %f (millisecondes), %t (titre de la fenêtre), %p (programme). Préfixe / suffixe : écrivez-les tels quels, ex. mon_%t_%H%M%S%f", L"Variables: %Y %m %d %H %M %S (fecha y hora), %f (milisegundos), %t (título de la ventana), %p (programa). Prefijo / sufijo: escríbalos tal cual, p. ej. mi_%t_%H%M%S%f", L"Variablen: %Y %m %d %H %M %S (Datum und Uhrzeit), %f (Millisekunden), %t (Fenstertitel), %p (Programm). Präfix / Suffix: einfach so eingeben, z. B. mein_%t_%H%M%S%f") \
    X(S_CHOOSE, L"Choose…", L"Choisir…", L"Elegir…", L"Wählen…") \
    X(S_PICK_SND, L"Choose the capture sound", L"Choisir le son de capture", L"Elegir el sonido de captura", L"Aufnahmeton auswählen") \
    X(S_PICK_DIR, L"Choose a folder", L"Choisir un dossier", L"Elegir una carpeta", L"Ordner auswählen") \
    X(S_DIR_T, L"Folder", L"Dossier", L"Carpeta", L"Ordner") \
    X(S_DIR_ERR, L"Unable to open the folder picker.", L"Impossible d’ouvrir le sélecteur de dossier.", L"No se puede abrir el selector de carpetas.", L"Der Ordnerauswahldialog kann nicht geöffnet werden.") \
    X(S_SETTINGS_T, L"Settings", L"Réglages", L"Ajustes", L"Einstellungen") \
    X(S_WAV_FILES, L"WAV files", L"Fichiers WAV", L"Archivos WAV", L"WAV-Dateien") \
    X(S_LANGUAGE, L"Language", L"Langue", L"Idioma", L"Sprache") \
    X(S_CHK_GPU, L"GPU capture (DirectX)", L"Capture GPU (DirectX)", L"Captura por GPU (DirectX)", L"GPU-Aufnahme (DirectX)") \
    X(S_GPU_HINT, L"Lowers CPU usage. Falls back to the classic method if the GPU path is unavailable.", L"Réduit l’utilisation du CPU. Bascule sur la méthode classique si le mode GPU est indisponible.", L"Reduce el uso de CPU. Vuelve al método clásico si el modo GPU no está disponible.", L"Senkt die CPU-Last. Wechselt zur klassischen Methode, wenn der GPU-Modus nicht verfügbar ist.") \
    X(S_GPU_FB_T, L"GPU capture", L"Capture GPU", L"Captura por GPU", L"GPU-Aufnahme") \
    X(S_GPU_FB_M, L"Unavailable here: using the classic method.", L"Indisponible ici : méthode classique utilisée.", L"No disponible aquí: se usa el método clásico.", L"Hier nicht verfügbar: klassische Methode wird verwendet.") \
    X(S_RC_LBL, L"Rate control", L"Contrôle du débit", L"Control de tasa", L"Ratensteuerung") \
    X(S_RC_BR, L"Target bitrate", L"Débit cible", L"Tasa de bits objetivo", L"Ziel-Bitrate") \
    X(S_RC_QP, L"Constant quality (QP)", L"Qualité constante (QP)", L"Calidad constante (QP)", L"Konstante Qualität (QP)") \
    X(S_QP_I, L"QP I frames (0–51)", L"QP images I (0–51)", L"QP fotogramas I (0–51)", L"QP I-Bilder (0–51)") \
    X(S_QP_P, L"QP P frames (0–51)", L"QP images P (0–51)", L"QP fotogramas P (0–51)", L"QP P-Bilder (0–51)") \
    X(S_QP_B, L"QP B frames (0–51)", L"QP images B (0–51)", L"QP fotogramas B (0–51)", L"QP B-Bilder (0–51)") \
    X(S_QP_HINT, L"Lower QP = better quality and bigger files. 0–10: near lossless; 15–20: very high; 21–26: good; 27+: visibly lossy. Not every hardware encoder supports every setting.", L"QP bas = meilleure qualité et fichiers plus gros. 0–10 : quasi sans perte ; 15–20 : très haute ; 21–26 : bonne ; 27+ : perte visible. Tous les encodeurs matériels ne gèrent pas tous les réglages.", L"QP bajo = mejor calidad y archivos más grandes. 0–10: casi sin pérdida; 15–20: muy alta; 21–26: buena; 27+: pérdida visible. No todos los codificadores por hardware admiten todos los ajustes.", L"Niedriger QP = bessere Qualität, größere Dateien. 0–10: nahezu verlustfrei; 15–20: sehr hoch; 21–26: gut; ab 27: sichtbare Verluste. Nicht jeder Hardware-Encoder unterstützt alle Einstellungen.") \
    X(S_QP_UNSUP_T, L"Quality mode", L"Mode qualité", L"Modo de calidad", L"Qualitätsmodus") \
    X(S_QP_UNSUP_M, L"The encoder does not accept QP settings: target bitrate used.", L"L’encodeur n’accepte pas ces réglages QP : débit cible utilisé.", L"El codificador no admite ajustes QP: se usa la tasa de bits objetivo.", L"Der Encoder akzeptiert keine QP-Einstellungen: Ziel-Bitrate wird verwendet.") \
    X(S_HK_LBL_FULL, L"Full screen shortcut", L"Raccourci plein écran", L"Atajo de pantalla completa", L"Kürzel Vollbild") \
    X(S_HK_FULL, L"The “full screen” shortcut is already used by another application.", L"Le raccourci « plein écran » est déjà utilisé par une autre application.", L"El atajo de «pantalla completa» ya está en uso por otra aplicación.", L"Das Tastenkürzel „Vollbild“ wird bereits von einer anderen Anwendung verwendet.") \
    X(S_MENU_FULL_ON, L"Full screen\t%s", L"Plein écran\t%s", L"Pantalla completa\t%s", L"Vollbild\t%s") \
    X(S_MENU_FULL_OFF, L"Exit full screen\t%s", L"Quitter le plein écran\t%s", L"Salir de pantalla completa\t%s", L"Vollbild beenden\t%s") \
    X(S_TIP_FRAME, L"Show / hide the frame (or double-click the bar)", L"Afficher / masquer le cadre (ou double-clic sur la barre)", L"Mostrar / ocultar el marco (o doble clic en la barra)", L"Rahmen ein-/ausblenden (oder Doppelklick auf die Leiste)") \
    X(S_TIP_PAUSE, L"Pause recording", L"Mettre l’enregistrement en pause", L"Pausar la grabación", L"Aufnahme pausieren") \
    X(S_TIP_RESUME, L"Resume recording", L"Reprendre l’enregistrement", L"Reanudar la grabación", L"Aufnahme fortsetzen") \
    X(S_CHK_MIC, L"Also record microphone", L"Enregistrer aussi le microphone", L"Grabar también el micrófono", L"Auch Mikrofon aufnehmen") \
    X(S_CHK_CLIP, L"Copy screenshot to clipboard", L"Copier l’image dans le presse-papiers", L"Copiar imagen al portapapeles", L"Bild in Zwischenablage kopieren") \
    X(S_CHK_EXCL, L"Exclude interface from captures", L"Exclure l’interface des captures", L"Excluir interfaz de las capturas", L"Oberfläche aus Aufnahmen ausblenden") \
    X(S_CHK_CLICKS, L"Highlight mouse clicks in video", L"Mettre en évidence les clics souris", L"Resaltar clics del ratón en vídeo", L"Mausklicks im Video hervorheben") \
    X(S_ROW_DELAY, L"Image timer", L"Retardateur (image)", L"Temporizador (imagen)", L"Selbstauslöser (Bild)") \
    X(S_DLY_0, L"None", L"Aucun", L"Ninguno", L"Keiner") \
    X(S_DLY_3, L"3 s", L"3 s", L"3 s", L"3 s") \
    X(S_DLY_5, L"5 s", L"5 s", L"5 s", L"5 s") \
    X(S_DLY_10, L"10 s", L"10 s", L"10 s", L"10 s") \
    X(S_HK_LBL_PAUSE, L"Pause shortcut", L"Raccourci pause", L"Atajo de pausa", L"Kürzel Pause") \
    X(S_HK_PAUSE, L"The “pause” shortcut is already used by another application.", L"Le raccourci « pause » est déjà utilisé par une autre application.", L"El atajo de «pausa» ya está en uso por otra aplicación.", L"Das Tastenkürzel „Pause“ wird bereits von einer anderen Anwendung verwendet.") \
    X(S_MENU_PAUSE, L"Pause recording\t%s", L"Mettre en pause\t%s", L"Pausar grabación\t%s", L"Aufnahme pausieren\t%s") \
    X(S_MENU_RESUME, L"Resume recording\t%s", L"Reprendre l’enregistrement\t%s", L"Reanudar grabación\t%s", L"Aufnahme fortsetzen\t%s") \
    X(S_MENU_OPENDIR, L"Open captures folder", L"Ouvrir le dossier des captures", L"Abrir carpeta de capturas", L"Aufnahmeordner öffnen") \
    X(S_MENU_OPENLAST, L"Open last capture", L"Ouvrir la dernière capture", L"Abrir última captura", L"Letzte Aufnahme öffnen") \
    X(S_MENU_COPYLAST, L"Copy last screenshot", L"Copier la dernière image", L"Copiar última imagen", L"Letztes Bild kopieren") \
    X(S_COUNTDOWN, L"Screenshot in %d s…", L"Capture dans %d s…", L"Captura en %d s…", L"Aufnahme in %d s…") \
    X(S_SAVED_T, L"Capture saved", L"Capture enregistrée", L"Captura guardada", L"Aufnahme gespeichert")

enum {
#define X(id, en, fr, es, de) id,
    STRINGS(X)
#undef X
    S_COUNT
};

/* Globales partagées (définies dans main.c) */
extern CFG g_cfg;
extern SIZEP g_sizes[MAXS];
extern int g_sizeN;
extern WCHAR g_ini[MAX_PATH];
extern WCHAR g_soundPath[MAX_PATH], g_soundRecPath[MAX_PATH];
extern WCHAR g_nameFmt[128];
extern WCHAR g_outDir[MAX_PATH];
extern WCHAR g_lastFile[MAX_PATH], g_lastShot[MAX_PATH];
extern HINSTANCE g_inst;
extern HWND g_main, g_cfgwnd, g_szwnd, g_frame, g_hl, g_mag;
extern HFONT g_font;
extern HICON g_icoIdle, g_icoRec;
extern NOTIFYICONDATAW g_nid;
extern UINT g_wmTaskbar, g_wmShowMe;
extern DWORD g_cfgHid;
extern BOOL g_cfgModal;
extern volatile BOOL g_rec;
extern BOOL g_picking;
extern BOOL g_frameOn;
extern BOOL g_uiHidden;
extern BOOL g_full;
extern RECT g_fullRect;
extern WCHAR g_status[96];
extern RECT g_textRc;
extern BOOL g_barExpanded, g_menuOpen;
extern int g_shotWait;

/* main.c */
const WCHAR *T(int id);
void cfg_load(void);
void cfg_save(void);
void sizes_default(void);
void sizes_load(void);
void sizes_save(void);
void dir_resolve(WCHAR *dir);
void make_path(BOOL video, const WCHAR *ext, WCHAR *out);
int ini_int(const WCHAR *sec, const WCHAR *key, int def);
void geo_save(void);
BOOL geo_load(RECT *r);
void play_shutter(void);
void play_rec_sound(void);
UINT64 sound_len100(int mode, const WCHAR *path);
HWND mk(HWND p, const WCHAR *cls, const WCHAR *txt, DWORD st, int x, int y, int w, int h, int id);
BOOL copy_dib_clipboard(const GRAB *g);
void open_capture_dir(void);
void open_last_capture(void);
void copy_last_shot(void);
void hotkey_handle(WPARAM id);

/* theme.c */
extern BOOL g_dark;
#define DK_BG  RGB(32, 32, 32)
#define DK_CTL RGB(51, 51, 51)
#define DK_TXT RGB(240, 240, 240)
BOOL sys_dark(void);
void allow_dark_app(BOOL on);
void allow_dark_window(HWND w, BOOL on);
void set_theme(HWND w, const WCHAR *subapp, const WCHAR *idlist);
void dark_apply(HWND h);
void sync_theme(void);
LRESULT dark_ctl(HWND ctlWnd, HDC dc, UINT msg);
BOOL dark_erase(HWND h, HDC dc);
int lum(COLORREF c);
int clamp255(int v);
COLORREF shade(COLORREF c, int d);
COLORREF text_for(COLORREF bg);
COLORREF frame_color(void);
COLORREF title_color(void);

/* capture.c */
BOOL grab_init(GRAB *g, int w, int h);
void grab_free(GRAB *g);
void grab_frame(GRAB *g, int x, int y, BOOL cursor);
void get_region(RECT *r);
void take_screenshot(void);
void shot_trigger(void);
void shot_countdown_tick(void);
void click_poll(UINT64 now);
BOOL click_any_active(UINT64 now);
void draw_click_rings_gdi(HDC dc, int rx, int ry, UINT64 now);
BOOL gpu_init(int *pw, int *ph, int fps);
void gpu_free(void);
DWORD WINAPI video_thread(LPVOID unused);
DWORD WINAPI video_thread_gpu(LPVOID unused);
void pick_start(void);
void pick_end(void);
void pick_poll(void);
void pick_commit(const RECT *b);
void pick_mouse_down(void);
void pick_mouse_move(void);
void pick_mouse_up(void);
void pick_wheel(int delta);
void uia_release(void);
LRESULT CALLBACK MagProc(HWND h, UINT m, WPARAM w, LPARAM l);

/* encoder.c */
extern REC rc;
void encoder_set_d3d_manager(void *mgr);
UINT64 raw100(void);
UINT64 now100(void);
IMFMediaType *vtype(const GUID *sub, int w, int h, int fps, int kbps);
BOOL audio_open(void);
void audio_close(void);
DWORD WINAPI audio_thread(LPVOID unused);
HRESULT sink_open(const WCHAR *path, int w, int h, BOOL gpuMode);
void rec_start(void);
void rec_stop(void);
void rec_pause_toggle(void);
void toggle_rec(void);

/* ui.c */
void icons_init(void);
void icons_free(void);
void tray_add(void);
void tray_set(HICON ic, const WCHAR *tip);
void balloon(const WCHAR *title, const WCHAR *msg);
void tray_menu(void);
void cfg_toggle(void);
void sizes_editor(BOOL addCurrent);
void apply_wh(int w, int h);
void apply_size(int i);
void cur_size(int *w, int *h);
void cur_pos(int *x, int *y);
void ui_update(void);
void update_title(void);
void ui_repaint(void);
void ui_apply(void);
void bar_apply(void);
int bar_width(void);
void layout_bar(HWND h);
void frame_hole(HWND h);
void toggle_frame(void);
void toggle_full(void);
void toggle_visible(void);
void apply_opacity(void);
void apply_exclude_capture(void);
void create_aux_windows(void);
void hotkeys_register(void);
void hotkeys_unregister(void);
void hk_text(int hk, WCHAR *out, int n);
void dpi_refresh(HWND src, int newDpi, const RECT *suggested);
LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l);
LRESULT CALLBACK FrameProc(HWND h, UINT m, WPARAM w, LPARAM l);
LRESULT CALLBACK CfgProc(HWND h, UINT m, WPARAM w, LPARAM l);
LRESULT CALLBACK SzProc(HWND h, UINT m, WPARAM w, LPARAM l);

#endif
