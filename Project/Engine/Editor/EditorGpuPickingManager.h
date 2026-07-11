#pragma once

#include "EditorContext.h"
#include "EditorObjectInfo.h"
#include "EditorViewportController.h"

#include <BaseScene.h>
#include <DSVManager.h>
#include <DirectXCommon.h>
#include <GameViewportConstants.h>
#include <ObjectIdPipeline.h>
#include <RTVManager.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// R32_UINTのObject-ID RenderTargetへEditor Componentを描画し、クリックした1PixelだけをReadbackします。
	/// </summary>
	class EditorGpuPickingManager
	{
	public:
		struct ExecuteResult
		{
			bool executed = false;
			bool hit = false;
			std::string message;
		};

		static EditorGpuPickingManager* GetInstance()
		{
			static EditorGpuPickingManager instance;
			return &instance;
		}

		void Initialize()
		{
			if (initialized_)
			{
				return;
			}

			dxCommon_ = DirectXCommon::GetInstance();
			if (!dxCommon_ || !dxCommon_->GetDevice())
			{
				return;
			}

			CreateObjectIdRenderTarget();
			CreateDepthBuffer();
			CreateReadbackBuffer();
			viewport_ = D3D12_VIEWPORT(
				0.0f,
				0.0f,
				static_cast<float>(GameViewportConstants::Width),
				static_cast<float>(GameViewportConstants::Height),
				0.0f,
				1.0f);
			scissorRect_ = {
				0,
				0,
				static_cast<LONG>(GameViewportConstants::Width),
				static_cast<LONG>(GameViewportConstants::Height)
			};
			ObjectIdPipeline::GetInstance()->Initialize();
			initialized_ = objectIdBuffer_ && depthBuffer_ && readbackBuffer_;
		}

		void Finalize()
		{
			pendingRequest_ = {};
			objectIdBuffer_.Reset();
			depthBuffer_.Reset();
			readbackBuffer_.Reset();

			if (rtvIndex_ != UINT32_MAX)
			{
				RTVManager::GetInstance()->Free(rtvIndex_);
				rtvIndex_ = UINT32_MAX;
			}
			if (dsvIndex_ != UINT32_MAX)
			{
				DSVManager::GetInstance()->Free(dsvIndex_);
				dsvIndex_ = UINT32_MAX;
			}

			rtvHandle_ = {};
			dsvHandle_ = {};
			ObjectIdPipeline::GetInstance()->Finalize();
			dxCommon_ = nullptr;
			initialized_ = false;
		}

		void RequestPick(uint32_t pixelX, uint32_t pixelY, EditorViewportSelectionMode selectionMode)
		{
			pendingRequest_.pixelX = std::min(pixelX, GameViewportConstants::Width - 1u);
			pendingRequest_.pixelY = std::min(pixelY, GameViewportConstants::Height - 1u);
			pendingRequest_.selectionMode = selectionMode;
			pendingRequest_.pending = true; // ImGui入力を受けたフレームの描画順内でObject-ID Passを実行する。
		}

		bool HasPendingRequest() const { return pendingRequest_.pending; }

		ExecuteResult Execute(BaseScene* scene)
		{
			ExecuteResult result{};
			if (!pendingRequest_.pending)
			{
				return result;
			}

			result.executed = true;
			const PickRequest request = pendingRequest_;
			pendingRequest_.pending = false;

			if (!initialized_)
			{
				Initialize();
			}
			if (!initialized_ || !scene)
			{
				result.message = "Object-ID Pickingを実行できるSceneまたはGPU Resourceがありません。";
				return result;
			}

			std::vector<EditorObjectInfo> objects;
			scene->CollectEditorObjects(objects);
			std::vector<EditorObjectInfo> gpuIdTable(1); // ID=0は背景専用として予約する。
			for (const EditorObjectInfo& object : objects)
			{
				if (!object.canDrawObjectId || !object.drawObjectId)
				{
					continue;
				}
				gpuIdTable.push_back(object);
			}

			if (gpuIdTable.size() <= 1)
			{
				EditorContext::GetInstance()->GetSelection().Clear();
				result.message = "Object-ID Passへ描画できるComponentがないため選択を解除しました。";
				return result;
			}

			ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
			commandList->RSSetViewports(1, &viewport_);
			commandList->RSSetScissorRects(1, &scissorRect_);
			commandList->OMSetRenderTargets(1, &rtvHandle_, false, &dsvHandle_);
			const float clearId[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			commandList->ClearRenderTargetView(rtvHandle_, clearId, 0, nullptr);
			commandList->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			for (uint32_t gpuId = 1; gpuId < static_cast<uint32_t>(gpuIdTable.size()); ++gpuId)
			{
				gpuIdTable[gpuId].DrawObjectId(gpuId);
			}

			dxCommon_->ResourceTransition(
				objectIdBuffer_.Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_COPY_SOURCE);
			CopyRequestedPixel(commandList, request.pixelX, request.pixelY);
			dxCommon_->ResourceTransition(
				objectIdBuffer_.Get(),
				D3D12_RESOURCE_STATE_COPY_SOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET);

			// Pickingはクリック時だけGPU完了を待ち、1PixelのIDを同じフレームでSelectionへ反映する。
			dxCommon_->GetCommandManager()->ExecuteAndWait();
			const uint32_t gpuId = ReadbackObjectId();
			if (gpuId == 0 || gpuId >= gpuIdTable.size())
			{
				EditorContext::GetInstance()->GetSelection().Clear();
				result.message = "Viewportの空き領域を選択したため選択を解除しました。";
				return result;
			}

			const EditorObjectInfo* selected = &gpuIdTable[gpuId];
			if (request.selectionMode == EditorViewportSelectionMode::Actor)
			{
				selected = FindActorAncestor(objects, *selected);
			}

			if (!selected)
			{
				EditorContext::GetInstance()->GetSelection().Clear();
				result.message = "Object-IDは取得できましたが選択対象を解決できませんでした。";
				return result;
			}

			EditorContext::GetInstance()->GetSelection().Select(*selected);
			result.hit = true;
			result.message = "Viewport選択: " + selected->displayName + " / Editor ID=" + std::to_string(selected->id);
			return result;
		}

	private:
		struct PickRequest
		{
			uint32_t pixelX = 0;
			uint32_t pixelY = 0;
			EditorViewportSelectionMode selectionMode = EditorViewportSelectionMode::Actor;
			bool pending = false;
		};

		EditorGpuPickingManager() = default;
		~EditorGpuPickingManager() = default;
		EditorGpuPickingManager(const EditorGpuPickingManager&) = delete;
		EditorGpuPickingManager& operator=(const EditorGpuPickingManager&) = delete;

		void CreateObjectIdRenderTarget()
		{
			D3D12_RESOURCE_DESC desc{};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			desc.Width = GameViewportConstants::Width;
			desc.Height = GameViewportConstants::Height;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.Format = DXGI_FORMAT_R32_UINT;
			desc.SampleDesc.Count = 1;
			desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
			heapProperties.CreationNodeMask = 1;
			heapProperties.VisibleNodeMask = 1;

			D3D12_CLEAR_VALUE clearValue{};
			clearValue.Format = DXGI_FORMAT_R32_UINT;
			const HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				&clearValue,
				IID_PPV_ARGS(&objectIdBuffer_));
			if (FAILED(hr))
			{
				return;
			}
			objectIdBuffer_->SetName(L"EditorObjectIdBuffer_R32_UINT");

			rtvIndex_ = RTVManager::GetInstance()->Allocate();
			rtvHandle_ = RTVManager::GetInstance()->GetCPUDescriptorHandle(rtvIndex_);
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
			rtvDesc.Format = DXGI_FORMAT_R32_UINT;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			dxCommon_->GetDevice()->CreateRenderTargetView(objectIdBuffer_.Get(), &rtvDesc, rtvHandle_);
		}

		void CreateDepthBuffer()
		{
			D3D12_CLEAR_VALUE clearValue{};
			depthBuffer_ = DSVManager::GetInstance()->CreateDepthStencilBuffer(
				GameViewportConstants::Width,
				GameViewportConstants::Height,
				DXGI_FORMAT_D24_UNORM_S8_UINT,
				clearValue);
			if (!depthBuffer_)
			{
				return;
			}
			depthBuffer_->SetName(L"EditorObjectIdDepth");
			dsvIndex_ = DSVManager::GetInstance()->Allocate();
			DSVManager::GetInstance()->CreateDSVForDepthBuffer(dsvIndex_, depthBuffer_.Get());
			dsvHandle_ = DSVManager::GetInstance()->GetCPUDescriptorHandle(dsvIndex_);
		}

		void CreateReadbackBuffer()
		{
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
			heapProperties.CreationNodeMask = 1;
			heapProperties.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC desc{};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Width = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
			desc.Height = 1;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.SampleDesc.Count = 1;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			const HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&readbackBuffer_));
			if (SUCCEEDED(hr))
			{
				readbackBuffer_->SetName(L"EditorObjectIdReadback");
			}
		}

		void CopyRequestedPixel(ID3D12GraphicsCommandList* commandList, uint32_t pixelX, uint32_t pixelY)
		{
			D3D12_TEXTURE_COPY_LOCATION destination{};
			destination.pResource = readbackBuffer_.Get();
			destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			destination.PlacedFootprint.Offset = 0;
			destination.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_UINT;
			destination.PlacedFootprint.Footprint.Width = 1;
			destination.PlacedFootprint.Footprint.Height = 1;
			destination.PlacedFootprint.Footprint.Depth = 1;
			destination.PlacedFootprint.Footprint.RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;

			D3D12_TEXTURE_COPY_LOCATION source{};
			source.pResource = objectIdBuffer_.Get();
			source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			source.SubresourceIndex = 0;

			const D3D12_BOX sourceBox{
				pixelX,
				pixelY,
				0,
				pixelX + 1,
				pixelY + 1,
				1
			};
			commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, &sourceBox);
		}

		uint32_t ReadbackObjectId() const
		{
			if (!readbackBuffer_)
			{
				return 0;
			}

			void* mapped = nullptr;
			const D3D12_RANGE readRange{ 0, sizeof(uint32_t) };
			if (FAILED(readbackBuffer_->Map(0, &readRange, &mapped)) || !mapped)
			{
				return 0;
			}
			const uint32_t objectId = *static_cast<const uint32_t*>(mapped);
			const D3D12_RANGE writeRange{ 0, 0 };
			readbackBuffer_->Unmap(0, &writeRange);
			return objectId;
		}

		static const EditorObjectInfo* FindActorAncestor(
			const std::vector<EditorObjectInfo>& objects,
			const EditorObjectInfo& hitObject)
		{
			const EditorObjectInfo* current = &hitObject;
			for (std::size_t depth = 0; current && depth <= objects.size(); ++depth)
			{
				if (current->objectKind == EditorObjectKind::Actor)
				{
					return current;
				}
				if (current->parentId == 0)
				{
					return nullptr;
				}

				const auto parent = std::find_if(objects.begin(), objects.end(), [current](const EditorObjectInfo& object)
					{
						return object.id == current->parentId;
					});
				current = parent == objects.end() ? nullptr : &(*parent);
			}
			return nullptr;
		}

		DirectXCommon* dxCommon_ = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> objectIdBuffer_;
		Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_;
		Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer_;
		uint32_t rtvIndex_ = UINT32_MAX;
		uint32_t dsvIndex_ = UINT32_MAX;
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};
		D3D12_VIEWPORT viewport_{};
		D3D12_RECT scissorRect_{};
		PickRequest pendingRequest_{};
		bool initialized_ = false;
	};
} // namespace Ken4lowEngine
