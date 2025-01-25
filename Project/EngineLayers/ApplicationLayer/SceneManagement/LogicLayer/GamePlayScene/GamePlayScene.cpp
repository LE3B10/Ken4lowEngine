#include "GamePlayScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include <Input.h>
#include <ParameterManager.h>
#include <ParticleManager.h>

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void GamePlayScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();
	textureManager = TextureManager::GetInstance();
	particleManager = ParticleManager::GetInstance();

	// テクスチャのパスをリストで管理
	texturePaths_ = {
		"Resources/uvChecker.png",
		//"Resources/monsterBall.png",
	};

	/// ---------- TextureManagerの初期化 ----------///
	for (auto& texture : texturePaths_)
	{
		textureManager->LoadTexture(texture);
	}

	/// ---------- Spriteの初期化 ---------- ///
	for (uint32_t i = 0; i < 1; i++)
	{
		sprites_.push_back(std::make_unique<Sprite>());
		sprites_[i]->Initialize(texturePaths_[i % 2]);
		sprites_[i]->SetPosition(Vector2(100.0f * i, 100.0f * i));
	}

	/// ---------- Object3Dの初期化 ---------- ///
	object3DCommon_ = std::make_unique<Object3DCommon>();

	// .objのパスをリストで管理
	objectFiles = {
		"axis.obj",
		"multiMaterial.obj",
		"multiMesh.obj",
		"plane.obj",
		"cube.gltf",
		"sphere.gltf",
	};

	std::vector<Vector3> initialPositions = {
		{ -2.0f, 2.0f, 10.0f},    // axis.obj の座標
		{ 5.0f, 1.75f, 10.0f},    // multiMaterial.obj の座標
		{ -2.0f, -3.0f, 10.0f},   // multiMesh.obj の座標
		{ 5.0f, -3.0f, 10.0f},    // plane.obj の座標
		{ -2.0f, 0.0f, 10.0f},    // cube.gltf の座標
		{ 2.0f, 0.0f, 10.0f},     // sphere.gltf の座標
	};

	/// ---------- カメラ初期化処理 ---------- ///
	camera_ = std::make_unique<Camera>();
	camera_->SetRotate({ 0.3f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,10.0f,-20.0f });
	object3DCommon_->SetDefaultCamera(camera_.get());

	// 各オブジェクトを初期化し、座標を設定
	for (uint32_t i = 0; i < objectFiles.size(); ++i)
	{
		auto object = std::make_unique<Object3D>();
		object->Initialize(object3DCommon_.get(), objectFiles[i]);
		object->SetTranslate(initialPositions[i]);
		objects3D_.push_back(std::move(object));
	}

	/// ---------- サウンドの初期化 ---------- ///
	const char* fileName = "Resources/Sounds/Get-Ready.wav";
	wavLoader_ = std::make_unique<WavLoader>();
	wavLoader_->StreamAudioAsync(fileName, 0.5f, 1.0f, false);

	textureManager->LoadTexture("Resources/uvChecker.png");
	particleManager->Initialize(dxCommon_, camera_.get());
	particleManager->CreateParticleGroup("fire", "Resources/uvChecker.png");

	fireEmitter = std::make_unique<ParticleEmitter>(particleManager, "fire");
	fireEmitter->SetPosition({ 0.0f,0.0f,20.0f }); // 射出する位置を設定
	fireEmitter->SetEmissionRate(1200.0f);		  // 1秒間に20このパーティクルを射出
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
	// 3Dオブジェクトの更新処理
	for (const auto& object3D : objects3D_)
	{
		object3D->Update();
	}

	// カメラの更新処理
	camera_->Update();

	// スプライトの更新処理
	for (auto& sprite : sprites_)
	{
		sprite->Update();
	}

	// パーティクルエミッターの更新
	fireEmitter->Update(1.0f / 60.0f); // フレーム時間を渡す
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void GamePlayScene::Draw()
{
	// 3Dオブジェクトデータ設定
	for (const auto& object3D : objects3D_)
	{
		object3D->Draw();
	}

	///*-----スプライトの描画設定と描画-----*/
	for (auto& sprite : sprites_)
	{
		sprite->Draw();
	}
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
	ImGui::Begin("Test Window");

	for (uint32_t i = 0; i < objects3D_.size(); ++i)
	{
		ImGui::PushID(i); // オブジェクトごとにIDを区別
		if (ImGui::TreeNode(("Object3D " + std::to_string(i)).c_str()))
		{
			objects3D_[i]->DrawImGui();
			ImGui::TreePop();
		}
		ImGui::PopID(); // IDをリセット
	}

	for (uint32_t i = 0; i < sprites_.size(); i++)
	{
		ImGui::PushID(i); // スプライトごとに異なるIDを設定
		if (ImGui::TreeNode(("Sprite" + std::to_string(i)).c_str()))
		{
			Vector2 position = sprites_[i]->GetPosition();
			ImGui::DragFloat2("Position", &position.x, 1.0f);
			sprites_[i]->SetPosition(position);

			float rotation = sprites_[i]->GetRotation();
			ImGui::SliderAngle("Rotation", &rotation);
			sprites_[i]->SetRotation(rotation);

			Vector2 size = sprites_[i]->GetSize();
			ImGui::DragFloat2("Size", &size.x, 1.0f);
			sprites_[i]->SetSize(size);

			ImGui::TreePop();
		}
		ImGui::PopID(); // IDを元に戻す
	}

	if (ImGui::CollapsingHeader("ParticleEmitter Settings")) {
		Vector3 position = fireEmitter->GetPosition();
		if (ImGui::DragFloat3("Emitter Position", &position.x, 0.1f)) {
			fireEmitter->SetPosition(position); // 射出位置を更新
		}

		float rate = fireEmitter->GetEmissionRate();
		if (ImGui::DragFloat("Emission Rate", &rate, 1.0f, 1.0f, 100.0f)) {
			fireEmitter->SetEmissionRate(rate); // 射出レートを更新
		}
	}

	ImGui::End();

	camera_->DrawImGui();
}
