#pragma once
#include <Windows.h>
#include <string>
#include <format>

///----- ログ出力関連 -----///
void Log(const std::string& message);
std::wstring ConvertString(const std::string& str);
std::string ConvertString(const std::wstring& str);