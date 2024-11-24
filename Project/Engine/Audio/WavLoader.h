#pragma once
#include "AudioStructs.h"

#include <fstream>
#include <wrl.h>

using namespace Microsoft::WRL;

/// -------------------------------------------------------------
///				　	　.wavを読み込むクラス
/// -------------------------------------------------------------
class WavLoader
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 音声データの読み込み関数(.wav)
	SoundData SoundLoadWave(const char* fileName);

	// 音声再生
	IXAudio2SourceVoice* SoundPlayWave(const SoundData& soundData);

	// 音量調整
	void SetVolume(IXAudio2SourceVoice* pSourceVoice, float volume);

	// ピッチ調整
	void SetPitch(IXAudio2SourceVoice* pSourcevoice, float pitchRatio);

	// ループ再生
	void SoundPlayWaveLoop(const SoundData& soundData, int loopCount);

	// オーディオエフェクト
	void ApplyReverbEffct();

	// ストリーミング再生
	void StreamAudio(const char* fileName);

	// シーケンス再生
	void PlaySequence(const std::vector<SoundData>& soundSequence);

	// 音声データの解放
	void SoundUnload(SoundData* soundData);

private:

	//// RIFFヘッダーを読み込む
	//RiffHeader ReadRiffHeader(std::ifstream& file);
	//// fmt チャンクを読み込み、音声フォーマット情報を取得
	//FormatChunk ReadFormatChunk(std::ifstream& file);
	//// チャンクヘッダーを読み込む
	//ChunkHeader ReadChunkHeader(std::ifstream& file);
	//// データチャンクを読み込む
	//std::vector<BYTE> ReadDataChunk(std::ifstream& file, const ChunkHeader& chunkHeader);


private: /// ---------- メンバ変数 ---------- ///

	ComPtr<IXAudio2> xAudio2;

	IXAudio2MasteringVoice* masterVoice;

};

