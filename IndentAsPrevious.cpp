#include <shlwapi.h>
#include <strsafe.h>
#include <shlobj.h>

#include "IndentAsPrevious.h"

const TCHAR NPP_PLUGIN_NAME[] = TEXT("IndentAsPrevious");

const __int3264 nbFunc = 1;
FuncItem funcItem[nbFunc];

NppData nppData;

bool enable = true;

TCHAR iniFilePath[MAX_PATH];
bool iniSucceed;

void pluginInit(HANDLE hModule)
{
	if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, iniFilePath)))
	{
		return;
	}
	if (!PathAppend(iniFilePath, TEXT("Notepad++\\plugins\\config")))
	{
		return;
	}
	if (!PathAppend(iniFilePath, NPP_PLUGIN_NAME))
	{
		return;
	}
	if (!SUCCEEDED(StringCchCat(iniFilePath, MAX_PATH, TEXT(".ini"))))
	{
		return;
	}

	enable = GetPrivateProfileInt(TEXT("Settings"), TEXT("Enable"), 0, iniFilePath) != 0 ? true : false;
	iniSucceed = true;
}

void pluginCleanUp()
{
	if (iniSucceed)
	{
		WritePrivateProfileString(TEXT("Settings"), TEXT("Enable"), enable ? TEXT("1") : TEXT("0"), iniFilePath);
	}
}

void commandMenuInit()
{
	setCommand(0, TEXT("Enable"), toggle, NULL, enable);
}

void commandMenuCleanUp()
{
}

bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool checkOnInit) 
{
	if ((index >= nbFunc) || (!pFunc))
	{
		return false;
	}

	StringCchCopy(funcItem[index]._itemName, sizeof(funcItem[index]._itemName)/sizeof(TCHAR), cmdName);
	funcItem[index]._pFunc = pFunc;
	funcItem[index]._init2Check = checkOnInit;
	funcItem[index]._pShKey = sk;

	return true;
}

void basicAutoIndent()
{
	if (enable)
	{
		HWND curScintilla = getCurrentScintilla();

		SendMessage(curScintilla, SCI_BEGINUNDOACTION, 0, 0);

		/* Get position and line number */
		__int3264 position  = SendMessage(curScintilla, SCI_GETCURRENTPOS, 0, 0);
		__int3264 line_number = SendMessage(curScintilla, SCI_LINEFROMPOSITION, position, 0);

		/* Get current line start and indent positions */
		__int3264 line_start = SendMessage(curScintilla, SCI_POSITIONFROMLINE, line_number, 0);
		__int3264 line_indent = SendMessage(curScintilla, SCI_GETLINEINDENTPOSITION, line_number, 0);

		/* Delete beginning indent on the new line (if exist) */
		if ((line_indent - line_start) != 0)
		{
			SendMessage(curScintilla, SCI_SETSEL, line_start, line_indent);
			SendMessage(curScintilla, SCI_REPLACESEL, 0, (LPARAM)&"");
		}

		__int3264 prevline = line_number - 1;

		/* Get previous line start and indent positions */
		__int3264 prevline_start = SendMessage(curScintilla, SCI_POSITIONFROMLINE, prevline, 0);
		__int3264 prevline_indent = SendMessage(curScintilla, SCI_GETLINEINDENTPOSITION, prevline, 0);

		/* Get previous indentation as is */
		char prev_indent[4096];
		SendMessage(curScintilla, SCI_SETSEL, prevline_start, prevline_indent);
		SendMessage(curScintilla, SCI_GETSELTEXT, 0, (LPARAM)&prev_indent);

		/* Duplicate indentation on new line */
		SendMessage(curScintilla, SCI_INSERTTEXT, line_start, (LPARAM)&prev_indent);

		/* Place cursor at end of new indentation */
		__int3264 new_position = line_start + (prevline_indent - prevline_start);
		SendMessage(curScintilla, SCI_SETSEL, new_position, new_position);

		SendMessage(curScintilla, SCI_ENDUNDOACTION, 0, 0);
	}
}

void toggle()
{
	enable = !enable;

	HWND curScintilla = getCurrentScintilla();
	HMENU hMenu = GetMenu(nppData._nppHandle);

	if (hMenu)
	{
		CheckMenuItem(hMenu, funcItem[0]._cmdID, MF_BYCOMMAND | (enable ? MF_CHECKED : MF_UNCHECKED));
	}
}

bool isCharLineEnd(HWND curScintilla, int ch)
{
	__int3264 eol_mode = SendMessage(curScintilla, SCI_GETEOLMODE, 0, 0);

	switch (eol_mode)
	{
		case SC_EOL_CRLF:
		case SC_EOL_LF:
			return ch == '\n';
		case SC_EOL_CR:
			return ch == '\r';
	}

	return false;
}

HWND getCurrentScintilla()
{
	__int3264 which = -1;

	SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);

	if (which == -1)
	{
		return NULL;
	}

	if (which == 0)
	{
		return nppData._scintillaMainHandle;
	}
	else
	{
		return nppData._scintillaSecondHandle;
	}

	return nppData._scintillaMainHandle;
}

// DLL exported functions

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  reasonForCall, LPVOID lpReserved)
{
	switch (reasonForCall)
	{
		case DLL_PROCESS_ATTACH:
			pluginInit(hModule);
			break;

		case DLL_PROCESS_DETACH:
			pluginCleanUp();
			break;

		case DLL_THREAD_ATTACH:
			break;

		case DLL_THREAD_DETACH:
			break;
	}

	return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData)
{
	nppData = notpadPlusData;
	commandMenuInit();
}

extern "C" __declspec(dllexport) const TCHAR * getName()
{
	return NPP_PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem * getFuncsArray(int *nbF)
{
	*nbF = nbFunc;
	return funcItem;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification *notifyCode)
{
	switch (notifyCode->nmhdr.code) 
	{
		case SCN_CHARADDED:
		{
			if (isCharLineEnd(getCurrentScintilla(), notifyCode->ch))
			{
				basicAutoIndent();
			}
			break;
		}

		case NPPN_SHUTDOWN:
		{
			commandMenuCleanUp();
			break;
		}

		default:
			return;
	}
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

#ifdef UNICODE
extern "C" __declspec(dllexport) BOOL isUnicode()
{
	return TRUE;
}
#endif

