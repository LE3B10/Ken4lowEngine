#define NOMINMAX
#include "StageSelectScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include "SceneManager.h"
#include "StageRepository.h"
#include "LinearInterpolation.h"

#include <algorithm>
#include <SpriteManager.h>

void StageSelectScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	// ステージ配列の生成
	stages_.clear();
	stages_.push_back({ 0u, "始まりの森",	  "white.png", false, 0u, { 0.18f, 0.49f, 0.20f, 1.0f } });
	stages_.push_back({ 1u, "廃鉱山",		  "white.png", true,  0u, { 0.43f, 0.30f, 0.25f, 1.0f } });
	stages_.push_back({ 2u, "工業地帯",		  "white.png", true,  0u, { 0.96f, 0.49f, 0.00f, 1.0f } });
	stages_.push_back({ 3u, "朽ちた果てた街", "white.png", true,  0u, { 0.37f, 0.35f, 0.49f, 1.0f } });
	stages_.push_back({ 4u, "港湾ターミナル", "white.png", true,  0u, { 0.08f, 0.40f, 0.75f, 1.0f } });

	// フェードコントローラーの初期化
	fadeController_ = std::make_unique<FadeController>();
	float screenWidth = static_cast<float>(dxCommon_->GetSwapChainDesc().Width);
	float screenHeight = static_cast<float>(dxCommon_->GetSwapChainDesc().Height);
	fadeController_->Initialize(screenWidth, screenHeight, "white.png");
	fadeController_->SetFadeMode(FadeController::FadeMode::Checkerboard);
	fadeController_->SetGrid(14, 8);
	fadeController_->SetCheckerDelay(0.012f);
	fadeController_->StartFadeIn(0.32f); // 暗転明け

	// セレクタの初期化
	context_.screenWidth = screenWidth;
	context_.screenHeight = screenHeight;
	context_.input = input_;
	context_.fadeController = fadeController_.get();
	context_.stages = &stages_;

	// コールバック設定

	// 戻る要求
	context_.onRequestBack = [this]() {
		fadeController_->SetOnComplete([this]() { if (sceneManager_) { sceneManager_->ChangeScene("TitleScene"); }});
		fadeController_->StartFadeOut(0.32f);
		};

	context_.onRequestMap = [this](uint32_t stageIndex) {
		// ステージ一覧と開始フォーカスをリポジトリへ
		StageRepository::GetInstance().SetStages(stages_);
		StageRepository::GetInstance().SetStartIndex((int)stageIndex); // 選択IDの保持

		// フェード後に GamePlayScene へ
		fadeController_->SetOnComplete([this] {	if (sceneManager_) sceneManager_->ChangeScene("GamePlayScene");	});
		fadeController_->StartFadeOut(0.32f);
		};

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
		if (centerIndex < stages_.size()) {
			bgTarget_ = stages_.at(centerIndex).color;
		}
		});

	activeSelector_ = gridSelector_.get();
	activeSelector_->OnEnter();
}

void StageSelectScene::Update()
{
	float deltaTime = dxCommon_->GetFPSCounter().GetDeltaTime();
	if (activeSelector_) activeSelector_->Update(deltaTime); // セレクタ更新

	// 背景色を補間更新
	float t = std::clamp(deltaTime * 4.0f, 0.0f, 1.0f);
	bgNow_ = Lerp(bgNow_, bgTarget_, t);
	bgNow_.w = 1.0f; // 念のため
	bg_->SetColor(bgNow_);
	bg_->Update();

	if (input_->TriggerKey(DIK_ESCAPE)) if (context_.onRequestBack) context_.onRequestBack();

	if (fadeController_) fadeController_->Update(deltaTime); // フェード更新
}

void StageSelectScene::Draw3DObjects()
{

}

void StageSelectScene::Draw2DSprites()
{
	SpriteManager::GetInstance()->SetRenderSetting_Background();

	if (bg_) bg_->Draw();

	SpriteManager::GetInstance()->SetRenderSetting_UI();
	if (activeSelector_) activeSelector_->Draw2DSprites();
	if (fadeController_) fadeController_->Draw();
}

void StageSelectScene::Finalize()
{
	if (activeSelector_) activeSelector_->OnExit();
	activeSelector_ = nullptr;
	gridSelector_ = nullptr;
}

void StageSelectScene::DrawImGui()
{

}
