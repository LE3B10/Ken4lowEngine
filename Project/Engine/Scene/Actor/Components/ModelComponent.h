#pragma once
#include "SceneComponent.h"
#include "Object3D.h"
#include "ComponentProperty.h"
#include "MaterialBinding.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
	class Camera;

	/// -------------------------------------------------------------
	/// Actorに3Dモデル描画機能を追加するComponentクラス。
	/// -------------------------------------------------------------
	class ModelComponent : public SceneComponent
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// ModelComponentの初期化処理。
		/// </summary>
		void Initialize() override;

		/// <summary>
		/// ModelComponentの1フレーム更新処理。
		/// </summary>
		void Update(float deltaTime) override;

		/// <summary>
		/// PhysicsWorld更新後にObject3DのTransformを更新する
		/// </summary>
		void PostPhysicsUpdate(float deltaTime) override;

		/// <summary>
		/// ModelComponentの通常描画処理。
		/// </summary>
		void Draw() override;

		/// <summary>
		/// ModelComponentのシャドウ描画処理。
		/// </summary>
		void DrawShadow() override;
		bool SupportsShadowCasting() const override { return true; }
		bool SupportsEditorObjectId() const override { return true; }
		void DrawEditorObjectId(uint32_t objectId) override
		{
			if (visible_ && IsActiveInHierarchy() && object3D_)
			{
				object3D_->DrawEditorObjectId(objectId); // 通常Materialを使わずComponent IDだけをR32_UINTへ書き込む。
			}
		}

		/// <summary>
		/// ModelComponentのImGui描画処理。
		/// </summary>
		void DrawImGui() override;

		/// <summary>
		/// ModelComponentの終了処理。
		/// </summary>
		void Finalize() override;

	public: /// ---------- Jsonシリアライズ / デシリアライズ ---------- ///

		/// <summary>
		/// JSON保存・復元で使用するComponentのクラス種別を取得する。
		/// </summary>
		std::string GetClassTypeName() const override
		{
			return "ModelComponent"; // ModelComponentとして保存する。
		}

		/// <summary>
		/// ModelComponent固有情報をJSONへ保存する。
		/// </summary>
		void ToJson(nlohmann::json& outJson) const override;

		/// <summary>
		/// JSONからModelComponent固有情報を復元する
		/// </summary>
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- 設定処理 ---------- ///

		/// <summary>
		/// 描画するモデルファイルを設定する。
		/// </summary>
		void SetModelPath(std::string_view modelPath);
		void SetVisible(bool visible) { visible_ = visible; }

		/// <summary>
		/// 描画時に使用するカメラを設定する。
		/// </summary>
		void SetCamera(Camera* camera);

		/// <summary>共有MaterialAssetのIDを設定し、生成済みObject3Dへ即時反映します。</summary>
		void SetMaterialAssetId(std::string_view assetId);

		/// <summary>Component固有Material Overrideの有効状態を切り替えます。</summary>
		void SetMaterialOverrideEnabled(bool enabled);

		/// <summary>Material Bindingの読み取り専用情報を取得します。</summary>
		const MaterialBinding& GetMaterialBinding() const { return materialBinding_; }

		std::vector<ComponentProperty> CreateProperties(bool includeModelPath = true);

	private: /// ---------- 内部処理 ---------- ///

		/// <summary>
		/// OwnerActorのTransformComponentからObject3DへTransformを同期する。
		/// </summary>
		void SyncTransformToObject3D();

		/// <summary>共有AssetまたはComponent固有OverrideをObject3Dへ反映します。</summary>
		void ApplyMaterialBinding();

		/// <summary>共有MaterialAssetの更新世代が変わった場合だけObject3Dへ再反映します。</summary>
		void RefreshSharedMaterialBinding();

		/// <summary>Material Bindingを編集する日本語ImGuiを描画します。</summary>
		void DrawMaterialBindingImGui();

	private: /// ---------- メンバ変数 ---------- ///

		std::unique_ptr<Object3D> object3D_; // 実際の3Dモデル描画を担当するObject3D。
		std::string modelPath_;              // Object3Dに読み込ませるモデルファイルパス。
		Camera* camera_ = nullptr;           // 描画に使用するカメラ。所有権は持たない。
		bool visible_ = true;                // 描画するかどうか
		MaterialBinding materialBinding_{};  // 共有Asset参照とComponent固有Overrideだけを保持する。
		std::string materialBindingStatus_ = "モデル既定Materialを使用中";
		uint64_t materialRepositoryRevision_ = 0; // 共有Assetのライブ更新を毎フレームの検索なしで検知する。
	};
}