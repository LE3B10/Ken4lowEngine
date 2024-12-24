#include "GamePlayScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void GamePlayScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	textureManager = TextureManager::GetInstance();
	input_ = Input::GetInstance();

	/// ---------- Object3Dの初期化 ---------- ///
	object3DCommon_ = std::make_unique<Object3DCommon>();


	// トランスフォームの初期化
	transform_ = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,-1.0f,0.0f} };


	/// ---------- カメラ初期化処理 ---------- ///
	camera_ = std::make_unique<Camera>();
	camera_->SetRotate({ 0.0f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,0.0f,-15.0f });
	object3DCommon_->SetDefaultCamera(camera_.get());

	// 各オブジェクトを初期化し、座標を設定
	playerObject_ = std::make_unique<Object3D>();
	playerObject_->Initialize(object3DCommon_.get(), "plane.obj");
	playerObject_->SetTranslate(transform_.translate);

	/// ---------- サウンドの初期化 ---------- ///
	const char* fileName = "Resources/Sounds/Get-Ready.wav";
	wavLoader_ = std::make_unique<WavLoader>();
	wavLoader_->StreamAudioAsync(fileName, 0.2f, 1.0f, false);
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
	// レーン変更
	if (input_->TriggerKey(DIK_RIGHT) && currentLaneIndex_ < 2)
	{
		currentLaneIndex_++;
	}

	if (input_->TriggerKey(DIK_LEFT) && currentLaneIndex_ > 0)
	{
		currentLaneIndex_--;
	}

	// 目標位置を取得
	float targetX = lanePositions_[ currentLaneIndex_ ];

	// 線形補間で現在位置を更新
	transform_.translate.x += (targetX - transform_.translate.x) * moveSpeed_;

	// ジャンプ処理
	if (input_->TriggerKey(DIK_UP) && !isJumping_)
	{
		isJumping_ = true;
		jumpHeight_ = -1.0f;
	}

	if (isJumping_)
	{
		jumpHeight_ += jumpVelocity_;  // ジャンプの上昇
		jumpVelocity_ += gravity_;    // 重力を加算

		// 地面に到達したら停止
		if (jumpHeight_ <= -1.0f)
		{    
			jumpHeight_ = -1.0f;
			jumpVelocity_ = 0.2f;     // 初速度をリセット
			isJumping_ = false;
		}
	}

	transform_.translate.y = jumpHeight_;  // Y座標を更新


	// 回転処理
	if (input_->TriggerKey(DIK_DOWN) && !isRotating_)
	{
		isRotating_ = true;
		rotationAngle_ = 0.0f;
		rotationCount_ = 0;
	}

	if (isRotating_) {
		rotationAngle_ += rotationSpeed_;  // 回転速度分回転
		if (rotationAngle_ >= 180.0f) {    // 1回転したらカウント
			rotationAngle_ -= 180.0f;
			rotationCount_++;
		}

		if (rotationCount_ >= 2) {         // 2回転で終了
			isRotating_ = false;
			rotationAngle_ = 0.0f;
		}
	}

	transform_.rotate.x = rotationAngle_;  // Y軸回転角を更新



	// 位置を更新
	playerObject_->SetTranslate(transform_.translate);
	playerObject_->SetRotate(transform_.rotate);

	// プレイヤーオブジェクトの更新
	playerObject_->Update();

	// カメラの更新
	camera_->Update();
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void GamePlayScene::Draw()
{
	// 3Dオブジェクトデータ設定
	playerObject_->Draw();
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

	//for (uint32_t i = 0; i < objects3D_.size(); ++i)
	//{
	//	ImGui::PushID(i); // オブジェクトごとにIDを区別
	//	if (ImGui::TreeNode(("Object3D " + std::to_string(i)).c_str()))
	//	{
	//		objects3D_[i]->DrawImGui();
	//		ImGui::TreePop();
	//	}
	//	ImGui::PopID(); // IDをリセット
	//}

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

	ImGui::End();

	camera_->DrawImGui();
}
