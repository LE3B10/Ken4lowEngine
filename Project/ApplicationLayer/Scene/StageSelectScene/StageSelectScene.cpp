#define NOMINMAX
#include "StageSelectScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include "SceneManager.h"
#include "StageRepository.h"
#include "LinearInterpolation.h"
#include <SpriteManager.h>
#include <StageSelectSelectingState.h>
#include "StageSelectFadeInState.h"
#include "StageSelectFadeOutState.h"
#include "StageSelectLoadState.h"

#include <algorithm>

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void StageSelectScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	// ステージ情報初期化
	InitializeStages();

	// セレクタ初期化
	InitializeSelectors();

	// 背景初期化
	InitializeBackground();

	// フェードオーバーレイ初期化
	InitializeFadeOverlay();

	// --- 最初のステートをセット（Loading） ---
	state_ = State::Loading;
	ChangeState(std::make_unique<StageSelectLoadState>());
}

/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void StageSelectScene::Update()
{
	float deltaTime = dxCommon_->GetFPSCounter().GetDeltaTime();

	// ステート更新
	if (currentState_) currentState_->Update(this, deltaTime);

}

/// -------------------------------------------------------------
///				　		3Dオブジェクト描画処理
/// -------------------------------------------------------------
void StageSelectScene::Draw3DObjects()
{

}

/// -------------------------------------------------------------
///				　		2Dオブジェクト描画処理
/// -------------------------------------------------------------
void StageSelectScene::Draw2DSprites()
{
	// 背景描画設定（後面）
	SpriteManager::GetInstance()->SetRenderSetting_Background();

	// 背景描画
	if (bg_) bg_->Draw();

	// 背景描画設定（UI）
	SpriteManager::GetInstance()->SetRenderSetting_UI();

	// アクティブセレクタの2D描画
	if (activeSelector_) activeSelector_->Draw2DSprites();

	// 最前面にフェードオーバーレイ
	if (fadeSprite_ && fadeAlpha_ > 0.0f)
	{
		fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, fadeAlpha_ });
		fadeSprite_->Update();
		fadeSprite_->Draw();
	}
}

/// -------------------------------------------------------------
///				　			　終了処理
/// -------------------------------------------------------------
void StageSelectScene::Finalize()
{
	if (activeSelector_) activeSelector_->OnExit();
	activeSelector_ = nullptr;
	gridSelector_ = nullptr;
}

/// -------------------------------------------------------------
///				　			ImGui描画処理
/// -------------------------------------------------------------
void StageSelectScene::DrawImGui()
{

}

/// -------------------------------------------------------------
///				　		　ステージ情報初期化
/// -------------------------------------------------------------
void StageSelectScene::InitializeStages()
{
	// ステージ配列の生成
	stages_.clear();
	stages_.push_back({ 0u, "始まりの森",	  "white.png", false, 0u, { 0.18f, 0.49f, 0.20f, 1.0f } });
	stages_.push_back({ 1u, "廃鉱山",		  "white.png", true,  0u, { 0.43f, 0.30f, 0.25f, 1.0f } });
	stages_.push_back({ 2u, "工業地帯",		  "white.png", true,  0u, { 0.96f, 0.49f, 0.00f, 1.0f } });
	stages_.push_back({ 3u, "朽ちた果てた街", "white.png", true,  0u, { 0.37f, 0.35f, 0.49f, 1.0f } });
	stages_.push_back({ 4u, "港湾ターミナル", "white.png", true,  0u, { 0.08f, 0.40f, 0.75f, 1.0f } });
}

/// -------------------------------------------------------------
///				　		　セレクタ初期化
/// -------------------------------------------------------------
void StageSelectScene::InitializeSelectors()
{
	float screenWidth = static_cast<float>(dxCommon_->GetSwapChainDesc().Width);
	float screenHeight = static_cast<float>(dxCommon_->GetSwapChainDesc().Height);

	// セレクタの初期化
	context_.screenWidth = screenWidth;
	context_.screenHeight = screenHeight;
	context_.input = input_;
	context_.stages = &stages_;

	// コールバック設定

	// 戻る要求
	context_.onRequestBack = [this]() {
		// 次のシーンは Title
		SetNextScene(NextScene::Title);

		if (state_ == State::Selecting)
		{
			// フェードアウトへ
			ChangeState(std::make_unique<StageSelectFadeOutState>());
		}
		else
		{
			// 万が一 Selecting 以外で呼ばれたら即戻る
			BackToTitle();
		}
		};

	context_.onRequestMap = [this](uint32_t stageIndex) {
		// ステージ一覧と開始フォーカスをリポジトリへ
		StageRepository::GetInstance().SetStages(stages_);
		StageRepository::GetInstance().SetStartIndex((int)stageIndex); // 選択IDの保持

		// 次のシーンは GamePlay
		SetNextScene(NextScene::GamePlay);

		// ロードステートへ移行
		if (state_ == State::Selecting)
		{
			ChangeState(std::make_unique<StageSelectFadeOutState>());
		}
		};
}

/// -------------------------------------------------------------
///				　		　背景初期化
/// -------------------------------------------------------------
void StageSelectScene::InitializeBackground()
{
	// 保存済みステージ情報を反映
	auto& repo = StageRepository::GetInstance();
	const auto& saved = repo.GetStages();
	if (!saved.empty() && saved.size() == stages_.size()) {
		stages_ = saved;
	}

	// --- ここで StartIndex を読む ---
	int startIndex = 0;
	if (auto idxOpt = repo.GetStartIndex())
	{
		// クランプして代入
		startIndex = std::clamp(*idxOpt, 0, (int)stages_.size() - 1);
	}

	// 背景スプライト（全画面）
	bg_ = std::make_unique<Sprite>();
	bg_->Initialize("white.png");
	bg_->SetPosition({});
	bg_->SetSize({ context_.screenWidth, context_.screenHeight });
	bg_->SetColor(bgNow_);
	bg_->Update();

	// Gridセレクタを生成してアクティブ化
	gridSelector_ = std::make_unique<GridStageSelector>();
	gridSelector_->Initialize(context_);

	// 中央カードが変わったら背景ターゲット色を更新
	static_cast<GridStageSelector*>(gridSelector_.get())->SetOnCenterChanged([this](uint32_t centerIndex) {
		if (centerIndex < stages_.size()) { bgTarget_ = stages_.at(centerIndex).color; }
		});

	activeSelector_ = gridSelector_.get();

	if (startIndex >= 0 && startIndex < (int)stages_.size()) {
		activeSelector_->FocusToIndex(startIndex, false);

		if (stages_[startIndex].justUnlocked) {
			pendingUnlockIndex_ = startIndex; // ← フェード完了後に再生する
		}

		bgNow_ = bgTarget_ = stages_[startIndex].color;
		if (bg_) {
			bg_->SetColor(bgNow_);
			bg_->Update();
		}
	}

	// アクティブセレクタに通知
	activeSelector_->OnEnter();
}

/// -------------------------------------------------------------
///				　		　フェード用初期化
/// -------------------------------------------------------------
void StageSelectScene::InitializeFadeOverlay()
{
	// フェード用スプライト（画面全体を覆う黒）
	fadeSprite_ = std::make_unique<Sprite>();
	fadeSprite_->Initialize("white.png");
	fadeSprite_->SetSize({ context_.screenWidth, context_.screenHeight });
	fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	fadeSprite_->Update();
	fadeAlpha_ = 1.0f; // 最初は真っ黒からフェードイン
}

/// -------------------------------------------------------------
///				　		　ステート差し替え
/// -------------------------------------------------------------
void StageSelectScene::ChangeState(std::unique_ptr<IStageSelectSceneState> newState)
{
	// いまのステートから抜ける
	if (currentState_)
	{
		currentState_->Exit(this);
	}

	// ステート差し替え
	currentState_ = std::move(newState);

	// 新しいステートに入る
	if (currentState_)
	{
		currentState_->Enter(this);
	}
}

/// -------------------------------------------------------------
///				　		　シーン遷移ヘルパー
/// -------------------------------------------------------------
void StageSelectScene::BackToTitle()
{
	if (sceneManager_)
	{
		// タイトルシーンへ戻る
		sceneManager_->ChangeScene("TitleScene");
	}
}

/// -------------------------------------------------------------
///				　		　シーン遷移ヘルパー
/// -------------------------------------------------------------
void StageSelectScene::GoToGamePlay()
{
	if (sceneManager_)
	{
		// ゲームプレイシーンへ進む
		sceneManager_->ChangeScene("GamePlayScene");
	}
}
