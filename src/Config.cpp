#include "Config.h"

#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace OrphanWatch
{
	static std::wstring Utf8ToWide(const std::string &str)
	{
		if (str.empty())
			return {};

		const int    needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
		std::wstring result(static_cast<size_t>(needed), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), needed);
		return result;
	}

	bool Config::Load(const std::filesystem::path &path)
	{
		m_path = path;
		return Reload();
	}

	bool Config::Reload()
	{
		std::ifstream file(m_path);
		if (!file.is_open())
		{
			OutputDebugStringW(L"[OrphanWatch] Failed to open config file.\n");
			return false;
		}

		try
		{
			nlohmann::json j = nlohmann::json::parse(file);

			ConfigData data;

			if (j.contains("gracePeriodMs") && j["gracePeriodMs"].is_number_unsigned())
			{
				data.gracePeriodMs = j["gracePeriodMs"].get<DWORD>();
			}

			if (j.contains("processes") && j["processes"].is_array())
			{
				for (const auto &entry : j["processes"])
				{
					if (entry.is_string())
					{
						std::wstring name = Utf8ToWide(entry.get<std::string>());
						// Normalize to lowercase for case-insensitive matching
						std::ranges::transform(name, name.begin(), towlower);
						data.processes.push_back(std::move(name));
					}
				}
			}

			m_data = std::move(data);

			OutputDebugStringW(L"[OrphanWatch] Config loaded. Watching ");
			OutputDebugStringW(std::to_wstring(m_data.processes.size()).c_str());
			OutputDebugStringW(L" process(es).\n");

			return true;
		}
		catch (const nlohmann::json::exception &e)
		{
			const std::wstring msg = L"[OrphanWatch] JSON parse error: " + Utf8ToWide(e.what()) + L"\n";
			OutputDebugStringW(msg.c_str());
			return false;
		}
	}
} // namespace OrphanWatch
