#pragma once
#include "DX12Include.h"
#include "Vector3.h" 
#include "Vector4.h"
#include "Matrix4x4.h"
#include "LightParameterController.h"
#include <string>

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class DirectXCommon;


	/// -------------------------------------------------------------
	///				　		ライトの管理クラス
	/// -------------------------------------------------------------
	class LightManager
	{
	private: /// ---------- コンストラクタ・デストラクタ ---------- ///

		/// <summary>
		/// 外部からの生成を禁止するためのプライベートコンストラクタ。<br/>
		/// シングルトンパターンとして利用します。
		/// </summary>
		LightManager() = default;

		/// <summary>
		/// デフォルトデストラクタ。
		/// </summary>
		~LightManager() = default;

	public: /// ---------- 構造体 ---------- ///

		// パンクチュアルライトの構造体
		struct PunctualLightGPU
		{
			uint32_t lightType;		// ライトの種類（0：None、1：Directional、2：Point、3：Spot、4：RectArea、5：SphereArea）
			Vector4 color;			// ライトの色 （全ライト共通）
			float intensity;		// 輝度 （全ライト共通）
			Vector3 position;		// ライトの位置 （点光源、スポットライト用）
			float radius;			// ライトの届く最大距離 （点光源用）
			float decay;			// 減衰率 （点光源、スポットライト用）
			Vector3 direction;		// スポットライトの方向 （平行光源、スポットライト用）
			float distance;			// ライトの届く最大距離 （スポットライト用）
			float cosFalloffStart;	// 開始角度 （スポットライト用）
			float cosAngle;			// スポットライトの余弦 （スポットライト用）
			Vector3 areaSize;		// 疑似AreaLightサイズ (x=width, y=height, z=unused/radius)
			uint32_t enabled;		// 0: disabled, 1: enabled
		};


		// シャドウ行列の生成対象を明示するための種別。
		enum class ShadowCasterType : uint32_t
		{
			None = 0,
			Directional = 1,
			Spot = 2,
		};

		enum class ShadowFocusMode : uint32_t
		{
			Camera = 0,
			Player = 1,
			StageCenter = 2,
			Manual = 3,
		};
		// ライト数CB
		struct LightInfo
		{
			uint32_t lightCount; // ライトの数
			float pad[3]; 		 // パディング
		};

		// ステージを白飛びさせないためのライティング/露出調整CB。
		struct LightingSettingsGPU
		{
			Vector4 ambientColor = { 0.10f, 0.10f, 0.10f, 0.15f };
			Vector4 fogColor = { 0.58f, 0.64f, 0.70f, 1.0f };
			float exposure = 1.0f;
			float contrast = 1.0f;
			float fogStart = 45.0f;
			float fogEnd = 140.0f;
			uint32_t enableFog = 0;
			float specularStrength = 0.08f;
			float diffuseStrength = 1.0f;
			float specularPowerScale = 1.0f;
			float rimLightStrength = 0.0f;
			float rimLightPower = 2.0f;
			uint32_t enableRimLight = 0;
			uint32_t enableHalfLambert = 0;
			Vector4 rimLightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
			uint32_t shadingMode = 0;
			float pad[3] = {};
		};

		/// <summary>
		/// ParameterManagerとShadow設定をまとめて受け渡すための値オブジェクトです。<br/>
		/// GPU用構造体ではなく、LightParameterControllerからprivateメンバを直接触らせないための窓口として使います。
		/// </summary>
		struct ShadowSettings
		{
			bool enableShadow = true;
			float shadowBias = 0.0f;
			float normalBias = 0.025f;
			float shadowStrength = 0.6f;
			uint32_t shadowMapSize = 2048;
			bool showShadowMapDebug = false;
			bool showShadowFactorDebug = false;
			int32_t shadowCasterLightIndex = -1;
			ShadowFocusMode shadowFocusMode = ShadowFocusMode::Camera;
			Vector3 manualShadowFocusPosition = { 0.0f, 0.0f, 0.0f };
			float directionalShadowDistance = 60.0f;
			float directionalShadowWidth = 35.0f;
			float directionalShadowHeight = 35.0f;
			float directionalShadowNearZ = 0.1f;
			float directionalShadowFarZ = 120.0f;
			float directionalShadowFocusOffset = 0.0f;
			float spotShadowNearZ = 0.1f;
		};

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// LightManager のシングルトンインスタンスを取得します。
		/// </summary>
		/// <returns>LightManager の唯一のインスタンス。</returns>
		static LightManager* GetInstance();

		/// <summary>
		/// ライトマネージャの初期化処理。<br/>
		/// ・DirectXCommon の保持<br/>
		/// ・ライト数用 CBV の作成とマップ<br/>
		/// ・パンクチュアルライト用 Structured Buffer / SRV の準備（必要最小限のサイズ）<br/>
		/// を行います。
		/// </summary>
		/// <param name="dxCommon">DirectX12 共通クラスへのポインタ。</param>
		void Initialize(DirectXCommon* dxCommon);

		/// <summary>
		/// ライトマネージャの終了処理。<br/>
		/// 内部で確保したリソースを解放します。
		/// </summary>
		void Finalize();

		/// <summary>
		/// ImGui を用いたパンクチュアルライトの編集 UI を描画します。<br/>
		/// ・ライトの追加 / 全削除<br/>
		/// ・種類（None / Directional / Point / Spot）の切り替え<br/>
		/// ・色 / 輝度 / 位置 / 方向 / 半径 / 減衰 / 角度 の編集<br/>
		/// ・ライトごとの削除<br/>
		/// などを行います。
		/// </summary>
		void DrawImGui(bool* pOpen = nullptr);

		// Details Inspectorと専用Light Editorで同じPunctual Lights編集UIを共有する。
		void DrawPunctualLightsInspector();

		/// <summary>
		/// パンクチュアルライト情報をシェーダにバインドします。<br/>
		/// 内部で UpdatePunctualLight() を呼び出して GPU バッファと SRV を更新し、<br/>
		/// ・b2: LightInfo 用 CBV<br/>
		/// ・t2: パンクチュアルライト配列用 SRV<br/>
		/// をそれぞれルートパラメータに設定します。
		/// </summary>
		/// <param name="rootIndexCB_b2">ライト数 CBV をバインドするルートインデックス（b2）。</param>
		/// <param name="rootIndexSRV_t2">パンクチュアルライト SRV をバインドするルートインデックス（t2）。</param>
		void BindPunctualLights(uint32_t rootIndexCB_b2, uint32_t rootIndexSRV_t2);

		/// <summary>ライティング調整CBをシェーダにバインドします。</summary>
		void BindLightingSettings(uint32_t rootIndexCB_b5);

		/// <summary>
		/// デフォルトの指向性ライトを追加します。
		/// </summary>
		void AddDefaultDirectionalLight();

		/// <summary>既定プリセットを優先して初期ライトを再構成します。</summary>
		void ResetToDefaultLighting();
		bool SaveLightPreset(const std::string& assetId);
		bool ApplyLightPresetByPath(const std::string& filePath);

	public: /// ---------- ゲッター ---------- ///

		/// <summary>
		/// 点光源のリストを取得します。
		/// </summary>
		/// <returns>PunctualLightGPU オブジェクトのベクトルへの const 参照。</returns>
		const std::vector<PunctualLightGPU>& GetPunctualLights() const { return punctualLights_; }
		const LightingSettingsGPU& GetLightingSettings() const { return lightingSettings_; }
		float GetShadowBias() const { return shadowBias_; }
		float GetNormalBias() const { return normalBias_; }
		float GetShadowStrength() const { return shadowStrength_; }
		bool IsShadowEnabled() const { return enableShadow_; }
		uint32_t GetShadowMapSize() const { return shadowMapSize_; }
		bool IsShadowMapDebugEnabled() const { return showShadowMapDebug_; }
		bool IsShadowFactorDebugEnabled() const { return showShadowFactorDebug_; }
		LightingSettingsGPU& GetMutableLightingSettingsForEditor() { return lightingSettings_; }
		ShadowCasterType GetActiveShadowCasterType() const;
		Matrix4x4 BuildShadowLightViewProjection(const Vector3& focusPosition) const;
		bool TryGetActiveShadowCasterLightInfo(int32_t& outIndex, PunctualLightGPU& outLight, ShadowCasterType& outType) const;
		void SetShadowCasterLightIndex(int32_t index) { shadowCasterLightIndex_ = index; }
		int32_t GetShadowCasterLightIndex() const { return shadowCasterLightIndex_; }
		void SetShadowFocusMode(ShadowFocusMode mode) { shadowFocusMode_ = mode; }
		void SetManualShadowFocusPosition(const Vector3& pos) { manualShadowFocusPosition_ = pos; }
		void SetDirectionalShadowFrustum(float width, float height, float nearZ, float farZ) { directionalShadowWidth_ = width; directionalShadowHeight_ = height; directionalShadowNearZ_ = nearZ; directionalShadowFarZ_ = farZ; }
		void SetShadowMapSize(uint32_t size) { shadowMapSize_ = size; }

		// Editor Detailsから選択中ライトだけを書き換えるため、index検証付きヘルパー経由でのみ利用する。
		std::vector<PunctualLightGPU>& GetMutablePunctualLightsForEditor() { return punctualLights_; }

		/// <summary>
		/// Actorに追加されたLightComponent由来のPointLight一覧を反映します。
		/// </summary>
		void SetLightComponentPointLights(const std::vector<PunctualLightGPU>& lights);

	public: /// ---------- ParameterManager連携用の窓口 ---------- ///

		/// <summary>
		/// ParameterManager登録時に現在のLighting設定を読み取るための参照を返します。<br/>
		/// LightParameterController専用の窓口で、個別メンバを公開しすぎないためカテゴリ単位で受け渡します。
		/// </summary>
		const LightingSettingsGPU& GetLightingSettingsForParameter() const { return lightingSettings_; }

		/// <summary>
		/// ParameterManagerから読み込んだLighting設定を反映するための編集用参照を返します。<br/>
		/// LightManager本体はGPU転送に集中し、保存値の検証はLightParameterController側で行います。
		/// </summary>
		LightingSettingsGPU& GetMutableLightingSettingsForParameter() { return lightingSettings_; }

		/// <summary>
		/// ParameterManager登録・同期用にパンクチュアルライト配列を読み取ります。<br/>
		/// 現状の保存対象はLight #0のみですが、配列全体の所有はLightManagerに残します。
		/// </summary>
		const std::vector<PunctualLightGPU>& GetPunctualLightsForParameter() const { return punctualLights_; }

		/// <summary>
		/// ParameterManagerからLight #0を反映するための編集用ライト配列を返します。<br/>
		/// LightParameterControllerは必要な最小範囲だけを更新し、追加/削除UIの責務はLightManager側に残します。
		/// </summary>
		std::vector<PunctualLightGPU>& GetMutablePunctualLightsForParameter() { return punctualLights_; }

		/// <summary>
		/// Light #0保存対象が空配列で欠けている場合に、既定のDirectionalLightを補います。<br/>
		/// 保存済みJSONの反映先を必ず用意し、空配列アクセスによるクラッシュを防ぎます。
		/// </summary>
		void EnsureDefaultLightForParameter();

		/// <summary>
		/// ParameterManagerへ登録・同期するShadow設定をカテゴリ単位で取得します。<br/>
		/// privateメンバを直接公開せず、LightParameterControllerとの境界を狭く保つための窓口です。
		/// </summary>
		ShadowSettings GetShadowSettingsForParameter() const;

		/// <summary>
		/// ParameterManagerから検証済みのShadow設定をLightManagerへまとめて反映します。<br/>
		/// shadowMapSizeはGPUリソース再生成が必要なため、専用関数経由で適用します。
		/// </summary>
		void SetShadowSettingsFromParameter(const ShadowSettings& settings);

		/// <summary>
		/// ParameterManager由来のshadowMapSizeをLightManagerへ反映します。<br/>
		/// サイズ変更時だけShadowMapリソースを再生成し、毎回の無駄なGPUリソース更新を避けます。
		/// </summary>
		void ApplyShadowMapSizeFromParameter(uint32_t size);

	private: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// パンクチュアルライト用リソースの生成処理。<br/>
		/// ・ライト数 CBV 用バッファの生成とマップ<br/>
		/// ・SRV 用インデックスの確保<br/>
		/// ・GPU バッファの最小確保と SRV 作成<br/>
		/// を行います。Initialize から一度だけ呼び出されます。
		/// </summary>
		void CreatePunctualLight();

		/// <summary>
		/// パンクチュアルライトの GPU バッファ更新処理。<br/>
		/// ・有効なライトだけを抽出・正規化して一時配列に詰める<br/>
		/// ・必要であればバッファを再確保（サイズ拡張）し、データを書き込む<br/>
		/// ・NumElements に応じて SRV を再作成（最低 1 要素）<br/>
		/// ・ライト数 CBV に有効ライト数を書き込み<br/>
		/// ・デバッグ用ライトギズモの描画<br/>
		/// を行います。
		/// </summary>
		void UpdatePunctualLight();

		/// <summary>
		/// デバッグ用のライトギズモを描画します。<br/>
		/// Wireframe クラスを用いて、ライトの種類に応じて：<br/>
		/// ・平行光源：原点付近に矢印線<br/>
		/// ・点光源：位置に小さな球＋到達半径の球（半透明）<br/>
		/// ・スポットライト：位置に球＋方向線<br/>
		/// を描画します。
		/// </summary>
		void DebugDrawLightGizmos();

	private: /// ---------- メンバ変数 ---------- ///

		DirectXCommon* dxCommon_ = nullptr;

		std::vector<PunctualLightGPU> punctualLights_; // GPUに送るライト情報
		std::vector<PunctualLightGPU> lightComponentPointLights_; // LightComponentから収集したPointLight情報

		// GPU送信用バッファとSRV
		Microsoft::WRL::ComPtr<ID3D12Resource> punctualBuffer_;
		uint32_t punctualBufferBytes_ = 0;
		uint32_t punctualSRVIndex_ = UINT32_MAX;
		bool     punctualSRVAllocated_ = false;
		uint32_t punctualType_ = 1; // 0=None, 1=Directional, 2=Point, 3=Spot

		Microsoft::WRL::ComPtr<ID3D12Resource> lightInfoResource_;
		LightInfo* lightInfoData_ = nullptr;

		Microsoft::WRL::ComPtr<ID3D12Resource> lightingSettingsResource_;
		LightingSettingsGPU* lightingSettingsData_ = nullptr;
		LightingSettingsGPU lightingSettings_{};
		bool enableShadow_ = true;
		float shadowBias_ = 0.0f;
		float normalBias_ = 0.025f;
		float shadowStrength_ = 0.6f;
		uint32_t shadowMapSize_ = 2048;
		bool showShadowMapDebug_ = false;
		bool showShadowFactorDebug_ = false;

		// ステージに合わせてDirectionalLightの影有効範囲を調整するための設定値。
		float directionalShadowDistance_ = 60.0f;
		float directionalShadowWidth_ = 35.0f;
		float directionalShadowHeight_ = 35.0f;
		float directionalShadowNearZ_ = 0.1f;
		float directionalShadowFarZ_ = 120.0f;
		float directionalShadowFocusOffset_ = 0.0f;
		ShadowFocusMode shadowFocusMode_ = ShadowFocusMode::Camera;
		Vector3 manualShadowFocusPosition_ = { 0.0f, 0.0f, 0.0f };
		mutable Vector3 currentShadowFocusPosition_ = { 0.0f, 0.0f, 0.0f };
		mutable Vector3 currentShadowDirection_ = { 0.0f, -1.0f, 0.0f };
		mutable Matrix4x4 currentShadowLightViewProjection_ = Matrix4x4::MakeIdentity();
		mutable float currentShadowFrustumWidth_ = 35.0f;
		mutable float currentShadowFrustumHeight_ = 35.0f;
		mutable float currentShadowFrustumNearZ_ = 0.1f;
		mutable float currentShadowFrustumFarZ_ = 120.0f;
		float spotShadowNearZ_ = 0.1f;
		int32_t shadowCasterLightIndex_ = -1;
		LightParameterController lightParameterController_;

	private: /// ---------- コピー禁止 ---------- ///

		/// <summary>
		/// コピーコンストラクタは禁止。
		/// </summary>
		LightManager(const LightManager&) = delete;

		/// <summary>
		/// コピー代入演算子は禁止。
		/// </summary>
		LightManager& operator=(const LightManager&) = delete;

		/// <summary>
		/// ムーブコンストラクタは禁止。
		/// </summary>
		LightManager(LightManager&&) = delete;

		/// <summary>
		/// ムーブ代入演算子は禁止。
		/// </summary>
		LightManager& operator=(LightManager&&) = delete;
	};


} // namespace Ken4lowEngine
