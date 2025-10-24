#include "AudioManager.h"
#include <filesystem>

/// -------------------------------------------------------------
///				　シングルトンインスタンス取得
/// -------------------------------------------------------------
AudioManager* AudioManager::GetInstance()
{
	static AudioManager instance;
	return &instance;
}

/// -------------------------------------------------------------
///								音楽再生
/// -------------------------------------------------------------
void AudioManager::PlayBGM(const std::string& filePath, float volume, float pitch, bool loop)
{
	// ファイル拡張子取得
	std::filesystem::path path(filePath);
	std::string ext = path.extension().string();

	// BGMカテゴリーの音量取得
	if (ext == ".wav")
	{
		if (!wavLoader_) wavLoader_ = std::make_unique<WavLoader>(); // WAVローダーが存在しない場合に生成
		wavLoader_->StreamAudioAsync(filePath, volume, pitch, loop); // WAVでストリーミング再生
	}
	else if (ext == ".mp3")
	{
		if (!mp3Loader_) mp3Loader_ = std::make_unique<Mp3Loader>(); // MP3ローダーが存在しない場合に生成
		mp3Loader_->StreamAudioAsync(filePath, volume, pitch, loop); // MP3でストリーミング再生
	}
	else
	{
		// 対応していないフォーマット
		OutputDebugStringA(("Unsupported audio format: " + filePath + "\n").c_str());
	}
}

/// -------------------------------------------------------------
///								効果音再生
/// -------------------------------------------------------------
void AudioManager::PlaySE(const std::string& filePath, float volume, float pitch, bool loop)
{
	// ファイル拡張子取得
	std::filesystem::path path(filePath);
	std::string ext = path.extension().string();

	float categoryVolume = GetCategoryVolume(AudioCategory::SE); // SEカテゴリーの音量取得
	float actualVolume = volume * categoryVolume;				 // 実際の音量計算

	// SE再生 : WAV or MP3
	if (ext == ".wav")
	{
		auto loader = std::make_unique<WavLoader>();				   // WAVローダー生成
		loader->StreamAudioAsync(filePath, actualVolume, pitch, loop); // WAVでストリーミング再生
		seWavLoaders_.push_back(std::move(loader));					   // リストに追加
	}
	else if (ext == ".mp3")
	{
		auto loader = std::make_unique<Mp3Loader>();		// MP3ローダー生成
		loader->PlaySEAsync(filePath, actualVolume, pitch); // MP3でSE非同期再生
		seMp3Loaders_.push_back(std::move(loader));			// リストに追加
	}
	else
	{
		// 対応していないフォーマット
		OutputDebugStringA(("Unsupported SE format: " + filePath + "\n").c_str());
	}
}

/// -------------------------------------------------------------
///								ボイス再生
/// -------------------------------------------------------------
void AudioManager::PlayVoice(const std::string& filePath, float volume, float pitch, bool loop)
{
	// ファイル拡張子取得
	std::filesystem::path path(filePath);
	std::string ext = path.extension().string();

	float categoryVolume = GetCategoryVolume(AudioCategory::Voice); // ボイスカテゴリーの音量取得
	float actualVolume = volume * categoryVolume;					// 実際の音量計算

	// ボイス再生 : WAV or MP3
	if (ext == ".wav")
	{
		auto loader = std::make_unique<WavLoader>();					// WAVローダー生成
		loader->StreamAudioAsync(filePath, actualVolume, pitch, loop);	// WAVでストリーミング再生
	}
	else if (ext == ".mp3")
	{
		auto loader = std::make_unique<Mp3Loader>();					// MP3ローダー生成
		loader->StreamAudioAsync(filePath, actualVolume, pitch, loop);  // MP3でストリーミング再生
	}
	else
	{
		// 対応していないフォーマット
		OutputDebugStringA(("Unsupported Voice format: " + filePath + "\n").c_str());
	}
}

/// -------------------------------------------------------------]
///							BGMの停止
/// -------------------------------------------------------------
void AudioManager::StopBGM()
{
	if (wavLoader_) wavLoader_->StopBGM(); // WAVローダーが存在する場合に停止
	if (mp3Loader_) mp3Loader_->StopBGM(); // MP3ローダーが存在する場合に停止
}

/// -------------------------------------------------------------
///						BGMの一時停止
/// -------------------------------------------------------------
void AudioManager::PauseBGM()
{
	if (wavLoader_) wavLoader_->PauseBGM(); // WAVローダーが存在する場合に一時停止
	if (mp3Loader_) mp3Loader_->PauseBGM(); // MP3ローダーが存在する場合に一時停止
}

/// -------------------------------------------------------------
///						BGMの再開
/// -------------------------------------------------------------
void AudioManager::ResumeBGM()
{
	if (wavLoader_) wavLoader_->ResumeBGM(); // WAVローダーが存在する場合に再開
	if (mp3Loader_) mp3Loader_->ResumeBGM(); // MP3ローダーが存在する場合に再開
}
