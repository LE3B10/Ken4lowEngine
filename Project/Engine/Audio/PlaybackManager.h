#pragma once
#include <atomic>


/// ---------- 再生状態を表す列挙型 ---------- ///
enum class PlaybackState
{
	Stopped, // 音声を止める
	Playing, // 再生
	Paused	 // 一時停止
};


/// -------------------------------------------------------------
///				　	　再生状態を管理するクラス
/// -------------------------------------------------------------
class PlaybackManager
{
public: /// ---------- メンバ関数 ---------- ///

	// 再生状態を取得
	PlaybackState GetPlaybackState() const {return playbackState;}
	
	// 現在の再生状態を設定
	void SetPlaybackState(PlaybackState state) { playbackState = state; }

	// 再生
	bool IsPlaying() const { return playbackState == PlaybackState::Playing; }
	
	// 一時停止
	bool IsPaused() const { return playbackState == PlaybackState::Paused; }

private: /// ---------- メンバ変数 ---------- ///

	// 再生状態
	std::atomic<PlaybackState> playbackState = PlaybackState::Stopped;

};