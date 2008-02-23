#include "config.hpp"

#include <windows.h>

EXEConfiguration config;

/* Reads a DWORD from the specified value in the specified registry key.
 * If the value exists and has the DWORD type, it is written to the variable pointed to by out and true is returned.
 * Otherwise, nothing happens and false is returned.
 */
bool readDWord(HKEY key, LPCWSTR valueName, LPDWORD out) {
	DWORD type;
	DWORD data;
	DWORD size = sizeof(data);
	if (RegQueryValueEx(key, valueName, NULL, &type, (LPBYTE)&data, &size) == ERROR_SUCCESS) {
		if (type == REG_DWORD && size == sizeof(data)) {
			*out = data;
			return true;
		}
	}
	return false;
}

void readConfigFromRegistry(DLLConfiguration *dllconfig, EXEConfiguration *execonfig) {
	// Open the registry keys.
	HKEY softwareKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software", 0, KEY_READ, &softwareKey) == ERROR_SUCCESS) {
		HKEY appKey;
		if (RegOpenKeyEx(softwareKey, L"Taekwindow", 0, KEY_READ, &appKey) == ERROR_SUCCESS) {
			HKEY versionKey;
			// We'll only change the version number of the key once the registry structure is no longer backwards compatible.
			// That is, once newer versions can no longer interpret the settings of an older version as if the settings were their own.
			if (RegOpenKeyEx(appKey, L"0.2", 0, KEY_READ, &versionKey) == ERROR_SUCCESS) {
				// Read stuff for the DLL.
				readDWord(versionKey, L"modifier"    , (LPDWORD)&dllconfig->modifier    );
				readDWord(versionKey, L"moveButton"  , (LPDWORD)&dllconfig->moveButton  );
				readDWord(versionKey, L"resizeButton", (LPDWORD)&dllconfig->resizeButton);
				readDWord(versionKey, L"resizeMode"  , (LPDWORD)&dllconfig->resizeMode  );
				// Read stuff for the EXE.
				readDWord(versionKey, L"systemTrayIcon", (LPDWORD)&execonfig->systemTrayIcon);
				// Close the keys again.
				RegCloseKey(versionKey);
			}
			RegCloseKey(appKey);
		}
		RegCloseKey(softwareKey);
	}
}
