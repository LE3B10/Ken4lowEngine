#pragma once
#include "AudioCategory.h"
#include "MFAudioDecoder.h"

#include <xaudio2.h>
#include <wrl/client.h>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <string>

namespace Ken4lowEngine
{

	class AudioManager
	{
	public:
		static AudioManager* GetInstance();

		// 起動時に一度だけ呼べば十分。未呼び出しでも各再生関数で遅延初期化する。
		bool Initialize();
		void Finalize();
		void Update();

		void PlayBGM(const std::string& filePath, float volume = 1.0f, float pitch = 1.0f, bool loop = false);
		void PlaySE(const std::string& filePath, float volume = 1.0f, float pitch = 1.0f, bool loop = false);
		void PlayVoice(const std::string& filePath, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

		void StopBGM();
		void PauseBGM();
		void ResumeBGM();

		void SetCategoryVolume(AudioCategory category, float volume);
		float GetCategoryVolume(AudioCategory category) const;

	private:
		struct CachedClip
		{
			DecodedAudioData data;
		};

		struct ActiveVoice
		{
			IXAudio2SourceVoice* voice = nullptr;
			std::shared_ptr<CachedClip> clip;
			bool loop = false;
		};

	private:
		AudioManager() = default;
		~AudioManager() = default;
		AudioManager(const AudioManager&) = delete;
		AudioManager& operator=(const AudioManager&) = delete;

		bool EnsureInitialized();
		std::string NormalizePath(const std::string& filePath) const;
		float BuildActualVolume(AudioCategory category, float volume) const;
		std::shared_ptr<CachedClip> LoadClip(const std::string& filePath);
		IXAudio2SourceVoice* CreateVoiceForClip(const CachedClip& clip) const;
		void PlayOneShot(AudioCategory category, const std::string& filePath, float volume, float pitch, bool loop);
		void DestroyVoice(IXAudio2SourceVoice*& voice);
		void CleanupFinishedVoices();

	private:
		bool initialized_ = false;
		float categoryVolumes_[static_cast<int>(AudioCategory::Count)] = { 1.0f, 1.0f, 1.0f };

		Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
		IXAudio2MasteringVoice* masteringVoice_ = nullptr;

		std::unordered_map<std::string, std::weak_ptr<CachedClip>> cache_;
		std::vector<ActiveVoice> activeVoices_;

		IXAudio2SourceVoice* bgmVoice_ = nullptr;
		std::shared_ptr<CachedClip> bgmClip_;
		bool bgmPaused_ = false;

		mutable std::mutex mutex_;
	};

} // namespace Ken4lowEngine
