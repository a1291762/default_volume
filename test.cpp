#ifndef UNICODE
#define UNICODE
#endif 
#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <psapi.h>

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioSessionManager2 = __uuidof(IAudioSessionManager2);

LSTATUS get_sub_key_count(HKEY hkey, LPDWORD sub_key_count) {
	LSTATUS err = RegQueryInfoKey(hkey, NULL, NULL, NULL, sub_key_count, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	if (err != ERROR_SUCCESS) {
		printf("Failed to RegQueryInfoKey\n");
		return err;
	}
	return ERROR_SUCCESS;
}

LSTATUS get_sub_key_names(HKEY hkey, TCHAR ***names, DWORD sub_key_count) {
	if (*names) {
		for (int i = 0; (*names)[i]; i++) {
			free((*names)[i]);
		}
	}
	*names = (TCHAR**)realloc(*names, sizeof(TCHAR**) * sub_key_count + 1);
	for (DWORD i = 0; i < sub_key_count; i++) {
		TCHAR name[200];
		LSTATUS err = RegEnumKey(hkey, i, name, sizeof(name));
		if (err != ERROR_SUCCESS) {
			printf("Failed to RegEnumKey\n");
			return err;
		}
		(*names)[i] = wcsdup(name);
	}
	(*names)[sub_key_count] = NULL;
	return ERROR_SUCCESS;
}

LSTATUS GetVolumeObject(TCHAR *name, ISimpleAudioVolume **volumeControl) {
	LSTATUS err = -1;

	IMMDeviceEnumerator *enumerator = NULL;
	IAudioSessionManager2 *mgr = NULL;
	IAudioSessionEnumerator *sessionEnumerator = NULL;

	HRESULT hr;
	hr = CoInitialize(NULL);
	if (hr != S_OK) {
		printf("Failed to CoInitialize %lx\n", hr);
		goto cleanup;
	}
	hr = CoCreateInstance(
			CLSID_MMDeviceEnumerator, NULL,
			CLSCTX_ALL, IID_IMMDeviceEnumerator,
			(void**)&enumerator);
	if (hr != S_OK) {
		printf("Failed to CoCreateInstance %lx\n", hr);
		goto cleanup;
	}

	IMMDevice *speakers;
	enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &speakers);

	// activate the session manager. we need the enumerator
	hr = speakers->Activate(IID_IAudioSessionManager2, CLSCTX_ALL, NULL, (void**)&mgr);
	if (hr != S_OK) {
		printf("Failed to Activate\n");
		goto cleanup;
	}
	
	// enumerate sessions for on this device
	hr = mgr->GetSessionEnumerator(&sessionEnumerator);
	if (hr != S_OK) {
		printf("Failed to GetSessionEnumerator\n");
		goto cleanup;
	}

	int count;
	hr = sessionEnumerator->GetCount(&count);
	if (hr != S_OK) {
		printf("Failed to GetCount\n");
		goto cleanup;
	}

	// search for an audio session with the required name
	// NOTE: we could also use the process id instead of the app name (with IAudioSessionControl2)
	for (int i = 0; i < count; i++) {
		IAudioSessionControl *ctl = NULL;
		hr = sessionEnumerator->GetSession(i, &ctl);
		if (hr != S_OK) {
			printf("Failed to GetSession\n");
			goto cleanup;
		}
		// TCHAR *dn;
		// hr = ctl->GetDisplayName(&dn);
		// if (hr != S_OK) {
		// 	printf("Failed to GetDisplayName\n");
		// 	goto cleanup;
		// }
		// wprintf(L"Session %ls vs %ls\n", dn, name);
		// CoTaskMemFree(dn);

		IAudioSessionControl2 *ctl2;
		ctl->QueryInterface<IAudioSessionControl2>(&ctl2);
		DWORD pid;
		ctl2->GetProcessId(&pid);
		ctl2->Release();

		if (pid != 0) {
			wprintf(L"Session pid %ld\n", pid);
			HANDLE h = OpenProcess(READ_CONTROL|PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, pid);
			if (!h) {
				printf("Failed to OpenProcess\n");
				goto cleanup;
			}
			TCHAR filename[200] = {0};
			GetModuleFileNameEx(h, NULL, filename, sizeof(filename));
			wprintf(L"process filename %ls\n", filename);
			if (!CloseHandle(h)) {
				printf("Failed to CloseHandle\n");
				goto cleanup;
			}

			if (wcsstr(name, &filename[2]) != NULL) {
				ctl->QueryInterface<ISimpleAudioVolume>(volumeControl);
				ctl->Release();
				err = ERROR_SUCCESS;
				break;
			}
		}

		ctl->Release();
	}

cleanup:

	if (sessionEnumerator) sessionEnumerator->Release();
	if (mgr) mgr->Release();
	if (enumerator) enumerator->Release();

	return err;
}


LSTATUS handle_new_property(HKEY property_store, TCHAR *name) {
	LSTATUS err;
	ISimpleAudioVolume *volume = NULL;

	TCHAR value[1000] = {0};
	DWORD len = sizeof(value);
	err = RegGetValue(property_store, name, L"", RRF_RT_REG_SZ, NULL, value, &len);
	if (err != ERROR_SUCCESS) {
		printf("Failed to RegGetValue\n");
		goto cleanup;
	}
	wprintf(L"Value is %ls\n", value);

	TCHAR *context;
	TCHAR *token;
	token = wcstok(value, L"|", &context);
	//wprintf(L"token is %ls\n", token);
	token = wcstok(NULL, L"|", &context);
	//wprintf(L"token is %ls\n", token);
	token = wcstok(token, L"%", &context);
	//wprintf(L"token is %ls\n", token);

	err = GetVolumeObject(token, &volume);
	if (err != ERROR_SUCCESS) {
		printf("Failed to GetVolumeObject\n");
		goto cleanup;
	}

	printf("Set master volume to 0.4\n");
	HRESULT hr;
	hr = volume->SetMasterVolume(0.4, NULL);
	if (hr != S_OK) {
		printf("Failed to SetMasterVolume %lx\n", hr);
	}
	volume->Release();
	printf("release volume\n");


	// HKEY property;
	// err = RegOpenKeyEx(property_store, name, 0, KEY_READ|KEY_WRITE, &property);
	// if (err != ERROR_SUCCESS) {
	// 	printf("Failed to RegOpenKeyEx\n");
	// 	return err;
	// }

	// HKEY volume_store = {0};
	// err = RegCreateKey(property, L"{219ED5A0-9CBF-4F3A-B927-37C9E5C5F14F}", &volume_store);
	// if (err != ERROR_SUCCESS) {
	// 	printf("Failed to RegCreateKey\n");
	// 	goto cleanup;
	// }

	// BYTE data_for_3[24] = {0x4, 0, 0, 0, 0, 0, 0, 0, 
	// 		0xcd, 0xcc, 0xcc, 0x3e, 0, 0, 0, 0,
	// 		0, 0, 0, 0, 0, 0, 0, 0};
	// err = RegSetValueEx(volume_store, L"3", 0, REG_BINARY, data_for_3, sizeof(data_for_3));
	// if (err != ERROR_SUCCESS) {
	// 	printf("Failed to RegSetValueEx\n");
	// 	goto cleanup;
	// }

	// BYTE data_for_4[32] = {0x4, 0x20, 0, 0, 0, 0, 0, 0, 
	// 		0x18, 0, 0, 0, 0, 0, 0, 0,
	// 		0, 0, 0, 0, 0, 0, 0, 0,
	// 		0, 0, 0x80, 0x3f, 0, 0, 0x80, 0x3f};
	// err = RegSetValueEx(volume_store, L"4", 0, REG_BINARY, data_for_4, sizeof(data_for_4));
	// if (err != ERROR_SUCCESS) {
	// 	printf("Failed to RegSetValueEx\n");
	// 	goto cleanup;
	// }

	// BYTE data_for_5[24] = {0xb, 0, 0, 0, 0, 0, 0, 0, 
	// 		0, 0, 0, 0, 0, 0, 0, 0,
	// 		0, 0, 0, 0, 0, 0, 0, 0};
	// err = RegSetValueEx(volume_store, L"5", 0, REG_BINARY, data_for_5, sizeof(data_for_5));
	// if (err != ERROR_SUCCESS) {
	// 	printf("Failed to RegSetValueEx\n");
	// 	goto cleanup;
	// }

	err = ERROR_SUCCESS;
cleanup:
	// if (volume) volume->Release();

	// if (volume_store) {
	// 	err = RegCloseKey(volume_store);
	// 	if (err != ERROR_SUCCESS) {
	// 		wprintf(L"Failed to RegCloseKey\n");
	// 		return err;
	// 	}
	// }

	// err = RegCloseKey(property);
	// if (err != ERROR_SUCCESS) {
	// 	wprintf(L"Failed to RegCloseKey\n");
	// 	return err;
	// }

	return err;
}

LSTATUS get_last_modified(HKEY parent_key, TCHAR *sub_key, FILETIME *last_modified) {
	LSTATUS err;
	HKEY hkey = {0};
	
	if (sub_key) {
		err = RegOpenKeyEx(parent_key, sub_key, 0, KEY_READ, &hkey);
		if (err != ERROR_SUCCESS) {
			printf("Failed to RegOpenKeyEx\n");
			return err;
		}
	} else {
		hkey = parent_key;
	}

	err = RegQueryInfoKey(hkey, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, last_modified);
	if (err != ERROR_SUCCESS) {
		printf("Failed to RegQueryInfoKey\n");
		goto cleanup;
	}
	// TCHAR last_modified[200];
	// wrote = SHFormatDateTime(&last_modified_ft, NULL, last_modified, sizeof(last_modified));
	// if (wrote == 0) {
	// 	printf("Failed to SHFormatDateTime\n");
	// 	goto cleanup;
	// }
	// wprintf(L"last modified %ls\n", last_modified);

	err = ERROR_SUCCESS;
cleanup:
	if (sub_key && hkey) {
		err = RegCloseKey(hkey);
		if (err != ERROR_SUCCESS) {
			printf("Failed to RegCloseKey\n");
		}
	}

	return err;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow) {
	// TCHAR *name = (TCHAR*)L"foo";
	// ISimpleAudioVolume *volume = NULL;
	// LSTATUS err = GetVolumeObject(name, &volume);
	// if (err != ERROR_SUCCESS) {
	// 	printf("Failed to GetVolumeObject\n");
	// 	return 1;
	// }
	// printf("foo");
	// return 0;

	int ret = 1;
	int wrote;

	HKEY property_store = {0};
	LSTATUS err = RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Internet Explorer\\LowRegistry\\Audio\\PolicyConfig\\PropertyStore",
			0, KEY_READ|KEY_NOTIFY, &property_store);
	if (err != ERROR_SUCCESS) {
		printf("Failed to RegOpenKeyEx\n");
		goto cleanup;
	}

	FILETIME last_modified_ft;
	err = get_last_modified(property_store, NULL, &last_modified_ft);
	if (err != ERROR_SUCCESS) {
		printf("Failed to get_last_modified\n");
		goto cleanup;
	}
	TCHAR last_modified[200];
	wrote = SHFormatDateTime(&last_modified_ft, NULL, last_modified, sizeof(last_modified));
	if (wrote == 0) {
		printf("Failed to SHFormatDateTime\n");
		goto cleanup;
	}
	wprintf(L"last modified %ls\n", last_modified);

	// for (int i = 0; i < sub_key_count; i++) {
	// 	TCHAR name[200];
	// 	err = RegEnumKey(property_store, i, name, sizeof(name));
	// 	if (err != ERROR_SUCCESS) {
	// 		printf("Failed to RegEnumKey\n");
	// 		goto cleanup;
	// 	}
	// 	wprintf(L"sub key %ld = %ls\n", i, name);
	// }

	// DWORD orig_sub_key_count;
	// TCHAR **sub_key_names = NULL;
	// err = get_sub_key_count(property_store, &orig_sub_key_count);
	// if (err != ERROR_SUCCESS) {
	// 	printf("Failed to get_sub_key_count\n");
	// 	goto cleanup;
	// }
	// printf("There are %ld sub keys\n", orig_sub_key_count);

	// // In order to detect new properties, we need to record the name of every property
	// err = get_sub_key_names(property_store, &sub_key_names, orig_sub_key_count);
	// if (err != ERROR_SUCCESS) {
	// 	printf("Failed to get_sub_key_names\n");
	// 	goto cleanup;
	// }

	printf("Waiting for changes...\n");
	for (;;) {
		err = get_last_modified(property_store, NULL, &last_modified_ft);
		if (err != ERROR_SUCCESS) {
			printf("Failed to get_last_modified\n");
			goto cleanup;
		}

		err = RegNotifyChangeKeyValue(property_store, TRUE, REG_NOTIFY_CHANGE_NAME|REG_NOTIFY_CHANGE_ATTRIBUTES|REG_NOTIFY_CHANGE_LAST_SET, NULL, FALSE);
		if (err != ERROR_SUCCESS) {
			printf("Failed to RegNotifyChangeKeyValue\n");
			goto cleanup;
		}
		printf("The registry changed!\n");

		DWORD sub_key_count;
		err = get_sub_key_count(property_store, &sub_key_count);
		if (err != ERROR_SUCCESS) {
			printf("Failed to get_sub_key_count\n");
			goto cleanup;
		}
		// if (sub_key_count < orig_sub_key_count) {
		// 	printf("A property was removed\n");
		// } else if (sub_key_count > orig_sub_key_count) {
		// 	printf("A property was added\n");
		// } else {
		// 	printf("A property was changed\n");
		// }

		// There's got to be a better way to identify a new property!
		for (DWORD i = 0; i < sub_key_count; i++) {
			TCHAR name[200];
			err = RegEnumKey(property_store, i, name, sizeof(name));
			if (err != ERROR_SUCCESS) {
				printf("Failed to RegEnumKey\n");
				goto cleanup;
			}
			// BOOL found = FALSE;
			// for (DWORD j = 0; j < orig_sub_key_count; j++) {
			// 	if (wcscmp(sub_key_names[j], name) == 0) {
			// 		found = TRUE;
			// 	}
			// }
			// if (found == FALSE) {
			// 	wprintf(L"Property %ls is new\n", name);

			// 	err = handle_new_property(property_store, name);
			// 	if (err != ERROR_SUCCESS) {
			// 		printf("Failed to handle_new_property\n");
			// 		goto cleanup;
			// 	}
			// }

			FILETIME last_modified_sub;
			err = get_last_modified(property_store, name, &last_modified_sub);
			if (err != ERROR_SUCCESS) {
				printf("Failed to get_last_modified\n");
				goto cleanup;
			}
			ULARGE_INTEGER p;
			p.u.HighPart = last_modified_ft.dwHighDateTime;
			p.u.LowPart = last_modified_ft.dwLowDateTime;
			ULARGE_INTEGER s;
			s.u.HighPart = last_modified_sub.dwHighDateTime;
			s.u.LowPart = last_modified_sub.dwLowDateTime;
			if (s.QuadPart > p.QuadPart) {
				wprintf(L"sub-key %ls is newer\n", name);

				err = handle_new_property(property_store, name);
				if (err != ERROR_SUCCESS) {
					printf("Failed to handle_new_property\n");
					goto cleanup;
				}
			}

		}

		// orig_sub_key_count = sub_key_count;
		// err = get_sub_key_names(property_store, &sub_key_names, orig_sub_key_count);
		// if (err != ERROR_SUCCESS) {
		// 	printf("Failed to get_sub_key_names\n");
		// 	goto cleanup;
		// }
	}

	ret = 0;
cleanup:
	if (property_store) {
		err = RegCloseKey(property_store);
		if (err != ERROR_SUCCESS) {
			wprintf(L"Failed to RegCloseKey\n");
			return 1;
		}
	}

	return ret;
}
