#pragma once
#include <BaseScene.h>

#include "GamePlayFlow.h"
#include "GamePlayStageContext.h"
#include "GamePlayWorld.h"
#include "GamePlayIntroDirector.h"
#include "GamePlayDebugTools.h"
#include "FadeManager.h"
#include "ApplicationLayer/DebugTools/FrustumCulling/FrustumCullingDebugController.h"

#include <memory>

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class Input; }

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// ゲームプレイシーン
/// 
/// 役割:
/// - ゲームプレイ全体の司令塔
/// - 各サブシステムの初期化 / 更新 / 描画順制御
/// - フローごとの分岐管理
/// 
/// なるべく「中身を全部持つ」のではなく、
/// 「各クラスを呼び分ける」ことに集中させる
/// -------------------------------------------------------------
class GamePlayScene : public BaseScene
{
public: /// ---------- BaseScene override ---------- ///

	// 初期化
	void Initialize() override;

	// 更新
	void Update() override;

	// 3D描画
	void Draw3DObjects() override;

	// シャドウ描画
	void DrawShadowObjects() override;

	// 2D描画
	void Draw2DSprites() override;

	// 終了処理
	void Finalize() override;

	// ImGui描画
	void DrawImGui() override;

	// 段階ロード
	void StartLoad() override;

	void UpdateLoad() override;

	// 段階アンロード
	void StartUnload() override;
	
	void UpdateUnload() override;

	bool IsReadyToStartUncover() const override;

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

	// リトライ遷移制御
	bool isRetryTransitionActive_ = false; // リトライ演出中か
	bool isRetryRestartDone_ = false;      // フェードアウト後の再初期化を実行済みか

	bool gameOverOpened_ = false;

	int loadStep_ = 0; // ロード段階
	bool isLoadReady_ = false; // ロード完了フラグ

	int unloadStep_ = 0;
	bool isUnloadReady_ = false;
};