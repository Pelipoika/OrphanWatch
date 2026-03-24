#include "ProcessMonitor.h"

#include <comdef.h>

namespace OrphanWatch
{
	ProcessMonitor::ProcessMonitor() = default;

	ProcessMonitor::~ProcessMonitor()
	{
		Stop();
	}

	void ProcessMonitor::SetProcessTree(std::shared_ptr<ProcessTree> tree)
	{
		m_tree = std::move(tree);
	}

	bool ProcessMonitor::Start()
	{
		if (!m_tree)
		{
			OutputDebugStringW(L"[OrphanWatch] ProcessMonitor::Start called without a ProcessTree.\n");
			return false;
		}

		// Create WMI locator
		HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
		                              IID_IWbemLocator, reinterpret_cast<void**>(&m_pLocator));

		if (FAILED(hr))
		{
			OutputDebugStringW(L"[OrphanWatch] Failed to create WbemLocator.\n");
			return false;
		}

		// Connect to ROOT\CIMV2
		hr = m_pLocator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr,
		                               0, nullptr, nullptr, &m_pServices);

		if (FAILED(hr))
		{
			OutputDebugStringW(L"[OrphanWatch] Failed to connect to WMI.\n");
			return false;
		}

		// Set security on the proxy
		hr = CoSetProxyBlanket(m_pServices,
		                       RPC_C_AUTHN_WINNT,
		                       RPC_C_AUTHZ_NONE,
		                       nullptr,
		                       RPC_C_AUTHN_LEVEL_CALL,
		                       RPC_C_IMP_LEVEL_IMPERSONATE,
		                       nullptr,
		                       EOAC_NONE);

		if (FAILED(hr))
		{
			OutputDebugStringW(L"[OrphanWatch] Failed to set proxy blanket.\n");
			return false;
		}

		// Seed tree with currently running processes before subscribing to events
		m_tree->SeedFromSnapshot();

		// Create start-event sink
		auto tree = m_tree;

		m_pStartSink = new WmiEventSink([tree](const DWORD pid, const DWORD parentPid, const std::wstring &name){
			                                tree->OnProcessStart(pid, parentPid, name);
		                                },
		                                true);

		hr = m_pServices->ExecNotificationQueryAsync(
		                                             _bstr_t(L"WQL"),
		                                             _bstr_t(L"SELECT * FROM Win32_ProcessStartTrace"),
		                                             WBEM_FLAG_SEND_STATUS,
		                                             nullptr,
		                                             m_pStartSink);

		if (FAILED(hr))
		{
			OutputDebugStringW(L"[OrphanWatch] Failed to subscribe to process start events.\n");
			return false;
		}

		// Create stop-event sink
		m_pStopSink = new WmiEventSink([tree](const DWORD pid, DWORD /*parentPid*/, const std::wstring & /*name*/){
			                               tree->OnProcessStop(pid);
		                               },
		                               false);

		hr = m_pServices->ExecNotificationQueryAsync(_bstr_t(L"WQL"),
		                                             _bstr_t(L"SELECT * FROM Win32_ProcessStopTrace"),
		                                             WBEM_FLAG_SEND_STATUS,
		                                             nullptr,
		                                             m_pStopSink);

		if (FAILED(hr))
		{
			OutputDebugStringW(L"[OrphanWatch] Failed to subscribe to process stop events.\n");
			return false;
		}

		OutputDebugStringW(L"[OrphanWatch] WMI monitoring started.\n");
		return true;
	}

	void ProcessMonitor::Stop()
	{
		if (m_pServices)
		{
			if (m_pStartSink)
			{
				m_pServices->CancelAsyncCall(m_pStartSink);
				m_pStartSink->Release();
				m_pStartSink = nullptr;
			}
			if (m_pStopSink)
			{
				m_pServices->CancelAsyncCall(m_pStopSink);
				m_pStopSink->Release();
				m_pStopSink = nullptr;
			}

			m_pServices->Release();
			m_pServices = nullptr;
		}
		if (m_pLocator)
		{
			m_pLocator->Release();
			m_pLocator = nullptr;
		}

		OutputDebugStringW(L"[OrphanWatch] WMI monitoring stopped.\n");
	}
} // namespace OrphanWatch
