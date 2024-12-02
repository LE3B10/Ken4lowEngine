//#include "GamePlayScene.h"
//#include <Camera.h>
//#include <Sprite.h>
//#include <TextureManager.h>
//#include <Object3D.h>
//#include <Object3DCommon.h>
//#include <WavLoader.h>
//#include <DirectXCommon.h>
//
///// -------------------------------------------------------------
/////				　			　初期化処理
///// -------------------------------------------------------------
//void GamePlayScene::Initialize()
//{
//	dxcommon_ = DirectXCommon::GetInstance();
//
//	textureManager = TextureManager::GetInstance();
//
//	// テクスチャのパスをリストで管理
//	texturePaths_ = {
//		"Resources/uvChecker.png",
//		//"Resources/monsterBall.png",
//	};
//
//	/// ---------- TextureManagerの初期化 ----------///
//	for (auto& texture : texturePaths_)
//	{
//		textureManager->LoadTexture(texture);
//	}
//
//	/// ---------- Spriteの初期化 ---------- ///
//	for (uint32_t i = 0; i < 1; i++)
//	{
//		sprites_.push_back(std::make_unique<Sprite>());
//		sprites_[i]->Initialize(texturePaths_[i % 2]);
//		sprites_[i]->SetPosition(Vector2(100.0f * i, 100.0f * i));
//	}
//
//	/// ---------- Object3Dの初期化 ---------- ///
//	object3DCommon_ = std::make_unique<Object3DCommon>();
//
//	// .objのパスをリストで管理
//	objectFiles = {
//		"axis.obj",
//		"multiMaterial.obj",
//		"multiMesh.obj",
//		"plane.obj",
//		//"Skydome.obj",
//	};
//
//	std::vector<Vector3> initialPositions = {
//	{ -1.0f, 1.0f, 0.0f},    // axis.obj の座標
//	{ 4.0f, 0.75f, 0.0f},    // multiMaterial.obj の座標
//	{ -1.0f, -2.0f, 0.0f},    // multiMesh.obj の座標
//	{ 4.0f, -2.0f, 0.0f},    // plane.obj の座標
//	//{ 0.0f, 0.0f, 0.0f},    // skydome.obj の座標
//	};
//
//	/// ---------- カメラ初期化処理 ---------- ///
//	camera_ = std::make_unique<Camera>();
//	camera_->SetRotate({ 0.0f,0.0f,0.0f });
//	camera_->SetTranslate({ 0.0f,0.0f,-15.0f });
//	object3DCommon_->SetDefaultCamera(camera_.get());
//
//	// 各オブジェクトを初期化し、座標を設定
//	for (uint32_t i = 0; i < objectFiles.size(); ++i)
//	{
//		auto object = std::make_unique<Object3D>();
//		object->Initialize(object3DCommon_.get(), objectFiles[i]);
//		object->SetTranslate(initialPositions[i]);
//		objects3D_.push_back(std::move(object));
//	}
//
//	/// ---------- サウンドの初期化 ---------- ///
//	const char* fileName = "Resources/Sounds/Get-Ready.wav";
//	wavLoader_ = std::make_unique<WavLoader>();
//	wavLoader_->StreamAudioAsync(fileName, 0.5f, 1.0f, false);
//}
//
//
///// -------------------------------------------------------------
/////				　			　 更新処理
///// -------------------------------------------------------------
//void GamePlayScene::Update()
//{
//	// 3Dオブジェクトの更新処理
//	for (const auto& object3D : objects3D_)
//	{
//		object3D->Update();
//	}
//
//	camera_->Update();
//
//	// スプライトの更新処理
//	for (auto& sprite : sprites_)
//	{
//		sprite->Update();
//	}
//}
//
//
///// -------------------------------------------------------------
/////				　			　 描画処理
///// -------------------------------------------------------------
//void GamePlayScene::Draw()
//{
//	// 3Dオブジェクトデータ設定
//	for (const auto& object3D : objects3D_)
//	{
//		object3D->SetObject3DBufferData(dxCommon->GetCommandList());
//		object3D->DrawCall(dxCommon->GetCommandList());
//	}
//
//	///*-----スプライトの描画設定と描画-----*/
//	for (auto& sprite : sprites_)
//	{
//		sprite->SetSpriteBufferData(dxCommon->GetCommandList());
//		sprite->DrawCall(dxCommon->GetCommandList());
//	}
//}
//
//
///// -------------------------------------------------------------
/////				　			　 終了処理
///// -------------------------------------------------------------
//void GamePlayScene::Finalize()
//{
//
//}
