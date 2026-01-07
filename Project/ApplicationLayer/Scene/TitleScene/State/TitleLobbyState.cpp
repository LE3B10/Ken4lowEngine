#include "TitleLobbyState.h"
#include "TitleScene.h"
#include "TitleAttractState.h"
#include "TitleLobbyToTitleState.h"
#include "TitleLoadState.h"
#include "SceneManager.h"
#include "Input.h"
#include <LinearInterpolation.h>

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
	using State = TitleScene::State;
	State state = scene->GetState();

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
	if (input->TriggerMouse(0) && inBtn)
	{
		battleButtonUI.isPressing = true;
	}

	const bool mouseHeld = input->PushMouse(0);
	const bool mouseUp = input->ReleaseMouse(0);

	if (mouseUp)
	{
		if (battleButtonUI.isPressing && inBtn)
		{
			battleButtonUI.isPressing = false;

			// 即シーン変更要求
			SceneManager::GetInstance()->ChangeScene("StageSelectScene", true);

			return;
		}

		battleButtonUI.isPressing = false;
	}

	// ----- 視覚効果 -----
	const float pressTarget = (battleButtonUI.isPressing && mouseHeld) ? 1.0f : 0.0f;
	const float hoverTarget = (!pressTarget && inBtn) ? 1.0f : 0.0f;

	const float s = std::clamp(deltaTime * 12.0f, 0.0f, 1.0f);
	battleButtonUI.pressAnim = Lerp(battleButtonUI.pressAnim, pressTarget, s);
	battleButtonUI.hoverAnim = Lerp(battleButtonUI.hoverAnim, hoverTarget, s);

	const float scale =
		(1.0f + battleButtonUI.scaleHover * battleButtonUI.hoverAnim) - (battleButtonUI.scalePress * battleButtonUI.pressAnim);

	const Vector2 pos = {
		battleButtonUI.position.x,
		battleButtonUI.position.y + battleButtonUI.pressOffsetPx * battleButtonUI.pressAnim
	};

	const float tint = 1.0f - 0.15f * battleButtonUI.pressAnim;

	if (battleButtonUI.btnSprite)
	{
		battleButtonUI.btnSprite->SetSize({ battleButtonUI.size.x * scale, battleButtonUI.size.y * scale });
		battleButtonUI.btnSprite->SetPosition(pos);
		battleButtonUI.btnSprite->SetColor({ tint, tint, tint, 1.0f });
		battleButtonUI.btnSprite->Update();
	}

	if (battleButtonUI.btnShadow)
	{
		const float shadowOffset = Lerp(6.0f, 2.0f, battleButtonUI.pressAnim);
		battleButtonUI.btnShadow->SetSize({ battleButtonUI.size.x * (scale + 0.02f), battleButtonUI.size.y * (scale + 0.02f) });
		battleButtonUI.btnShadow->SetPosition({ battleButtonUI.position.x, battleButtonUI.position.y + shadowOffset });
		battleButtonUI.btnShadow->SetColor({ 0, 0, 0, 0.35f + 0.1f * battleButtonUI.hoverAnim });
		battleButtonUI.btnShadow->Update();
	}

	// 無操作タイマー更新
	timers.idle += deltaTime;
	if (input->TriggerMouse(0))
	{
		timers.idle = 0.0f;
	}

	// カメラ更新など（省略：元のままでOK）
	if (camera)
	{
		lobbySwing.phase += deltaTime * lobbySwing.speed;
		const float theta = lobbySwing.baseTheta + std::sin(lobbySwing.phase) * lobbySwing.amplitude;

		const float x = lobbySwing.lookAt.x + lobbySwing.radius * std::sin(theta);
		const float z = lobbySwing.lookAt.z + lobbySwing.radius * std::cos(theta);
		const float y = lobbySwing.height;
		camera->SetTranslate({ x, y, z });

		const float yaw = theta + std::numbers::pi_v<float>;
		camera->SetRotate({ lobbySwing.basePitch, yaw, 0.0f });
		camera->Update();
		orbitState.lastYaw = yaw; orbitState.lastPitch = lobbySwing.basePitch;
	}

	// 規定時間無操作なら戻る（元のままでOK）
	if (timers.idle >= timers.returnSeconds && camera)
	{
		Vector3 orbitPos{
			orbitState.center.x + orbitState.radius * std::sin(orbitState.angle),
			orbitState.center.y,
			orbitState.center.z + orbitState.radius * std::cos(orbitState.angle)
		};
		float toYaw = 0.0f, toPitch = 0.0f;
		YawPitchLookAt(orbitPos, orbitState.center, toYaw, toPitch);

		poseFrom = { camera->GetTranslate(), orbitState.lastYaw, orbitState.lastPitch };
		poseTo = { orbitPos, toYaw, toPitch };
		timers.time = 0.0f;
		timers.state = 0.0f;
		timers.inputCooldownLeft = timers.afterReturnCooldown;
		logoUI.showLeft = logoUI.showDelay;

		scene->SetState(State::ToTitle);
		if (state == State::ToTitle) { scene->ChangeState(std::make_unique<TitleLobbyToTitleState>()); }
		return;
	}
}
void TitleLobbyState::Exit(TitleScene* scene)
{
	// 特に何もしない
	(void)scene; // 未使用引数対策
}
