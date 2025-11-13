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

	/// <summary>
	/// AudioManager のシングルトンインスタンスを取得します。
	/// </summary>
	/// <returns>AudioManager の唯一のインスタンス。</returns>
	static AudioManager* GetInstance();

	/// <summary>
	/// BGM を再生します。<br/>
	/// 引数のファイルパスから拡張子を判定し、<br/>
	/// ・.wav → WavLoader でストリーミング再生<br/>
	/// ・.mp3 → Mp3Loader でストリーミング再生<br/>
	/// を行います。すでにローダーが存在しない場合は内部で生成します。:contentReference[oaicite:1]{index=1}  
	/// </summary>
	/// <param name="filePath">再生する BGM ファイル名（パス）。</param>
	/// <param name="volume">BGM の音量（0.0f～1.0f を想定）。</param>
	/// <param name="pitch">BGM のピッチ（周波数比）。</param>
	/// <param name="loop">true のとき、ループ再生を行います。</param>
	void PlayBGM(const std::string& filePath, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

	/// <summary>
	/// 効果音(SE) を再生します。<br/>
	/// ・拡張子から WAV / MP3 を判別<br/>
	/// ・AudioCategory::SE のカテゴリー音量と掛け合わせて実音量を決定<br/>
	/// ・SE ごとに一時的な WavLoader / Mp3Loader を生成し、list に保持して再生<br/>
	/// といった処理を行います。:contentReference[oaicite:2]{index=2}  
	/// </summary>
	/// <param name="filePath">SE のファイル名（パス）。</param>
	/// <param name="volume">SE 個別の音量（カテゴリ音量と掛け合わされます）。</param>
	/// <param name="pitch">SE のピッチ（周波数比）。</param>
	/// <param name="loop">ループさせたい場合は true。</param>
	void PlaySE(const std::string& filePath, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

	/// <summary>
	/// ボイス(キャラクターボイスなど) を再生します。<br/>
	/// ・拡張子から WAV / MP3 を判別<br/>
	/// ・AudioCategory::Voice のカテゴリー音量と掛け合わせて実音量を決定<br/>
	/// ・ボイス用の WavLoader / Mp3Loader インスタンスでストリーミング再生<br/>
	/// といった処理を行います。:contentReference[oaicite:3]{index=3}  
	/// </summary>
	/// <param name="filePath">ボイスのファイル名（パス）。</param>
	/// <param name="volume">ボイス個別の音量（カテゴリ音量と掛け合わされます）。</param>
	/// <param name="pitch">ボイスのピッチ（周波数比）。</param>
	/// <param name="loop">ループさせたい場合は true。</param>
	void PlayVoice(const std::string& filePath, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

	/// <summary>
	/// 再生中の BGM を停止します。<br/>
	/// 内部で保持している BGM 用 WavLoader / Mp3Loader があれば、それぞれ StopBGM() を呼び出します。:contentReference[oaicite:4]{index=4}  
	/// </summary>
	void StopBGM();

	/// <summary>
	/// 再生中の BGM を一時停止します。<br/>
	/// 内部の BGM ローダーが存在する場合、それぞれ PauseBGM() を呼び出します。:contentReference[oaicite:5]{index=5}  
	/// </summary>
	void PauseBGM();

	/// <summary>
	/// 一時停止中の BGM を再開します。<br/>
	/// 内部の BGM ローダーが存在する場合、それぞれ ResumeBGM() を呼び出します。:contentReference[oaicite:6]{index=6}  
	/// </summary>
	void ResumeBGM();

	/// <summary>
	/// 毎フレームの更新処理を行います。<br/>
	/// SE / Voice 用のローダーリストから、<br/>
	/// 「再生が終了して不要になったローダーの後始末」などを行うためのフックとして用意しておきます。<br/>
	/// （実装内容は必要に応じて拡張してください）
	/// </summary>
	void Update();

public: /// ---------- セッター ---------- ///

	/// <summary>
	/// カテゴリーごとの基準音量を設定します。<br/>
	/// ・AudioCategory::BGM<br/>
	/// ・AudioCategory::SE<br/>
	/// ・AudioCategory::Voice<br/>
	/// の 3 種類を想定しており、0.0f ～ 1.0f の範囲にクランプして保持します。
	/// </summary>
	/// <param name="category">音量を設定したいカテゴリー。</param>
	/// <param name="volume">設定する音量（0.0f～1.0f）。</param>
	void SetCategoryVolume(AudioCategory category, float volume)
	{
		categoryVolumes[static_cast<int>(category)] = std::clamp(volume, 0.0f, 1.0f);
	}

public: /// ---------- ゲッタ ---------- ///

	/// <summary>
	/// 指定したカテゴリーの基準音量を取得します。<br/>
	/// 実際の再生時には、この値と個別の volume 引数を掛け合わせて使用します。
	/// </summary>
	/// <param name="category">音量を取得したいカテゴリー。</param>
	/// <returns>0.0f～1.0f の範囲の音量。</returns>
	float GetCategoryVolume(AudioCategory category) const
	{
		return categoryVolumes[static_cast<int>(category)];
	}

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

	/// <summary>外部から生成させないためのプライベートコンストラクタ。</summary>
	AudioManager() = default;
	/// <summary>デフォルトデストラクタ。</summary>
	~AudioManager() = default;
	/// <summary>コピーコンストラクタは禁止。</summary>
	AudioManager(const AudioManager&) = delete;
	/// <summary>代入演算子は禁止。</summary>
	AudioManager& operator=(const AudioManager&) = delete;
};
