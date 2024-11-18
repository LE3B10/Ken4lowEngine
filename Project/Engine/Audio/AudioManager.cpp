//#include "AudioManager.h"
//
//
//#pragma comment(lib, "xaudio2.lib")
//
//
///// -------------------------------------------------------------
/////				　			初期化処理
///// -------------------------------------------------------------
//void AudioManager::Initialize()
//{
//	HRESULT result{};
//
//	// XAudioエンジンのインスタンスを生成
//	result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
//
//	// マスターボイスを生成
//	result = xAudio2->CreateMasteringVoice(&masterVoice);
//}
//
//
///// -------------------------------------------------------------
/////				　	　音声データ読み込み
///// -------------------------------------------------------------
//SoundData AudioManager::SoundLoadWave(const char* fileName)
//{
//	// ファイルをバイナリモードで開く
//	std::ifstream file(fileName, std::ios::binary);
//	if (!file.is_open())
//	{
//		throw std::runtime_error("Failed to open file." + std::string(fileName));
//	}
//
//	// RIFFヘッダーを読み込む
//	RiffHeader riff = ReadRiffHeader(file);
//
//	// fmtチャンクを読み込む
//	FormatChunk format = ReadFormatChunk(file);
//
//	// 拡張フィールドの読み込み
//	if (format.chunk.size > sizeof(WAVEFORMAT))
//	{
//		file.read(reinterpret_cast<char*>(&format.fmt.cbSize), sizeof(format.fmt.cbSize));
//	}
//
//	// dataチャンクを探して読み込む
//	ChunkHeader dataHeader;
//	do
//	{
//		// 次のチャンクヘッダーを読み込む
//		dataHeader = ReadChunkHeader(file);
//
//		// dataチャンクでない場合は次のチャンクへ
//		if (strncmp(dataHeader.id, "data", 4) != 0)
//		{
//			file.seekg(dataHeader.size, std::ios_base::cur);
//		}
//
//	} while (strncmp(dataHeader.id, "data", 4) != 0);
//
//	// dataチャンクの波形データを読み込む
//	std::vector<BYTE> buffer = ReadDataChunk(file, dataHeader);
//
//	// 音声データを構築して返す
//	SoundData soundData = {};
//	soundData.wfex = format.fmt;       // 波形フォーマット情報
//	soundData.buffer = std::move(buffer); // 波形データバッファ
//
//	return soundData;
//}
//
//
///// -------------------------------------------------------------
/////				　			音声再生
///// -------------------------------------------------------------
//void AudioManager::SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData)
//{
//	HRESULT result{};
//
//	// ソースボイスを作成
//	IXAudio2SourceVoice* sourceVoice = nullptr;
//
//	result = xAudio2->CreateSourceVoice(&sourceVoice, &soundData.wfex);
//	if (FAILED(result))
//	{
//		throw std::runtime_error("Failed to create source voice.");
//	}
//
//	// バッファの設定
//	XAUDIO2_BUFFER buffer = {};
//
//	// データサイズの範囲を確認
//	if (soundData.buffer.size() > UINT32_MAX)
//	{
//		throw std::runtime_error("Audio data size exceeds the allowable limit for XAudio2.");
//	}
//
//	buffer.AudioBytes = static_cast<UINT32>(soundData.buffer.size()); // バッファサイズ
//	buffer.pAudioData = soundData.buffer.data();                      // バッファのデータ
//	buffer.Flags = XAUDIO2_END_OF_STREAM;                             // 再生終了を示すフラグ
//	
//	// バッファを送信
//	result = sourceVoice->SubmitSourceBuffer(&buffer);
//	if (FAILED(result))
//	{
//		sourceVoice->DestroyVoice(); // ソースボイスの解放
//		throw std::runtime_error("Failed to submit source buffer.");
//	}
//
//	// 再生開始
//	result = sourceVoice->Start(0); // フラグを0にして再生開始
//	if (FAILED(result))
//	{
//		sourceVoice->DestroyVoice(); // ソースボイスの解放
//		throw std::runtime_error("Failed to start playback.");
//	}
//
//	// 再生が完了するまで待機（非同期で処理する場合は別途スレッドなどを使用）
//	std::thread([sourceVoice]() {
//		XAUDIO2_VOICE_STATE state;
//		do
//		{
//			sourceVoice->GetState(&state);
//			Sleep(10);
//		} while (state.BuffersQueued > 0);
//		// 再生終了後にソースボイスを破棄
//		sourceVoice->DestroyVoice();
//		}).detach();
//}
//
//
///// -------------------------------------------------------------
/////				　			メモリ開放
///// -------------------------------------------------------------
//void AudioManager::SoundUnload(SoundData* soundData)
//{
//	if (soundData == nullptr)
//	{
//		return;
//	}
//
//	// バッファをクリア
//	soundData->buffer.clear();
//	soundData->buffer.shrink_to_fit(); // 必要ならバッファを完全解放
//
//	// 他のフィールドを初期化
//	ZeroMemory(&soundData->wfex, sizeof(soundData->wfex));
//}
//
//
///// -------------------------------------------------------------
/////				　	　RIFFヘッダーを読み込む
///// -------------------------------------------------------------
//RiffHeader AudioManager::ReadRiffHeader(std::ifstream& file)
//{
//	RiffHeader riff{};
//
//	// ファイルからRIFFヘッダーを読み込む
//	file.read(reinterpret_cast<char*>(&riff), sizeof(riff));
//
//	// RIFFチャンクIDが"RIFF"であることを確認
//	if (strncmp(riff.chunk.id, "RIFF", 4) != 0)
//	{
//		throw std::runtime_error("Invalid RIFF header.");
//	}
//
//	// タイプが"WAVE"であることを確認
//	if (strncmp(riff.type, "WAVE", 4) != 0)
//	{
//		throw std::runtime_error("Invalid WAVE type.");
//	}
//
//	// 正常に読み込んだRIFFヘッダーを返す
//	return riff;
//}
//
//
///// -------------------------------------------------------------
/////		fmt チャンクを読み込み、音声フォーマット情報を取得
///// -------------------------------------------------------------
//FormatChunk AudioManager::ReadFormatChunk(std::ifstream& file)
//{
//	FormatChunk format{};
//
//	// チャンクヘッダー部分を読み込む
//	file.read(reinterpret_cast<char*>(&format.chunk), sizeof(format.chunk));
//
//	// チャンクIDが"fmt "であることを確認
//	if (strncmp(format.chunk.id, "fmt ", 4) != 0)
//	{
//		throw std::runtime_error("Invalid fmt chunk.");
//	}
//
//	// fmtチャンク本体を読み込む（サイズはチャンクサイズに基づく）
//	file.read(reinterpret_cast<char*>(&format.fmt), format.chunk.size);
//
//	// 読み込んだフォーマットチャンクを返す
//	return format;
//}
//
///// -------------------------------------------------------------
/////				チャンクヘッダーを読み込む　
/////		主にデータチャンクやJUNKチャンクの検出に利用される
///// -------------------------------------------------------------
//ChunkHeader AudioManager::ReadChunkHeader(std::ifstream& file)
//{
//	ChunkHeader chunk{};
//
//	// ファイルからチャンクヘッダーを読み込む
//	file.read(reinterpret_cast<char*>(&chunk), sizeof(chunk));
//
//	// 正しく読み込めたか確認
//	if (file.gcount() != sizeof(chunk))
//	{
//		throw std::runtime_error("Failed to read chunk header.");
//	}
//
//	// 読み込んだチャンクヘッダーを返す
//	return chunk;
//}
//
//
///// -------------------------------------------------------------
/////			データチャンクを読み込み、波形データを返す
///// -------------------------------------------------------------
//std::vector<BYTE> AudioManager::ReadDataChunk(std::ifstream& file, const ChunkHeader& chunkHeader)
//{
//	// チャンクIDが"data"であることを確認
//	if (strncmp(chunkHeader.id, "data", 4) != 0)
//	{
//		throw std::runtime_error("Invalid data chunk.");
//	}
//
//	// チャンクサイズに基づいてデータを格納するバッファを作成
//	std::vector<BYTE> buffer(chunkHeader.size);
//
//	// データ部をファイルから読み込む
//	file.read(reinterpret_cast<char*>(buffer.data()), chunkHeader.size);
//
//	// 読み込んだデータサイズを確認
//	if (file.gcount() != chunkHeader.size)
//	{
//		throw std::runtime_error("Failed to read data chunk.");
//	}
//
//	// 波形データを格納したバッファを返す
//	return buffer;
//}
