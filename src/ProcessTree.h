#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <functional>

namespace OrphanWatch
{
	struct TrackedProcess
	{
		DWORD        pid       = 0;
		DWORD        parentPid = 0;
		std::wstring name;
		FILETIME     creationTime  = {};
		bool         isWatchedRoot = false;
		DWORD        rootPid       = 0; // the watched-root ancestor this belongs to
	};

	using OrphanCallback = std::function<void(DWORD                       rootPid,
	                                          const std::wstring &        rootName,
	                                          std::vector<TrackedProcess> orphans)>;

	class ProcessTree
	{
	public:
		void SetWatchedNames(const std::vector<std::wstring> &names);
		void SetOrphanCallback(OrphanCallback cb);

		void OnProcessStart(DWORD pid, DWORD parentPid, const std::wstring &name);
		void OnProcessStop(DWORD pid);

		// Seed the tree with a snapshot of currently-running processes
		void SeedFromSnapshot();

	private:
		bool                        IsWatchedName(const std::wstring &name) const;
		static FILETIME             GetProcessCreationTime(DWORD pid);
		std::vector<TrackedProcess> CollectDescendants(DWORD rootPid) const;
		void                        AddProcess(DWORD pid, DWORD parentPid, const std::wstring &name, bool isRoot, DWORD rootPid);

		mutable std::shared_mutex                 m_mutex;
		std::unordered_map<DWORD, TrackedProcess> m_tracked;
		std::vector<std::wstring>                 m_watchedNames; // all lowercase
		OrphanCallback                            m_orphanCallback;
	};
} // namespace OrphanWatch
