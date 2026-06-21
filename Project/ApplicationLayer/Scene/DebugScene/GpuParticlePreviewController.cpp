#define NOMINMAX
#include "GpuParticlePreviewController.h"

#include "GpuParticleBuffers.h"
#include "GpuParticleEmitter.h"
#include "GpuParticleManager.h"
#include "ModelPathResolver.h"
#include <LogString.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <string_view>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace K4E = ::Ken4lowEngine;

namespace
{
	constexpr const char* kFallbackTexture = "Effects/white.dds";
	constexpr uint32_t kMaxPreviewEmitOnce = 4096;
	constexpr uint32_t kMaxPreviewEmitPerFrame = 64;
	constexpr uint32_t kMeshAssetIdStart = 1'000'000;
	constexpr uint32_t kMeshAssetIdBlockSize = 256;

	void AppendMessage(std::string& destination, const std::string& message)
	{
		if (message.empty()) return;
		if (!destination.empty()) destination += " | ";
		destination += message;
	}

	std::string ResolveSafeTexturePath(const std::string& requestedPath, std::string& warning)
	{
		if (!requestedPath.empty())
		{
			std::error_code errorCode;
			const std::filesystem::path requested(requestedPath);
			if (std::filesystem::is_regular_file(requested, errorCode) && !errorCode)
			{
				return requested.generic_string();
			}

			errorCode.clear();
			const std::filesystem::path compiledCandidate =
				std::filesystem::path("Resources/Textures/Compiled") / requested;
			if (std::filesystem::is_regular_file(compiledCandidate, errorCode) && !errorCode)
			{
				return requested.generic_string();
			}

			constexpr std::string_view kOldTextureRoot = "Resources/Textures/";
			if (requestedPath.rfind(kOldTextureRoot, 0) == 0)
			{
				std::filesystem::path relative = requestedPath.substr(kOldTextureRoot.size());
				if (relative.generic_string().rfind("Compiled/", 0) != 0)
				{
					relative = std::filesystem::path("Compiled") / relative;
				}
				errorCode.clear();
				const auto candidate = std::filesystem::path("Resources/Textures") / relative;
				if (std::filesystem::is_regular_file(candidate, errorCode) && !errorCode)
				{
					return candidate.generic_string();
				}
			}
			AppendMessage(warning, "texturePathが見つからないためEffects/white.ddsを使用");
		}
		return kFallbackTexture;
	}

	bool ResolveMeshLogicalPath(const std::string& requestedPath, std::string& outLogicalPath)
	{
		if (requestedPath.empty()) return false;
		std::string normalized = requestedPath;
		std::replace(normalized.begin(), normalized.end(), '\\', '/');

		constexpr std::string_view kSourcesRoot = "Resources/Models/Sources/";
		constexpr std::string_view kModelsRoot = "Resources/Models/";
		if (normalized.rfind(kSourcesRoot, 0) == 0)
		{
			normalized.erase(0, kSourcesRoot.size());
		}
		else if (normalized.rfind(kModelsRoot, 0) == 0)
		{
			normalized.erase(0, kModelsRoot.size());
			if (normalized.rfind("Sources/", 0) == 0) normalized.erase(0, std::string_view("Sources/").size());
		}

		if (normalized.empty() || !K4E::ModelPathResolver::ExistsSource(normalized)) return false;
		outLogicalPath = normalized;
		return true;
	}

	K4E::GpuParticleEmitter::EmitterInfo BuildRuntimeEmitterInfo(
		const K4E::GpuParticleEmitterDesc& desc,
		const K4E::Vector3& previewPosition,
		uint32_t previewEmitCount,
		bool forceVisibleSprite,
		std::string& warning)
	{
		const auto settings = K4E::BuildPreviewSpawnSettings(desc, previewPosition, previewEmitCount, forceVisibleSprite);
		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.useDescSpawnOverride = true;
		// ImGuiで編集した値を固定値で上書きせず、補正済みPreview設定からRuntimeへ反映する。
		info.maxParticles = settings.maxParticles;
		info.positionRandom = settings.positionRandom;
		info.velocity = settings.velocity;
		info.velocityRandom = settings.velocityRandom;
		info.startSize = settings.startSize;
		info.endSize = settings.endSize;
		info.startColor = settings.startColor;
		info.endColor = settings.endColor;
		info.lifeTime = settings.lifeTime;
		info.lifeTimeRandom = settings.lifeTimeRandom;
		info.gravity = settings.gravity;
		info.damping = settings.damping;
		info.speed = settings.speed;
		info.speedRandom = settings.speedRandom;
		info.sizeRandom = settings.sizeRandom;
		info.startRotation = settings.startRotation;
		info.rotationSpeed = settings.rotationSpeed;
		info.rotationRandom = settings.rotationRandom;
		info.spawnShape = static_cast<uint32_t>(settings.spawnShape);
		info.spawnRadius = settings.spawnRadius;
		info.spawnBoxSize = settings.spawnBoxSize;
		info.colorRandom = settings.colorRandom;
		info.alphaFade = settings.alphaFade;
		info.startScale3D = desc.startScale3D;
		info.endScale3D = desc.endScale3D;
		info.useSpriteSheet = desc.useSpriteSheet;
		info.spriteSheetRows = static_cast<uint32_t>((std::max)(desc.spriteSheetRows, 1));
		info.spriteSheetColumns = static_cast<uint32_t>((std::max)(desc.spriteSheetColumns, 1));
		info.spriteSheetFrameRate = (std::max)(desc.spriteSheetFrameRate, 0.0f);
		info.lifeScale = 1.0f;
		info.speedScale = 1.0f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		if (desc.blendMode != K4E::GpuParticleBlendMode::Alpha)
		{
			AppendMessage(warning, "BlendModeは保存・編集のみ対応（Preview RuntimeはAlpha）");
		}
		if (desc.renderType == K4E::GpuParticleRenderType::Mesh &&
			(desc.angularVelocity.x != 0.0f || desc.angularVelocity.y != 0.0f || desc.angularVelocity.z != 0.0f ||
			 desc.angularVelocityRandom.x != 0.0f || desc.angularVelocityRandom.y != 0.0f || desc.angularVelocityRandom.z != 0.0f))
		{
			AppendMessage(warning, "Mesh angularVelocityは保存・編集のみ対応");
		}

		if (desc.renderType == K4E::GpuParticleRenderType::Mesh)
		{
			info.kind = K4E::GpuParticleKind::Mesh;
			info.spriteType = K4E::GpuParticleType::Debris; // Mesh VSのdead判定を避け、PSのdrawTypeと一致させる。
			info.billboardFlags = K4E::BillboardMode::None;
		}
		else
		{
			info.textureFilePath = ResolveSafeTexturePath(settings.texturePath, warning);
			info.kind = K4E::GpuParticleKind::Sprite;
			info.spriteType = K4E::GpuParticleType::Default;
			info.billboardFlags = desc.billboard ? K4E::BillboardMode::Camera : K4E::BillboardMode::None;

			if (forceVisibleSprite)
			{
				info.billboardFlags = K4E::BillboardMode::Camera;
				AppendMessage(warning, "Force Visible Sprite: Effects/white.dds / size 0.5 / lifetime 2 sec");
			}
		}
		return info;
	}
}

namespace Ken4lowEngine
{
	GpuParticlePreviewSpawnSettings BuildPreviewSpawnSettings(
		const GpuParticleEmitterDesc& desc,
		const Vector3& previewPosition,
		uint32_t previewEmitCount,
		bool forceVisibleSprite)
	{
		GpuParticlePreviewSpawnSettings settings{};
		settings.maxParticles = (std::min)(desc.maxParticles == 0 ? 1024u : desc.maxParticles, GpuParticleBuffers::GetMaxParticles());
		settings.emitCount = (std::min)({ (std::max)(previewEmitCount, 1u), settings.maxParticles, kMaxPreviewEmitOnce });
		settings.position = { previewPosition.x + desc.position.x, previewPosition.y + desc.position.y, previewPosition.z + desc.position.z };
		settings.positionRandom = { std::abs(desc.positionRandom.x), std::abs(desc.positionRandom.y), std::abs(desc.positionRandom.z) };
		settings.velocity = desc.velocity;
		settings.velocityRandom = { std::abs(desc.velocityRandom.x), std::abs(desc.velocityRandom.y), std::abs(desc.velocityRandom.z) };
		settings.gravity = desc.gravity;
		settings.damping = (std::max)(desc.damping, 0.0f);
		settings.speed = (std::max)(desc.speed, 0.0f);
		settings.speedRandom = (std::max)(desc.speedRandom, 0.0f);
		settings.lifeTime = (std::max)(desc.lifeTime, 0.01f);
		settings.lifeTimeRandom = (std::max)(desc.lifeTimeRandom, 0.0f);
		settings.startSize = { (std::max)(desc.startSize.x, 0.0001f), (std::max)(desc.startSize.y, 0.0001f) };
		settings.endSize = { (std::max)(desc.endSize.x, 0.0001f), (std::max)(desc.endSize.y, 0.0001f) };
		settings.sizeRandom = (std::max)(desc.sizeRandom, 0.0f);
		settings.startColor = desc.startColor;
		settings.endColor = desc.endColor;
		settings.colorRandom = desc.colorRandom;
		settings.alphaFade = desc.alphaFade;
		settings.startRotation = desc.startRotation;
		settings.rotationSpeed = desc.rotationSpeed;
		settings.rotationRandom = (std::max)(desc.rotationRandom, 0.0f);
		settings.spawnShape = desc.spawnShape;
		settings.spawnRadius = (std::max)(desc.spawnRadius, 0.0f);
		settings.spawnBoxSize = { std::abs(desc.spawnBoxSize.x), std::abs(desc.spawnBoxSize.y), std::abs(desc.spawnBoxSize.z) };
		settings.blendMode = desc.blendMode;
		settings.texturePath = desc.texturePath;

		if (forceVisibleSprite && desc.renderType == GpuParticleRenderType::Sprite)
		{
			// Runtime接続確認用の強制表示モード。この分岐だけがDescを無視した固定値を使用する。
			settings.position = previewPosition;
			settings.positionRandom = {};
			settings.velocity = { 0.0f, 1.0f, 0.0f };
			settings.velocityRandom = {};
			settings.gravity = {};
			settings.damping = 0.0f;
			settings.lifeTime = 2.0f;
			settings.lifeTimeRandom = 0.0f;
			settings.startSize = { 0.5f, 0.5f };
			settings.endSize = { 0.5f, 0.5f };
			settings.sizeRandom = 0.0f;
			settings.startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
			settings.endColor = { 1.0f, 1.0f, 1.0f, 0.0f };
			settings.colorRandom = {};
			settings.startRotation = 0.0f;
			settings.rotationSpeed = 0.0f;
			settings.rotationRandom = 0.0f;
			settings.spawnShape = GpuParticleSpawnShape::Point;
			settings.spawnRadius = 0.0f;
			settings.spawnBoxSize = {};
			settings.texturePath = kFallbackTexture;
		}
		return settings;
	}

	bool ApplyEmitterDescToRuntimeEmitter(
		const GpuParticleEmitterDesc& desc,
		GpuParticleEmitter& runtimeEmitter,
		const Vector3& previewPosition,
		uint32_t previewEmitCount,
		bool continuous,
		bool forceVisibleSprite,
		std::string& outWarning)
	{
		(void)continuous;
		outWarning.clear();
		const auto previousInfo = runtimeEmitter.GetInfo();
		auto info = BuildRuntimeEmitterInfo(desc, previewPosition, previewEmitCount, forceVisibleSprite, outWarning);
		const auto settings = BuildPreviewSpawnSettings(desc, previewPosition, previewEmitCount, forceVisibleSprite);

		// Mesh Asset IDはControllerが所有するため、毎フレームのDesc同期でも選択済みIDを維持する。
		if (desc.renderType == GpuParticleRenderType::Mesh)
		{
			if (previousInfo.textureFilePath.rfind("Mesh:", 0) != 0)
			{
				outWarning = "Mesh Runtime Asset IDが設定されていません";
				return false;
			}
			info.textureFilePath = previousInfo.textureFilePath;
		}

		runtimeEmitter.GetInfoMutable() = info;
		runtimeEmitter.SetPosition(settings.position);
		return true;
	}
}

void GpuParticlePreviewController::Initialize()
{
	RemoveRuntimeEmitters();
	hasRuntimeRequest_ = false;
	runtimeSpawnCalled_ = false;
	lastStatus_ = "Preview ready. Sprite / Mesh Runtime connected.";
	lastErrorMessage_.clear();
}

std::vector<size_t> GpuParticlePreviewController::CollectTargetEmitterIndices(
	const K4E::GpuParticleEffectDesc& effect,
	int selectedEmitterIndex,
	bool selectedOnly)
{
	std::vector<size_t> indices;
	if (selectedOnly)
	{
		if (selectedEmitterIndex >= 0 && selectedEmitterIndex < static_cast<int>(effect.emitters.size()))
		{
			indices.push_back(static_cast<size_t>(selectedEmitterIndex));
		}
		return indices;
	}

	indices.reserve(effect.emitters.size());
	for (size_t index = 0; index < effect.emitters.size(); ++index) indices.push_back(index);
	return indices;
}

uint32_t GpuParticlePreviewController::AllocateMeshAssetBaseId() const
{
	auto* manager = K4E::GpuParticleManager::GetInstance();
	for (uint32_t baseId = kMeshAssetIdStart; baseId < 0xF0000000u; baseId += kMeshAssetIdBlockSize)
	{
		bool isFree = true;
		for (uint32_t offset = 0; offset < kMeshAssetIdBlockSize; ++offset)
		{
			if (manager->FindMeshAsset(baseId + offset)) { isFree = false; break; }
		}
		if (isFree) return baseId;
	}
	return 0;
}

bool GpuParticlePreviewController::CreateRuntimeEmitter(
	const K4E::GpuParticleEmitterDesc& desc,
	size_t sourceEmitterIndex,
	const K4E::Vector3& previewPosition,
	uint32_t initialEmitCount,
	uint32_t& outAcceptedCount)
{
	outAcceptedCount = 0;
	std::string warning;
	const auto previewSettings = K4E::BuildPreviewSpawnSettings(desc, previewPosition, initialEmitCount, forceVisibleSprite_);
	auto info = BuildRuntimeEmitterInfo(desc, previewPosition, initialEmitCount, forceVisibleSprite_, warning);
	RuntimeEmitterRecord record{};
	record.sourceEmitterIndex = sourceEmitterIndex;

	if (desc.renderType == K4E::GpuParticleRenderType::Mesh)
	{
		std::string logicalMeshPath;
		if (!ResolveMeshLogicalPath(desc.meshPath, logicalMeshPath))
		{
			AppendMessage(lastErrorMessage_, desc.name + ": meshPathが空、またはResources/Models/Sourcesに存在しません");
			return false;
		}

		const uint32_t baseMeshId = AllocateMeshAssetBaseId();
		if (baseMeshId == 0)
		{
			AppendMessage(lastErrorMessage_, desc.name + ": Preview用Mesh Asset IDを確保できません");
			return false;
		}

		try
		{
			// モデル内テクスチャの不正パスでassertしないよう、Meshだけ読み込み安全確認済みtexturePathを後から設定する。
			if (!K4E::GpuParticleManager::GetInstance()->LoadMeshAssetsFromAssimp(baseMeshId, logicalMeshPath, false))
			{
				AppendMessage(lastErrorMessage_, desc.name + ": Mesh Asset読み込みに失敗しました");
				return false;
			}
		}
		catch (const std::exception& error)
		{
			AppendMessage(lastErrorMessage_, desc.name + ": Mesh読み込み失敗: " + error.what());
			return false;
		}

		for (uint32_t offset = 0; offset < kMeshAssetIdBlockSize; ++offset)
		{
			const uint32_t meshId = baseMeshId + offset;
			if (K4E::GpuParticleManager::GetInstance()->FindMeshAsset(meshId)) record.ownedMeshAssetIds.push_back(meshId);
		}
		if (record.ownedMeshAssetIds.empty())
		{
			AppendMessage(lastErrorMessage_, desc.name + ": 有効なSubMeshがありません");
			return false;
		}

		const std::string safeTexture = ResolveSafeTexturePath(desc.texturePath, warning);
		for (const uint32_t meshId : record.ownedMeshAssetIds)
		{
			K4E::GpuParticleManager::GetInstance()->SetMeshAssetTexturePath(meshId, safeTexture);
		}
		info.textureFilePath = "Mesh:" + std::to_string(record.ownedMeshAssetIds.front());
		if (record.ownedMeshAssetIds.size() > 1)
		{
			AppendMessage(warning, "複数SubMeshのうち先頭をPreview表示");
		}
	}

	record.runtimeName = "__DebugGpuPreview_" + std::to_string(generation_) + "_" + std::to_string(sourceEmitterIndex);
	auto* emitter = K4E::GpuParticleManager::GetInstance()->CreateRuntimeEmitter(record.runtimeName, info);
	if (!emitter)
	{
		for (const uint32_t meshId : record.ownedMeshAssetIds) K4E::GpuParticleManager::GetInstance()->UnregisterMeshAsset(meshId);
		AppendMessage(lastErrorMessage_, desc.name + ": Runtime Emitterを作成できませんでした");
		return false;
	}

	emitter->SetPosition(previewSettings.position);
	lastEmitPosition_ = previewSettings.position;
	lastUsedVelocity_ = info.velocity;
	lastUsedLifeTime_ = info.lifeTime;
	lastUsedStartSize_ = info.startSize;
	lastUsedStartColor_ = info.startColor;
	lastUsedTexturePath_ = info.textureFilePath;
	lastUsedMode_ = forceVisibleSprite_ && desc.renderType == K4E::GpuParticleRenderType::Sprite ? "ForceVisible" : "Desc";
	// ImGuiのPreviewボタンからRuntimeへ発生要求を渡す確認用処理。受理数をStatusへ返す。
	runtimeSpawnCalled_ = true;
	outAcceptedCount = emitter->RequestEmit(initialEmitCount);
	if (outAcceptedCount == 0)
	{
		AppendMessage(lastErrorMessage_, desc.name + ": Runtimeが発生要求を受理しませんでした（maxParticlesまたは生存上限）");
	}
	runtimeEmitters_.push_back(std::move(record));
	AppendMessage(lastErrorMessage_, warning);
	return true;
}

void GpuParticlePreviewController::EmitOnce(
	const K4E::GpuParticleEffectDesc& effect,
	int selectedEmitterIndex,
	const K4E::Vector3& previewPosition,
	uint32_t emitCount,
	bool selectedOnly)
{
	++emitButtonPressedCount_;
	auto* runtimeManager = K4E::GpuParticleManager::GetInstance();
	updateCountAtLastRequest_ = runtimeManager->GetUpdateCallCount();
	drawCountAtLastRequest_ = runtimeManager->GetDrawCallCount();
	emitDispatchCountAtLastRequest_ = runtimeManager->GetEmitDispatchCount();
	runtimeSpawnCalled_ = false;
	hasRuntimeRequest_ = true;
	lastEmitRequestedCount_ = 0;
	lastEmitAcceptedCount_ = 0;
	lastEmitPosition_ = previewPosition;
	K4E::Log("[GpuParticlePreview] Emit Once pressed\n");
	RemoveRuntimeEmitters();
	++generation_;
	playing_ = false;
	lastErrorMessage_.clear();
	lastEmitCount_ = 0;
	lastSpawnAccumulator_ = 0.0f;
	lastSpriteConnectedCount_ = 0;
	lastMeshConnectedCount_ = 0;
	const auto indices = CollectTargetEmitterIndices(effect, selectedEmitterIndex, selectedOnly);
	selectedIndexValid_ = selectedOnly
		? selectedEmitterIndex >= 0 && selectedEmitterIndex < static_cast<int>(effect.emitters.size())
		: !effect.emitters.empty();
	lastRequestedEmitterCount_ = static_cast<uint32_t>(indices.size());
	lastConnectedEmitterCount_ = 0;

	if (indices.empty())
	{
		SetStatus("Emit Once skipped.", "対象Emitterが選択されていません");
		return;
	}

	const uint32_t requestedPreviewCount = (std::max)(emitCount, 1u);
	const uint32_t safeCount = (std::min)(requestedPreviewCount, kMaxPreviewEmitOnce);
	if (emitCount == 0) AppendMessage(lastErrorMessage_, "Preview Emit Countが0のため1へ補正");
	else if (safeCount != emitCount) AppendMessage(lastErrorMessage_, "Emit Onceを安全上限4096へClamp");
	for (const size_t index : indices)
	{
		const auto& desc = effect.emitters[index];
		const uint32_t selectedCount = useBurstCountForEmitOnce_ && desc.burstCount > 0 ? desc.burstCount : safeCount;
		lastEmitCountSource_ = useBurstCountForEmitOnce_ && desc.burstCount > 0 ? "Emitter burstCount" : "Preview Emit Count";
		const auto settings = K4E::BuildPreviewSpawnSettings(desc, previewPosition, selectedCount, forceVisibleSprite_);
		const uint32_t perEmitterCount = settings.emitCount;
		lastEmitRequestedCount_ += perEmitterCount;
		K4E::Log("[GpuParticlePreview] Selected emitter: " + desc.name + "\n");
		K4E::Log("[GpuParticlePreview] Try emit " + std::string(desc.renderType == K4E::GpuParticleRenderType::Mesh ? "mesh" : "sprite") + " count: " + std::to_string(perEmitterCount) + "\n");
		uint32_t acceptedCount = 0;
		if (CreateRuntimeEmitter(desc, index, previewPosition, perEmitterCount, acceptedCount))
		{
			++lastConnectedEmitterCount_;
			lastEmitAcceptedCount_ += acceptedCount;
			lastEmitCount_ += acceptedCount;
			if (desc.renderType == K4E::GpuParticleRenderType::Mesh) ++lastMeshConnectedCount_;
			else ++lastSpriteConnectedCount_;
			K4E::Log("[GpuParticlePreview] Runtime spawn success, accepted: " + std::to_string(acceptedCount) + "\n");
		}
		else K4E::Log("[GpuParticlePreview] Runtime spawn failed: " + lastErrorMessage_ + "\n");
	}
	SetStatus(lastConnectedEmitterCount_ > 0 ? "Emit Once requested." : "Emit Once could not connect to Runtime.", lastErrorMessage_);
}

void GpuParticlePreviewController::Play(
	const K4E::GpuParticleEffectDesc& effect,
	int selectedEmitterIndex,
	const K4E::Vector3& previewPosition,
	uint32_t emitCount,
	bool selectedOnly)
{
	(void)emitCount; // 互換用UI値。Play開始時はEmitterDesc固有のburstCountを優先する。
	auto* runtimeManager = K4E::GpuParticleManager::GetInstance();
	updateCountAtLastRequest_ = runtimeManager->GetUpdateCallCount();
	drawCountAtLastRequest_ = runtimeManager->GetDrawCallCount();
	emitDispatchCountAtLastRequest_ = runtimeManager->GetEmitDispatchCount();
	runtimeSpawnCalled_ = false;
	hasRuntimeRequest_ = true;
	lastEmitRequestedCount_ = 0;
	lastEmitAcceptedCount_ = 0;
	lastEmitPosition_ = previewPosition;
	RemoveRuntimeEmitters();
	++generation_;
	lastErrorMessage_.clear();
	lastEmitCount_ = 0;
	lastSpawnAccumulator_ = 0.0f;
	lastSpriteConnectedCount_ = 0;
	lastMeshConnectedCount_ = 0;
	lastSelectedOnly_ = selectedOnly;
	lastSelectedEmitterIndex_ = selectedEmitterIndex;
	const auto indices = CollectTargetEmitterIndices(effect, selectedEmitterIndex, selectedOnly);
	selectedIndexValid_ = selectedOnly
		? selectedEmitterIndex >= 0 && selectedEmitterIndex < static_cast<int>(effect.emitters.size())
		: !effect.emitters.empty();
	lastRequestedEmitterCount_ = static_cast<uint32_t>(indices.size());
	lastConnectedEmitterCount_ = 0;

	if (indices.empty())
	{
		SetStatus("Play failed.", "対象Emitterが選択されていません");
		return;
	}

	for (const size_t index : indices)
	{
		const auto& desc = effect.emitters[index];
		// Effect再生開始時はloopの有無に関係なく、Emitter固有のBurstを一度だけ生成する。
		const auto settings = K4E::BuildPreviewSpawnSettings(desc, previewPosition, (std::max)(desc.burstCount, 1u), forceVisibleSprite_);
		const uint32_t initialCount = settings.emitCount;
		lastEmitCountSource_ = "Emitter burstCount";
		lastEmitRequestedCount_ += initialCount;
		uint32_t acceptedCount = 0;
		if (CreateRuntimeEmitter(desc, index, previewPosition, initialCount, acceptedCount))
		{
			++lastConnectedEmitterCount_;
			lastEmitAcceptedCount_ += acceptedCount;
			lastEmitCount_ += acceptedCount;
			if (desc.renderType == K4E::GpuParticleRenderType::Mesh) ++lastMeshConnectedCount_;
			else ++lastSpriteConnectedCount_;
		}
	}

	playing_ = lastConnectedEmitterCount_ > 0;
	SetStatus(playing_ ? "Preview playing with spawnRate accumulator." : "Preview could not connect to Runtime.", lastErrorMessage_);
}

void GpuParticlePreviewController::Stop()
{
	playing_ = false;
	SetStatus("Preview stopped. Existing particles will expire by lifetime.");
}

void GpuParticlePreviewController::RemoveRuntimeEmitters()
{
	for (const auto& record : runtimeEmitters_)
	{
		K4E::GpuParticleManager::GetInstance()->RemoveEmitter(record.runtimeName);
		for (const uint32_t meshId : record.ownedMeshAssetIds)
		{
			K4E::GpuParticleManager::GetInstance()->UnregisterMeshAsset(meshId);
		}
	}
	runtimeEmitters_.clear();
	playing_ = false;
}

void GpuParticlePreviewController::Clear()
{
	RemoveRuntimeEmitters();
	lastRequestedEmitterCount_ = 0;
	lastConnectedEmitterCount_ = 0;
	lastSpriteConnectedCount_ = 0;
	lastMeshConnectedCount_ = 0;
	lastEmitCount_ = 0;
	lastSpawnAccumulator_ = 0.0f;
	SetStatus("Preview Runtime Emitters / Mesh Assets cleared.");
}

uint32_t GpuParticlePreviewController::GetCurrentAliveParticleCount() const
{
	uint64_t alive = 0;
	for (const auto& record : runtimeEmitters_)
	{
		if (const auto* emitter = K4E::GpuParticleManager::GetInstance()->GetEmitter(record.runtimeName))
		{
			alive += emitter->GetEstimatedActiveParticleCount();
		}
	}
	return static_cast<uint32_t>((std::min)(alive, static_cast<uint64_t>((std::numeric_limits<uint32_t>::max)())));
}

void GpuParticlePreviewController::Update(
	float deltaTime,
	const K4E::GpuParticleEffectDesc& effect,
	int selectedEmitterIndex,
	const K4E::Vector3& previewPosition,
	uint32_t emitCount,
	bool autoPlay,
	bool selectedOnly)
{
	(void)emitCount; // 互換用UI値。継続再生はEmitterDescのburstCount/spawnRateを使用する。
	if (!autoPlay) autoPlayAttempted_ = false;
	if (autoPlay && !playing_ && !autoPlayAttempted_)
	{
		autoPlayAttempted_ = true;
		Play(effect, selectedEmitterIndex, previewPosition, emitCount, selectedOnly);
		return;
	}
	if (!playing_) return;

	if (autoPlay && (selectedOnly != lastSelectedOnly_ || selectedEmitterIndex != lastSelectedEmitterIndex_))
	{
		Play(effect, selectedEmitterIndex, previewPosition, emitCount, selectedOnly);
		return;
	}

	lastEmitCount_ = 0;
	lastSpawnAccumulator_ = 0.0f;
	bool anyEmitterCanSpawn = false;
	for (auto& record : runtimeEmitters_)
	{
		if (record.sourceEmitterIndex >= effect.emitters.size())
		{
			Stop();
			SetStatus("Preview stopped.", "Emitter削除またはLoadでプレビュー元Indexが無効になりました");
			return;
		}

		const auto& desc = effect.emitters[record.sourceEmitterIndex];
		auto* emitter = K4E::GpuParticleManager::GetInstance()->GetEmitter(record.runtimeName);
		if (!emitter)
		{
			playing_ = false;
			SetStatus("Preview stopped.", "Runtime Emitterが見つかりません");
			return;
		}

		std::string warning;
		if (!K4E::ApplyEmitterDescToRuntimeEmitter(desc, *emitter, previewPosition, emitCount, true, forceVisibleSprite_, warning))
		{
			Stop();
			SetStatus("Preview stopped.", warning);
			return;
		}
		const auto& appliedInfo = emitter->GetInfo();
		lastEmitPosition_ = emitter->GetPosition();
		lastUsedVelocity_ = appliedInfo.velocity;
		lastUsedLifeTime_ = appliedInfo.lifeTime;
		lastUsedStartSize_ = appliedInfo.startSize;
		lastUsedStartColor_ = appliedInfo.startColor;
		lastUsedTexturePath_ = appliedInfo.textureFilePath;
		lastUsedMode_ = forceVisibleSprite_ && desc.renderType == K4E::GpuParticleRenderType::Sprite ? "ForceVisible" : "Desc";

		record.elapsedTime += (std::max)(deltaTime, 0.0f);
		const bool withinDuration = desc.loop || record.elapsedTime < (std::max)(desc.duration, 0.0f);
		const uint32_t effectiveMaxParticles = emitter->GetInfo().maxParticles;
		if (withinDuration && desc.spawnRate > 0.0f && effectiveMaxParticles > 0)
		{
			anyEmitterCanSpawn = true;
			// フレームレートに依存せず、1秒あたりの発生数で制御するAccumulator。
			record.spawnAccumulator = (std::min)(
				record.spawnAccumulator + desc.spawnRate * (std::max)(deltaTime, 0.0f),
				static_cast<float>(kMaxPreviewEmitPerFrame) + 0.999f);
			uint32_t requestCount = static_cast<uint32_t>(std::floor(record.spawnAccumulator));
			record.spawnAccumulator -= static_cast<float>(requestCount);
			requestCount = (std::min)(requestCount, kMaxPreviewEmitPerFrame);

			const uint32_t alive = emitter->GetEstimatedActiveParticleCount();
			const uint32_t available = alive < effectiveMaxParticles ? effectiveMaxParticles - alive : 0;
			const uint32_t requested = (std::min)(requestCount, available);
			if (requested > 0)
			{
				runtimeSpawnCalled_ = true;
				lastEmitCountSource_ = "spawnRate accumulator";
				lastEmitRequestedCount_ = requested;
				lastEmitAcceptedCount_ = emitter->RequestEmit(requested);
				lastEmitCount_ += lastEmitAcceptedCount_;
			}
		}
		lastSpawnAccumulator_ += record.spawnAccumulator;
	}
	if (!anyEmitterCanSpawn)
	{
		playing_ = false;
		SetStatus("Preview duration completed. Existing particles will expire by lifetime.", lastErrorMessage_);
	}
}

void GpuParticlePreviewController::SetStatus(const std::string& message, const std::string& error)
{
	lastStatus_ = message;
	lastErrorMessage_ = error;
	K4E::Log("[GpuParticlePreview] " + message + (error.empty() ? "" : " " + error) + "\n");
}

void GpuParticlePreviewController::DrawImGui(
	const K4E::GpuParticleEffectDesc& effect,
	int selectedEmitterIndex,
	K4E::Vector3& previewPosition,
	uint32_t& emitCount,
	bool& autoPlay,
	bool& selectedOnly)
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("Preview");
	ImGui::DragFloat3("Preview Position", &previewPosition.x, 0.1f);
	int editableEmitCount = static_cast<int>((std::min)(emitCount, kMaxPreviewEmitOnce));
	if (ImGui::DragInt("Preview Emit Count", &editableEmitCount, 1.0f, 0, static_cast<int>(kMaxPreviewEmitOnce)))
	{
		emitCount = static_cast<uint32_t>((std::max)(editableEmitCount, 0));
	}

	ImGui::Checkbox("Auto Play", &autoPlay);
	ImGui::Checkbox("Emit Once Uses burstCount", &useBurstCountForEmitOnce_);
	ImGui::Checkbox("Force Visible Sprite", &forceVisibleSprite_);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("通常のEmitterDesc設定を無視し、白いSpriteを固定値で出すRuntime接続確認用です。");
	const bool forceVisibleTargetsSprite = selectedEmitterIndex >= 0 && selectedEmitterIndex < static_cast<int>(effect.emitters.size()) &&
		effect.emitters[static_cast<size_t>(selectedEmitterIndex)].renderType == K4E::GpuParticleRenderType::Sprite;
	if (forceVisibleSprite_ && forceVisibleTargetsSprite)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Force Visible is ON: edited motion/size/color values are overridden.");
	}
	if (ImGui::RadioButton("Preview Selected Emitter Only", selectedOnly)) selectedOnly = true;
	ImGui::SameLine();
	if (ImGui::RadioButton("Preview Effect All Emitters", !selectedOnly)) selectedOnly = false;

	if (ImGui::Button("Emit Once")) EmitOnce(effect, selectedEmitterIndex, previewPosition, emitCount, selectedOnly);
	ImGui::SameLine();
	if (ImGui::Button("Play Effect")) Play(effect, selectedEmitterIndex, previewPosition, emitCount, selectedOnly);
	ImGui::SameLine();
	if (ImGui::Button("Stop Effect")) { autoPlay = false; Stop(); }
	ImGui::SameLine();
	if (ImGui::Button("Clear Preview Particles")) { autoPlay = false; Clear(); }

	// 何も表示されない原因を確認しやすくするため、接続種別・生存数・Accumulatorを表示する。
	ImGui::SeparatorText("Preview Status");
	const auto* runtimeManager = K4E::GpuParticleManager::GetInstance();
	const bool runtimeUpdateCalled = hasRuntimeRequest_ && runtimeManager->GetUpdateCallCount() > updateCountAtLastRequest_;
	const bool runtimeDrawCalled = hasRuntimeRequest_ && runtimeManager->GetDrawCallCount() > drawCountAtLastRequest_;
	const bool runtimeEmitDispatched = hasRuntimeRequest_ && runtimeManager->GetEmitDispatchCount() > emitDispatchCountAtLastRequest_;
	const bool currentSelectedIndexValid = selectedOnly
		? selectedEmitterIndex >= 0 && selectedEmitterIndex < static_cast<int>(effect.emitters.size())
		: !effect.emitters.empty();
	const bool selectedIsSprite = selectedEmitterIndex >= 0 && selectedEmitterIndex < static_cast<int>(effect.emitters.size()) &&
		effect.emitters[static_cast<size_t>(selectedEmitterIndex)].renderType == K4E::GpuParticleRenderType::Sprite;
	const bool forceVisibleActive = forceVisibleSprite_ && selectedIsSprite;
	// 設定が反映されない原因を確認しやすくするため、固定表示とDesc反映のどちらかを明示する。
	ImGui::Text("Preview Mode: %s", forceVisibleActive ? "ForceVisible" : "Desc");
	ImGui::Text("Force Visible Sprite: %s", forceVisibleSprite_ ? "ON" : "OFF");
	ImGui::Text("Desc Applied: %s", forceVisibleActive ? "NO" : "YES");
	ImGui::Text("Using Texture Path: %s", lastUsedTexturePath_.empty() ? "not emitted yet" : lastUsedTexturePath_.c_str());
	ImGui::Text("Using Size: %.3f, %.3f", lastUsedStartSize_.x, lastUsedStartSize_.y);
	ImGui::Text("Using Lifetime: %.3f", lastUsedLifeTime_);
	ImGui::Text("Using StartColor: %.2f, %.2f, %.2f, %.2f",
		lastUsedStartColor_.x, lastUsedStartColor_.y, lastUsedStartColor_.z, lastUsedStartColor_.w);
	ImGui::Text("Using Velocity: %.2f, %.2f, %.2f", lastUsedVelocity_.x, lastUsedVelocity_.y, lastUsedVelocity_.z);
	ImGui::Text("Preview Playing: %s", playing_ ? "ON" : "OFF");
	ImGui::Text("Emit Button Pressed Count: %llu", static_cast<unsigned long long>(emitButtonPressedCount_));
	ImGui::Text("Last Emit Requested Count: %u", lastEmitRequestedCount_);
	ImGui::Text("Last Emit Accepted Count: %u", lastEmitAcceptedCount_);
	ImGui::Text("Last Emit Count Source: %s", lastEmitCountSource_.c_str());
	ImGui::Text("Selected Index Valid: %s", currentSelectedIndexValid ? "YES" : "NO");
	ImGui::Text("Runtime Spawn Called: %s", runtimeSpawnCalled_ ? "YES" : "NO");
	ImGui::Text("Runtime Emit Dispatch Called: %s", runtimeEmitDispatched ? "YES" : "NO");
	ImGui::Text("Runtime Update Called: %s", runtimeUpdateCalled ? "YES" : "NO");
	ImGui::Text("Runtime Draw Called: %s", runtimeDrawCalled ? "YES" : "NO");
	ImGui::Text("Runtime Draw Commands: %u", runtimeManager->GetLastDrawCallCount());
	ImGui::Text("Runtime Alive Count: %u", GetCurrentAliveParticleCount());
	ImGui::Text("Last Emit Position: %.2f, %.2f, %.2f", lastEmitPosition_.x, lastEmitPosition_.y, lastEmitPosition_.z);
	// 設定を変えても効かない理由を分かるよう、主要項目のRuntime接続状況と実使用値を表示する。
	ImGui::Text("Connected: velocity YES");
	ImGui::Text("Connected: gravity YES");
	ImGui::Text("Connected: size YES");
	ImGui::Text("Connected: color YES");
	ImGui::Text("Connected: lifetime YES");
	ImGui::Text("Connected: spawnShape YES");
	ImGui::Text("Connected: blendMode NO (Preview pipeline uses Alpha)");
	ImGui::Text("Last Used Velocity: %.2f, %.2f, %.2f", lastUsedVelocity_.x, lastUsedVelocity_.y, lastUsedVelocity_.z);
	ImGui::Text("Last Used Mode: %s", lastUsedMode_.c_str());
	ImGui::Text("Last Used Texture: %s", lastUsedTexturePath_.empty() ? "not emitted yet" : lastUsedTexturePath_.c_str());
	ImGui::Text("Last Used LifeTime: %.3f", lastUsedLifeTime_);
	ImGui::Text("Last Used StartSize: %.3f, %.3f", lastUsedStartSize_.x, lastUsedStartSize_.y);
	ImGui::Text("Last Used StartColor: %.2f, %.2f, %.2f, %.2f",
		lastUsedStartColor_.x, lastUsedStartColor_.y, lastUsedStartColor_.z, lastUsedStartColor_.w);
	if (selectedEmitterIndex >= 0 && selectedEmitterIndex < static_cast<int>(effect.emitters.size()))
	{
		const auto& selected = effect.emitters[static_cast<size_t>(selectedEmitterIndex)];
		ImGui::Text("Selected Emitter: %s", selected.name.c_str());
		ImGui::Text("Render Type: %s", selected.renderType == K4E::GpuParticleRenderType::Mesh ? "Mesh" : "Sprite");
		ImGui::Text("Spawn Rate: %.2f / sec", selected.spawnRate);
		if (selected.renderType == K4E::GpuParticleRenderType::Mesh && hasRuntimeRequest_ && lastMeshConnectedCount_ == 0)
		{
			// TODO: Mesh Asset接続に失敗した場合はSprite表示確認を優先し、理由を明示する。
			ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Mesh particle runtime is not connected yet");
		}
		if (selected.renderType == K4E::GpuParticleRenderType::Mesh && forceVisibleSprite_)
		{
			ImGui::TextDisabled("Force Visible Sprite is Sprite-only; Mesh preview continues to use Desc.");
		}
	}
	else
	{
		ImGui::TextDisabled("Selected Emitter: none");
	}
	ImGui::Text("Last Emit Count: %u", lastEmitCount_);
	ImGui::Text("Current Alive Particles: %u", GetCurrentAliveParticleCount());
	ImGui::Text("Spawn Accumulator: %.3f", lastSpawnAccumulator_);
	const char* connectionText = lastRequestedEmitterCount_ > 0 && lastConnectedEmitterCount_ == lastRequestedEmitterCount_
		? "YES" : (lastConnectedEmitterCount_ > 0 ? "PARTIAL" : "NO");
	ImGui::Text("Runtime Connected: %s (%u/%u)", connectionText, lastConnectedEmitterCount_, lastRequestedEmitterCount_);
	ImGui::Text("Sprite Runtime Connected: %s (%u)", lastSpriteConnectedCount_ > 0 ? "YES" : "NO", lastSpriteConnectedCount_);
	ImGui::Text("Mesh Runtime Connected: %s (%u)", lastMeshConnectedCount_ > 0 ? "YES" : "NO", lastMeshConnectedCount_);
	ImGui::TextWrapped("Last Runtime Error: %s", lastErrorMessage_.empty() ? "none" : lastErrorMessage_.c_str());
	ImGui::TextWrapped("Status: %s", lastStatus_.c_str());
	if (!lastErrorMessage_.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Last Error / Limitation:");
		ImGui::TextWrapped("%s", lastErrorMessage_.c_str());
	}
#else
	(void)effect; (void)selectedEmitterIndex; (void)previewPosition;
	(void)emitCount; (void)autoPlay; (void)selectedOnly;
#endif
}
