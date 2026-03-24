#pragma once

#include <Windows.h>
#include <Wbemidl.h>
#include <memory>
#include "ProcessTree.h"
#include "WmiEventSink.h"

namespace OrphanWatch
{
	class ProcessMonitor
	{
	public:
		ProcessMonitor();
		~ProcessMonitor();

		ProcessMonitor(const ProcessMonitor &)            = delete;
		ProcessMonitor &operator=(const ProcessMonitor &) = delete;

		void SetProcessTree(std::shared_ptr<ProcessTree> tree);
		bool Start();
		void Stop();

	private:
		std::shared_ptr<ProcessTree> m_tree;

		IWbemLocator * m_pLocator   = nullptr;
		IWbemServices *m_pServices  = nullptr;
		WmiEventSink * m_pStartSink = nullptr;
		WmiEventSink * m_pStopSink  = nullptr;
	};
} // namespace OrphanWatch
