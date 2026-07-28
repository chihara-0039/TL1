#include "D3DResourceLeakChecker.h"
#include <dxgidebug.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

D3DResourceLeakChecker::~D3DResourceLeakChecker() {
	
	//リソースリークチェック
	Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
	// DXGIのデバッグインターフェースを取得
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
		// DXGI全体のリソースチェック（アプリが作ったリソースがまだ残ってるか確認）
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
	}
}
