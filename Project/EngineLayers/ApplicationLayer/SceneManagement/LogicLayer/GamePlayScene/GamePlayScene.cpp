#include "GamePlayScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include <Input.h>
#include "Object3DCommon.h"
#include <ParameterManager.h>
#include <ParticleManager.h>
#include "Wireframe.h"

#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG


/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void GamePlayScene::Initialize()
{
#ifdef _DEBUG
	// デバッグカメラの初期化
	DebugCamera::GetInstance()->Initialize();
#endif // _DEBUG

	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();
	textureManager = TextureManager::GetInstance();
	particleManager = ParticleManager::GetInstance();

	/// ---------- サウンドの初期化 ---------- ///
	const char* fileName = "Resources/Sounds/Get-Ready.wav";
	wavLoader_ = std::make_unique<WavLoader>();
	wavLoader_->StreamAudioAsync(fileName, 0.0f, 1.0f, false);

	// terrainの生成と初期化
	objectTerrain_ = std::make_unique<Object3D>();
	objectTerrain_->Initialize("terrain.gltf");

	objectBall_ = std::make_unique<Object3D>();
	objectBall_->Initialize("sphere.gltf");


	obb.center = { 0.0f,5.0f,0.0f };
	// X・Y・Z 軸の向き（回転適用）
	obb.orientations[0] = Vector3(1.0f, 0.0f, 0.0f);  // X軸方向
	obb.orientations[1] = Vector3(0.0f, 1.0f, 0.0f);  // Y軸方向
	obb.orientations[2] = Vector3(0.0f, 0.0f, 1.0f);  // Z軸方向
	obb.size = { 1.0f,1.0f,1.0f };
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
#ifdef _DEBUG
	if (input_->TriggerKey(DIK_F12))
	{
		Object3DCommon::GetInstance()->SetDebugCamera(!Object3DCommon::GetInstance()->GetDebugCamera());
		Wireframe::GetInstance()->SetDebugCamera(!Wireframe::GetInstance()->GetDebugCamera());
		isDebugCamera_ = !isDebugCamera_;
	}
#endif // _DEBUG

	// オブジェクトの更新処理
	objectTerrain_->Update();
	objectBall_->Update();
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void GamePlayScene::Draw()
{


	/// ---------------------------------------- ///
	/// ---------- オブジェクト3D描画 ---------- ///
	/// ---------------------------------------- ///
	// オブジェクト3D共通描画設定
	Object3DCommon::GetInstance()->SetRenderSetting();

	// Terrain.obj の描画
	/*objectTerrain_->Draw();
	objectBall_->Draw();*/

	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 20.0f, { 0.25f, 0.25f, 0.25f,1.0f });
	/*Wireframe::GetInstance()->DrawSphere({ 0.0f,0.0f,0.0f }, 1.0f, { 1.0f,1.0f,1.0f,1.0f });
	Wireframe::GetInstance()->DrawOBB(obb, { 1.0f, 1.0f, 1.0f, 1.0f });
	Wireframe::GetInstance()->DrawTetrahedron(baseCenter, baseSize, height, axis, color);*/
	//Wireframe::GetInstance()->DrawOctahedron(center, size, color);
	//Wireframe::GetInstance()->DrawDodecahedron(center, size, color);
	//Wireframe::GetInstance()->DrawIcosahedron(center, size, color);

	time += 1.0f / 60.0f; // 60FPS前提（dtを加算）

	//Wireframe::GetInstance()->DrawPentagonalPrism(center, radius, height, color);
	//Wireframe::GetInstance()->DrawPentagonalPyramid(center, radius, height, color);
	//Wireframe::GetInstance()->DrawMobiusStrip({ 0.0f, 0.0f, 0.0f }, 5.0f, 1.0f, 100, 20, { 1.0f, 1.0f, 1.0f, 1.0f });
	//Wireframe::GetInstance()->DrawLemniscate3D({ 0.0f, 0.0f, 0.0f }, 5.0f, 3.0f, 2.0f, 100, { 1.0f, 0.5f, 1.0f, 1.0f });
	//Wireframe::GetInstance()->DrawMagicCircle({ 0.0f, 0.0f, 0.0f }, 5.0f, { 1.0f, 0.5f, 0.0f, 1.0f });
	//Wireframe::GetInstance()->DrawRotatingMagicCircle({ 0.0f, 0.0f, 0.0f }, 5.0f, color, time);
	Wireframe::GetInstance()->DrawProgressiveMagicCircle({ 0.0f, 0.0f, 0.0f }, 5.0f, { 1.0f, 0.5f, 0.0f, 1.0f }, time);

	/// ---------------------------------------- ///
	/// ---------- オブジェクト3D描画 ---------- ///
	/// ---------------------------------------- ///



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

	// TerrainのImGui
	objectTerrain_->DrawImGui();

	ImGui::End();
}
