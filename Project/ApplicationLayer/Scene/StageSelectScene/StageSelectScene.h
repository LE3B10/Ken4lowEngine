#pragma once
#include "BaseScene.h"
#include <Sprite.h>

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <limits>

#include "IStageSelector.h"        // StageInfo / SelectorContext / IStageSelector
#include "GridStageSelector.h"     // まずは Grid を使う
#include "IStageSelectSceneState.h" // ステート基底クラス

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class DirectXCommon; }
namespace Ken4lowEngine { class Input; }


/// -------------------------------------------------------------
///				　	ステージセレクトシーン
/// -------------------------------------------------------------
class StageSelectScene : public BaseScene
{
public: /// ---------- 型定義 ---------- ///

	// シーンの状態を管理する列挙型
	enum class State
	{
		Selecting,  // ステージセレクト中
		Loading,    // ローディング中
	};

	// 次に遷移するシーン
	enum class NextScene
	{
		None,	  // なし
		Title,	  // タイトルへ
		GamePlay, // ゲームプレイへ
	};

public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~StageSelectScene() = default;

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update() override;

	// 3Dオブジェクトの描画
	void Draw3DObjects() override;

	// シャドウマップ描画処理
	void DrawShadowObjects() override;

	// 2Dオブジェクトの描画
	void Draw2DSprites() override;

	// 終了処理
	void Finalize() override;

	// ImGui描画処理
	void DrawImGui() override;

private: /// ---------- メンバ関数 ---------- ///

	// ステージ情報初期化
	void InitializeStages();

	// セレクター初期化
	void InitializeSelectors();

	// 背景初期化
	void InitializeBackground();

public: /// ---------- 状態管理 ---------- ///

	// ステート差し替え
	void ChangeState(std::unique_ptr<IStageSelectSceneState> newState);

	// 現在の enum 状態
	State GetState() const { return state_; }
	void SetState(State s) { state_ = s; }

	// ステート用アクセサ（ステートクラスから必要な情報だけ触れるようにする）
	K4E::DirectXCommon* GetDxCommon() const { return dxCommon_; }
	K4E::Input* GetInput() const { return input_; }

	std::vector<StageInfo>& GetStages() { return stages_; }
	const std::vector<StageInfo>& GetStages() const { return stages_; }

	IStageSelector* GetActiveSelector() const { return activeSelector_; }

	K4E::Sprite* GetBgSprite() const { return bg_.get(); }
	K4E::Vector4& GetBgNow() { return bgNow_; }
	K4E::Vector4& GetBgTarget() { return bgTarget_; }

	// ペンディングアンロックインデックス
	int& GetPendingUnlockIndex() { return pendingUnlockIndex_; }

	// 次に遷移するシーンの設定
	void SetNextScene(NextScene n) { nextScene_ = n; }
	NextScene GetNextScene() const { return nextScene_; }

	SelectorContext& GetSelectorContext() { return context_; }

public: /// ---------- シーン遷移ヘルパー ---------- ///

	// シーン遷移のヘルパー
	void BackToTitle();     // ← TitleScene へ戻る

	void GoToGamePlay();    // ← GamePlayScene へ進む

private: /// ---------- メンバ変数 ---------- ///

	// 状態管理
	State state_ = State::Selecting; // とりあえず「セレクト中」から始める
	std::unique_ptr<IStageSelectSceneState> currentState_; // 現在のステート

	// 次に遷移するシーン
	NextScene nextScene_ = NextScene::None;

	// 依存注入
	K4E::DirectXCommon* dxCommon_ = nullptr;
	K4E::Input* input_ = nullptr;

	// データ
	std::vector<StageInfo> stages_;

	// セレクタ
	SelectorContext context_{};
	std::unique_ptr<IStageSelector> gridSelector_ = nullptr;
	IStageSelector* activeSelector_ = nullptr; // 生ポインタでアクセス

	// 背景色
	std::unique_ptr<K4E::Sprite> bg_;
	K4E::Vector4 bgNow_ = { 0.18f, 0.49f, 0.20f, 1.0f }; // 現在の色
	K4E::Vector4 bgTarget_ = bgNow_; // 目標の色

	int pendingUnlockIndex_ = -1;
};

