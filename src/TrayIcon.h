#pragma once

#include <Windows.h>
#include <shellapi.h>
#include <functional>
#include <string>

namespace OrphanWatch
{
	using TrayMenuCallback = std::function<void(UINT commandId)>;

	class TrayIcon
	{
	public:
		TrayIcon();
		~TrayIcon();

		TrayIcon(const TrayIcon &)            = delete;
		TrayIcon &operator=(const TrayIcon &) = delete;

		bool Create(HINSTANCE hInstance);
		void Destroy();

		void ShowBalloon(const std::wstring &title, const std::wstring &message);
		void SetMenuCallback(TrayMenuCallback cb);
		void SetBalloonClickCallback(std::function<void()> cb);

		HWND GetHwnd() const { return m_hwnd; }

		// Must be called from message loop
		static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	private:
		void ShowContextMenu() const;

		HWND                  m_hwnd  = nullptr;
		NOTIFYICONDATAW       m_nid   = {};
		HICON                 m_hIcon = nullptr;
		TrayMenuCallback      m_menuCallback;
		std::function<void()> m_balloonClickCallback;
	};
} // namespace OrphanWatch
