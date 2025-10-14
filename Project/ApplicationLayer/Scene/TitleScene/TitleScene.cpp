#define NOMINMAX
#include "TitleScene.h"
#include <DirectXCommon.h>
#include <SpriteManager.h>
#include <Object3DCommon.h>
#include <ImGuiManager.h>
#include "SceneManager.h"
#include <CollisionUtility.h>
#include "Input.h"
#include <Wireframe.h>
#include <LinearInterpolation.h>
#include <SkyBoxManager.h>
#include <AudioManager.h>

/// -------------------------------------------------------------
///				　			　補助関数
/// -------------------------------------------------------------
static inline void YawPitchLookAt(const Vector3& from, const Vector3& to, float& outYaw, float& outPitch)
{
	const float dx = to.x - from.x;
	const float dy = to.y - from.y;
	const float dz = to.z - from.z;
	outYaw = std::atan2(dx, dz);                    // 水平角（Y軸まわり）
	const float distXZ = std::sqrt(dx * dx + dz * dz);
	outPitch = std::atan2(dy, distXZ);                // 上下角（X軸まわり）
}


/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void TitleScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	// フェードコントローラーの初期化
	fadeController_ = std::make_unique<FadeController>();
	fadeController_->Initialize(static_cast<float>(dxCommon_->GetSwapChainDesc().Width), static_cast<float>(dxCommon_->GetSwapChainDesc().Height), "white.png");
	fadeController_->SetFadeMode(FadeController::FadeMode::Checkerboard);
	fadeController_->SetGrid(14, 8);
	fadeController_->SetCheckerDelay(0.012f);
	fadeController_->StartFadeIn(0.32f); // 暗転明け

	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize("SkyBox/skybox.dds");

	state_ = State::TitleAttract; // 最初はタイトルアトラクトモード
	stateTimer_ = idleTimer_ = 0.0f;
	inputCooldownLeft_ = 0.0f;
	logoAlpha_ = 0.0f;   // タイトル入りで透明→フェードイン
	logoScale_ = 0.9f;   // 少し小さく出して拡大
	logoShowDelayLeft_ = logoShowDelay_;
	logoExitFadeLeft_ = 0.0f;

	// カメラの生成と初期化
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();
	if (camera_)
	{
		camera_->SetTranslate({ orbitCenter_.x, orbitCenter_.y, orbitCenter_.z - orbitRadius_ });
		lastPitch_ = -0.10f; lastYaw_ = std::numbers::pi_v<float>;
		camera_->SetRotate({ lastPitch_, lastYaw_, 0.0f });
		camera_->Update();
	}

	// タイトルロゴ（透明PNG推奨、例: Resources/UI/logo_rittai_sensen.png）
	logoSprite_ = std::make_unique<Sprite>();
	logoSprite_->Initialize("logo_rittai_sensen.png");
	logoBaseSize_ = logoSprite_->GetSize();
	logoBaseSize_ *= 0.7f; // 元画像が大きい場合は適宜縮小
	logoSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	logoSprite_->SetPosition({ 1280.0f * 0.5f, 180.0f });

	// これまで vector に入れていた btn_battle.png を専用ポインタで保持
	btnBattle_ = std::make_unique<Sprite>();
	btnBattle_->Initialize("btn_battle.png");
	btnBattle_->SetAnchorPoint(battleBtnAnchor_);
	btnBattle_->SetPosition(battleBtnPos_);
	btnBattle_->SetSize(battleBtnSize_);

	btnBattleShadow_ = std::make_unique<Sprite>();
	btnBattleShadow_->Initialize("btn_battle.png");
	btnBattleShadow_->SetAnchorPoint(battleBtnAnchor_);
	btnBattleShadow_->SetPosition({ battleBtnPos_.x, battleBtnPos_.y + 6.0f }); // 影は少し下
	btnBattleShadow_->SetSize({ battleBtnSize_.x * 1.02f, battleBtnSize_.y * 1.02f }); // わずかに大きく
	btnBattleShadow_->SetColor({ 0, 0, 0, 0.35f }); // 半透明の黒

	// --- HUDアイコン/バー（仮アセット名でOK。手持ちの白テクでも可） ---
	auto make = [&](std::unique_ptr<Sprite>& s, const std::string& path, Vector2 pos, Vector2 anchor) {
		s = std::make_unique<Sprite>();
		s->Initialize("icon/" + path);
		s->SetAnchorPoint(anchor);
		s->SetPosition(pos);
		};

	make(iconGear_, "ui_gear.png", { 1230, 60 }, { 0.5f,0.5f });     // 右上（1280x720想定）
	make(iconCoin_, "ui_coin.png", { 1080, 60 }, { 0.5f,0.5f });     // 右上・コイン
	make(btnShop_, "btn_shop.png", { 180, 640 }, { 0.5f,0.5f });    // 左下
	make(xpBack_, "ui_xp_back.png", { 40, 60 }, { 0.0f,0.5f });    // 左上（左端基準）
	xpBackBaseSize_ = xpBack_->GetSize();                        // 元サイズ保持
	make(xpFill_, "ui_xp_fill.png", { 40, 60 }, { 0.0f,0.5f });    // 同位置（幅だけ後で変える）

	clickHint_ = std::make_unique<Sprite>();
	clickHint_->Initialize("ui_click_hint.png");
	clickHint_->SetAnchorPoint({ 0.5f, 0.0f });      // 中央上
	// ここは今の 1/10 スケール指定のままでOK
	clickHint_->SetPosition({ 1280.0f * 0.5f + clickHintOffset_.x, 180.0f + clickHintOffset_.y });
	clickHint_->SetSize({ 153.6f, 102.4f });       // ←あなたが入れた値
	clickHintBaseSize_ = clickHint_->GetSize();    // ←基準サイズを保存

	// 背景用の3Dオブジェクトを生成
	object3D_ = std::make_unique<Object3D>();
	object3D_->Initialize("sphere.gltf"); // 適当な3Dモデルを指定

	terrain_ = std::make_unique<Object3D>();
	terrain_->Initialize("lobby03.gltf");
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void TitleScene::Update()
{
	UpdateDebug();
	if (object3D_) object3D_->Update();
	if (terrain_) terrain_->Update();

	if (isDebugCamera_) return; // デバッグカメラ中はポーズ無効

	const float dt = ImGui::GetIO().DeltaTime;
	stateTimer_ += dt;
	if (inputCooldownLeft_ > 0.0f) { inputCooldownLeft_ = std::max(0.0f, inputCooldownLeft_ - dt); }

	switch (state_)
	{
	case TitleScene::State::TitleAttract: // タイトルアトラクトモード
		UpdateTitleAttract(dt);
		break;

	case TitleScene::State::TransitionToLobby: // ロビーへの遷移
		UpdateTransitionToLobby(dt);
		break;

	case TitleScene::State::LobbyIdle: // ロビーでの待機
		UpdateLobbyIdle(dt);
		break;

	case TitleScene::State::ToTitle: // 操作時間が無かったらタイトルへ戻る
		UpdateToTitle(dt);
		break;
	}

	skyBox_->Update();

	fadeController_->Update(dxCommon_->GetFPSCounter().GetDeltaTime());

#ifdef _DEBUG
	if (input_->TriggerKey(DIK_ESCAPE) && state_ != State::ToTitle)
	{
		// いまの姿勢からタイトルのオービットへ戻すスナップショット作成
		Vector3 orbitPos{
			orbitCenter_.x + orbitRadius_ * std::sin(orbitAngle_),
			orbitCenter_.y,
			orbitCenter_.z + orbitRadius_ * std::cos(orbitAngle_)
		};
		float toYaw = 0.0f, toPitch = 0.0f;
		YawPitchLookAt(orbitPos, orbitCenter_, toYaw, toPitch);

		poseFrom_ = { camera_->GetTranslate(), lastYaw_, lastPitch_ };
		poseTo_ = { orbitPos, toYaw, toPitch };
		transTime_ = 0.0f;
		stateTimer_ = 0.0f;
		state_ = State::ToTitle;
		inputCooldownLeft_ = afterReturnCooldown_;   // 戻ってすぐの誤爆を防止
	}
#endif // _DEBUG
}


/// -------------------------------------------------------------
///				　	3Dオブジェクトの描画
/// -------------------------------------------------------------
void TitleScene::Draw3DObjects()
{
#pragma region オブジェクト3Dの描画

	SkyBoxManager::GetInstance()->SetRenderSetting();

	skyBox_->Draw();

	// オブジェクト3D共通描画設定
	Object3DCommon::GetInstance()->SetRenderSetting();

	if (object3D_)	object3D_->Draw();
	if (terrain_) terrain_->Draw();

#pragma endregion

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
	if (state_ == State::TitleAttract || state_ == State::ToTitle || logoExitFadeLeft_ > 0.0f)
	{
		if (logoSprite_)
		{
			logoSprite_->SetColor({ 1,1,1,logoAlpha_ });
			const Vector2 sz = { logoBaseSize_.x * logoScale_, logoBaseSize_.y * logoScale_ };
			logoSprite_->SetSize(sz);
			logoSprite_->Draw();

			if (clickHintVisible_ && clickHint_) { clickHint_->Draw(); }
		}
	}

	// ロビー系（TransitionToLobby と LobbyIdle の間だけロビーUIを表示）
	if (state_ == State::TransitionToLobby || state_ == State::LobbyIdle)
	{
		if (btnBattleShadow_) { btnBattleShadow_->Draw(); } // ← 影を先に
		if (btnBattle_) { btnBattle_->Draw(); } // ← ボタン本体

		// HUD
		if (xpBack_)  xpBack_->Draw();
		if (xpFill_)  xpFill_->Draw();
		if (iconCoin_) iconCoin_->Draw();
		if (btnShop_)  btnShop_->Draw();
		if (iconGear_) iconGear_->Draw();
	}

	// フェードコントローラー
	fadeController_->Draw();

#pragma endregion

}


/// -------------------------------------------------------------
///				　			　 終了処理
/// -------------------------------------------------------------
void TitleScene::Finalize()
{
	AudioManager::GetInstance()->StopBGM();
}


/// -------------------------------------------------------------
///				　		　ImGui描画処理
/// -------------------------------------------------------------
void TitleScene::DrawImGui()
{
	ImGui::Begin("Title Debug");
	const char* stateNames =
		state_ == State::TitleAttract ? "TitleAttract" :
		state_ == State::TransitionToLobby ? "TransitionToLobby" :
		state_ == State::LobbyIdle ? "LobbyIdle" : "ToTitle";
	ImGui::Text("State: %s", stateNames);
	ImGui::Text("Idle: %.1fs / Return: %.0fs", idleTimer_, returnSeconds_);
	ImGui::End();

	if (state_ == State::TransitionToLobby || state_ == State::LobbyIdle) {
		// 半透明・装飾無しのミニHUD
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin("HUD", nullptr,
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

		// 左上：レベル/XP
		ImGui::SetWindowPos(ImVec2(40, 30));
		ImGui::Text("Lv.%d  %d/%d XP", debugLevel_, debugXP_, debugXPNext_);

		// 右上：コイン
		ImGui::SetWindowPos(ImVec2(1100, 30));
		ImGui::Text("%d", debugCoins_);

		ImGui::End();
	}

	LightManager::GetInstance()->DrawImGui();
	object3D_->DrawImGui();
}

/// -------------------------------------------------------------
///				　タイトルアトラクトモードの更新
/// -------------------------------------------------------------
void TitleScene::UpdateTitleAttract(float dt)
{
	// ゆっくりカメラを周回させる
	if (camera_)
	{
		// 角度を進める
		orbitAngle_ += orbitSpeed_ * dt;

		const float x = orbitCenter_.x + orbitRadius_ * sin(orbitAngle_);
		const float z = orbitCenter_.z + orbitRadius_ * cos(orbitAngle_);
		camera_->SetTranslate({ x, orbitCenter_.y, z });

		// 中心を見る
		YawPitchLookAt({ x, orbitCenter_.y, z }, orbitCenter_, lastYaw_, lastPitch_);
		camera_->SetRotate({ lastPitch_, lastYaw_, 0.0f });
		camera_->Update();
	}

	// ディレイ消化
	if (logoShowDelayLeft_ > 0.0f) {
		logoShowDelayLeft_ = std::max(0.0f, logoShowDelayLeft_ - dt);
	}

	// --- ロゴのフェード＆スケール（0.8秒でふわっと出す） ---
	{
		float t = (logoShowDelayLeft_ > 0.0f) ? 0.0f
			: std::clamp((stateTimer_ - logoShowDelay_) / 0.8f, 0.0f, 1.0f);
		float te = EaseOutCubic(t);
		logoAlpha_ = te;
		logoScale_ = 0.9f + 0.1f * te;
	}

	// 整数ピクセルにスナップ（ドット絵の滲み防止）
	auto snap = [](float v) { return std::floor(v + 0.5f); };

	bool canAcceptInput =
		(stateTimer_ >= (minTitleSeconds_ + logoShowDelay_)) &&
		(inputCooldownLeft_ <= 0.0f);

	clickHintVisible_ = canAcceptInput;   // 表示条件

	// === クリックヒント：追従・アニメ・ヒットテスト ===
	bool clickHintCommit = false;
	if (clickHintVisible_ && clickHint_ && logoSprite_) {

		clickHintPhase_ += dt;

		// ロゴのすぐ下（アンカー：ヒント=中央上）
		const Vector2 logoPos = logoSprite_->GetPosition();
		const Vector2 logoSz = { logoBaseSize_.x * logoScale_, logoBaseSize_.y * logoScale_ };
		Vector2 basePos = { logoPos.x, logoPos.y + (logoSz.y * 0.5f) + clickHintMarginY_ };

		// アニメ成分（点滅・上下ゆれ・脈動）
		const float blink = clickHintBlinkMin_ + (1.0f - clickHintBlinkMin_) * (0.5f * (sinf(clickHintPhase_ * 2.2f) + 1.0f));
		const float wobble = sinf(clickHintPhase_ * 4.0f) * clickHintWobblePx_;
		const float pulse = 1.0f + clickHintPulseMag_ * sinf(clickHintPhase_ * 2.0f);

		// いまの“見た目”でヒットテスト（前フレームの押し/ホバー値を反映）
		const float scaleNow =
			(pulse + clickHintScaleHover_ * clickHintHoverAnim_) -
			(clickHintScalePress_ * clickHintPressAnim_);
		Vector2 posNow = { basePos.x, basePos.y + wobble + clickHintOffsetPressY_ * clickHintPressAnim_ };
		posNow.y = std::min(posNow.y, 720.0f - 60.0f); // 画面下クランプ（1280x720基準）

		const Vector2 sizeNow = { clickHintBaseSize_.x * scaleNow, clickHintBaseSize_.y * scaleNow };
		const float minX = posNow.x - sizeNow.x * 0.5f; // アンカー(0.5,0.0)
		const float minY = posNow.y;                    // 上端
		const float maxX = minX + sizeNow.x;
		const float maxY = minY + sizeNow.y;

		// マウス
		const Vector2 mp = input_->GetMousePosition();
		const bool inHint = (mp.x >= minX && mp.x <= maxX && mp.y >= minY && mp.y <= maxY);

		// 入力：押し始めは内側、離したのも内側なら確定
		if (input_->TriggerMouse(0) && inHint) clickHintPressing_ = true;
		const bool mouseHeld = input_->PushMouse(0);
		const bool mouseUp = input_->ReleaseMouse(0);
		if (mouseUp) {
			if (clickHintPressing_ && inHint) clickHintCommit = true; // ← 確定
			clickHintPressing_ = false;
		}

		// 目標値→アニメ補間
		const float pressTarget = (clickHintPressing_ && mouseHeld) ? 1.0f : 0.0f;
		const float hoverTarget = (!pressTarget && inHint) ? 1.0f : 0.0f;
		const float s = std::clamp(dt * 12.0f, 0.0f, 1.0f);
		clickHintPressAnim_ = Lerp(clickHintPressAnim_, pressTarget, s);
		clickHintHoverAnim_ = Lerp(clickHintHoverAnim_, hoverTarget, s);

		//“更新後”の見た目で描画セット（次フレームの ③ で使われる）
		const float scaleDraw =
			(pulse + clickHintScaleHover_ * clickHintHoverAnim_) -
			(clickHintScalePress_ * clickHintPressAnim_);
		Vector2 posDraw = {
			basePos.x,
			basePos.y + wobble + clickHintOffsetPressY_ * clickHintPressAnim_
		};
		posDraw.y = std::min(posDraw.y, 720.0f - 60.0f);

		clickHint_->SetPosition({ snap(posDraw.x), snap(posDraw.y) });
		clickHint_->SetSize({ clickHintBaseSize_.x * scaleDraw, clickHintBaseSize_.y * scaleDraw });
		clickHint_->SetColor({ 1.0f, 0.95f, 0.25f, blink });
		clickHint_->Update();
	}

	// === 入力受付 ===
	if (canAcceptInput && (
		clickHintCommit ||
		input_->TriggerKey(DIK_RETURN) ||          // Enter
		input_->TriggerKey(DIK_SPACE) ||          // Space
		input_->TriggerButton(XButtons.A)		// Aが押されたら
		)) {
		// 逆参照しないように
		auto* camera = camera_ ? camera_ : EnsureCamera();

		// 現在姿勢 -> ロビーの姿勢 へのスナップショットを取得
		poseFrom_ = { camera->GetTranslate(), lastYaw_, lastPitch_ };
		float toYaw = 0.0f, toPitch = 0.0f;
		YawPitchLookAt(lobbyPosition_, lobbyLookAt_, toYaw, toPitch);
		poseTo_ = { lobbyPosition_, toYaw, toPitch };
		transTime_ = 0.0f; // 遷移時間リセット
		stateTimer_ = 0.0f; // タイマーリセット

		state_ = State::TransitionToLobby; // ロビーへの遷移へ
		logoExitFadeLeft_ = logoExitFade_;          // Exitフェード開始
	}

	logoSprite_->Update();
}

/// -------------------------------------------------------------
///				　		ロビーへの遷移の更新
/// -------------------------------------------------------------
void TitleScene::UpdateTransitionToLobby(float dt)
{
	transTime_ += dt;
	float t = std::clamp(transTime_ / transDuration_, 0.0f, 1.0f);
	float te = EaseInOutCubic(t);                // ← “滑らか”補間（お好みで変更可）

	Vector3 p;
	p.x = Lerp(poseFrom_.position.x, poseTo_.position.x, te);
	p.y = Lerp(poseFrom_.position.y, poseTo_.position.y, te);
	p.z = Lerp(poseFrom_.position.z, poseTo_.position.z, te);
	const float yaw = LerpAngle(poseFrom_.yaw, poseTo_.yaw, te);
	const float pitch = Lerp(poseFrom_.pitch, poseTo_.pitch, te);

	camera_->SetTranslate(p);
	camera_->SetRotate({ pitch, yaw, 0.0f });
	camera_->Update();
	lastYaw_ = yaw; lastPitch_ = pitch;

	if (t >= 1.0f)
	{
		state_ = State::LobbyIdle;
		stateTimer_ = idleTimer_ = 0.0f;

		// lookAt からの水平半径と高さ
		const Vector3& P = poseTo_.position;
		lobbyRadius_ = std::hypot(P.x - lobbyLookAt_.x, P.z - lobbyLookAt_.z);
		lobbyHeight_ = P.y;

		// 基準角（θ）とピッチを記録：yaw = θ + π なので θ = yaw - π
		lobbyBaseTheta_ = poseTo_.yaw - std::numbers::pi_v<float>;
		lobbyBasePitch_ = poseTo_.pitch;
		swayPhase_ = 0.0f;
	}

	// ロゴの退出フェード（あれば減衰）
	if (logoExitFadeLeft_ > 0.0f)
	{
		logoExitFadeLeft_ = std::max(0.0f, logoExitFadeLeft_ - dt);
		logoAlpha_ = (logoExitFadeLeft_ / logoExitFade_);  // 線形でOK
	}
}

/// -------------------------------------------------------------
///				　		ロビーでの待機の更新
/// -------------------------------------------------------------
void TitleScene::UpdateLobbyIdle(float dt)
{
	Vector2 mp = input_->GetMousePosition();

	// ボタン矩形（アンカー対応）
	const float minX = battleBtnPos_.x - battleBtnSize_.x * battleBtnAnchor_.x;
	const float minY = battleBtnPos_.y - battleBtnSize_.y * battleBtnAnchor_.y;
	const float maxX = minX + battleBtnSize_.x;
	const float maxY = minY + battleBtnSize_.y;
	const bool inBtn = (mp.x >= minX && mp.x <= maxX && mp.y >= minY && mp.y <= maxY);

	// ----- クリック確定ルール -----
	// 1) ボタン内で押し始めたら「押下中」フラグを立てる
	if (input_->TriggerMouse(0) && inBtn)
	{
		battleBtnPressing_ = true;
	}
	// 2) ボタンを押している間は「押下演出」を出す
	const bool mouseHeld = input_->PushMouse(0);      // ※ Input に実装済み
	const bool mouseUp = input_->ReleaseMouse(0);   // ※ 離しの立ち上がり

	// 3) 離した瞬間、押下開始していて かつ いまもボタン内なら確定
	if (mouseUp)
	{
		if (battleBtnPressing_ && inBtn && !fadeController_->IsFading())
		{
			fadeController_->SetFadeMode(FadeController::FadeMode::Checkerboard);
			fadeController_->SetOnComplete([this] {if (sceneManager_) { sceneManager_->ChangeScene("StageSelectScene"); }});
			fadeController_->StartFadeOut(0.32f); // 暗転
			battleBtnPressing_ = false;
			return;
		}
		// 外で離したらキャンセル
		battleBtnPressing_ = false;
	}

	// ----- 視覚効果（押し込み・ホバーをスムーズに補間） -----
	// 目標値
	const float pressTarget = (battleBtnPressing_ && mouseHeld) ? 1.0f : 0.0f;
	const float hoverTarget = (!pressTarget && inBtn) ? 1.0f : 0.0f;

	// 補間（指数近似っぽく）
	const float s = std::clamp(dt * 12.0f, 0.0f, 1.0f);
	battlePressAnim_ = Lerp(battlePressAnim_, pressTarget, s);
	battleHoverAnim_ = Lerp(battleHoverAnim_, hoverTarget, s);

	// スケール：ホバーで+、押下で-（両方効く）
	const float scale =
		(1.0f + battleScaleHover_ * battleHoverAnim_) - (battleScalePress_ * battlePressAnim_);

	// 位置：押下時だけ下に沈む
	const Vector2 pos = {
		battleBtnPos_.x,
		battleBtnPos_.y + battlePressOffsetPx_ * battlePressAnim_
	};

	// 色：押下時は少し暗く（0.85倍）
	const float tint = 1.0f - 0.15f * battlePressAnim_;

	if (btnBattle_)
	{
		btnBattle_->SetSize({ battleBtnSize_.x * scale, battleBtnSize_.y * scale });
		btnBattle_->SetPosition(pos);
		btnBattle_->SetColor({ tint, tint, tint, 1.0f });
		btnBattle_->Update();
	}

	// 影：押下すると「距離が縮む」＝影のオフセットを減らす
	if (btnBattleShadow_)
	{
		const float shadowOffset = Lerp(6.0f, 2.0f, battlePressAnim_); // 未押下→押下で 6px→2px
		btnBattleShadow_->SetSize({ battleBtnSize_.x * (scale + 0.02f), battleBtnSize_.y * (scale + 0.02f) });
		btnBattleShadow_->SetPosition({ battleBtnPos_.x, battleBtnPos_.y + shadowOffset });
		btnBattleShadow_->SetColor({ 0, 0, 0, 0.35f + 0.1f * battleHoverAnim_ }); // ホバーで少し濃く
		btnBattleShadow_->Update();
	}

	// --- キーボード/パッド開始 ---
	if ((input_->TriggerKey(DIK_RETURN) || input_->TriggerButton(XButtons.A)) && !fadeController_->IsFading())
	{
		fadeController_->SetFadeMode(FadeController::FadeMode::Checkerboard);
		fadeController_->SetOnComplete([this] {if (sceneManager_) { sceneManager_->ChangeScene("StageSelectScene"); }});
		fadeController_->StartFadeOut(0.32f); // 暗転
		return;
	}

	// 無操作タイマー更新（何かキーでリセット）
	idleTimer_ += dt;
	if (input_->TriggerMouse(0) || input_->TriggerKey(DIK_RETURN) || input_->TriggerButton(XButtons.A))
	{
		idleTimer_ = 0.0f;
	}

	// --- カメラの水平スイング（左右のみ／上下固定） ---
	if (camera_)
	{
		swayPhase_ += dt * swaySpeed_;
		const float theta = lobbyBaseTheta_ + std::sin(swayPhase_) * swayAmplitude_;
		// 位置：lookAt 周りの円弧（y は固定）
		const float x = lobbyLookAt_.x + lobbyRadius_ * std::sin(theta);
		const float z = lobbyLookAt_.z + lobbyRadius_ * std::cos(theta);
		const float y = lobbyHeight_;
		camera_->SetTranslate({ x, y, z });
		// 向き：常に中心を見る（yaw=θ+π、pitchは基準のまま）
		const float yaw = theta + std::numbers::pi_v<float>;
		camera_->SetRotate({ lobbyBasePitch_, yaw, 0.0f });
		camera_->Update();
		lastYaw_ = yaw; lastPitch_ = lobbyBasePitch_;
	}

	// 規定時間無操作なら「タイトルへ戻る補間」を開始
	if (idleTimer_ >= returnSeconds_ && camera_)
	{
		// 戻り先：現在のオービット角での位置（必ず中心を見る）
		Vector3 orbitPos{
			orbitCenter_.x + orbitRadius_ * std::sin(orbitAngle_),
			orbitCenter_.y,
			orbitCenter_.z + orbitRadius_ * std::cos(orbitAngle_)
		};
		float toYaw = 0.0f, toPitch = 0.0f;
		YawPitchLookAt(orbitPos, orbitCenter_, toYaw, toPitch);

		// 現在姿勢 → タイトル姿勢 のスナップショット
		poseFrom_ = { camera_->GetTranslate(), lastYaw_, lastPitch_ };
		poseTo_ = { orbitPos, toYaw, toPitch };
		transTime_ = 0.0f;
		stateTimer_ = 0.0f;
		state_ = State::ToTitle;   // ← 補間専用ステートへ
		inputCooldownLeft_ = afterReturnCooldown_;   // 戻った直後の誤爆防止
		logoShowDelayLeft_ = logoShowDelay_;         // 戻り後の出現ディレイを仕込む
		return;
	}

	// アイコン—右上系は48px統一
	if (iconGear_) iconGear_->SetSize({ 48,48 });
	if (iconCoin_) iconCoin_->SetSize({ 48,48 });

	// XPバー—左上 480×20 に固定、fillは幅だけ可変
	if (xpBack_) { xpBack_->SetSize({ 480,20 }); xpBackBaseSize_ = xpBack_->GetSize(); }
	if (xpFill_) { xpFill_->SetSize({ 480,20 }); }

	// 見た目だけなのでUpdate呼ぶ（アニメ用）
	if (xpBack_) xpBack_->Update();
	if (xpFill_) xpFill_->Update();
	if (iconGear_) iconGear_->Update();
	if (iconCoin_) iconCoin_->Update();
	if (btnShop_) btnShop_->Update();

	if (btnBattle_) btnBattle_->Update();
}

/// -------------------------------------------------------------
///				　	タイトルへ戻る補間の更新
/// -------------------------------------------------------------
void TitleScene::UpdateToTitle(float dt)
{
	transTime_ += dt;
	float t = std::clamp(transTime_ / transDuration_, 0.0f, 1.0f);
	float te = EaseInOutCubic(t); // 好みでカーブ変更可

	// 位置・角度を補間（角度は最短回転で）
	Vector3 p;
	p.x = Lerp(poseFrom_.position.x, poseTo_.position.x, te);
	p.y = Lerp(poseFrom_.position.y, poseTo_.position.y, te);
	p.z = Lerp(poseFrom_.position.z, poseTo_.position.z, te);
	float yaw = LerpAngle(poseFrom_.yaw, poseTo_.yaw, te);
	float pitch = Lerp(poseFrom_.pitch, poseTo_.pitch, te);

	camera_->SetTranslate(p);
	camera_->SetRotate({ pitch, yaw, 0.0f });
	camera_->Update();
	lastYaw_ = yaw; lastPitch_ = pitch;

	// 補間完了→タイトルのオービットへ
	if (t >= 1.0f)
	{
		idleTimer_ = stateTimer_ = 0.0f;
		state_ = State::TitleAttract;

		logoAlpha_ = 0.0f;            // 戻ってもしばらく見えない
		logoScale_ = 0.9f;
	}
}

/// -------------------------------------------------------------
///				カメラの確保（なければデフォルトを取得）
/// -------------------------------------------------------------
Camera* TitleScene::EnsureCamera()
{
	if (!camera_) {
		camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();
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
		if (sceneManager_)
		{
			sceneManager_->ChangeScene("PhysicalScene"); // 戻るキーでゲームプレイシーンに戻る
		}
	}

	if (input_->TriggerKey(DIK_F12))
	{
		//if (isPaused_) return; // ポーズ中はデバッグカメラ切り替えを無効化

		Object3DCommon::GetInstance()->SetDebugCamera(!Object3DCommon::GetInstance()->GetDebugCamera());
		Wireframe::GetInstance()->SetDebugCamera(!Wireframe::GetInstance()->GetDebugCamera());
		ParticleManager::GetInstance()->SetDebugCamera(!ParticleManager::GetInstance()->GetDebugCamera());
		/*skyBox_->SetDebugCamera(!skyBox_->GetDebugCamera());
		player_->SetDebugCamera(!player_->IsDebugCamera());*/
		isDebugCamera_ = !isDebugCamera_;
		//Input::GetInstance()->SetLockCursor(isDebugCamera_);
		//ShowCursor(!isDebugCamera_);// 表示・非表示も連動（オプション）
	}
#endif // _DEBUG
}