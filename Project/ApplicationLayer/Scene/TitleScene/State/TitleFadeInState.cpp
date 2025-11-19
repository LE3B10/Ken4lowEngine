#include "TitleFadeInState.h"
#include "TitleScene.h"
#include "TitleAttractState.h"
#include "Camera.h"

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

void TitleFadeInState::Enter(TitleScene* scene)
{
	if (!scene) { return; }

	scene->SetState(TitleScene::State::FadeIn);
	timer_ = 0.0f;

	// 真っ黒からスタート
	scene->SetFadeAlpha(1.0f);
}

void TitleFadeInState::Update(TitleScene* scene, float deltaTime)
{
	using State = TitleScene::State;
	auto& orbitState = scene->GetOrbitState();
	Camera* camera = scene->GetCamera();

	if (!scene) { return; }

	// ゆっくりカメラを周回させる
	if (camera)
	{
		// 角度を進める
		orbitState.angle += orbitState.speed * deltaTime;

		const float x = orbitState.center.x + orbitState.radius * sin(orbitState.angle);
		const float z = orbitState.center.z + orbitState.radius * cos(orbitState.angle);
		camera->SetTranslate({ x, orbitState.center.y, z });

		// 中心を見る
		YawPitchLookAt({ x, orbitState.center.y, z }, orbitState.center, orbitState.lastYaw, orbitState.lastPitch);
		camera->SetRotate({ orbitState.lastPitch, orbitState.lastYaw, 0.0f });
		camera->Update();
	}

	timer_ += deltaTime;

	float t = std::clamp(timer_ / duration_, 0.0f, 1.0f);
	float alpha = 1.0f - t;   // 1 -> 0
	scene->SetFadeAlpha(alpha);

	if (t >= 1.0f)
	{
		// フェードイン完了 → セレクト状態へ
		scene->SetState(State::TitleAttract);
		scene->ChangeState(std::make_unique<TitleAttractState>());
	}
}

void TitleFadeInState::Exit(TitleScene* scene)
{
	(void)scene; // 未使用
}
