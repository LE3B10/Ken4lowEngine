#define NOMINMAX
#include "TitleScene.h"
#include <DirectXCommon.h>
#include <SpriteManager.h>
#include <CameraManager.h>
#include <ImGuiManager.h>
#include "SceneManager.h"
#include <CollisionUtility.h>
#include "Input.h"
#include <Wireframe.h>
#include <LinearInterpolation.h>
#include <AudioManager.h>
#include <PostEffectManager.h>
#include <LightManager.h>
#include <GameTimer.h>

#include "TitleLoadState.h"
#include "TitleAttractState.h"
#include "TitleLobbyState.h"

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void TitleScene::Initialize()
{
	LightManager::GetInstance()->AddDefaultDirectionalLight();

	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize("SkyBox/skybox.dds");

	timers_.state = timers_.idle = 0.0f;
	timers_.inputCooldownLeft = 0.0f;


	// カメラの初期化
	InitializeCamera();

	// タイトルロゴの初期化
	InitializeLogoUI();

	// バトルへボタンの初期化
	InitializeBattleButtonUI();

	// 影の初期化
	InitializeButtonShadowSprite();

	// クリックヒントの初期化
	InitializeClickHintUI();

	// 段階ロードの初期状態
	loadStep_ = 0;
	isLoadReady_ = false;

	// まだ重い生成はしない
	skyBox_.reset();
	terrain_.reset();
	currentState_.reset();

	state_ = State::Loading;
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void TitleScene::Update()
{
	// デバッグ更新
	UpdateDebug();

	// ロード中は軽く回すだけ
	if (state_ == State::Loading)
	{
		if (skyBox_) skyBox_->Update();
		if (terrain_) terrain_->Update();
		return;
	}

	// 地形更新
	if (terrain_) terrain_->Update();

	// スカイボックス更新
	if (skyBox_) skyBox_->Update();

	if (isDebugCamera_) return; // デバッグカメラ中はポーズ無効

	// 時間更新
	const float dt = K4E::GameTimer::GetInstance()->GetDeltaTime();
	timers_.state += dt;

	// 入力クールダウン更新
	if (timers_.inputCooldownLeft > 0.0f) { timers_.inputCooldownLeft = std::max(0.0f, timers_.inputCooldownLeft - dt); }

	// ====== すでにオーバーレイが出ているならそれを最優先で更新＆早期return ======
	if (quitOverlay_)
	{
		quitOverlay_->Update();                                           // ConfirmQuitOverlay は自前でマウス/キー処理を持つ
		if (quitOverlay_->IsClose()) { quitOverlay_.reset(); }            // 閉じられたら破棄
		skyBox_->Update();
		return;                                                           // オーバーレイ中は他の入力・遷移を止める
	}

	// ====== トリガー（ESC / Q）で生成して Open(sceneManager_) ======
	if (input_->TriggerKey(DIK_ESCAPE)) {
		quitOverlay_ = std::make_unique<ConfirmQuitOverlay>();
		quitOverlay_->Open(sceneManager_);                                // BaseOverlay::Open で SceneManager を注入
		// Yes: アプリ終了 / No: 何もしない（Close は Overlay 側が呼ぶ）
		quitOverlay_->SetCallbacks([]() {
#ifdef _WIN32
			PostQuitMessage(0); // Windowsアプリ終了
#else
			std::exit(0); // 他プラットフォームでは標準終了
#endif
			},
			[]()
			{}
		);
	}

	// ----- 状態更新共通処理 -----
	if (currentState_)
	{
		// ステートクラスに丸投げ
		currentState_->Update(this, dt);
	}

	UpdateLightViewProjection();
	UpdateShadowMatrices();
}


/// -------------------------------------------------------------
///				　	3Dオブジェクトの描画
/// -------------------------------------------------------------
void TitleScene::Draw3DObjects()
{
#pragma region オブジェクト3Dの描画

	skyBox_->Draw();

	if (terrain_) terrain_->Draw();

#pragma endregion

}

void TitleScene::DrawShadowObjects()
{
	if (terrain_) terrain_->DrawShadow();
}

/// -------------------------------------------------------------
///				　	2Dオブジェクトの描画
/// -------------------------------------------------------------
void TitleScene::Draw2DSprites()
{
#pragma region 背景の描画（後面）

	// 背景用の共通描画設定（後面）
	SpriteManager::GetInstance()->SetRenderSetting_Background();


#pragma endregion


#pragma region UIの描画（前面）
	// UI用の共通描画設定
	SpriteManager::GetInstance()->SetRenderSetting_UI();

	// タイトル系（TitleAttract と ToTitle の間だけロゴを表示）
	if (state_ == State::TitleAttract || state_ == State::ToTitle || logoUI_.exitLeft > 0.0f)
	{
		if (logoSprite_)
		{
			logoSprite_->SetColor({ 1,1,1,logoUI_.alpha });
			const Vector2 sz = { logoUI_.baseSize.x * logoUI_.scale, logoUI_.baseSize.y * logoUI_.scale };
			logoSprite_->SetSize(sz);
			logoSprite_->Draw();

			if (clickHintUI_.isVisible && clickHintUI_.hintSprite) { clickHintUI_.hintSprite->Draw(); }
		}
	}

	// ロビー系（TransitionToLobby と LobbyIdle の間だけロビーUIを表示）
	if (state_ == State::TransitionToLobby ||
		state_ == State::LobbyIdle ||
		state_ == State::Loading)
	{
		if (battleButtonUI_.btnShadow) { battleButtonUI_.btnShadow->Draw(); } // ← 影を先に
		if (battleButtonUI_.btnSprite) { battleButtonUI_.btnSprite->Draw(); } // ← ボタン本体
	}

	// ====== 最後にオーバーレイを最前面へ重ね描き ======
	if (quitOverlay_) {
		quitOverlay_->Draw2D();
	}

#pragma endregion

}


/// -------------------------------------------------------------
///				　			　 終了処理
/// -------------------------------------------------------------
void TitleScene::Finalize()
{
	AudioManager::GetInstance()->StopBGM();

	// ステートを確実に終了
	if (currentState_) { currentState_->Exit(this); }
	currentState_.reset();

	// オーバーレイ破棄
	quitOverlay_.reset();

	// UI破棄（ここが今抜けてる）
	clickHintUI_.hintSprite.reset();
	battleButtonUI_.btnSprite.reset();
	battleButtonUI_.btnShadow.reset();

	logoSprite_.reset();
	terrain_.reset();
	skyBox_.reset();

	camera_ = nullptr;
	input_ = nullptr;
	dxCommon_ = nullptr;
}


/// -------------------------------------------------------------
///				　		　ImGui描画処理
/// -------------------------------------------------------------
void TitleScene::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("Title Debug");
	const char* stateNames =
		state_ == State::TitleAttract ? "TitleAttract" :
		state_ == State::TransitionToLobby ? "TransitionToLobby" :
		state_ == State::LobbyIdle ? "LobbyIdle" :
		state_ == State::Loading ? "Loading" : "";
	ImGui::Text("State: %s", stateNames);
	ImGui::Text("Idle: %.1fs / Return: %.0fs", timers_.idle, timers_.returnSeconds);
	ImGui::End();

#endif // USE_IMGUI

	LightManager::GetInstance()->DrawImGui();
}

void TitleScene::StartLoad()
{
	loadStep_ = 0;
	isLoadReady_ = false;
	state_ = State::Loading;
}

void TitleScene::UpdateLoad()
{
	switch (loadStep_)
	{
	case 0:
		// SkyBox 読み込み
		skyBox_ = std::make_unique<SkyBox>();
		skyBox_->Initialize("SkyBox/skybox.dds");
		++loadStep_;
		break;

	case 1:
		// 地形読み込み
		terrain_ = std::make_unique<Object3D>();
		terrain_->Initialize("Stages/lobby.gltf");
		++loadStep_;
		break;

	case 2:
		// 最初のステートに入る
		ChangeState(std::make_unique<TitleAttractState>());
		state_ = State::TitleAttract;
		isLoadReady_ = true;
		++loadStep_;
		break;

	default:
		break;
	}
}

bool TitleScene::IsReadyToStartUncover() const
{
	return isLoadReady_;
}

void TitleScene::UpdateLightViewProjection()
{
	// 今は新規シーンで最小確認したいので、中心は原点付近固定でOK
	shadowCenter_ = { 0.0f, 1.0f, 0.0f };

	lightViewProjection_ = Matrix4x4::MakeLightViewProjection(
		lightDirection_,
		shadowCenter_,
		shadowDistance_,
		orthoHalfWidth_,
		orthoHalfHeight_,
		nearZ_,
		farZ_
	);
}

void TitleScene::UpdateShadowMatrices()
{
	if (terrain_) { terrain_->UpdateShadowMatrix(lightViewProjection_); }
}

/// -------------------------------------------------------------
///				　		　カメラ初期化処理
/// -------------------------------------------------------------
void TitleScene::InitializeCamera()
{
	// カメラの生成と初期化
	camera_ = CameraManager::GetInstance()->GetMainCamera();
	if (camera_)
	{
		// ロビー用の初期位置にセット
		camera_->SetTranslate({ orbitState_.center.x, orbitState_.center.y, orbitState_.center.z - orbitState_.radius });
		orbitState_.lastPitch = -0.10f; orbitState_.lastYaw = std::numbers::pi_v<float>;
		camera_->SetRotate({ orbitState_.lastPitch, orbitState_.lastYaw, 0.0f });
		camera_->Update();
	}
}

/// -------------------------------------------------------------
///				　		　ロゴUI初期化処理
/// -------------------------------------------------------------
void TitleScene::InitializeLogoUI()
{
	logoUI_.scale = 0.9f;   // 少し小さく出して拡大
	logoUI_.showLeft = logoUI_.showDelay;
	logoUI_.exitLeft = 0.0f;

	// ロゴスプライトの生成
	logoSprite_ = std::make_unique<Sprite>();
	logoSprite_->Initialize("UI/Common/logo_rittai_sensen.dds");
	logoUI_.baseSize = logoSprite_->GetSize();
	logoUI_.baseSize *= 0.7f; // 元画像が大きい場合は適宜縮小
	logoSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	logoSprite_->SetPosition({ dxCommon_->GetClientWidth() * 0.5f, dxCommon_->GetClientHeight() * 0.25f });
}

/// -------------------------------------------------------------
///				　	バトルボタンUI初期化処理
/// -------------------------------------------------------------
void TitleScene::InitializeBattleButtonUI()
{
	// バトルボタンUI
	battleButtonUI_.btnSprite = std::make_unique<Sprite>();
	battleButtonUI_.btnSprite->Initialize("UI/Common/btn_battle.dds");
	battleButtonUI_.btnSprite->SetAnchorPoint(battleButtonUI_.anchor);

	battleButtonUI_.position = { dxCommon_->GetClientWidth() * 0.5f, dxCommon_->GetClientHeight() * 0.75f }; // 画面中央下

	battleButtonUI_.btnSprite->SetPosition(battleButtonUI_.position);
	battleButtonUI_.btnSprite->SetSize(battleButtonUI_.size);
}

/// -------------------------------------------------------------
///				　	ボタン影スプライト初期化処理
/// -------------------------------------------------------------
void TitleScene::InitializeButtonShadowSprite()
{
	// 影スプライトも作成
	battleButtonUI_.btnShadow = std::make_unique<Sprite>();
	battleButtonUI_.btnShadow->Initialize("UI/Common/btn_battle.dds");
	battleButtonUI_.btnShadow->SetAnchorPoint(battleButtonUI_.anchor);
	battleButtonUI_.btnShadow->SetPosition({ battleButtonUI_.position.x, battleButtonUI_.position.y + 6.0f }); // 影は少し下
	battleButtonUI_.btnShadow->SetSize({ battleButtonUI_.size.x * 1.02f, battleButtonUI_.size.y * 1.02f }); // わずかに大きく
	battleButtonUI_.btnShadow->SetColor({ 0, 0, 0, 0.35f }); // 半透明の黒
}

/// -------------------------------------------------------------
///				　	クリックヒントUI初期化処理
/// -------------------------------------------------------------
void TitleScene::InitializeClickHintUI()
{
	clickHintUI_.hintSprite = std::make_unique<Sprite>();
	clickHintUI_.hintSprite->Initialize("UI/Common/ui_click_hint.dds");
	clickHintUI_.hintSprite->SetAnchorPoint({ 0.5f, 0.0f });      // 中央上
	// ここは今の 1/10 スケール指定のままでOK
	clickHintUI_.hintSprite->SetPosition({ dxCommon_->GetClientWidth() * 0.5f + clickHintUI_.offset.x, dxCommon_->GetClientHeight() * 0.25f + clickHintUI_.offset.y });
	clickHintUI_.hintSprite->SetSize({ 153.6f, 102.4f });       // 元画像が1536x1024pxなので1/10スケール
	clickHintUI_.baseSize = clickHintUI_.hintSprite->GetSize(); // 基準サイズを保存
}

/// -------------------------------------------------------------
///				　　ステート変更処理の共通化
/// -------------------------------------------------------------
void TitleScene::ChangeState(std::unique_ptr<ITitleSceneState> newState)
{
	// 今のステートから抜ける
	if (currentState_) {
		currentState_->Exit(this);
	}

	// ステート差し替え
	currentState_ = std::move(newState);

	// 新しいステートに入る
	if (currentState_) {
		currentState_->Enter(this);
	}
}


/// -------------------------------------------------------------
///				カメラの確保（なければデフォルトを取得）
/// -------------------------------------------------------------
Camera* TitleScene::EnsureCamera()
{
	if (!camera_) {
		camera_ = CameraManager::GetInstance()->GetMainCamera();
	}
	return camera_;
}

/// -------------------------------------------------------------
///				　	デバッグ用更新（キー入力など）
/// -------------------------------------------------------------
void TitleScene::UpdateDebug()
{
#ifdef _DEBUG
	if (input_->TriggerKey(DIK_BACK))
	{
		// タイトルに来たら必ずピクセルエフェクトはOFFにしておく
		PostEffectManager::GetInstance()->DisableEffect("PixelateEffect");

		if (sceneManager_)
		{
			sceneManager_->ChangeScene("PhysicalScene"); // 戻るキーでゲームプレイシーンに戻る
		}
	}

	if (input_->TriggerKey(DIK_F12))
	{
		const bool next = !CameraManager::GetInstance()->IsUsingDebugCamera();

		CameraManager::GetInstance()->SetUseDebugCamera(next);
		skyBox_->SetDebugCamera(next);
		Wireframe::GetInstance()->SetDebugCamera(next);
		isDebugCamera_ = next;
	}
#endif // _DEBUG
}