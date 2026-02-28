/*
 *	スクリーンセーバ（動画再生スクリーンセーバー）
 *
 *	オリジナル：VC6 / Windows XP 向け
*	更新：v143/VS2022向けに修正 (ANSI API 明示、標準関数へ置換、関数プロトタイプ修正)
 *
 *	(c) 1997-2025,BearBeetle
 *
 */
#include <windows.h>
#include <SCRNSAVE.H>
#include <mmsystem.h>
#include <shlobj.h> // V2
#include <COMMCTRL.H>
#include <direct.h> // V2 for _chdir
#include <stdio.h>
#include <string.h>
#include "resource.h"

char strVideoFilter[400];	// 動画フィルタ文字列
char strFileNames[32000];
char strIniFile[MAX_PATH];  // V2
extern HINSTANCE hMainInstance; /* screen saver instance handle  */

#define INT_TIME_MAX	600	// 表示間隔最大値[s]
#define	INT_TIME_MIN	0	// 表示間隔最小値[s] ※０のときは中心固定
#define END_TIME_MAX	600	// 終了時間最大[分]
#define	END_TIME_MIN	0	// 終了時間最小[分] ※０のときは終了せず

/*
 * INIファイルパスをサーチ／作成するルーチン
 * 2018/05/22(V2で追加)
 */
void IniFileCreate()
{
	static char strCurrentDir[MAX_PATH];
	/* カレントディレクトリ */
	GetCurrentDirectoryA(sizeof(strCurrentDir), strCurrentDir);
	/* VS2.INIファイル */
	SHGetSpecialFolderPathA(0, strIniFile, CSIDL_LOCAL_APPDATA, TRUE);
	strcat_s(strIniFile, sizeof(strIniFile), "\\BearBeetle");
	if (_chdir(strIniFile) != 0) {
		_mkdir(strIniFile);
	}
	strcat_s(strIniFile, sizeof(strIniFile), "\\VS2");
	if (_chdir(strIniFile) != 0) {
		_mkdir(strIniFile);
	}
	strcat_s(strIniFile, sizeof(strIniFile), "\\VS2.INI");
	/* カレントディレクトリを元に戻す */
	SetCurrentDirectoryA(strCurrentDir);
}

/*
 * | -> \0
 * @ -> \0+end
 * 文字列変換ユーティリティ（オリジナルの意図を維持）
 */
void Change0(char *strDat)
{
	int i;
	for (i = 0; i < 500; i++) {
		if (strDat[i] == '|') {
			strDat[i] = '\0';
			if (strDat[i + 1] == '|') {
				strDat[i + 1] = '\0';
				break;
			}
		}
	}
}

// レジストリからMedia Playerの拡張子リストを取得する関数
void GetWMPVideoFilter(char* outFilter, size_t bufferSize) {
	HKEY hParentKey;
	const char* subkeyPath = "SOFTWARE\\Microsoft\\MediaPlayer\\Player\\Extensions\\Types";

	outFilter[0] = '\0'; // 初期化

	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subkeyPath, 0, KEY_READ | KEY_WOW64_64KEY, &hParentKey) == ERROR_SUCCESS) {
		DWORD index = 0;
		char valueName[256];
		DWORD valueNameSize;
		BYTE valueData[1024];
		DWORD valueDataSize;
		DWORD valueType;
		LSTATUS sts;

		while (1) {
			valueNameSize = sizeof(valueName);
			valueDataSize = sizeof(valueData);

			sts = RegEnumValueA(hParentKey, index, valueName, &valueNameSize, NULL, &valueType, valueData, &valueDataSize);

			if (sts != ERROR_SUCCESS) {
				break; // 259 (ERROR_NO_MORE_ITEMS) が出たら終了
			}

			// 文字列データ (REG_SZ) の場合のみ処理
			if (valueType == REG_SZ) {
				if (outFilter[0] != '\0') {
					strcat_s(outFilter, bufferSize, ";");
				}
				strcat_s(outFilter, bufferSize, (char*)valueData);
			}
			index++;
		}
		RegCloseKey(hParentKey);
	}

	// 万が一取得できなかった場合のフォールバック（最低限のセット）
	if (outFilter[0] == '\0') {
		strcpy_s(outFilter, bufferSize, "*.mp4;*.avi;*.wmv;*.mov;*.mkv");
	}
}

BOOL AddFile(HWND hSSCD, char *strFileName[], int *iCount)
{
	// OPENFILE ダイアログで複数選択可能にする
	char strFilter[500];
	int i, i2, iEnd;
	OPENFILENAMEA stFile;

	strFileNames[0] = 0;
	memset(&stFile, 0, sizeof(stFile));
	stFile.lStructSize = sizeof(OPENFILENAMEA);
	stFile.hwndOwner = hSSCD;
	stFile.lpstrCustomFilter = NULL;
	// 動画フィルタは事前に作成された strVideoFilter を使う
	GetWMPVideoFilter(strVideoFilter, sizeof(strVideoFilter));
	sprintf_s(strFilter, sizeof(strFilter), "動画ファイル|%s|All Files (*.*)|*.*||", strVideoFilter);
	Change0(strFilter);
	stFile.lpstrFilter = strFilter;
	stFile.nFilterIndex = 0;
	stFile.lpstrFile = strFileNames;
	stFile.nMaxFile = (DWORD)sizeof(strFileNames);
	stFile.lpstrFileTitle = NULL;
	stFile.lpstrInitialDir = NULL;
	stFile.lpstrTitle = NULL;
	stFile.Flags = OFN_ALLOWMULTISELECT | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_EXPLORER;
	stFile.lpstrDefExt = NULL;

	// ファイル取得
	if (!GetOpenFileNameA(&stFile)) {
		return FALSE;
	}

	// strFileName 配列を作成
	strFileName[0] = strFileNames;
	iEnd = *iCount;
	i2 = 1;
	for (i = 1; i < (int)(sizeof(strFileNames) - 1); i++) {
		if (strFileNames[i] == '\0') {
			if (strFileNames[i + 1] == '\0')
				break;
			strFileName[i2] = &strFileNames[i + 1];
			i2++;
			if (i2 >= iEnd) {
				MessageBoxA(hSSCD, "ファイル数が多すぎます", NULL, MB_OK);
				return FALSE;
			}
		}
	}
	*iCount = i2;
	return TRUE;
}

/* 標準的なプロトタイプに修正 */
BOOL WINAPI ScreenSaverConfigureDialog(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char *strDat[500], strSize[100];
	int *lpIndexs;
	int i, j, k;
	char strSection[64], strKey[64], strSendDat[80], strDevName[80];
	static char *strSizeList[] = {
		"50%",
		"100%",
		"200%",
		"300%",
		"Full Screen",
		"Custum"
	};
	static HWND hwndEditSize;			// IDC_EDIT_SIZE のハンドル


	switch (uMsg) {

	case WM_INITDIALOG:
		IniFileCreate();
		LoadStringA(hMainInstance, IDS_SECTION_NAME, strSection, (int)sizeof(strSection));
		LoadStringA(hMainInstance, IDS_KEY_NAME, strKey, (int)sizeof(strKey));
		// 各設定を INI から読み取り�iANSI 明示�j
		CheckDlgButton(hwndDlg, IDC_CHECK_RAND, GetPrivateProfileIntA(strSection, "RAND", FALSE, strIniFile));
		CheckDlgButton(hwndDlg, IDC_CHECK_CONT, GetPrivateProfileIntA(strSection, "CONT", FALSE, strIniFile));
		CheckDlgButton(hwndDlg, IDC_CHECK_MUTE, GetPrivateProfileIntA(strSection, "MUTE", FALSE, strIniFile));
		CheckDlgButton(hwndDlg, IDC_CHECK_TITLE, GetPrivateProfileIntA(strSection, "F_INS", FALSE, strIniFile));

		// サイズコンボに文字列を追加�iANSI版を明示�j
		for (i = 0; i < 6; i++) {
			SendDlgItemMessageA(hwndDlg, IDC_COMBO_SIZE, CB_ADDSTRING, 0, (LPARAM)strSizeList[i]);
		}

		hwndEditSize = GetDlgItem(hwndDlg, IDC_EDIT_SIZE);
		i = GetPrivateProfileIntA(strSection, "SIZE", 100, strIniFile);
		switch (i) {
		case 0:	// Full Screen
			SendDlgItemMessageA(hwndDlg, IDC_COMBO_SIZE, CB_SETCURSEL, 4, 0);
			break;
		case 50:	// 50%
			SendDlgItemMessageA(hwndDlg, IDC_COMBO_SIZE, CB_SETCURSEL, 0, 0);
			break;
		case 100:	// 100%
			SendDlgItemMessageA(hwndDlg, IDC_COMBO_SIZE, CB_SETCURSEL, 1, 0);
			break;
		case 200:	// 200%
			SendDlgItemMessageA(hwndDlg, IDC_COMBO_SIZE, CB_SETCURSEL, 2, 0);
			break;
		case 300:	// 300%
			SendDlgItemMessageA(hwndDlg, IDC_COMBO_SIZE, CB_SETCURSEL, 3, 0);
			break;
		default:	// カスタム
			SendDlgItemMessageA(hwndDlg, IDC_COMBO_SIZE, CB_SETCURSEL, 5, 0);
			EnableWindow(hwndEditSize, TRUE);
			break;
		}
		// サイズ表示
		SetDlgItemInt(hwndDlg, IDC_EDIT_SIZE, i, FALSE);

		// インターバル読み込み
		i = GetPrivateProfileIntA(strSection, "INTERVAL", 30, strIniFile);
		if (i < INT_TIME_MIN) i = INT_TIME_MIN;
		if (i > INT_TIME_MAX) i = INT_TIME_MAX;
		SetDlgItemInt(hwndDlg, IDC_EDIT_INTERVAL, i, FALSE);

		// 終了時間読み込み
		i = GetPrivateProfileIntA(strSection, "ENDTIME", 0, strIniFile);
		if (i < END_TIME_MIN) i = END_TIME_MIN;
		if (i > END_TIME_MAX) i = END_TIME_MAX;
		SetDlgItemInt(hwndDlg, IDC_EDIT_ENDTIME, i, FALSE);

		// スピンコントロール設定
		{
			HWND hWndSpin;
			hWndSpin = GetDlgItem(hwndDlg, IDC_SPIN_INTERVAL);
			SendMessageA(hWndSpin, UDM_SETRANGE, 0L, MAKELONG(INT_TIME_MAX, INT_TIME_MIN));
			hWndSpin = GetDlgItem(hwndDlg, IDC_SPIN_ENDTIME);
			SendMessageA(hWndSpin, UDM_SETRANGE, 0L, MAKELONG(INT_TIME_MAX, INT_TIME_MIN));
		}

		// ファイルリスト取得
		GetPrivateProfileStringA(strSection, strKey, "|", strFileNames, (DWORD)sizeof(strFileNames), strIniFile);
#if 1	// 2026/02/07
		char *p = strFileNames;
		char* buf = (char*)malloc(MAX_PATH);

		while ((p = strchr(p, '|')) != NULL) {
			p++;  // '|' の次へ進む
			const char* q = strchr(p, '|');  // 次の '|' を探す
			if (q == NULL) break;

			size_t len = q - p;
			if (len > 0) {
				strncpy(buf, p, len);
				buf[len] = '\0';
				SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_ADDSTRING, 0, (LPARAM)buf);
			}
			//p = q + 1;  // 次の検索位置へ
		}
		free(buf);

#else
		if (strFileNames[0] == '|' || strFileNames[0] == 0)
			goto GET_FILE_EXE;
		strDat[0] = strFileNames;
		j = (int)strlen(strFileNames);
		for (i = 0; i < j; i++) {
			if (strFileNames[i] == '|'){
				(strFileNames[i - 1] >= ' ' && strFileNames[i - 1] <= 'z') &&
				(strFileNames[i - 1] != 0)) {
				strFileNames[i] = '\0';
				SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_ADDSTRING, 0, (LPARAM)strDat[0]);
				if (i < (j - 1)) {
					strDat[0] = &strFileNames[i + 1];
					i++;
				}
			}
		}
		GET_FILE_EXE:
#endif
		// MCI デバイス数取得
		if (mciSendStringA("Sysinfo all quantity", strFileNames, (UINT)sizeof(strFileNames), NULL) != 0) {
			MessageBoxA(hwndDlg, "MCI取得失敗", NULL, MB_OK);
			EndDialog(hwndDlg, LOWORD(wParam) == IDOK);
		}
		// デバイス列挙からフィルタ作成
		k = 0;
		j = atoi(strFileNames);
		strVideoFilter[0] = 0;
		for (i = 1; i <= j; i++) {
			sprintf_s(strSendDat, sizeof(strSendDat), "sysinfo all name %d ", i);
			mciSendStringA(strSendDat, strDevName, (UINT)sizeof(strDevName), NULL);
			if (_stricmp("ActiveMovie", strDevName) == 0 ||
				_stricmp("MPEGVideo", strDevName) == 0) {
				static BOOL IsFirst = TRUE;
				if (IsFirst) {
					IsFirst = FALSE;
					if (k != 0) {
						strVideoFilter[k] = ';';
						k++;
					}
					strcpy_s(&strVideoFilter[k], sizeof(strVideoFilter) - k, "*.mpg;*.mpeg;*.mpe");
					k += (int)strlen("*.mpg;*.mpeg;*.mpe");
				}
			}
			if (_stricmp("QTWVideo", strDevName) == 0 ||
				_stricmp("ActiveMovie", strDevName) == 0 ||
				_stricmp("MPEGVIDEO", strDevName) == 0) {
				static BOOL IsFirst2 = TRUE;
				if (IsFirst2) {
					IsFirst2 = FALSE;
					if (k != 0) {
						strVideoFilter[k] = ';';
						k++;
					}
					strcpy_s(&strVideoFilter[k], sizeof(strVideoFilter) - k, "*.mov");
					k += (int)strlen("*.mov");
				}
			}
			if (_stricmp("avivideo", strDevName) == 0) {
				static BOOL IsFirst3 = TRUE;
				if (IsFirst3) {
					IsFirst3 = FALSE;
					if (k != 0) {
						strVideoFilter[k] = ';';
						k++;
					}
					strcpy_s(&strVideoFilter[k], sizeof(strVideoFilter) - k, "*.avi");
					k += (int)strlen("*.avi");
				}
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BUTTON_ADD:
			j = 500;
			if (AddFile(hwndDlg, strDat, &j)) {
				if (j == 1) {
					SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_ADDSTRING, 0, (LPARAM)strDat[0]);
					break;
				}
				if (j < 2)
					break;
				for (i = 1; i < j; i++) {
					char strFileName[300];
					if (*(strDat[0] + strlen(strDat[0]) - 1) == '\\') {
						sprintf_s(strFileName, sizeof(strFileName), "%s%s", strDat[0], strDat[i]);
					}
					else {
						sprintf_s(strFileName, sizeof(strFileName), "%s\\%s", strDat[0], strDat[i]);
					}
					SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_ADDSTRING, 0, (LPARAM)strFileName);
				}
			}
			break;

		case IDC_BUTTON_DEL:
			lpIndexs = (LPINT)strFileNames;	// ワーク領域として流用
			// 選択数取得
			j = SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_GETSELCOUNT, 0, 0);
			// 選択インデックス取得
			j = SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_GETSELITEMS, j, (LPARAM)lpIndexs);
			// 削除（後ろから�j
			for (i = j - 1; i >= 0; i--) {
				SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_DELETESTRING, (WPARAM)lpIndexs[i], 0);
			}
			break;

		case IDC_BUTTON_DEL_ALL:
			SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_RESETCONTENT, 0, 0);
			break;

		case IDC_BUTTON_UP:
			lpIndexs = (LPINT)strFileNames;
			j = SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_GETSELCOUNT, 0, 0);
			j = SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_GETSELITEMS, j, (LPARAM)lpIndexs);
			for (i = 0; i < j; i++) {
				char strListName[MAX_PATH];
				k = lpIndexs[i] - 1;
				if (k < 0) break;
				SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_GETTEXT, (WPARAM)k, (LPARAM)strListName);
				SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_DELETESTRING, (WPARAM)k, 0);
				SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_INSERTSTRING, (WPARAM)(k + 1), (LPARAM)strListName);
			}
			break;

		case IDC_BUTTON_DOWN:
			lpIndexs = (LPINT)strFileNames;
			j = SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_GETSELCOUNT, 0, 0);
			j = SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_GETSELITEMS, j, (LPARAM)lpIndexs);
			j--;
			for (i = j; i >= 0; i--) {
				char strListName[MAX_PATH];
				k = lpIndexs[i] + 1;
				if (SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_GETTEXT, (WPARAM)k, (LPARAM)strListName) == LB_ERR) {
					break;
				}
				SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_DELETESTRING, (WPARAM)k, 0);
				SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_INSERTSTRING, (WPARAM)(k - 1), (LPARAM)strListName);
			}
			break;

		case IDOK:
			j = SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_GETCOUNT, 0, 0);
			strFileNames[0] = '|';
			k = 1;
			for (i = 0; i < j; i++) {
				// LB_GETTEXT で追記していく
				SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_GETTEXT, i, (LPARAM)&strFileNames[k]);
				k += SendDlgItemMessageA(hwndDlg, IDC_LIST_CONTENTS, LB_GETTEXTLEN, i, 0);
				strFileNames[k] = '|';
				k++;
				strFileNames[k] = 0;
			}
			// ファイルリスト保存
			LoadStringA(hMainInstance, IDS_SECTION_NAME, strSection, (int)sizeof(strSection));
			LoadStringA(hMainInstance, IDS_KEY_NAME, strKey, (int)sizeof(strKey));
			WritePrivateProfileStringA(strSection, strKey, strFileNames, strIniFile);

			// 各チェックボックス
			if (IsDlgButtonChecked(hwndDlg, IDC_CHECK_RAND))
				WritePrivateProfileStringA(strSection, "RAND", "1", strIniFile);
			else
				WritePrivateProfileStringA(strSection, "RAND", "0", strIniFile);

			if (IsDlgButtonChecked(hwndDlg, IDC_CHECK_CONT))
				WritePrivateProfileStringA(strSection, "CONT", "1", strIniFile);
			else
				WritePrivateProfileStringA(strSection, "CONT", "0", strIniFile);

			if (IsDlgButtonChecked(hwndDlg, IDC_CHECK_MUTE))
				WritePrivateProfileStringA(strSection, "MUTE", "1", strIniFile);
			else
				WritePrivateProfileStringA(strSection, "MUTE", "0", strIniFile);

			if (IsDlgButtonChecked(hwndDlg, IDC_CHECK_TITLE))
				WritePrivateProfileStringA(strSection, "F_INS", "1", strIniFile);
			else
				WritePrivateProfileStringA(strSection, "F_INS", "0", strIniFile);

			// サイズ
			i = SendDlgItemMessageA(hwndDlg, IDC_COMBO_SIZE, CB_GETCURSEL, 0, 0);
			if (i == 4)
				sprintf_s(strSize, sizeof(strSize), "0");
			else
				sprintf_s(strSize, sizeof(strSize), "%d", i);

			switch (i) {
			case 4:
				sprintf_s(strSize, sizeof(strSize), "0");
				break;
			case 0:
				sprintf_s(strSize, sizeof(strSize), "50");
				break;
			case 1:
				sprintf_s(strSize, sizeof(strSize), "100");
				break;
			case 2:
				sprintf_s(strSize, sizeof(strSize), "200");
				break;
			case 3:
				sprintf_s(strSize, sizeof(strSize), "300");
				break;
			default:
				j = GetDlgItemInt(hwndDlg, IDC_EDIT_SIZE, NULL, FALSE);
				if (j < 25) j = 25;
				if (j > 300) j = 300;
				sprintf_s(strSize, sizeof(strSize), "%d", j);
				break;
			}
			WritePrivateProfileStringA(strSection, "SIZE", strSize, strIniFile);

			// インターバル
			i = GetDlgItemInt(hwndDlg, IDC_EDIT_INTERVAL, NULL, FALSE);
			if (i < INT_TIME_MIN || i > INT_TIME_MAX) {
				MessageBoxA(hwndDlg, "間隔の値が不正です\n0～600 の範囲で指定してください", NULL, MB_OK);
				return FALSE;
			}
			sprintf_s(strSendDat, sizeof(strSendDat), "%d", i);
			WritePrivateProfileStringA(strSection, "INTERVAL", strSendDat, strIniFile);

			// 終了時間
			i = GetDlgItemInt(hwndDlg, IDC_EDIT_ENDTIME, NULL, FALSE);
			if (i < END_TIME_MIN || i > END_TIME_MAX) {
				MessageBoxA(hwndDlg, "終了時間の値が不正です\n0～600 の範囲で指定してください", NULL, MB_OK);
				return FALSE;
			}
			sprintf_s(strSendDat, sizeof(strSendDat), "%d", i);
			WritePrivateProfileStringA(strSection, "ENDTIME", strSendDat, strIniFile);

			WritePrivateProfileStringA(strSection, "POS", "0", strIniFile);
			WritePrivateProfileStringA(strSection, "CONPOS", "-1", strIniFile);

			// break は意図的に落とす（IDCANCEL へ�j
		case IDCANCEL:
			EndDialog(hwndDlg, IDOK);
			return TRUE;

		case IDC_COMBO_SIZE:
			if (LOWORD(wParam) == IDC_COMBO_SIZE && HIWORD(wParam) == 1) {
				int iGetSel, iInterval;
				iGetSel = SendDlgItemMessageA(hwndDlg, IDC_COMBO_SIZE, CB_GETCURSEL, 0, 0);
				if (iGetSel < 5) {
					EnableWindow(hwndEditSize, FALSE);
				}
				else {
					EnableWindow(hwndEditSize, TRUE);
				}
				if (iGetSel == 4) {	// フルスクリーン選択
					iInterval = GetDlgItemInt(hwndDlg, IDC_EDIT_INTERVAL, NULL, FALSE);
					if (iInterval != 0) {
						if (MessageBoxA(hwndDlg, "間隔を0に変更しますか？\n\n\"はい(Y)\" を選ぶと間隔が0になります", "フルスクリーン選択", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1) == IDYES) {
							SetDlgItemInt(hwndDlg, IDC_EDIT_INTERVAL, 0, FALSE);
						}
					}
				}
			}
			return FALSE;

		case IDC_BUTTON_COPYRIGHT:
			ShellExecuteA(hwndDlg, NULL, "http://hp.vector.co.jp/authors/VA011973/", NULL, NULL, SW_SHOW);
			return FALSE;

		case IDC_BUTTON_HELP:
			ShellExecuteA(hwndDlg, NULL, "http://hp.vector.co.jp/authors/VA011973/vs_help.htm", NULL, NULL, SW_SHOW);
			return FALSE;
		}
	}
	return FALSE;
}



