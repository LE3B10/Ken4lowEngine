#include "AudioManager.h"
#include <filesystem>

AudioManager* AudioManager::GetInstance()
{
	static AudioManager instance;
	return &instance;
}

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

void AudioManager::PlaySE(const std::string& filePath, float volume, float pitch, bool loop )
{
	std::filesystem::path path(filePath);
	std::string ext = path.extension().string();

	float categoryVolume = GetCategoryVolume(AudioCategory::SE);
	float actualVolume = volume * categoryVolume;

	if (ext == ".wav")
	{
		auto loader = std::make_unique<WavLoader>();
		loader->StreamAudioAsync(filePath, actualVolume, pitch, loop);
		// 🔁 必要であれば loader を std::list で保持
	}
	else if (ext == ".mp3")
	{
		auto loader = std::make_unique<Mp3Loader>();
		loader->StreamAudioAsync(filePath, actualVolume, pitch, loop);
	}
	else
	{
		OutputDebugStringA(("Unsupported SE format: " + filePath + "\n").c_str());
	}
}

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

void AudioManager::StopBGM()
{
	if (wavLoader_) wavLoader_->StopBGM();
	if (mp3Loader_) mp3Loader_->StopBGM();
}

void AudioManager::PauseBGM()
{
	if (wavLoader_) wavLoader_->PauseBGM();
	if (mp3Loader_) mp3Loader_->PauseBGM();
}

void AudioManager::ResumeBGM()
{
	if (wavLoader_) wavLoader_->ResumeBGM();
	if (mp3Loader_) mp3Loader_->ResumeBGM();
}
