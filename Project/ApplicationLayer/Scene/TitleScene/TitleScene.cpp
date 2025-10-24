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
#include <AudioManager.h>

/// -------------------------------------------------------------
///				　			　補助関数
/// -------------------------------------------------------------
static inline void YawPitchLookAt(const Vector3& from, const Vector3& to, float& outYaw, float& outPitch)
{
	const float dx = to.x - from.x; // Xは横方向
	const float dy = to.y - from.y; // Yは高さ
	const float dz = to.z - from.z;	// Zは奥行き
	outYaw = std::atan2(dx, dz);                    // 水平角（Y軸まわり）
	const float distXZ = std::sqrt(dx * dx + dz * dz); // XZ平面距離
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
	timers_.state = timers_.idle = 0.0f;
	timers_.inputCooldownLeft = 0.0f;

	logoUI_.scale = 0.9f;   // 少し小さく出して拡大
	logoUI_.showLeft = logoUI_.showDelay;
	logoUI_.exitLeft = 0.0f;

	// カメラの生成と初期化
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();
	if (camera_)
	{
		camera_->SetTranslate({ orbitState_.center.x, orbitState_.center.y, orbitState_.center.z - orbitState_.radius });
		orbitState_.lastPitch = -0.10f; orbitState_.lastYaw = std::numbers::pi_v<float>;
		camera_->SetRotate({ orbitState_.lastPitch, orbitState_.lastYaw, 0.0f });
		camera_->Update();
	}

	// タイトルロゴ（透明PNG推奨、例: Resources/UI/logo_rittai_sensen.png）
	logoSprite_ = std::make_unique<Sprite>();
	logoSprite_->Initialize("logo_rittai_sensen.png");
	logoUI_.baseSize = logoSprite_->GetSize();
	logoUI_.baseSize *= 0.7f; // 元画像が大きい場合は適宜縮小
	logoSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	logoSprite_->SetPosition({ 1280.0f * 0.5f, 180.0f });

	// バトルボタンUI
	battleButtonUI_.btnSprite = std::make_unique<Sprite>();
	battleButtonUI_.btnSprite->Initialize("btn_battle.png");
	battleButtonUI_.btnSprite->SetAnchorPoint(battleButtonUI_.anchor);
	battleButtonUI_.btnSprite->SetPosition(battleButtonUI_.position);
	battleButtonUI_.btnSprite->SetSize(battleButtonUI_.size);

	// 影スプライトも作成
	battleButtonUI_.btnShadow = std::make_unique<Sprite>();
	battleButtonUI_.btnShadow->Initialize("btn_battle.png");
	battleButtonUI_.btnShadow->SetAnchorPoint(battleButtonUI_.anchor);
	battleButtonUI_.btnShadow->SetPosition({ battleButtonUI_.position.x, battleButtonUI_.position.y + 6.0f }); // 影は少し下
	battleButtonUI_.btnShadow->SetSize({ battleButtonUI_.size.x * 1.02f, battleButtonUI_.size.y * 1.02f }); // わずかに大きく
	battleButtonUI_.btnShadow->SetColor({ 0, 0, 0, 0.35f }); // 半透明の黒

	// --- HUDアイコン/バー（仮アセット名でOK。手持ちの白テクでも可） ---
	auto make = [&](std::unique_ptr<Sprite>& s, const std::string& path, Vector2 pos, Vector2 anchor) {
		s = std::make_unique<Sprite>();
		s->Initialize("icon/" + path);
		s->SetAnchorPoint(anchor);
		s->SetPosition(pos);
		};

	make(iconGear_, "ui_gear.png", { 1230, 60 }, { 0.5f,0.5f }); // 右上（1280x720想定）
	make(iconCoin_, "ui_coin.png", { 1080, 60 }, { 0.5f,0.5f }); // 右上・コイン
	make(btnShop_, "btn_shop.png", { 180, 640 }, { 0.5f,0.5f }); // 左下
	make(xpBack_, "ui_xp_back.png", { 40, 60 }, { 0.0f,0.5f });  // 左上（左端基準）
	xpBackBaseSize_ = xpBack_->GetSize();                        // 元サイズ保持
	make(xpFill_, "ui_xp_fill.png", { 40, 60 }, { 0.0f,0.5f });  // 同位置（幅だけ後で変える）

	clickHintUI_.hintSprite = std::make_unique<Sprite>();
	clickHintUI_.hintSprite->Initialize("ui_click_hint.png");
	clickHintUI_.hintSprite->SetAnchorPoint({ 0.5f, 0.0f });      // 中央上
	// ここは今の 1/10 スケール指定のままでOK
	clickHintUI_.hintSprite->SetPosition({ 1280.0f * 0.5f + clickHintUI_.offset.x, 180.0f + clickHintUI_.offset.y });
	clickHintUI_.hintSprite->SetSize({ 153.6f, 102.4f });       // 元画像が1536x1024pxなので1/10スケール
	clickHintUI_.baseSize = clickHintUI_.hintSprite->GetSize(); // 基準サイズを保存

	terrain_ = std::make_unique<Object3D>();
	terrain_->Initialize("lobby03.gltf");
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void TitleScene::Update()
{
	UpdateDebug();
	if (terrain_) terrain_->Update();

	if (isDebugCamera_) return; // デバッグカメラ中はポーズ無効

	const float dt = ImGui::GetIO().DeltaTime;
	timers_.state += dt;
	if (timers_.inputCooldownLeft > 0.0f) { timers_.inputCooldownLeft = std::max(0.0f, timers_.inputCooldownLeft - dt); }

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
			orbitState_.center.x + orbitState_.radius * std::sin(orbitState_.angle),
			orbitState_.center.y,
			orbitState_.center.z + orbitState_.radius * std::cos(orbitState_.angle)
		};
		float toYaw = 0.0f, toPitch = 0.0f;
		YawPitchLookAt(orbitPos, orbitState_.center, toYaw, toPitch);

		poseFrom_ = { camera_->GetTranslate(), orbitState_.lastYaw, orbitState_.lastPitch };
		poseTo_ = { orbitPos, toYaw, toPitch };
		timers_.time = 0.0f;
		timers_.state = 0.0f;
		state_ = State::ToTitle;
		timers_.inputCooldownLeft = timers_.afterReturnCooldown;   // 戻ってすぐの誤爆を防止
	}
#endif // _DEBUG
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
	if (state_ == State::TransitionToLobby || state_ == State::LobbyIdle)
	{
		if (battleButtonUI_.btnShadow) { battleButtonUI_.btnShadow->Draw(); } // ← 影を先に
		if (battleButtonUI_.btnSprite) { battleButtonUI_.btnSprite->Draw(); } // ← ボタン本体

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
	ImGui::Text("Idle: %.1fs / Return: %.0fs", timers_.idle, timers_.returnSeconds);
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
		orbitState_.angle += orbitState_.speed * dt;

		const float x = orbitState_.center.x + orbitState_.radius * sin(orbitState_.angle);
		const float z = orbitState_.center.z + orbitState_.radius * cos(orbitState_.angle);
		camera_->SetTranslate({ x, orbitState_.center.y, z });

		// 中心を見る
		YawPitchLookAt({ x, orbitState_.center.y, z }, orbitState_.center, orbitState_.lastYaw, orbitState_.lastPitch);
		camera_->SetRotate({ orbitState_.lastPitch, orbitState_.lastYaw, 0.0f });
		camera_->Update();
	}

	// ディレイ消化
	if (logoUI_.showLeft > 0.0f) {
		logoUI_.showLeft = std::max(0.0f, logoUI_.showLeft - dt);
	}

	// --- ロゴのフェード＆スケール（0.8秒でふわっと出す） ---
	{
		float t = (logoUI_.showLeft > 0.0f) ? 0.0f
			: std::clamp((timers_.state - logoUI_.showDelay) / 0.8f, 0.0f, 1.0f);
		float te = EaseOutCubic(t);
		logoUI_.alpha = te;
		logoUI_.scale = 0.9f + 0.1f * te;
	}

	// 整数ピクセルにスナップ（ドット絵の滲み防止）
	auto snap = [](float v) { return std::floor(v + 0.5f); };

	bool canAcceptInput =
		(timers_.state >= (timers_.minTitleSeconds + logoUI_.showDelay)) &&
		(timers_.inputCooldownLeft <= 0.0f);

	clickHintUI_.isVisible = canAcceptInput;   // 表示条件

	// === クリックヒント：追従・アニメ・ヒットテスト ===
	bool clickHintCommit = false;
	if (clickHintUI_.isVisible && clickHintUI_.hintSprite && logoSprite_) {

		clickHintUI_.phase += dt;

		// ロゴのすぐ下（アンカー：ヒント=中央上）
		const Vector2 logoPos = logoSprite_->GetPosition();
		const Vector2 logoSz = { logoUI_.baseSize.x * logoUI_.scale, logoUI_.baseSize.y * logoUI_.scale };
		Vector2 basePos = { logoPos.x, logoPos.y + (logoSz.y * 0.5f) + clickHintUI_.marginY };

		// アニメ成分（点滅・上下ゆれ・脈動）
		const float blink = clickHintUI_.blinkMin + (1.0f - clickHintUI_.blinkMin) * (0.5f * (sinf(clickHintUI_.phase * 2.2f) + 1.0f));
		const float wobble = sinf(clickHintUI_.phase * 4.0f) * clickHintUI_.wobblePx;
		const float pulse = 1.0f + clickHintUI_.pulseMag * sinf(clickHintUI_.phase * 2.0f);

		// いまの“見た目”でヒットテスト（前フレームの押し/ホバー値を反映）
		const float scaleNow =
			(pulse + clickHintUI_.scaleHover * clickHintUI_.hoverAnim) -
			(clickHintUI_.scalePress * clickHintUI_.pressAnim);
		Vector2 posNow = { basePos.x, basePos.y + wobble + clickHintUI_.offsetPressY * clickHintUI_.pressAnim };
		posNow.y = std::min(posNow.y, 720.0f - 60.0f); // 画面下クランプ（1280x720基準）

		const Vector2 sizeNow = { clickHintUI_.baseSize.x * scaleNow, clickHintUI_.baseSize.y * scaleNow };
		const float minX = posNow.x - sizeNow.x * 0.5f; // アンカー(0.5,0.0)
		const float minY = posNow.y;                    // 上端
		const float maxX = minX + sizeNow.x;
		const float maxY = minY + sizeNow.y;

		// マウス
		const Vector2 mp = input_->GetMousePosition();
		const bool inHint = (mp.x >= minX && mp.x <= maxX && mp.y >= minY && mp.y <= maxY);

		// 入力：押し始めは内側、離したのも内側なら確定
		if (input_->TriggerMouse(0) && inHint) clickHintUI_.isPressing = true;
		const bool mouseHeld = input_->PushMouse(0);
		const bool mouseUp = input_->ReleaseMouse(0);
		if (mouseUp) {
			if (clickHintUI_.isPressing && inHint) clickHintCommit = true; // ← 確定
			clickHintUI_.isPressing = false;
		}

		// 目標値→アニメ補間
		const float pressTarget = (clickHintUI_.isPressing && mouseHeld) ? 1.0f : 0.0f;
		const float hoverTarget = (!pressTarget && inHint) ? 1.0f : 0.0f;
		const float s = std::clamp(dt * 12.0f, 0.0f, 1.0f);
		clickHintUI_.pressAnim = Lerp(clickHintUI_.pressAnim, pressTarget, s);
		clickHintUI_.hoverAnim = Lerp(clickHintUI_.hoverAnim, hoverTarget, s);

		//“更新後”の見た目で描画セット（次フレームの ③ で使われる）
		const float scaleDraw =
			(pulse + clickHintUI_.scaleHover * clickHintUI_.hoverAnim) -
			(clickHintUI_.scalePress * clickHintUI_.pressAnim);
		Vector2 posDraw = {
			basePos.x,
			basePos.y + wobble + clickHintUI_.offsetPressY * clickHintUI_.pressAnim
		};
		posDraw.y = std::min(posDraw.y, 720.0f - 60.0f);

		clickHintUI_.hintSprite->SetPosition({ snap(posDraw.x), snap(posDraw.y) });
		clickHintUI_.hintSprite->SetSize({ clickHintUI_.baseSize.x * scaleDraw, clickHintUI_.baseSize.y * scaleDraw });
		clickHintUI_.hintSprite->SetColor({ 1.0f, 0.95f, 0.25f, blink });
		clickHintUI_.hintSprite->Update();
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
		poseFrom_ = { camera->GetTranslate(), orbitState_.lastYaw, orbitState_.lastPitch };
		float toYaw = 0.0f, toPitch = 0.0f;
		YawPitchLookAt(lobbySwing_.cameraPosition, lobbySwing_.lookAt, toYaw, toPitch);
		poseTo_ = { lobbySwing_.cameraPosition, toYaw, toPitch };
		timers_.time = 0.0f; // 遷移時間リセット
		timers_.state = 0.0f; // タイマーリセット

		state_ = State::TransitionToLobby; // ロビーへの遷移へ
		logoUI_.exitLeft = logoUI_.exitFade;          // Exitフェード開始
	}

	logoSprite_->Update();
}

/// -------------------------------------------------------------
///				　		ロビーへの遷移の更新
/// -------------------------------------------------------------
void TitleScene::UpdateTransitionToLobby(float dt)
{
	timers_.time += dt;
	float t = std::clamp(timers_.time / timers_.duration, 0.0f, 1.0f);
	float te = EaseInOutCubic(t);                // ← “滑らか”補間（お好みで変更可）

	Vector3 p;

	// 位置・向き補間
	p.x = Lerp(poseFrom_.position.x, poseTo_.position.x, te);
	p.y = Lerp(poseFrom_.position.y, poseTo_.position.y, te);
	p.z = Lerp(poseFrom_.position.z, poseTo_.position.z, te);
	const float yaw = LerpAngle(poseFrom_.yaw, poseTo_.yaw, te);
	const float pitch = Lerp(poseFrom_.pitch, poseTo_.pitch, te);

	// カメラ更新
	camera_->SetTranslate(p);
	camera_->SetRotate({ pitch, yaw, 0.0f });
	camera_->Update();
	orbitState_.lastYaw = yaw; orbitState_.lastPitch = pitch;

	if (t >= 1.0f)
	{
		state_ = State::LobbyIdle;
		timers_.state = timers_.idle = 0.0f;

		// lookAt からの水平半径と高さ
		const Vector3& P = poseTo_.position;
		lobbySwing_.radius = std::hypot(P.x - lobbySwing_.lookAt.x, P.z - lobbySwing_.lookAt.z);
		lobbySwing_.height = P.y;

		// 基準角（θ）とピッチを記録：yaw = θ + π なので θ = yaw - π
		lobbySwing_.baseTheta = poseTo_.yaw - std::numbers::pi_v<float>;
		lobbySwing_.basePitch = poseTo_.pitch;
		lobbySwing_.phase = 0.0f;
	}

	// ロゴの退出フェード（あれば減衰）
	if (logoUI_.exitLeft > 0.0f)
	{
		logoUI_.exitLeft = std::max(0.0f, logoUI_.exitLeft - dt);
		logoUI_.alpha = (logoUI_.exitLeft / logoUI_.exitFade);  // 線形でOK
	}
}

/// -------------------------------------------------------------
///				　		ロビーでの待機の更新
/// -------------------------------------------------------------
void TitleScene::UpdateLobbyIdle(float dt)
{
	Vector2 mp = input_->GetMousePosition();

	// ボタン矩形（アンカー対応）
	const float minX = battleButtonUI_.position.x - battleButtonUI_.size.x * battleButtonUI_.anchor.x;
	const float minY = battleButtonUI_.position.y - battleButtonUI_.size.y * battleButtonUI_.anchor.y;
	const float maxX = minX + battleButtonUI_.size.x;
	const float maxY = minY + battleButtonUI_.size.y;
	const bool inBtn = (mp.x >= minX && mp.x <= maxX && mp.y >= minY && mp.y <= maxY);

	// ----- クリック確定ルール -----
	// 1) ボタン内で押し始めたら「押下中」フラグを立てる
	if (input_->TriggerMouse(0) && inBtn)
	{
		battleButtonUI_.isPressing = true;
	}
	// 2) ボタンを押している間は「押下演出」を出す
	const bool mouseHeld = input_->PushMouse(0);      // ※ Input に実装済み
	const bool mouseUp = input_->ReleaseMouse(0);   // ※ 離しの立ち上がり

	// 3) 離した瞬間、押下開始していて かつ いまもボタン内なら確定
	if (mouseUp)
	{
		if (battleButtonUI_.isPressing && inBtn && !fadeController_->IsFading())
		{
			fadeController_->SetFadeMode(FadeController::FadeMode::Checkerboard);
			fadeController_->SetOnComplete([this] {if (sceneManager_) { sceneManager_->ChangeScene("StageSelectScene"); }});
			fadeController_->SetCheckerDelay(0.012f);
			fadeController_->StartFadeOut(0.32f); // 暗転
			battleButtonUI_.isPressing = false;
			return;
		}
		// 外で離したらキャンセル
		battleButtonUI_.isPressing = false;
	}

	// ----- 視覚効果（押し込み・ホバーをスムーズに補間） -----
	// 目標値
	const float pressTarget = (battleButtonUI_.isPressing && mouseHeld) ? 1.0f : 0.0f;
	const float hoverTarget = (!pressTarget && inBtn) ? 1.0f : 0.0f;

	// 補間（指数近似っぽく）
	const float s = std::clamp(dt * 12.0f, 0.0f, 1.0f);
	battleButtonUI_.pressAnim = Lerp(battleButtonUI_.pressAnim, pressTarget, s);
	battleButtonUI_.hoverAnim = Lerp(battleButtonUI_.hoverAnim, hoverTarget, s);

	// スケール：ホバーで+、押下で-（両方効く）
	const float scale =
		(1.0f + battleButtonUI_.scaleHover * battleButtonUI_.hoverAnim) - (battleButtonUI_.scalePress * battleButtonUI_.pressAnim);

	// 位置：押下時だけ下に沈む
	const Vector2 pos = {
		battleButtonUI_.position.x,
		battleButtonUI_.position.y + battleButtonUI_.pressOffsetPx * battleButtonUI_.pressAnim
	};

	// 色：押下時は少し暗く（0.85倍）
	const float tint = 1.0f - 0.15f * battleButtonUI_.pressAnim;

	// ボタン本体
	if (battleButtonUI_.btnSprite)
	{
		battleButtonUI_.btnSprite->SetSize({ battleButtonUI_.size.x * scale, battleButtonUI_.size.y * scale });
		battleButtonUI_.btnSprite->SetPosition(pos);
		battleButtonUI_.btnSprite->SetColor({ tint, tint, tint, 1.0f });
		battleButtonUI_.btnSprite->Update();
	}

	// 影：押下すると「距離が縮む」＝影のオフセットを減らす
	if (battleButtonUI_.btnShadow)
	{
		const float shadowOffset = Lerp(6.0f, 2.0f, battleButtonUI_.pressAnim); // 未押下→押下で 6px→2px
		battleButtonUI_.btnShadow->SetSize({ battleButtonUI_.size.x * (scale + 0.02f), battleButtonUI_.size.y * (scale + 0.02f) });
		battleButtonUI_.btnShadow->SetPosition({ battleButtonUI_.position.x, battleButtonUI_.position.y + shadowOffset });
		battleButtonUI_.btnShadow->SetColor({ 0, 0, 0, 0.35f + 0.1f * battleButtonUI_.hoverAnim }); // ホバーで少し濃く
		battleButtonUI_.btnShadow->Update();
	}

	// --- キーボード/パッド開始 ---
	if ((input_->TriggerKey(DIK_RETURN) || input_->TriggerButton(XButtons.A)) && !fadeController_->IsFading())
	{
		fadeController_->SetFadeMode(FadeController::FadeMode::Checkerboard);
		fadeController_->SetOnComplete([this] {if (sceneManager_) { sceneManager_->ChangeScene("StageSelectScene"); }});
		fadeController_->SetCheckerDelay(0.012f);
		fadeController_->StartFadeOut(0.32f); // 暗転
		return;
	}

	// 無操作タイマー更新（何かキーでリセット）
	timers_.idle += dt;
	if (input_->TriggerMouse(0) || input_->TriggerKey(DIK_RETURN) || input_->TriggerButton(XButtons.A))
	{
		timers_.idle = 0.0f;
	}

	// --- カメラの水平スイング（左右のみ／上下固定） ---
	if (camera_)
	{
		lobbySwing_.phase += dt * lobbySwing_.speed;
		const float theta = lobbySwing_.baseTheta + std::sin(lobbySwing_.phase) * lobbySwing_.amplitude;
		// 位置：lookAt 周りの円弧（y は固定）
		const float x = lobbySwing_.lookAt.x + lobbySwing_.radius * std::sin(theta);
		const float z = lobbySwing_.lookAt.z + lobbySwing_.radius * std::cos(theta);
		const float y = lobbySwing_.height;
		camera_->SetTranslate({ x, y, z });
		// 向き：常に中心を見る（yaw=θ+π、pitchは基準のまま）
		const float yaw = theta + std::numbers::pi_v<float>;
		camera_->SetRotate({ lobbySwing_.basePitch, yaw, 0.0f });
		camera_->Update();
		orbitState_.lastYaw = yaw; orbitState_.lastPitch = lobbySwing_.basePitch;
	}

	// 規定時間無操作なら「タイトルへ戻る補間」を開始
	if (timers_.idle >= timers_.returnSeconds && camera_)
	{
		// 戻り先：現在のオービット角での位置（必ず中心を見る）
		Vector3 orbitPos{
			orbitState_.center.x + orbitState_.radius * std::sin(orbitState_.angle),
			orbitState_.center.y,
			orbitState_.center.z + orbitState_.radius * std::cos(orbitState_.angle)
		};
		float toYaw = 0.0f, toPitch = 0.0f;
		YawPitchLookAt(orbitPos, orbitState_.center, toYaw, toPitch);

		// 現在姿勢 → タイトル姿勢 のスナップショット
		poseFrom_ = { camera_->GetTranslate(), orbitState_.lastYaw, orbitState_.lastPitch };
		poseTo_ = { orbitPos, toYaw, toPitch };
		timers_.time = 0.0f;
		timers_.state = 0.0f;
		state_ = State::ToTitle;   // ← 補間専用ステートへ
		timers_.inputCooldownLeft = timers_.afterReturnCooldown;   // 戻った直後の誤爆防止
		logoUI_.showLeft = logoUI_.showDelay;         // 戻り後の出現ディレイを仕込む
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

	if (battleButtonUI_.btnSprite) battleButtonUI_.btnSprite->Update();
}

/// -------------------------------------------------------------
///				　	タイトルへ戻る補間の更新
/// -------------------------------------------------------------
void TitleScene::UpdateToTitle(float dt)
{
	timers_.time += dt;
	float t = std::clamp(timers_.time / timers_.duration, 0.0f, 1.0f);
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
	orbitState_.lastYaw = yaw; orbitState_.lastPitch = pitch;

	// 補間完了→タイトルのオービットへ
	if (t >= 1.0f)
	{
		timers_.idle = timers_.state = 0.0f;
		state_ = State::TitleAttract;

		logoUI_.alpha = 0.0f;            // 戻ってもしばらく見えない
		logoUI_.scale = 0.9f;
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
		Object3DCommon::GetInstance()->SetDebugCamera(!Object3DCommon::GetInstance()->GetDebugCamera());
		Wireframe::GetInstance()->SetDebugCamera(!Wireframe::GetInstance()->GetDebugCamera());
		isDebugCamera_ = !isDebugCamera_;
	}
#endif // _DEBUG
}