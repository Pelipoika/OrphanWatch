#include "OrphanDialog.h"

#include <commctrl.h>
#include <sstream>
#include "../resources/resource.h"

#pragma comment(lib, "comctl32.lib")

namespace OrphanWatch
{
	static void TerminateOrphan(const DWORD pid)
	{
		if (const HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid))
		{
			if (TerminateProcess(hProcess, 1))
			{
				OutputDebugStringW(L"[OrphanWatch] Terminated PID ");
				OutputDebugStringW(std::to_wstring(pid).c_str());
				OutputDebugStringW(L"\n");
			}
			else
			{
				OutputDebugStringW(L"[OrphanWatch] Failed to terminate PID ");
				OutputDebugStringW(std::to_wstring(pid).c_str());
				OutputDebugStringW(L"\n");
			}

			CloseHandle(hProcess);
		}
	}

	void OrphanDialog::Show(const HWND                   parent,
	                        const std::wstring &         rootName,
	                        const DWORD                  rootPid,
	                        std::vector<TrackedProcess> &orphans)
	{
		// Build dialog template in memory
		// We use DialogBoxIndirectParam with an in-memory template

		DialogData data;
		data.rootName = rootName;
		data.rootPid  = rootPid;
		data.orphans  = &orphans;

		// Allocate a dialog template buffer
		// Template: DLGTEMPLATE + title string + controls
		constexpr size_t    kBufSize      = 4096;
		alignas(DWORD) BYTE buf[kBufSize] = {};
		BYTE *              p             = buf;

		// DLGTEMPLATE
		auto *dlg            = reinterpret_cast<DLGTEMPLATE*>(p);
		dlg->style           = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
		dlg->dwExtendedStyle = 0;
		dlg->cdit            = 3; // list view, terminate button, ignore button
		dlg->x               = 0;
		dlg->y               = 0;
		dlg->cx              = 280;
		dlg->cy              = 180;
		p                    += sizeof(DLGTEMPLATE);

		// Menu (none)
		auto *pw = reinterpret_cast<WORD*>(p);
		*pw++    = 0;
		// Class (default)
		*pw++ = 0;
		// Title
		const auto   title    = L"Orphaned Processes";
		const size_t titleLen = wcslen(title) + 1;
		memcpy(pw, title, titleLen * sizeof(wchar_t));
		pw += titleLen;

		// Align to DWORD
		p = reinterpret_cast<BYTE*>(pw);
		p = reinterpret_cast<BYTE*>((reinterpret_cast<ULONG_PTR>(p) + 3) & ~3);

		// List view control (SysListView32)
		{
			auto *item            = reinterpret_cast<DLGITEMTEMPLATE*>(p);
			item->style           = WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS;
			item->dwExtendedStyle = 0;
			item->x               = 5;
			item->y               = 5;
			item->cx              = 270;
			item->cy              = 140;
			item->id              = IDC_ORPHAN_LIST;
			p                     += sizeof(DLGITEMTEMPLATE);

			pw = reinterpret_cast<WORD*>(p);
			// Class: specify by string
			const auto   cls    = L"SysListView32";
			const size_t clsLen = wcslen(cls) + 1;
			memcpy(pw, cls, clsLen * sizeof(wchar_t));
			pw += clsLen;
			// Title (empty)
			*pw++ = 0;
			// Extra data size
			*pw++ = 0;
			p     = reinterpret_cast<BYTE*>(pw);
			p     = reinterpret_cast<BYTE*>((reinterpret_cast<ULONG_PTR>(p) + 3) & ~3);
		}

		// Terminate button
		{
			auto *item            = reinterpret_cast<DLGITEMTEMPLATE*>(p);
			item->style           = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
			item->dwExtendedStyle = 0;
			item->x               = 110;
			item->y               = 155;
			item->cx              = 75;
			item->cy              = 14;
			item->id              = IDC_TERMINATE_BTN;
			p                     += sizeof(DLGITEMTEMPLATE);

			pw                  = reinterpret_cast<WORD*>(p);
			*pw++               = 0xFFFF;
			*pw++               = 0x0080; // Button class
			const auto   txt    = L"Terminate Selected";
			const size_t txtLen = wcslen(txt) + 1;
			memcpy(pw, txt, txtLen * sizeof(wchar_t));
			pw    += txtLen;
			*pw++ = 0;
			p     = reinterpret_cast<BYTE*>(pw);
			p     = reinterpret_cast<BYTE*>((reinterpret_cast<ULONG_PTR>(p) + 3) & ~3);
		}

		// Ignore button
		{
			auto *item            = reinterpret_cast<DLGITEMTEMPLATE*>(p);
			item->style           = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
			item->dwExtendedStyle = 0;
			item->x               = 195;
			item->y               = 155;
			item->cx              = 50;
			item->cy              = 14;
			item->id              = IDC_IGNORE_BTN;
			p                     += sizeof(DLGITEMTEMPLATE);

			pw                  = reinterpret_cast<WORD*>(p);
			*pw++               = 0xFFFF;
			*pw++               = 0x0080; // Button class
			const auto   txt    = L"Ignore";
			const size_t txtLen = wcslen(txt) + 1;
			memcpy(pw, txt, txtLen * sizeof(wchar_t));
			pw    += txtLen;
			*pw++ = 0;
		}

		DialogBoxIndirectParamW(
		                        GetModuleHandleW(nullptr),
		                        reinterpret_cast<DLGTEMPLATE*>(buf),
		                        parent,
		                        DlgProc,
		                        reinterpret_cast<LPARAM>(&data));
	}

	INT_PTR CALLBACK OrphanDialog::DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
			case WM_INITDIALOG:
			{
				auto *data = reinterpret_cast<DialogData*>(lParam);
				SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));

				// Set dialog title with root info
				std::wstring title = L"Orphans from: " + data->rootName +
				                     L" (PID " + std::to_wstring(data->rootPid) + L")";
				SetWindowTextW(hwnd, title.c_str());

				// Setup list view
				HWND hList = GetDlgItem(hwnd, IDC_ORPHAN_LIST);
				ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);

				LVCOLUMNW col = {};
				col.mask      = LVCF_TEXT | LVCF_WIDTH;

				col.pszText = const_cast<LPWSTR>(L"PID");
				col.cx      = 60;
				ListView_InsertColumn(hList, 0, &col);

				col.pszText = const_cast<LPWSTR>(L"Name");
				col.cx      = 150;
				ListView_InsertColumn(hList, 1, &col);

				col.pszText = const_cast<LPWSTR>(L"Parent PID");
				col.cx      = 80;
				ListView_InsertColumn(hList, 2, &col);

				// Populate
				for (size_t i = 0; i < data->orphans->size(); ++i)
				{
					const auto &orphan = (*data->orphans)[i];

					LVITEMW item = {};
					item.mask    = LVIF_TEXT;
					item.iItem   = static_cast<int>(i);

					std::wstring pidStr = std::to_wstring(orphan.pid);
					item.pszText        = const_cast<LPWSTR>(pidStr.c_str());
					item.iSubItem       = 0;
					ListView_InsertItem(hList, &item);

					ListView_SetItemText(hList, static_cast<int>(i), 1,
					                     const_cast<LPWSTR>(orphan.name.c_str()));

					std::wstring parentStr = std::to_wstring(orphan.parentPid);
					ListView_SetItemText(hList, static_cast<int>(i), 2,
					                     const_cast<LPWSTR>(parentStr.c_str()));

					// Check all by default
					ListView_SetCheckState(hList, static_cast<int>(i), TRUE);
				}

				return TRUE;
			}

			case WM_COMMAND:
			{
				auto *data = reinterpret_cast<DialogData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
				WORD  id   = LOWORD(wParam);

				if (id == IDC_TERMINATE_BTN)
				{
					HWND hList = GetDlgItem(hwnd, IDC_ORPHAN_LIST);
					int  count = ListView_GetItemCount(hList);

					for (int i = 0; i < count; ++i)
					{
						if (ListView_GetCheckState(hList, i))
						{
							if (data && data->orphans && i < static_cast<int>(data->orphans->size()))
							{
								TerminateOrphan((*data->orphans)[static_cast<size_t>(i)].pid);
							}
						}
					}
					EndDialog(hwnd, IDOK);
					return TRUE;
				}
				else if (id == IDC_IGNORE_BTN || id == IDCANCEL)
				{
					EndDialog(hwnd, IDCANCEL);
					return TRUE;
				}
				break;
			}

			case WM_CLOSE:
				EndDialog(hwnd, IDCANCEL);
				return TRUE;
		}

		return FALSE;
	}
} // namespace OrphanWatch
