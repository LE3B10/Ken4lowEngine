#include "TitleLobbyState.h"
#include "TitleScene.h"
#include "TitleAttractState.h"
#include "TitleLobbyToTitleState.h"
#include "TitleLoadState.h"
#include "SceneManager.h"
#include "Input.h"
#include <LinearInterpolation.h>
#include <TitleFadeOutState.h>

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

void TitleLobbyState::Enter(TitleScene* scene)
{
	scene->SetState(TitleScene::State::LobbyIdle);
}

void TitleLobbyState::Update(TitleScene* scene, float deltaTime)
{
	// 今のシーン状態を取得
	using State = TitleScene::State;
	State state = scene->GetState();

	// TitleScene 内部状態への参照／ポインタを取得
	auto& orbitState = scene->GetOrbitState();
	auto& logoUI = scene->GetLogoUI();
	auto& timers = scene->GetTimers();
	auto& lobbySwing = scene->GetLobbySwing();
	auto& poseFrom = scene->GetPoseFrom();
	auto& poseTo = scene->GetPoseTo();
	auto& battleButtonUI = scene->GetBattleButtonUI();

	Camera* camera = scene->GetCamera();
	Input* input = scene->GetInput();

	Vector2 mp = input->GetMousePosition();

	// ボタン矩形（アンカー対応）
	const float minX = battleButtonUI.position.x - battleButtonUI.size.x * battleButtonUI.anchor.x;
	const float minY = battleButtonUI.position.y - battleButtonUI.size.y * battleButtonUI.anchor.y;
	const float maxX = minX + battleButtonUI.size.x;
	const float maxY = minY + battleButtonUI.size.y;
	const bool inBtn = (mp.x >= minX && mp.x <= maxX && mp.y >= minY && mp.y <= maxY);

	// ----- クリック確定ルール -----
	// ボタン内で押し始めたら「押下中」フラグを立てる
	if (input->TriggerMouse(0) && inBtn)
	{
		battleButtonUI.isPressing = true;
	}
	// ボタンを押している間は「押下演出」を出す
	const bool mouseHeld = input->PushMouse(0);      // ※ Input に実装済み
	const bool mouseUp = input->ReleaseMouse(0);   // ※ 離しの立ち上がり

	// 離した瞬間、押下開始していて かつ いまもボタン内なら確定
	if (mouseUp)
	{
		// 押下開始していて かつ いまもボタン内なら確定
		if (battleButtonUI.isPressing && inBtn)
		{
			// 押下フラグリセット
			battleButtonUI.isPressing = false;

			// ロード状態に遷移
			scene->ChangeState(std::make_unique<TitleFadeOutState>());

			return;
		}
		// 外で離したらキャンセル
		battleButtonUI.isPressing = false;
	}

	// ----- 視覚効果（押し込み・ホバーをスムーズに補間） -----
	// 目標値
	const float pressTarget = (battleButtonUI.isPressing && mouseHeld) ? 1.0f : 0.0f;
	const float hoverTarget = (!pressTarget && inBtn) ? 1.0f : 0.0f;

	// 補間（指数近似っぽく）
	const float s = std::clamp(deltaTime * 12.0f, 0.0f, 1.0f);
	battleButtonUI.pressAnim = Lerp(battleButtonUI.pressAnim, pressTarget, s);
	battleButtonUI.hoverAnim = Lerp(battleButtonUI.hoverAnim, hoverTarget, s);

	// スケール：ホバーで+、押下で-（両方効く）
	const float scale =
		(1.0f + battleButtonUI.scaleHover * battleButtonUI.hoverAnim) - (battleButtonUI.scalePress * battleButtonUI.pressAnim);

	// 位置：押下時だけ下に沈む
	const Vector2 pos = {
		battleButtonUI.position.x,
		battleButtonUI.position.y + battleButtonUI.pressOffsetPx * battleButtonUI.pressAnim
	};

	// 色：押下時は少し暗く（0.85倍）
	const float tint = 1.0f - 0.15f * battleButtonUI.pressAnim;

	// ボタン本体
	if (battleButtonUI.btnSprite)
	{
		battleButtonUI.btnSprite->SetSize({ battleButtonUI.size.x * scale, battleButtonUI.size.y * scale });
		battleButtonUI.btnSprite->SetPosition(pos);
		battleButtonUI.btnSprite->SetColor({ tint, tint, tint, 1.0f });
		battleButtonUI.btnSprite->Update();
	}

	// 影：押下すると「距離が縮む」＝影のオフセットを減らす
	if (battleButtonUI.btnShadow)
	{
		const float shadowOffset = Lerp(6.0f, 2.0f, battleButtonUI.pressAnim); // 未押下→押下で 6px→2px
		battleButtonUI.btnShadow->SetSize({ battleButtonUI.size.x * (scale + 0.02f), battleButtonUI.size.y * (scale + 0.02f) });
		battleButtonUI.btnShadow->SetPosition({ battleButtonUI.position.x, battleButtonUI.position.y + shadowOffset });
		battleButtonUI.btnShadow->SetColor({ 0, 0, 0, 0.35f + 0.1f * battleButtonUI.hoverAnim }); // ホバーで少し濃く
		battleButtonUI.btnShadow->Update();
	}

	// --- キーボード/パッド開始 ---
	if ((input->TriggerKey(DIK_RETURN) || input->TriggerButton(XButtons.A)))
	{
		// 直接シーン変更せず、ロードステートへ
		scene->ChangeState(std::make_unique<TitleLoadState>());
		return;
	}

	// 無操作タイマー更新（何かキーでリセット）
	timers.idle += deltaTime;
	if (input->TriggerMouse(0) || input->TriggerKey(DIK_RETURN) || input->TriggerButton(XButtons.A))
	{
		timers.idle = 0.0f;
	}

	// --- カメラの水平スイング（左右のみ／上下固定） ---
	if (camera)
	{
		lobbySwing.phase += deltaTime * lobbySwing.speed;
		const float theta = lobbySwing.baseTheta + std::sin(lobbySwing.phase) * lobbySwing.amplitude;

		// 位置：lookAt 周りの円弧（y は固定）
		const float x = lobbySwing.lookAt.x + lobbySwing.radius * std::sin(theta);
		const float z = lobbySwing.lookAt.z + lobbySwing.radius * std::cos(theta);
		const float y = lobbySwing.height;
		camera->SetTranslate({ x, y, z });

		// 向き：常に中心を見る（yaw=θ+π、pitchは基準のまま）
		const float yaw = theta + std::numbers::pi_v<float>;
		camera->SetRotate({ lobbySwing.basePitch, yaw, 0.0f });
		camera->Update();
		orbitState.lastYaw = yaw; orbitState.lastPitch = lobbySwing.basePitch;
	}

	// 規定時間無操作なら「タイトルへ戻る補間」を開始
	if (timers.idle >= timers.returnSeconds && camera)
	{
		// 戻り先：現在のオービット角での位置（必ず中心を見る）
		Vector3 orbitPos{
			orbitState.center.x + orbitState.radius * std::sin(orbitState.angle),
			orbitState.center.y,
			orbitState.center.z + orbitState.radius * std::cos(orbitState.angle)
		};
		float toYaw = 0.0f, toPitch = 0.0f;
		YawPitchLookAt(orbitPos, orbitState.center, toYaw, toPitch);

		// 現在姿勢 → タイトル姿勢 のスナップショット
		poseFrom = { camera->GetTranslate(), orbitState.lastYaw, orbitState.lastPitch };
		poseTo = { orbitPos, toYaw, toPitch };
		timers.time = 0.0f;
		timers.state = 0.0f;
		timers.inputCooldownLeft = timers.afterReturnCooldown;   // 戻った直後の誤爆防止
		logoUI.showLeft = logoUI.showDelay;         // 戻り後の出現ディレイを仕込む

		scene->SetState(State::ToTitle);
		if (state == State::ToTitle) { scene->ChangeState(std::make_unique<TitleLobbyToTitleState>()); }
		return;
	}

	if (battleButtonUI.btnSprite) battleButtonUI.btnSprite->Update();
}

void TitleLobbyState::Exit(TitleScene* scene)
{
	// 特に何もしない
	(void)scene; // 未使用引数対策
}
