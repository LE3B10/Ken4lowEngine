#pragma once
#include "AudioStructs.h"

#include <xaudio2.h>
#include <wrl.h>
#include <string>
#include <future>
#include <mutex>
#include <atomic>


/// -------------------------------------------------------------
///				　	　MP3 を読み込むクラス
/// -------------------------------------------------------------
class Mp3Loader
{
private: /// ---------- 構造体 ---------- ///

	/// ---------- 再生状態 ---------- ///
	enum class PlaybackState
	{
		Stopped, // 停止
		Playing, // 再生
		Paused	 // 一時停止
	};

public: /// ---------- メンバ関数 ---------- ///

	// デストラクタでリソース解放
	~Mp3Loader();

	/// <summary>
	/// ストリーミング再生
	/// </summary>
	/// <param name="fileName : ">ファイル名</param>
	/// <param name="volume : ">音量</param>
	/// <param name="pitch : ">音の高さ</param>
	/// <param name="Loop : ">ループ再生</param>
	void StreamAudioAsync(const std::string& fileName, float volume = 1.0f, float pitch = 1.0f, bool Loop = false);

	/// <summary>
	/// サウンドエフェクトを非同期で再生
	/// </summary>
	/// <param name="fileName">ファイル名</param>
	/// <param name="volume">音量</param>
	/// <param name="pitch">音の高さ</param>
	void PlaySEAsync(const std::string& fileName, float volume, float pitch);

	// BGMを停止
	void StopBGM();

	// BGMを一時停止
	void PauseBGM();

	// BGMを再開
	void ResumeBGM();

public: /// ---------- セッター ---------- ///

	// 音量を設定
	void SetStreamVolume(float volume) { currentVolume = volume; }

	// ピッチを設定
	void SetStreamPitch(float pitch) { frequencyRatio = pitch; }

	// ループ再生を設定
	void SetLoopPlayback(bool loop) { loopPlayback = loop; }

public: /// ---------- ゲッター ---------- ///

	// 再生状態を取得
	PlaybackState GetPlaybackState() const { return playbackState; }

private: /// ---------- メンバ関数 ---------- ///

	// XAudio2の初期化
	void Initialize();

	// ストリーミング再生
	void StreamAudio(const std::string& fileName, float volume, float pitch, bool Loop);

	// 再生中の音声に対して「ピッチ（音の高さ）」と「音量（ボリューム）」を動的に変更する処理 
	void UpdatePitchAndVolume(IXAudio2SourceVoice* voice, float volume, float pitch, float& previousPitch, float& previousVolume);

	// バッファを送信
	void SubmitAudioBuffer(IXAudio2SourceVoice* voice, const void* buffer, size_t size);

	// バッファの再生を待機
	void WaitForBufferPlayback(IXAudio2SourceVoice* voice);

private: /// ---------- メンバ変数 ---------- ///

	// XAudio2のインスタンス
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;

	IXAudio2MasteringVoice* masterVoice = nullptr; // マスターボイス
	IXAudio2SourceVoice* pSourceVoice = nullptr; // ソースボイス

	// MP3デコーダ
	std::future<void> bgmFuture; // 非同期処理用
	std::mutex sourceVoiceMutex; // 排他制御用ミューテックス

	// ストリーミング再生の状態
	std::atomic<PlaybackState> playbackState = PlaybackState::Stopped;

	std::atomic<bool> isPaused = false; // 一時停止フラグ
	std::atomic<bool> isPlaying = false; // 再生フラグ
	std::atomic<bool> loopPlayback = false; // ループ再生フラグ

	// 音量とピッチの設定
	std::atomic<float> currentVolume = 1.0f;
	std::atomic<float> frequencyRatio = 1.0f;
};
