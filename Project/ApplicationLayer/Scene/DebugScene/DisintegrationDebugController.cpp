#define NOMINMAX
#include "DisintegrationDebugController.h"

#include <Input.h>

#ifdef USE_IMGUI
#include <ImGuiManager.h>
#endif // USE_IMGUI

#include <Windows.h>
#include <algorithm>

using namespace Ken4lowEngine;

namespace
{
	/// -------------------------------------------------------------
	/// Visual Studio の「出力」ウィンドウへ文字列を出す
	/// 改行付きで送るための簡易ヘルパー
	/// -------------------------------------------------------------
	void DebugLog(const std::string& message)
	{
		std::string line = message + "\n";
		OutputDebugStringA(line.c_str());
	}
}

void DisintegrationDebugController::Initialize()
{
	input_ = Input::GetInstance();

	debugDisintegrationModel_ = std::make_unique<Object3D>();
	debugDisintegrationModel_->Initialize(debugDisintegrationModelPath_);
	debugDisintegrationModel_->SetTranslate(debugDisintegrationPosition_);
	debugDisintegrationModel_->SetRotate(debugDisintegrationRotation_);
	debugDisintegrationModel_->SetScale(debugDisintegrationScale_);
	debugDisintegrationModel_->Update();

	debugDisintegrationEffect_ = std::make_unique<ModelDisintegrationEffect>();
	debugDisintegrationEffect_->Initialize();

	debugReconstructionModel_ = std::make_unique<Object3D>();
	debugReconstructionModel_->Initialize(debugReconstructionModelPath_);
	debugReconstructionModel_->SetTranslate(debugReconstructionPosition_);
	debugReconstructionModel_->SetRotate(debugReconstructionRotation_);
	debugReconstructionModel_->SetScale(debugReconstructionScale_);
	debugReconstructionModel_->Update();

	debugReconstructionEffect_ = std::make_unique<ModelReconstructionEffect>();
	debugReconstructionEffect_->Initialize();

	debugModelBlockSequenceModel_ = std::make_unique<Object3D>();
	debugModelBlockSequenceModel_->Initialize(debugModelBlockSequenceModelPath_);
	debugModelBlockSequenceModel_->SetTranslate(debugModelBlockSequencePosition_);
	debugModelBlockSequenceModel_->SetRotate(debugModelBlockSequenceRotation_);
	debugModelBlockSequenceModel_->SetScale(debugModelBlockSequenceScale_);
	debugModelBlockSequenceModel_->Update();

	debugSequenceDisintegrationEffect_ = std::make_unique<ModelDisintegrationEffect>();
	debugSequenceDisintegrationEffect_->Initialize();
	debugSequenceReconstructionEffect_ = std::make_unique<ModelReconstructionEffect>();
	debugSequenceReconstructionEffect_->Initialize();
	debugModelBlockSequence_ = std::make_unique<ModelBlockEffectSequence>();
	debugModelBlockSequence_->Initialize(debugSequenceReconstructionEffect_.get(), debugSequenceDisintegrationEffect_.get());
}

void DisintegrationDebugController::Update(float deltaTime)
{
	UpdateDebugDisintegrationTest(deltaTime);
	UpdateDebugReconstructionTest(deltaTime);
	UpdateDebugModelBlockSequence(deltaTime);
}

void DisintegrationDebugController::Draw3DObjects()
{
	if (debugDisintegrationModelVisible_ && debugDisintegrationModel_)
	{
		debugDisintegrationModel_->Draw();
	}

	if (debugDisintegrationEffect_)
	{
		debugDisintegrationEffect_->Draw();
	}

	if (debugReconstructionModelVisible_ && debugReconstructionModel_)
	{
		debugReconstructionModel_->Draw();
	}

	if (debugReconstructionEffect_)
	{
		debugReconstructionEffect_->Draw();
	}

	if (debugModelBlockSequence_ && debugModelBlockSequence_->IsModelVisible() && debugModelBlockSequenceModel_)
	{
		debugModelBlockSequenceModel_->Draw();
	}

	if (debugSequenceReconstructionEffect_)
	{
		debugSequenceReconstructionEffect_->Draw();
	}

	if (debugSequenceDisintegrationEffect_)
	{
		debugSequenceDisintegrationEffect_->Draw();
	}
}

void DisintegrationDebugController::DrawShadowObjects()
{
	if (debugDisintegrationModelVisible_ && debugDisintegrationModel_)
	{
		debugDisintegrationModel_->DrawShadow();
	}

	if (debugReconstructionModelVisible_ && debugReconstructionModel_)
	{
		debugReconstructionModel_->DrawShadow();
	}

	if (debugModelBlockSequence_ && debugModelBlockSequence_->IsModelVisible() && debugModelBlockSequenceModel_)
	{
		debugModelBlockSequenceModel_->DrawShadow();
	}
}

void DisintegrationDebugController::Finalize()
{
	debugModelBlockSequence_.reset();
	debugSequenceReconstructionEffect_.reset();
	debugSequenceDisintegrationEffect_.reset();
	debugModelBlockSequenceModel_.reset();
	debugReconstructionEffect_.reset();
	debugReconstructionModel_.reset();
	debugDisintegrationEffect_.reset();
	debugDisintegrationModel_.reset();
	input_ = nullptr;
}

void DisintegrationDebugController::DrawImGui()
{
#ifdef USE_IMGUI


	// エフェクト調整時に迷わないよう、崩壊・再構築・シーケンス系ImGuiの表示文言は日本語で統一する。
	/// ---------- モデル崩壊エフェクト単体テスト ---------- ///
	{
		static char modelPathBuffer[256] = "Characters/body.gltf";

		ImGui::Begin("モデル崩壊エフェクト確認");
		ImGui::TextWrapped("DebugScene専用テストです。F9またはボタンで表示中モデルの表面をサンプリングし、崩壊エフェクトを再生します。");
		ImGui::InputText("モデルパス", modelPathBuffer, IM_ARRAYSIZE(modelPathBuffer));
		ImGui::DragFloat3("位置", &debugDisintegrationPosition_.x, 0.05f);
		ImGui::DragFloat3("回転", &debugDisintegrationRotation_.x, 0.01f);
		ImGui::DragFloat3("スケール", &debugDisintegrationScale_.x, 0.01f, 0.01f, 10.0f);

		if (ImGui::Button("テストモデル再読み込み"))
		{
			pendingDebugDisintegrationPath_ = modelPathBuffer;
			pendingDebugDisintegrationReload_ = true;
			debugDisintegrationLog_ = "テストモデル再読み込みを予約: " + pendingDebugDisintegrationPath_;
		}

		ImGui::SameLine();
		if (ImGui::Button("崩壊再生"))
		{
			pendingDebugDisintegrationPath_ = modelPathBuffer;
			pendingDebugDisintegrationPlay_ = true;
			debugDisintegrationLog_ = "再生を予約: " + pendingDebugDisintegrationPath_;
		}
		ImGui::SameLine();
		if (ImGui::Button("リセット"))
		{
			pendingDebugDisintegrationReset_ = true;
			debugDisintegrationLog_ = "崩壊リセットを予約しました。";
		}

		if (ImGui::Button(debugDisintegrationPaused_ ? "再開" : "一時停止"))
		{
			debugDisintegrationPaused_ = !debugDisintegrationPaused_;
		}
		ImGui::SameLine();
		if (ImGui::Button("1フレーム送り"))
		{
			debugDisintegrationStepFrame_ = true;
			debugDisintegrationPaused_ = true;
		}

		ImGui::Text("モデル表示: %s", debugDisintegrationModelVisible_ ? "はい" : "いいえ");
		ImGui::TextWrapped("%s", debugDisintegrationLog_.c_str());
		ImGui::End();
	}

	if (debugDisintegrationEffect_)
	{
		debugDisintegrationEffect_->DrawImGui();
	}


	/// ---------- モデル再構築エフェクト単体テスト ---------- ///
	{
		static char reconstructionModelPathBuffer[256] = "Characters/body.gltf";

		ImGui::Begin("モデル再構築エフェクト確認");
		ImGui::TextWrapped("DebugScene専用テストです。F10またはボタンで散らばったCPUキューブブロックをモデル表面のサンプル位置へ集めます。");
		ImGui::InputText("モデルパス", reconstructionModelPathBuffer, IM_ARRAYSIZE(reconstructionModelPathBuffer));
		ImGui::DragFloat3("位置", &debugReconstructionPosition_.x, 0.05f);
		ImGui::DragFloat3("回転", &debugReconstructionRotation_.x, 0.01f);
		ImGui::DragFloat3("スケール", &debugReconstructionScale_.x, 0.01f, 0.01f, 10.0f);

		if (ImGui::Button("テストモデル再読み込み##Reconstruction"))
		{
			pendingDebugReconstructionPath_ = reconstructionModelPathBuffer;
			pendingDebugReconstructionReload_ = true;
			debugReconstructionLog_ = "再構築テストモデル再読み込みを予約: " + pendingDebugReconstructionPath_;
		}

		ImGui::SameLine();
		if (ImGui::Button("再構築再生"))
		{
			pendingDebugReconstructionPath_ = reconstructionModelPathBuffer;
			pendingDebugReconstructionPlay_ = true;
			debugReconstructionLog_ = "再構築再生を予約: " + pendingDebugReconstructionPath_;
		}

		ImGui::Text("モデル表示: %s", debugReconstructionModelVisible_ ? "はい" : "いいえ");
		ImGui::TextWrapped("%s", debugReconstructionLog_.c_str());
		ImGui::End();
	}

	if (debugReconstructionEffect_)
	{
		debugReconstructionEffect_->DrawImGui();
	}

	/// ---------- モデルブロック演出シーケンステスト ---------- ///
	{
		static char sequenceModelPathBuffer[256] = "Characters/body.gltf";

		ImGui::Begin("ブロック演出シーケンス確認");
		ImGui::TextWrapped("DebugScene専用のシーケンステストです。再構築 → モデル表示 → 崩壊、および崩壊 → 再構築の流れを確認します。");
		ImGui::InputText("モデルパス##BlockSequence", sequenceModelPathBuffer, IM_ARRAYSIZE(sequenceModelPathBuffer));
		ImGui::DragFloat3("位置##BlockSequence", &debugModelBlockSequencePosition_.x, 0.05f);
		ImGui::DragFloat3("回転##BlockSequence", &debugModelBlockSequenceRotation_.x, 0.01f);
		ImGui::DragFloat3("スケール##BlockSequence", &debugModelBlockSequenceScale_.x, 0.01f, 0.01f, 10.0f);

		if (debugModelBlockSequence_)
		{
			auto& sequenceParams = debugModelBlockSequence_->GetParameters();
			ImGui::DragFloat("通常モデル表示時間", &sequenceParams.showDuration, 0.05f, 0.0f, 10.0f);
			ImGui::Checkbox("モデルブレンドを使う", &sequenceParams.useModelBlend);
			ImGui::DragFloat("モデル切り替えブレンド時間", &sequenceParams.modelBlendDuration, 0.01f, 0.0f, 3.0f);
			ImGui::DragFloat("ブロック完全体保持時間", &sequenceParams.blockHoldDuration, 0.05f, 0.0f, 10.0f);
			ImGui::Checkbox("崩壊までブロックを保持", &sequenceParams.keepBlocksUntilDisintegration);
			ImGui::Checkbox("通常モデルを挟まない", &sequenceParams.skipNormalModelInSequence);
			ImGui::Checkbox("ブロックから通常モデルへフェード", &sequenceParams.useBlockToModelFade);
			ImGui::Checkbox("通常モデルからブロックへフェード", &sequenceParams.useModelToBlockFade);
			ImGui::DragFloat("待機時間", &sequenceParams.waitDuration, 0.05f, 0.0f, 10.0f);
			ImGui::Checkbox("ループ有効", &sequenceParams.loopEnabled);
			ImGui::SliderInt("ブロック数##BlockSequence", &sequenceParams.blockCount, 32, 8000);
			if (ImGui::SliderFloat("ブロックサイズ##BlockSequence", &sequenceParams.blockSize, 0.005f, 0.30f))
			{
				sequenceParams.blockSize = std::max(sequenceParams.blockSize, 0.005f);
			}
			ImGui::Checkbox("表面サンプリング##BlockSequence", &sequenceParams.surfaceSampling);
			const char* sequencePlacementModeLabels[] = { "ランダム表面配置", "均一表面配置", "整列表面配置", "ボクセル敷き詰め配置" };
			int sequencePlacementModeIndex = sequenceParams.placementMode == DisintegrationPlacementMode::VoxelFill ? 3 : (sequenceParams.placementMode == DisintegrationPlacementMode::AlignedSurfaceGrid ? 2 : (sequenceParams.placementMode == DisintegrationPlacementMode::UniformSurface ? 1 : 0));
			if (ImGui::Combo("配置モード##BlockSequence", &sequencePlacementModeIndex, sequencePlacementModeLabels, IM_ARRAYSIZE(sequencePlacementModeLabels)))
			{
				sequenceParams.placementMode = sequencePlacementModeIndex == 3 ? DisintegrationPlacementMode::VoxelFill : (sequencePlacementModeIndex == 2 ? DisintegrationPlacementMode::AlignedSurfaceGrid : (sequencePlacementModeIndex == 1 ? DisintegrationPlacementMode::UniformSurface : DisintegrationPlacementMode::RandomSurface));
				if (sequenceParams.placementMode == DisintegrationPlacementMode::VoxelFill)
				{
					sequenceParams.useRandomScale = false;
					sequenceParams.useRandomRotation = false;
					sequenceParams.surfaceSampling = false;
					sequenceParams.useSurfaceInset = false;
					sequenceParams.voxelSpacing = sequenceParams.blockSize;
					sequenceParams.placementSpacing = sequenceParams.voxelSpacing;
					sequenceParams.voxelSurfaceThickness = sequenceParams.blockSize * 1.5f;
					sequenceParams.maxVoxelBlockCount = std::max(sequenceParams.maxVoxelBlockCount, 10000);
				}
				else if (sequenceParams.placementMode != DisintegrationPlacementMode::RandomSurface)
				{
					sequenceParams.useRandomScale = false;
					sequenceParams.useRandomRotation = false;
					sequenceParams.useSurfaceInset = true;
				}
			}
			ImGui::Checkbox("ランダムサイズを使う##BlockSequence", &sequenceParams.useRandomScale);
			ImGui::SliderFloat("サイズばらつき##BlockSequence", &sequenceParams.scaleVariation, 0.0f, 0.75f);
			ImGui::Checkbox("ランダム回転を使う##BlockSequence", &sequenceParams.useRandomRotation);
			ImGui::SliderFloat("回転ばらつき##BlockSequence", &sequenceParams.rotationRandomness, 0.0f, 8.0f);
			ImGui::SeparatorText("フェードイン##BlockSequence");
			ImGui::Checkbox("フェードインを使う##BlockSequence", &sequenceParams.useFadeIn);
			ImGui::SliderFloat("初期透明度##BlockSequence", &sequenceParams.initialAlpha, 0.0f, 1.0f);
			ImGui::SliderFloat("目標透明度##BlockSequence", &sequenceParams.targetAlpha, 0.0f, 1.0f);
			ImGui::SliderFloat("フェードイン時間##BlockSequence", &sequenceParams.fadeInDuration, 0.01f, 3.0f);
			ImGui::SliderFloat("フェードイン遅延幅##BlockSequence", &sequenceParams.fadeInDelayRange, 0.0f, 2.0f);
			ImGui::SliderFloat("フェードインの鋭さ##BlockSequence", &sequenceParams.fadeInEasePower, 0.1f, 8.0f);
			ImGui::Checkbox("距離でフェードイン##BlockSequence", &sequenceParams.fadeInByDistance);
			ImGui::Checkbox("到着付近で不透明化##BlockSequence", &sequenceParams.fadeInNearTarget);
			int sequencePlacementSeed = static_cast<int>(sequenceParams.placementSeed);
			if (ImGui::InputInt("配置シード##BlockSequence", &sequencePlacementSeed))
			{
				sequenceParams.placementSeed = static_cast<uint32_t>(std::max(sequencePlacementSeed, 0));
			}
			ImGui::SliderFloat("配置間隔##BlockSequence", &sequenceParams.placementSpacing, 0.0f, 0.5f);
			if (sequenceParams.placementMode == DisintegrationPlacementMode::VoxelFill)
			{
				ImGui::SliderFloat("ボクセル間隔##BlockSequence", &sequenceParams.voxelSpacing, 0.005f, 0.50f);
				ImGui::SliderInt("最大ブロック数##BlockSequence", &sequenceParams.maxVoxelBlockCount, 128, 30000);
				ImGui::SliderFloat("表面厚み##BlockSequence", &sequenceParams.voxelSurfaceThickness, 0.0f, 1.0f);
				ImGui::Checkbox("内外判定を使う##BlockSequence", &sequenceParams.useVoxelInsideTest);
				ImGui::Checkbox("表面近傍判定を使う##BlockSequence", &sequenceParams.useVoxelSurfaceNearTest);
				ImGui::Checkbox("グリッド原点を中央に揃える##BlockSequence", &sequenceParams.alignVoxelGridToCenter);
			}
			ImGui::Checkbox("表面内側オフセットを使う##BlockSequence", &sequenceParams.useSurfaceInset);
			ImGui::Checkbox("ブロックサイズから自動計算##BlockSequence", &sequenceParams.autoSurfaceInsetFromBlockSize);
			if (!sequenceParams.autoSurfaceInsetFromBlockSize)
			{
				ImGui::SliderFloat("表面内側オフセット量##BlockSequence", &sequenceParams.surfaceInset, 0.0f, 0.30f);
			}
			else
			{
				const float effectiveSurfaceInset = sequenceParams.blockSize * 0.5f;
				ImGui::Text("表面内側オフセット量: %.3f", effectiveSurfaceInset);
			}
			if (sequenceParams.placementMode == DisintegrationPlacementMode::UniformSurface)
			{
				ImGui::TextWrapped("均一表面配置はSweep Erosionで形を保って蝕む表現に推奨です。");
			}
			else if (sequenceParams.placementMode == DisintegrationPlacementMode::AlignedSurfaceGrid)
			{
				ImGui::TextWrapped("整列表面配置はAABBではなくモデル表面上の格子候補へ配置します。");
			}
			else if (sequenceParams.placementMode == DisintegrationPlacementMode::VoxelFill)
			{
				ImGui::TextWrapped("ボクセル敷き詰め配置は同じ3Dグリッド配置を再構築と崩壊で共有します。");
			}
			ImGui::SeparatorText("侵食崩壊##BlockSequence");
			ImGui::Checkbox("侵食を使う##BlockSequence", &sequenceParams.useSweepErosion);
			const char* sequenceErosionModeLabels[] = { "方向侵食", "中心侵食" };
			int sequenceErosionModeIndex = sequenceParams.erosionMode == ErosionMode::CenterOut ? 1 : 0;
			if (ImGui::Combo("侵食モード##BlockSequence", &sequenceErosionModeIndex, sequenceErosionModeLabels, IM_ARRAYSIZE(sequenceErosionModeLabels)))
			{
				sequenceParams.erosionMode = sequenceErosionModeIndex == 1 ? ErosionMode::CenterOut : ErosionMode::DirectionalSweep;
				sequenceParams.useSweepErosion = true;
			}
			ImGui::SeparatorText("方向侵食##BlockSequence");
			ImGui::DragFloat3("侵食方向##BlockSequence", &sequenceParams.sweepDirection.x, 0.01f, -1.0f, 1.0f);
			ImGui::SliderFloat("侵食時間##BlockSequence", &sequenceParams.sweepDuration, 0.05f, 8.0f);
			ImGui::SliderFloat("侵食ノイズ強度##BlockSequence", &sequenceParams.erosionNoisePower, 0.0f, 4.0f);
			ImGui::SliderFloat("侵食境界幅##BlockSequence", &sequenceParams.erosionBandWidth, 0.0f, 2.0f);
			ImGui::SeparatorText("中心侵食##BlockSequence");
			ImGui::Checkbox("モデル中心を使う##BlockSequence", &sequenceParams.useModelCenterAsErosionCenter);
			ImGui::DragFloat3("侵食中心##BlockSequence", &sequenceParams.erosionCenter.x, 0.01f);
			ImGui::SliderFloat("中心侵食時間##BlockSequence", &sequenceParams.centerErosionDuration, 0.05f, 8.0f);
			ImGui::SliderFloat("中心侵食ノイズ強度##BlockSequence", &sequenceParams.centerErosionNoisePower, 0.0f, 4.0f);
			ImGui::SliderFloat("中心発光幅##BlockSequence", &sequenceParams.centerGlowWidth, 0.001f, 2.0f);
			ImGui::SliderFloat("侵食前の発光幅##BlockSequence", &sequenceParams.preGlowWidth, 0.0f, 2.0f);
			ImGui::SliderFloat("侵食後の発光幅##BlockSequence", &sequenceParams.postGlowWidth, 0.0f, 2.0f);
			ImGui::SliderFloat("発光エッジ幅##BlockSequence", &sequenceParams.glowEdgeWidth, 0.001f, 2.0f);
			ImGui::SliderFloat("発光強度##BlockSequence", &sequenceParams.glowIntensity, 0.0f, 8.0f);
			ImGui::SliderFloat("発光の鋭さ##BlockSequence", &sequenceParams.glowSharpness, 0.1f, 8.0f);
			ImGui::ColorEdit4("発光色##BlockSequence", &sequenceParams.glowColor.x);

			if (ImGui::Button("モデル再読み込み##BlockSequence"))
			{
				pendingDebugModelBlockSequencePath_ = sequenceModelPathBuffer;
				debugModelBlockSequenceRequest_ = DebugModelBlockSequenceRequest::Reload;
				debugModelBlockSequenceLog_ = "シーケンスモデル再読み込みを予約: " + pendingDebugModelBlockSequencePath_;
			}

			if (ImGui::Button("再構築 → 崩壊 を再生"))
			{
				pendingDebugModelBlockSequencePath_ = sequenceModelPathBuffer;
				debugModelBlockSequenceRequest_ = DebugModelBlockSequenceRequest::PlaySpawnThenDisintegrate;
				debugModelBlockSequenceLog_ = "再構築 → 崩壊 の再生を予約: " + pendingDebugModelBlockSequencePath_;
			}

			if (ImGui::Button("崩壊 → 再構築 を再生"))
			{
				pendingDebugModelBlockSequencePath_ = sequenceModelPathBuffer;
				debugModelBlockSequenceRequest_ = DebugModelBlockSequenceRequest::PlayDisintegrateThenReconstruct;
				debugModelBlockSequenceLog_ = "崩壊 → 再構築 の再生を予約: " + pendingDebugModelBlockSequencePath_;
			}

			if (ImGui::Button("ループ再生"))
			{
				pendingDebugModelBlockSequencePath_ = sequenceModelPathBuffer;
				debugModelBlockSequenceRequest_ = DebugModelBlockSequenceRequest::PlayLoop;
				debugModelBlockSequenceLog_ = "ループ再生を予約: " + pendingDebugModelBlockSequencePath_;
			}

			ImGui::SameLine();
			if (ImGui::Button(debugModelBlockSequencePaused_ ? "再開##BlockSequence" : "一時停止##BlockSequence"))
			{
				debugModelBlockSequencePaused_ = !debugModelBlockSequencePaused_;
			}

			ImGui::SameLine();
			if (ImGui::Button("1フレーム送り##BlockSequence"))
			{
				debugModelBlockSequenceStepFrame_ = true;
				debugModelBlockSequencePaused_ = true;
			}

			ImGui::SameLine();
			if (ImGui::Button("停止"))
			{
				debugModelBlockSequenceRequest_ = DebugModelBlockSequenceRequest::Stop;
				debugModelBlockSequenceLog_ = "シーケンス停止を予約しました。";
			}

			ImGui::SameLine();
			if (ImGui::Button("リセット"))
			{
				debugModelBlockSequenceRequest_ = DebugModelBlockSequenceRequest::Reset;
				debugModelBlockSequenceLog_ = "シーケンスリセットを予約しました。";
			}

			ImGui::Separator();
			ImGui::Text("現在の状態: %s", debugModelBlockSequence_->GetStateName());
			ImGui::Text("シーケンス経過時間: %.2f", debugModelBlockSequence_->GetSequenceElapsed());
			ImGui::Text("再構築完了: %s", debugModelBlockSequence_->IsReconstructionCompleted() ? "はい" : "いいえ");
			ImGui::Text("崩壊完了: %s", debugModelBlockSequence_->IsDisintegrationCompleted() ? "はい" : "いいえ");
			ImGui::Text("モデル表示: %s", debugModelBlockSequence_->IsModelVisible() ? "はい" : "いいえ");
		}
		ImGui::TextWrapped("%s", debugModelBlockSequenceLog_.c_str());
		ImGui::End();
	}
#endif // USE_IMGUI
}

void DisintegrationDebugController::UpdateDebugDisintegrationTest(float deltaTime)
{
	if (input_ && input_->TriggerKey(DIK_F9))
	{
		pendingDebugDisintegrationPlay_ = true;
	}

	ProcessDebugDisintegrationRequest();

	if (debugDisintegrationModel_)
	{
		debugDisintegrationModel_->SetTranslate(debugDisintegrationPosition_);
		debugDisintegrationModel_->SetRotate(debugDisintegrationRotation_);
		debugDisintegrationModel_->SetScale(debugDisintegrationScale_);
		debugDisintegrationModel_->Update();
	}

	if (debugDisintegrationEffect_)
	{
		const bool shouldAdvanceEffect = !debugDisintegrationPaused_ || debugDisintegrationStepFrame_;
		debugDisintegrationEffect_->Update(shouldAdvanceEffect ? deltaTime : 0.0f);
		debugDisintegrationStepFrame_ = false;
		debugDisintegrationModelVisible_ = !debugDisintegrationEffect_->IsActive();
	}
}

void DisintegrationDebugController::ReloadDebugDisintegrationModel()
{
	debugDisintegrationModel_ = std::make_unique<Object3D>();
	debugDisintegrationModel_->Initialize(debugDisintegrationModelPath_);
	debugDisintegrationModel_->SetTranslate(debugDisintegrationPosition_);
	debugDisintegrationModel_->SetRotate(debugDisintegrationRotation_);
	debugDisintegrationModel_->SetScale(debugDisintegrationScale_);
	debugDisintegrationModel_->Update();
	debugDisintegrationModelVisible_ = true;
	debugDisintegrationLog_ = "テストモデルを再読み込み: " + debugDisintegrationModelPath_;
	DebugLog(debugDisintegrationLog_);
}

void DisintegrationDebugController::ProcessDebugDisintegrationRequest()
{
	if (!pendingDebugDisintegrationReload_ && !pendingDebugDisintegrationPlay_ && !pendingDebugDisintegrationReset_)
	{
		return;
	}

	if (!pendingDebugDisintegrationPath_.empty())
	{
		debugDisintegrationModelPath_ = pendingDebugDisintegrationPath_;
	}

	if (pendingDebugDisintegrationReload_)
	{
		ReloadDebugDisintegrationModel();
	}

	if (pendingDebugDisintegrationReset_ && debugDisintegrationEffect_)
	{
		debugDisintegrationEffect_->Initialize();
		debugDisintegrationModelVisible_ = true;
		debugDisintegrationPaused_ = false;
		debugDisintegrationStepFrame_ = false;
	}

	if (pendingDebugDisintegrationPlay_)
	{
		PlayDebugDisintegrationEffect();
	}

	pendingDebugDisintegrationReload_ = false;
	pendingDebugDisintegrationPlay_ = false;
	pendingDebugDisintegrationReset_ = false;
	pendingDebugDisintegrationPath_.clear();
}

void DisintegrationDebugController::PlayDebugDisintegrationEffect()
{
	if (!debugDisintegrationEffect_)
	{
		return;
	}

	const Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(
		debugDisintegrationScale_,
		debugDisintegrationRotation_,
		debugDisintegrationPosition_);

	// 表示中のテストモデルと同じ行列から生成し、形状サンプリングのずれを確認しやすくする。
	debugDisintegrationEffect_->PlayFromModel(debugDisintegrationModelPath_, worldMatrix);
	debugDisintegrationPaused_ = false;
	debugDisintegrationStepFrame_ = false;
	debugDisintegrationModelVisible_ = !debugDisintegrationEffect_->IsActive();
	debugDisintegrationLog_ = "崩壊再生: " + debugDisintegrationModelPath_;
	DebugLog(debugDisintegrationLog_);
}

void DisintegrationDebugController::UpdateDebugReconstructionTest(float deltaTime)
{
	if (input_ && input_->TriggerKey(DIK_F10))
	{
		pendingDebugReconstructionPlay_ = true;
	}

	ProcessDebugReconstructionRequest();

	if (debugReconstructionModel_)
	{
		debugReconstructionModel_->SetTranslate(debugReconstructionPosition_);
		debugReconstructionModel_->SetRotate(debugReconstructionRotation_);
		debugReconstructionModel_->SetScale(debugReconstructionScale_);
		debugReconstructionModel_->Update();
	}

	if (debugReconstructionEffect_)
	{
		debugReconstructionEffect_->Update(deltaTime);
		if (debugReconstructionEffect_->IsActive())
		{
			debugReconstructionModelVisible_ = debugReconstructionEffect_->ShouldShowFinalModel();
		}
		else if (debugReconstructionEffect_->IsComplete())
		{
			debugReconstructionModelVisible_ = debugReconstructionEffect_->ShouldShowFinalModel();
		}
	}
}

void DisintegrationDebugController::PlayDebugReconstructionEffect()
{
	if (!debugReconstructionEffect_)
	{
		return;
	}

	const Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(
		debugReconstructionScale_,
		debugReconstructionRotation_,
		debugReconstructionPosition_);

	// 表面サンプルを最終到達位置にして、散らばったブロックをモデル形状へ集める。
	debugReconstructionEffect_->PlayFromModel(debugReconstructionModelPath_, worldMatrix);
	debugReconstructionModelVisible_ = debugReconstructionEffect_->ShouldShowFinalModel();
	debugReconstructionLog_ = "再構築再生: " + debugReconstructionModelPath_;
	DebugLog(debugReconstructionLog_);
}

void DisintegrationDebugController::ReloadDebugReconstructionModel()
{
	debugReconstructionModel_ = std::make_unique<Object3D>();
	debugReconstructionModel_->Initialize(debugReconstructionModelPath_);
	debugReconstructionModel_->SetTranslate(debugReconstructionPosition_);
	debugReconstructionModel_->SetRotate(debugReconstructionRotation_);
	debugReconstructionModel_->SetScale(debugReconstructionScale_);
	debugReconstructionModel_->Update();
	debugReconstructionModelVisible_ = true;
	debugReconstructionLog_ = "再構築テストモデルを再読み込み: " + debugReconstructionModelPath_;
	DebugLog(debugReconstructionLog_);
}

void DisintegrationDebugController::ProcessDebugReconstructionRequest()
{
	if (!pendingDebugReconstructionReload_ && !pendingDebugReconstructionPlay_)
	{
		return;
	}

	// ImGui からのGPUリソース更新要求は、コマンドリスト記録前のUpdateでだけ実行する。
	if (!pendingDebugReconstructionPath_.empty())
	{
		debugReconstructionModelPath_ = pendingDebugReconstructionPath_;
	}

	if (pendingDebugReconstructionReload_)
	{
		ReloadDebugReconstructionModel();
	}

	if (pendingDebugReconstructionPlay_)
	{
		PlayDebugReconstructionEffect();
	}

	pendingDebugReconstructionReload_ = false;
	pendingDebugReconstructionPlay_ = false;
	pendingDebugReconstructionPath_.clear();
}


void DisintegrationDebugController::UpdateDebugModelBlockSequence(float deltaTime)
{
	ProcessDebugModelBlockSequenceRequest();

	if (debugModelBlockSequenceModel_)
	{
		debugModelBlockSequenceModel_->SetTranslate(debugModelBlockSequencePosition_);
		debugModelBlockSequenceModel_->SetRotate(debugModelBlockSequenceRotation_);
		debugModelBlockSequenceModel_->SetScale(debugModelBlockSequenceScale_);
		debugModelBlockSequenceModel_->Update();
	}

	if (debugModelBlockSequence_)
	{
		const bool shouldAdvanceSequence = !debugModelBlockSequencePaused_ || debugModelBlockSequenceStepFrame_;
		debugModelBlockSequence_->Update(shouldAdvanceSequence ? deltaTime : 0.0f);
		debugModelBlockSequenceStepFrame_ = false;
	}
}

void DisintegrationDebugController::ReloadDebugModelBlockSequenceModel()
{
	debugModelBlockSequenceModel_ = std::make_unique<Object3D>();
	debugModelBlockSequenceModel_->Initialize(debugModelBlockSequenceModelPath_);
	debugModelBlockSequenceModel_->SetTranslate(debugModelBlockSequencePosition_);
	debugModelBlockSequenceModel_->SetRotate(debugModelBlockSequenceRotation_);
	debugModelBlockSequenceModel_->SetScale(debugModelBlockSequenceScale_);
	debugModelBlockSequenceModel_->Update();
	debugModelBlockSequenceLog_ = "シーケンステストモデルを再読み込み: " + debugModelBlockSequenceModelPath_;
	DebugLog(debugModelBlockSequenceLog_);
}

void DisintegrationDebugController::ProcessDebugModelBlockSequenceRequest()
{
	if (debugModelBlockSequenceRequest_ == DebugModelBlockSequenceRequest::None)
	{
		return;
	}

	if (!pendingDebugModelBlockSequencePath_.empty())
	{
		debugModelBlockSequenceModelPath_ = pendingDebugModelBlockSequencePath_;
	}

	const DebugModelBlockSequenceRequest request = debugModelBlockSequenceRequest_;
	debugModelBlockSequenceRequest_ = DebugModelBlockSequenceRequest::None;
	pendingDebugModelBlockSequencePath_.clear();

	if (!debugModelBlockSequence_)
	{
		return;
	}

	switch (request)
	{
	case DebugModelBlockSequenceRequest::Reload:
		ReloadDebugModelBlockSequenceModel();
		break;
	case DebugModelBlockSequenceRequest::PlaySpawnThenDisintegrate:
		debugModelBlockSequencePaused_ = false;
		debugModelBlockSequenceStepFrame_ = false;
		ReloadDebugModelBlockSequenceModel();
		debugModelBlockSequence_->PlaySpawnThenDisintegrate(debugModelBlockSequenceModelPath_, MakeDebugModelBlockSequenceWorldMatrix());
		debugModelBlockSequenceLog_ = "再構築 → 崩壊 を再生: " + debugModelBlockSequenceModelPath_;
		DebugLog(debugModelBlockSequenceLog_);
		break;
	case DebugModelBlockSequenceRequest::PlayDisintegrateThenReconstruct:
		debugModelBlockSequencePaused_ = false;
		debugModelBlockSequenceStepFrame_ = false;
		ReloadDebugModelBlockSequenceModel();
		debugModelBlockSequence_->PlayDisintegrateThenReconstruct(debugModelBlockSequenceModelPath_, MakeDebugModelBlockSequenceWorldMatrix());
		debugModelBlockSequenceLog_ = "崩壊 → 再構築 を再生: " + debugModelBlockSequenceModelPath_;
		DebugLog(debugModelBlockSequenceLog_);
		break;
	case DebugModelBlockSequenceRequest::PlayLoop:
		debugModelBlockSequencePaused_ = false;
		debugModelBlockSequenceStepFrame_ = false;
		ReloadDebugModelBlockSequenceModel();
		debugModelBlockSequence_->PlayLoop(debugModelBlockSequenceModelPath_, MakeDebugModelBlockSequenceWorldMatrix());
		debugModelBlockSequenceLog_ = "ループ再生: " + debugModelBlockSequenceModelPath_;
		DebugLog(debugModelBlockSequenceLog_);
		break;
	case DebugModelBlockSequenceRequest::Stop:
		debugModelBlockSequencePaused_ = false;
		debugModelBlockSequenceStepFrame_ = false;
		debugModelBlockSequence_->Stop(false);
		debugModelBlockSequenceLog_ = "シーケンスを停止しました。";
		DebugLog(debugModelBlockSequenceLog_);
		break;
	case DebugModelBlockSequenceRequest::Reset:
		debugModelBlockSequencePaused_ = false;
		debugModelBlockSequenceStepFrame_ = false;
		debugModelBlockSequence_->Reset();
		debugModelBlockSequenceLog_ = "シーケンスをリセットしました。";
		DebugLog(debugModelBlockSequenceLog_);
		break;
	case DebugModelBlockSequenceRequest::None:
	default:
		break;
	}
}

Matrix4x4 DisintegrationDebugController::MakeDebugModelBlockSequenceWorldMatrix() const
{
	return Matrix4x4::MakeAffineMatrix(
		debugModelBlockSequenceScale_,
		debugModelBlockSequenceRotation_,
		debugModelBlockSequencePosition_);
}