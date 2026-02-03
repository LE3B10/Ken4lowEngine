#pragma once
#include "AudioStructs.h"

#include <atomic> // 再生フラグ用
#include <fstream>
#include <future> // std::futureを使用するためのヘッダファイル
#include <mutex>
#include <wrl.h>

namespace Ken4lowEngine
{

// 省略
using namespace Microsoft::WRL;


/// -------------------------------------------------------------
///				　	　.wavを読み込むクラス
/// -------------------------------------------------------------
class WavLoader
{
private: /// ---------- メンバ関数 ---------- ///

	/// ---------- 再生状態を表す列挙型 ---------- ///
	enum class PlaybackState
	{
		Stopped, // 音声を止める
		Playing, // 再生
		Paused	 // 一時停止
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// デストラクタ。<br/>
	/// BGM の再生を停止し、SourceVoice / MasteringVoice / XAudio2 エンジンを解放します。
	/// </summary>
	~WavLoader();

	/// <summary>
	/// WAV ファイルを非同期でストリーミング再生します。<br/>
	/// 内部では std::async(std::launch::async, ...) で別スレッドを起動し、<br/>
	/// StreamAudio() をループ再生も含めて実行します。<br/>
	/// Resources/Sounds/ 以下にある .wav を想定しています。
	/// </summary>
	/// <param name="fileName">再生する WAV ファイル名（例： "bgm.wav"）。パスの一部は内部で補完されます。</param>
	/// <param name="volume">初期音量（0.0f～1.0f を想定）。</param>
	/// <param name="pitch">
	/// 再生ピッチ（周波数比）。<br/>
	/// 1.0f で等倍、2.0f で 1 オクターブ上、0.5f で 1 オクターブ下など。
	/// </param>
	/// <param name="Loop">true の場合、ファイル末尾まで再生したら先頭に戻ってループ再生します。</param>
	void StreamAudioAsync(const std::string& fileName, float volume = 1.0f, float pitch = 1.0f, bool Loop = false);

	/// <summary>
	/// BGM の再生を停止します。<br/>
	/// ・再生フラグ isPlaying を false にしてループを終了させ、<br/>
	/// ・バックグラウンドスレッド（bgmFuture）が生きていれば join（get）し、<br/>
	/// ・SourceVoice を停止／破棄します。<br/>
	/// Initialize() で確保した XAudio2 エンジン本体の解放はデストラクタ側で行います。
	/// </summary>
	void StopBGM();

	/// <summary>
	/// BGM の再生を一時停止します。<br/>
	/// 再生中(PlaybackState::Playing)かつ SourceVoice が存在する場合に、<br/>
	/// IXAudio2SourceVoice::Stop() を呼び出し、状態を Paused に変更します。
	/// </summary>
	void PauseBGM();

	/// <summary>
	/// 一時停止中の BGM 再生を再開します。<br/>
	/// 再生状態が Paused のときに IXAudio2SourceVoice::Start() を呼び、<br/>
	/// 状態を Playing に戻します。
	/// </summary>
	void ResumeBGM();

public: /// ---------- セッター ---------- ///

	/// <summary>
	/// 再生中ストリームの音量を変更します。<br/>
	/// 値は atomic に保持され、次のバッファ処理タイミングで反映されます。
	/// </summary>
	/// <param name="volume">設定する音量（0.0f～1.0f を想定）。</param>
	void SetStreamVolume(float volume) { currentVolume = volume; }

	/// <summary>
	/// 再生中ストリームのピッチ(周波数比)を変更します。<br/>
	/// 値は atomic に保持され、次のバッファ処理タイミングで反映されます。
	/// </summary>
	/// <param name="pitch">設定する周波数比（1.0f で等倍）。</param>
	void SetStreamPitch(float pitch) { frequencyRatio = pitch; }

	/// <summary>
	/// ループ再生フラグを設定します。<br/>
	/// true にするとファイル末尾到達時に先頭に戻り、連続再生します。
	/// </summary>
	/// <param name="loop">ループさせる場合は true。</param>
	void SetLoopPlayback(bool loop) { loopPlayback = loop; }

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// 現在の再生状態を取得します。<br/>
	/// Stopped / Playing / Paused のいずれかが返ります。
	/// </summary>
	PlaybackState GetPlaybackState() const { return playbackState; }

private: /// ---------- 内部処理用メンバ関数 ---------- ///

	/// <summary>
	/// XAudio2 エンジンとマスターボイス、SourceVoice の初期化を行います。<br/>
	/// ・XAudio2Create で IXAudio2 を生成<br/>
	/// ・CreateMasteringVoice でマスターボイスを生成<br/>
	/// ・指定ファイルを開き、RIFF / fmt チャンクを読み込んでフォーマット情報を取得<br/>
	/// ・CreateSourceVoice で SourceVoice を生成<br/>
	/// といった流れで、再生に必要な準備を行います。<br/>
	/// エラー時には WavLoaderException をスローします。
	/// </summary>
	/// <param name="fileName">初期化対象の WAV ファイルパス。</param>
	void Initialize(const std::string& fileName);

	/// <summary>
	/// ストリーミング再生のメイン処理を行う関数です。<br/>
	/// StreamAudioAsync から別スレッドで呼び出されます。<br/>
	/// 1. Resources/Sounds/ 以下のファイルパスを組み立てる<br/>
	/// 2. Initialize() を呼び出して XAudio2 / Voice を準備<br/>
	/// 3. 一定サイズのバッファ(例: 1MB)を確保<br/>
	/// 4. ループ内でファイルから読み込み → SourceVoice に Submit → 再生完了を Wait<br/>
	/// 5. ループ時に音量・ピッチの変更をチェックして反映<br/>
	/// 6. isPlaying / loopPlayback フラグに応じてループ継続／終了を判断<br/>
	/// といった処理を行います。終了時には SourceVoice を停止・破棄します。
	/// </summary>
	void StreamAudio(const std::string& fileName, float volume, float pitch, bool Loop);

	/// <summary>
	/// ピッチと音量を前回値と比較し、変化がある場合のみ XAudio2 に設定します。<br/>
	/// ・SetFrequencyRatio でピッチ変更<br/>
	/// ・SetVolume で音量変更<br/>
	/// を行い、previousPitch / previousVolume を更新します。
	/// </summary>
	void UpdatePitchAndVolume(IXAudio2SourceVoice* voice, float volume, float pitch, float& previousPitch, float& previousVolume);

	/// <summary>
	/// 読み込んだ音声バッファを SourceVoice に送信します。<br/>
	/// XAUDIO2_BUFFER を構築し、IXAudio2SourceVoice::SubmitSourceBuffer を呼び出します。<br/>
	/// 失敗した場合は WavLoaderException をスローします。
	/// </summary>
	void SubmitAudioBuffer(IXAudio2SourceVoice* voice, const char* buffer, size_t size);

	/// <summary>
	/// 再生中のバッファキューが空になるまで待機します。<br/>
	/// GetState() で BuffersQueued をポーリングし、<br/>
	/// 0 になるか isPlaying フラグが false になるまで Sleep(10) で待ちます。
	/// </summary>
	void WaitForBufferPlayback(IXAudio2SourceVoice* voice);

	/// <summary>
	/// RIFF ヘッダ(RiffHeader)を読み込み、"RIFF" / "WAVE" かどうかを判定します。
	/// </summary>
	bool ReadRiffHeader(std::ifstream& file, RiffHeader& riff);

	/// <summary>
	/// "fmt " チャンクを読み込み、フォーマット情報(FormatChunk)を取得します。
	/// </summary>
	bool ReadFormatChunk(std::ifstream& file, FormatChunk& format);

	/// <summary>
	/// WAV ファイル内から "data" チャンクを探し、そのヘッダ情報を返します。<br/>
	/// 見つかった場合は true、見つからなければ false を返します。
	/// </summary>
	bool FindDataChunk(std::ifstream& file, ChunkHeader& data);

private: /// ---------- メンバ変数 ---------- ///

	// XAudio2 の SourceVoice（音声再生用）
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	std::mutex sourceVoiceMutex;

	// XAudio2 エンジン本体とマスターボイス
	ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice = nullptr;

	std::future<void> bgmFuture; // BGM再生スレッド

	std::atomic<PlaybackState> playbackState = PlaybackState::Stopped; // 再生状態
	std::atomic<bool> isPaused = false;		  // 一時停止
	std::atomic<bool> isPlaying = false;	  // 再生フラグ
	std::atomic<bool> loopPlayback = false;	  // ループ再生フラグ

	std::atomic<float> currentVolume = 1.0f;  // デフォルト音量
	std::atomic<float> frequencyRatio = 1.0f; // デフォルトは通常再生
};


} // namespace Ken4lowEngine
