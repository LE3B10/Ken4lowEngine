#pragma once
#include "AudioCategory.h"
#include "WavLoader.h"
#include "Mp3Loader.h"
#include <string>
#include <memory>


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

public: /// ---------- セッター ---------- ///

	/// <summary>
	/// カテゴリーと音量を設定する
	/// </summary>
	/// <param name="category">カテゴリー BGM - SE - Voice</param>
	/// <param name="volume">音量</param>
	void SetCategoryVolume(AudioCategory category, float volume) { categoryVolumes[static_cast<int>(category)] = std::clamp(volume, 0.0f, 1.0f); }

private: /// ---------- ゲッタ ---------- ///

	// カテゴリーを取得する
	float GetCategoryVolume(AudioCategory category) const { return categoryVolumes[static_cast<int>(category)]; }

private: /// ---------- メンバ変数 ---------- ///

	float categoryVolumes[static_cast<int>(AudioCategory::Count)] = { 1.0f, 1.0f, 1.0f };

	std::unique_ptr<WavLoader> wavLoader_;
	std::unique_ptr<Mp3Loader> mp3Loader_;

private: /// ---------- コピー禁止 ---------- ///

	AudioManager() = default;
	~AudioManager() = default;
	AudioManager(const AudioManager&) = delete;
	AudioManager& operator=(const AudioManager&) = delete;
};

