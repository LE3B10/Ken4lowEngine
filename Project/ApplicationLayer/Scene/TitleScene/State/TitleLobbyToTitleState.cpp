#include "TitleLobbyToTitleState.h"
#include "TitleScene.h"
#include <LinearInterpolation.h>
#include "TitleAttractState.h"

void TitleLobbyToTitleState::Enter(TitleScene* scene)
{
	scene->SetState(TitleScene::State::ToTitle);
}

void TitleLobbyToTitleState::Update(TitleScene* scene, float deltaTime)
{
	// 今のシーン状態を取得
	using State = TitleScene::State;
	State state = scene->GetState();

	// TitleScene 内部状態への参照／ポインタを取得
	auto& orbitState = scene->GetOrbitState();
	auto& logoUI = scene->GetLogoUI();
	auto& timers = scene->GetTimers();
	auto& poseFrom = scene->GetPoseFrom();
	auto& poseTo = scene->GetPoseTo();

	Camera* camera = scene->GetCamera();

	timers.time += deltaTime;
	float t = std::clamp(timers.time / timers.duration, 0.0f, 1.0f);
	float te = EaseInOutCubic(t); // 好みでカーブ変更可

	// 位置・角度を補間（角度は最短回転で）
	Vector3 p;
	p.x = Lerp(poseFrom.position.x, poseTo.position.x, te);
	p.y = Lerp(poseFrom.position.y, poseTo.position.y, te);
	p.z = Lerp(poseFrom.position.z, poseTo.position.z, te);
	float yaw = LerpAngle(poseFrom.yaw, poseTo.yaw, te);
	float pitch = Lerp(poseFrom.pitch, poseTo.pitch, te);

	camera->SetTranslate(p);
	camera->SetRotate({ pitch, yaw, 0.0f });
	camera->Update();
	orbitState.lastYaw = yaw; orbitState.lastPitch = pitch;

	// 補間完了→タイトルのオービットへ
	if (t >= 1.0f)
	{
		timers.idle = timers.state = 0.0f;
		scene->SetState(State::TitleAttract); // タイトルへ遷移

		logoUI.alpha = 0.0f;            // 戻ってもしばらく見えない
		logoUI.scale = 0.9f;
	}

	if (state == State::TitleAttract)
	{
		// ここでステートクラス自体を Lobby にバトンタッチ
		scene->ChangeState(std::make_unique<TitleAttractState>());
	}
}

void TitleLobbyToTitleState::Exit(TitleScene* scene)
{
	// 特に無し
	(void)scene; // 未使用パラメータ回避
}
