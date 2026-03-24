#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <filesystem>

namespace OrphanWatch
{
	struct ConfigData
	{
		std::vector<std::wstring> processes;
		DWORD                     gracePeriodMs = 5000;
	};

	class Config
	{
	public:
		bool Load(const std::filesystem::path &path);
		bool Reload();

		const ConfigData &           Data() const { return m_data; }
		const std::filesystem::path &FilePath() const { return m_path; }

	private:
		std::filesystem::path m_path;
		ConfigData            m_data;
	};
} // namespace OrphanWatch
