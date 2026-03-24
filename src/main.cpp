#include <Windows.h>
#include <objbase.h>
#include "Application.h"

int WINAPI wWinMain(
	_In_ const HINSTANCE hInstance,
	_In_opt_ HINSTANCE /*hPrevInstance*/,
	_In_ LPWSTR /*lpCmdLine*/,
	_In_ int /*nShowCmd*/)
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		MessageBoxW(nullptr, L"Failed to initialize COM.", L"OrphanWatch", MB_ICONERROR);
		return 1;
	}

	hr = CoInitializeSecurity(
	                          nullptr, -1, nullptr, nullptr,
	                          RPC_C_AUTHN_LEVEL_DEFAULT,
	                          RPC_C_IMP_LEVEL_IMPERSONATE,
	                          nullptr, EOAC_NONE, nullptr);

	if (FAILED(hr))
	{
		MessageBoxW(nullptr, L"Failed to initialize COM security.", L"OrphanWatch", MB_ICONERROR);
		CoUninitialize();
		return 1;
	}

	{
		OrphanWatch::Application app(hInstance);
		if (!app.Init())
		{
			MessageBoxW(nullptr, L"Failed to initialize OrphanWatch.", L"OrphanWatch", MB_ICONERROR);
			CoUninitialize();
			return 1;
		}
		app.Run();
	}

	CoUninitialize();
	return 0;
}
