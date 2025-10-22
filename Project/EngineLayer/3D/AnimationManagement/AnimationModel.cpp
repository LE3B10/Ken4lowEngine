#define NOMINMAX
#include "AnimationModel.h"
#include "ModelManager.h"
#include <TextureManager.h>
#include <DirectXCommon.h>
#include <Object3DCommon.h>
#include <SRVManager.h>
#include <ResourceManager.h>
#include <Wireframe.h>
#include "AssimpLoader.h"
#include "LightManager.h"
#include <imgui.h>
#include <numeric>
#include <AnimationPipelineBuilder.h>
#include <UAVManager.h>

namespace
{
	struct FlattenResult
	{
		std::vector<VertexData> vertices;
		std::vector<uint32_t>   indices;
		std::vector<AnimationModel::LODEntry::SubMeshRange> ranges;
	};

	// subMeshes を 1 本の VB/IB に結合（インデックスは baseVertex を加算）
	static FlattenResult FlattenSubMeshes(const ModelData& md)
	{
		FlattenResult out;
		out.vertices.reserve(4096);
		out.indices.reserve(8192);

		uint32_t baseVertex = 0;
		uint32_t startIndex = 0;

		for (const auto& sm : md.subMeshes)
		{
			// 頂点コピー
			out.vertices.insert(out.vertices.end(), sm.vertices.begin(), sm.vertices.end());

			// インデックスコピー（baseVertex を足す）
			AnimationModel::LODEntry::SubMeshRange R{};
			R.startIndex = startIndex;
			R.indexCount = static_cast<uint32_t>(sm.indices.size());

			for (uint32_t idx : sm.indices)
			{
				out.indices.push_back(idx + baseVertex);
			}
			startIndex += R.indexCount;

			// マテリアルSRVはここでは未設定（Initialize側で設定）
			out.ranges.push_back(R);

			baseVertex += static_cast<uint32_t>(sm.vertices.size());
		}
		return out;
	}

	// テクスチャロード＆SRV取得（空ならフォールバック）
	static D3D12_GPU_DESCRIPTOR_HANDLE LoadSrvOrFallback(const std::string& path)
	{
		static const std::string kFallback = "white.png";
		auto* tm = TextureManager::GetInstance();
		const std::string& p = path.empty() ? kFallback : path;
		tm->LoadTexture(p);
		return tm->GetSrvHandleGPU(p);
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

	dxCommon_ = DirectXCommon::GetInstance();
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();

	// モデル読み込み
	modelData = AssimpLoader::LoadModel(fileName_);

	// アニメーションモデルを読み込む
	animation = LoadAnimationFile(fileName_);

	// subMeshes をフラット化して、CS/スタンドアロン用 DEFAULTミラーとUAVを用意
	auto flat = FlattenSubMeshes(modelData);
	const UINT vbSize = UINT(sizeof(VertexData) * flat.vertices.size());

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

		// ★ 初期は COMMON（警告回避）
		HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
			&defaultHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&staticVBDefault_));
		assert(SUCCEEDED(hr));

		// 一時UploadにCPUから詰める
		ComPtr<ID3D12Resource> upload = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), vbSize);
		void* p = nullptr;
		upload->Map(0, nullptr, &p);
		std::memcpy(p, flat.vertices.data(), vbSize);
		upload->Unmap(0, nullptr);

		auto* cl = dxCommon_->GetCommandManager()->GetCommandList();

		// ★ COMMON → COPY_DEST に明示遷移
		dxCommon_->ResourceTransition(staticVBDefault_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

		// コピー
		cl->CopyBufferRegion(staticVBDefault_.Get(), 0, upload.Get(), 0, vbSize);

		// ★ COPY_DEST → GENERIC_READ に明示遷移（CSのSRV読み取り用）
		dxCommon_->ResourceTransition(staticVBDefault_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

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

	InitializeBones();

	// t1: 入力頂点 SRV（SRVヒープ）
	srvInputVerticesOnUavHeap_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateSRVForStructureBuffer(srvInputVerticesOnUavHeap_, staticVBDefault_.Get(), static_cast<UINT>(flat.vertices.size()), sizeof(VertexData));

	D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_RESOURCE_DESC   desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(VertexData) * flat.vertices.size(), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&skinnedVB_));
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
	csCBMapped_->numVertices = !lods_.empty() ? lods_[0].vertexCount
		: static_cast<uint32_t>(flat.vertices.size());
	csCBMapped_->isSkinning = isSkinning;
}

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
	++frame_;

	if (culledByDistance_) {                // ★遠距離は何もしない
		material_.Update();                 // ただしマテリアルの軽い更新は保持
		return;
	}

	if (isAnimationPlaying_ && animation.duration > 0.0f)
	{
		// FPSの取得 deltaTimeの計算
		deltaTime = 1.0f / dxCommon_->GetFPSCounter().GetFPS();
		animationTime_ += deltaTime;
		animationTime_ = std::fmod(animationTime_, animation.duration);
	}

	// ★ LODごとの更新間引き（重い処理はスキップ可）
	int li = std::min(lodIndex_, (int)lodUpdateEvery_.size() - 1);
	uint32_t every = std::max(1u, lodUpdateEvery_[li]);
	bool doHeavy = (frame_ % every) == 0;

	if (doHeavy && csCBMapped_ && csCBMapped_->isSkinning)
	{
		auto& nodeAnimations = animation.nodeAnimations;
		auto& joints = skeleton_->GetJoints();

		// 2. ノードアニメーションの適用
		for (auto& joint : joints)
		{
			auto it = nodeAnimations.find(joint.name);

			// ノードアニメーションが見つからなかった場合は、親の行列を使用
			if (it != nodeAnimations.end())
			{
				NodeAnimation& nodeAnim = (*it).second;
				Vector3 translate = CalculateValue(nodeAnim.translate, animationTime_);
				Quaternion rotate = CalculateValue(nodeAnim.rotate, animationTime_);
				Vector3 scale = CalculateValue(nodeAnim.scale, animationTime_);

				// 座標系調整（Z軸反転で伸びを防ぐ）
				joint.transform.translate = translate;
				joint.transform.rotate = rotate;
				joint.transform.scale = scale;

				joint.localMatrix = Matrix4x4::MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
			}
		}

		// 3. スケルトンの更新
		skeleton_->UpdateSkeleton();

		// 4. パレット更新（LODごと）
		if (!skinClusterLOD_.empty()) {
			skinClusterLOD_[lodIndex_]->UpdatePaletteMatrix(*skeleton_);
		}
	}

	UpdateAnimation();

	// マテリアルの更新処理
	material_.Update();
}


/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void AnimationModel::Draw()
{
	// --- Standalone only（大量描画では Scene 側で一括する）---
	// Compute 一括セット（スタンドアロン用）
	UAVManager::GetInstance()->PreDispatch();
	AnimationPipelineBuilder::GetInstance()->SetComputeSetting();

	// コンピュートでスキニング（個体差だけを束ねて実行）
	if (csCBMapped_->isSkinning) {
		DispatchSkinningCS();
	}

	// Graphics 一括セット（スタンドアロン用）
	SRVManager::GetInstance()->PreDraw();
	AnimationPipelineBuilder::GetInstance()->SetRenderSetting();

	// スキン済みメッシュを描画（個体差だけを束ねて実行）
	DrawSkinned();
}

void AnimationModel::DrawBatched(const std::unique_ptr<AnimationModel>& models)
{
	if (models) {
		if (models->IsVisible()) models->Draw();
	}
}

void AnimationModel::DrawBatched(const std::vector<std::unique_ptr<AnimationModel>>& models)
{
	// Compute パス（全体で一回）
	UAVManager::GetInstance()->PreDispatch();
	AnimationPipelineBuilder::GetInstance()->SetComputeSetting();
	for (auto& m : models) {
		if (!m) continue;
		if (m->IsVisible()) m->DispatchSkinningCS();
	}

	// Graphics パス（全体で一回）
	SRVManager::GetInstance()->PreDraw();
	AnimationPipelineBuilder::GetInstance()->SetRenderSetting();
	for (auto& m : models) {
		if (!m) continue;
		if (m->IsVisible()) m->DrawSkinned();
	}
}

void AnimationModel::DrawBatched(const std::vector<AnimationModel*>& models)
{
	UAVManager::GetInstance()->PreDispatch();
	AnimationPipelineBuilder::GetInstance()->SetComputeSetting();
	for (auto* m : models) {
		if (!m) continue;
		if (m->IsVisible()) m->DispatchSkinningCS();
	}
	SRVManager::GetInstance()->PreDraw();
	AnimationPipelineBuilder::GetInstance()->SetRenderSetting();
	for (auto* m : models) {
		if (!m) continue;
		if (m->IsVisible()) m->DrawSkinned();
	}
}

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

void AnimationModel::Clear()
{
	animationMesh_.reset();
	skeleton_.reset();
	for (auto& sc : skinClusterLOD_) { sc.reset(); }

	wvpResource.Reset();
	cameraResource.Reset();

	wvpData_ = nullptr;
	cameraData = nullptr;

	modelData = {};       // モデルデータ初期化
	animation = {};       // アニメーション初期化
	animationTime_ = 0.0f;
	bodyPartColliders_.clear();
}

void AnimationModel::DrawSkeletonWireframe()
{
#ifdef _DEBUG
	if (!skeleton_) { return; }

	const auto& joints = skeleton_->GetJoints();
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

	for (const auto& joint : joints) {
		if (joint.parent.has_value()) {
			const auto& parentJoint = joints[*joint.parent];

			Vector3 parentLocal = parentJoint.skeletonSpaceMatrix.GetTranslation();
			Vector3 jointLocal = joint.skeletonSpaceMatrix.GetTranslation();

			Vector3 parentPos = Vector3::Transform(parentLocal, worldMatrix);
			Vector3 jointPos = Vector3::Transform(jointLocal, worldMatrix);

			Wireframe::GetInstance()->DrawLine(parentPos, jointPos, { 1.0f, 0.0f, 0.0f, 1.0f });
		}
	}
#endif // _DEBUG
}

void AnimationModel::DrawBodyPartColliders()
{
	if (!skeleton_) { return; }

	const auto& joints = skeleton_->GetJoints();
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

	for (const auto& part : bodyPartColliders_) {
		if (part.endJointIndex < 0) {
			// スフィア用
			const auto& joint = joints[part.startJointIndex];
			Vector3 localPos = joint.skeletonSpaceMatrix.GetTranslation() + part.offset;
			Vector3 worldPos = Vector3::Transform(localPos, worldMatrix);

			Wireframe::GetInstance()->DrawSphere(worldPos, part.radius, { 0.0f, 1.0f, 0.0f, 1.0f });
		}
		else {
			// カプセル用
			const auto& jointA = joints[part.startJointIndex];
			const auto& jointB = joints[part.endJointIndex];

			Vector3 a = Vector3::Transform(jointA.skeletonSpaceMatrix.GetTranslation(), worldMatrix);
			Vector3 b = Vector3::Transform(jointB.skeletonSpaceMatrix.GetTranslation(), worldMatrix);

			Vector3 center = (a + b) * 0.5f;
			Vector3 axis = Vector3::Normalize(b - a);
			float height = Vector3::Length(b - a);

			Wireframe::GetInstance()->DrawCapsule(center, part.radius, height, axis, 8, { 0.0f, 1.0f, 0.0f, 1.0f });
		}
	}
}

std::vector<std::pair<std::string, Capsule>> AnimationModel::GetBodyPartCapsulesWorld() const
{
	std::vector<std::pair<std::string, Capsule>> out;
	if (!skeleton_) { return out; }

	const auto& joints = skeleton_->GetJoints();
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

	for (const auto& part : bodyPartColliders_)
	{
		Capsule capsule{};
		capsule.radius = part.radius;

		if (part.endJointIndex < 0) {
			// Sphere → pointA = pointB
			const Vector3  local = joints[part.startJointIndex].skeletonSpaceMatrix.GetTranslation() + part.offset;
			Vector3 world = Vector3::Transform(local, worldMatrix);
			capsule.segment.origin = capsule.segment.diff = world;
		}
		else {
			// カプセル → 始点と終点両方に回転適用
			Vector3 a = Vector3::Transform(joints[part.startJointIndex].skeletonSpaceMatrix.GetTranslation(), worldMatrix);
			Vector3 b = Vector3::Transform(joints[part.endJointIndex].skeletonSpaceMatrix.GetTranslation(), worldMatrix);
			capsule.segment.origin = a;
			capsule.segment.diff = b;
		}
		out.emplace_back(part.name, capsule);
	}
	return out;
}

std::vector<std::pair<std::string, Sphere>> AnimationModel::GetBodyPartSpheresWorld() const
{
	std::vector<std::pair<std::string, Sphere>> out;
	if (!skeleton_) { return out; }

	const auto& joints = skeleton_->GetJoints();
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(
		worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

	for (const auto& part : bodyPartColliders_) {
		if (part.endJointIndex < 0) {
			Sphere s{};
			Vector3 local = joints[part.startJointIndex].skeletonSpaceMatrix.GetTranslation() + part.offset;
			s.center = Vector3::Transform(local, worldMatrix);
			s.radius = part.radius;
			out.emplace_back(part.name, s);
		}
	}
	return out;
}


void AnimationModel::InitializeLODs()
{
	auto* device = dxCommon_->GetDevice();
	auto* cl = dxCommon_->GetCommandManager()->GetCommandList();

	// LOD 入力を決定：指定が無ければ「単一」、あればその数だけ
	std::vector<std::string> files = lodSourceFiles_.empty()
		? std::vector<std::string>{ fileName_ }    // 単一 LOD
	: lodSourceFiles_;                          // 指定された LOD 群

	// 書き込み前に必ずサイズ確保
	lods_.clear();
	lods_.resize(files.size());
	skinClusterLOD_.resize(files.size());
	lodFileName_.resize(files.size());

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
			D3D12_HEAP_PROPERTIES heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			D3D12_RESOURCE_DESC   bufDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

			HRESULT hr = device->CreateCommittedResource(
				&heapDefault, D3D12_HEAP_FLAG_NONE, &bufDesc,
				D3D12_RESOURCE_STATE_COMMON, nullptr,
				IID_PPV_ARGS(&defaultVB));
			assert(SUCCEEDED(hr));

			// Upload 経由でコピー
			ComPtr<ID3D12Resource> upload = ResourceManager::CreateBufferResource(device, vbSize);
			void* p = nullptr; upload->Map(0, nullptr, &p);
			std::memcpy(p, flat.vertices.data(), vbSize);
			upload->Unmap(0, nullptr);

			dxCommon_->ResourceTransition(defaultVB.Get(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
			cl->CopyBufferRegion(defaultVB.Get(), 0, upload.Get(), 0, vbSize);
			dxCommon_->ResourceTransition(defaultVB.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

			dxCommon_->GetCommandManager()->ExecuteAndWait();
		}

		// UAVヒープに t1 SRV を作成
		uint32_t t1Index = UAVManager::GetInstance()->Allocate();
		UAVManager::GetInstance()->CreateSRVForStructureBuffer(
			t1Index, defaultVB.Get(),
			static_cast<UINT>(flat.vertices.size()),
			sizeof(VertexData));

		// --- t2: SkinCluster（LOD ごとに作成） ---
		skinClusterLOD_[i] = std::make_unique<SkinCluster>();
		skinClusterLOD_[i]->Initialize(md, *skeleton_);

		// --- IB: DEFAULT で作成 ---
		const UINT ibSize = UINT(sizeof(uint32_t) * flat.indices.size());
		ComPtr<ID3D12Resource> defaultIB;
		{
			D3D12_HEAP_PROPERTIES heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			D3D12_RESOURCE_DESC   bufDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);

			HRESULT hr = device->CreateCommittedResource(
				&heapDefault, D3D12_HEAP_FLAG_NONE, &bufDesc,
				D3D12_RESOURCE_STATE_COMMON, nullptr,
				IID_PPV_ARGS(&defaultIB));
			assert(SUCCEEDED(hr));

			ComPtr<ID3D12Resource> upload = ResourceManager::CreateBufferResource(device, ibSize);
			void* p = nullptr; upload->Map(0, nullptr, &p);
			std::memcpy(p, flat.indices.data(), ibSize);
			upload->Unmap(0, nullptr);

			dxCommon_->ResourceTransition(defaultIB.Get(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
			cl->CopyBufferRegion(defaultIB.Get(), 0, upload.Get(), 0, ibSize);
			dxCommon_->ResourceTransition(defaultIB.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

			dxCommon_->GetCommandManager()->ExecuteAndWait();
		}
		D3D12_INDEX_BUFFER_VIEW ibv{};
		ibv.BufferLocation = defaultIB->GetGPUVirtualAddress();
		ibv.Format = DXGI_FORMAT_R32_UINT;
		ibv.SizeInBytes = static_cast<UINT>(defaultIB->GetDesc().Width);

		// --- u0: 出力頂点（UAV） & VBV ---
		ComPtr<ID3D12Resource> skinnedVB;
		{
			D3D12_HEAP_PROPERTIES heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			D3D12_RESOURCE_DESC   desc = CD3DX12_RESOURCE_DESC::Buffer(
				sizeof(VertexData) * flat.vertices.size(),
				D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

			HRESULT hr = device->CreateCommittedResource(
				&heapDefault, D3D12_HEAP_FLAG_NONE, &desc,
				D3D12_RESOURCE_STATE_COMMON, nullptr,
				IID_PPV_ARGS(&skinnedVB));
			assert(SUCCEEDED(hr));
		}
		uint32_t u0Index = UAVManager::GetInstance()->Allocate();
		UAVManager::GetInstance()->CreateUAVForStructuredBuffer(
			u0Index, skinnedVB.Get(),
			static_cast<UINT>(flat.vertices.size()),
			sizeof(VertexData));

		D3D12_VERTEX_BUFFER_VIEW skinnedVBV{};
		skinnedVBV.BufferLocation = skinnedVB->GetGPUVirtualAddress();
		skinnedVBV.SizeInBytes = UINT(sizeof(VertexData) * flat.vertices.size());
		skinnedVBV.StrideInBytes = sizeof(VertexData);

		// --- LODEntry へ格納 ---
		auto& L = lods_[i];
		L.staticVBDefault = defaultVB;
		L.srvInputVerticesOnUavHeap = t1Index;
		L.influenceSrvGpuOnUavHeap = skinClusterLOD_[i]->GetInfluenceSrvOnUAVHeap();
		L.indexBuffer = defaultIB;
		L.ibv = ibv;
		L.skinnedVB = skinnedVB;
		L.skinnedVBV = skinnedVBV;
		L.uavIndex = u0Index;
		L.vertexCount = static_cast<uint32_t>(flat.vertices.size());
		L.indexCount = static_cast<uint32_t>(flat.indices.size());
		L.skinnedState = D3D12_RESOURCE_STATE_COMMON;

		// サブメッシュ範囲とマテリアルSRVを設定
		L.subMeshRanges = flat.ranges;
		for (int si = 0; si < L.subMeshRanges.size(); ++si)
		{
			const auto& sm = md.subMeshes[si];
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
	if (skeleton_ && csCBMapped_ && !skeleton_->GetJoints().empty() && csCBMapped_->isSkinning)
	{
		Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);
		Matrix4x4 worldViewProjectionMatrix;

		if (camera_)
		{
			const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
			worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjectionMatrix);
		}
		else
		{
			worldViewProjectionMatrix = worldMatrix;
		}

		wvpData_->World = worldMatrix;
		wvpData_->WVP = worldViewProjectionMatrix;
		wvpData_->WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(worldMatrix));
	}
	else
	{
		// アニメーション無し（通常モデル用のWVP更新）
		NodeAnimation& rootNodeAnimation = animation.nodeAnimations[modelData.rootNode.name];
		Vector3 translate = CalculateValue(rootNodeAnimation.translate, animationTime_);
		Quaternion rotate = CalculateValue(rootNodeAnimation.rotate, animationTime_);
		Vector3 scale = CalculateValue(rootNodeAnimation.scale, animationTime_);

		Matrix4x4 localMatrix = Matrix4x4::MakeAffineMatrix(scale, rotate, translate);

		Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);
		Matrix4x4 worldViewProjectionMatrix;

		if (camera_)
		{
			const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
			worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjectionMatrix);
		}
		else
		{
			worldViewProjectionMatrix = worldMatrix;
		}

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
			keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
			keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z };
			nodeAnimation.scale.push_back(keyframe);
		}
	}
	// 解析終了
	return animation;
}

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
	switch (lodIndex_) {
	case 0:                if (dist > outThresh(0)) newLOD = 1; break;
	default:
		if (lodIndex_ > 0) {
			if (dist < inThresh(lodIndex_ - 1)) newLOD = lodIndex_ - 1;
			else if (lodIndex_ < last && dist > outThresh(lodIndex_)) newLOD = lodIndex_ + 1;
		}
		break;
	}

	if (newLOD != lodIndex_) {
		lodIndex_ = newLOD;
		if (csCBMapped_) { csCBMapped_->numVertices = lods_[lodIndex_].vertexCount; }
	}

	// ▼ ここが追加：最終 LOD の“出しきい値 + 余白”を越えたらカリング
	const float lastOut = outThresh(last);        // 例）LOD3の出しきい値 = 10 + 15*3 + 2 = 57
	culledByDistance_ = (dist > lastOut + farCullExtra_); // 例）57 + 20 = 77 より遠いとカリング
}

void AnimationModel::DispatchSkinningCS()
{
	if (culledByDistance_) { return; } // ★ 追加：遠距離はスキニング自体しない

	auto* cl = dxCommon_->GetCommandManager()->GetCommandList();
	auto& L = lods_[lodIndex_];

	// ★毎回、現在LODの頂点数に更新（ここが無いと“半分だけ更新”が起きる）
	csCBMapped_->numVertices = L.vertexCount;

	// 実状態 → UAV
	if (L.skinnedState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
		dxCommon_->ResourceTransition(
			L.skinnedVB.Get(),
			L.skinnedState,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
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
	if (L.skinnedState != D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER) {
		dxCommon_->ResourceTransition(
			L.skinnedVB.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		L.skinnedState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	}
}

void AnimationModel::DrawSkinned()
{
	if (culledByDistance_) { return; } // ★ 追加：描画もしない

	auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
	auto& L = lods_[lodIndex_];

	TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 4, environmentMapHandle_); // t4: 環境マップ

	// VB/IB
	if (csCBMapped_->isSkinning)
	{
		commandList->IASetVertexBuffers(0, 1, &L.skinnedVBV);  // ← 1本だけ
		commandList->IASetIndexBuffer(&L.ibv);

		material_.SetPipeline();

		// RootParam #1 : WVP（b?）
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
		// ★ 非CS: AnimationMesh が複数VB/IB対応済み前提でループ
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
