#define NOMINMAX
#include "GamePlayScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include <Input.h>
#include <SpriteManager.h>
#include "Object3DCommon.h"
#include "SkyBoxManager.h"
#include "Wireframe.h"
#include "AudioManager.h"
#include <SceneManager.h>
#include "LevelLoader.h"
#include "LinearInterpolation.h"

#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG
#include <CollisionUtility.h>

static inline float DeltaAngle(float a, float b)
{
	// -π..+π の差に正規化
	float d = std::fmod(b - a + std::numbers::pi_v<float>, 2.0f * std::numbers::pi_v<float>);
	if (d < 0.0f) d += 2.0f * std::numbers::pi_v<float>;
	return d - std::numbers::pi_v<float>;
}

float GamePlayScene::SmoothDampAngle(float current, float target, float& currentVelocity, float smoothTime, float deltaTime)
{
	const float eps = 1e-5f;
	smoothTime = (smoothTime < eps) ? eps : smoothTime;
	float omega = 2.0f / smoothTime;

	float delta = DeltaAngle(current, target);
	float temp = (currentVelocity + omega * delta) * deltaTime;

	// 近似的な指数減衰（臨界減衰）
	float x = omega * deltaTime;
	float expDecay = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

	float result = current + (delta + temp) * expDecay;
	currentVelocity = (currentVelocity - omega * temp) * expDecay;
	return result;
}

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void GamePlayScene::Initialize()
{
#ifdef _DEBUG
	// デバッグカメラの初期化
	DebugCamera::GetInstance()->Initialize();
#endif // _DEBUG

	//StartIntroCutscene();
	//gameState_ = GameState::CutScene;   // 最初は必ずCutSceneへ
	Input::GetInstance()->SetLockCursor(true);
	ShowCursor(false);

	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	fadeController_ = std::make_unique<FadeController>();
	fadeController_->Initialize(static_cast<float>(dxCommon_->GetSwapChainDesc().Width), static_cast<float>(dxCommon_->GetSwapChainDesc().Height), "white.png");
	fadeController_->SetFadeMode(FadeController::FadeMode::Checkerboard);
	fadeController_->SetGrid(14, 8);
	fadeController_->SetCheckerDelay(0.036f);
	fadeController_->StartFadeIn(0.8f); // 暗転明け

	terrein_ = std::make_unique<Object3D>();
	// 地形オブジェクトの初期化
	terrein_->Initialize("cube.gltf");
	terrein_->SetTranslate({ 0.0f,1.0f,0.0f });

	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize("SkyBox/skybox.dds");

	player_ = std::make_unique<Player>();
	player_->Initialize();

	crosshair_ = std::make_unique<Crosshair>();
	crosshair_->Initialize();

	scarecrow_ = std::make_unique<Scarecrow>();
	scarecrow_->Initialize();

	itemManager_ = std::make_unique<ItemManager>();
	itemManager_->Initialize();
	itemManager_->Spawn(ItemType::HealSmall, player_->GetWorldTransform()->translate_ + Vector3{ 0.0f, 0.0f, -30.0f });

	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	auto levelLoader = std::make_unique<LevelLoader>();
	levelObjectManager_ = std::make_unique<LevelObjectManager>();
	levelObjectManager_->Initialize(*levelLoader->LoadLevel("Stage1.json"), "Stage1.gltf");
	player_->SetLevelObjectManager(levelObjectManager_.get());
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
	// デルタタイムの取得
	const float deltaTime = dxCommon_->GetFPSCounter().GetDeltaTime();

	// デバッグカメラの更新
	UpdateDebug();

	// --- ポーズトグル（ESCキーでON/OFF） ---
	if (input_->TriggerKey(DIK_ESCAPE))
	{
		if (isDebugCamera_) return; // デバッグカメラ中はポーズ無効

		if (gameState_ == GameState::Playing)
		{
			isPaused_ = true; // ポーズ状態にする
			gameState_ = GameState::Paused;
			Input::GetInstance()->SetLockCursor(false);
			ShowCursor(true);
		}
		else if (gameState_ == GameState::Paused)
		{
			isPaused_ = false; // ポーズ解除
			gameState_ = GameState::Playing;
			Input::GetInstance()->SetLockCursor(true);
			ShowCursor(false);
		}
	}

	// カットシーン中の処理
	if (gameState_ == GameState::CutScene)
	{
		// カットシーン更新のみ
		bool finished = UpdateIntroCutscene(deltaTime);

		// 画的に必要な更新だけ許可
		skyBox_->Update();
		fadeController_->Update(deltaTime);
		scarecrow_->Update();
		itemManager_->Update(player_.get(), deltaTime);

		if (finished)
		{
			gameState_ = GameState::Playing;
			introDone_ = true;
			/*Input::GetInstance()->SetLockCursor(true);
			ShowCursor(false);*/
		}
		return; // ここで早期リターン → 以降のプレイ処理を止める
	}

	switch (gameState_)
	{
	case GameState::Playing:
		player_->Update(deltaTime);
		skyBox_->Update();
		scarecrow_->Update();
		crosshair_->Update();
		itemManager_->Update(player_.get(), deltaTime);
		break;
	case GameState::Paused:
		break;
	case GameState::Result:
		break;
	default:
		break;
	}

	levelObjectManager_->Update();

	// 衝突マネージャの更新
	collisionManager_->Update();
	CheckAllCollisions();
}


/// -------------------------------------------------------------
///				　		3Dオブジェクトの描画
/// -------------------------------------------------------------
void GamePlayScene::Draw3DObjects()
{
#pragma region スカイボックスの描画

	// スカイボックスの共通描画設定
	SkyBoxManager::GetInstance()->SetRenderSetting();

	skyBox_->Draw();

#pragma endregion


#pragma region オブジェクト3Dの描画

	//terrein_->Draw();

	if (gameState_ != GameState::CutScene)
	{
		player_->Draw();
		scarecrow_->Draw();
		itemManager_->Draw();
	}

	levelObjectManager_->Draw();

#pragma endregion


#pragma region アニメーションモデルの描画

#pragma endregion


#ifdef _DEBUG
	// 衝突判定を行うオブジェクトの描画
	collisionManager_->Draw();

	// FPSカメラの描画
	//fpsCamera_->DrawDebugCamera();

	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 50.0f, { 0.25f, 0.25f, 0.25f,1.0f });

#endif // _DEBUG
}


/// -------------------------------------------------------------
///				　		2Dスプライトの描画
/// -------------------------------------------------------------
void GamePlayScene::Draw2DSprites()
{
#pragma region スプライトの描画                    

	// 背景用の共通描画設定（後面）
	SpriteManager::GetInstance()->SetRenderSetting_Background();

#pragma endregion


#pragma region UIの描画

	// UI用の共通描画設定
	SpriteManager::GetInstance()->SetRenderSetting_UI();

	// カットシーン中はクロスヘア非表示
	if (gameState_ != GameState::CutScene) {
		crosshair_->Draw();
	}
	fadeController_->Draw();

#pragma endregion
}


/// -------------------------------------------------------------
///				　			　 終了処理
/// -------------------------------------------------------------
void GamePlayScene::Finalize()
{
}


/// -------------------------------------------------------------
///				　			ImGui描画処理
/// -------------------------------------------------------------
void GamePlayScene::DrawImGui()
{
	// ライト
	LightManager::GetInstance()->DrawImGui();

	player_->DrawImGui();

	if (ImGui::Begin("Intro Cutscene")) {

		// --- 再生制御 ---
		if (ImGui::Button("Play / Restart Intro")) {
			RestartIntroCutscene();
		}
		ImGui::SameLine();
		ImGui::Checkbox("Loop while Editing", &introLoop_);
		ImGui::SameLine();
		ImGui::Checkbox("Snap Start Yaw", &forceSnapFirstYaw_);

		ImGui::SliderFloat("Smooth Time (yaw)", &introSmoothTime_, 0.2f, 1.2f, "%.2f");

		// 再生の現在時刻を編集（プレビュー用）
		ImGui::SliderFloat("Time", &introTime_, 0.0f, std::max(0.001f, introLength_), "%.2f");
		// 長さの確認
		ImGui::Text("Length: %.2f sec", introLength_);

		ImGui::Separator();

		// --- カメラキー編集 ---
		if (ImGui::TreeNode("Camera Keys")) {
			for (int i = 0; i < (int)introKeys_.size(); ++i) {
				ImGui::PushID(i);
				ImGui::DragFloat("time", &introKeys_[i].time, 0.01f, 0.0f, 999.0f, "%.2f");
				ImGui::DragFloat3("pos", &introKeys_[i].position.x, 0.1f);
				ImGui::DragFloat3("look", &introKeys_[i].lookAt.x, 0.1f);
				if (ImGui::Button("Remove") && introKeys_.size() > 2) {
					introKeys_.erase(introKeys_.begin() + i);
					// 再生長さを再計算
					introLength_ = introKeys_.back().time;
					ImGui::PopID();
					break;
				}
				ImGui::Separator();
				ImGui::PopID();
			}
			if (ImGui::Button("Add Key (duplicate last)")) {
				auto last = introKeys_.back();
				last.time += 1.0f;
				introKeys_.push_back(last);
				introLength_ = introKeys_.back().time;
			}
			if (ImGui::Button("Sort Keys by time")) {
				std::sort(introKeys_.begin(), introKeys_.end(),
					[](auto& a, auto& b) { return a.time < b.time; });
				introLength_ = introKeys_.back().time;
			}
			ImGui::TreePop();
		}

		ImGui::Separator();

		// --- Yawキー編集 ---
		if (ImGui::TreeNode("Yaw Keys (deg)")) {
			for (int i = 0; i < (int)introYawKeys_.size(); ++i) {
				ImGui::PushID(1000 + i);
				ImGui::DragFloat("time", &introYawKeys_[i].time, 0.01f, 0.0f, 999.0f, "%.2f");
				ImGui::DragFloat("deg", &introYawKeys_[i].deg, 0.1f, -180.0f, 180.0f, "%.1f");
				if (ImGui::Button("Remove##yaw") && introYawKeys_.size() > 1) {
					introYawKeys_.erase(introYawKeys_.begin() + i);
					ImGui::PopID();
					break;
				}
				ImGui::Separator();
				ImGui::PopID();
			}
			if (ImGui::Button("Add Yaw Key (duplicate last)")) {
				auto last = introYawKeys_.back();
				last.time += 1.0f;
				introYawKeys_.push_back(last);
			}
			if (ImGui::Button("Sort Yaw Keys by time")) {
				std::sort(introYawKeys_.begin(), introYawKeys_.end(),
					[](auto& a, auto& b) { return a.time < b.time; });
			}
			ImGui::TreePop();
		}

		ImGui::Separator();

		// その場でプレビュー反映（編集しながら絵を確認できる）
		if (ImGui::Button("Preview Here (no restart)")) {
			// その場の introTime_ で UpdateIntroCutscene(0) を1回呼ぶ
			// deltaTime=0 でも補間結果は出せるよう、Update側は dt=0 を許容している前提
			UpdateIntroCutscene(0.0f);
		}
	}
	ImGui::End();
}

/// -------------------------------------------------------------
///				　			Debug用更新処理
/// -------------------------------------------------------------
void GamePlayScene::UpdateDebug()
{
#ifdef _DEBUG
	if (input_->TriggerKey(DIK_F12))
	{
		if (isPaused_) return; // ポーズ中はデバッグカメラ切り替えを無効化

		Object3DCommon::GetInstance()->SetDebugCamera(!Object3DCommon::GetInstance()->GetDebugCamera());
		Wireframe::GetInstance()->SetDebugCamera(!Wireframe::GetInstance()->GetDebugCamera());
		//ParticleManager::GetInstance()->SetDebugCamera(!ParticleManager::GetInstance()->GetDebugCamera());
		skyBox_->SetDebugCamera(!skyBox_->GetDebugCamera());
		player_->SetDebugCamera(!player_->IsDebugCamera());
		isDebugCamera_ = !isDebugCamera_;
		Input::GetInstance()->SetLockCursor(!isDebugCamera_);
		ShowCursor(isDebugCamera_);// 表示・非表示も連動（オプション）
	}
#endif // _DEBUG
}

/// -------------------------------------------------------------
///				　			イントロカットシーン
/// -------------------------------------------------------------
void GamePlayScene::StartIntroCutscene()
{
	// --- Yaw キー ---
	introYawKeys_.clear();
	introYawKeys_.push_back({ 0.0f,  10.0f });
	introYawKeys_.push_back({ 2.5f,  -50.0f });
	introYawKeys_.push_back({ 5.0f, -100.0f });
	introYawKeys_.push_back({ 7.0f,  -140.0f });
	introYawKeys_.push_back({ 9.0f,   -20.0f });

	// --- Camera キー　---
	introKeys_.clear();
	introKeys_.push_back({ 0.0f, { -55, 18,  45 }, {  0, -0.5f,  0 } });  // 左奥上空
	introKeys_.push_back({ 3.0f, { 35, 16,  35 }, {  0, -0.5f,  0 } });  // 右手前方向へ
	introKeys_.push_back({ 6.0f, { 20, 10,   0 }, {  0, 0.5f,  0 } }); // スタート地点上空
	introKeys_.push_back({ 10.0f, {  -10,  3,  10 }, {  0, 1.8f,  0 } }); // 徐々に地上へ

	introLength_ = introKeys_.back().time;  // 長さセット
	introTime_ = 0.0f;
}

/// -------------------------------------------------------------
///				　		イントロカットシーン更新
/// -------------------------------------------------------------
bool GamePlayScene::UpdateIntroCutscene(float dt)
{
	introTime_ += dt;
	float t = (introTime_ < introLength_) ? introTime_ : introLength_;

	// 区間を探す
	int i = 0;
	while (i + 1 < (int)introKeys_.size() && introKeys_[i + 1].time < t) ++i;
	int i1 = std::max(0, i - 1);
	int i2 = i;
	int i3 = std::min((int)introKeys_.size() - 1, i + 1);
	int i4 = std::min((int)introKeys_.size() - 1, i + 2);

	float segT0 = introKeys_[i2].time;
	float segT1 = introKeys_[i3].time;
	float u = (segT1 > segT0) ? (t - segT0) / (segT1 - segT0) : 0.0f;
	u = clamp01(u);

	Vector3 pos = CatmullRom(introKeys_[i1].position, introKeys_[i2].position,
		introKeys_[i3].position, introKeys_[i4].position, u);

	// 接線（少し先の位置）
	float uAhead = std::min(1.0f, u + 0.02f);
	Vector3 posAhead = CatmullRom(introKeys_[i1].position, introKeys_[i2].position,
		introKeys_[i3].position, introKeys_[i4].position, uAhead);
	Vector3 dir = Vector3::Normalize(posAhead - pos);
	float pathYaw = std::atan2(dir.x, dir.z); // +Z前提

	float yawOffsetRad = SampleYawDeg(t) * (std::numbers::pi_v<float> / 180.0f);
	float targetYaw = pathYaw + yawOffsetRad;

	// 初回だけ進行方向にスナップ（“変な向きから開始”対策）
	if (forceSnapFirstYaw_ && introTime_ == dt) {
		introYawRad_ = pathYaw + (SampleYawDeg(0.0f) * (std::numbers::pi_v<float> / 180.0f));
		introYawVel_ = 0.0f;
	}

	// スムーズ時間をImGuiから可変
	introYawRad_ = SmoothDampAngle(introYawRad_, targetYaw, introYawVel_, introSmoothTime_, dt);

	auto* cam = Object3DCommon::GetInstance()->GetDefaultCamera();
	cam->SetTranslate(pos);
	cam->SetRotate({ 0.0f, introYawRad_, 0.0f });
	cam->Update();

	// ループ再生対応：終端に達したら巻き戻す or 終了
	if (introTime_ >= introLength_) {
		if (introLoop_) {
			introTime_ = 0.0f;
			// ループ頭でも向きが暴れないよう速度リセット
			introYawVel_ = 0.0f;
			return false; // 継続
		}
		return true; // 終了（Playingへ）
	}
	return false;
}

float GamePlayScene::SampleYawDeg(float t) const
{
	if (introYawKeys_.empty()) return 0.0f;
	if (t <= introYawKeys_.front().time) return introYawKeys_.front().deg;
	if (t >= introYawKeys_.back().time)  return introYawKeys_.back().deg;
	for (size_t i = 0; i + 1 < introYawKeys_.size(); ++i) {
		const auto& a = introYawKeys_[i];
		const auto& b = introYawKeys_[i + 1];
		if (t >= a.time && t <= b.time) {
			float u = (t - a.time) / (b.time - a.time);
			return a.deg + (b.deg - a.deg) * u;
		}
	}
	return 0.0f;
}

void GamePlayScene::RestartIntroCutscene()
{
	introTime_ = 0.0f;
	introDone_ = false;
	introYawRad_ = 0.0f;
	introYawVel_ = 0.0f;

	gameState_ = GameState::CutScene;
	Input::GetInstance()->SetLockCursor(true);
	ShowCursor(false);
}

/// -------------------------------------------------------------
///				　			衝突判定と応答
/// -------------------------------------------------------------
void GamePlayScene::CheckAllCollisions()
{
	// 衝突マネージャのリセット
	collisionManager_->Reset();

	// レベルオブジェクトのコライダーを登録
	for (auto& uptr : levelObjectManager_->GetWorldColliders())
	{
		collisionManager_->AddCollider(uptr.get());
	}

	// コライダーをリストに登録
	collisionManager_->AddCollider(player_.get()); // プレイヤー

	collisionManager_->AddCollider(scarecrow_.get()); // 案山子

	// アイテムのコライダーを登録
	itemManager_->RegisterColliders(collisionManager_.get());

	// 衝突判定と応答
	collisionManager_->CheckAllCollisions();
}

