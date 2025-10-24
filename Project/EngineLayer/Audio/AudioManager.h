#pragma once
#include "AudioCategory.h"
#include "WavLoader.h"
#include "Mp3Loader.h"

#include <string>
#include <list>
#include <memory>

class AudioLoader; // 前方宣言


class AudioManager
{
public: /// ---------- メンバ関数 ---------- /// 

	// シングルトンインスタンス
	static AudioManager* GetInstance();

	/// <summary>
	/// 音楽を再生する
	/// </summary>
	/// <param name="filePath">ファイル名</param>
	/// <param name="volume">音量</param>
	/// <param name="pitch">音の高さ</param>
	/// <param name="loop">ループ再生</param>
	void PlayBGM(const std::string& filePath, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

	/// <summary>
	/// SEを再生する
	/// </summary>
	/// <param name="filePath">SEファイル名</param>
	void PlaySE(const std::string& filePath, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

	/// <summary>
	/// ボイスを再生する
	/// </summary>
	/// <param name="filePath">ボイスファイル名</param>
	void PlayVoice(const std::string& filePath, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

	// 音楽を停止する
	void StopBGM();

	// 音楽を一時停止する
	void PauseBGM();

	// 音楽を再開する
	void ResumeBGM();

	// 更新処理
	void Update();

public: /// ---------- セッター ---------- ///

	/// <summary>
	/// カテゴリーと音量を設定する
	/// </summary>
	/// <param name="category">カテゴリー BGM - SE - Voice</param>
	/// <param name="volume">音量</param>
	void SetCategoryVolume(AudioCategory category, float volume) { categoryVolumes[static_cast<int>(category)] = std::clamp(volume, 0.0f, 1.0f); }

public: /// ---------- ゲッタ ---------- ///

	// カテゴリーを取得する
	float GetCategoryVolume(AudioCategory category) const { return categoryVolumes[static_cast<int>(category)]; }

private: /// ---------- メンバ変数 ---------- ///

	// カテゴリーごとの音量
	float categoryVolumes[static_cast<int>(AudioCategory::Count)] = { 1.0f, 1.0f, 1.0f };

	std::unique_ptr<WavLoader> wavLoader_;					// BGM用WAVローダー
	std::unique_ptr<Mp3Loader> mp3Loader_;					// BGM用WAV/MP3ローダー
	std::list<std::unique_ptr<WavLoader>> seWavLoaders_;	// SE用WAVローダーリスト
	std::list<std::unique_ptr<Mp3Loader>> seMp3Loaders_;	// SE用MP3ローダーリスト
	std::list<std::unique_ptr<WavLoader>> voiceWavLoaders_; // ボイス用WAVローダーリスト
	std::list<std::unique_ptr<Mp3Loader>> voiceMp3Loaders_; // ボイス用MP3ローダーリスト

private: /// ---------- コピー禁止 ---------- ///

	AudioManager() = default;
	~AudioManager() = default;
	AudioManager(const AudioManager&) = delete;
	AudioManager& operator=(const AudioManager&) = delete;
};

