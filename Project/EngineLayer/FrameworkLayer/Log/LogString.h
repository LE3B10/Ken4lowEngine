#pragma once
#include <Windows.h>
#include <string>
#include <format>

// ログ出力
void Log(const std::string& message);
// 文字コードユーティリティ
// string->wstring
std::wstring ConvertString(const std::string& str);
// wstring->string
std::string ConvertString(const std::wstring& str);