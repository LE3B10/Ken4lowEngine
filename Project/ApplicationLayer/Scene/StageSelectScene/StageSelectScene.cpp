#define NOMINMAX
#include "GameViewportConstants.h"
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
#include <CameraManager.h>
#include <Editor/EditorTransformAccess.h>
#include "FontAtlasLoader.h"
#include "TextSpriteDrawer.h"

#include <algorithm>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#include <Editor/EditorWindowManager.h>
#endif // USE_IMGUI

namespace K4E = ::Ken4lowEngine;

namespace
{
	// カテゴリに対応する表示名を取得
	const char* GetCategoryDisplayName(const std::string& category)
	{
		if (category == "WAVE")    return "WAVE STAGE";
		if (category == "SEARCH")  return "SEARCH STAGE";
		if (category == "DEFENSE") return "DEFENSE STAGE";
		if (category == "ESCAPE")  return "ESCAPE STAGE";
		if (category == "BOSS")    return "BOSS STAGE";
		return "UNKNOWN";
	}

	// ステージのロック状態に応じた解放条件テキストを生成
	std::string BuildUnlockConditionText(const StageInfo& stage)
	{
		if (!stage.locked)
		{
			return "";
		}

		return "前ステージクリアで解放";
	}

	// カテゴリに対応するキャッチコピーを取得
	const char* GetCategoryCatchCopy(const std::string& category)
	{
		if (category == "WAVE")    return "正面突破で戦況を切り開け";
		if (category == "SEARCH")  return "探索して進路を切り開け";
		if (category == "DEFENSE") return "拠点を守り抜け";
		if (category == "ESCAPE")  return "敵をかわして脱出せよ";
		if (category == "BOSS")    return "最終決戦に挑め";
		return "";
	}

	// カテゴリに対応するアクセントカラーを取得
	K4E::Vector4 GetCategoryAccentColor(const std::string& category)
	{
		if (category == "WAVE")    return { 0.95f, 0.95f, 1.00f, 1.0f };
		if (category == "SEARCH")  return { 1.00f, 0.90f, 0.65f, 1.0f };
		if (category == "DEFENSE") return { 0.75f, 0.90f, 1.00f, 1.0f };
		if (category == "ESCAPE")  return { 1.00f, 0.80f, 0.70f, 1.0f };
		if (category == "BOSS")    return { 1.00f, 0.60f, 0.60f, 1.0f };
		return { 1,1,1,1 };
	}
}

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

	textLayoutDebug_ = CreateDefaultTextLayoutDebug();

	textAnim_.prevStageIndex = currentStageIndex_;
	textAnim_.changeTimer = textAnim_.changeDuration;
	textAnim_.guidePulseTimer = 0.0f;
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
		if (bg_) { bg_->Update(); }
		return;
	}

	// ステート更新
	if (currentState_)
	{
		currentState_->Update(this, deltaTime);
	}

	// ステージ切替検知
	if (textAnim_.prevStageIndex != currentStageIndex_)
	{
		textAnim_.prevStageIndex = currentStageIndex_;
		textAnim_.changeTimer = 0.0f;
	}

	// 切替演出タイマー更新
	textAnim_.changeTimer = std::min(textAnim_.changeTimer + deltaTime, textAnim_.changeDuration);

	// 操作ガイド点滅タイマー更新
	textAnim_.guidePulseTimer += deltaTime;
}


/// -------------------------------------------------------------
///				 	Editor中の更新処理
/// -------------------------------------------------------------
void StageSelectScene::UpdateEditor(float deltaTime)
{
	// Edit/Pause中はステージ決定やTitleへの戻り入力を処理せず、表示確認用の軽い演出だけ進める。
	if (bg_) { bg_->Update(); }

	if (textAnim_.prevStageIndex != currentStageIndex_)
	{
		textAnim_.prevStageIndex = currentStageIndex_;
		textAnim_.changeTimer = 0.0f;
	}

	textAnim_.changeTimer = std::min(textAnim_.changeTimer + deltaTime, textAnim_.changeDuration);
	textAnim_.guidePulseTimer += deltaTime;
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
	K4E::SpriteManager::GetInstance()->SetRenderSetting_Background();
	if (bg_) bg_->Draw();

	K4E::SpriteManager::GetInstance()->SetRenderSetting_UI();
	float animT = 1.0f;
	if (textAnim_.changeDuration > 0.0f)
	{
		animT = std::clamp(textAnim_.changeTimer / textAnim_.changeDuration, 0.0f, 1.0f);
	}
	float enterEase = K4E::EaseOutCubic(animT);

	// 下から少し上がってくる
	float enterOffsetY = K4E::Lerp(20.0f, 0.0f, enterEase);

	// フェードイン
	float enterAlpha = K4E::Lerp(0.0f, 1.0f, enterEase);

	// カテゴリだけ少し拡大して戻す
	float categoryScaleAnim = K4E::Lerp(1.08f, 1.0f, enterEase);

	// 操作ガイドのゆるい点滅
	float pulse = (std::sin(textAnim_.guidePulseTimer * 2.0f) + 1.0f) * 0.5f; // 0..1
	float guideAlpha = K4E::Lerp(0.70f, 0.95f, pulse);

	if (activeSelector_) activeSelector_->Draw2DSprites();

	if (!(textJPDrawer_ && textLatinDrawer_ && isTextReady_)) { return; }
	if (stages_.empty()) { return; }

	int index = std::clamp(currentStageIndex_, 0, (int)stages_.size() - 1);
	const StageInfo& stage = stages_[index];

	textLatinDrawer_->Reset();
	textJPDrawer_->Reset();

	const float centerX = context_.screenWidth * 0.5f + textLayoutDebug_.centerXOffset;
	const float screenH = context_.screenHeight;

	// タイトル
	textLatinDrawer_->SetScale(textLayoutDebug_.titleScale);
	textLatinDrawer_->SetLetterSpacing(2.0f);
	textLatinDrawer_->SetLineSpacing(6.0f);
	textLatinDrawer_->SetColor({ 1.0f, 1.0f, 1.0f, 0.95f });
	textLatinDrawer_->DrawTextCentered(
		"STAGE SELECT",
		{ centerX, textLayoutDebug_.titleY }
	);

	// ステージ番号
	char stageNo[32];
	std::snprintf(stageNo, sizeof(stageNo), "STAGE %02u", stage.id + 1);

	textLatinDrawer_->SetScale(textLayoutDebug_.stageNoScale);
	textLatinDrawer_->SetColor({ 0.92f, 0.96f, 1.0f, enterAlpha });
	textLatinDrawer_->DrawTextCentered(
		stageNo,
		{ centerX, screenH + textLayoutDebug_.stageNoY + enterOffsetY }
	);

	// ステージ名
	textJPDrawer_->SetScale(textLayoutDebug_.stageNameScale);
	textJPDrawer_->SetColor({ 1.0f, 1.0f, 1.0f, enterAlpha });
	textJPDrawer_->DrawTextCentered(
		stage.name,
		{ centerX, screenH + textLayoutDebug_.stageNameY + enterOffsetY }
	);

	// カテゴリ
	K4E::Vector4 accent = GetCategoryAccentColor(stage.category);

	textLatinDrawer_->SetScale(textLayoutDebug_.categoryScale * categoryScaleAnim);
	textLatinDrawer_->SetColor({ accent.x, accent.y, accent.z, enterAlpha });
	textLatinDrawer_->DrawTextCentered(
		GetCategoryDisplayName(stage.category),
		{ centerX, screenH + textLayoutDebug_.categoryY + enterOffsetY }
	);

	// キャッチコピー
	textJPDrawer_->SetScale(textLayoutDebug_.catchScale);
	textJPDrawer_->SetColor({ 0.95f, 0.95f, 0.95f, enterAlpha });
	textJPDrawer_->DrawTextCentered(
		GetCategoryCatchCopy(stage.category),
		{ centerX, screenH + textLayoutDebug_.catchY + enterOffsetY }
	);

	// 説明文
	textJPDrawer_->SetScale(textLayoutDebug_.descScale);
	textJPDrawer_->SetColor({ 0.88f, 0.92f, 0.95f, enterAlpha });
	textJPDrawer_->DrawTextCentered(
		stage.description,
		{ centerX, screenH + textLayoutDebug_.descY + enterOffsetY }
	);

	// ロック時の解放条件
	if (stage.locked)
	{
		textJPDrawer_->SetScale(textLayoutDebug_.unlockScale);
		textJPDrawer_->SetColor({ 1.0f, 0.82f, 0.82f, enterAlpha });
		textJPDrawer_->DrawTextCentered(
			BuildUnlockConditionText(stage),
			{ centerX, screenH + textLayoutDebug_.unlockY + enterOffsetY }
		);
	}

	// 操作ガイド
	textLatinDrawer_->SetScale(textLayoutDebug_.guideScale);
	textLatinDrawer_->SetColor({ 1.0f, 1.0f, 1.0f, guideAlpha });
	textLatinDrawer_->DrawTextCentered(
		"CLICK : SELECT   WHEEL / DRAG : MOVE   ESC : BACK",
		{ centerX, screenH + textLayoutDebug_.guideY }
	);
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
#ifdef USE_IMGUI
	// WindowメニューのStage Select Debug表示フラグを×ボタン状態と共有する
	auto& editorWindowState = K4E::EditorWindowManager::GetInstance()->GetWindowState();
	if (!textLayoutDebug_.enableImGui || !editorWindowState.showStageSelectDebug) { return; }

	if (ImGui::Begin("StageSelect Text Layout", &editorWindowState.showStageSelectDebug))
	{
		ImGui::Text("Current Stage Index : %d", currentStageIndex_);

		ImGui::SeparatorText("Global");
		ImGui::DragFloat("Center X Offset", &textLayoutDebug_.centerXOffset, 1.0f, -400.0f, 400.0f);

		ImGui::SeparatorText("Title");
		ImGui::DragFloat("Title Y", &textLayoutDebug_.titleY, 1.0f, 0.0f, 300.0f);
		ImGui::DragFloat("Title Scale", &textLayoutDebug_.titleScale, 0.01f, 0.2f, 3.0f);

		ImGui::SeparatorText("Stage No");
		ImGui::DragFloat("StageNo Y", &textLayoutDebug_.stageNoY, 1.0f, -1000.0f, 100.0f);
		ImGui::DragFloat("StageNo Scale", &textLayoutDebug_.stageNoScale, 0.01f, 0.2f, 3.0f);

		ImGui::SeparatorText("Stage Name");
		ImGui::DragFloat("StageName Y", &textLayoutDebug_.stageNameY, 1.0f, -1000.0f, 100.0f);
		ImGui::DragFloat("StageName Scale", &textLayoutDebug_.stageNameScale, 0.01f, 0.2f, 3.0f);

		ImGui::SeparatorText("Category");
		ImGui::DragFloat("Category Y", &textLayoutDebug_.categoryY, 1.0f, -1000.0f, 100.0f);
		ImGui::DragFloat("Category Scale", &textLayoutDebug_.categoryScale, 0.01f, 0.2f, 3.0f);

		ImGui::SeparatorText("Catch Copy");
		ImGui::DragFloat("Catch Y", &textLayoutDebug_.catchY, 1.0f, -1000.0f, 100.0f);
		ImGui::DragFloat("Catch Scale", &textLayoutDebug_.catchScale, 0.01f, 0.2f, 3.0f);

		ImGui::SeparatorText("Description");
		ImGui::DragFloat("Desc Y", &textLayoutDebug_.descY, 1.0f, -1000.0f, 100.0f);
		ImGui::DragFloat("Desc Scale", &textLayoutDebug_.descScale, 0.01f, 0.2f, 3.0f);

		ImGui::SeparatorText("Unlock");
		ImGui::DragFloat("Unlock Y", &textLayoutDebug_.unlockY, 1.0f, -1000.0f, 100.0f);
		ImGui::DragFloat("Unlock Scale", &textLayoutDebug_.unlockScale, 0.01f, 0.2f, 3.0f);

		ImGui::SeparatorText("Guide");
		ImGui::DragFloat("Guide Y", &textLayoutDebug_.guideY, 1.0f, -1000.0f, 100.0f);
		ImGui::DragFloat("Guide Scale", &textLayoutDebug_.guideScale, 0.01f, 0.2f, 3.0f);

		if (ImGui::Button("Reset Layout"))
		{
			textLayoutDebug_ = CreateDefaultTextLayoutDebug();
		}
	}
	ImGui::End();
#endif // USE_IMGUI

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

	// StageSelect UIはMain Viewportの表示サイズに引っ張られない固定内部解像度で配置する。
	context_.screenWidth = static_cast<float>(K4E::GameViewportConstants::Width);
	context_.screenHeight = static_cast<float>(K4E::GameViewportConstants::Height);

	bg_->SetSize({ context_.screenWidth, context_.screenHeight });
	bg_->SetColor(bgNow_);
	bg_->Update();

	// Gridセレクタを生成してアクティブ化
	gridSelector_ = std::make_unique<GridStageSelector>();
	gridSelector_->Initialize(context_);

	// 中央カードが変わったら背景ターゲット色を更新
	static_cast<GridStageSelector*>(gridSelector_.get())->SetOnCenterChanged(
		[this](uint32_t centerIndex)
		{
			if (centerIndex < stages_.size())
			{
				currentStageIndex_ = static_cast<int>(centerIndex);
				bgTarget_ = stages_.at(centerIndex).color;
			}
		});

	activeSelector_ = gridSelector_.get();

	if (startIndex >= 0 && startIndex < (int)stages_.size())
	{
		currentStageIndex_ = startIndex;
		activeSelector_->FocusToIndex(startIndex, false);

		if (stages_[startIndex].justUnlocked)
		{
			pendingUnlockIndex_ = startIndex;
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

StageSelectScene::StageSelectTextLayoutDebug StageSelectScene::CreateDefaultTextLayoutDebug() const
{
	StageSelectTextLayoutDebug d{};
	d.enableImGui = true;

	d.titleY = 40.0f;
	d.titleScale = 1.15f;

	d.stageNoY = -750.0f;
	d.stageNoScale = 0.60f;

	d.stageNameY = -700.0f;
	d.stageNameScale = 0.90f;

	d.categoryY = -390.0f;
	d.categoryScale = 0.62f;

	d.catchY = -340.0f;
	d.catchScale = 0.56f;

	d.descY = -300.0f;
	d.descScale = 0.50f;

	d.unlockY = -420.0f;
	d.unlockScale = 0.46f;

	d.guideY = -50.0f;
	d.guideScale = 0.40f;

	d.centerXOffset = 0.0f;

	return d;
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

void StageSelectScene::CollectEditorObjects(std::vector<Ken4lowEngine::EditorObjectInfo>& outObjects)
{
	const auto addObject = [&outObjects](uint64_t id, const char* displayName, const char* typeName)
	{
		// Transform未対応の管理項目はDetailsで理由だけを表示し、編集入口を持たせない。
		outObjects.push_back({ id, displayName, typeName, "StageSelectScene" });
	};
	const auto addCameraObject = [&outObjects](uint64_t id, const char* displayName, const char* typeName)
	{
		// StageSelectSceneはCameraManagerのメインカメラを安全なTransform編集入口として公開する。
		outObjects.push_back(Ken4lowEngine::MakeCameraEditorObject(id, displayName, typeName, "StageSelectScene", K4E::CameraManager::GetInstance()->GetMainCamera()));
	};
	const auto addLightObject = [&outObjects](uint64_t id, const char* displayName, const char* typeName)
	{
		// LightManagerの先頭ライトを安全なindex指定でDetails編集へ公開する。
		outObjects.push_back(Ken4lowEngine::MakePunctualLightEditorObject(id, displayName, typeName, "StageSelectScene", 0));
	};
	const auto addSpriteObject = [&outObjects, this](uint64_t id, const char* displayName, const char* typeName)
	{
		// 背景Spriteは2D TransformとしてPosition/Rotation/Scale(Size)へ写像して編集する。
		outObjects.push_back(Ken4lowEngine::MakeSpriteEditorObject(id, displayName, typeName, "StageSelectScene", bg_.get()));
	};

	// Outlinerはステージ選択画面の編集入口として、現在生成済みでなくても主要カテゴリを安定IDで列挙する。
	addObject(Ken4lowEngine::MakeStableEditorObjectId("StageSelectScene.Root"), "StageSelect Root", "Scene Root");
	addCameraObject(Ken4lowEngine::MakeStableEditorObjectId("StageSelectScene.Camera"), "Camera", "Camera");
	addLightObject(Ken4lowEngine::MakeStableEditorObjectId("StageSelectScene.DirectionalLight.0"), "Directional Light", "Directional Light");
	addObject(Ken4lowEngine::MakeStableEditorObjectId("StageSelectScene.StagePanels"), "Stage Panels", activeSelector_ ? "Stage Selector" : "Stage Selector (pending)");
	{
		Ken4lowEngine::EditorObjectInfo textLayout{ Ken4lowEngine::MakeStableEditorObjectId("StageSelectScene.StageTextLayout"), "Stage Text Layout", "StageSelect Text Layout", "StageSelectScene" };
		// 専用StageSelect Text Layoutウィンドウと競合しないよう、Detailsには編集先の案内だけを持たせる。
		textLayout.inspectorHint = "Use StageSelect Text Layout tab for detailed editing.";
		outObjects.push_back(std::move(textLayout));
	}
	addSpriteObject(Ken4lowEngine::MakeStableEditorObjectId("StageSelectScene.BackgroundUI"), "Background UI", bg_ ? "Sprite UI" : "Sprite UI (pending)");
}
