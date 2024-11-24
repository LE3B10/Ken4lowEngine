#include "WavLoader.h"

#include <cassert>

#pragma comment(lib, "xaudio2.lib")


/// -------------------------------------------------------------
///				　	　		初期化処理
/// -------------------------------------------------------------
void WavLoader::Initialize()
{
	HRESULT result{};

	// XAudioエンジンのインスタンスを生成
	result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);

	// マスターボイスを生成
	result = xAudio2->CreateMasteringVoice(&masterVoice);

}


/// -------------------------------------------------------------
///				　	　音声データ読み込み
/// -------------------------------------------------------------
SoundData WavLoader::SoundLoadWave(const char* fileName)
{
	HRESULT result{};

	// ファイル入力ストリームのインスタンス
	std::ifstream file;
	// .wavファイルをバイナリモードで開く
	file.open(fileName, std::ios_base::binary);
	// ファイルオープン失敗を検出する
	assert(file.is_open());

	// RIFFヘッダーの読み込み
	RiffHeader riff{};
	file.read((char*)&riff, sizeof(riff));
	// ファイルがRIFFかチェック
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0)
	{
		assert(0);
	}
	// タイプがWAVEかチェック
	if (strncmp(riff.type, "WAVE", 4) != 0)
	{
		assert(0);
	}

	// Formatチャンクの読み込み
	FormatChunk format{};
	// チャンクヘッダーの確認
	file.read((char*)&format, sizeof(ChunkHeader));
	if (strncmp(format.chunk.id, "fmt ", 4) != 0)
	{
		assert(0);
	}

	// チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char*)&format.fmt, format.chunk.size);

	// Dataチャンクの読み込み
	ChunkHeader data{};
	file.read((char*)&data, sizeof(data));
	// JUNKチャンクを検出した場合
	if (strncmp(data.id, "JUNK", 4) == 0)
	{
		// 読みより一をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);
		// 再読み込み
		file.read((char*)&data, sizeof(data));
	}

	if (strncmp(data.id, "data", 4) != 0)
	{
		assert(0);
	}

	// Dataチャンクのデータ部（波形データ）の読み込み
	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);

	// Waveファイルを閉じる
	file.close();

	// returnするための音声データ
	SoundData soundData = {};

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}


/// -------------------------------------------------------------
///				　	　		音声再生
/// -------------------------------------------------------------
IXAudio2SourceVoice* WavLoader::SoundPlayWave(const SoundData& soundData)
{
	IXAudio2SourceVoice* pSourceVoice = nullptr;

	// 波形フォーマットを元にSourceVoiceの生成
	HRESULT result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);

	// 再生する波形のデータ形式
	XAUDIO2_BUFFER buffer{};
	buffer.pAudioData = soundData.pBuffer;
	buffer.AudioBytes = soundData.bufferSize;
	buffer.Flags = XAUDIO2_END_OF_STREAM;

	// 波形データの再生
	result = pSourceVoice->SubmitSourceBuffer(&buffer);
	assert(SUCCEEDED(result));

	result = pSourceVoice->Start(0);
	assert(SUCCEEDED(result));

	return pSourceVoice; // 作成したSourceVoiceを返す
}


/// -------------------------------------------------------------
///				　	　		音量調整
/// -------------------------------------------------------------
void WavLoader::SetVolume(IXAudio2SourceVoice* pSourceVoice, float volume)
{
	assert(pSourceVoice != nullptr);
	HRESULT result = pSourceVoice->SetVolume(volume);
	assert(SUCCEEDED(result));
}


/// -------------------------------------------------------------
///				　	　		ピッチ調整
/// -------------------------------------------------------------
void WavLoader::SetPitch(IXAudio2SourceVoice* pSourcevoice, float pitchRatio)
{
	assert(pSourcevoice != nullptr);
	HRESULT result = pSourcevoice->SetFrequencyRatio(pitchRatio);
	assert(SUCCEEDED(result));
}


/// -------------------------------------------------------------
///				　	　		ループ再生
/// -------------------------------------------------------------
void WavLoader::SoundPlayWaveLoop(const SoundData& soundData, int loopCount)
{
	HRESULT result{};

	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);

	XAUDIO2_BUFFER buffer{};
	buffer.pAudioData = soundData.pBuffer;
	buffer.AudioBytes = soundData.bufferSize;
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.LoopCount = loopCount; // 0: ループなし、XAUDIO2_LOOP_INFINITE: 無限ループ

	result = pSourceVoice->SubmitSourceBuffer(&buffer);
	result = pSourceVoice->Start();
}


/// -------------------------------------------------------------
///				　	　オーディオエフェクト
/// -------------------------------------------------------------
void WavLoader::ApplyReverbEffct()
{
	XAUDIO2_EFFECT_DESCRIPTOR effects[] = {
		{/*エフェクトの設定を記述*/}
	};

	XAUDIO2_EFFECT_CHAIN effectChain = { 1, effects };

	HRESULT result = masterVoice->SetEffectChain(&effectChain);
	assert(SUCCEEDED(result));
}


/// -------------------------------------------------------------
///				　	　長いBGMを流す処理
/// -------------------------------------------------------------
void WavLoader::StreamAudio(const char* fileName)
{

}


/// -------------------------------------------------------------
///				　複数の音を連続で再生するシステム
/// -------------------------------------------------------------
void WavLoader::PlaySequence(const std::vector<SoundData>& soundSequence)
{
	for (const auto& soundData : soundSequence)
	{
		SoundPlayWave(soundData);
	}
}


/// -------------------------------------------------------------
///				　	　	音声データ解放
/// -------------------------------------------------------------
void WavLoader::SoundUnload(SoundData* soundData)
{
	// バッファのメモリを解放
	delete[] soundData->pBuffer;

	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}
