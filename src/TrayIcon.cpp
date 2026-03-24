#include "TrayIcon.h"

#include <commctrl.h>
#include "../resources/resource.h"

namespace OrphanWatch
{
	static const auto kWindowClassName = L"OrphanWatchTrayWnd";

	TrayIcon::TrayIcon() = default;

	TrayIcon::~TrayIcon()
	{
		Destroy();
	}

	bool TrayIcon::Create(const HINSTANCE hInstance)
	{
		// Register window class
		WNDCLASSEXW wc   = {};
		wc.cbSize        = sizeof(wc);
		wc.lpfnWndProc   = WndProc;
		wc.hInstance     = hInstance;
		wc.lpszClassName = kWindowClassName;

		if (!RegisterClassExW(&wc))
		{
			if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
			{
				OutputDebugStringW(L"[OrphanWatch] Failed to register tray window class.\n");
				return false;
			}
		}

		// Create message-only window
		m_hwnd = CreateWindowExW(
		                         0, kWindowClassName, L"OrphanWatch",
		                         0, 0, 0, 0, 0,
		                         HWND_MESSAGE, nullptr, hInstance, nullptr);

		if (!m_hwnd)
		{
			OutputDebugStringW(L"[OrphanWatch] Failed to create tray window.\n");
			return false;
		}

		// Store 'this' pointer for WndProc
		SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

		// Load icon
		m_hIcon = static_cast<HICON>(LoadImageW(
		                                        hInstance, MAKEINTRESOURCEW(IDI_TRAYICON),
		                                        IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));

		if (!m_hIcon)
		{
			m_hIcon = LoadIconW(nullptr, IDI_APPLICATION);
		}

		// Add tray icon
		m_nid                  = {};
		m_nid.cbSize           = sizeof(m_nid);
		m_nid.hWnd             = m_hwnd;
		m_nid.uID              = IDI_TRAYICON;
		m_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
		m_nid.uCallbackMessage = WM_TRAYICON;
		m_nid.hIcon            = m_hIcon;
		wcscpy_s(m_nid.szTip, L"OrphanWatch — Process Monitor");

		if (!Shell_NotifyIconW(NIM_ADD, &m_nid))
		{
			OutputDebugStringW(L"[OrphanWatch] Failed to add tray icon.\n");
			return false;
		}

		// Use version 4 for balloon support
		m_nid.uVersion = NOTIFYICON_VERSION_4;
		Shell_NotifyIconW(NIM_SETVERSION, &m_nid);

		OutputDebugStringW(L"[OrphanWatch] Tray icon created.\n");
		return true;
	}

	void TrayIcon::Destroy()
	{
		if (m_nid.hWnd)
		{
			Shell_NotifyIconW(NIM_DELETE, &m_nid);
			m_nid.hWnd = nullptr;
		}
		if (m_hwnd)
		{
			DestroyWindow(m_hwnd);
			m_hwnd = nullptr;
		}
		if (m_hIcon)
		{
			DestroyIcon(m_hIcon);
			m_hIcon = nullptr;
		}
	}

	void TrayIcon::ShowBalloon(const std::wstring &title, const std::wstring &message)
	{
		m_nid.uFlags      = NIF_INFO;
		m_nid.dwInfoFlags = NIIF_WARNING;
		wcsncpy_s(m_nid.szInfoTitle, title.c_str(), _TRUNCATE);
		wcsncpy_s(m_nid.szInfo, message.c_str(), _TRUNCATE);
		Shell_NotifyIconW(NIM_MODIFY, &m_nid);
	}

	void TrayIcon::SetMenuCallback(TrayMenuCallback cb)
	{
		m_menuCallback = std::move(cb);
	}

	void TrayIcon::SetBalloonClickCallback(std::function<void()> cb)
	{
		m_balloonClickCallback = std::move(cb);
	}

	LRESULT CALLBACK TrayIcon::WndProc(const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam)
	{
		const auto *self = reinterpret_cast<TrayIcon*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

		switch (msg)
		{
			case WM_TRAYICON:
			{
				switch (LOWORD(lParam))
				{
					case WM_RBUTTONUP:
						if (self)
						{
							self->ShowContextMenu();
						}
						return 0;

					case NIN_BALLOONUSERCLICK:
						if (self && self->m_balloonClickCallback)
						{
							self->m_balloonClickCallback();
						}
						return 0;
				}
				break;
			}

			case WM_ORPHAN_DETECTED:
				// Forward to Application via posted message — handled in Application
				break;

			case WM_DESTROY:
				PostQuitMessage(0);
				return 0;
		}

		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	void TrayIcon::ShowContextMenu() const
	{
		const HMENU hMenu = CreatePopupMenu();
		if (!hMenu)
		{
			return;
		}

		AppendMenuW(hMenu, MF_STRING, IDM_RELOAD_CONFIG, L"Reload Config");
		AppendMenuW(hMenu, MF_STRING, IDM_OPEN_CONFIG, L"Open Config File");
		AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
		AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

		POINT pt;
		GetCursorPos(&pt);

		// Required for TrackPopupMenu to work correctly from a tray icon
		SetForegroundWindow(m_hwnd);

		const UINT cmd = TrackPopupMenu(
		                                hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
		                                pt.x, pt.y, 0, m_hwnd, nullptr);

		DestroyMenu(hMenu);

		// Fix disappearing menu
		PostMessageW(m_hwnd, WM_NULL, 0, 0);

		if (cmd != 0 && m_menuCallback)
		{
			m_menuCallback(cmd);
		}
	}
} // namespace OrphanWatch
