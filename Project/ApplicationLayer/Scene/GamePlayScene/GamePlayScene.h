#pragma once
#include <BaseScene.h>

#include "GamePlayFlow.h"
#include "GamePlayStageContext.h"
#include "GamePlayWorld.h"
#include "GamePlayIntroDirector.h"
#include "GamePlayDebugTools.h"
#include "FadeManager.h"
#include "ApplicationLayer/DebugTools/FrustumCulling/FrustumCullingDebugController.h"
#include "PostEffect/PlayerHealthPostEffectController.h"

#include <memory>

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class Input; }

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// ゲームプレイシーン
/// 
/// 役割:
/// - ゲームプレイ全体の司令塔として、Flow / World / Intro / Debug / Fade を所有する。
/// - SceneManager から呼ばれるライフサイクルに合わせて、各サブシステムの初期化 / 更新 / 描画順を制御する。
/// - ポーズ、イントロ、リトライ、リザルトなど「Worldを進めるかどうか」の分岐を管理する。
/// 
/// 所有権:
/// - unique_ptrで保持しているゲームプレイ構成要素は、このシーンの開始から終了までが寿命。
/// - 再試行時はFadeManagerだけ残し、WorldやFlowは再生成してステージ状態を初期化する。
/// -------------------------------------------------------------
class GamePlayScene : public BaseScene
{
public: /// ---------- BaseScene override ---------- ///

	// エンジン入力やフェード管理を準備し、段階ロード前の空のゲームプレイ状態へ戻す。
	void Initialize() override;

	// フェード、イントロ、ポーズ、リザルト、通常World更新を現在フローに応じて1フレーム進める。
	void Update() override;

	// Editor中は敵/弾/Wave/Player操作を止め、描画確認に必要な軽い更新だけ行う。
	void UpdateEditor(float deltaTime) override;

	// ステージ、キャラクター、弾、アイテムなどWorld側の3D要素を描画する。
	void Draw3DObjects() override;

	// ステージとキャラクターのシャドウ描画を、イントロ表示状態に合わせて行う。
	void DrawShadowObjects() override;

	// HUD、ポーズ/リザルトUI、フェードを2D描画順に沿って描画する。
	void Draw2DSprites() override;

	// カーソル状態を復帰し、GamePlaySceneが所有するWorld/Flow/Debug/Fadeを解放する。
	void Finalize() override;

	// ImGui描画
	void DrawImGui() override;

	// World OutlinerへGamePlaySceneの主要オブジェクトを公開する。
	void CollectEditorObjects(std::vector<Ken4lowEngine::EditorObjectInfo>& outObjects) override;

	// FPS操作が必要なGamePlaySceneだけF8入力キャプチャを許可する。
	EditorInputPolicy GetEditorInputPolicy() const override;

	// SceneManagerのフェード遷移中に、GamePlay構成要素を複数フレームへ分けて生成する準備。
	void StartLoad() override;

	// Flow、StageContext、World、Debug、Introを順番に生成し、完了後にゲーム開始状態へ進める。
	void UpdateLoad() override;

	// SceneManagerの遷移中に、重い解放処理を複数フレームへ分ける準備。
	void StartUnload() override;
	
	// 入力復帰、Debug解放、World解放、Fade解放を順番に行い、解放完了フラグを立てる。
	void UpdateUnload() override;

	// ロード済みになり、画面覆いを外してもよいかをSceneManagerへ返す。
	bool IsReadyToStartUncover() const override;

	// アンロード済みになり、次シーンへ差し替えてもよいかをSceneManagerへ返す。
	bool IsReadyToSwapOut() const override;

private: /// ---------- 初期化 / 終了系 ---------- ///

	// エンジン依存システム取得やカーソル設定
	void InitializeSystems();

	// ゲームプレイ構成オブジェクト生成
	void InitializeGameplayObjects();

	// 新規ゲーム開始時のセットアップ
	void SetupNewGame(bool skipIntro = false);

	// カーソル状態を通常に戻す
	void RestoreCursorState();

	// HP連動ポストエフェクトを初期化して被弾通知を接続する
	void InitializeHealthPostEffectController();

	// 生成済みオブジェクトの破棄
	void ReleaseGameplayObjects();

private: /// ---------- 更新系 ---------- ///

	// デバッグ停止のトグル処理
	// true を返したらそのフレームの Update は打ち切る
	bool HandleDebugFreeze();

	// リトライフェード更新
	// true を返したらそのフレームの Update は打ち切る
	bool UpdateRetryTransition();

	// イントロ更新
	// true を返したらそのフレームの Update は打ち切る
	bool UpdateIntro(float deltaTime);

	// リザルト更新
	// true を返したらそのフレームの Update は打ち切る
	bool UpdateResult(float deltaTime);

	// ESC によるポーズ切り替え
	// true を返したらそのフレームの Update は打ち切る
	bool HandlePauseToggle();

	// ポーズ中更新
	// true を返したらそのフレームの Update は打ち切る
	bool UpdatePause(float deltaTime);

	// デバッグカメラなどの通常時デバッグ更新
	void UpdateDebug();

	// 通常のワールド更新
	void UpdateWorld(float deltaTime);

	// クリア / ゲームオーバー判定
	void CheckGameEnd();

private: /// ---------- 補助系 ---------- ///

	// イントロ中はキャラクター表示を抑制したいか
	bool ShouldHideCharactersDuringIntro() const;
	// ボス登場演出中は通常HUDを表示しない
	bool ShouldHideGameplayUI() const;

	// リトライ要求開始
	void RequestRetryWithFade();

	// リスタート
	void RestartGame(bool skipIntro = true);

	// フェード開始 / 完了判定
	void StartRetryFadeOut();
	bool IsRetryFadeOutFinished() const;
	void StartRetryFadeIn();
	bool IsRetryFadeInFinished() const;

private: /// ---------- メンバ変数 ---------- ///

	// エンジン側シングルトン参照
	K4E::Input* input_ = nullptr;

	// ゲームプレイ構成要素
	std::unique_ptr<GamePlayFlow> flow_;
	std::unique_ptr<GamePlayStageContext> stageContext_;
	std::unique_ptr<GamePlayWorld> world_;
	std::unique_ptr<GamePlayIntroDirector> introDirector_;
	std::unique_ptr<GamePlayDebugTools> debugTools_;
	std::unique_ptr<FrustumCullingDebugController> frustumCullingDebug_;
	std::unique_ptr<FadeManager> fadeManager_;
	std::unique_ptr<PlayerHealthPostEffectController> hpPostEffectController_;

	// リトライ遷移制御
	bool isRetryTransitionActive_ = false; // リトライ演出中か
	bool isRetryRestartDone_ = false;      // フェードアウト後の再初期化を実行済みか

	bool gameOverOpened_ = false;

	int loadStep_ = 0; // ロード段階
	bool isLoadReady_ = false; // ロード完了フラグ

	int unloadStep_ = 0;
	bool isUnloadReady_ = false;
};
