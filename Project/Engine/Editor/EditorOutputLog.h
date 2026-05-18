#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	enum class EditorLogLevel
	{
		Info,
		Warning,
		Error
	};

	struct EditorLogEntry
	{
		EditorLogLevel level = EditorLogLevel::Info;
		std::string message;
	};

	class EditorOutputLog
	{
	public:
		void Add(EditorLogLevel level, const std::string& message);
		void Info(const std::string& message);
		void Warning(const std::string& message);
		void Error(const std::string& message);
		void Clear();
		std::vector<EditorLogEntry> GetEntries() const;

	private:
		mutable std::mutex mutex_;
		std::vector<EditorLogEntry> entries_;
	};

	const char* ToString(EditorLogLevel level);

} // namespace Ken4lowEngine
