#include "WmiEventSink.h"

#include <comdef.h>

namespace OrphanWatch
{
	WmiEventSink::WmiEventSink(ProcessEventCallback callback, const bool isStartEvent) : m_refCount(1)
	                                                                                   , m_callback(std::move(callback))
	                                                                                   , m_isStartEvent(isStartEvent) { }

	WmiEventSink::~WmiEventSink() = default;

	ULONG STDMETHODCALLTYPE WmiEventSink::AddRef()
	{
		return ++m_refCount;
	}

	ULONG STDMETHODCALLTYPE WmiEventSink::Release()
	{
		const LONG count = --m_refCount;
		if (count <= 0)
		{
			delete this;
		}

		return static_cast<ULONG>(count);
	}

	HRESULT STDMETHODCALLTYPE WmiEventSink::QueryInterface(REFIID riid, void **ppv)
	{
		if (riid == IID_IUnknown || riid == IID_IWbemObjectSink)
		{
			*ppv = static_cast<IWbemObjectSink*>(this);
			AddRef();
			return WBEM_S_NO_ERROR;
		}

		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	HRESULT STDMETHODCALLTYPE WmiEventSink::Indicate(LONG lObjectCount, IWbemClassObject __RPC_FAR* __RPC_FAR*apObjArray)
	{
		for (LONG i = 0; i < lObjectCount; ++i)
		{
			IWbemClassObject *pObj = apObjArray[i];
			if (!pObj)
			{
				continue;
			}

			VARIANT vtPid       = {};
			VARIANT vtParentPid = {};
			VARIANT vtName      = {};

			DWORD        pid       = 0;
			DWORD        parentPid = 0;
			std::wstring name;

			// ProcessID
			if (SUCCEEDED(pObj->Get(L"ProcessID", 0, &vtPid, nullptr, nullptr)))
			{
				if (vtPid.vt == VT_I4 || vtPid.vt == VT_UI4)
				{
					pid = static_cast<DWORD>(vtPid.uintVal);
				}
				VariantClear(&vtPid);
			}

			// ParentProcessID (only available in start trace events)
			if (m_isStartEvent)
			{
				if (SUCCEEDED(pObj->Get(L"ParentProcessID", 0, &vtParentPid, nullptr, nullptr)))
				{
					if (vtParentPid.vt == VT_I4 || vtParentPid.vt == VT_UI4)
					{
						parentPid = static_cast<DWORD>(vtParentPid.uintVal);
					}
					VariantClear(&vtParentPid);
				}
			}

			// ProcessName
			if (SUCCEEDED(pObj->Get(L"ProcessName", 0, &vtName, nullptr, nullptr)))
			{
				if (vtName.vt == VT_BSTR && vtName.bstrVal)
				{
					name = vtName.bstrVal;
				}
				VariantClear(&vtName);
			}

			if (pid != 0 && m_callback)
			{
				m_callback(pid, parentPid, name);
			}
		}

		return WBEM_S_NO_ERROR;
	}

	HRESULT STDMETHODCALLTYPE WmiEventSink::SetStatus(LONG /*lFlags*/, const HRESULT hResult, BSTR /*strParam*/, IWbemClassObject __RPC_FAR* /*pObjParam*/)
	{
		if (FAILED(hResult))
		{
			OutputDebugStringW(L"[OrphanWatch] WMI event sink received error status.\n");
		}

		return WBEM_S_NO_ERROR;
	}
} // namespace OrphanWatch
