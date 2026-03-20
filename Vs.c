/* Vs.c
 *
 * ビデオスクリーンセーバ
 * 
 * <履歴>
 * V1.2 2018/XX/XX  VC6 → VS2022(v143) 対応修正：ANSI API 明示、関数プロトタイプ修正、バッファ安全化)
 * V3.0 2026/03/XX  動画再生をMCIからWMP(Windows Media Player)に変更
 *
 * (c) 1997-2026,BearBeetle
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
#define INT_TIMEOUT    100	// WM_TIMERが発生する間隔[ms]

char    *strPlayFileName[500];
int     iMaxPlayFile;        // 再生ファイル数
long    lZoom = 100;         // 表示倍率[%]
BOOL    IsEnd = FALSE, IsNoContents = FALSE, IsFirst = TRUE, IsRandPlay = FALSE, IsMute = FALSE, IsCont = FALSE, IsFIns = FALSE;
ULONG   ulPos;               // 再生位置
long    lCon;                // 再生インデックス保存
HWND    hMainWnd;            // メインウィンドウ
HWND    hVideoChild;         // 動画表示用子ウィンドウ
HWND    hTitleWnd;           // タイトル表示ウィンドウ
IMFPMediaPlayer* pPlayer = NULL; // Media Foundation メディアプレーヤー
DWORD lastVolume = 100;      // ミュート前の音量を保存


struct VSIZE {                // 動画サイズ
    long lWidth, lHeight;
} stVSize;
struct VSIZE stScreenSize;    // 画面サイズ
HWAVEOUT     hWaveOut;
long         lPrimaryOffsetX = 0;
long         lPrimaryOffsetY = 0;

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
    if (!pPlayer || !IsMute) return;
    DBG_Print("Video_MuteOn Set\n");
    hr = IMFPMediaPlayer_GetVolume(pPlayer, (float *) &fLast_Volume);
    if (FAILED(hr)) {
        sprintf_s(buf, sizeof(buf), "Video_MuteOn GetVolume failed: 0x%08X\n", hr);
        DBG_Print(buf);
    }
    lastVolume = (DWORD)(fLast_Volume * 100.0f);
    hr = IMFPMediaPlayer_SetVolume(pPlayer, 0);
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
    if (!pPlayer || !IsMute ) return;
    DBG_Print("Video_MuteOff Set\n");
    fLast_Volume = (float)lastVolume / 100.0f;
    hr = IMFPMediaPlayer_SetVolume(pPlayer, fLast_Volume);
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
    if (!pPlayer) {
        return MFP_MEDIAPLAYER_STATE_EMPTY;
    }

    MFP_MEDIAPLAYER_STATE state;
    hr = IMFPMediaPlayer_GetState(pPlayer, &state);
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

    if (pPlayer == NULL) return E_POINTER;

    // 1. メディアアイテムを作成
    hr = IMFPMediaPlayer_CreateMediaItemFromURL(pPlayer, filename, TRUE, 0, &pItem);

    if (SUCCEEDED(hr)) {
        // 2. 【重要】作成したアイテムをプレーヤーに登録する
        hr = IMFPMediaPlayer_SetMediaItem(pPlayer, pItem);

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
    if (!pPlayer) return;
    Video_MuteOff();
    if (pPlayer) {
        IMFPMediaPlayer_Shutdown(pPlayer);
        IMFPMediaPlayer_Release(pPlayer);
        pPlayer = NULL;
    }
}

/* メディアプレーヤー再生一時停止 */
void Video_Stop()
{
    DBG_Print("Video_Stop called\n");
    if (!pPlayer) return;
    if (Video_GetState() == MFP_MEDIAPLAYER_STATE_STOPPED)  return;

    /* 一時停止 */
    IMFPMediaPlayer_Pause(pPlayer);

    /*` 再生位置 -> ulPos */
    // 修正: GetPositionには引数が2つ必要です（guidPositionType, pvPositionValue）
    // Media FoundationのMFP_POSITIONTYPE_100NSを使い、PROPVARIANT型で値を受け取る必要があります
    PROPVARIANT varPos;
    PropVariantInit(&varPos);
    HRESULT hr = IMFPMediaPlayer_GetPosition(
        pPlayer,
        (const GUID *)&MFP_POSITIONTYPE_100NS, // ポインタ型にキャスト
        &varPos
    );
    if (SUCCEEDED(hr) && varPos.vt == VT_I8) {
		char data[64];  
        ulPos = (ULONG)(varPos.hVal.QuadPart / 10000); // MFTIME to milliseconds
		sprintf_s(data, sizeof(data), "Video_Stop GetPosition OK ulPos: %lu ms\n", ulPos);
		DBG_Print(data);
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
    if (!pPlayer) return FALSE;

    // 2. シーク位置の計算
    if (ulPos != 0) {
        PROPVARIANT varPos;
        PropVariantInit(&varPos);

        // 型をVT_I8（符号あり64ビット整数）に固定する
        varPos.vt = VT_I8;
        varPos.hVal.QuadPart = (LONGLONG)ulPos * 10000;

        hr = IMFPMediaPlayer_SetPosition(pPlayer, &MFP_POSITIONTYPE_100NS, &varPos);

        if (FAILED(hr)) {
            char buf[64];
            sprintf_s(buf, sizeof(buf), "SetPosition failed: 0x%08X\n", hr);
            DBG_Print(buf);
        }
        else {
            DBG_Print("SetPosition OK\n");
        }
        PropVariantClear(&varPos);
    }

    // 3. 再生開始
    hr = IMFPMediaPlayer_Play(pPlayer);
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
    RECT stRect;
    SIZE stSize;    /* 動画の本来のサイズ */
	HRESULT hr;

    DBG_Print("MoveVideoWindow called\n");
    if (!pPlayer) return;

    hr = IMFPMediaPlayer_GetNativeVideoSize(pPlayer, &stSize, NULL);
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
        stVSize.lWidth  = stScreenSize.lWidth;
        stVSize.lHeight = stScreenSize.lHeight;
    }
    else {
        stVSize.lWidth  = (stSize.cx * lZoom) / 100;
        stVSize.lHeight = (stSize.cy * lZoom) / 100;
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
        sprintf_s(buf, sizeof(buf), "Video size: %ld x %ld\n", stSize.cx, stSize.cy);
        DBG_Print(buf);
    }

    GetClientRect(hVideoChild, &stRect);
    if (IsMove) {
        stRect.left = lPrimaryOffsetX + (rand() * (stScreenSize.lWidth - stVSize.lWidth)) / RAND_MAX;
        stRect.top  = lPrimaryOffsetY + (rand() * (stScreenSize.lHeight - stVSize.lHeight)) / RAND_MAX;
    }
    else {
        stRect.left = lPrimaryOffsetX + (stScreenSize.lWidth - stVSize.lWidth) / 2;
        stRect.top  = lPrimaryOffsetY + (stScreenSize.lHeight - stVSize.lHeight) / 2;
    }
    stRect.right  = stRect.left + stVSize.lWidth;
    stRect.bottom = stRect.top + stVSize.lHeight;
    {
		char buf[128];
        sprintf_s(buf, sizeof(buf), "Calculated video position: left=%ld, top=%ld, right=%ld, bottom=%ld\n",
            stRect.left, stRect.top, stRect.right, stRect.bottom);
		DBG_Print(buf);
    }

    // IMFPMediaPlayer::UpdateVideo は引数を取らないため、
    // 表示先矩形はプレーヤーが返すビデオ HWND を移動／サイズ変更して指定します。
    SetWindowPos(hVideoChild, HWND_TOP, stRect.left, stRect.top,
        stRect.right - stRect.left, stRect.bottom - stRect.top,
        SWP_NOZORDER | SWP_NOACTIVATE);

    // 描画更新を通知
    IMFPMediaPlayer_UpdateVideo(pPlayer);

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
        if (pPlayer != NULL) {
            DBG_Print("pPlayer != NULL\n");
            Video_Stop();
            Video_Close();
        }
        DBG_Print("PlayNextContent Step0 End\n");
        PlayStep = 1; // プレーヤーの初期化は次のステップで行う
        break;

    case 1:
        DBG_Print("PlayNextContent Step1 Start\n");
        hr = MFPCreateMediaPlayer(NULL, FALSE, 0, NULL, hVideoChild, &pPlayer);
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
            static int retryCount = 0;
            DBG_Print("PlayNextContent Step3 (State=MFP_MEDIAPLAYER_STATE_EMPTY) End\n");
            if (retryCount < 100) {
                retryCount++;
            } else {
				retryCount = 0;
                DBG_Print("PlayNextContent Step3 Err End after retries\n");
                IsCancel = TRUE;
			}
            break;
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
        SetActiveWindow(hMainWnd);
        BringWindowToTop(hMainWnd);  /* 最前面に移動 */
        SetFocus(hMainWnd);
        if (Video_GetState() == MFP_MEDIAPLAYER_STATE_PLAYING) {
            PlayStep = 6; // ステップを一つ飛ばす
        } else {
            DBG_Print("PlayNextContent Step4 State is not PLAYING\n");
            PlayStep = 5; // 次のステップへ
        }
        DBG_Print("PlayNextContent Step4 End\n");
        break;

	case 5:
		DBG_Print("PlayNextContent Step5 Start\n");
        if (pPlayer == NULL) {
            DBG_Print("PlayNextContent Step5 End pPlayer == NULL\n");
            IsCancel = TRUE;
            break;
        }
        switch ( Video_GetState()) {
            case MFP_MEDIAPLAYER_STATE_PLAYING:
                PlayStep = 6; // 次のステップへ
                DBG_Print("PlayNextContent Step5 End : MFP_MEDIAPLAYER_STATE_PLAYING\n");
                break;
			case MFP_MEDIAPLAYER_STATE_STOPPED:
                DBG_Print("PlayNextContent Step5 End : MFP_MEDIAPLAYER_STATE_STOPPED\n");
                break;
            default:
                DBG_Print("PlayNextContent Step5 End : other\n");
                IsCancel = TRUE;
                break;
        }
        break;

    case 6:
        DBG_Print("PlayNextContent Step6 Start\n");
        if (pPlayer == NULL) {
            DBG_Print("PlayNextContent Step6 End pPlayer == NULL\n");
            IsCancel = TRUE;
            break;
        }
        if (Video_GetState() != MFP_MEDIAPLAYER_STATE_PLAYING) {
            DBG_Print("PlayNextContent Step6 End State is not PLAYING\n");
            IsCancel = TRUE;
            break;
		}
        DBG_Print("PlayNextContent Step6 End\n");
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
	static HBRUSH hBrushBlack; // 動画子ウィンドウ用の黒ブラシ


    HDC hDc;
    PAINTSTRUCT ps;
    static HICON hicon = NULL;

    switch (uMsg) {
    case WM_PAINT:
        if (hwnd != hMainWnd && !IsSmall) {
			// 2ndy以降のウィンドウは黒で塗りつぶす
			hDc = BeginPaint(hwnd, &ps);
			FillRect(hDc, &ps.rcPaint, hBrushBlack);
            EndPaint(hwnd, &ps);
            break;
		}
        if (IsSmall) {
            hDc = BeginPaint( hwnd, &ps );
            if (hicon) {
                DrawIcon(hDc, 0, 0, hicon);
            }
            EndPaint( hwnd , &ps );
			IsEnd = TRUE;
        } else {
            hDc = BeginPaint(hwnd, &ps);

            // MFPlayに描画を更新させる
            if (pPlayer) {
                IMFPMediaPlayer_UpdateVideo(pPlayer);
            }

            EndPaint(hwnd, &ps);
            break;
        }

        break;

    case WM_CREATE:
        hicon = LoadIconA(hMainInstance, MAKEINTRESOURCEA(IDI_ICON1));
        {
		    RECT stRect;
            long lWidth;
            GetWindowRect(hwnd, &stRect);
            lWidth = stRect.right - stRect.left;
            if (lWidth < 400) {
                IsSmall = TRUE;
                IsEnd = TRUE;
                hMainWnd = hwnd;
                break;
            }
        }
        {
            // 現在のウィンドウが存在するモニターのハンドルを取得
            HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi;
            mi.cbSize = sizeof(MONITORINFO);
            if (GetMonitorInfo(hMonitor, &mi)) {
                // プライマリモニターかどうかの判定
                if (mi.dwFlags & MONITORINFOF_PRIMARY) {
                    char buf[128];
                    RECT stPrimaryRect = mi.rcMonitor;

                    // プライマリ画面サイズを取得
                    stScreenSize.lHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
					stScreenSize.lWidth = mi.rcMonitor.right - mi.rcMonitor.left;   
                    sprintf_s(buf, sizeof(buf), "Primary monitor detected(%08X): width=%ld, height=%ld\n", (DWORD)hwnd, stScreenSize.lWidth, stScreenSize.lHeight);
					DBG_Print(buf);

                    // スクリーン座標(全体)を、hMainWnd内のクライアント座標(相対)に変換する
                    MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&stPrimaryRect, 2);
					lPrimaryOffsetX = stPrimaryRect.left;
					lPrimaryOffsetY = stPrimaryRect.top;
                    sprintf_s(buf, sizeof(buf), "Primary monitor offset: X=%ld, Y=%ld\n", lPrimaryOffsetX, lPrimaryOffsetY);
                    DBG_Print(buf);

                } else {
					// プライマリモニターでない場合はWM_CREATEを抜ける　（ここは通らないが念のため）
                    char buf[128];
                    sprintf_s(buf, sizeof(buf), "Secondry monitor detected(%08X)\n", (DWORD)hwnd);
                    DBG_Print(buf);
                    break;
                }
            } else {
                // モニター情報の取得に失敗した場合はWM_CREATEを抜ける
                DBG_Print("GetMonitorInfo Fail\n");
                IsSmall = TRUE;
                IsEnd = TRUE;
                break;
			}
        }
        IniFileCreate();
        IsFirst = TRUE;
        hMainWnd = hwnd;
        hVideoChild = CreateWindowEx( 0, TEXT("STATIC"), NULL,
                        WS_CHILD | WS_VISIBLE ,                         // 子ウィンドウとして表示
                        lPrimaryOffsetX, lPrimaryOffsetY, stScreenSize.lWidth, stScreenSize.lHeight,   // 位置とサイズ
                        hMainWnd,                                       // 親ウィンドウのハンドル
                        (HMENU)101, hMainInstance, NULL );
        if (hVideoChild == NULL) {
            IsSmall = TRUE;
            IsEnd = TRUE;
        }
        hBrushBlack = CreateSolidBrush(RGB(0, 0, 0)); // 動画子ウィンドウ用の黒ブラシ
        LoadStringA(hMainInstance, IDS_SECTION_NAME, strSection, (int)sizeof(strSection));
        LoadStringA(hMainInstance, IDS_KEY_NAME, strKey, (int)sizeof(strKey));
        GetPrivateProfileStringA(strSection, strKey, "", strFileNames, (DWORD)sizeof(strFileNames), strIniFile);
        j = (int)strlen(strFileNames);
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
		uIntValue = GetPrivateProfileIntA(strSection, "INTERVAL", 30, strIniFile) * 1000;  // 秒 → ミリ秒
        if (!IsNoContents) {
            uTimer = SetTimer(hwnd, 1, INT_TIMEOUT, NULL);
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
                uTimerCount += INT_TIMEOUT;
                if (uTimerCount >= uIntValue) { // 30秒ごとに表示位置を変える
                    uTimerCount = 0;
                    nMauseMoveCount = 0;
                    if (uIntValue != 0)
                        MoveVideoWindow(FALSE, TRUE);
                }
                PlayNextContent(FALSE);
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
        if (hwnd != hMainWnd) {
            // 2ndy以降のウィンドウでは処理しない
            break;
        }
        if (IsTimeInt)      // タイマ処理中は無視 v1.71
            return FALSE;
        IsEnd = TRUE;
        break;

    case WM_KEYDOWN:        //  Terminates the screen saver.
        if (hwnd != hMainWnd) {
            // 2ndy以降のウィンドウでは処理しない
            break;
        }
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
                    pPlayer,
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
        if (hwnd != hMainWnd) {
            // 2ndy以降のウィンドウでは処理しない
            break;
        }
        nMauseMoveCount++;
        if (nMauseMoveCount > 10) {
            IsEnd = TRUE;
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
        break;

    case WM_MOUSEMOVE:
        if (hwnd != hMainWnd) {
            // 2ndy以降のウィンドウでは処理しない
            break;
        }
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
        if (hwnd != hMainWnd) {
            // 2ndy以降のウィンドウでは処理しない
            break;
        }
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
        Video_Stop();
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
    case WM_CTLCOLORSTATIC:
        {
            hDc = (HDC)wParam;
            SetBkColor(hDc, RGB(0, 0, 0));   // 背景色を黒に
            SetTextColor(hDc, RGB(255, 255, 255)); // 文字色（必要なら）
            return (LRESULT)hBrushBlack;     // 黒ブラシを返す
        }

    }
    return DefScreenSaverProc(hwnd, uMsg, wParam, lParam);
}

/* 登録用（古い署名から標準署名へ変更） */
BOOL WINAPI RegisterDialogClasses(HINSTANCE hInst)
{
    hMainInstance = hInst;
    LANGID lid = GetUserDefaultUILanguage();
    // 日本語以外なら英語（ENU）を優先するようにスレッド言語を設定
    if (PRIMARYLANGID(lid) == LANG_JAPANESE) {
        SetThreadUILanguage(MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN));
    }
    else {
        SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    }
    return TRUE;
}
