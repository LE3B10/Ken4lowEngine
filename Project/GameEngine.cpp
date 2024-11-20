#include "GameEngine.h"


/// -------------------------------------------------------------
///				　		　　初期化処理
/// -------------------------------------------------------------
void GameEngine::Initialize(uint32_t Width, uint32_t Height)
{
	winApp = WinApp::GetInstance();
	dxCommon = DirectXCommon::GetInstance();
	input = Input::GetInstance();
	imguiManager = ImGuiManager::GetInstance();
	textureManager = TextureManager::GetInstance();
	modelManager = ModelManager::GetInstance();

	pipelineStateManager_ = std::make_unique<PipelineStateManager>();
	object3DCommon_ = std::make_unique<Object3DCommon>();
	camera_ = std::make_unique<Camera>();

	/// ---------- WindowsAPIのウィンドウ作成 ---------- ///
	winApp->CreateMainWindow(Width, Height);

	/// ---------- 入力の初期化 ---------- ///
	input->Initialize(winApp);

	/// ---------- DirectXの初期化 ----------///
	dxCommon->Initialize(winApp, Width, Height);

	/// ---------- ImGuiManagerの初期化 ---------- ///
	imguiManager->Initialize(winApp, dxCommon);

	/// ---------- PipelineStateManagerの初期化 ---------- ///
	pipelineStateManager_->Initialize(dxCommon);

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

	// .objのパスをリストで管理
	objectFiles = {
	   "axis.obj",
	   "multiMaterial.obj",
	   "multiMesh.obj",
	   "plane.obj",
	   //"Skydome.obj",
	};

	initialPositions = {
	{ -1.0f, 1.0f, 0.0f},    // axis.obj の座標
	{ 4.0f, 0.75f, 0.0f},    // multiMaterial.obj の座標
	{ -1.0f, -2.0f, 0.0f},    // multiMesh.obj の座標
	{ 4.0f, -2.0f, 0.0f},    // plane.obj の座標
	//{ 0.0f, 0.0f, 0.0f},    // skydome.obj の座標
	};

	//camera_->Initialize(Width, Height);
	camera_->SetRotate({ 0.0f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,0.0f,-20.0f });
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
	wavLoader_ = std::make_unique<WavLoader>();
	wavLoader_->Initialize();
	soundData = wavLoader_->SoundLoadWave("Resources/fanfare.wav");
	wavLoader_->SoundPlayWave(soundData);
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void GameEngine::Update()
{
	//ウィンドウのｘボタンが押されるまでループ
	while (!winApp->ProcessMessage())
	{
		//入力の更新
		input->Update();

		/// ---------- ImGuiフレーム開始 ---------- ///
		imguiManager->BeginFrame();

#ifdef _DEBUG
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

	/*-----シーン（モデル）の描画設定と描画-----*/
	// ルートシグネチャとパイプラインステートの設定
	pipelineStateManager_->SetGraphicsPipeline(dxCommon->GetCommandList());

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
	winApp->Finalize();
	dxCommon->Finalize();
	imguiManager->Finalize();

	wavLoader_->SoundUnload(&soundData);
}
