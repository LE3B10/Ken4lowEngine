#include "GpuParticleBuffers.h"
#include <DirectXCommon.h>
#include <ResourceManager.h>
#include <LogString.h>
#include <SRVManager.h>
#include <UAVManager.h>
#include <Camera.h>
#include <DebugCamera.h>

/// -------------------------------------------------------------
///			　　　	初期化処理
/// -------------------------------------------------------------
void GpuParticleBuffers::Initialize(Camera* camera)
{
	// 引数でカメラのポインタを受け取ってメンバ変数に記録する
	camera_ = camera;

	// パーティクルバッファの生成
	CreateParticleBuffer();

	// ビュー行列と射影行列バッファの生成
	CreatePerViewBuffer();

	// エミッターバッファの生成
	CreateEmitterBuffer();

	// 時間計測用バッファの生成
	CreatePerFrameBuffer();

	// フリーリストインデックスバッファの生成
	CreateFreeListIndexBuffer();

	// フリーリストバッファの生成
	CreateFreeListBuffer();
}

/// -------------------------------------------------------------
///			　　　			更新処理
/// -------------------------------------------------------------
void GpuParticleBuffers::Update()
{
	// ビュー行列とプロジェクション行列をカメラから取得
	Matrix4x4 cameraMatrix = Matrix4x4::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, camera_->GetRotate(), camera_->GetTranslate());
	Matrix4x4 viewMatrix = Matrix4x4::Inverse(cameraMatrix);
	Matrix4x4 projectionMatrix = camera_->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = Matrix4x4::Multiply(viewMatrix, projectionMatrix);

	// デバッグカメラが有効ならそちらを使う
	if (isDebugCamera_)
	{
#ifdef _DEBUG
		debugViewProjectionMatrix_ = DebugCamera::GetInstance()->GetViewProjectionMatrix();
		viewProjectionMatrix = debugViewProjectionMatrix_;
#endif
	}

	// ビルボード用行列（回転行列のみ）
	Matrix4x4 billboardMatrix = cameraMatrix;
	billboardMatrix.m[3][0] = billboardMatrix.m[3][1] = billboardMatrix.m[3][2] = 0.0f;

	perViewData_->viewProjectionMatrix = viewProjectionMatrix;
	perViewData_->billboardMatrix = Matrix4x4::Transpose(Matrix4x4::Inverse(billboardMatrix));
	perViewData_->billboardMode = static_cast<uint32_t>(BillboardMode::Camera); // とりあえず常にカメラ

	// エミッターの更新
	perFrameData_->time += perFrameData_->deltaTime;

	emitterData_->frequencyTime += perFrameData_->deltaTime; // 仮に60FPS固定で更新

	if (emitterData_->frequency <= emitterData_->frequencyTime)
	{
		emitterData_->frequencyTime -= emitterData_->frequency;
		emitterData_->emit = 1; // 発生フラグON
	}
	else
	{
		emitterData_->emit = 0; // 発生フラグOFF
	}
}

/// -------------------------------------------------------------
///			　	パーティクルバッファの生成
/// -------------------------------------------------------------
void GpuParticleBuffers::CreateParticleBuffer()
{
	// リソースを生成
	particleBuffer_ = ResourceManager::CreateBufferResource(
		DirectXCommon::GetInstance()->GetDevice(), sizeof(ParticleCS) * kMaxParticles,
		D3D12_HEAP_TYPE_DEFAULT,					// デフォルトヒープ
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,	// UAVとして使用するためのフラグ
		D3D12_RESOURCE_STATE_COMMON					// UAV用は COMMON ステートで作成
	);

	// SRVの生成
	particleSrvIndex_ = SRVManager::GetInstance()->Allocate();
	SRVManager::GetInstance()->CreateSRVForStructureBuffer(particleSrvIndex_, particleBuffer_.Get(), kMaxParticles, sizeof(ParticleCS));

	// UAVの生成
	particleUavIndex_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateUAVForStructuredBuffer(particleUavIndex_, particleBuffer_.Get(), kMaxParticles, sizeof(ParticleCS));
}

/// -------------------------------------------------------------
///		　		ビュー行列と射影行列バッファの生成
/// -------------------------------------------------------------
void GpuParticleBuffers::CreatePerViewBuffer()
{
	auto* device = DirectXCommon::GetInstance()->GetDevice();
	UINT size = (sizeof(PerView) + 255) & ~255u; // 256byteアライン

	perViewBuffer_ = ResourceManager::CreateBufferResource(device, size);

	perViewBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));
	perViewData_->viewProjectionMatrix = Matrix4x4::MakeIdentity();
	perViewData_->billboardMatrix = Matrix4x4::MakeIdentity();
	perViewData_->billboardMode = static_cast<uint32_t>(BillboardMode::Camera);
}

/// -------------------------------------------------------------
///			　		エミッターバッファの生成
/// -------------------------------------------------------------
void GpuParticleBuffers::CreateEmitterBuffer()
{
	// 今回は球体エミッターのみ対応
	emitterBuffer_ = ResourceManager::CreateBufferResource(DirectXCommon::GetInstance()->GetDevice(), sizeof(EmitterSphere));

	// マッピング
	emitterBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&emitterData_));

	// 初期化
	emitterData_->count = 10;
	emitterData_->frequency = 0.5f;
	emitterData_->frequencyTime = 0.0f;
	emitterData_->translate = { 0.0f, 2.0f, 0.0f };
	emitterData_->radius = 1.0f;
	emitterData_->emit = 1; // 発生フラグON
}

/// -------------------------------------------------------------
///			　		時間計測用バッファの生成
/// -------------------------------------------------------------
void GpuParticleBuffers::CreatePerFrameBuffer()
{
	perFrameBuffer_ = ResourceManager::CreateBufferResource(DirectXCommon::GetInstance()->GetDevice(), sizeof(PerFrame));
	perFrameBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&perFrameData_));

	// 初期化
	perFrameData_->time = 0.0f;
	perFrameData_->deltaTime = 1.0f / 60.0f; // 仮に60FPS固定で更新
}

/// -------------------------------------------------------------
///			　		フリーカウンターバッファの生成
/// -------------------------------------------------------------
void GpuParticleBuffers::CreateFreeListIndexBuffer()
{
	auto* device = DirectXCommon::GetInstance()->GetDevice();

	// リソースを生成
	freeListIndexBuffer_ = ResourceManager::CreateBufferResource(
		device,
		sizeof(int32_t),
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON
	);

	// UAVの生成
	freeListIndexUavIndex_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateUAVForStructuredBuffer(freeListIndexUavIndex_, freeListIndexBuffer_.Get(), 1, sizeof(int32_t));
}

/// -------------------------------------------------------------
///			　		フリーリストバッファの生成
/// -------------------------------------------------------------
void GpuParticleBuffers::CreateFreeListBuffer()
{
	auto* device = DirectXCommon::GetInstance()->GetDevice();

	// リソースを生成
	freeListBuffer_ = ResourceManager::CreateBufferResource(
		device,
		sizeof(int32_t) * kMaxParticles,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON
	);

	// UAVの生成
	freeListUavIndex_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateUAVForStructuredBuffer(freeListUavIndex_, freeListBuffer_.Get(), kMaxParticles, sizeof(int32_t));
}
