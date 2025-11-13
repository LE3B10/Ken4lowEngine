#include "D3DResourceLeakChecker.h"
#include <dxgidebug.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

/// -------------------------------------------------------------
///			　				デストラクタ
/// -------------------------------------------------------------
D3DResourceLeakChecker::~D3DResourceLeakChecker()
{
	// リソースリークチェック
	Microsoft::WRL::ComPtr<IDXGIDebug> debug;

	// デバッグインターフェースの取得
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
	{
		//ライブオブジェクトの報告
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);   // 全てのライブオブジェクトを報告
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);   // アプリケーションのライブオブジェクトを報告
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL); // D3D12のライブオブジェクトを報告
	}
}
