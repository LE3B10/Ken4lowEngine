#define NOMINMAX
#include "AnimationModel.h"
#include <AnimationPipelineBuilder.h>
#include "ModelManager.h"
#include <TextureManager.h>
#include <DirectXCommon.h>
#include <Object3DCommon.h>
#include <ResourceManager.h>
#include <Wireframe.h>
#include "AssimpLoader.h"
#include "LightManager.h"
#include <SRVManager.h>
#include <UAVManager.h>

#include <imgui.h>
#include <numeric>

// -------------------------------------------------------------
///				　			補助関数群
/// -------------------------------------------------------------
namespace
{
	// subMesh をフラット化した結果
	struct FlattenResult
	{
		std::vector<VertexData> vertices; // 全頂点
		std::vector<uint32_t>   indices;  // 全インデックス
		std::vector<AnimationModel::LODEntry::SubMeshRange> ranges; // サブメッシュ範囲
	};

	// subMesh をフラット化
	static FlattenResult FlattenSubMeshes(const ModelData& md)
	{
		// 結果格納用
		FlattenResult out;
		out.vertices.reserve(4096); // 仮予約 : 頂点
		out.indices.reserve(8192);	// 仮予約 : インデックス

		uint32_t baseVertex = 0; // 頂点オフセット
		uint32_t startIndex = 0; // インデックスオフセット

		// サブメッシュごとにループ
		for (const auto& sm : md.subMeshes)
		{
			// 頂点コピー
			out.vertices.insert(out.vertices.end(), sm.vertices.begin(), sm.vertices.end());

			// インデックスコピー（baseVertex を足す）
			AnimationModel::LODEntry::SubMeshRange R{};
			R.startIndex = startIndex;								 // 開始インデックス
			R.indexCount = static_cast<uint32_t>(sm.indices.size()); // インデックス数

			// インデックスコピー
			for (uint32_t idx : sm.indices)	out.indices.push_back(idx + baseVertex);

			// オフセット更新
			startIndex += R.indexCount;

			// マテリアルSRVはここでは未設定（Initialize側で設定）
			out.ranges.push_back(R);

			// 頂点オフセット更新
			baseVertex += static_cast<uint32_t>(sm.vertices.size());
		}

		// 戻す
		return out;
	}

	// テクスチャロード＆SRV取得（空ならフォールバック）
	static D3D12_GPU_DESCRIPTOR_HANDLE LoadSrvOrFallback(const std::string& path)
	{
		static const std::string kFallback = "white.png";		// フォールバックテクスチャ
		auto* tm = TextureManager::GetInstance();				// テクスチャマネージャ取得
		const std::string& p = path.empty() ? kFallback : path; // パス決定
		tm->LoadTexture(p);										// テクスチャロード
		return tm->GetSrvHandleGPU(p);							// SRVハンドル取得
	}
}

/// -------------------------------------------------------------
///				　		 初期化処理
/// -------------------------------------------------------------
void AnimationModel::Initialize(const std::string& fileName, bool isSkinning)
{
	fileName_ = fileName;  // ファイル名保存

	// リソースを破棄
	Clear();

	// 共通部分の初期化
	dxCommon_ = DirectXCommon::GetInstance();
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();

	// モデル読み込み
	modelData = AssimpLoader::LoadModel(fileName_);

	// アニメーションモデルを読み込む
	animation = LoadAnimationFile(fileName_);

	// subMeshes をフラット化して、CS/スタンドアロン用 DEFAULTミラーとUAVを用意
	auto flat = FlattenSubMeshes(modelData);
	const UINT vbSize = UINT(sizeof(VertexData) * flat.vertices.size());

	// スケルトンの初期化
	skeleton_ = std::make_unique<Skeleton>();
	skeleton_ = Skeleton::CreateFromRootNode(modelData.rootNode);

	// マテリアルデータの初期化処理
	material_.Initialize();

	// アニメーション用の頂点とインデックスバッファを作成
	animationMesh_ = std::make_unique<AnimationMesh>();
	animationMesh_->Initialize(dxCommon_->GetDevice(), modelData);

	// 環境マップ
	TextureManager::GetInstance()->LoadTexture("SkyBox/skybox.dds");

	// 環境マップのハンドルを取得
	environmentMapHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("SkyBox/skybox.dds");

	// DEFAULTヒープに頂点バッファ（CS入力用ミラー）を作成
	{
		D3D12_HEAP_PROPERTIES defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC   bufDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

		// 頂点バッファ本体
		HRESULT hr = S_FALSE;
		hr = dxCommon_->GetDevice()->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&staticVBDefault_));
		assert(SUCCEEDED(hr));

		// 一時UploadにCPUから詰める
		ComPtr<ID3D12Resource> upload = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), vbSize);
		void* p = nullptr;
		upload->Map(0, nullptr, &p);
		std::memcpy(p, flat.vertices.data(), vbSize);
		upload->Unmap(0, nullptr);

		auto* cl = dxCommon_->GetCommandManager()->GetCommandList();

		// COMMON → COPY_DEST に明示遷移
		dxCommon_->ResourceTransition(staticVBDefault_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

		// コピー
		cl->CopyBufferRegion(staticVBDefault_.Get(), 0, upload.Get(), 0, vbSize);

		// COPY_DEST → GENERIC_READ に明示遷移（CSのSRV読み取り用）
		dxCommon_->ResourceTransition(staticVBDefault_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

		// コマンド実行
		dxCommon_->GetCommandManager()->ExecuteAndWait();
	}

	// 行列データの初期化
	wvpResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(TransformationAnimationMatrix));
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_)); // 書き込むためのアドレスを取得
	wvpData_->World = Matrix4x4::MakeIdentity();
	wvpData_->WVP = Matrix4x4::MakeIdentity();
	wvpData_->WorldInversedTranspose = Matrix4x4::MakeIdentity();

#pragma region カメラ用のリソースを作成
	// カメラ用のリソースを作る
	cameraResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(CameraForGPU));
	// 書き込むためのアドレスを取得
	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
	// カメラの初期位置
	cameraData->worldPosition = camera_->GetTranslate();
#pragma endregion

	// ジョイント初期化
	InitializeBones();

	// t1: 入力頂点 SRV（SRVヒープ）
	srvInputVerticesOnUavHeap_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateSRVForStructureBuffer(srvInputVerticesOnUavHeap_, staticVBDefault_.Get(), static_cast<UINT>(flat.vertices.size()), sizeof(VertexData));

	// スキニング用スキン頂点バッファ作成
	D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_RESOURCE_DESC   desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(VertexData) * flat.vertices.size(), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	// スキン済み頂点バッファ作成
	HRESULT hr = S_FALSE;
	hr = dxCommon_->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&skinnedVB_));
	assert(SUCCEEDED(hr));

	// UAV（u0）作成（UAVヒープ）
	uavOutIndex_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateUAVForStructuredBuffer(uavOutIndex_, skinnedVB_.Get(), static_cast<UINT>(flat.vertices.size()), sizeof(VertexData));

	// VBV 設定
	skinnedVBV_.BufferLocation = skinnedVB_->GetGPUVirtualAddress();
	skinnedVBV_.SizeInBytes = UINT(sizeof(VertexData) * flat.vertices.size());
	skinnedVBV_.StrideInBytes = sizeof(VertexData);

	// IBV 設定
	InitializeLODs();

	// b0: コンスタントバッファ（スキニング用）
	csCB_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(SkinningInformationForGPU));
	csCB_->Map(0, nullptr, reinterpret_cast<void**>(&csCBMapped_));
	csCBMapped_->numVertices = !lods_.empty() ? lods_[0].vertexCount : static_cast<uint32_t>(flat.vertices.size());
	csCBMapped_->isSkinning = isSkinning;
}

/// -------------------------------------------------------------
///				　　複数 LOD を直接渡すオーバーロード
/// -------------------------------------------------------------
void AnimationModel::Initialize(const std::string& fileName, const std::vector<std::string>& lodFiles, bool isSkinning)
{
	// 先に LOD リストを設定してから既存 Initialize を呼ぶ
	SetLodFiles(lodFiles);
	Initialize(fileName, isSkinning);
}

/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void AnimationModel::Update()
{
	// フレームカウント更新
	++frame_;

	// 遠距離カリングされている場合はアニメーション更新をスキップ
	if (culledByDistance_)
	{
		// 遠距離は何もしない
		material_.Update(); // ただしマテリアルの軽い更新は保持
		return;
	}

	// アニメーション時間の更新
	if (isAnimationPlaying_ && animation.duration > 0.0f)
	{
		// FPSの取得 deltaTimeの計算
		deltaTime = 1.0f / dxCommon_->GetFPSCounter().GetFPS();
		animationTime_ += deltaTime;
		animationTime_ = std::fmod(animationTime_, animation.duration);
	}

	// LODごとの更新間引き（重い処理はスキップ可）
	int li = std::min(lodIndex_, (int)lodUpdateEvery_.size() - 1); // LODインデックス
	uint32_t every = std::max(1u, lodUpdateEvery_[li]);			   // 最低1フレームに1回は更新
	bool doHeavy = (frame_ % every) == 0;						   // 重い処理を行うか

	// スキニング処理
	if (doHeavy && csCBMapped_ && csCBMapped_->isSkinning)
	{
		std::map<std::string, NodeAnimation>& nodeAnimations = animation.nodeAnimations; // ノードアニメーション群
		std::vector<Joint>& joints = skeleton_->GetJoints();			 // ジョイント群

		// ノードアニメーションの適用
		for (Joint& joint : joints)
		{
			// ノードアニメーションをノード名で検索
			auto it = nodeAnimations.find(joint.name);

			// ノードアニメーションが見つからなかった場合は、親の行列を使用
			if (it != nodeAnimations.end())
			{
				// ノードアニメーションが見つかった場合は、変換を適用
				NodeAnimation& nodeAnim = (*it).second;
				Vector3 translate = CalculateValue(nodeAnim.translate, animationTime_); // 座標系調整（Z軸反転で伸びを防ぐ）
				Quaternion rotate = CalculateValue(nodeAnim.rotate, animationTime_);	// 回転
				Vector3 scale = CalculateValue(nodeAnim.scale, animationTime_);			// 拡縮

				// 座標系調整（Z軸反転で伸びを防ぐ）
				joint.transform.translate = translate; // Vector3(translate.x, translate.y, -translate.z);
				joint.transform.rotate = rotate;	   // Quaternion(rotate.x, rotate.y, -rotate.z, -rotate.w);
				joint.transform.scale = scale;		   // 拡縮はそのまま

				// ローカル行列を更新
				joint.localMatrix = Matrix4x4::MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
			}
		}

		//  スケルトンの更新
		skeleton_->UpdateSkeleton();

		//  パレット更新（LODごと）
		if (!skinClusterLOD_.empty()) {
			skinClusterLOD_[lodIndex_]->UpdatePaletteMatrix(*skeleton_);
		}
	}

	// アニメーション行列の更新
	UpdateAnimation();

	// マテリアルの更新処理
	material_.Update();
}

/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void AnimationModel::Draw()
{
	// Compute 一括セット（スタンドアロン用）
	UAVManager::GetInstance()->PreDispatch();
	AnimationPipelineBuilder::GetInstance()->SetComputeSetting();

	// コンピュートでスキニング（個体差だけを束ねて実行）
	if (csCBMapped_->isSkinning) DispatchSkinningCS();

	// Graphics 一括セット（スタンドアロン用）
	SRVManager::GetInstance()->PreDraw();
	AnimationPipelineBuilder::GetInstance()->SetRenderSetting();

	// スキン済みメッシュを描画（個体差だけを束ねて実行）
	DrawSkinned();
}

/// -------------------------------------------------------------
///				　　単一のモデルをまとめて描画
/// -------------------------------------------------------------
void AnimationModel::DrawBatched(const std::unique_ptr<AnimationModel>& models)
{
	if (models) {
		if (models->IsVisible()) models->Draw();
	}
}

/// -------------------------------------------------------------
///				　可視チェックも含めてまとめて描画
/// -------------------------------------------------------------
void AnimationModel::DrawBatched(const std::vector<std::unique_ptr<AnimationModel>>& models)
{
	// Compute パス（全体で一回）
	UAVManager::GetInstance()->PreDispatch();
	AnimationPipelineBuilder::GetInstance()->SetComputeSetting();
	for (auto& m : models)
	{
		if (!m) continue;
		if (m->IsVisible()) m->DispatchSkinningCS();
	}

	// Graphics パス（全体で一回）
	SRVManager::GetInstance()->PreDraw();
	AnimationPipelineBuilder::GetInstance()->SetRenderSetting();
	for (auto& m : models)
	{
		if (!m) continue;
		if (m->IsVisible()) m->DrawSkinned();
	}
}

/// -------------------------------------------------------------
///				　ポインタ配列版も欲しければオーバーロード
/// -------------------------------------------------------------
void AnimationModel::DrawBatched(const std::vector<AnimationModel*>& models)
{
	// Compute パス（全体で一回）
	UAVManager::GetInstance()->PreDispatch();
	AnimationPipelineBuilder::GetInstance()->SetComputeSetting();

	// Compute パス（全体で一回）
	for (auto* m : models)
	{
		if (!m) continue;
		if (m->IsVisible()) m->DispatchSkinningCS();
	}

	// Graphics パス（全体で一回）
	SRVManager::GetInstance()->PreDraw();
	AnimationPipelineBuilder::GetInstance()->SetRenderSetting();

	// Graphics パス（全体で一回）
	for (auto* m : models) 
	{
		if (!m) continue;
		if (m->IsVisible()) m->DrawSkinned();
	}
}

/// -------------------------------------------------------------
///				　		 ImGui描画処理
/// -------------------------------------------------------------
void AnimationModel::DrawImGui()
{
	if (ImGui::Begin("AnimationModel Debug"))
	{
		auto& L = lods_[lodIndex_];

		ImGui::SeparatorText(fileName_.c_str());
		ImGui::Text("IsSkinning: %s", csCBMapped_ && csCBMapped_->isSkinning ? "true" : "false");
		ImGui::Text("Current LOD: %d / %d", lodIndex_, (int)lods_.size() - 1);
		ImGui::Text("LOD File    : %s", lodFileName_.empty() ? fileName_.c_str() : lodFileName_[lodIndex_].c_str());
		ImGui::Text("Vertices    : %u", L.vertexCount);
		ImGui::Text("Indices     : %u", L.indexCount);
		if (csCBMapped_) {
			ImGui::Text("CS numVertices (b0): %u", csCBMapped_->numVertices);
		}
		//ImGui::Text("Texture: %s", modelData.material.textureFilePath.c_str());

		ImGui::Checkbox("Force LOD", &forceLOD_);
		if (forceLOD_ && !lods_.empty()) {
			int prev = forcedLODIndex_;
			int maxLod = std::max(0, (int)lods_.size() - 1);
			ImGui::SliderInt("Forced LOD", &forcedLODIndex_, 0, maxLod);
			if (forcedLODIndex_ != prev) {
				lodIndex_ = std::clamp(forcedLODIndex_, 0, maxLod);
				if (csCBMapped_) { csCBMapped_->numVertices = lods_[lodIndex_].vertexCount; }
			}
		}
		ImGui::Checkbox("Use Compute Skinning", &useComputeSkinning_);
	}
	ImGui::End();
}

/// -------------------------------------------------------------
///				　			削除処理
/// -------------------------------------------------------------
void AnimationModel::Clear()
{
	animationMesh_.reset(); // アニメーションメッシュ解放
	skeleton_.reset();		// スケルトン解放

	// LODごとのスキンクラスタ解放
	for (auto& sc : skinClusterLOD_) { sc.reset(); }

	wvpResource.Reset();		// 行列リソース解放
	cameraResource.Reset();		// カメラリソース解放

	wvpData_ = nullptr;			// 行列データ初期化
	cameraData = nullptr;		// カメラデータ初期化

	modelData = {};				// モデルデータ初期化
	animation = {};				// アニメーション初期化
	animationTime_ = 0.0f;		// アニメーション時間初期化
	bodyPartColliders_.clear(); // ボディパートコライダー情報初期化
}

/// -------------------------------------------------------------
///				　		ワイヤーフレーム描画
/// -------------------------------------------------------------
void AnimationModel::DrawSkeletonWireframe()
{
#ifdef _DEBUG
	// スケルトンがなければ描画しない
	if (!skeleton_) { return; }

	// ジョイント群を取得
	const auto& joints = skeleton_->GetJoints();

	// ワールド行列を取得
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

	// ジョイントを親子で結ぶ線を描画
	for (const auto& joint : joints)
	{
		// 親ジョイントがある場合のみ描画
		if (joint.parent.has_value())
		{
			// 親ジョイントを取得
			const auto& parentJoint = joints[*joint.parent];

			// ワールド座標系に変換して線を描画
			Vector3 parentLocal = parentJoint.skeletonSpaceMatrix.GetTranslation();

			// joint のローカル座標も取得
			Vector3 jointLocal = joint.skeletonSpaceMatrix.GetTranslation();

			// 親の位置をワールド変換
			Vector3 parentPos = Vector3::Transform(parentLocal, worldMatrix);

			// ジョイントの位置をワールド変換
			Vector3 jointPos = Vector3::Transform(jointLocal, worldMatrix);

			// 線を描画
			Wireframe::GetInstance()->DrawLine(parentPos, jointPos, { 1.0f, 0.0f, 0.0f, 1.0f });
		}
	}
#endif // _DEBUG
}

/// -------------------------------------------------------------
///				　		ボディパートコライダー描画
/// -------------------------------------------------------------
void AnimationModel::DrawBodyPartColliders()
{
	// スケルトンがなければ描画しない
	if (!skeleton_) { return; }

	// ジョイント群を取得
	const auto& joints = skeleton_->GetJoints();

	// ワールド行列を取得
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

	// 各ボディパートコライダーを描画
	for (const auto& part : bodyPartColliders_)
	{
		// スフィアかカプセルかで分岐
		if (part.endJointIndex < 0)
		{
			// ジョイント位置＋オフセットをワールド変換
			const auto& joint = joints[part.startJointIndex];

			// ローカル位置計算
			Vector3 localPos = joint.skeletonSpaceMatrix.GetTranslation() + part.offset;

			// ワールド変換
			Vector3 worldPos = Vector3::Transform(localPos, worldMatrix);

			// スフィア描画
			Wireframe::GetInstance()->DrawSphere(worldPos, part.radius, { 0.0f, 1.0f, 0.0f, 1.0f });
		}
		else
		{
			// カプセル用
			const auto& jointA = joints[part.startJointIndex]; // 始点ジョイント
			const auto& jointB = joints[part.endJointIndex];   // 終点ジョイント

			Vector3 a = Vector3::Transform(jointA.skeletonSpaceMatrix.GetTranslation(), worldMatrix); // 始点ワールド変換
			Vector3 b = Vector3::Transform(jointB.skeletonSpaceMatrix.GetTranslation(), worldMatrix); // 終点ワールド変換

			// カプセル中心・軸・高さ計算
			Vector3 center = (a + b) * 0.5f;
			Vector3 axis = Vector3::Normalize(b - a);
			float height = Vector3::Length(b - a);

			// カプセル描画
			Wireframe::GetInstance()->DrawCapsule(center, part.radius, height, axis, 8, { 0.0f, 1.0f, 0.0f, 1.0f });
		}
	}
}

/// -------------------------------------------------------------
///				　		ボディパートコライダー情報取得
/// -------------------------------------------------------------
std::vector<std::pair<std::string, Capsule>> AnimationModel::GetBodyPartCapsulesWorld() const
{
	// 結果格納用
	std::vector<std::pair<std::string, Capsule>> out;

	// スケルトンがなければ描画しない
	if (!skeleton_) { return out; }

	// ジョイント群を取得
	const auto& joints = skeleton_->GetJoints();

	// ワールド行列を取得
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

	// 各ボディパートコライダーを描画
	for (const auto& part : bodyPartColliders_)
	{
		Capsule capsule{};
		capsule.radius = part.radius; // 半径設定

		// スフィアかカプセルかで分岐
		if (part.endJointIndex < 0)
		{
			// スフィア → 始点のみ回転適用
			const Vector3  local = joints[part.startJointIndex].skeletonSpaceMatrix.GetTranslation() + part.offset;
			Vector3 world = Vector3::Transform(local, worldMatrix); // ワールド変換
			capsule.segment.origin = capsule.segment.diff = world;  // 始点と終点を同じに
		}
		else
		{
			// カプセル → 始点と終点両方に回転適用
			Vector3 a = Vector3::Transform(joints[part.startJointIndex].skeletonSpaceMatrix.GetTranslation(), worldMatrix);
			Vector3 b = Vector3::Transform(joints[part.endJointIndex].skeletonSpaceMatrix.GetTranslation(), worldMatrix);
			capsule.segment.origin = a; // 始点
			capsule.segment.diff = b;   // 終点
		}
		// 結果格納
		out.emplace_back(part.name, capsule);
	}

	// 戻す
	return out;
}

/// -------------------------------------------------------------
///				　		ボディパートコライダー情報取得
/// -------------------------------------------------------------
std::vector<std::pair<std::string, Sphere>> AnimationModel::GetBodyPartSpheresWorld() const
{
	// 結果格納用
	std::vector<std::pair<std::string, Sphere>> out;

	// スケルトンがなければ描画しない
	if (!skeleton_) { return out; }

	const auto& joints = skeleton_->GetJoints(); // ジョイント群を取得

	// ワールド行列を取得
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

	// 各ボディパートコライダーを描画
	for (const auto& part : bodyPartColliders_)
	{
		if (part.endJointIndex < 0)
		{
			Sphere s{};

			// ローカル位置計算
			Vector3 local = joints[part.startJointIndex].skeletonSpaceMatrix.GetTranslation() + part.offset;
			s.center = Vector3::Transform(local, worldMatrix); // ワールド変換
			s.radius = part.radius;  						   // 半径設定
			out.emplace_back(part.name, s);					   // 結果格納
		}
	}
	return out;
}

/// -------------------------------------------------------------
///				　		LOD初期化処理
/// -------------------------------------------------------------
void AnimationModel::InitializeLODs()
{
	auto* device = dxCommon_->GetDevice();
	auto* cl = dxCommon_->GetCommandManager()->GetCommandList();

	// LOD 入力を決定：指定が無ければ「単一」、あればその数だけ
	std::vector<std::string> files = lodSourceFiles_.empty()
		? std::vector<std::string>{ fileName_ }    // 単一 LOD
	: lodSourceFiles_;                          // 指定された LOD 群

	// 書き込み前に必ずサイズ確保
	lods_.clear();						  // LOD情報クリア
	lods_.resize(files.size());			  // LOD情報も LOD ごとに確保
	skinClusterLOD_.resize(files.size()); // スキンクラスタも LOD ごとに確保
	lodFileName_.resize(files.size());	  // LODファイル名も LOD ごとに確保

	// LODごとに処理
	for (int i = 0; i < files.size(); ++i)
	{
		const std::string& fname = files[i];
		lodFileName_[i] = fname; // デバッグ表示用

		// --- モデル読込（同一スケルトン前提） ---
		ModelData md = AssimpLoader::LoadModel(fname);

		// サブメッシュをフラット化
		auto flat = FlattenSubMeshes(md);
		const UINT vbSize = UINT(sizeof(VertexData) * flat.vertices.size()); // 頂点バッファサイズ

		ComPtr<ID3D12Resource> defaultVB;
		{
			// --- VB: DEFAULT で作成 ---
			D3D12_HEAP_PROPERTIES heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			D3D12_RESOURCE_DESC   bufDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

			// DEFAULTヒープに頂点バッファを作成
			HRESULT hr = S_FALSE;
			hr = device->CreateCommittedResource(&heapDefault, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&defaultVB));
			assert(SUCCEEDED(hr));

			// Upload 経由でコピー
			ComPtr<ID3D12Resource> upload = ResourceManager::CreateBufferResource(device, vbSize);
			void* p = nullptr; upload->Map(0, nullptr, &p);
			std::memcpy(p, flat.vertices.data(), vbSize);
			upload->Unmap(0, nullptr);

			// トランジション＆コピー
			dxCommon_->ResourceTransition(defaultVB.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
			cl->CopyBufferRegion(defaultVB.Get(), 0, upload.Get(), 0, vbSize);
			dxCommon_->ResourceTransition(defaultVB.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

			// コマンド実行
			dxCommon_->GetCommandManager()->ExecuteAndWait();
		}

		// UAVヒープに t1 SRV を作成
		uint32_t t1Index = UAVManager::GetInstance()->Allocate();
		UAVManager::GetInstance()->CreateSRVForStructureBuffer(t1Index, defaultVB.Get(), static_cast<UINT>(flat.vertices.size()), sizeof(VertexData));

		// --- t2: SkinCluster（LOD ごとに作成） ---
		skinClusterLOD_[i] = std::make_unique<SkinCluster>();
		skinClusterLOD_[i]->Initialize(md, *skeleton_);

		// --- IB: DEFAULT で作成 ---
		const UINT ibSize = UINT(sizeof(uint32_t) * flat.indices.size());
		ComPtr<ID3D12Resource> defaultIB;
		{
			// DEFAULTヒープに頂点バッファを作成
			D3D12_HEAP_PROPERTIES heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			D3D12_RESOURCE_DESC   bufDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);

			// DEFAULTヒープに頂点バッファを作成
			HRESULT hr = S_FALSE;
			hr = device->CreateCommittedResource(&heapDefault, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&defaultIB));
			assert(SUCCEEDED(hr));

			// Upload 経由でコピー
			ComPtr<ID3D12Resource> upload = ResourceManager::CreateBufferResource(device, ibSize); // 一時アップロードバッファ
			void* p = nullptr; upload->Map(0, nullptr, &p);										   // マップ
			std::memcpy(p, flat.indices.data(), ibSize);										   // コピー
			upload->Unmap(0, nullptr);															   // アンマップ

			// トランジション＆コピー
			dxCommon_->ResourceTransition(defaultIB.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
			cl->CopyBufferRegion(defaultIB.Get(), 0, upload.Get(), 0, ibSize);
			dxCommon_->ResourceTransition(defaultIB.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

			// コマンド実行
			dxCommon_->GetCommandManager()->ExecuteAndWait();
		}

		// IBV 設定
		D3D12_INDEX_BUFFER_VIEW ibv{};
		ibv.BufferLocation = defaultIB->GetGPUVirtualAddress();			 // インデックスバッファ
		ibv.Format = DXGI_FORMAT_R32_UINT;								 // インデックスフォーマット
		ibv.SizeInBytes = static_cast<UINT>(defaultIB->GetDesc().Width); // インデックスバッファサイズ

		// --- u0: 出力頂点（UAV） & VBV ---
		ComPtr<ID3D12Resource> skinnedVB;
		{
			// スキニング用スキン頂点バッファ作成
			D3D12_HEAP_PROPERTIES heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			D3D12_RESOURCE_DESC   desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(VertexData) * flat.vertices.size(), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

			// スキン済み頂点バッファ作成
			HRESULT hr = S_FALSE;
			hr = device->CreateCommittedResource(&heapDefault, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&skinnedVB));
			assert(SUCCEEDED(hr));
		}

		// UAV ヒープに u0 UAV を作成
		uint32_t u0Index = UAVManager::GetInstance()->Allocate();

		// UAV 作成
		UAVManager::GetInstance()->CreateUAVForStructuredBuffer(u0Index, skinnedVB.Get(), static_cast<UINT>(flat.vertices.size()), sizeof(VertexData));

		// スキン済み頂点バッファビュー設定
		D3D12_VERTEX_BUFFER_VIEW skinnedVBV{};
		skinnedVBV.BufferLocation = skinnedVB->GetGPUVirtualAddress();				 // スキン済み頂点バッファ
		skinnedVBV.SizeInBytes = UINT(sizeof(VertexData) * flat.vertices.size());	 // スキン済み頂点バッファサイズ
		skinnedVBV.StrideInBytes = sizeof(VertexData);								 // スキン済み頂点バッファビュー

		// --- LODEntry へ格納 ---
		auto& L = lods_[i];															 // LODエントリ参照
		L.staticVBDefault = defaultVB;												 // 静的頂点バッファ
		L.srvInputVerticesOnUavHeap = t1Index;										 // 入力頂点SRV
		L.influenceSrvGpuOnUavHeap = skinClusterLOD_[i]->GetInfluenceSrvOnUAVHeap(); // スキンクラスタの影響情報SRV
		L.indexBuffer = defaultIB;													 // インデックスバッファ
		L.ibv = ibv;																 // インデックスバッファビュー
		L.skinnedVB = skinnedVB;													 // スキン済み頂点バッファ
		L.skinnedVBV = skinnedVBV;													 // スキン済み頂点バッファビュー
		L.uavIndex = u0Index;														 // 出力頂点UAV
		L.vertexCount = static_cast<uint32_t>(flat.vertices.size());				 // 頂点数
		L.indexCount = static_cast<uint32_t>(flat.indices.size());					 // インデックス数
		L.skinnedState = D3D12_RESOURCE_STATE_COMMON;								 // スキン済み頂点バッファの初期状態

		// サブメッシュ範囲とマテリアルSRVを設定
		L.subMeshRanges = flat.ranges;

		// 各サブメッシュのマテリアルテクスチャSRVを設定
		for (int si = 0; si < L.subMeshRanges.size(); ++si)
		{
			// サブメッシュ情報参照
			const auto& sm = md.subMeshes[si];

			// テクスチャSRVを読み込み、設定
			L.subMeshRanges[si].baseColorSrvGpuHandle = LoadSrvOrFallback(sm.material.textureFilePath);
		}
	}

	// 初期 LOD は 0
	lodIndex_ = 0;
}

/// -------------------------------------------------------------
///				　	アニメーションの更新処理
/// -------------------------------------------------------------
void AnimationModel::UpdateAnimation()
{
	// スキニングモデルか通常モデルかで分岐
	if (skeleton_ && csCBMapped_ && !skeleton_->GetJoints().empty() && csCBMapped_->isSkinning)
	{
		// スキニングあり（スキニングモデル用のWVP更新）
		Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);
		Matrix4x4 worldViewProjectionMatrix;

		// カメラがあればビュー射影行列を掛ける
		if (camera_)
		{
			// カメラ行列取得
			const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();

			// ワールドビュー射影行列計算
			worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjectionMatrix);
		}
		else
		{
			// カメラ無し → ワールド行列のみ
			worldViewProjectionMatrix = worldMatrix;
		}

		// スキニングあり（スキニングモデル用のWVP更新）
		wvpData_->World = worldMatrix;
		wvpData_->WVP = worldViewProjectionMatrix;
		wvpData_->WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(worldMatrix));
	}
	else
	{
		// アニメーション無し（通常モデル用のWVP更新）
		NodeAnimation& rootNodeAnimation = animation.nodeAnimations[modelData.rootNode.name]; // ルートノードのアニメーション取得
		Vector3 translate = CalculateValue(rootNodeAnimation.translate, animationTime_);	  // 座標系調整（Z軸反転で伸びを防ぐ）
		Quaternion rotate = CalculateValue(rootNodeAnimation.rotate, animationTime_);		  // 回転
		Vector3 scale = CalculateValue(rootNodeAnimation.scale, animationTime_);			  // 拡縮

		// ローカル行列計算
		Matrix4x4 localMatrix = Matrix4x4::MakeAffineMatrix(scale, rotate, translate);

		// ワールド行列計算
		Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);
		Matrix4x4 worldViewProjectionMatrix;

		// カメラ行列取得
		if (camera_)
		{
			// カメラ行列取得
			const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();

			// ワールドビュー射影行列計算
			worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjectionMatrix);
		}
		else
		{
			// カメラ無し → ワールド行列のみ
			worldViewProjectionMatrix = worldMatrix;
		}

		// 行列更新
		wvpData_->WVP = localMatrix * worldViewProjectionMatrix;
		wvpData_->World = localMatrix * worldMatrix;
		wvpData_->WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(localMatrix * worldMatrix));
	}
}


/// -------------------------------------------------------------
///				　アニメーションファイルを読み込む
/// -------------------------------------------------------------
Animation AnimationModel::LoadAnimationFile(const std::string& fileName)
{
	// アニメーションを解析
	Assimp::Importer importer;
	std::string filePath = "Resources/Models/" + fileName;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);

	// シーンが無い、またはアニメーションが無い場合は空のアニメーションを返す
	if (!scene || scene->mNumAnimations == 0)
	{
		// アニメなし → 空のまま返す
		animation.duration = 0.0f;
		return animation;
	}

	aiAnimation* animationAssimp = scene->mAnimations[0]; // 最初のアニメーションだけ採用。もちろん複数対応するに越したことない
	animation.duration = float(animationAssimp->mDuration / animationAssimp->mTicksPerSecond); // 時間の単位を秒に変換

	// NodeAnimationを解析する

	// Assimpでは個々のNodeのAnimationをchannelと読んでいるのでchannelをまわしてNodeAnimationの情報を撮ってくる
	for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex)
	{
		aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
		NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];

		// 座標（transform）のキーフレームを追加
		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex)
		{
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
			KeyframeVector3 keyframe;
			keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond); // ここも秒に変換
			keyframe.value = { -keyAssimp.mValue.x, keyAssimp.mValue.y,keyAssimp.mValue.z }; // 右手 → 左手
			nodeAnimation.translate.push_back(keyframe);
		}

		// 回転（rotate）のキーフレームを追加
		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex)
		{
			aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
			KeyframeQuaternion keyframe;
			keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
			keyframe.value = { keyAssimp.mValue.x, -keyAssimp.mValue.y, -keyAssimp.mValue.z, keyAssimp.mValue.w }; // 右手 → 左手
			nodeAnimation.rotate.push_back(keyframe);
		}

		// スケール（scale）のキーフレームを追加
		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex)
		{
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
			KeyframeVector3 keyframe;
			keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);		 // ここも秒に変換
			keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z }; // スケールはそのまま
			nodeAnimation.scale.push_back(keyframe);
		}
	}
	// 解析終了
	return animation;
}

/// -------------------------------------------------------------
///				　		ボーン初期化処理
/// -------------------------------------------------------------
void AnimationModel::InitializeBones()
{
	auto& jointMap = skeleton_->GetJointMap();

	/// ---------- 頭・首 ---------- ///
	if (auto it = jointMap.find("mixamorig:Head"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "Head", it->second, -1, {0, 0.12f, 0}, 0.12f * scaleFactor, 0.0f });
	}
	if (auto it = jointMap.find("mixamorig:Neck"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "Neck", it->second, -1, {0, 0.05f, 0}, 0.1f * scaleFactor, 0.0f });
	}

	/// ---------- 腹・胸 ---------- ///
	if (jointMap.contains("mixamorig:Spine") && jointMap.contains("mixamorig:Spine1")) {
		bodyPartColliders_.push_back({ "SpineLower", jointMap.at("mixamorig:Spine"), jointMap.at("mixamorig:Spine1"), {}, 0.15f * scaleFactor, 0.0f });
	}
	if (jointMap.contains("mixamorig:Spine1") && jointMap.contains("mixamorig:Spine2")) {
		bodyPartColliders_.push_back({ "SpineUpper", jointMap.at("mixamorig:Spine1"), jointMap.at("mixamorig:Spine2"), {0.0f,0.06f,0.0f}, 0.18f * scaleFactor, 0.0f });
	}

	/// ---------- 腰 ---------- ///
	if (auto it = jointMap.find("mixamorig:Hips"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "Pelvis", it->second, -1, {}, 0.16f * scaleFactor, 0.0f });
	}

	/// ---------- 左上腕・左前腕 ---------- ///
	if (jointMap.contains("mixamorig:LeftArm") && jointMap.contains("mixamorig:LeftForeArm")) {
		bodyPartColliders_.push_back({ "LeftUpperArm", jointMap.at("mixamorig:LeftArm"), jointMap.at("mixamorig:LeftForeArm"), {}, 0.1f * scaleFactor, 0.0f });
	}
	if (jointMap.contains("mixamorig:LeftForeArm") && jointMap.contains("mixamorig:LeftHand")) {
		bodyPartColliders_.push_back({ "LeftLowerArm", jointMap.at("mixamorig:LeftForeArm"), jointMap.at("mixamorig:LeftHand"), {}, 0.09f * scaleFactor, 0.0f });
	}

	/// ---------- 右上腕・右前腕 ---------- ///
	if (jointMap.contains("mixamorig:RightArm") && jointMap.contains("mixamorig:RightForeArm")) {
		bodyPartColliders_.push_back({ "RightUpperArm", jointMap.at("mixamorig:RightArm"), jointMap.at("mixamorig:RightForeArm"), {}, 0.1f * scaleFactor, 0.0f });
	}
	if (jointMap.contains("mixamorig:RightForeArm") && jointMap.contains("mixamorig:RightHand")) {
		bodyPartColliders_.push_back({ "RightLowerArm", jointMap.at("mixamorig:RightForeArm"), jointMap.at("mixamorig:RightHand"), {}, 0.09f * scaleFactor, 0.0f });
	}

	/// ---------- 左大腿・左下腿 ---------- ///
	if (jointMap.contains("mixamorig:LeftUpLeg") && jointMap.contains("mixamorig:LeftLeg")) {
		bodyPartColliders_.push_back({ "LeftUpperLeg", jointMap.at("mixamorig:LeftUpLeg"), jointMap.at("mixamorig:LeftLeg"), {}, 0.12f * scaleFactor, 0.0f });
	}
	if (jointMap.contains("mixamorig:LeftLeg") && jointMap.contains("mixamorig:LeftFoot")) {
		bodyPartColliders_.push_back({ "LeftLowerLeg", jointMap.at("mixamorig:LeftLeg"), jointMap.at("mixamorig:LeftFoot"), {}, 0.1f * scaleFactor, 0.0f });
	}

	/// ---------- 右大腿・右下腿 ---------- ///
	if (jointMap.contains("mixamorig:RightUpLeg") && jointMap.contains("mixamorig:RightLeg")) {
		bodyPartColliders_.push_back({ "RightUpperLeg", jointMap.at("mixamorig:RightUpLeg"), jointMap.at("mixamorig:RightLeg"), {}, 0.12f * scaleFactor, 0.0f });
	}
	if (jointMap.contains("mixamorig:RightLeg") && jointMap.contains("mixamorig:RightFoot")) {
		bodyPartColliders_.push_back({ "RightLowerLeg", jointMap.at("mixamorig:RightLeg"), jointMap.at("mixamorig:RightFoot"), {}, 0.1f * scaleFactor, 0.0f });
	}

	/// ---------- 左足 ---------- ///
	if (jointMap.contains("mixamorig:LeftFoot") && jointMap.contains("mixamorig:LeftToeBase")) {
		bodyPartColliders_.push_back({ "LeftToe", jointMap.at("mixamorig:LeftFoot"), jointMap.at("mixamorig:LeftToeBase"), {}, 0.07f * scaleFactor, 0.0f });
	}

	/// ---------- 右足 ---------- ///
	if (jointMap.contains("mixamorig:RightFoot") && jointMap.contains("mixamorig:RightToeBase")) {
		bodyPartColliders_.push_back({ "RightToe", jointMap.at("mixamorig:RightFoot"), jointMap.at("mixamorig:RightToeBase"), {}, 0.07f * scaleFactor, 0.0f });
	}

	/// ---------- 左肩と右肩 ---------- ///
	if (auto it = jointMap.find("mixamorig:LeftShoulder"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "LeftShoulder",it->second, -1, {-0.08f, 0.0f, 0.0f}, 0.11f * scaleFactor, 0.0f });
	}
	if (auto it = jointMap.find("mixamorig:RightShoulder"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "RightShoulder",	it->second, -1, {0.08f, 0.0f, 0.0f}, 0.11f * scaleFactor, 0.0f });
	}

	/// ---------- 左腕・右腕 ---------- ///
	if (auto it = jointMap.find("mixamorig:LeftForeArm"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "LeftElbow", it->second, -1, {}, 0.09f * scaleFactor, 0.0f });
	}
	if (auto it = jointMap.find("mixamorig:RightForeArm"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "RightElbow", it->second, -1, {}, 0.09f * scaleFactor, 0.0f });
	}

	/// ---------- 左膝・右膝 ---------- ///
	if (auto it = jointMap.find("mixamorig:LeftLeg"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "LeftKnee", it->second, -1, {}, 0.10f * scaleFactor, 0.0f });
	}
	if (auto it = jointMap.find("mixamorig:RightLeg"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "RightKnee",	it->second, -1, {}, 0.10f * scaleFactor, 0.0f });
	}

	/// ---------- 左手首・右手首 ---------- ///
	if (auto it = jointMap.find("mixamorig:LeftHand"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "LeftWrist",	it->second, -1, {}, 0.08f * scaleFactor, 0.0f });
	}
	if (auto it = jointMap.find("mixamorig:RightHand"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "RightWrist", it->second, -1, {}, 0.08f * scaleFactor, 0.0f });
	}
}

/// -------------------------------------------------------------
///				　		LOD距離設定処理
/// -------------------------------------------------------------
void AnimationModel::SetLODByDistance(float dist)
{
	if (lods_.size() <= 1) { return; } // 単一 LOD のときは何もしない

	const float baseIn = 10.f;
	const float step = 15.f;
	const float gap = 2.f;

	auto inThresh = [&](int i) { return baseIn + step * i; };
	auto outThresh = [&](int i) { return inThresh(i) + gap; };

	int newLOD = lodIndex_;
	const int last = (int)lods_.size() - 1;

	// 既存のヒステリシス LOD 遷移
	switch (lodIndex_)
	{
	case 0: // 最初 LOD 群

		// 内しきい値未満なら変化無し、外しきい値超過で LOD1 へ
		if (dist > outThresh(0)) newLOD = 1; break;

	default: // 中間 LOD 群

		// 内しきい値未満で LOD-1、外しきい値超過で LOD+1
		if (lodIndex_ > 0)
		{
			// 内しきい値未満で LOD-1、外しきい値超過で LOD+1
			if (dist < inThresh(lodIndex_ - 1)) newLOD = lodIndex_ - 1;

			// 外しきい値超過で LOD+1
			else if (lodIndex_ < last && dist > outThresh(lodIndex_)) newLOD = lodIndex_ + 1;
		}
		break;
	}

	// LOD 変更があれば反映
	if (newLOD != lodIndex_)
	{
		lodIndex_ = newLOD; // 変更

		// 頂点数も更新
		if (csCBMapped_) { csCBMapped_->numVertices = lods_[lodIndex_].vertexCount; }
	}

	// 最終 LOD の出しきい値 + 余白を越えたらカリング
	const float lastOut = outThresh(last);        // 例）LOD3の出しきい値 = 10 + 15*3 + 2 = 57
	culledByDistance_ = (dist > lastOut + farCullExtra_); // 例）57 + 20 = 77 より遠いとカリング
}

/// -------------------------------------------------------------
///				　		スキニング計算処理
/// -------------------------------------------------------------
void AnimationModel::DispatchSkinningCS()
{
	if (culledByDistance_) { return; } // 遠距離はスキニング自体しない

	auto* cl = dxCommon_->GetCommandManager()->GetCommandList();
	auto& L = lods_[lodIndex_];

	// 毎回、現在LODの頂点数に更新（ここが無いと“半分だけ更新”が起きる）
	csCBMapped_->numVertices = L.vertexCount;

	// 実状態 → UAV
	if (L.skinnedState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		dxCommon_->ResourceTransition(L.skinnedVB.Get(), L.skinnedState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		L.skinnedState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	// ルート：t0/t1/t2/u0/b0
	cl->SetComputeRootDescriptorTable(0, skinClusterLOD_[lodIndex_]->GetPaletteSrvOnUAVHeap());
	cl->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(L.srvInputVerticesOnUavHeap));
	cl->SetComputeRootDescriptorTable(2, L.influenceSrvGpuOnUavHeap);
	cl->SetComputeRootDescriptorTable(3, UAVManager::GetInstance()->GetGPUDescriptorHandle(L.uavIndex));
	cl->SetComputeRootConstantBufferView(4, csCB_->GetGPUVirtualAddress());

	// Dispatch（HLSLの[numthreads]と合わせる）
	constexpr uint32_t GROUP = 256;
	cl->Dispatch((L.vertexCount + GROUP - 1) / GROUP, 1, 1);

	// UAV → VB
	if (L.skinnedState != D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
	{
		dxCommon_->ResourceTransition(L.skinnedVB.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		L.skinnedState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	}
}

/// -------------------------------------------------------------
///				　		スキニング描画処理
/// -------------------------------------------------------------
void AnimationModel::DrawSkinned()
{
	if (culledByDistance_) { return; } // 描画もしない

	auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
	auto& L = lods_[lodIndex_];

	TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 4, environmentMapHandle_); // t4: 環境マップ

	// VB/IB
	if (csCBMapped_->isSkinning)
	{
		commandList->IASetVertexBuffers(0, 1, &L.skinnedVBV);  // ← 1本だけ
		commandList->IASetIndexBuffer(&L.ibv);

		// マテリアル＆描画
		material_.SetPipeline();

		// ルート定数バッファ
		commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());     // WVP (b#1)
		commandList->SetGraphicsRootConstantBufferView(3, cameraResource->GetGPUVirtualAddress());  // カメラ (b#3)

		for (const auto& range : L.subMeshRanges)
		{
			TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 2, range.baseColorSrvGpuHandle);
			commandList->DrawIndexedInstanced(range.indexCount, 1, range.startIndex, 0, 0);
		}

	}
	else
	{
		// 非CS: AnimationMesh が複数VB/IB対応済み前提でループ
		material_.SetPipeline();
		commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(3, cameraResource->GetGPUVirtualAddress());

		const size_t subCount = animationMesh_->GetSubmeshCount();
		for (size_t i = 0; i < subCount; ++i)
		{
			const auto& vbv = animationMesh_->GetVertexBufferView(i);
			const auto& ibv = animationMesh_->GetIndexBufferView(i);

			commandList->IASetVertexBuffers(0, 1, &vbv);
			commandList->IASetIndexBuffer(&ibv);

			// マテリアルSRV（InitializeLODs と同様、subMeshes[i] のテクスチャを使用）
			const auto& sm = modelData.subMeshes[i];
			TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 2, LoadSrvOrFallback(sm.material.textureFilePath));
			commandList->DrawIndexedInstanced(UINT(sm.indices.size()), 1, 0, 0, 0);

		}
	}
}
