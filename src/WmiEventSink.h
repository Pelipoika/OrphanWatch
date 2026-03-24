#pragma once

#include <Windows.h>
#include <Wbemidl.h>
#include <functional>
#include <string>
#include <atomic>

namespace OrphanWatch
{
	// Callback signature: (pid, parentPid, processName)
	using ProcessEventCallback = std::function<void(DWORD pid, DWORD parentPid, const std::wstring &name)>;

	class WmiEventSink : public IWbemObjectSink
	{
	public:
		WmiEventSink(ProcessEventCallback callback, bool isStartEvent);
		~WmiEventSink();

		// IUnknown
		ULONG STDMETHODCALLTYPE   AddRef() override;
		ULONG STDMETHODCALLTYPE   Release() override;
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override;

		// IWbemObjectSink
		HRESULT STDMETHODCALLTYPE Indicate(
			LONG                                  lObjectCount,
			IWbemClassObject __RPC_FAR* __RPC_FAR*apObjArray) override;

		HRESULT STDMETHODCALLTYPE SetStatus(
			LONG                       lFlags,
			HRESULT                    hResult,
			BSTR                       strParam,
			IWbemClassObject __RPC_FAR*pObjParam) override;

	private:
		std::atomic<LONG>    m_refCount;
		ProcessEventCallback m_callback;
		bool                 m_isStartEvent;
	};
} // namespace OrphanWatch
