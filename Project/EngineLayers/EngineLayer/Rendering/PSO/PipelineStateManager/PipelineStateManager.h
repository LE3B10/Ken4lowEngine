#pragma once
#include "PipelineState.h"

#include <vector>
#include <memory>
#include <thread>
#include <mutex>


/// -------------------------------------------------------------
///				　パイプラインを管理するクラス
/// -------------------------------------------------------------
class PipelineStateManager
{
public: /// ---------- メンバ関数 ---------- ///

	// パイプラインを追加
	void AddPipeline(std::unique_ptr<PipelineState> pipeline);

	// 全てのパイプライン
	void RenderAll();

public: /// ---------- ゲッター ---------- ///

	// パイプラインを取得
	PipelineState* GetPipeline(size_t index);

private: /// ---------- メンバ関数 ---------- ///

	std::vector<std::unique_ptr<PipelineState>> pipelines_;
	std::vector<std::thread> renderThreads_;
	std::mutex renderMutex_;
	bool running_ = true;
};

