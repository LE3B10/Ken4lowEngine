#include "AudioManager.h"

#include <mfapi.h>
#include <algorithm>
#include <filesystem>
#include <comdef.h>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "propsys.lib")

namespace Ken4lowEngine
{
	namespace
	{
		constexpr DWORD kVoiceReadFlags = XAUDIO2_VOICE_NOSAMPLESPLAYED;
	}

	AudioManager* AudioManager::GetInstance()
	{
		static AudioManager instance;
		return &instance;
	}

	bool AudioManager::Initialize()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (initialized_)
		{
			return true;
		}

		HRESULT hr = MFStartup(MF_VERSION);
		if (FAILED(hr))
		{
			_com_error err(hr);
			OutputDebugStringW((std::wstring(L"MFStartup failed: ") + err.ErrorMessage() + L"\n").c_str());
			return false;
		}

		hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
		if (FAILED(hr))
		{
			_com_error err(hr);
			OutputDebugStringW((std::wstring(L"XAudio2Create failed: ") + err.ErrorMessage() + L"\n").c_str());
			MFShutdown();
			return false;
		}

		hr = xAudio2_->CreateMasteringVoice(&masteringVoice_);
		if (FAILED(hr))
		{
			_com_error err(hr);
			OutputDebugStringW((std::wstring(L"CreateMasteringVoice failed: ") + err.ErrorMessage() + L"\n").c_str());
			xAudio2_.Reset();
			MFShutdown();
			return false;
		}

		initialized_ = true;
		return true;
	}

	void AudioManager::Finalize()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!initialized_)
		{
			return;
		}

		for (auto& active : activeVoices_)
		{
			DestroyVoice(active.voice);
		}
		activeVoices_.clear();

		DestroyVoice(bgmVoice_);
		bgmClip_.reset();
		cache_.clear();

		if (masteringVoice_)
		{
			masteringVoice_->DestroyVoice();
			masteringVoice_ = nullptr;
		}

		xAudio2_.Reset();
		MFShutdown();
		initialized_ = false;
	}

	bool AudioManager::EnsureInitialized()
	{
		return initialized_ || Initialize();
	}

	void AudioManager::Update()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!initialized_)
		{
			return;
		}
		CleanupFinishedVoices();
	}

	void AudioManager::PlayBGM(const std::string& filePath, float volume, float pitch, bool loop)
	{
		if (!EnsureInitialized())
		{
			return;
		}

		std::lock_guard<std::mutex> lock(mutex_);

		StopBGM();

		bgmClip_ = LoadClip(filePath);
		if (!bgmClip_)
		{
			return;
		}

		bgmVoice_ = CreateVoiceForClip(*bgmClip_);
		if (!bgmVoice_)
		{
			bgmClip_.reset();
			return;
		}

		XAUDIO2_BUFFER buffer{};
		buffer.AudioBytes = static_cast<UINT32>(bgmClip_->data.pcmData.size());
		buffer.pAudioData = bgmClip_->data.pcmData.data();
		buffer.Flags = XAUDIO2_END_OF_STREAM;
		if (loop)
		{
			buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
		}

		const float actualVolume = BuildActualVolume(AudioCategory::BGM, volume);
		bgmVoice_->SetVolume(actualVolume);
		bgmVoice_->SetFrequencyRatio(pitch);

		if (FAILED(bgmVoice_->SubmitSourceBuffer(&buffer)))
		{
			DestroyVoice(bgmVoice_);
			bgmClip_.reset();
			OutputDebugStringA("AudioManager: BGM SubmitSourceBuffer failed\n");
			return;
		}

		if (FAILED(bgmVoice_->Start(0)))
		{
			DestroyVoice(bgmVoice_);
			bgmClip_.reset();
			OutputDebugStringA("AudioManager: BGM Start failed\n");
			return;
		}

		bgmPaused_ = false;
	}

	void AudioManager::PlaySE(const std::string& filePath, float volume, float pitch, bool loop)
	{
		if (!EnsureInitialized())
		{
			return;
		}

		std::lock_guard<std::mutex> lock(mutex_);
		PlayOneShot(AudioCategory::SE, filePath, volume, pitch, loop);
	}

	void AudioManager::PlayVoice(const std::string& filePath, float volume, float pitch, bool loop)
	{
		if (!EnsureInitialized())
		{
			return;
		}

		std::lock_guard<std::mutex> lock(mutex_);
		PlayOneShot(AudioCategory::Voice, filePath, volume, pitch, loop);
	}

	void AudioManager::StopBGM()
	{
		if (bgmVoice_)
		{
			bgmVoice_->Stop(0);
			bgmVoice_->FlushSourceBuffers();
			DestroyVoice(bgmVoice_);
		}
		bgmClip_.reset();
		bgmPaused_ = false;
	}

	void AudioManager::PauseBGM()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (bgmVoice_ && !bgmPaused_)
		{
			bgmVoice_->Stop(0);
			bgmPaused_ = true;
		}
	}

	void AudioManager::ResumeBGM()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (bgmVoice_ && bgmPaused_)
		{
			bgmVoice_->Start(0);
			bgmPaused_ = false;
		}
	}

	void AudioManager::SetCategoryVolume(AudioCategory category, float volume)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		categoryVolumes_[static_cast<int>(category)] = std::clamp(volume, 0.0f, 1.0f);

		if (category == AudioCategory::BGM && bgmVoice_)
		{
			bgmVoice_->SetVolume(categoryVolumes_[static_cast<int>(AudioCategory::BGM)]);
		}
	}

	float AudioManager::GetCategoryVolume(AudioCategory category) const
	{
		return categoryVolumes_[static_cast<int>(category)];
	}

	std::string AudioManager::NormalizePath(const std::string& filePath) const
	{
		std::filesystem::path path(filePath);
		if (path.has_parent_path())
		{
			return path.generic_string();
		}
		return (std::filesystem::path("Resources/Sounds") / path).generic_string();
	}

	float AudioManager::BuildActualVolume(AudioCategory category, float volume) const
	{
		return std::clamp(volume, 0.0f, 4.0f) * categoryVolumes_[static_cast<int>(category)];
	}

	std::shared_ptr<AudioManager::CachedClip> AudioManager::LoadClip(const std::string& filePath)
	{
		const std::string normalized = NormalizePath(filePath);

		if (auto found = cache_.find(normalized); found != cache_.end())
		{
			if (auto locked = found->second.lock())
			{
				return locked;
			}
		}

		auto clip = std::make_shared<CachedClip>();
		if (!MFAudioDecoder::DecodeFile(normalized, clip->data))
		{
			OutputDebugStringA(("AudioManager: decode failed -> " + normalized + "\n").c_str());
			return nullptr;
		}

		cache_[normalized] = clip;
		return clip;
	}

	IXAudio2SourceVoice* AudioManager::CreateVoiceForClip(const CachedClip& clip) const
	{
		if (!xAudio2_)
		{
			return nullptr;
		}

		IXAudio2SourceVoice* voice = nullptr;
		HRESULT hr = xAudio2_->CreateSourceVoice(&voice, &clip.data.waveFormat);
		if (FAILED(hr))
		{
			_com_error err(hr);
			OutputDebugStringW((std::wstring(L"CreateSourceVoice failed: ") + err.ErrorMessage() + L"\n").c_str());
			return nullptr;
		}
		return voice;
	}

	void AudioManager::PlayOneShot(AudioCategory category, const std::string& filePath, float volume, float pitch, bool loop)
	{
		auto clip = LoadClip(filePath);
		if (!clip)
		{
			return;
		}

		IXAudio2SourceVoice* voice = CreateVoiceForClip(*clip);
		if (!voice)
		{
			return;
		}

		XAUDIO2_BUFFER buffer{};
		buffer.AudioBytes = static_cast<UINT32>(clip->data.pcmData.size());
		buffer.pAudioData = clip->data.pcmData.data();
		buffer.Flags = XAUDIO2_END_OF_STREAM;
		if (loop)
		{
			buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
		}

		voice->SetVolume(BuildActualVolume(category, volume));
		voice->SetFrequencyRatio(pitch);

		if (FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->Start(0)))
		{
			DestroyVoice(voice);
			OutputDebugStringA("AudioManager: one shot start failed\n");
			return;
		}

		activeVoices_.push_back({ voice, std::move(clip), loop });
	}

	void AudioManager::DestroyVoice(IXAudio2SourceVoice*& voice)
	{
		if (voice)
		{
			voice->Stop(0);
			voice->FlushSourceBuffers();
			voice->DestroyVoice();
			voice = nullptr;
		}
	}

	void AudioManager::CleanupFinishedVoices()
	{
		activeVoices_.erase(
			std::remove_if(activeVoices_.begin(), activeVoices_.end(),
				[this](ActiveVoice& active)
				{
					if (!active.voice)
					{
						return true;
					}

					if (active.loop)
					{
						return false;
					}

					XAUDIO2_VOICE_STATE state{};
					active.voice->GetState(&state, kVoiceReadFlags);
					if (state.BuffersQueued == 0)
					{
						DestroyVoice(active.voice);
						active.clip.reset();
						return true;
					}
					return false;
				}),
			activeVoices_.end());
	}

} // namespace Ken4lowEngine
