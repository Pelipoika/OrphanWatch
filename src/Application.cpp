#include "Application.h"

#include <shellapi.h>
#include <shlwapi.h>
#include <commctrl.h>
#include "OrphanDialog.h"
#include "../resources/resource.h"

namespace OrphanWatch
{
	Application::Application(const HINSTANCE hInstance) : m_hInstance(hInstance) { }

	Application::~Application()
	{
		if (m_processMonitor)
		{
			m_processMonitor->Stop();
		}

		m_trayIcon.Destroy();
	}

	bool Application::Init()
	{
		// Initialize common controls (needed for ListView in dialog)
		INITCOMMONCONTROLSEX icc;
		icc.dwSize = sizeof(icc);
		icc.dwICC  = ICC_LISTVIEW_CLASSES;
		InitCommonControlsEx(&icc);

		// Load config
		std::filesystem::path configPath = GetConfigPath();
		if (!m_config.Load(configPath))
		{
			// Try fallback: config next to the working directory
			configPath = L"config\\watchlist.json";
			if (!m_config.Load(configPath))
			{
				OutputDebugStringW(L"[OrphanWatch] Could not load config from any location.\n");
				return false;
			}
		}

		// Create tray icon
		if (!m_trayIcon.Create(m_hInstance))
			return false;

		m_trayIcon.SetMenuCallback([this](const UINT id){
			OnMenuCommand(id);
		});

		m_trayIcon.SetBalloonClickCallback([this](){
			OnBalloonClick();
		});

		// Create process tree
		m_processTree = std::make_shared<ProcessTree>();
		m_processTree->SetWatchedNames(m_config.Data().processes);
		m_processTree->SetOrphanCallback([this](const DWORD rootPid, const std::wstring &rootName, std::vector<TrackedProcess> orphans){
			OnOrphansDetected(rootPid, rootName, std::move(orphans));
		});

		// Create and start monitor
		m_processMonitor = std::make_unique<ProcessMonitor>();
		m_processMonitor->SetProcessTree(m_processTree);

		if (!m_processMonitor->Start())
		{
			OutputDebugStringW(L"[OrphanWatch] Failed to start process monitor.\n");
			return false;
		}

		// Show startup balloon
		m_trayIcon.ShowBalloon(L"OrphanWatch Active",
		                       L"Monitoring " + std::to_wstring(m_config.Data().processes.size()) + L" process name(s).");

		return true;
	}

	void Application::Run()
	{
		MSG msg;
		while (GetMessageW(&msg, nullptr, 0, 0))
		{
			// Check for pending orphan alerts (posted from WMI thread)
			if (msg.message == WM_ORPHAN_DETECTED)
			{
				std::vector<OrphanAlert> alerts;
				{
					std::scoped_lock lock(m_alertMutex);
					alerts.swap(m_pendingAlerts);
				}

				for (auto &alert : alerts)
				{
					// Apply grace period: sleep then re-verify
					const DWORD grace = m_config.Data().gracePeriodMs;
					if (grace > 0)
					{
						Sleep(grace);
					}

					// Re-verify orphans are still alive
					std::vector<TrackedProcess> alive;
					for (auto &orphan : alert.orphans)
					{
						const HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, orphan.pid);
						if (h)
						{
							DWORD exitCode = 0;
							if (GetExitCodeProcess(h, &exitCode) && exitCode == STILL_ACTIVE)
							{
								alive.push_back(std::move(orphan));
							}
							CloseHandle(h);
						}
					}

					if (!alive.empty())
					{
						std::wstring balloonMsg = std::to_wstring(alive.size()) +
						                          L" orphaned process(es) from " + alert.rootName +
						                          L". Click for details.";
						m_trayIcon.ShowBalloon(L"Orphaned Processes Detected", balloonMsg);

						// Store for balloon click
						alert.orphans = std::move(alive);
						std::scoped_lock lock(m_alertMutex);
						m_pendingAlerts.push_back(std::move(alert));
					}
				}
				continue;
			}

			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	void Application::OnOrphansDetected(const DWORD                 rootPid,
	                                    const std::wstring &        rootName,
	                                    std::vector<TrackedProcess> orphans)
	{
		// Called from WMI thread — marshal to the UI thread
		{
			std::scoped_lock lock(m_alertMutex);
			m_pendingAlerts.push_back({.rootPid = rootPid, .rootName = rootName, .orphans = std::move(orphans)});
		}

		if (const HWND hwnd = m_trayIcon.GetHwnd())
		{
			PostMessageW(hwnd, WM_ORPHAN_DETECTED, 0, 0);
		}
	}

	void Application::OnMenuCommand(const UINT commandId)
	{
		switch (commandId)
		{
			case IDM_RELOAD_CONFIG:
				ReloadConfig();
				break;

			case IDM_OPEN_CONFIG:
				OpenConfigFile();
				break;

			case IDM_EXIT:
				PostQuitMessage(0);
				break;
		}
	}

	void Application::OnBalloonClick()
	{
		std::vector<OrphanAlert> alerts;
		{
			std::scoped_lock lock(m_alertMutex);
			alerts.swap(m_pendingAlerts);
		}

		for (auto &alert : alerts)
		{
			if (!alert.orphans.empty())
			{
				OrphanDialog::Show(m_trayIcon.GetHwnd(), alert.rootName, alert.rootPid, alert.orphans);
			}
		}
	}

	void Application::ReloadConfig()
	{
		if (m_config.Reload())
		{
			m_processTree->SetWatchedNames(m_config.Data().processes);
			m_trayIcon.ShowBalloon(L"Config Reloaded",
			                       L"Now watching " + std::to_wstring(m_config.Data().processes.size()) + L" process(es).");
		}
		else
		{
			m_trayIcon.ShowBalloon(L"Config Error", L"Failed to reload watchlist.json.");
		}
	}

	void Application::OpenConfigFile() const
	{
		const std::filesystem::path configPath = m_config.FilePath();
		ShellExecuteW(nullptr, L"open", configPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}

	std::filesystem::path Application::GetConfigPath()
	{
		// Look for config relative to the executable
		wchar_t exePath[MAX_PATH] = {};
		GetModuleFileNameW(nullptr, exePath, MAX_PATH);
		const std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();

		// Primary: config next to executable (handles post-build copy)
		std::filesystem::path candidate = exeDir / L"config" / L"watchlist.json";
		if (std::filesystem::exists(candidate))
			return candidate;

		// Fallback: one level up from executable (handles build/ subdirectory)
		candidate = exeDir.parent_path() / L"config" / L"watchlist.json";
		if (std::filesystem::exists(candidate))
			return candidate;

		// Default: return the primary path (will be reported as missing by caller)
		return exeDir / L"config" / L"watchlist.json";
	}
} // namespace OrphanWatch
