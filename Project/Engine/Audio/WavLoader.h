#pragma once
#include "AudioStructs.h"

#include <thread> // スレッド用
#include <atomic> // 再生フラグ用
#include <mutex>
#include <fstream>

#include <wrl.h>

// 省略
using namespace Microsoft::WRL;

/// -------------------------------------------------------------
///				　	　.wavを読み込むクラス
/// -------------------------------------------------------------
class WavLoader
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタでリソース解放
	~WavLoader();

	// 初期化処理
	void Initialize(const char* fileName);

	// ストリーミング再生（非同期処理）
	void StreamAudioAsync(const char* fileName, float volume = 1.0f, float pitch = 1.0f, bool Loop = false);

	// 音楽を止める
	void StopBGM();

public: /// ---------- セッター ---------- ///

	// 音量調節
	void SetStreamVolume(float volume) { currentVolume = volume; }

	// ピッチ調整
	void SetStreamPitch(float pitch) { frequencyRatio = pitch; }

	// ループ再生
	void SetLoopPlayback(bool loop) { loopPlayback = loop; }

private: /// ---------- メンバ関数 ---------- ///

	// 実際の再生処理
	void StreamAudio(const char* fileName, float volume, float pitch, bool Loop);

	// ピッチと音量調整
	void UpdatePitchAndVolume(IXAudio2SourceVoice* voice, float volume, float pitch, float& previousPitch, float& previousVolume);

	// バッファ送信
	void SubmitAudioBuffer(IXAudio2SourceVoice* voice, const char* buffer, size_t size);

	// バッファ監視
	void WaitForBufferPlayback(IXAudio2SourceVoice* voice);

	// RIFFヘッダー読み込み
	bool ReadRiffHeader(std::ifstream& file, RiffHeader& riff);

	// フォーマットチャンク読み込み
	bool ReadFormatChunk(std::ifstream& file, FormatChunk& format);

	// データチャンク探査
	bool FindDataChunk(std::ifstream& file, ChunkHeader& data);


private: /// ---------- メンバ変数 ---------- ///

	IXAudio2SourceVoice* pSourceVoice = nullptr;
	std::mutex sourceVoiceMutex;

	ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice;
	std::atomic<bool> isPlaying; // 再生フラグ
	std::thread bgmThread;		 // BGM再生スレッド

	std::atomic<float> currentVolume = 1.0f;  // デフォルト音量
	std::atomic<float> frequencyRatio = 1.0f; // デフォルトは通常再生
	std::atomic<bool> loopPlayback = false;	  // ループ再生フラグ
};

