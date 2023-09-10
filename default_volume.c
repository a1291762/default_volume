#include <windows.h>
#include <stdio.h>
#include <shlwapi.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <psapi.h>

extern void dbglog(const char *format, ...);
extern void wdbglog(const TCHAR *format, ...);

#define AUDCLNT_S_NO_SINGLE_PROCESS AUDCLNT_SUCCESS (0x00d)

LSTATUS get_volume_control(TCHAR *executable_name, ISimpleAudioVolume **volume) {
	LSTATUS err = -1;

	HRESULT hr;
	IMMDeviceEnumerator *device_enumerator = NULL;
	IMMDevice *speakers = NULL;
	IAudioSessionManager2 *session_manager = NULL;
	IAudioSessionEnumerator *session_enumerator = NULL;
	IAudioSessionControl *session_control = NULL;
	IAudioSessionControl2 *session_control_2 = NULL;

	// device_enumerator
	hr = CoCreateInstance(
			&CLSID_MMDeviceEnumerator, NULL,
			CLSCTX_ALL, &IID_IMMDeviceEnumerator,
			(void**)&device_enumerator);
	if (hr != S_OK) {
		dbglog("Failed to CoCreateInstance %lx\n", hr);
		goto cleanup;
	}

	// speakers
	hr = device_enumerator->lpVtbl->GetDefaultAudioEndpoint(device_enumerator, eRender, eMultimedia, &speakers);
	if (hr != S_OK) {
		dbglog("Failed to GetDefaultAudioEndpoint %lx\n", hr);
		goto cleanup;
	}

	// manager
	hr = speakers->lpVtbl->Activate(speakers, &IID_IAudioSessionManager2, CLSCTX_ALL, NULL, (void**)&session_manager);
	if (hr != S_OK) {
		dbglog("Failed to Activate %lx\n", hr);
		goto cleanup;
	}
	
	// session_enumerator
	hr = session_manager->lpVtbl->GetSessionEnumerator(session_manager, &session_enumerator);
	if (hr != S_OK) {
		dbglog("Failed to GetSessionEnumerator %lx\n", hr);
		goto cleanup;
	}

	int count;
	hr = session_enumerator->lpVtbl->GetCount(session_enumerator, &count);
	if (hr != S_OK) {
		dbglog("Failed to GetCount %lx\n", hr);
		goto cleanup;
	}

	// search for an audio session matching the executable name
	for (int i = 0; i < count; i++) {
		hr = session_enumerator->lpVtbl->GetSession(session_enumerator, i, &session_control);
		if (hr != S_OK) {
			dbglog("Failed to GetSession %lx\n", hr);
			continue;
		}

		hr = session_control->lpVtbl->QueryInterface(session_control, &IID_IAudioSessionControl2, (void**)&session_control_2);
		if (hr != S_OK) {
			dbglog("Failed to QueryInterface\n");
			goto cleanup;
		}

		// display name is not reliable...
		DWORD pid = 0;
		hr = session_control_2->lpVtbl->GetProcessId(session_control_2, &pid);
		if (hr != S_OK && hr != AUDCLNT_S_NO_SINGLE_PROCESS) {
			dbglog("Failed to GetProcessId %lx\n", hr);
			goto cleanup;
		}

		if (pid != 0) {
			dbglog("Audio Session PID %ld\n", pid);

			TCHAR process_filename[200] = {0};
			HANDLE h = OpenProcess(READ_CONTROL|PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, pid);
			if (!h) {
				dbglog("Failed to OpenProcess\n");
				goto cleanup;
			}
			DWORD len = GetModuleFileNameEx(h, NULL, process_filename, sizeof(process_filename));
			if (!CloseHandle(h)) {
				dbglog("Failed to CloseHandle\n");
				goto cleanup;
			}

			if (len) {
				if (wcsstr(executable_name, &process_filename[2]) != NULL) {
					wdbglog(L"Process filename matches registry %ls\n", &process_filename[2]);
					// return the volume interface
					session_control->lpVtbl->QueryInterface(session_control, &IID_ISimpleAudioVolume, (void**)volume);
					err = ERROR_SUCCESS;
					goto cleanup;
				}
			}
		}

		session_control_2->lpVtbl->Release(session_control_2);
		session_control_2 = NULL;
		session_control->lpVtbl->Release(session_control);
		session_control = NULL;
	}

cleanup:

	if (session_control_2) session_control_2->lpVtbl->Release(session_control_2);
	if (session_control) session_control->lpVtbl->Release(session_control);
	if (session_enumerator) session_enumerator->lpVtbl->Release(session_enumerator);
	if (session_manager) session_manager->lpVtbl->Release(session_manager);
	if (device_enumerator) device_enumerator->lpVtbl->Release(device_enumerator);

	return err;
}


LSTATUS set_volume(HKEY property) {
	LSTATUS err;

	// Get the executable name from this long string
	TCHAR value[1000] = {0};
	DWORD len = sizeof(value);
	err = RegGetValue(property, NULL, L"", RRF_RT_REG_SZ, NULL, value, &len);
	if (err != ERROR_SUCCESS) {
		dbglog("Failed to RegGetValue\n");
		return err;
	}
	TCHAR *context;
	TCHAR *token;
	token = wcstok(value, L"|", &context);
	token = wcstok(NULL, L"|", &context);
	TCHAR *executable_name = wcstok(token, L"%", &context);

	ISimpleAudioVolume *volume = NULL;
	err = get_volume_control(executable_name, &volume);
	if (err != ERROR_SUCCESS) {
		dbglog("Failed to get_volume_control\n");
		return err;
	}

	int percent = GetPrivateProfileInt(L"Default", L"Volume", 40, L".\\default_volume.ini");
	if (percent <= 0 || percent > 100) {
		percent = 40;
	}
	HRESULT hr = volume->lpVtbl->SetMasterVolume(volume, percent/100.0f, NULL);
	if (hr != S_OK) {
		dbglog("Failed to SetMasterVolume %lx\n", hr);
	}
	dbglog("Set the volume to %d%%\n", percent);

	volume->lpVtbl->Release(volume);

	return ERROR_SUCCESS;
}

LSTATUS handle_new_property(HKEY property_store, TCHAR *name) {
	LSTATUS err;

	HKEY property;
	err = RegOpenKeyEx(property_store, name, 0, KEY_READ, &property);
	if (err != ERROR_SUCCESS) {
		dbglog("Failed to RegOpenKeyEx\n");
		return err;
	}

	HKEY volume_store = {0};
	err = RegOpenKeyEx(property, L"{219ED5A0-9CBF-4F3A-B927-37C9E5C5F14F}", 0, KEY_READ, &volume_store);
	if (err == ERROR_SUCCESS) {
		dbglog("Property has volume settings\n");
		goto cleanup;
	}

	set_volume(property);

	err = ERROR_SUCCESS;
cleanup:
	if (volume_store) {
		err = RegCloseKey(volume_store);
		if (err != ERROR_SUCCESS) {
			dbglog("Failed to RegCloseKey\n");
		}
	}

	err = RegCloseKey(property);
	if (err != ERROR_SUCCESS) {
		dbglog("Failed to RegCloseKey\n");
	}

	return err;
}

LSTATUS get_last_modified(HKEY parent_key, TCHAR *sub_key, ULARGE_INTEGER *last_modified) {
	LSTATUS err;
	HKEY hkey = {0};
	
	if (sub_key) {
		err = RegOpenKeyEx(parent_key, sub_key, 0, KEY_READ, &hkey);
		if (err != ERROR_SUCCESS) {
			dbglog("Failed to RegOpenKeyEx\n");
			return err;
		}
	} else {
		hkey = parent_key;
	}

	FILETIME ft;
	err = RegQueryInfoKey(hkey, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &ft);
	if (err != ERROR_SUCCESS) {
		dbglog("Failed to RegQueryInfoKey\n");
		goto cleanup;
	}

	last_modified->u.HighPart = ft.dwHighDateTime;
	last_modified->u.LowPart = ft.dwLowDateTime;

	err = ERROR_SUCCESS;
cleanup:
	if (sub_key && hkey) {
		err = RegCloseKey(hkey);
		if (err != ERROR_SUCCESS) {
			dbglog("Failed to RegCloseKey\n");
		}
	}

	return err;
}

int monitor_registry(HKEY *g_property_store) {
	int ret = 1;

	HKEY property_store = NULL;
	LSTATUS err = RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Internet Explorer\\LowRegistry\\Audio\\PolicyConfig\\PropertyStore",
			0, KEY_READ|KEY_NOTIFY, &property_store);
	if (err != ERROR_SUCCESS) {
		dbglog("Failed to RegOpenKeyEx\n");
		goto cleanup;
	}
	*g_property_store = property_store;

	dbglog("Waiting for registry changes...\n");
	for (;;) {
		ULARGE_INTEGER property_last_modified;
		err = get_last_modified(property_store, NULL, &property_last_modified);
		if (err != ERROR_SUCCESS) {
			dbglog("Failed to get_last_modified\n");
			goto cleanup;
		}

		err = RegNotifyChangeKeyValue(property_store, TRUE,
				REG_NOTIFY_CHANGE_NAME|REG_NOTIFY_CHANGE_ATTRIBUTES|REG_NOTIFY_CHANGE_LAST_SET,
				NULL, FALSE);
		if (err != ERROR_SUCCESS) {
			dbglog("Failed to RegNotifyChangeKeyValue\n");
			goto cleanup;
		}
		dbglog("The registry changed!\n");

		DWORD sub_key_count;
		err = RegQueryInfoKey(property_store, NULL, NULL, NULL, &sub_key_count, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		if (err != ERROR_SUCCESS) {
			dbglog("Failed to RegQueryInfoKey\n");
			goto cleanup;
		}

		// Just look at every property that is newer than the parent was
		for (DWORD i = 0; i < sub_key_count; i++) {
			TCHAR name[200];
			err = RegEnumKey(property_store, i, name, sizeof(name));
			if (err != ERROR_SUCCESS) {
				dbglog("Failed to RegEnumKey\n");
				goto cleanup;
			}

			ULARGE_INTEGER sub_last_modified;
			err = get_last_modified(property_store, name, &sub_last_modified);
			if (err != ERROR_SUCCESS) {
				dbglog("Failed to get_last_modified\n");
				goto cleanup;
			}

			if (sub_last_modified.QuadPart > property_last_modified.QuadPart) {
				wdbglog(L"Found newer sub-key %ls\n", name);

				err = handle_new_property(property_store, name);
				if (err != ERROR_SUCCESS) {
					dbglog("Failed to handle_new_property\n");
					goto cleanup;
				}
			}
		}
	}

	ret = 0;
cleanup:
	if (property_store) {
		err = RegCloseKey(property_store);
		*g_property_store = NULL;
		if (err != ERROR_SUCCESS) {
			dbglog("Failed to RegCloseKey\n");
			return 1;
		}
	}

	return ret;
}
