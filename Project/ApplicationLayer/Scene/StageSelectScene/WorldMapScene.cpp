#define NOMINMAX
#include "WorldMapScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include <SpriteManager.h>
#include "SceneManager.h"
#include "StageRepository.h"

void WorldMapScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	// 1) ステージ一覧をリポジトリから取得
	stages_ = StageRepository::GetInstance().GetStages();

	if (stages_.empty()) 
	{
		// フォールバック（万一空なら最低限を用意）
		stages_.push_back({ 0u,"Forest","white.png",false,0u });
		stages_.push_back({ 1u,"Desert","white.png",false,0u });
		stages_.push_back({ 2u,"City",  "white.png",false,0u });
	}

	// 2) フェード
	fade_ = std::make_unique<FadeController>();
	const float w = (float)dxCommon_->GetSwapChainDesc().Width;
	const float h = (float)dxCommon_->GetSwapChainDesc().Height;
	fade_->Initialize(w, h, "white.png");
	fade_->SetFadeMode(FadeController::FadeMode::Checkerboard);
	fade_->SetGrid(14, 8); fade_->SetCheckerDelay(0.012f);
	fade_->StartFadeIn(0.28f);

	// 3) WorldMap セレクタを起動
	ctx_.screenWidth = w;
	ctx_.screenHeight = h;
	ctx_.input = input_;
	ctx_.fadeController = fade_.get();
	ctx_.stages = &stages_;

	// 3a) クリック時の遷移：中央クリック→GamePlay
	ctx_.onRequestPlay = [this](uint32_t stageId) {
		fade_->SetOnComplete([this] {
			if (sceneManager_) sceneManager_->ChangeScene("GamePlayScene");
			});
		fade_->StartFadeOut(0.32f);
		(void)stageId;
		};
	// 3b) 戻る：右クリックやEscで戻すなどを入れるならここに
	ctx_.onRequestBack = [this] {
		fade_->SetOnComplete([this] {
			if (sceneManager_) sceneManager_->ChangeScene("StageSelectScene");
			});
		fade_->StartFadeOut(0.18f);
		};
	// 3c) onRequestMap は不要（ここはマップそのもののシーン）

	auto world = std::make_unique<WorldMapStageSelector>();
	world->Initialize(ctx_);
	selector_ = std::move(world);
	active_ = selector_.get();
	active_->OnEnter();

	// 4) StageSelectScene から渡された開始フォーカスに寄せる
	if (auto start = StageRepository::GetInstance().GetStartIndex()) {
		active_->FocusToIndex(*start, /*tween=*/true);
		StageRepository::GetInstance().ClearStartIndex();
	}
}

void WorldMapScene::Update()
{
	float dt = dxCommon_->GetFPSCounter().GetDeltaTime();
	if (!(dt > 0.f) || dt > 0.05f) dt = 1.f / 60.f;

	if (active_) active_->Update(dt);
	if (fade_)   fade_->Update(dt);

	// 任意：Escで戻る
	if (input_->TriggerKey(DIK_ESCAPE)) {
		ctx_.onRequestBack();
	}
}

void WorldMapScene::Draw3DObjects()
{
	if (active_) active_->Draw3DObjects(); // 基本空
}

void WorldMapScene::Draw2DSprites()
{
	SpriteManager::GetInstance()->SetRenderSetting_UI();
	if (active_) active_->Draw2DSprites();
	if (fade_)   fade_->Draw();
}

void WorldMapScene::Finalize()
{
	if (active_) active_->OnExit();
	selector_.reset();
	fade_.reset();
}

void WorldMapScene::DrawImGui()
{
}
