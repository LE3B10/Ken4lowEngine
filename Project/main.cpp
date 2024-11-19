#include "WinApp.h"
#include "Input.h"
#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include "D3DResourceLeakChecker.h"
#include "LogString.h"
#include "PipelineStateManager.h"
#include "TextureManager.h"
#include "Sprite.h"
#include "ResourceManager.h"
#include "Object3D.h"
#include "Object3DCommon.h"
#include "ModelManager.h"

#include "GameStateManager.h"
#include "Player.h"
#include "Boss.h"

#include "ResourceObject.h"

D3DResourceLeakChecker resourceLeakCheck;

// クライアント領域サイズ
static const uint32_t kClientWidth = 1280;
static const uint32_t kClientHeight = 720;

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	/// ---------- シングルトンインスタンス ---------- ///
	WinApp* winApp = WinApp::GetInstance();
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	Input* input = Input::GetInstance();
	ImGuiManager* imguiManager = ImGuiManager::GetInstance();
	TextureManager* textureManager = TextureManager::GetInstance();
	ModelManager* modelManager = ModelManager::GetInstance();

	/// ---------- WindowsAPIのウィンドウ作成 ---------- ///
	winApp->CreateMainWindow(kClientWidth, kClientHeight);

	/// ---------- 入力の初期化 ---------- ///
	input->Initialize(winApp);

	/// ---------- DirectXの初期化 ----------///
	dxCommon->Initialize(winApp, kClientWidth, kClientHeight);

	/// ---------- ImGuiManagerの初期化 ---------- ///
	imguiManager->Initialize(winApp, dxCommon);

	/// ---------- PipelineStateManagerの初期化 ---------- ///
	std::unique_ptr<PipelineStateManager> pipelineStateManager = std::make_unique<PipelineStateManager>();
	pipelineStateManager->Initialize(dxCommon);

	// テクスチャのパスをリストで管理
	std::vector<std::string> texturePaths = {
		"Resources/uvChecker.png",
		//"Resources/monsterBall.png",
	};

	/// ---------- TextureManagerの初期化 ----------///
	for (auto& texture : texturePaths)
	{
		textureManager->LoadTexture(texture);
	}

	/// ---------- Spriteの初期化 ---------- ///
	std::vector<std::unique_ptr<Sprite>> sprites;
	for (uint32_t i = 0; i < 1; i++)
	{
		sprites.push_back(std::make_unique<Sprite>());
		sprites[i]->Initialize(texturePaths[i % 2]);
		sprites[i]->SetPosition(Vector2(100.0f * i, 100.0f * i));
	}

	/// ---------- Object3Dの初期化 ---------- ///
	std::vector<std::unique_ptr<Object3D>> objects3D;

	std::unique_ptr<Object3DCommon> object3DCommon = std::make_unique<Object3DCommon>();

	// .objのパスをリストで管理
	std::vector<std::string> objectFiles = {
		"Skydome.obj",
		/*"axis.obj",
		"multiMaterial.obj",
		"multiMesh.obj",
		"plane.obj",*/
	};

	std::vector<Vector3> initialPositions = {
	{ 0.0f, 0.0f, 0.0f},    // skydome.obj の座標
	//{ -1.0f, 1.0f, 0.0f},    // axis.obj の座標
	//{ 4.0f, 0.75f, 0.0f},    // multiMaterial.obj の座標
	//{ -1.0f, -2.0f, 0.0f},    // multiMesh.obj の座標
	//{ 4.0f, -2.0f, 0.0f},    // plane.obj の座標
	};

	/// ---------- カメラ初期化処理 ---------- ///
	Camera* camera = new Camera();
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-15.0f });
	object3DCommon->SetDefaultCamera(camera);

	// 各オブジェクトを初期化し、座標を設定
	for (uint32_t i = 0; i < objectFiles.size(); ++i)
	{
		auto object = std::make_unique<Object3D>();
		object->Initialize(object3DCommon.get(), objectFiles[i]);
		object->SetTranslate(initialPositions[i]);
		objects3D.push_back(std::move(object));
	}

	/// ---------- ゲームステート初期化処理 ---------- ///
	std::unique_ptr<GameStateManager> gameStateManager = std::make_unique<GameStateManager>();

	// 初期状態をメニューに設定
	gameStateManager->ChangeState(new MenuState);

	/// ---------- プレイヤー初期化処理 ---------- ///
	std::unique_ptr<Player> player = std::make_unique<Player>();
	// 初期状態
	player->ChangeState(new IdleState());

	/// ---------- ボスの初期化処理 ---------- ///
	std::unique_ptr<Boss> boss = std::make_unique<Boss>();
	// 初期状態
	boss->ChangeState(new BossIdleState());


#pragma endregion

	bool useMonsterBall = true;

	//ウィンドウのｘボタンが押されるまでループ
	while (!winApp->ProcessMessage())
	{
		// 入力の更新
		input->Update();

		if (input->TriggerKey(DIK_0))
		{
			OutputDebugStringA("Hit 0 \n");
		}

		/// ---------- ImGuiフレーム開始 ---------- ///
		imguiManager->BeginFrame();

#ifdef _DEBUG
		// 開発用のUIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
		ImGui::ShowDemoWindow();

		ImGui::Begin("Test Window");

		for (uint32_t i = 0; i < objects3D.size(); ++i)
		{
			ImGui::PushID(i); // オブジェクトごとにIDを区別
			if (ImGui::TreeNode(("Object3D " + std::to_string(i)).c_str()))
			{
				objects3D[i]->DrawImGui();
				ImGui::TreePop();
			}
			ImGui::PopID(); // IDをリセット
		}

		for (uint32_t i = 0; i < sprites.size(); i++)
		{
			ImGui::PushID(i); // スプライトごとに異なるIDを設定
			if (ImGui::TreeNode(("Sprite" + std::to_string(i)).c_str()))
			{
				Vector2 position = sprites[i]->GetPosition();
				ImGui::DragFloat2("Position", &position.x, 1.0f);
				sprites[i]->SetPosition(position);

				float rotation = sprites[i]->GetRotation();
				ImGui::SliderAngle("Rotation", &rotation);
				sprites[i]->SetRotation(rotation);

				Vector2 size = sprites[i]->GetSize();
				ImGui::DragFloat2("Size", &size.x, 1.0f);
				sprites[i]->SetSize(size);

				ImGui::TreePop();
			}
			ImGui::PopID(); // IDを元に戻す
		}

		ImGui::Checkbox("useMonsterBall", &useMonsterBall);

		ImGui::End();

		camera->DrawImGui();

#endif // _DEBUG

		/// ---------- ImGuiフレーム終了 ---------- ///
		imguiManager->EndFrame();

		// 3Dオブジェクトの更新処理
		for (const auto& object3D : objects3D)
		{
			object3D->Update();
		}

		camera->Update();

		// スプライトの更新処理
		for (auto& sprite : sprites)
		{
			sprite->Update();
		}


		/// ---------- ステートパターン ---------- ///

		// 状態の更新
		if (input->TriggerKey(DIK_1))
		{
			gameStateManager->Update();
		}

		// 描画処理
		if (input->TriggerKey(DIK_2))
		{
			gameStateManager->Draw();
		}

		// キー入力されたらゲームプレイに移行
		if (input->TriggerKey(DIK_3))
		{
			gameStateManager->ChangeState(new GamePlayState());
		}

		/// ---------- ステートパターン ---------- ///


		/// ---------- プレイヤー ---------- ///

		// 状態更新
		if (input->TriggerKey(DIK_4))
		{
			player->Update();
		}

		// 
		if (input->TriggerKey(DIK_5))
		{
			player->Draw();
		}
		

		if (input->TriggerKey(DIK_6))
		{
			player->ChangeState(new RunningState());
		}

		/// ---------- プレイヤー ---------- ///

		/// ---------- ボス ---------- ///

		if (input->TriggerKey(DIK_7))
		{
			// 状態更新
			boss->Update();
		}

		if (input->TriggerKey(DIK_8))
		{
			boss->ChangeState(new BossAttackState()); // 攻撃状態に変更
		}

		if (input->TriggerKey(DIK_8))
		{
			boss->ChangeState(new EnragedState()); // 怒り状態に変更
		}

		if (input->TriggerKey(DIK_9))
		{
			boss->ChangeState(new BossDownState()); // ダウン状態に変更
		}
		
		/// ---------- ボス ---------- ///


		// 描画開始処理
		dxCommon->BeginDraw();

		// ディスクリプタヒープの設定
		ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon->GetSRVDescriptorHeap() };
		dxCommon->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);

		/*-----シーン（モデル）の描画設定と描画-----*/
		// ルートシグネチャとパイプラインステートの設定
		pipelineStateManager->SetGraphicsPipeline(dxCommon->GetCommandList());

		// 3Dオブジェクトデータ設定
		for (const auto& object3D : objects3D)
		{
			object3D->SetObject3DBufferData(dxCommon->GetCommandList());
			object3D->DrawCall(dxCommon->GetCommandList());
		}

		///*-----スプライトの描画設定と描画-----*/
		for (auto& sprite : sprites)
		{
			sprite->SetSpriteBufferData(dxCommon->GetCommandList());
			//sprite->DrawCall(dxCommon->GetCommandList());
		}

		/*-----ImGuiの描画-----*/
		// ImGui描画のコマンドを積む
		imguiManager->Draw();

		// 描画終了処理
		dxCommon->EndDraw();
	}

	winApp->Finalize();
	dxCommon->Finalize();
	imguiManager->Finalize();

	return 0;
}