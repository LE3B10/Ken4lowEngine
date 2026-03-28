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
		// GetState() で総再生サンプル数は不要なので、最小限の情報だけを取得する
		constexpr DWORD kVoiceReadFlags = XAUDIO2_VOICE_NOSAMPLESPLAYED;
	}

	AudioManager* AudioManager::GetInstance()
	{
		// 初回呼び出し時に 1 度だけ生成されるシングルトン
		static AudioManager instance;
		return &instance;
	}

	bool AudioManager::Initialize()
	{
		std::lock_guard<std::mutex> lock(mutex_);

		// すでに初期化済みなら何もしない
		if (initialized_) return true;

		// Media Foundation を初期化する
		// 音声ファイルのデコードに使用する
		HRESULT hr = MFStartup(MF_VERSION);
		if (FAILED(hr))
		{
			_com_error err(hr);
			OutputDebugStringW((std::wstring(L"MFStartup failed: ") + err.ErrorMessage() + L"\n").c_str());
			return false;
		}

		// XAudio2 本体を作成する
		// ここから各 SourceVoice を生成して再生を行う
		hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
		if (FAILED(hr))
		{
			_com_error err(hr);
			OutputDebugStringW((std::wstring(L"XAudio2Create failed: ") + err.ErrorMessage() + L"\n").c_str());
			MFShutdown();
			return false;
		}

		// 最終出力先となるマスターボイスを作成する
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

		// 未初期化なら終了処理は不要
		if (!initialized_)
		{
			return;
		}

		// 再生中のワンショット Voice をすべて破棄する
		for (auto& active : activeVoices_)
		{
			DestroyVoice(active.voice);
		}
		activeVoices_.clear();

		// BGM 用 Voice も停止して破棄する
		DestroyVoice(bgmVoice_);
		bgmClip_.reset();
		bgmBaseVolume_ = 1.0f;

		// デコード済みキャッシュを破棄する
		cache_.clear();

		// マスターボイスを破棄する
		if (masteringVoice_)
		{
			masteringVoice_->DestroyVoice();
			masteringVoice_ = nullptr;
		}

		// XAudio2 と Media Foundation を終了する
		xAudio2_.Reset();
		MFShutdown();

		initialized_ = false;
	}

	bool AudioManager::EnsureInitialized()
	{
		// すでに初期化済みならそのまま true
		// 未初期化ならここで初期化を試みる
		return initialized_ || Initialize();
	}

	void AudioManager::Update()
	{
		std::lock_guard<std::mutex> lock(mutex_);

		// 未初期化なら何もしない
		if (!initialized_)
		{
			return;
		}

		// 再生が終わった Voice を回収する
		CleanupFinishedVoices();
	}

	void AudioManager::PlayBGM(const std::string& filePath, float volume, float pitch, bool loop)
	{
		// 未初期化ならここで初期化を試みる
		if (!EnsureInitialized())
		{
			return;
		}

		std::lock_guard<std::mutex> lock(mutex_);

		// 既存の BGM が鳴っていれば停止して差し替える
		StopBGM();

		// 後からカテゴリ音量変更が来ても個別音量を保持できるよう保存する
		bgmBaseVolume_ = std::clamp(volume, 0.0f, 4.0f);

		// 音声ファイルを読み込み、デコード済みデータを取得する
		bgmClip_ = LoadClip(filePath);
		if (!bgmClip_)
		{
			bgmBaseVolume_ = 1.0f;
			return;
		}

		// クリップの波形情報から BGM 用 SourceVoice を作成する
		bgmVoice_ = CreateVoiceForClip(*bgmClip_);
		if (!bgmVoice_)
		{
			bgmClip_.reset();
			bgmBaseVolume_ = 1.0f;
			return;
		}

		// PCM データ全体を 1 つのバッファとして登録する
		XAUDIO2_BUFFER buffer{};
		buffer.AudioBytes = static_cast<UINT32>(bgmClip_->data.pcmData.size());
		buffer.pAudioData = bgmClip_->data.pcmData.data();
		buffer.Flags = XAUDIO2_END_OF_STREAM;

		// ループ再生時は無限ループ設定にする
		if (loop)
		{
			buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
		}

		// 引数音量とカテゴリ音量を掛け合わせた最終音量を適用する
		const float actualVolume = BuildActualVolume(AudioCategory::BGM, bgmBaseVolume_);
		bgmVoice_->SetVolume(actualVolume);

		// 再生ピッチを設定する
		bgmVoice_->SetFrequencyRatio(pitch);

		// バッファ登録に失敗した場合は後始末して終了
		if (FAILED(bgmVoice_->SubmitSourceBuffer(&buffer)))
		{
			DestroyVoice(bgmVoice_);
			bgmClip_.reset();
			bgmBaseVolume_ = 1.0f;
			OutputDebugStringA("AudioManager: BGM SubmitSourceBuffer failed\n");
			return;
		}

		// 再生開始に失敗した場合も同様に後始末して終了
		if (FAILED(bgmVoice_->Start(0)))
		{
			DestroyVoice(bgmVoice_);
			bgmClip_.reset();
			bgmBaseVolume_ = 1.0f;
			OutputDebugStringA("AudioManager: BGM Start failed\n");
			return;
		}

		bgmPaused_ = false;
	}

	void AudioManager::PlaySE(const std::string& filePath, float volume, float pitch, bool loop)
	{
		// 未初期化ならここで初期化を試みる
		if (!EnsureInitialized())
		{
			return;
		}

		std::lock_guard<std::mutex> lock(mutex_);

		// 効果音カテゴリとしてワンショット再生する
		PlayOneShot(AudioCategory::SE, filePath, volume, pitch, loop);
	}

	void AudioManager::PlayVoice(const std::string& filePath, float volume, float pitch, bool loop)
	{
		// 未初期化ならここで初期化を試みる
		if (!EnsureInitialized())
		{
			return;
		}

		std::lock_guard<std::mutex> lock(mutex_);

		// ボイスカテゴリとしてワンショット再生する
		PlayOneShot(AudioCategory::Voice, filePath, volume, pitch, loop);
	}

	void AudioManager::StopBGM()
	{
		// BGM 用 Voice が存在するなら停止して破棄する
		if (bgmVoice_)
		{
			bgmVoice_->Stop(0);
			bgmVoice_->FlushSourceBuffers();
			DestroyVoice(bgmVoice_);
		}

		// BGM 用クリップ参照と保持していた個別音量も解放・初期化する
		bgmClip_.reset();
		bgmBaseVolume_ = 1.0f;
		bgmPaused_ = false;
	}

	void AudioManager::PauseBGM()
	{
		std::lock_guard<std::mutex> lock(mutex_);

		// 再生中の BGM がある場合だけ一時停止する
		if (bgmVoice_ && !bgmPaused_)
		{
			bgmVoice_->Stop(0);
			bgmPaused_ = true;
		}
	}

	void AudioManager::ResumeBGM()
	{
		std::lock_guard<std::mutex> lock(mutex_);

		// 一時停止中の BGM がある場合だけ再開する
		if (bgmVoice_ && bgmPaused_)
		{
			bgmVoice_->Start(0);
			bgmPaused_ = false;
		}
	}

	void AudioManager::SetCategoryVolume(AudioCategory category, float volume)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		// カテゴリ音量は 0.0f ～ 1.0f に丸めて保持する
		categoryVolumes_[static_cast<int>(category)] = std::clamp(volume, 0.0f, 1.0f);

		// BGM は現在再生中の Voice に即時反映する
		// 再生開始時の個別音量を保持しておき、カテゴリ音量変更時も掛け合わせた値を使う
		if (category == AudioCategory::BGM && bgmVoice_)
		{
			bgmVoice_->SetVolume(BuildActualVolume(AudioCategory::BGM, bgmBaseVolume_));
		}
	}

	float AudioManager::GetCategoryVolume(AudioCategory category) const
	{
		return categoryVolumes_[static_cast<int>(category)];
	}

	std::string AudioManager::NormalizePath(const std::string& filePath) const
	{
		std::filesystem::path path(filePath);

		// すでに親フォルダ付きならそのまま使う
		if (path.has_parent_path())
		{
			return path.generic_string();
		}

		// ファイル名だけなら Resources/Sounds 配下として扱う
		return (std::filesystem::path("Resources/Sounds") / path).generic_string();
	}

	float AudioManager::BuildActualVolume(AudioCategory category, float volume) const
	{
		// 呼び出し側の音量を安全な範囲に丸めたうえで、カテゴリ音量を掛け合わせる
		return std::clamp(volume, 0.0f, 4.0f) * categoryVolumes_[static_cast<int>(category)];
	}

	std::shared_ptr<AudioManager::CachedClip> AudioManager::LoadClip(const std::string& filePath)
	{
		// キャッシュキーとして扱いやすいようにパスを正規化する
		const std::string normalized = NormalizePath(filePath);

		// すでにデコード済みならキャッシュを再利用する
		if (auto found = cache_.find(normalized); found != cache_.end())
		{
			if (auto locked = found->second.lock())
			{
				return locked;
			}
		}

		// 未キャッシュなら新しくデコードする
		auto clip = std::make_shared<CachedClip>();
		if (!MFAudioDecoder::DecodeFile(normalized, clip->data))
		{
			OutputDebugStringA(("AudioManager: decode failed -> " + normalized + "\n").c_str());
			return nullptr;
		}

		// 後続の再利用のためキャッシュに登録する
		cache_[normalized] = clip;
		return clip;
	}

	IXAudio2SourceVoice* AudioManager::CreateVoiceForClip(const CachedClip& clip) const
	{
		// XAudio2 が無効なら Voice は作れない
		if (!xAudio2_)
		{
			return nullptr;
		}

		IXAudio2SourceVoice* voice = nullptr;

		// デコード済みの波形フォーマットを使って SourceVoice を生成する
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
		// 再生対象のクリップを取得する
		auto clip = LoadClip(filePath);
		if (!clip)
		{
			return;
		}

		// そのクリップ専用の SourceVoice を作成する
		IXAudio2SourceVoice* voice = CreateVoiceForClip(*clip);
		if (!voice)
		{
			return;
		}

		// PCM データ全体を 1 回分の再生バッファとして登録する
		XAUDIO2_BUFFER buffer{};
		buffer.AudioBytes = static_cast<UINT32>(clip->data.pcmData.size());
		buffer.pAudioData = clip->data.pcmData.data();
		buffer.Flags = XAUDIO2_END_OF_STREAM;

		// loop=true の場合はループ再生も可能
		if (loop)
		{
			buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
		}

		// カテゴリ別音量とピッチを適用する
		voice->SetVolume(BuildActualVolume(category, volume));
		voice->SetFrequencyRatio(pitch);

		// バッファ登録または再生開始に失敗したら Voice を破棄する
		if (FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->Start(0)))
		{
			DestroyVoice(voice);
			OutputDebugStringA("AudioManager: one shot start failed\n");
			return;
		}

		// 再生中に PCM データが消えないよう clip を保持しつつ管理リストに登録する
		activeVoices_.push_back({ voice, std::move(clip), loop });
	}

	void AudioManager::DestroyVoice(IXAudio2SourceVoice*& voice)
	{
		// 有効な Voice があるときだけ安全に破棄する
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
		// 再生終了したワンショット Voice を一覧から削除する
		activeVoices_.erase(
			std::remove_if(activeVoices_.begin(), activeVoices_.end(),
				[this](ActiveVoice& active)
				{
					// すでに無効な Voice は削除対象
					if (!active.voice)
					{
						return true;
					}

					// ループ再生中のものはここでは回収しない
					if (active.loop)
					{
						return false;
					}

					// 再生キューが空なら再生終了とみなして破棄する
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