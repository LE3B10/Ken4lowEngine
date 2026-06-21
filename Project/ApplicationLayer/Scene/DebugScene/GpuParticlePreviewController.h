#pragma once

#include "GpuParticleEffectDesc.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class GpuParticleEmitter;

	/// <summary>
	/// ImGuiで編集中のEmitterDescをPreview Runtimeへ渡すための安全な中間設定です。
	/// 不正値の補正と、Preview Positionを含む実際の発生値を一か所へ集約します。
	/// </summary>
	struct GpuParticlePreviewSpawnSettings
	{
		uint32_t maxParticles = 1024;
		uint32_t emitCount = 1;
		Vector3 position{};
		Vector3 positionRandom{};
		Vector3 velocity{};
		Vector3 velocityRandom{};
		Vector3 gravity{};
		float damping = 0.0f;
		float speed = 0.0f;
		float speedRandom = 0.0f;
		float lifeTime = 1.0f;
		float lifeTimeRandom = 0.0f;
		Vector2 startSize{ 1.0f, 1.0f };
		Vector2 endSize{ 1.0f, 1.0f };
		float sizeRandom = 0.0f;
		Vector4 startColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 endColor{ 1.0f, 1.0f, 1.0f, 0.0f };
		Vector4 colorRandom{};
		bool alphaFade = true;
		float startRotation = 0.0f;
		float rotationSpeed = 0.0f;
		float rotationRandom = 0.0f;
		GpuParticleSpawnShape spawnShape = GpuParticleSpawnShape::Point;
		float spawnRadius = 0.0f;
		Vector3 spawnBoxSize{};
		GpuParticleBlendMode blendMode = GpuParticleBlendMode::Alpha;
		std::string texturePath;
	};

	/// <summary>現在のEmitterDescを、Preview発生に使用する補正済み設定へ変換します。</summary>
	GpuParticlePreviewSpawnSettings BuildPreviewSpawnSettings(
		const GpuParticleEmitterDesc& desc,
		const Vector3& previewPosition,
		uint32_t previewEmitCount,
		bool forceVisibleSprite);

	/// <summary>
	/// ImGuiで編集したEmitter設定を、実際のGPUパーティクルRuntime Emitterへ渡します。
	/// Preview専用の上書きフラグを使うため、既存EmitterのType別生成設定は変更しません。
	/// </summary>
	bool ApplyEmitterDescToRuntimeEmitter(
		const GpuParticleEmitterDesc& desc,
		GpuParticleEmitter& runtimeEmitter,
		const Vector3& previewPosition,
		uint32_t previewEmitCount,
		bool continuous,
		bool forceVisibleSprite,
		std::string& outWarning);
}

/// <summary>
/// DebugSceneだけでGPUパーティクル設定を試射するためのプレビュー制御クラスです。
/// Sprite/Mesh用のRuntime EmitterとMesh Assetを専用名・専用IDで所有し、本番Effectへ干渉させません。
/// </summary>
class GpuParticlePreviewController final
{
public:
	/// <summary>プレビュー状態を初期化します。</summary>
	void Initialize();

	/// <summary>フレームレートに依存しないspawnRate発生と、再生中Emitter設定の同期を行います。</summary>
	void Update(
		float deltaTime,
		const Ken4lowEngine::GpuParticleEffectDesc& effect,
		int selectedEmitterIndex,
		const Ken4lowEngine::Vector3& previewPosition,
		uint32_t emitCount,
		bool autoPlay,
		bool selectedOnly);

	/// <summary>プレビュー操作と、何も表示されない原因を確認しやすいStatusをImGuiへ表示します。</summary>
	void DrawImGui(
		const Ken4lowEngine::GpuParticleEffectDesc& effect,
		int selectedEmitterIndex,
		Ken4lowEngine::Vector3& previewPosition,
		uint32_t& emitCount,
		bool& autoPlay,
		bool& selectedOnly);

	/// <summary>選択中Emitter、またはEffect全体をDebugScene上で1回だけ指定数発射する確認用処理です。</summary>
	void EmitOnce(
		const Ken4lowEngine::GpuParticleEffectDesc& effect,
		int selectedEmitterIndex,
		const Ken4lowEngine::Vector3& previewPosition,
		uint32_t emitCount,
		bool selectedOnly);

	/// <summary>DebugSceneでパーティクル設定を確認するためのプレビュー再生を開始します。</summary>
	void Play(
		const Ken4lowEngine::GpuParticleEffectDesc& effect,
		int selectedEmitterIndex,
		const Ken4lowEngine::Vector3& previewPosition,
		uint32_t emitCount,
		bool selectedOnly);

	/// <summary>新規発生だけを止め、既に発生した粒子は寿命まで表示します。</summary>
	void Stop();

	/// <summary>DebugScene専用Runtime EmitterとMesh Assetを解除します。</summary>
	void Clear();

	bool IsPlaying() const { return playing_; }

private:
	struct RuntimeEmitterRecord
	{
		std::string runtimeName;
		size_t sourceEmitterIndex = 0;
		float spawnAccumulator = 0.0f;
		float elapsedTime = 0.0f; ///< loop=falseのduration判定に使うPreview再生時間です。
		std::vector<uint32_t> ownedMeshAssetIds;
	};

	std::vector<size_t> CollectTargetEmitterIndices(
		const Ken4lowEngine::GpuParticleEffectDesc& effect,
		int selectedEmitterIndex,
		bool selectedOnly);
	bool CreateRuntimeEmitter(
		const Ken4lowEngine::GpuParticleEmitterDesc& desc,
		size_t sourceEmitterIndex,
		const Ken4lowEngine::Vector3& previewPosition,
		uint32_t initialEmitCount,
		uint32_t& outAcceptedCount);
	uint32_t AllocateMeshAssetBaseId() const;
	uint32_t GetCurrentAliveParticleCount() const;
	void RemoveRuntimeEmitters();
	void SetStatus(const std::string& message, const std::string& error = {});

	std::vector<RuntimeEmitterRecord> runtimeEmitters_;
	uint64_t generation_ = 0;
	uint64_t emitButtonPressedCount_ = 0;
	uint64_t updateCountAtLastRequest_ = 0;
	uint64_t drawCountAtLastRequest_ = 0;
	uint64_t emitDispatchCountAtLastRequest_ = 0;
	bool playing_ = false;
	bool selectedIndexValid_ = false;
	bool runtimeSpawnCalled_ = false;
	bool hasRuntimeRequest_ = false;
	bool forceVisibleSprite_ = false;
	bool useBurstCountForEmitOnce_ = false;
	bool autoPlayAttempted_ = false;
	bool lastSelectedOnly_ = true;
	int lastSelectedEmitterIndex_ = -1;
	uint32_t lastEmitCount_ = 0;
	uint32_t lastEmitRequestedCount_ = 0;
	uint32_t lastEmitAcceptedCount_ = 0;
	uint32_t lastRequestedEmitterCount_ = 0;
	uint32_t lastConnectedEmitterCount_ = 0;
	uint32_t lastSpriteConnectedCount_ = 0;
	uint32_t lastMeshConnectedCount_ = 0;
	float lastSpawnAccumulator_ = 0.0f;
	Ken4lowEngine::Vector3 lastEmitPosition_{};
	Ken4lowEngine::Vector3 lastUsedVelocity_{};
	float lastUsedLifeTime_ = 0.0f;
	Ken4lowEngine::Vector2 lastUsedStartSize_{};
	Ken4lowEngine::Vector4 lastUsedStartColor_{};
	std::string lastUsedTexturePath_;
	std::string lastUsedMode_ = "Desc";
	std::string lastEmitCountSource_ = "Preview Emit Count";
	std::string lastStatus_ = "Preview ready.";
	std::string lastErrorMessage_;
};
