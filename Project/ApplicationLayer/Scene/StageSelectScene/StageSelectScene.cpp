#define NOMINMAX
#include "StageSelectScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include "SceneManager.h"
#include "StageRepository.h"
#include "LinearInterpolation.h"
#include <SpriteManager.h>
#include <StageSelectSelectingState.h>
#include "StageSelectLoadState.h"

#include <algorithm>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void StageSelectScene::Initialize()
{
	dxCommon_ = K4E::DirectXCommon::GetInstance();
	input_ = K4E::Input::GetInstance();

	// ステージ情報初期化
	InitializeStages();

	// セレクタ初期化
	InitializeSelectors();

	// 背景初期化
	InitializeBackground();

	// --- 最初のステートをセット（Loading） ---
	state_ = State::Loading;
	ChangeState(std::make_unique<StageSelectSelectingState>());
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

void StageSelectScene::DrawShadowObjects()
{
}

/// -------------------------------------------------------------
///				　		2Dオブジェクト描画処理
/// -------------------------------------------------------------
void StageSelectScene::Draw2DSprites()
{
	// 背景描画設定（後面）
	K4E::SpriteManager::GetInstance()->SetRenderSetting_Background();

	// 背景描画
	if (bg_) bg_->Draw();

	// 背景描画設定（UI）
	K4E::SpriteManager::GetInstance()->SetRenderSetting_UI();

	// アクティブセレクタの2D描画
	if (activeSelector_) activeSelector_->Draw2DSprites();
}

/// -------------------------------------------------------------
///				　			　終了処理
/// -------------------------------------------------------------
void StageSelectScene::Finalize()
{
	// 1) ステートを確実に抜ける（ステート側で何か掴んでる可能性がある）
	if (currentState_) {
		currentState_->Exit(this);
	}
	currentState_.reset();

	// 2) セレクタを確実に抜ける
	// activeSelector_ は gridSelector_.get() の生ポインタなので、先に OnExit してから所有側を破棄
	if (activeSelector_) {
		activeSelector_->OnExit();
	}
	activeSelector_ = nullptr;
	gridSelector_.reset();

	// 3) スプライト解放
	bg_.reset();

	// 4) データや参照を整理（任意だけど安全）
	stages_.clear();
	pendingUnlockIndex_ = -1;
	nextScene_ = NextScene::None;
	state_ = State::Selecting;

	// 依存注入ポインタは最後に切る
	input_ = nullptr;
	dxCommon_ = nullptr;
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

	context_.screenWidth = screenWidth;
	context_.screenHeight = screenHeight;
	context_.input = input_;
	context_.stages = &stages_;

	// 戻る要求（即タイトルへ）
	context_.onRequestBack = [this]() {

		// 多重遷移防止
		if (nextScene_ != NextScene::None) return;

		SetNextScene(NextScene::Title);
		BackToTitle();                 // ★即 ChangeScene（SceneManager側フェードに任せる）
		};

	// ステージ決定（即ゲームへ）
	context_.onRequestMap = [this](uint32_t stageIndex) {

		// 多重遷移防止
		if (nextScene_ != NextScene::None) return;

		StageRepository::GetInstance().SetStages(stages_);
		StageRepository::GetInstance().SetStartIndex((int)stageIndex);

		SetNextScene(NextScene::GamePlay);
		GoToGamePlay();                // ★即 ChangeScene（SceneManager側フェードに任せる）
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
	bg_ = std::make_unique<K4E::Sprite>();
	bg_->Initialize("white.png");
	bg_->SetPosition({});

	context_.screenWidth = static_cast<float>(dxCommon_->GetClientWidth());
	context_.screenHeight = static_cast<float>(dxCommon_->GetClientHeight());

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
