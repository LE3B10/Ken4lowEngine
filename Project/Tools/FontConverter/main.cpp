#include <Windows.h>
#include <cstdint>
#include <cassert>

#include "FontConverter.h"

enum class CommandLineArgument : std::uint8_t
{
	kApplicationPath,
	kFilePath,

	NumArguments
};

int wmain(int argc, wchar_t* argv[])
{
	if (argc < static_cast<int>(CommandLineArgument::NumArguments))
	{
		FontConverter::OutputUsage();
		return 1;
	}

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	assert(SUCCEEDED(hr) && "COMライブラリの初期化に失敗しました。");

	FontConverter fontConverter;

	int numOptions = argc - static_cast<int>(CommandLineArgument::NumArguments);
	wchar_t** options = argv + static_cast<int>(CommandLineArgument::NumArguments);

	const bool success = fontConverter.ConvertFont(argv[static_cast<std::uint8_t>(CommandLineArgument::kFilePath)], numOptions, options);

	CoUninitialize();
	return success ? 0 : 1;
}