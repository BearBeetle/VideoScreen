/* Vs.c
 *
 * ビデオスクリーンセーバ
 * 
 * <履歴>
 * V1.2 2018/XX/XX  VC6 → VS2022(v143) 対応修正：ANSI API 明示、関数プロトタイプ修正、バッファ安全化)
 * V2.0 2024/02/XX  動画再生をMCIからMedia Foundationに変更
 *
 * (c) 1997-2018,BearBeetle
 */

#include <windows.h>
#include "resource.h"
#include "SCRNSAVE.H"
#define COBJMACROS  // <mfapi.h>には、これが必要
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <stdlib.h>
#include <time.h>    // srand 等
#include <stdio.h>   // sprintf_s
#include <string.h>  // strlen, memcpy, strcmp
#pragma comment(lib,"ScrnSave.lib")
#pragma comment(lib,"Comctl32.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")

extern char strFileNames[MAX_PATH*1024];
extern HINSTANCE hMainInstance; /* screen saver instance handle  */
extern char strIniFile[];  // INIファイルパス
extern void IniFileCreate();

char    *strPlayFileName[500];
int     iMaxPlayFile;        // 再生ファイル数
long    lZoom = 100;         // 表示倍率[%]
BOOL    IsEnd = FALSE, IsNoContents = FALSE, IsFirst = TRUE, IsRandPlay = FALSE, IsMute = FALSE, IsCont = FALSE, IsFIns = FALSE;
ULONG   ulPos;               // 再生位置
long    lCon;                // 再生インデックス保存
HWND    hMainWnd;            // メインウィンドウ
HWND    hVideoChild;         // 動画表示用子ウィンドウ
HWND    hTitleWnd;           // タイトル表示ウィンドウ
IMFPMediaPlayer* g_pPlayer = NULL; // Media Foundation メディアプレーヤー
DWORD g_lastVolume = 100;   // ミュート前の音量を保存


struct VSIZE {                // 動画サイズ
    long lWidth, lHeight;
} stVSize;
struct VSIZE stScreenSize;    // 画面サイズ
HWAVEOUT hWaveOut;

/* デバッグ用ログ出力 */
void DBG_Print(char* strDat)
{
#ifdef _DEBUG
    FILE* dbgfp;
    static char* strDbgFileName = "vs_dbg.log";
    if (fopen_s(&dbgfp, strDbgFileName, "a") == 0) {
        fprintf(dbgfp, "%s", strDat);
        fclose(dbgfp);
	}
#endif
}

void ErrVideoMsg(HRESULT hr);

// 文字コード変換用関数
static void CharToWChar(const char* src, wchar_t* dst, size_t dstCount)
{
    MultiByteToWideChar(CP_ACP, 0, src, -1, dst, (int)dstCount);
}

/* タイトルウィンドウを破棄 */
void TitleWndOff(void)
{
    if (hTitleWnd)
        DestroyWindow(hTitleWnd);
}

/* タイトルウィンドウを表示 */
void TitleWndOn(char *strTitle)
{
    long lTitleLen;
    HDC hDc;

    TitleWndOff();
    lTitleLen = (long)strlen(strTitle);
    hTitleWnd = CreateWindowA("STATIC", strTitle, WS_POPUP | WS_VISIBLE,
        0, 0, stScreenSize.lWidth, 16,
        hMainWnd, NULL, hMainInstance, NULL);
    hDc = GetDC(hTitleWnd);
    TextOutA(hDc, 0, 0, strTitle, (int)lTitleLen);
    ReleaseDC(hTitleWnd, hDc);
}

/* ミュート制御 */
void Video_MuteOn(void)
{
    float fLast_Volume = 0.0f;
    HRESULT hr;
    char buf[64];

    DBG_Print("Video_MuteOn called\n");
    if (!g_pPlayer || !IsMute) return;
    DBG_Print("Video_MuteOn Set\n");
    hr = IMFPMediaPlayer_GetVolume(g_pPlayer, (float *) &fLast_Volume);
    if (FAILED(hr)) {
        sprintf_s(buf, sizeof(buf), "Video_MuteOn GetVolume failed: 0x%08X\n", hr);
        DBG_Print(buf);
    }
    fLast_Volume = fLast_Volume * 100.0f;
    hr = IMFPMediaPlayer_SetVolume(g_pPlayer, 0);
    if (FAILED(hr)) {
        sprintf_s(buf, sizeof(buf), "Video_MuteOn SetVolume failed: 0x%08X\n", hr);
        DBG_Print(buf);
    }
}
void Video_MuteOff(void)
{
    float fLast_Volume = 0.0f;
    HRESULT hr;
    char buf[64];

    DBG_Print("Video_MuteOff called\n");
    if (!g_pPlayer || !IsMute ) return;
    DBG_Print("Video_MuteOff Set\n");
    fLast_Volume = (float)g_lastVolume / 100.0f;
    hr = IMFPMediaPlayer_SetVolume(g_pPlayer, fLast_Volume);
    if (FAILED(hr)) {
        sprintf_s(buf, sizeof(buf), "Video_MuteOff SetVolume failed: 0x%08X\n", hr);
        DBG_Print(buf);
    }
}


/* メディアプレーヤーの状態を取得 */
MFP_MEDIAPLAYER_STATE Video_GetState()
{
    HRESULT hr;
    char buf[64];

    DBG_Print("Video_GetState called\n");
    if (!g_pPlayer) {
        return MFP_MEDIAPLAYER_STATE_EMPTY;
    }

    MFP_MEDIAPLAYER_STATE state;
    hr = IMFPMediaPlayer_GetState(g_pPlayer, &state);
    if (FAILED(hr)) {
        sprintf_s(buf, sizeof(buf), "Video_GetState GetState failed: 0x%08X\n", hr);
		DBG_Print(buf);
        return MFP_MEDIAPLAYER_STATE_EMPTY;
    }
    sprintf_s(buf, sizeof(buf), "Video_GetState GetState OK State=: 0x%08X\n", state);
    DBG_Print(buf);


    return state;
}
HRESULT Video_Open(const wchar_t* filename)
{
    HRESULT hr;
    IMFPMediaItem* pItem = NULL;
    DBG_Print("Video_Open called\n");

    if (g_pPlayer == NULL) return E_POINTER;

    // 1. メディアアイテムを作成
    hr = IMFPMediaPlayer_CreateMediaItemFromURL(g_pPlayer, filename, TRUE, 0, &pItem);

    if (SUCCEEDED(hr)) {
        // 2. 【重要】作成したアイテムをプレーヤーに登録する
        hr = IMFPMediaPlayer_SetMediaItem(g_pPlayer, pItem);

        // 登録後は pItem 自体は Release して構いません
        IMFPMediaItem_Release(pItem);
        DBG_Print("Video_Open: SetMediaItem Success\n");
    }

    if (FAILED(hr)) {
        char buf[64];
        sprintf_s(buf, sizeof(buf), "Open/SetMediaItem failed: 0x%08X\n", hr);
        DBG_Print(buf);
    }
    return hr;
}

/* メディアプレーヤーを閉じる */
void Video_Close()
{
    DBG_Print("Video_Close called\n");
    if (!g_pPlayer) return;
    Video_MuteOff();
    if (g_pPlayer) {
        IMFPMediaPlayer_Shutdown(g_pPlayer);
        IMFPMediaPlayer_Release(g_pPlayer);
        g_pPlayer = NULL;
    }
}

/* メディアプレーヤー再生一時停止 */
void Video_Stop()
{
    DBG_Print("Video_Stop called\n");
    if (Video_GetState() == MFP_MEDIAPLAYER_STATE_STOPPED)
        return;

    /* 一時停止 */
    if (g_pPlayer)
        IMFPMediaPlayer_Pause(g_pPlayer);

    /*` 再生位置 -> ulPos */
    // 修正: GetPositionには引数が2つ必要です（guidPositionType, pvPositionValue）
    // Media FoundationのMFP_POSITIONTYPE_100NSを使い、PROPVARIANT型で値を受け取る必要があります
    PROPVARIANT varPos;
    PropVariantInit(&varPos);
    HRESULT hr = IMFPMediaPlayer_GetPosition(
        g_pPlayer,
        (const GUID *)&MFP_POSITIONTYPE_100NS, // ポインタ型にキャスト
        &varPos
    );
    if (SUCCEEDED(hr) && varPos.vt == VT_I8) {
        ulPos = (ULONG)(varPos.hVal.QuadPart / 10000); // MFTIME to milliseconds
    } else {
        ulPos = 0;
    }
    PropVariantClear(&varPos);
}

/* メディアプレーヤー再生開始 */
BOOL Video_Play(void)
{
    HRESULT hr;
    DBG_Print("Video_Play called\n");
    if (!g_pPlayer) return FALSE;

    // 2. シーク位置の計算（g_iIntTime ではなく g_iInterval を使用）
    if (ulPos != 0) {
        PROPVARIANT var;
        PropVariantInit(&var);
        var.vt = VT_I8;
        hr = IMFPMediaPlayer_GetDuration(g_pPlayer, &MFP_POSITIONTYPE_100NS, &var);
        if (SUCCEEDED(hr)) {
            var.hVal.QuadPart = (LONGLONG)ulPos * 10000;
            hr = IMFPMediaPlayer_SetPosition(g_pPlayer, &MFP_POSITIONTYPE_100NS, &var);
            if (FAILED(hr)) DBG_Print("SetPosition failed\n");
        }
        PropVariantClear(&var);
    }

    // 3. 再生開始
    hr = IMFPMediaPlayer_Play(g_pPlayer);
    if (FAILED(hr)) {
        char buf[64];
        sprintf_s(buf, sizeof(buf), "IMFPMediaPlayer_Play failed: 0x%08X\n", hr);
        DBG_Print(buf);
        return FALSE;
    }

    DBG_Print("Video_Play OK\n");
    return TRUE;
}
/* ビデオウィンドウ移動 */
void MoveVideoWindow(BOOL IsFirstCall, BOOL IsMove)
{
    RECT rc;
    SIZE sz;    /* 動画の本来のサイズ */
	HRESULT hr;

    DBG_Print("MoveVideoWindow called\n");
    if (!g_pPlayer) return;

    hr = IMFPMediaPlayer_GetNativeVideoSize(g_pPlayer, &sz, NULL);
    if (FAILED(hr)) {
        DBG_Print("GetNativeVideoSize err\n");
        ErrVideoMsg(hr);
        Video_Stop();
        Video_Close();
        return;
    }

    // stScreenSize = 拡大可能な最大の動画サイズ（指定無し時はスクリーンサイズ）
    if (stScreenSize.lWidth < 0)
        stScreenSize.lWidth = GetSystemMetrics(SM_CXSCREEN);
    if (stScreenSize.lHeight < 0)
        stScreenSize.lHeight = GetSystemMetrics(SM_CYSCREEN);

    // 倍率をかける
    if (lZoom == 0) {
        stVSize.lWidth = stScreenSize.lWidth;
        stVSize.lHeight = stScreenSize.lHeight;
    }
    else {
        stVSize.lWidth = (sz.cx * lZoom) / 100;
        stVSize.lHeight = (sz.cy * lZoom) / 100;
    }

	// stVSize = 表示する動画サイズ（倍率をかけたサイズ）
    if (stVSize.lWidth > stScreenSize.lWidth)
        stVSize.lWidth = stScreenSize.lWidth;
    if (stVSize.lHeight > stScreenSize.lHeight)
        stVSize.lHeight = stScreenSize.lHeight;



    // ビデオウィンドウの設定
    {
        char buf[128];
        sprintf_s(buf, sizeof(buf), "stVSize size: %ld x %ld\n", stVSize.lWidth, stVSize.lHeight);
        DBG_Print(buf);
        sprintf_s(buf, sizeof(buf), "Video size: %ld x %ld\n", sz.cx, sz.cy);
        DBG_Print(buf);
    }

    GetClientRect(hVideoChild, &rc);
    if (IsMove) {
        rc.left = (rand() * (stScreenSize.lWidth - stVSize.lWidth)) / RAND_MAX;
        rc.top = (rand() * (stScreenSize.lHeight - stVSize.lHeight)) / RAND_MAX;
    }
    else {
        rc.left = (stScreenSize.lWidth - stVSize.lWidth) / 2;
        rc.top = (stScreenSize.lHeight - stVSize.lHeight) / 2;
    }
    rc.right = rc.left + stVSize.lWidth;
    rc.bottom = rc.top + stVSize.lHeight;
    {
		char buf[128];
        sprintf_s(buf, sizeof(buf), "Calculated video position: left=%ld, top=%ld, right=%ld, bottom=%ld\n",
			rc.left, rc.top, rc.right, rc.bottom);
		DBG_Print(buf);
    }

    // IMFPMediaPlayer::UpdateVideo は引数を取らないため、
    // 表示先矩形はプレーヤーが返すビデオ HWND を移動／サイズ変更して指定します。
    SetWindowPos(hVideoChild, HWND_TOP, rc.left, rc.top,
                     rc.right - rc.left, rc.bottom - rc.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);

    // 描画更新を通知
    IMFPMediaPlayer_UpdateVideo(g_pPlayer);

    DBG_Print("MoveVideoWindow End\n");

}


void ErrVideoMsg(HRESULT hr)
{
    char buf[256];
    const size_t bufSize = sizeof(buf);
    DWORD flags, len;

    switch (hr) {
        case MF_E_INVALIDREQUEST:
            strcpy_s(buf, bufSize, "Invalid request");
            break;
        case MF_E_UNSUPPORTED_FORMAT:
            strcpy_s(buf, bufSize, "Unsupported format");
            break;
        case MF_E_NO_MORE_TYPES:
            strcpy_s(buf, bufSize, "No more types");
            break;
        case MF_E_NOT_INITIALIZED:
            strcpy_s(buf, bufSize, "Media Foundation not initialized");
            break;
        case MF_E_UNSUPPORTED_BYTESTREAM_TYPE:
            strcpy_s(buf, bufSize, "Unsupported bytestream type");
            break;
        default:
            flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
            len = FormatMessageA(flags, NULL, hr, 0, buf, (DWORD)bufSize, NULL);
            if (len == 0) {
                // FormatMessage が失敗した場合
                snprintf(buf, bufSize, "Unknown error (HRESULT=0x%08X)", hr);
            }
            break;
    }
    DBG_Print(buf);
	DBG_Print("\n");
    TitleWndOn(buf);
    Sleep(3500);
    TitleWndOff();
}


/* 次のコンテンツを再生 */
void PlayNextContent(BOOL IsMove)
{
    static int iCount = 0;
    static BOOL IsRunning = FALSE;
    BOOL IsCancel = FALSE;
    static int PlayStep = 0;
    HRESULT hr;
    DBG_Print("PlayNextContent called\n");

    // 多重呼び出しチェック
    if (IsRunning) {
        return;
    }
    IsRunning = TRUE;


    switch (PlayStep) {
    case 0:
        DBG_Print("PlayNextContent Step0 Start\n");
        // 最初の呼び出しか？
        if (IsFirst) {
            if (IsCont && lCon >= 0) {	// 続き再生
                iCount = (int)lCon;
            }
            else {
                if (IsRandPlay) {
                    // ランダムモードの場合適当な値
                    iCount = (rand() * iMaxPlayFile) / RAND_MAX;
                    if (iCount >= iMaxPlayFile)
                        iCount = iMaxPlayFile - 1;
                    lCon = (long)iCount;
                    ulPos = 0;
                }
                else {
                    iCount = 0;
                }
            }
        }
        else {
            if (IsCont) {	// 続き再生
                lCon = (long)iCount;
            }
        }
        if (g_pPlayer != NULL) {
            DBG_Print("g_pPlayer != NULL\n");
            Video_Stop();
            Video_Close();
        }
        DBG_Print("PlayNextContent Step0 End\n");
        PlayStep = 1; // プレーヤーの初期化は次のステップで行う
        break;

    case 1:
        DBG_Print("PlayNextContent Step1 Start\n");
        hr = MFPCreateMediaPlayer(NULL, FALSE, 0, NULL, hVideoChild, &g_pPlayer);
        if (FAILED(hr)) {
            ErrVideoMsg(hr);
            Video_Stop();
            Video_Close();
            DBG_Print("PlayNextContent Step1 Err End\n");
            IsCancel = TRUE;
            break;
        }
        DBG_Print("PlayNextContent Step1 End\n");
        PlayStep = 2; // 次のステップへ
        break;

    case 2:
        DBG_Print("PlayNextContent Step2 Start\n");
        {
            wchar_t wFileName[MAX_PATH];
            char buf[128];
            CharToWChar(strPlayFileName[iCount], wFileName, MAX_PATH);
            sprintf_s(buf, sizeof(buf), "Converted filename: %ls\n", wFileName);
            DBG_Print(buf);
            hr = Video_Open(wFileName);
            if (FAILED(hr)) {
                Video_Stop();
                Video_Close();
                DBG_Print("PlayNextContent Step2 Err End\n");
                IsCancel = TRUE;
                break;
            }
        }
        DBG_Print("PlayNextContent Step2 End\n");
        PlayStep = 3; // 次のステップへ
        break;

    case 3:
        DBG_Print("PlayNextContent Step3 Start\n");
        if (Video_GetState() == MFP_MEDIAPLAYER_STATE_EMPTY) {
            DBG_Print("PlayNextContent Step3 (State=MFP_MEDIAPLAYER_STATE_EMPTY) End\n");
            break;
        }
        // デバッグ用：最終的な状態を確認
        {
            char stateBuf[64];
            sprintf_s(stateBuf, sizeof(stateBuf), "Final State before Play: %d\n", Video_GetState());
            DBG_Print(stateBuf);
            DBG_Print("PlayNextContent Step3 End\n");
        }
        PlayStep = 4; // 次のステップへ
        break;

    case 4:
        DBG_Print("PlayNextContent Step4 Start\n");
        if (!Video_Play()) {
            Video_Stop();
            Video_Close();
            DBG_Print("PlayNextContent Step4 Err End\n");
            IsCancel = TRUE;
            break;
        }
        // ミュート設定
        if (IsMute) {
            Video_MuteOn();
        }
        else {
            Video_MuteOff();
        }
        // 
        //　表示位置設定
        MoveVideoWindow(TRUE, IsMove);
        // タイトル表示
        if (IsFIns) {
            TitleWndOn(strPlayFileName[iCount]);
        }
        DBG_Print("PlayNextContent Step5 End\n");

        SetActiveWindow(hMainWnd);
        BringWindowToTop(hMainWnd);  /* 最前面に移動 */
        SetFocus(hMainWnd);

        PlayStep = 5; // 次のステップへ
        DBG_Print("PlayNextContent Step4 End\n");
        break;

    case 5:
        DBG_Print("PlayNextContent Step5 Start\n");
        if (g_pPlayer == NULL) {
            DBG_Print("PlayNextContent Step5 End g_pPlayer == NULL\n");
            IsCancel = TRUE;
            break;
        }
        if (Video_GetState() != MFP_MEDIAPLAYER_STATE_PLAYING) {
            DBG_Print("PlayNextContent Step5 End State is not PLAYING\n");
            IsCancel = TRUE;
            break;
		}
        DBG_Print("PlayNextContent Step5 End\n");
        break;
    }

    if ( IsCancel ) {
		// 次のコンテンツ再生へ
        DBG_Print("Next Play\n");
        if (IsRandPlay) {
            iCount = (rand() * iMaxPlayFile) / RAND_MAX;
            if (iCount >= iMaxPlayFile)
                iCount = iMaxPlayFile - 1;
        }
        else {
            iCount++;
        }
        if (iCount >= iMaxPlayFile) {
            iCount = 0;
        }
        PlayStep = 0; // 最初のステップへ
    }
    IsFirst = FALSE;
    IsRunning = FALSE; // フラグを戻すのを忘れずに
    return;
}

/* ScreenSaver のメインプロシージャ（ANSI 版） */
LONG WINAPI ScreenSaverProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int j, k;
    static char strSection[128], strKey[128];
    static UINT uTimer = 0, uTimerCount = 0, uIntValue;
    static UINT uTimer2 = 0, uMinCount = 0, uEndValue;
    static int nMauseMoveCount = 0;
    static BOOL IsSmall, IsTimeInt = FALSE;
    RECT rect;
    HDC hDc;
    PAINTSTRUCT ps;
    HICON hicon;

    switch (uMsg) {
    case WM_PAINT:
        if (IsSmall) {
            hDc = BeginPaint( hwnd,&ps );
            /* 文字列リソース名 "ID_APP" を使うため ANSI 版を明示 */
            hicon = LoadIconA(hMainInstance, "ID_APP");
            DrawIcon(hDc,0,0,hicon);
            EndPaint( hwnd , &ps );
        } else {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // MFPlayに描画を更新させる
            if (g_pPlayer) {
                IMFPMediaPlayer_UpdateVideo(g_pPlayer);
            }

            EndPaint(hwnd, &ps);
            break;
        }

        break;

    case WM_CREATE:
        IniFileCreate();
        IsFirst = TRUE;
        hMainWnd = hwnd;
        {
            RECT rc;
            GetClientRect(hMainWnd, &rc);
            hVideoChild = CreateWindowEx(
                0,
                TEXT("STATIC"),        // クラス名。単純な矩形なら "STATIC" でOK
                NULL,
                WS_CHILD | WS_VISIBLE , // 子ウィンドウとして表示
                0, 0, rc.right - rc.left, rc.bottom - rc.top,   // 位置とサイズ
                hMainWnd,           // 親ウィンドウのハンドル
                (HMENU)101,
                hMainInstance,
                NULL
            );
        }
        if (hVideoChild == NULL) {
            DWORD error = GetLastError();
            char strDat[100];
            sprintf_s(strDat, sizeof(strDat), "CreateWindowEx failed! %u", error);
        }
        GetWindowRect(hwnd, &rect);
        stScreenSize.lWidth = rect.right - rect.left;
        if (stScreenSize.lWidth < 400) {
            IsSmall = TRUE;
            IsEnd = TRUE;
            break;
        }
        LoadStringA(hMainInstance, IDS_SECTION_NAME, strSection, (int)sizeof(strSection));
        LoadStringA(hMainInstance, IDS_KEY_NAME, strKey, (int)sizeof(strKey));
        GetPrivateProfileStringA(strSection, strKey, "", strFileNames, (DWORD)sizeof(strFileNames), strIniFile);
        j = (int)strlen(strFileNames);
#if 1
        char* p = strFileNames;
        k = 0;

        while ((p = strchr(p, '|')) != NULL) {
            p++;  // '|' の次へ進む
            const char* q = strchr(p, '|');  // 次の '|' を探す
            if (q == NULL) break;

            size_t len = q - p;
            if (len > 0) {
                strPlayFileName[k] = p;
                k++;
            }
        }
        for (int i = 0; i < j; i++) {
            if (strFileNames[i] == '|') {
                strFileNames[i] = '\0';
            }
        }
        if(0) { // デバッグ用
            FILE* fp = fopen("aaa.log", "w");
            fprintf(fp, "k=%d\n", k);
            for (int i = 0; i < k; i++) {
                fprintf(fp, "File %d: %s\n", i, strPlayFileName[i]);
            }
            fclose(fp);
        }
#else
        strPlayFileName[0] = strFileNames;
        k = 1;
        for (i = 1; i < j; i++) {
            if (strFileNames[i] == '|' &&
                (strFileNames[i - 1] >= ' ' && strFileNames[i - 1] <= 'z') &&
                (strFileNames[i - 1] != 0)) {
                strFileNames[i] = '\0';
                if (i < (j - 1)) {
                    strPlayFileName[k] = &strFileNames[i + 1];
                    i++;
                    k++;
                }
            }
        }
#endif
        iMaxPlayFile = k;

        IsEnd = FALSE;
        if (iMaxPlayFile == 0)
            IsNoContents = TRUE;

        // 乱数初期化
        IsRandPlay = GetPrivateProfileIntA(strSection, "RAND", FALSE, strIniFile);
        srand((unsigned)GetTickCount64());

        // ウィンドウサイズ設定
        lZoom = GetPrivateProfileIntA(strSection, "SIZE", 100, strIniFile);
        if (lZoom > 300) lZoom = 300;
        if (lZoom < 0) lZoom = 0;

        // 続き再生設定
        IsCont = GetPrivateProfileIntA(strSection, "CONT", 0, strIniFile);
        if (IsCont) {
            lCon = GetPrivateProfileIntA(strSection, "CONPOS", 0, strIniFile);
            ulPos = GetPrivateProfileIntA(strSection, "POS", 0, strIniFile);
        }
        else {
            lCon = 0;
            ulPos = 0;
        }

        // ミュート設定
        IsMute = GetPrivateProfileIntA(strSection, "MUTE", FALSE, strIniFile);

        //　ファイル名表示有無
        IsFIns = GetPrivateProfileIntA(strSection, "F_INS", FALSE, strIniFile);

        // タイマ設定
        uIntValue = GetPrivateProfileIntA(strSection, "INTERVAL", 30, strIniFile) * 2;
        if (!IsNoContents) {
            uTimer = SetTimer(hwnd, 1, 500, NULL);
            uTimerCount = 0;
        }
        nMauseMoveCount = 0;

        // タイマ設定(終了時間)
        uEndValue = GetPrivateProfileIntA(strSection, "ENDTIME", 0, strIniFile) * 10;
        if (!IsNoContents && uEndValue) {
            uTimer2 = SetTimer(hwnd, 2, 6000, NULL);
            uMinCount = 0;
        }

        /* 表示サイズ制限 */
        stScreenSize.lWidth = GetPrivateProfileIntA(strSection, "SCREEN_WIDTH", -1, strIniFile);
        stScreenSize.lHeight = GetPrivateProfileIntA(strSection, "SCREEN_HEIGHT", -1, strIniFile);

        /* メディアプレーヤー初期化 */
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        MFStartup(MF_VERSION, MFSTARTUP_FULL);

        break;

    case WM_TIMER:
        if (IsTimeInt)	// 前の処理中はタイマを無視
            break;

        if (wParam == 1) {
            IsTimeInt = TRUE;
            {
                // マウスに移動があればイベントを起こす
                // (何故かマウスイベントが起こらないため）
                static POINT stPos1;
                POINT stPos2;
                GetCursorPos(&stPos2);
                if (memcmp(&stPos1, &stPos2, sizeof(POINT))) {
                    PostMessage(hwnd, WM_USER, 0, 0);
                }
                memcpy(&stPos1, &stPos2, sizeof(POINT));
            }
            /* 動画再生中 */
            if (Video_GetState() == MFP_MEDIAPLAYER_STATE_PLAYING) {
                uTimerCount++;
                if (uTimerCount >= uIntValue) { // 60秒ごとに表示位置を変える
                    uTimerCount = 0;
                    nMauseMoveCount = 0;
                    if (uIntValue != 0)
                        MoveVideoWindow(FALSE, TRUE);
                }
                IsTimeInt = FALSE;
                break;
            }
            else {
                if (!IsSmall) {
					BOOL IsMove;
                    IsMove = (uIntValue != 0);
                    PlayNextContent(IsMove);
                }
            }
            IsTimeInt = FALSE;
        }
        if (wParam == 2) {	// 終了時間イベント
            if (uEndValue == 0) // 念のため
                break;
            IsTimeInt = TRUE;
            uMinCount++;
            if (uMinCount >= uEndValue) {
				Video_Stop();
                Video_Close();
                IsEnd = FALSE;
                if (uTimer) {
                    KillTimer(hwnd, uTimer);
                    uTimer = 0;
                }
                if (uTimer2) {
                    KillTimer(hwnd, uTimer2);
                    uTimer2 = 0;
                }
                IsSmall = TRUE;
            }
            IsTimeInt = FALSE;
        }
        break;

    case WM_LBUTTONDOWN:    //  Terminates the screen saver.
    case WM_MBUTTONDOWN:    //  Terminates the screen saver.
    case WM_RBUTTONDOWN:    //  Terminates the screen saver.
        if (IsTimeInt)      // タイマ処理中は無視 v1.71
            return FALSE;
        IsEnd = TRUE;
        break;

    case WM_KEYDOWN:        //  Terminates the screen saver.
        if (IsTimeInt)      // タイマ処理中は無視 v1.71
            return FALSE;

        IsTimeInt = TRUE;
        if (!IsSmall) {
            nMauseMoveCount = 0;
            uTimerCount = 0;
            if ((int)wParam == 'N' || (int)wParam == 'n') {     // 「N」を押した場合
                Video_Stop();
                Video_Close();
                IsEnd = FALSE;
                IsTimeInt = FALSE;
                return FALSE;
            }
            if ((int)wParam == 'R' || (int)wParam == 'r') {     // 「R」を押した場合

                if (Video_GetState() != MFP_MEDIAPLAYER_STATE_PLAYING) {
                    // 動画再生中でない場合
                    IsTimeInt = FALSE;
                    return FALSE;
                }
                // 頭に戻す
                PROPVARIANT varPos;
                PropVariantInit(&varPos);
                varPos.vt = VT_I8;
                varPos.hVal.QuadPart = 0;
                IMFPMediaPlayer_SetPosition(
                    g_pPlayer,
                    (const GUID *)&MFP_POSITIONTYPE_100NS,
                    &varPos
                );
                PropVariantClear(&varPos);

                IsEnd = FALSE;
                IsTimeInt = FALSE;
                return FALSE;
            }
            if ((int)wParam == 'S' || (int)wParam == 's') {     // 「S」を押した場合
                if (Video_GetState() != MFP_MEDIAPLAYER_STATE_PLAYING) {
                    // 動画再生中でない場合
                    IsEnd = FALSE;
                    IsTimeInt = FALSE;
                    return FALSE;
                }
                if (IsMute) {
                    Video_MuteOff(); // 先にIsMute=FALSEにするとOFF処理されないから
                    IsMute = FALSE;
                }
                else {
                    IsMute = TRUE;
                    Video_MuteOn();
                }
                IsEnd = FALSE;
                IsTimeInt = FALSE;
                return FALSE;
            }
        }
        IsEnd = TRUE;
        IsTimeInt = FALSE;
        break;

    case WM_USER:
        nMauseMoveCount++;
        if (nMauseMoveCount > 10) {
            IsEnd = TRUE;
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
        break;

    case WM_MOUSEMOVE:
        if (IsTimeInt)
            return FALSE;
        nMauseMoveCount++;
        if (nMauseMoveCount > 10)
            IsEnd = TRUE;
        else
            return FALSE;
        break;

    case WM_CLOSE:
    case WM_DESTROY:
        if (IsEnd == FALSE)
            return FALSE;
        if (uTimer) {
            KillTimer(hwnd, uTimer);
            uTimer = 0;
        }
        if (uTimer2) {
            KillTimer(hwnd, uTimer2);
            uTimer2 = 0;
        }
		Video_Close();
        TitleWndOff();
        {
            char strDat[100];
            sprintf_s(strDat, sizeof(strDat), "%u", (unsigned)ulPos);
            WritePrivateProfileStringA(strSection, "POS", strDat, strIniFile);
            sprintf_s(strDat, sizeof(strDat), "%u", (unsigned)lCon);
            WritePrivateProfileStringA(strSection, "CONPOS", strDat, strIniFile);
        }
		// PostMessage(hVideoChild, WM_CLOSE, 0, 0);
        break;
    }

    return DefScreenSaverProc(hwnd, uMsg, wParam, lParam);
}

/* 登録用（古い署名から標準署名へ変更） */
BOOL WINAPI RegisterDialogClasses(HINSTANCE hInst)
{
    hMainInstance = hInst;
    return TRUE;
}
