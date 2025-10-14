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
	std::filesystem::path path(filePath);
	std::string ext = path.extension().string();

	if (ext == ".wav")
	{
		if (!wavLoader_) wavLoader_ = std::make_unique<WavLoader>();
		wavLoader_->StreamAudioAsync(filePath, volume, pitch, loop);
	}
	else if (ext == ".mp3")
	{
		if (!mp3Loader_) mp3Loader_ = std::make_unique<Mp3Loader>();
		mp3Loader_->StreamAudioAsync(filePath, volume, pitch, loop);
	}
	else
	{
		OutputDebugStringA(("Unsupported audio format: " + filePath + "\n").c_str());
	}
}

/// -------------------------------------------------------------
///								効果音再生
/// -------------------------------------------------------------
void AudioManager::PlaySE(const std::string& filePath, float volume, float pitch, bool loop)
{
	std::filesystem::path path(filePath);
	std::string ext = path.extension().string();

	float categoryVolume = GetCategoryVolume(AudioCategory::SE);
	float actualVolume = volume * categoryVolume;

	if (ext == ".wav")
	{
		auto loader = std::make_unique<WavLoader>();
		loader->StreamAudioAsync(filePath, actualVolume, pitch, loop);
		seWavLoaders_.push_back(std::move(loader));
	}
	else if (ext == ".mp3")
	{
		auto loader = std::make_unique<Mp3Loader>();
		loader->PlaySEAsync(filePath, actualVolume, pitch);
		seMp3Loaders_.push_back(std::move(loader));
	}
	else
	{
		OutputDebugStringA(("Unsupported SE format: " + filePath + "\n").c_str());
	}
}

/// -------------------------------------------------------------
///								ボイス再生
/// -------------------------------------------------------------
void AudioManager::PlayVoice(const std::string& filePath, float volume, float pitch, bool loop)
{
	std::filesystem::path path(filePath);
	std::string ext = path.extension().string();

	float categoryVolume = GetCategoryVolume(AudioCategory::Voice);
	float actualVolume = volume * categoryVolume;

	if (ext == ".wav")
	{
		auto loader = std::make_unique<WavLoader>();
		loader->StreamAudioAsync(filePath, actualVolume, pitch, loop);
	}
	else if (ext == ".mp3")
	{
		auto loader = std::make_unique<Mp3Loader>();
		loader->StreamAudioAsync(filePath, actualVolume, pitch, loop);
	}
	else
	{
		OutputDebugStringA(("Unsupported Voice format: " + filePath + "\n").c_str());
	}
}

/// -------------------------------------------------------------]
///							BGMの停止
/// -------------------------------------------------------------
void AudioManager::StopBGM()
{
	if (wavLoader_) wavLoader_->StopBGM();
	if (mp3Loader_) mp3Loader_->StopBGM();
}

/// -------------------------------------------------------------
///						BGMの一時停止
/// -------------------------------------------------------------
void AudioManager::PauseBGM()
{
	if (wavLoader_) wavLoader_->PauseBGM();
	if (mp3Loader_) mp3Loader_->PauseBGM();
}

/// -------------------------------------------------------------
///						BGMの再開
/// -------------------------------------------------------------
void AudioManager::ResumeBGM()
{
	if (wavLoader_) wavLoader_->ResumeBGM();
	if (mp3Loader_) mp3Loader_->ResumeBGM();
}
