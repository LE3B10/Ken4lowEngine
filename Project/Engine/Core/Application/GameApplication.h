#pragma once
#include <Framework.h>

namespace Ken4lowEngine
{


/// -------------------------------------------------------------
///				　	ゲーム全体を管理するクラス
/// -------------------------------------------------------------
class GameApplication : public Framework
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// ゲーム全体の初期化処理。<br/>
	/// まず Framework::Initialize() を呼び出して、ウィンドウ・DirectX・各種マネージャなど
	/// 基盤部分を初期化したあと、<br/>
	/// ・Input の初期化<br/>
	/// ・ParameterManager によるグローバルパラメータの読み込み<br/>
	/// ・SceneFactory の生成と SceneManager への登録<br/>
	/// ・最初のシーン（TitleScene）の設定<br/>
	/// を行います。
	/// </summary>
	void Initialize() override;

	/// <summary>
	/// 毎フレームの更新処理。<br/>
	/// Framework::Update() で共通の更新（カメラや各マネージャの更新前処理など）を行ったあと、<br/>
	/// ・入力状態の更新（Input）<br/>
	/// ・DebugCamera/通常カメラの更新切り替え<br/>
	/// ・SceneManager の更新（現在シーンの Update）<br/>
	/// ・PostEffectManager の更新<br/>
	/// などを行い、ゲーム全体の状態を進めます。
	/// </summary>
	void Update() override;

	/// <summary>
	/// 毎フレームの描画処理。<br/>
	/// おおまかな流れは以下の通りです：<br/>
	/// 1. DirectXCommon::BeginDraw() でバックバッファをクリア<br/>
	/// 2. ImGui フレーム開始（BeginFrame）と各種 ImGui ウィンドウの描画<br/>
	/// 3. PostEffectManager::BeginDraw() でオフスクリーンレンダリング開始（RTV/DSV 設定）<br/>
	/// 4. SceneManager による 3D オブジェクト描画 / ParticleManager / GpuParticleManager / Wireframe 描画<br/>
	/// 5. PostEffectManager::EndDraw() でオフスクリーン終了（SRV へ切り替え）<br/>
	/// 6. PostEffectManager::RenderPostEffect() でポストエフェクト適用（フルスクリーンクアッド描画）<br/>
	/// 7. SceneManager による 2D スプライト描画（UI など）<br/>
	/// 8. ImGui の描画（ImGuiManager::Draw）<br/>
	/// 9. DirectXCommon::EndDraw() で Present<br/>
	/// というレンダリングパイプラインを構築しています。
	/// </summary>
	void Draw() override;

	/// <summary>
	/// ゲーム終了時の後始末処理。<br/>
	/// まず Framework::Finalize() を呼び出して、WinApp / DirectXCommon など
	/// 基盤部分を解放したあと、<br/>
	/// SceneManager::Finalize() によって各シーンの終了処理を行います。
	/// </summary>
	void Finalize() override;
};


} // namespace Ken4lowEngine
