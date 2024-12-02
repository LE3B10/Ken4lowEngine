#include "GameEngine.h"


/// -------------------------------------------------------------
///				　		　　初期化処理
/// -------------------------------------------------------------
void GameEngine::Initialize(uint32_t Width, uint32_t Height)
{
	/// ---------- シングルトンインスタンス ---------- ///
	winApp = WinApp::GetInstance();
	dxCommon = DirectXCommon::GetInstance();
	srvManager = std::make_unique<SRVManager>();
	input = Input::GetInstance();
	imguiManager = ImGuiManager::GetInstance();
	textureManager = TextureManager::GetInstance();
	modelManager = ModelManager::GetInstance();


	/// ---------- WindowsAPIのウィンドウ作成 ---------- ///
	winApp->CreateMainWindow(Width, Height);

	/// ---------- 入力の初期化 ---------- ///
	input->Initialize(winApp);

	/// ---------- DirectXの初期化 ----------///
	dxCommon->Initialize(winApp, Width, Height);

	/// ---------- SRVManagerの初期化 ---------- ///
	srvManager->Initialize(dxCommon);

	textureManager->Initialize(dxCommon, srvManager.get());

	/// ---------- ImGuiManagerの初期化 ---------- ///
	imguiManager->Initialize(winApp, dxCommon, srvManager.get());

	/// ---------- PipelineStateManagerの初期化 ---------- ///
	pipelineStateManager_ = std::make_unique<PipelineStateManager>();
	pipelineStateManager_->Initialize(dxCommon);

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
		//"Skydome.obj",
	};

	std::vector<Vector3> initialPositions = {
	{ -1.0f, 1.0f, 0.0f},    // axis.obj の座標
	{ 4.0f, 0.75f, 0.0f},    // multiMaterial.obj の座標
	{ -1.0f, -2.0f, 0.0f},    // multiMesh.obj の座標
	{ 4.0f, -2.0f, 0.0f},    // plane.obj の座標
	//{ 0.0f, 0.0f, 0.0f},    // skydome.obj の座標
	};

	/// ---------- カメラ初期化処理 ---------- ///
	camera_ = std::make_unique<Camera>();
	camera_->SetRotate({ 0.0f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,0.0f,-15.0f });
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

	//// 動作確認
	//std::this_thread::sleep_for(std::chrono::seconds(2)); // 2秒再生
	//wavLoader->PauseBGM(); // 一時停止


	//std::this_thread::sleep_for(std::chrono::seconds(2)); // 2秒待機
	//wavLoader->ResumeBGM(); // 再開

	//std::this_thread::sleep_for(std::chrono::seconds(30)); // 再生を続ける
	//wavLoader->StopBGM();
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void GameEngine::Update()
{
	//ウィンドウのｘボタンが押されるまでループ
	while (!winApp->ProcessMessage())
	{
		// 入力の更新
		input->Update();

		if (input->TriggerKey(DIK_0))
		{
			OutputDebugStringA("Hit 0\n");
		}

		/// ---------- ImGuiフレーム開始 ---------- ///
		imguiManager->BeginFrame();

#ifdef _DEBUG
		// 開発用のUIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
		ImGui::ShowDemoWindow();

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

		ImGui::End();

		camera_->DrawImGui();

#endif // _DEBUG

		/// ---------- ImGuiフレーム終了 ---------- ///
		imguiManager->EndFrame();

		// 3Dオブジェクトの更新処理
		for (const auto& object3D : objects3D_)
		{
			object3D->Update();
		}

		camera_->Update();

		// スプライトの更新処理
		for (auto& sprite : sprites_)
		{
			sprite->Update();
		}

		Draw();
	}
}


/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void GameEngine::Draw()
{
	// 描画開始処理
	dxCommon->BeginDraw();

	// SRVの処理
	srvManager->PreDraw();

	/*-----シーン（モデル）の描画設定と描画-----*/
	pipelineStateManager_->SetGraphicsPipeline(dxCommon->GetCommandList()); // ルートシグネチャとパイプラインステートの設定

	// 3Dオブジェクトデータ設定
	for (const auto& object3D : objects3D_)
	{
		object3D->SetObject3DBufferData(dxCommon->GetCommandList());
		object3D->DrawCall(dxCommon->GetCommandList());
	}

	///*-----スプライトの描画設定と描画-----*/
	for (auto& sprite : sprites_)
	{
		sprite->SetSpriteBufferData(dxCommon->GetCommandList());
		sprite->DrawCall(dxCommon->GetCommandList());
	}

	/*-----ImGuiの描画-----*/
	// ImGui描画のコマンドを積む
	imguiManager->Draw();

	// 描画終了処理
	dxCommon->EndDraw();
}


/// -------------------------------------------------------------
///				　			終了処理
/// -------------------------------------------------------------
void GameEngine::Finalize()
{
	wavLoader_->StopBGM();

	winApp->Finalize();
	dxCommon->Finalize();
	imguiManager->Finalize();
}
