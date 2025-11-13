#include "WavLoader.h"
#include "WavLoaderException.h"

#pragma comment(lib, "xaudio2.lib")


/// -------------------------------------------------------------
///				　	　		メモリ開放
/// -------------------------------------------------------------
WavLoader::~WavLoader()
{
	StopBGM(); // BGM再生を停止

	if (masterVoice) {
		masterVoice->DestroyVoice(); // マスターボイスの解放
		masterVoice = nullptr;
	}

	xAudio2 = nullptr; // XAudio2エンジンの解放
}


/// -------------------------------------------------------------
///				　	　		初期化処理
/// -------------------------------------------------------------
void WavLoader::Initialize(const std::string& fileName)
{
	HRESULT result{};

	// XAudioエンジンのインスタンスを生成
	result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(result)) {
		throw WavLoaderException("Failed to initialize XAudio2");
	}

	// マスターボイスを生成
	result = xAudio2->CreateMasteringVoice(&masterVoice);
	if (FAILED(result)) {
		throw WavLoaderException("Failed to create MasteringVoice");
	}

	// WAV ファイルを開く
	std::ifstream file(fileName, std::ios::binary);
	if (!file.is_open()) {
		throw WavLoaderException("Failed to open WAV file: " + fileName);
	}

	// WAV ファイルからフォーマット情報を取得
	RiffHeader riff{};
	if (!ReadRiffHeader(file, riff)) {
		throw WavLoaderException("Invalid RIFF header in file: " + fileName);
	}

	// フォーマットチャンク読み込み
	FormatChunk format{};
	if (!ReadFormatChunk(file, format)) {
		throw WavLoaderException("Invalid format chunk in file: " + fileName);
	}

	// SourceVoice を作成
	result = xAudio2->CreateSourceVoice(&pSourceVoice, &format.fmt);
	if (FAILED(result)) {
		throw WavLoaderException("Failed to create SourceVoice");
	}

	OutputDebugStringA("SourceVoice created successfully\n");
}


/// -------------------------------------------------------------
///				　			非同期処理
/// -------------------------------------------------------------
void WavLoader::StreamAudioAsync(const std::string& fileName, float volume, float pitch, bool Loop)
{
	// 既存の再生を停止
	StopBGM();

	// 再生フラグをセット
	isPlaying = true;

	// 再生状態
	playbackState = PlaybackState::Playing;

	// 非同期タスクをstd::asyncで開始
	bgmFuture = std::async(std::launch::async, [this, fileName, volume, pitch, Loop]()
		{
			try {
				std::lock_guard<std::mutex> lock(sourceVoiceMutex); // 排他制御
				StreamAudio(fileName, volume, pitch, Loop);
			}
			catch (const WavLoaderException& e) {
				OutputDebugStringA(("Error in StreamAudio: " + std::string(e.what()) + "\n").c_str());
			}
			catch (const std::exception& e) {
				OutputDebugStringA(("Unexpected error: " + std::string(e.what()) + "\n").c_str());
			}
		});
}


/// -------------------------------------------------------------
///				　		 音楽を止める処理
/// -------------------------------------------------------------
void WavLoader::StopBGM()
{
	isPlaying = false; // 再生フラグを停止
	playbackState = PlaybackState::Stopped; // 停止

	// スレッドが実行中の場合
	if (bgmFuture.valid())
	{
		bgmFuture.get();  // スレッド終了を待機
	}

	if (pSourceVoice) {
		pSourceVoice->Stop();
		pSourceVoice->DestroyVoice();
		pSourceVoice = nullptr;
	}
}


/// -------------------------------------------------------------
///				　		　再生の一時停止
/// -------------------------------------------------------------
void WavLoader::PauseBGM()
{
	if (playbackState == PlaybackState::Playing && pSourceVoice)
	{
		pSourceVoice->Stop(); // 再生を停止
		playbackState = PlaybackState::Paused; // 一時停止
		isPaused = true;
	}

	OutputDebugStringA("Paused\n");
}


/// -------------------------------------------------------------
///				　		一時停止から再開
/// -------------------------------------------------------------
void WavLoader::ResumeBGM()
{
	if (playbackState == PlaybackState::Paused && pSourceVoice)
	{
		pSourceVoice->Start(); // 再生を再開
		playbackState = PlaybackState::Playing; // 再生
		isPaused = false;
	}

	OutputDebugStringA("Resumed\n");
}


/// -------------------------------------------------------------
///                 ストリーミング再生のメイン関数
/// -------------------------------------------------------------
void WavLoader::StreamAudio(const std::string& fileName, float volume, float pitch, bool Loop)
{
	std::string fileDirectory = "Resources/Sounds/" + fileName;

	// 引数を代入
	currentVolume = volume;
	frequencyRatio = pitch;
	loopPlayback = Loop;
	// 初期化処理
	Initialize(fileDirectory);

	HRESULT result{};
	// 音楽ファイルの読み込み
	std::ifstream file(fileDirectory, std::ios::binary);
	if (!file.is_open()) {
		OutputDebugStringA("Failed to open file\n");
		return;
	}

	// 再生を開始
	result = pSourceVoice->Start(0);

	// バッファを用意（1MB）
	const size_t bufferSize = static_cast<size_t>(1) << 20;
	std::vector<char> buffer(bufferSize);

	// ピッチと音量の前回値を記録
	float previousPitch = -1.0f;
	float previousVolume = -1.0f;

	// ストリーミングループ
	do {
		// ピッチと音量を更新（必要に応じて設定変更）
		UpdatePitchAndVolume(pSourceVoice, currentVolume, frequencyRatio, previousPitch, previousVolume);

		// バッファに音声データを読み込み
		file.read(buffer.data(), bufferSize);
		size_t bytesRead = file.gcount(); // 読み込んだバイト数

		// ファイルの終端に達した場合の処理
		if (bytesRead == 0)
		{
			// ループ再生する場合
			if (loopPlayback)
			{
				// ループ再生時はファイルの先頭に戻る
				file.clear(); // EOFフラグをクリア
				file.seekg(sizeof(RiffHeader) + sizeof(FormatChunk) + sizeof(ChunkHeader), std::ios::beg);
				continue;
			}
			else
			{
				break; // 通常再生の場合は終了
			}
		}

		// 音声データをSourceVoiceに送信
		SubmitAudioBuffer(pSourceVoice, buffer.data(), bytesRead);

		// バッファ再生の完了を待機
		WaitForBufferPlayback(pSourceVoice);

	} while (isPlaying); // 再生フラグが有効な間、ループ

	// ファイルを閉じる
	file.close();

	// 再生を停止し、リソースを解放
	pSourceVoice->Stop();
	pSourceVoice->DestroyVoice();
	pSourceVoice = nullptr;
}


/// -------------------------------------------------------------
///				　	　バッファ送信
/// -------------------------------------------------------------
void WavLoader::SubmitAudioBuffer(IXAudio2SourceVoice* voice, const char* buffer, size_t size)
{
	// 指定されたバッファデータをXAudio2に渡す

	XAUDIO2_BUFFER xBuffer = {};
	xBuffer.AudioBytes = static_cast<UINT32>(size);
	xBuffer.pAudioData = reinterpret_cast<const BYTE*>(buffer);

	HRESULT result = voice->SubmitSourceBuffer(&xBuffer);
	if (FAILED(result)) {
		throw WavLoaderException("Failed to submit audio buffer");
	}
}


/// -------------------------------------------------------------
///				　	　バッファ監視
/// -------------------------------------------------------------
void WavLoader::WaitForBufferPlayback(IXAudio2SourceVoice* voice)
{
	// 再生中のバッファキューが空になるまで監視
	XAUDIO2_VOICE_STATE state;
	do {
		voice->GetState(&state);
		Sleep(10);
	} while (state.BuffersQueued > 0 && isPlaying);
}


/// -------------------------------------------------------------
///				　	　RIFFヘッダー読み込み
/// -------------------------------------------------------------
bool WavLoader::ReadRiffHeader(std::ifstream& file, RiffHeader& riff)
{
	// ファイル形式が"RIFF"かつ"Wave"であることを確認
	file.read(reinterpret_cast<char*>(&riff), sizeof(riff));
	return strncmp(riff.chunk.id, "RIFF", 4) == 0 && strncmp(riff.type, "WAVE", 4) == 0;
}


/// -------------------------------------------------------------
///				　	フォーマットチャンク読み込み
/// -------------------------------------------------------------
bool WavLoader::ReadFormatChunk(std::ifstream& file, FormatChunk& format)
{
	// 音声フォーマットの詳細情報を取得
	ChunkHeader header{};
	file.read(reinterpret_cast<char*>(&header), sizeof(header));
	if (strncmp(header.id, "fmt ", 4) != 0) return false;

	file.read(reinterpret_cast<char*>(&format.fmt), header.size);
	return true;
}


/// -------------------------------------------------------------
///				　	　データチャンク探査
/// -------------------------------------------------------------
bool WavLoader::FindDataChunk(std::ifstream& file, ChunkHeader& data)
{
	// "data"チャンクの位置を見つけてヘッダー情報を返す
	while (true)
	{
		file.read(reinterpret_cast<char*>(&data), sizeof(data));
		if (strncmp(data.id, "data", 4) == 0) return true;

		file.seekg(data.size, std::ios::cur);
		if (file.eof()) return false;
	}
}


/// -------------------------------------------------------------
///				　	　ピッチと音量の更新
/// -------------------------------------------------------------
void WavLoader::UpdatePitchAndVolume(IXAudio2SourceVoice* voice, float volume, float pitch, float& previousPitch, float& previousVolume)
{
	// 前回値と異なる場合のみ設定変更を行う

	 // ピッチの変更が必要な場合
	if (voice && pitch != previousPitch)
	{
		voice->SetFrequencyRatio(pitch); // ピッチを設定
		previousPitch = pitch;			  // 記録
	}

	// 音量の変更が必要な場合
	if (voice && volume != previousVolume)
	{
		voice->SetVolume(volume); // 音量を設定
		previousVolume = volume;  // 記録
	}
}
