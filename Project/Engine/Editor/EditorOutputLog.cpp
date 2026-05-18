#include "EditorOutputLog.h"

namespace Ken4lowEngine
{
	void EditorOutputLog::Add(EditorLogLevel level, const std::string& message)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		entries_.push_back({ level, message });
	}

	void EditorOutputLog::Info(const std::string& message)
	{
		Add(EditorLogLevel::Info, message);
	}

	void EditorOutputLog::Warning(const std::string& message)
	{
		Add(EditorLogLevel::Warning, message);
	}

	void EditorOutputLog::Error(const std::string& message)
	{
		Add(EditorLogLevel::Error, message);
	}

	void EditorOutputLog::Clear()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		entries_.clear();
	}

	std::vector<EditorLogEntry> EditorOutputLog::GetEntries() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return entries_;
	}

	const char* ToString(EditorLogLevel level)
	{
		switch (level)
		{
		case EditorLogLevel::Info:
			return "Info";
		case EditorLogLevel::Warning:
			return "Warning";
		case EditorLogLevel::Error:
			return "Error";
		default:
			return "Info";
		}
	}

} // namespace Ken4lowEngine
