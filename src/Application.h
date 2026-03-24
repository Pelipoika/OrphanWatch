#pragma once

#include <Windows.h>
#include <memory>
#include <vector>
#include <mutex>
#include "Config.h"
#include "ProcessTree.h"
#include "ProcessMonitor.h"
#include "TrayIcon.h"

namespace OrphanWatch
{
	class Application
	{
	public:
		explicit Application(HINSTANCE hInstance);
		~Application();

		Application(const Application &)            = delete;
		Application &operator=(const Application &) = delete;

		bool Init();
		void Run();

	private:
		void OnOrphansDetected(DWORD                       rootPid,
		                       const std::wstring &        rootName,
		                       std::vector<TrackedProcess> orphans);
		void OnMenuCommand(UINT commandId);
		void OnBalloonClick();
		void ReloadConfig();
		void OpenConfigFile() const;

		static std::filesystem::path GetConfigPath();

		HINSTANCE m_hInstance = nullptr;

		Config                          m_config;
		TrayIcon                        m_trayIcon;
		std::shared_ptr<ProcessTree>    m_processTree;
		std::unique_ptr<ProcessMonitor> m_processMonitor;

		// Pending orphan data (populated by WMI thread, consumed by UI thread)
		struct OrphanAlert
		{
			DWORD                       rootPid;
			std::wstring                rootName;
			std::vector<TrackedProcess> orphans;
		};

		std::mutex               m_alertMutex;
		std::vector<OrphanAlert> m_pendingAlerts;
	};
} // namespace OrphanWatch
