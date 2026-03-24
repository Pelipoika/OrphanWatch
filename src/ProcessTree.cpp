#include "ProcessTree.h"

#include <TlHelp32.h>
#include <algorithm>

namespace OrphanWatch
{
	void ProcessTree::SetWatchedNames(const std::vector<std::wstring> &names)
	{
		std::unique_lock lock(m_mutex);
		m_watchedNames = names;
	}

	void ProcessTree::SetOrphanCallback(OrphanCallback cb)
	{
		m_orphanCallback = std::move(cb);
	}

	void ProcessTree::OnProcessStart(const DWORD pid, const DWORD parentPid, const std::wstring &rawName)
	{
		std::wstring name = rawName;
		std::ranges::transform(name, name.begin(), towlower);

		std::unique_lock lock(m_mutex);

		if (IsWatchedName(name))
		{
			// This is a watched root process
			AddProcess(pid, parentPid, name, true, pid);

			OutputDebugStringW(L"[OrphanWatch] Tracking root: ");
			OutputDebugStringW(name.c_str());
			OutputDebugStringW(L" (PID ");
			OutputDebugStringW(std::to_wstring(pid).c_str());
			OutputDebugStringW(L")\n");
			return;
		}

		// Check if parent is tracked — if so, this is a descendant
		const auto it = m_tracked.find(parentPid);
		if (it != m_tracked.end())
		{
			const DWORD rootPid = it->second.rootPid;
			AddProcess(pid, parentPid, name, false, rootPid);

			OutputDebugStringW(L"[OrphanWatch] Tracking child: ");
			OutputDebugStringW(name.c_str());
			OutputDebugStringW(L" (PID ");
			OutputDebugStringW(std::to_wstring(pid).c_str());
			OutputDebugStringW(L") under root PID ");
			OutputDebugStringW(std::to_wstring(rootPid).c_str());
			OutputDebugStringW(L"\n");
		}
	}

	void ProcessTree::OnProcessStop(const DWORD pid)
	{
		std::unique_lock lock(m_mutex);

		const auto it = m_tracked.find(pid);
		if (it == m_tracked.end())
		{
			return;
		}

		const TrackedProcess stopped = std::move(it->second);
		m_tracked.erase(it);

		if (stopped.isWatchedRoot)
		{
			// Root exited — collect orphaned descendants
			auto orphans = CollectDescendants(stopped.rootPid);
			if (!orphans.empty() && m_orphanCallback)
			{
				OutputDebugStringW(L"[OrphanWatch] Root exited: ");
				OutputDebugStringW(stopped.name.c_str());
				OutputDebugStringW(L" (PID ");
				OutputDebugStringW(std::to_wstring(stopped.pid).c_str());
				OutputDebugStringW(L"). Orphans: ");
				OutputDebugStringW(std::to_wstring(orphans.size()).c_str());
				OutputDebugStringW(L"\n");

				lock.unlock();
				m_orphanCallback(stopped.pid, stopped.name, std::move(orphans));
			}
		}
	}

	void ProcessTree::SeedFromSnapshot()
	{
		const HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnap == INVALID_HANDLE_VALUE)
		{
			OutputDebugStringW(L"[OrphanWatch] Failed to create process snapshot.\n");
			return;
		}

		// First pass: collect all processes
		struct SnapEntry
		{
			DWORD        pid;
			DWORD        parentPid;
			std::wstring name;
		};
		std::vector<SnapEntry> entries;

		PROCESSENTRY32W pe = {};
		pe.dwSize          = sizeof(pe);
		if (Process32FirstW(hSnap, &pe))
		{
			do
			{
				std::wstring name = pe.szExeFile;
				std::ranges::transform(name, name.begin(), towlower);
				entries.push_back({.pid = pe.th32ProcessID, .parentPid = pe.th32ParentProcessID, .name = std::move(name)});
			}
			while (Process32NextW(hSnap, &pe));
		}
		CloseHandle(hSnap);

		std::unique_lock lock(m_mutex);

		// Second pass: add watched roots
		for (const auto &e : entries)
		{
			if (IsWatchedName(e.name))
			{
				AddProcess(e.pid, e.parentPid, e.name, true, e.pid);
			}
		}

		// Third pass: add children of tracked processes (repeat until no new additions)
		bool added = true;
		while (added)
		{
			added = false;
			for (const auto &e : entries)
			{
				if (m_tracked.contains(e.pid))
					continue;

				auto parentIt = m_tracked.find(e.parentPid);
				if (parentIt != m_tracked.end())
				{
					AddProcess(e.pid, e.parentPid, e.name, false, parentIt->second.rootPid);
					added = true;
				}
			}
		}

		OutputDebugStringW(L"[OrphanWatch] Seeded ");
		OutputDebugStringW(std::to_wstring(m_tracked.size()).c_str());
		OutputDebugStringW(L" process(es) from snapshot.\n");
	}

	bool ProcessTree::IsWatchedName(const std::wstring &name) const
	{
		// Assumes name is already lowercase
		return std::ranges::find(m_watchedNames, name) != m_watchedNames.end();
	}

	FILETIME ProcessTree::GetProcessCreationTime(const DWORD pid)
	{
		FILETIME creation = {}, exit = {}, kernel = {}, user = {};
		if (const HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid))
		{
			GetProcessTimes(hProcess, &creation, &exit, &kernel, &user);
			CloseHandle(hProcess);
		}

		return creation;
	}

	std::vector<TrackedProcess> ProcessTree::CollectDescendants(const DWORD rootPid) const
	{
		std::vector<TrackedProcess> descendants;
		for (const auto &[pid, proc] : m_tracked)
		{
			if (proc.rootPid == rootPid && !proc.isWatchedRoot)
			{
				// Verify the process is still alive with matching creation time
				const FILETIME current = GetProcessCreationTime(pid);
				if (current.dwLowDateTime == proc.creationTime.dwLowDateTime &&
				    current.dwHighDateTime == proc.creationTime.dwHighDateTime &&
				    (current.dwLowDateTime != 0 || current.dwHighDateTime != 0))
				{
					descendants.push_back(proc);
				}
			}
		}

		return descendants;
	}

	void ProcessTree::AddProcess(const DWORD pid, const DWORD parentPid, const std::wstring &name, const bool isRoot, const DWORD rootPid)
	{
		TrackedProcess tp;
		tp.pid           = pid;
		tp.parentPid     = parentPid;
		tp.name          = name;
		tp.creationTime  = GetProcessCreationTime(pid);
		tp.isWatchedRoot = isRoot;
		tp.rootPid       = rootPid;
		m_tracked[pid]   = std::move(tp);
	}
} // namespace OrphanWatch
