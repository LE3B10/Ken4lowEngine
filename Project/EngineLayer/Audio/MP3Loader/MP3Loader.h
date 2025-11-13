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

	/// <summary>
	/// デストラクタ。<br/>
	/// BGM の再生を停止し、SourceVoice / MasteringVoice / XAudio2 インスタンスを解放します。
	/// </summary>
	~Mp3Loader();

	/// <summary>
	/// BGM 用の MP3 を非同期ストリーミング再生します。<br/>
	/// 内部では std::async(std::launch::async, ...) で別スレッドを起動し、<br/>
	/// StreamAudio() をループ再生も含めて実行します。<br/>
	/// すでに再生中の BGM があれば StopBGM() で停止してから新しく再生を開始します。
	/// </summary>
	/// <param name="fileName">再生する MP3 ファイル名（例: "bgm.mp3"）。パスの一部は内部で補完されます。</param>
	/// <param name="volume">初期音量（0.0f ～ 1.0f を想定）。</param>
	/// <param name="pitch">
	/// 再生ピッチ（周波数比）。<br/>
	/// 1.0f で等倍、2.0f で 1 オクターブ上、0.5f で 1 オクターブ下など。
	/// </param>
	/// <param name="Loop">true の場合、ファイル末尾まで再生したら先頭に戻ってループ再生します。</param>
	void StreamAudioAsync(const std::string& fileName, float volume = 1.0f, float pitch = 1.0f, bool Loop = false);

	/// <summary>
	/// サウンドエフェクト(SE) 用の MP3 を非同期でワンショット再生します。<br/>
	/// ・内部で一時的な Mp3Loader インスタンスを new<br/>
	/// ・Initialize() で XAudio2 を初期化<br/>
	/// ・ファイル全体をデコードして単発再生<br/>
	/// ・再生終了を待ってからボイス・デコーダ・インスタンスを解放<br/>
	/// という流れを新規スレッドで行います。<br/>
	/// BGM 用のストリーミングとは独立して動作します。
	/// </summary>
	/// <param name="fileName">再生する SE の MP3 ファイル名。</param>
	/// <param name="volume">SE の音量。</param>
	/// <param name="pitch">SE のピッチ（周波数比）。</param>
	void PlaySEAsync(const std::string& fileName, float volume, float pitch);

	/// <summary>
	/// BGM の再生を停止します。<br/>
	/// ・isPlaying フラグを false にしてストリーミングループを終了<br/>
	/// ・bgmFuture が有効なら get() してバックグラウンドスレッドの完了を待機<br/>
	/// ・ SourceVoice を Stop / DestroyVoice して解放<br/>
	/// といった後始末を行います。
	/// </summary>
	void StopBGM();

	/// <summary>
	/// BGM の再生を一時停止します。<br/>
	/// 再生中(PlaybackState::Playing) かつ SourceVoice が有効なときに、<br/>
	/// IXAudio2SourceVoice::Stop() を呼び出し、状態を Paused / isPaused = true に変更します。
	/// </summary>
	void PauseBGM();

	/// <summary>
	/// 一時停止中の BGM を再開します。<br/>
	/// 再生状態が Paused のときに IXAudio2SourceVoice::Start() を呼び、<br/>
	/// 状態を Playing / isPaused = false に戻します。
	/// </summary>
	void ResumeBGM();

public: /// ---------- セッター ---------- ///

	/// <summary>
	/// ストリーミング中 BGM の音量を設定します。<br/>
	/// 値は atomic 変数に保存され、StreamAudio のループ内で変化があれば XAudio2 に反映されます。
	/// </summary>
	/// <param name="volume">設定する音量（0.0f ～ 1.0f を想定）。</param>
	void SetStreamVolume(float volume) { currentVolume = volume; }

	/// <summary>
	/// ストリーミング中 BGM のピッチ(周波数比)を設定します。<br/>
	/// 値は atomic 変数に保存され、StreamAudio のループ内で変化があれば SourceVoice に反映されます。
	/// </summary>
	/// <param name="pitch">設定する周波数比（1.0f で等倍）。</param>
	void SetStreamPitch(float pitch) { frequencyRatio = pitch; }

	/// <summary>
	/// BGM のループ再生フラグを設定します。<br/>
	/// true の場合、ファイル末尾に到達しても mp3dec_ex_seek で先頭に戻り再生を続行します。
	/// </summary>
	/// <param name="loop">ループ再生させる場合は true。</param>
	void SetLoopPlayback(bool loop) { loopPlayback = loop; }

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// 現在の再生状態を取得します。<br/>
	/// Stopped / Playing / Paused のいずれかを返します。
	/// </summary>
	PlaybackState GetPlaybackState() const { return playbackState; }

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// XAudio2 の初期化処理を行います。<br/>
	/// ・XAudio2Create で IXAudio2 インスタンスを生成<br/>
	/// ・CreateMasteringVoice でマスターボイスを作成<br/>
	/// に失敗した場合は std::runtime_error をスローします。
	/// </summary>
	void Initialize();

	/// <summary>
	/// BGM 用のストリーミング再生のメイン処理。<br/>
	/// StreamAudioAsync から別スレッドで呼び出されます。<br/>
	/// 1. "Resources/Sounds/" + fileName でパスを構築<br/>
	/// 2. Initialize() で XAudio2 エンジンとマスターボイスを準備<br/>
	/// 3. minimp3 (mp3dec_ex) で MP3 を開き、フォーマット情報から WAVEFORMATEX を構築<br/>
	/// 4. CreateSourceVoice → Start で SourceVoice を起動<br/>
	/// 5. 約 1MB のバッファ単位で MP3 → PCM にデコードして送信(SubmitAudioBuffer)<br/>
	/// 6. 各チャンクごとに UpdatePitchAndVolume でピッチ／音量変更を反映<br/>
	/// 7. ループフラグと isPlaying を見ながら再生継続／終了を判定<br/>
	/// 8. 終了処理として Voice 停止・破棄／デコーダクローズ<br/>
	/// を行います。
	/// </summary>
	/// <param name="fileName">再生する MP3 ファイル名。</param>
	/// <param name="volume">初期音量。</param>
	/// <param name="pitch">初期ピッチ。</param>
	/// <param name="Loop">true の場合はループ再生。</param>
	void StreamAudio(const std::string& fileName, float volume, float pitch, bool Loop);

	/// <summary>
	/// 再生中の音声に対して「ピッチ（音の高さ）」と「音量（ボリューム）」を動的に変更します。<br/>
	/// 直前の値(previousPitch / previousVolume) と比較し、変化がある場合のみ<br/>
	/// SetFrequencyRatio / SetVolume を呼び出して XAudio2 に反映します。
	/// </summary>
	/// <param name="voice">パラメータを変更する対象の SourceVoice。</param>
	/// <param name="volume">現在の目標音量。</param>
	/// <param name="pitch">現在の目標ピッチ。</param>
	/// <param name="previousPitch">前フレームのピッチ値（関数内で更新されます）。</param>
	/// <param name="previousVolume">前フレームの音量値（関数内で更新されます）。</param>
	void UpdatePitchAndVolume(IXAudio2SourceVoice* voice, float volume, float pitch, float& previousPitch, float& previousVolume);

	/// <summary>
	/// PCM バッファを SourceVoice に送信します。<br/>
	/// XAUDIO2_BUFFER を構築して SubmitSourceBuffer を呼び出します。<br/>
	/// 引数 size はバイト数を想定しています。
	/// </summary>
	/// <param name="voice">送信先の SourceVoice。</param>
	/// <param name="buffer">PCM データへのポインタ。</param>
	/// <param name="size">バッファのサイズ（バイト数）。</param>
	void SubmitAudioBuffer(IXAudio2SourceVoice* voice, const void* buffer, size_t size);

	/// <summary>
	/// 送信済みバッファの再生が完了するまで待機します。<br/>
	/// GetState() で BuffersQueued を監視し、0 になるか isPlaying が false になるまで<br/>
	/// Sleep(10) でポーリングします。
	/// </summary>
	/// <param name="voice">監視対象の SourceVoice。</param>
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
