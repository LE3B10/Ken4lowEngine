#pragma once

#include "ComponentProperty.h"
#include "HumanoidDefinition.h"
#include "MaterialBinding.h"
#include "Object3D.h"
#include "SceneComponent.h"
#include "WorldTransformEx.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	/// 人型の部位生成、Transform階層、描画、Material、JSONをActorから分離するComponent。
	class HumanoidVisualComponent : public SceneComponent
	{
	public:
		/// 生成済みの1部位と描画に必要な実行時状態を保持する。
		struct BodyPart
		{
			std::string id;
			std::string parentId;
			std::unique_ptr<Object3D> object;
			WorldTransformEx transform;
			bool visible = true;
			bool active = true;
		};

	public:
		/// 外部定義または標準定義から人型部位を生成する。
		void Initialize() override;

		/// Componentと全部位の親子Transformおよび共有Materialを更新する。
		void Update(float deltaTime) override;

		/// Editor停止中も部位のTransformとMaterial表示を同期する。
		void UpdateEditor(float deltaTime) override;

		/// 物理更新後に確定したActor Transformへ全部位を追従させる。
		void PostPhysicsUpdate(float deltaTime) override;

		/// 表示中の全部位を通常描画する。
		void Draw() override;

		/// 表示中かつ影を落とす設定の全部位をShadow Mapへ描画する。
		void DrawShadow() override;

		/// 人型全体を指定Object IDでEditor Picking用Targetへ描画する。
		void DrawEditorObjectId(uint32_t objectId) override;

		/// 定義ファイル、スキン、Material、部位表示をDetailsへ表示する。
		void DrawImGui() override;

		/// 生成済み部位のGPU描画オブジェクトを破棄する。
		void Finalize() override;

		/// このComponentがShadow Caster設定を持つことを返す。
		bool SupportsShadowCasting() const override { return true; }

		/// このComponentがEditor Object-ID描画に対応することを返す。
		bool SupportsEditorObjectId() const override { return true; }

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "HumanoidVisualComponent"; }

		/// Component設定と人型定義をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONからComponent設定と人型定義を復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// 検証済みの人型定義へ差し替えて生成済み部位を再構築する。
		bool SetDefinition(const HumanoidDefinition& definition, std::string* outError = nullptr);

		/// 現在使用中の人型定義を返す。
		const HumanoidDefinition& GetDefinition() const { return definition_; }

		/// 次回読み込み対象となる人型定義ファイルパスを設定する。
		void SetDefinitionPath(std::string_view definitionPath) { definitionPath_ = std::string(definitionPath); }

		/// 現在設定されている人型定義ファイルパスを返す。
		const std::string& GetDefinitionPath() const { return definitionPath_; }

		/// 指定JSONファイルを読み込み、人型定義と生成済み部位を置き換える。
		bool LoadDefinitionFromFile(std::string_view definitionPath, std::string* outError = nullptr);

		/// 現在の人型定義を指定JSONファイルへ保存する。
		bool SaveDefinitionToFile(std::string_view definitionPath, std::string* outError = nullptr) const;

		/// 指定IDに一致する生成済み部位を返す。
		BodyPart* FindPart(std::string_view partId);
		const BodyPart* FindPart(std::string_view partId) const;

		/// 生成済みの全部位を構築順で返す。部位実体の所有権はこのComponentだけが持つ。
		std::vector<BodyPart>& GetParts() { return parts_; }
		const std::vector<BodyPart>& GetParts() const { return parts_; }

		/// 旧APIが渡すLight行列を全部位へ反映する。
		void UpdateShadowMatrices(const Matrix4x4& lightViewProjection);

		/// 指定IDの部位表示を切り替える。
		bool SetPartVisible(std::string_view partId, bool visible);

		/// 全部位の表示状態を一括で切り替える。
		void SetAllPartsVisible(bool visible);

		/// 全部位へ適用するスキンテクスチャを設定する。
		void SetSkinTexturePath(std::string_view texturePath);

		/// 現在のスキンテクスチャパスを返す。
		const std::string& GetSkinTexturePath() const { return skinTexturePath_; }

		/// 現在のスキンテクスチャを生成済み全部位へ再適用する。
		void ApplySkinToAllParts();

		/// 指定スキンテクスチャへ差し替えて生成済み全部位へ適用する。
		void ApplySkinToAllParts(std::string_view texturePath);

		/// 全部位で共有するMaterialAsset IDを設定する。
		void SetMaterialAssetId(std::string_view assetId);

		/// Component固有Material Overrideの有効状態を切り替える。
		void SetMaterialOverrideEnabled(bool enabled);

		/// 現在の共有Material設定を返す。
		const MaterialBinding& GetMaterialBinding() const { return materialBinding_; }

		/// 現在の共有MaterialとOverrideを生成済み全部位へ再適用する。
		void ApplyMaterialToAllParts();

		/// 最後の定義読み込みまたは再構築結果を返す。
		const std::string& GetStatusMessage() const { return statusMessage_; }

	private:
		/// 定義順に依存せず、親が先になる順序で部位とObject3Dを生成する。
		bool BuildBodyHierarchy(std::string* outError = nullptr);

		/// Detailsで予約された定義の再読込と保存を描画開始前の更新フェーズで処理する。
		void ProcessDeferredDefinitionRequests();

		/// Componentをルートとして全部位のWorldTransformとObject3Dを更新する。
		void UpdateHierarchy();

		/// 共有Materialとスキンテクスチャを生成済み全部位へ適用する。
		void ApplyAppearanceToAllParts();

		/// MaterialRepositoryの更新を検知して共有Materialを再適用する。
		void RefreshSharedMaterialBinding();

		/// 共有Material選択とComponent固有OverrideのDetails UIを描画する。
		void DrawMaterialBindingImGui();

		/// JSONとDetailsで共有する文字列プロパティ一覧を生成する。
		std::vector<ComponentProperty> CreateProperties();

	private:
		HumanoidDefinition definition_;
		std::string definitionPath_ = "Resources/JSON/Characters/DefaultHumanoid.json"; // Editor追加直後から外部データ駆動の標準人型を読み込む。
		std::vector<BodyPart> parts_;
		WorldTransformEx visualRootTransform_;
		std::string skinTexturePath_;
		MaterialBinding materialBinding_{};
		std::string materialBindingStatus_ = "モデル既定Materialを使用中";
		std::string statusMessage_ = "人型定義は未初期化です。";
		uint64_t materialRepositoryRevision_ = 0;
		bool requestDefinitionReload_ = false;
		bool requestDefinitionSave_ = false;
	};
} // namespace Ken4lowEngine
