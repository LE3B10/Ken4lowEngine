#include "TextureConverter.h"
#include <iostream>
#include <windows.h>

using namespace std;
// DirectX名前空間を使用
using namespace DirectX;

/// -------------------------------------------------------------
///			テクスチャをWIC形式からDDS形式に変換する処理
/// -------------------------------------------------------------
void TextureConverter::ConvertTextureWICToDDS(const std::string& filePath, int numOptions, char* options[])
{
	// WIC形式のテクスチャを読み込む
	LoadWICTextureFromFile(filePath);

	// DDS形式に変換して保存する処理をここに追加する
	SaveDDSTextureToFile(numOptions, options);
}

/// -------------------------------------------------------------
///					使用方法を出力する処理
/// -------------------------------------------------------------
void TextureConverter::OoutputUsage()
{
	cout << "画像ファイルをWIC形式からDDS形式に変換します。" << endl;
	cout << endl; // 空行
	cout << "TextureConverter [ドライブ:][パス][ファイル名]" << endl;
	cout << endl; // 空行
	cout << " [ドライブ:][パス][ファイル名]: 変換するWIC形式の画像ファイルのパスを指定します。" << endl;
	cout << endl; // 空行
	cout << " [-ml level]: ミップレベルを指定します。0を指定すると1x1までのフルミップマップチェーンを生成します。" << endl;
}

/// -------------------------------------------------------------
///				テクスチャファイル読み込み(WIC形式)
/// -------------------------------------------------------------
void TextureConverter::LoadWICTextureFromFile(const std::string& filePath)
{
	// ファイルパスをワイド文字列に変換
	std::wstring wideFilePath = ConcertMultiByteStringToWideString(filePath);

	// WIC形式のテクスチャを読み込む
	HRESULT hr = LoadFromWICFile(wideFilePath.c_str(), WIC_FLAGS_NONE, &metadate_, scrachImage_);
	assert(SUCCEEDED(hr) && "WIC形式のテクスチャの読み込みに失敗しました。");

	// フォルダパスとファイル名を分離する
	SeparateFilePath(wideFilePath);
}

/// -------------------------------------------------------------
///			　マルチバイト文字列をワイド文字列に変換
/// -------------------------------------------------------------
std::wstring TextureConverter::ConcertMultiByteStringToWideString(const std::string& multiByteString)
{
	// ワイド文字列に変換した際の文字数を計算
	int filePathBufferSize = MultiByteToWideChar(CP_ACP, 0, multiByteString.c_str(), -1, nullptr, 0);

	// ワイド文字列用のバッファを確保
	std::wstring wideString;
	wideString.resize(filePathBufferSize);

	// マルチバイト文字列をワイド文字列に変換
	MultiByteToWideChar(CP_ACP, 0, multiByteString.c_str(), -1, wideString.data(), filePathBufferSize);

	// ワイド文字列を返す
	return wideString;
}

/// -------------------------------------------------------------
///		　		フォルダパスとファイル名を分離する
/// -------------------------------------------------------------
void TextureConverter::SeparateFilePath(const std::wstring& filePath)
{
	size_t position1 = 0;
	std::wstring exceptExit;

	// 区切り文字 '.' が出てくる一番最後の部分を検索
	position1 = filePath.rfind(L'.');

	// 検索がヒットしたら
	if (position1 != std::wstring::npos)
	{
		// 区切り文字の後ろをファイル拡張子として保存
		extension_ = filePath.substr(position1 + 1, filePath.size() - position1 - 1);

		// 区切り文字の前までを抜き出す
		exceptExit = filePath.substr(0, position1);
	}
	else
	{
		extension_ = L"";
		exceptExit = filePath;
	}

	// 区切り文字 '\\' が出てくる一番最後の部分を検索
	position1 = exceptExit.rfind(L'\\');

	// 検索がヒットしたら
	if (position1 != std::wstring::npos)
	{
		// 区切り文字の前までをフォルダパスとして保存
		directoryPath_ = exceptExit.substr(0, position1);

		// 区切り文字の後ろをファイル名として保存
		fileName_ = exceptExit.substr(position1 + 1, exceptExit.size() - position1 - 1);

		return;
	}

	// 区切り文字 '/' が出てくる一番最後の部分を検索
	position1 = exceptExit.rfind(L'/');

	// 検索がヒットしたら
	if (position1 != std::wstring::npos)
	{
		// 区切り文字の前までをフォルダパスとして保存
		directoryPath_ = exceptExit.substr(0, position1);

		// 区切り文字の後ろをファイル名として保存
		fileName_ = exceptExit.substr(position1 + 1, exceptExit.size() - position1 - 1);
		return;
	}

	// 区切り文字が見つからなかった場合、フォルダパスは空、ファイル名はそのまま保存
	directoryPath_ = L"";
	fileName_ = exceptExit;
}

/// -------------------------------------------------------------
///			 	 DDSテクスチャとしてファイル書き出し
/// -------------------------------------------------------------
void TextureConverter::SaveDDSTextureToFile(int numOptions, char* options[])
{
	HRESULT hr = S_FALSE;

	size_t mipLevel = 0;

	// ミップマップレベル数を計算
	for (int i = 0; i < numOptions; i++)
	{
		if (std::string(options[i]) == "-ml")
		{
			// ミップレベル数を取得
			mipLevel = stoi(options[i + 1]);
			break;
		}
	}

	ScratchImage mipChain;
	// ミップマップの生成
	hr = GenerateMipMaps(scrachImage_.GetImages(), scrachImage_.GetImageCount(), scrachImage_.GetMetadata(), TEX_FILTER_DEFAULT, mipLevel, mipChain);

	// ミップマップ生成に成功した場合
	if (SUCCEEDED(hr))
	{
		// イメージとメタデータを、ミップマップ生成後のものに置き換える
		scrachImage_ = std::move(mipChain);
		metadate_ = scrachImage_.GetMetadata();
	}

	// 圧縮形式 : BC7に変換
	ScratchImage convertedImage;
	hr = Compress(scrachImage_.GetImages(), scrachImage_.GetImageCount(), metadate_,
		DXGI_FORMAT_BC7_UNORM_SRGB, TEX_COMPRESS_BC7_QUICK | TEX_COMPRESS_SRGB_OUT | TEX_COMPRESS_PARALLEL, 1.0f, convertedImage);

	// 圧縮変換に成功した場合
	if (SUCCEEDED(hr))
	{
		// イメージとメタデータを、圧縮後のものに置き換える
		scrachImage_ = std::move(convertedImage);
		metadate_ = scrachImage_.GetMetadata();
	}

	// 読み込んだテクスチャをSRGBA形式で保存する
	metadate_.format = MakeSRGB(metadate_.format);

	// 出力ファイル名を設定する
	std::wstring filePath = directoryPath_ + L"\\" + fileName_ + L".dds";

	// DDS形式でファイルに保存する
	hr = SaveToDDSFile(scrachImage_.GetImages(), scrachImage_.GetImageCount(), metadate_, DDS_FLAGS_NONE, filePath.c_str());
	assert(SUCCEEDED(hr) && "DDS形式でのテクスチャの保存に失敗しました。");
}
