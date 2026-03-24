#pragma once

#include <Windows.h>
#include <vector>
#include "ProcessTree.h"

namespace OrphanWatch
{
	class OrphanDialog
	{
	public:
		// Shows a modal dialog listing orphaned processes.
		// Returns true if user chose to terminate any.
		static void Show(HWND                         parent,
		                 const std::wstring &         rootName,
		                 DWORD                        rootPid,
		                 std::vector<TrackedProcess> &orphans);

	private:
		static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		struct DialogData
		{
			std::wstring                 rootName;
			DWORD                        rootPid = 0;
			std::vector<TrackedProcess> *orphans = nullptr;
		};
	};
} // namespace OrphanWatch
