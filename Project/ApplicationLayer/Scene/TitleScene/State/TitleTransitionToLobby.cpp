#define NOMINMAX
#include "TitleTransitionToLobby.h"
#include "TitleScene.h"
#include "TitleLobbyState.h"
#include <LinearInterpolation.h>

void TitleTransitionToLobby::Enter(TitleScene* scene)
{
	scene->SetState(TitleScene::State::TransitionToLobby);
}

void TitleTransitionToLobby::Update(TitleScene* scene, float deltaTime)
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
	Camera* camera = scene->GetCamera();

	timers.time += deltaTime;
	float t = std::clamp(timers.time / timers.duration, 0.0f, 1.0f);
	float te = EaseInOutCubic(t);                // ← “滑らか”補間（お好みで変更可）

	Vector3 p;

	// 位置・向き補間
	p.x = Lerp(poseFrom.position.x, poseTo.position.x, te);
	p.y = Lerp(poseFrom.position.y, poseTo.position.y, te);
	p.z = Lerp(poseFrom.position.z, poseTo.position.z, te);
	const float yaw = LerpAngle(poseFrom.yaw, poseTo.yaw, te);
	const float pitch = Lerp(poseFrom.pitch, poseTo.pitch, te);

	// カメラ更新
	camera->SetTranslate(p);
	camera->SetRotate({ pitch, yaw, 0.0f });
	camera->Update();
	orbitState.lastYaw = yaw; orbitState.lastPitch = pitch;

	if (t >= 1.0f)
	{
		scene->SetState(State::LobbyIdle);
		timers.state = timers.idle = 0.0f;

		// lookAt からの水平半径と高さ
		const Vector3& P = poseTo.position;
		lobbySwing.radius = std::hypot(P.x - lobbySwing.lookAt.x, P.z - lobbySwing.lookAt.z);
		lobbySwing.height = P.y;

		// 基準角（θ）とピッチを記録：yaw = θ + π なので θ = yaw - π
		lobbySwing.baseTheta = poseTo.yaw - std::numbers::pi_v<float>;
		lobbySwing.basePitch = poseTo.pitch;
		lobbySwing.phase = 0.0f;
	}

	// ロゴの退出フェード（あれば減衰）
	if (logoUI.exitLeft > 0.0f)
	{
		logoUI.exitLeft = std::max(0.0f, logoUI.exitLeft - deltaTime);
		logoUI.alpha = (logoUI.exitLeft / logoUI.exitFade);  // 線形でOK
	}

	if (state == State::LobbyIdle)
	{
		// ロビー待機へ移行
		scene->ChangeState(std::make_unique<TitleLobbyState>());
	}
}

void TitleTransitionToLobby::Exit(TitleScene* scene)
{
	(void)scene; // 未使用引数対策
}
