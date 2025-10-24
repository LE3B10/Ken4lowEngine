#include "Mp3Loader.h"
#include <fstream>

#pragma comment(lib, "xaudio2.lib")

/// ---------- minimp3の実装 ---------- ///
#define MINIMP3_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable: 4244)  // narrowing (int→char)
#pragma warning(disable: 4267)  // size_t→int など
#pragma warning(disable: 4456)  // ローカル変数隠蔽
#pragma warning(disable: 4459)  // 外部スコープ変数隠蔽
#include "minimp3_ex.h"
#pragma warning(pop)

/// -------------------------------------------------------------
///				　	　	　デストラクタ
/// -------------------------------------------------------------
Mp3Loader::~Mp3Loader()
{
	StopBGM();
	if (masterVoice)
	{
		masterVoice->DestroyVoice();
		masterVoice = nullptr;
	}
	xAudio2.Reset();
}


/// -------------------------------------------------------------
///				　	　	ストリーミング再生
/// -------------------------------------------------------------
void Mp3Loader::StreamAudioAsync(const std::string& fileName, float volume, float pitch, bool Loop)
{
	StopBGM();
	isPlaying = true;
	playbackState = PlaybackState::Playing;

	bgmFuture = std::async(std::launch::async, [this, fileName, volume, pitch, Loop]() {
		try {
			std::lock_guard<std::mutex> lock(sourceVoiceMutex);
			StreamAudio(fileName, volume, pitch, Loop);
		}
		catch (const std::exception& e) {
			OutputDebugStringA(("MP3 ERROR: " + std::string(e.what()) + "\n").c_str());
		}
		});
}


/// -------------------------------------------------------------
///				　	サウンドエフェクトを非同期で再生
/// -------------------------------------------------------------
void Mp3Loader::PlaySEAsync(const std::string& fileName, float volume, float pitch)
{
	std::thread([fileName, volume, pitch]() {
		Mp3Loader* loader = new Mp3Loader();

		try {
			loader->Initialize();

			std::string fileDirectory = "Resources/Sounds/" + fileName;

			mp3dec_ex_t mp3;
			if (mp3dec_ex_open(&mp3, fileDirectory.c_str(), MP3D_SEEK_TO_SAMPLE)) {
				OutputDebugStringA("Failed to decode MP3 SE\n");
				delete loader;
				return;
			}

			WAVEFORMATEX format = {};
			format.wFormatTag = WAVE_FORMAT_PCM;
			format.nChannels = static_cast<WORD>(mp3.info.channels);
			format.nSamplesPerSec = mp3.info.hz;
			format.wBitsPerSample = 16;
			format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
			format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

			IXAudio2SourceVoice* voice = nullptr;
			HRESULT result = loader->xAudio2->CreateSourceVoice(&voice, &format);
			if (FAILED(result)) {
				OutputDebugStringA("Failed to create SE Voice\n");
				mp3dec_ex_close(&mp3);
				delete loader;
				return;
			}

			const size_t bufferSamples = mp3.info.channels * 1152 * static_cast<size_t>(1) << 8; // 1MB相当のバッファサイズ
			std::vector<short> pcm(bufferSamples);
			size_t samplesRead = mp3dec_ex_read(&mp3, pcm.data(), bufferSamples);

			XAUDIO2_BUFFER buffer = {};
			buffer.AudioBytes = static_cast<UINT32>(samplesRead * sizeof(short));
			buffer.pAudioData = reinterpret_cast<BYTE*>(pcm.data());
			buffer.Flags = XAUDIO2_END_OF_STREAM;

			voice->SetVolume(volume);
			voice->SetFrequencyRatio(pitch);
			voice->SubmitSourceBuffer(&buffer);
			voice->Start();

			// 再生が終わるまで待つ
			XAUDIO2_VOICE_STATE state{};
			do {
				voice->GetState(&state);
				Sleep(10);
			} while (state.BuffersQueued > 0);

			voice->DestroyVoice();
			mp3dec_ex_close(&mp3);
		}
		catch (...) {
			OutputDebugStringA("SE playback failed\n");
		}

		delete loader;
		}).detach();
}


/// -------------------------------------------------------------
///				　	　	　	　停止
/// -------------------------------------------------------------
void Mp3Loader::StopBGM()
{
	isPlaying = false;

	if (bgmFuture.valid()) {
		bgmFuture.get(); // 再生スレッドが終了するのを待つ
	}

	if (pSourceVoice) {
		pSourceVoice->Stop();
		pSourceVoice->DestroyVoice();
		pSourceVoice = nullptr;
	}
}


/// -------------------------------------------------------------
///				　	　		一時停止
/// -------------------------------------------------------------
void Mp3Loader::PauseBGM()
{
	// 再生中で、ボイスが有効なら停止処理
	if (playbackState == PlaybackState::Playing && pSourceVoice)
	{
		pSourceVoice->Stop(); // 一時停止
		playbackState = PlaybackState::Paused;
		isPaused = true;

		OutputDebugStringA("MP3 Paused.\n");
	}
}


/// -------------------------------------------------------------
///				　	　一時停止からの再開
/// -------------------------------------------------------------
void Mp3Loader::ResumeBGM()
{
	// 一時停止中で、ボイスが有効なら再開処理
	if (playbackState == PlaybackState::Paused && pSourceVoice)
	{
		pSourceVoice->Start(); // 再開
		playbackState = PlaybackState::Playing;
		isPaused = false;

		OutputDebugStringA("MP3 Resumed.\n");
	}
}


/// -------------------------------------------------------------
///	　XAudio2の初期化（インスタンス生成＋マスターボイス作成）
/// -------------------------------------------------------------
void Mp3Loader::Initialize()
{
	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(result)) throw std::runtime_error("Failed to initialize XAudio2");

	result = xAudio2->CreateMasteringVoice(&masterVoice);
	if (FAILED(result)) throw std::runtime_error("Failed to create MasteringVoice");
}


/// -------------------------------------------------------------
///				　	　	ストリーミング再生
/// -------------------------------------------------------------
void Mp3Loader::StreamAudio(const std::string& fileName, float volume, float pitch, bool Loop)
{
	// 再生するファイルのフルパスを構築
	std::string fileDirectory = "Resources/Sounds/" + fileName;

	// XAudio2の初期化（インスタンス生成＋マスターボイス作成）
	Initialize();

	// MP3デコーダを初期化し、ファイルを開く
	mp3dec_ex_t mp3;
	if (mp3dec_ex_open(&mp3, fileDirectory.c_str(), MP3D_SEEK_TO_SAMPLE))
	{
		OutputDebugStringA(("Failed to decode MP3: " + fileDirectory + "\n").c_str());
		return;
	}

	// デコーダから取得した情報をもとに音声フォーマットを構築
	WAVEFORMATEX format = {};
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = static_cast<WORD>(mp3.info.channels);
	format.nSamplesPerSec = mp3.info.hz;
	format.wBitsPerSample = 16;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
	format.cbSize = 0;

	// XAudio2のソースボイスを作成（再生用のチャンネル）
	HRESULT result = xAudio2->CreateSourceVoice(&pSourceVoice, &format);
	if (FAILED(result))
	{
		OutputDebugStringA("Failed to create SourceVoice\n");
		mp3dec_ex_close(&mp3);
		return;
	}

	// ソースボイスを開始（実際の再生処理が可能になる）
	result = pSourceVoice->Start(0);
	if (FAILED(result))
	{
		OutputDebugStringA("Failed to start SourceVoice\n");
		mp3dec_ex_close(&mp3);
		return;
	}

	// 一時的にPCMデータを保存するバッファ（1MB相当）
	/*const size_t bufferSamples = static_cast<size_t>(1152) * mp3.info.channels * static_cast<size_t>(1) << 20;
	std::vector<short> pcmBuffer(bufferSamples);*/

	// 目標チャンク
	const size_t targetBytes = 1ull << 20; // 1MB
	const size_t bytesPerSample = sizeof(short) * mp3.info.channels; // 1サンプルあたりのバイト数
	const size_t bufferSamples = targetBytes / bytesPerSample; // チャンクあたりのサンプル数

	std::vector<short> pcmBuffer(bufferSamples);

	// ピッチ・音量の初期値（変更検出用）
	float previousPitch = -1.0f;
	float previousVolume = -1.0f;

	// 再生ループ
	do {
		// ピッチ・音量が変化していたら更新
		UpdatePitchAndVolume(pSourceVoice, volume, pitch, previousPitch, previousVolume);

		// MP3データをPCMにデコードしてバッファへ格納
		size_t samplesRead = mp3dec_ex_read(&mp3, pcmBuffer.data(), bufferSamples);
		if (samplesRead == 0)
		{
			// ループ再生する場合はファイル先頭に巻き戻す
			if (Loop)
			{
				mp3dec_ex_seek(&mp3, 0);
				continue;
			}
			// 通常再生なら終了
			break;
		}

		// 読み込んだPCMデータをXAudio2に送信して再生
		SubmitAudioBuffer(pSourceVoice, pcmBuffer.data(), samplesRead * sizeof(short));

		// 送信したバッファがすべて再生されるまで待機
		WaitForBufferPlayback(pSourceVoice);

	} while (isPlaying);

	// 再生フラグを無効化して終了処理へ
	isPlaying = false;

	// 残りのバッファが終わるのを待ってから完全停止
	WaitForBufferPlayback(pSourceVoice);

	// リソース解放
	pSourceVoice->Stop();
	pSourceVoice->DestroyVoice();
	pSourceVoice = nullptr;
	mp3dec_ex_close(&mp3);
}


/// -------------------------------------------------------------
///	再生中の音声に対して「ピッチ（音の高さ）」と「音量（ボリューム）」を動的に変更する処理 
/// -------------------------------------------------------------
void Mp3Loader::UpdatePitchAndVolume(IXAudio2SourceVoice* voice, float volume, float pitch, float& previousPitch, float& previousVolume)
{
	if (voice && pitch != previousPitch)
	{
		voice->SetFrequencyRatio(pitch); // ピッチ（再生速度）を設定
		previousPitch = pitch;           // 現在値を記録
	}

	if (voice && volume != previousVolume)
	{
		voice->SetVolume(volume);        // 音量を設定
		previousVolume = volume;         // 現在値を記録
	}
}


/// -------------------------------------------------------------
///				　	　	　バッファを送信
/// -------------------------------------------------------------
void Mp3Loader::SubmitAudioBuffer(IXAudio2SourceVoice* voice, const void* buffer, size_t size)
{
	if (!voice || size == 0 || !buffer)
	{
		OutputDebugStringA("Invalid buffer or voice.\n");
		return;
	}

	XAUDIO2_BUFFER xBuffer = {};
	xBuffer.AudioBytes = static_cast<UINT32>(size);
	xBuffer.pAudioData = reinterpret_cast<const BYTE*>(buffer);
	xBuffer.Flags = XAUDIO2_END_OF_STREAM; // 正常な終了を示す

	HRESULT result = voice->SubmitSourceBuffer(&xBuffer);
	if (FAILED(result))
	{
		OutputDebugStringA("Failed to submit audio buffer.\n");
	}
}


/// -------------------------------------------------------------
///				　	　	バッファの再生を待機
/// -------------------------------------------------------------
void Mp3Loader::WaitForBufferPlayback(IXAudio2SourceVoice* voice)
{
	if (!voice)
	{
		OutputDebugStringA("Voice is null during WaitForBufferPlayback.\n");
		return;
	}

	XAUDIO2_VOICE_STATE state{};
	do {
		voice->GetState(&state);
		Sleep(10); // CPU負荷軽減
	} while (state.BuffersQueued > 0 && isPlaying);
}
