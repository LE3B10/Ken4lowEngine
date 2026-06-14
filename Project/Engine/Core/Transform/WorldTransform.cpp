#include "WorldTransform.h"
#include <ResourceManager.h>
#include <DirectXCommon.h>
#include <CameraManager.h>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///                 ワールド変換行列初期化処理
	/// -------------------------------------------------------------
	void WorldTransform::Initialize()
	{
		// WVP / World / 逆転置行列をまとめて送るための定数バッファを作成する。
		wvpResource = ResourceManager::CreateBufferResource(DirectXCommon::GetInstance()->GetDevice(), sizeof(TransformationMatrix));

		// CPU から毎フレーム行列を書き換えられるよう、バッファをマッピングする。
		wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));

		// 初期状態では何も変換しない単位行列を入れておき、未更新時の不正な描画を防ぐ。
		wvpData->World = Matrix4x4::MakeIdentity();
		wvpData->WVP = Matrix4x4::MakeIdentity();
		wvpData->WorldInversedTranspose = Matrix4x4::MakeIdentity();

		// CPU 側のキャッシュも同じ内容で初期化しておく。
		matWorld_ = Matrix4x4::MakeIdentity();
	}

	/// -------------------------------------------------------------
	///                 ワールド変換行列更新処理
	/// -------------------------------------------------------------
	void WorldTransform::Update()
	{
		// scale / rotate / translate から、このオブジェクト単体のローカル行列を作成する。
		Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(scale_, rotate_, translate_);

		// 親がある場合は親のワールド行列を掛け、親子関係込みのワールド行列にする。
		if (parent_)
		{
			worldMatrix = Matrix4x4::Multiply(worldMatrix, parent_->matWorld_);
		}

		// 親の回転を含めた見た目上のワールド回転を保持する。
		worldRotate_ = parent_ ? parent_->worldRotate_ + rotate_ : rotate_;

		// 行列の平行移動成分から、外部参照用のワールド座標を取り出す。
		worldTranslate_ = { worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2] };

		// 現在アクティブなカメラを基準に、描画用の ViewProjection 行列を取得する。
		const Matrix4x4 viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();

		// シェーダーで頂点をクリップ空間へ変換するため、World と ViewProjection を合成する。
		const Matrix4x4 worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjection);

		// CPU 側のキャッシュと GPU 側の定数バッファを同じ内容に更新する。
		matWorld_ = worldMatrix;
		wvpData->WVP = worldViewProjectionMatrix;
		wvpData->World = worldMatrix;
		// 法線変換用に、拡大縮小の影響を補正できる World の逆転置行列を送る。
		wvpData->WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(worldMatrix));
	}

	/// -------------------------------------------------------------
	///                 合成済みワールド行列の反映処理
	/// -------------------------------------------------------------
	void WorldTransform::UpdateWithWorldMatrix(const Matrix4x4& worldMatrix)
	{
		// 外部で作成された行列をそのまま採用し、ローカルの scale/rotate/translate からは再計算しない。
		matWorld_ = worldMatrix;

		// 行列の平行移動成分だけは、当たり判定やデバッグ表示で使えるように取り出しておく。
		worldTranslate_ = { worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2] };

		// 現在のアクティブカメラに合わせて WVP 行列を作成する。
		const Matrix4x4 viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		const Matrix4x4 worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjection);

		// 合成済み行列を GPU 側の定数バッファへ反映する。
		wvpData->WVP = worldViewProjectionMatrix;
		wvpData->World = worldMatrix;
		wvpData->WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(worldMatrix));
	}

	/// -------------------------------------------------------------
	///                 パイプライン設定処理
	/// -------------------------------------------------------------
	void WorldTransform::SetPipeline(UINT rootParameterIndex)
	{
		// 現在のコマンドリストを取得し、この描画呼び出しで使う行列バッファを設定する。
		auto commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();

		// ルートパラメータへ GPU 仮想アドレスを渡し、頂点シェーダーから参照できるようにする。
		commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, wvpResource->GetGPUVirtualAddress());
	}

} // namespace Ken4lowEngine
