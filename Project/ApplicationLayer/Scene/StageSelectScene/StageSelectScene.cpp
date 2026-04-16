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
#include <GameTimer.h>
#include "FontAtlasLoader.h"
#include "TextSpriteDrawer.h"

#include <algorithm>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void StageSelectScene::Initialize()
{
	dxCommon_ = K4E::DirectXCommon::GetInstance();
	input_ = K4E::Input::GetInstance();

	// 軽い初期値だけ
	stages_.clear();
	context_ = {};
	gridSelector_.reset();
	activeSelector_ = nullptr;
	bg_.reset();

	pendingUnlockIndex_ = -1;
	nextScene_ = NextScene::None;

	loadStep_ = 0;
	isLoadReady_ = false;

	// ロード中として待機
	state_ = State::Loading;
	currentState_.reset();

	textJPDrawer_ = std::make_unique<K4E::TextSpriteDrawer>();
	textLatinDrawer_ = std::make_unique<K4E::TextSpriteDrawer>();
	isTextReady_ = false;

	try
	{
		// ここは自分の実際のフォント生成物のパスに合わせて変更
		auto fontDefJP = K4E::FontAtlasLoader::LoadFromJson(
			"UI/Font/JP/DotGothic16-Regular_atlas.dds",
			"Resources/Fonts/Compiled/JP/DotGothic16-Regular.json",
			32.0f,
			32.0f,
			U'?'
		);

		auto fontDefLatin = K4E::FontAtlasLoader::LoadFromJson(
			"UI/Font/Latin/DotGothic16-Regular_atlas.dds",
			"Resources/Fonts/Compiled/Latin/DotGothic16-Regular.json",
			32.0f,
			32.0f,
			U'?'
		);

		textJPDrawer_->Initialize(fontDefJP);
		textLatinDrawer_->Initialize(fontDefLatin);
		isTextReady_ = true;
	}
	catch (...)
	{
		isTextReady_ = false;
	}
}

/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void StageSelectScene::Update()
{
	float deltaTime = K4E::GameTimer::GetInstance()->GetDeltaTime();

	// ロード中は通常更新をしない
	if (state_ == State::Loading)
	{
		// 背景だけあれば最低限更新
		if (bg_) { bg_->Update(); }
		return;
	}

	// ステート更新
	if (currentState_)
	{
		currentState_->Update(this, deltaTime);
	}

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

	// ---- ここからテキスト描画テスト ----
	if (textJPDrawer_ && textLatinDrawer_ && isTextReady_)
	{
		textLatinDrawer_->Reset();
		textJPDrawer_->Reset();

		textLatinDrawer_->SetScale(1.0f);
		textLatinDrawer_->SetLetterSpacing(2.0f);
		textLatinDrawer_->SetLineSpacing(6.0f);

		textLatinDrawer_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		textLatinDrawer_->DrawTextCentered(
			"STAGE SELECT",
			{ context_.screenWidth * 0.5f, 40.0f }
		);

		textJPDrawer_->SetScale(0.9f);
		textJPDrawer_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		textJPDrawer_->DrawTextCentered(
			"始まりの平原",
			{ context_.screenWidth * 0.5f, context_.screenHeight - 120.0f }
		);

		textLatinDrawer_->SetScale(0.7f);
		textLatinDrawer_->SetColor({ 0.85f, 0.90f, 1.0f, 1.0f });
		textLatinDrawer_->DrawTextCentered(
			"WAVE",
			{ context_.screenWidth * 0.5f, context_.screenHeight - 84.0f }
		);
	}
}

/// -------------------------------------------------------------
///				　			　終了処理
/// -------------------------------------------------------------
void StageSelectScene::Finalize()
{
	if (textJPDrawer_)
	{
		textJPDrawer_->Finalize();
		textJPDrawer_.reset();
	}

	if (textLatinDrawer_)
	{
		textLatinDrawer_->Finalize();
		textLatinDrawer_.reset();
	}

	isTextReady_ = false;

	if (currentState_)
	{
		currentState_->Exit(this);
	}
	currentState_.reset();

	if (activeSelector_)
	{
		activeSelector_->OnExit();
	}
	activeSelector_ = nullptr;
	gridSelector_.reset();

	bg_.reset();

	stages_.clear();
	pendingUnlockIndex_ = -1;
	nextScene_ = NextScene::None;
	state_ = State::Selecting;

	input_ = nullptr;
	dxCommon_ = nullptr;
}

/// -------------------------------------------------------------
///				　			ImGui描画処理
/// -------------------------------------------------------------
void StageSelectScene::DrawImGui()
{

}

void StageSelectScene::StartLoad()
{
	loadStep_ = 0;
	isLoadReady_ = false;
	state_ = State::Loading;
}

void StageSelectScene::UpdateLoad()
{
	switch (loadStep_)
	{
	case 0:
		// ステージ情報だけ先に作る
		InitializeStages();
		++loadStep_;
		break;

	case 1:
		// セレクタのコンテキストだけ組む
		InitializeSelectors();
		++loadStep_;
		break;

	case 2:
		// 背景と GridSelector の生成
		InitializeBackground();
		++loadStep_;
		break;

	case 3:
		// 選択状態へ入る
		ChangeState(std::make_unique<StageSelectSelectingState>());
		state_ = State::Selecting;
		isLoadReady_ = true;
		++loadStep_;
		break;

	default:
		break;
	}
}

bool StageSelectScene::IsReadyToStartUncover() const
{
	return isLoadReady_;
}

/// -------------------------------------------------------------
///				　		　ステージ情報初期化
/// -------------------------------------------------------------
void StageSelectScene::InitializeStages()
{
	stages_.clear();

	stages_.push_back({
		0u,
		"始まりの平原",
		"WAVE",
		"UI/StageSelect/stage01.dds",
		"基本戦闘を学ぶウェーブ制ステージ",
		"",
		false,
		0u,
		{ 0.18f, 0.49f, 0.20f, 1.0f },
		false
		});

	stages_.push_back({
		1u,
		"忘れられた坑道",
		"SEARCH",
		"UI/StageSelect/stage02.dds",
		"ルート探索と装置起動を進める探索ステージ",
		"Stage 1 クリアで解放",
		true,
		0u,
		{ 0.43f, 0.30f, 0.25f, 1.0f },
		false
		});

	stages_.push_back({
		2u,
		"旧防衛拠点",
		"DEFENSE",
		"UI/StageSelect/stage03.dds",
		"波状攻撃から拠点を守り抜く防衛ステージ",
		"Stage 2 クリアで解放",
		true,
		0u,
		{ 0.25f, 0.38f, 0.62f, 1.0f },
		false
		});

	stages_.push_back({
		3u,
		"崩落都市圏",
		"ESCAPE",
		"UI/StageSelect/stage04.dds",
		"敵をかわしながら出口を目指す脱出ステージ",
		"Stage 3 クリアで解放",
		true,
		0u,
		{ 0.60f, 0.32f, 0.22f, 1.0f },
		false
		});

	stages_.push_back({
		4u,
		"中枢制御塔",
		"BOSS",
		"UI/StageSelect/stage05.dds",
		"最終ボスとの決戦に挑む最終ステージ",
		"Stage 4 クリアで解放",
		true,
		0u,
		{ 0.45f, 0.18f, 0.18f, 1.0f },
		false
		});
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
	if (!saved.empty() && saved.size() == stages_.size())
	{
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
	bg_->Initialize("Effects/white.dds");
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

	if (startIndex >= 0 && startIndex < (int)stages_.size())
	{
		activeSelector_->FocusToIndex(startIndex, false);

		if (stages_[startIndex].justUnlocked)
		{
			pendingUnlockIndex_ = startIndex; // ← フェード完了後に再生する
		}

		bgNow_ = bgTarget_ = stages_[startIndex].color;
		if (bg_)
		{
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
