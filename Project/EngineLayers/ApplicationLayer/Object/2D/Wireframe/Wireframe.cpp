#include "Wireframe.h"
#include "WinApp.h"
#include "LogString.h"
#include "DirectXCommon.h"
#include "ShaderManager.h"
#include "DebugCamera.h"
#include "ResourceManager.h"

/// -------------------------------------------------------------
///				　	シングルトンインスタンス
/// -------------------------------------------------------------
Wireframe* Wireframe::GetInstance()
{
	static Wireframe instance;
	return &instance;
}


/// -------------------------------------------------------------
///				　	         初期化処理
/// -------------------------------------------------------------
void Wireframe::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
	isDebugCamera_ = false;

	projectionMatrix_ = Matrix4x4::MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 1.0f);
	viewProjectionMatrix_ = Matrix4x4::MakeViewportMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 1.0f);

	// パイプラインステートの生成
	// 三角形用のPSOを作成
	CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE, triangleRootSignature_, trianglePipelineState_);

	// 線用のPSOを作成
	CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE, lineRootSignature_, linePipelineState_);

	// 座標変換行列データの生成
	CreateTransformationMatrix();

	// 三角形の頂点データを生成
	triangleData_ = new TriangleData;
	CreateTriangleVertexData(triangleData_);

	// 矩形の頂点座標を生成
	boxData_ = new BoxData();
	CreateBoxVertexData(boxData_);

	// 線の頂点を生成
	lineData_ = new LineData();
	CreateLineVertexData(lineData_);

	// 球の頂点座標を計算
	CalcSphereVertexData();
}


/// -------------------------------------------------------------
///				　	        　更新処理
/// -------------------------------------------------------------
void Wireframe::Update()
{
	if (!isDebugCamera_)
	{
		transformationMatrixData_->WVP = camera_->GetViewMatrix() * camera_->GetProjectionMatrix();
	}
	else
	{
		transformationMatrixData_->WVP = DebugCamera::GetInstance()->GetViewProjectionMatrix();
	}
}


/// -------------------------------------------------------------
///				　	          描画処理
/// -------------------------------------------------------------
void Wireframe::Draw()
{
	auto commandList = dxCommon_->GetCommandList();

#pragma region ---------- 線の描画 ----------

	commandList->SetGraphicsRootSignature(lineRootSignature_.Get());										// ルートシグネチャの設定
	commandList->SetPipelineState(linePipelineState_.Get());												// パイプラインステートの設定
	commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);									// プリミティブトポロジーの設定
	commandList->IASetVertexBuffers(0, 1, &lineData_->vertexBufferView);									// 頂点バッファビューの設定
	commandList->SetGraphicsRootConstantBufferView(0, transformationMatrixBuffer_->GetGPUVirtualAddress()); // 座標変換行列の設定
	commandList->DrawInstanced(lineIndex_, lineIndex_ / kLineVertexCount, 0, 0);							// 描画

#pragma endregion ---------------------------


#pragma region ---------- 三角形の描画 ----------

	commandList->SetGraphicsRootSignature(triangleRootSignature_.Get());									// ルートシグネチャの設定
	commandList->SetPipelineState(trianglePipelineState_.Get());											// パイプラインステートの設定
	commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);								// プリミティブトポロジーの設定
	commandList->IASetVertexBuffers(0, 1, &triangleData_->vertexBufferView);								// 頂点バッファビューの設定
	commandList->SetGraphicsRootConstantBufferView(0, transformationMatrixBuffer_->GetGPUVirtualAddress()); // 座標変換行列の設定
	commandList->DrawInstanced(triangleIndex_, triangleIndex_ / kTriangleVertexCount, 0, 0);				// 描画

#pragma endregion -------------------------------


#pragma region ---------- 矩形の描画 ----------

	commandList->IASetVertexBuffers(0, 1, &boxData_->vertexBufferView);     // 頂点バッファビューの設定
	commandList->IASetIndexBuffer(&boxData_->indexBufferView);              // インデックスバッファビューの設定
	commandList->SetGraphicsRootConstantBufferView(0, transformationMatrixBuffer_->GetGPUVirtualAddress()); // 座標変換行列の設定
	commandList->DrawIndexedInstanced(kBoxIndexCount, boxVertexIndex_ / kBoxVertexCount, 0, 0, 0);               // インデックスを使用して描画

#pragma endregion -----------------------------
}


/// -------------------------------------------------------------
///				　	          リセット処理
/// -------------------------------------------------------------
void Wireframe::Reset()
{
	triangleIndex_ = 0;
	boxVertexIndex_ = 0;
	boxIndex_ = 0;
	lineIndex_ = 0;
}


/// -------------------------------------------------------------
///				　	          解放処理
/// -------------------------------------------------------------
void Wireframe::Finalize()
{
	// 三角形の解放
	delete triangleData_;
	triangleData_ = nullptr;
	// 矩形の解放
	delete boxData_;
	boxData_ = nullptr;
	// 線の解放
	delete lineData_;
	lineData_ = nullptr;
}


/// -------------------------------------------------------------
///				　	      線を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawLine(const Vector3& start, const Vector3& end, const Vector4& color)
{
	// 頂点データの設定
	lineData_->vertexData[lineIndex_].position = start;
	lineData_->vertexData[lineIndex_ + 1].position = end;

	// カラーの設定
	lineData_->vertexData[lineIndex_].color = color;
	lineData_->vertexData[lineIndex_ + 1].color = color;

	lineIndex_ += kLineVertexCount;
}


/// -------------------------------------------------------------
///				　	      円を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawCircle(const Vector3& center, float radius, uint32_t segmentCount, const Vector4& color)
{
	const float PI = 3.14159265358979323846f;
	float angleStep = (2.0f * PI) / float(segmentCount);

	std::vector<Vector3> points(segmentCount);

	for (uint32_t i = 0; i < segmentCount; i++) {
		float angle = angleStep * i;
		points[i] = center + Vector3(radius * cos(angle), 0.0f, radius * sin(angle));
	}

	// 頂点を線で結ぶ
	for (uint32_t i = 0; i < segmentCount; i++) {
		DrawLine(points[i], points[(i + 1) % segmentCount], color);
	}
}


/// -------------------------------------------------------------
///				　	      三角形を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawTriangle(const Vector3& position1, const Vector3& position2, const Vector3& position3, const Vector4& color)
{
	// 頂点データの設定
	triangleData_->vertexData[triangleIndex_].position = position1;
	triangleData_->vertexData[triangleIndex_ + 1].position = position2;
	triangleData_->vertexData[triangleIndex_ + 2].position = position3;

	// カラーデータの設定
	triangleData_->vertexData[triangleIndex_].color = color;
	triangleData_->vertexData[triangleIndex_ + 1].color = color;
	triangleData_->vertexData[triangleIndex_ + 2].color = color;

	triangleIndex_ += kTriangleVertexCount;
}


/// -------------------------------------------------------------
///				　	      四角形を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawBox(const Vector3& position, const Vector3& size, const Vector4& color)
{
	// 頂点データの設定
	boxData_->vertexData[boxVertexIndex_ + 0].position = Vector3(position.x, position.y, position.z);
	boxData_->vertexData[boxVertexIndex_ + 1].position = Vector3(position.x + size.x, position.y, position.z);
	boxData_->vertexData[boxVertexIndex_ + 2].position = Vector3(position.x + size.x, position.y + size.y, position.z);
	boxData_->vertexData[boxVertexIndex_ + 3].position = Vector3(position.x, position.y + size.y, position.z);

	// カラーデータの設定
	boxData_->vertexData[boxVertexIndex_ + 0].color = color;
	boxData_->vertexData[boxVertexIndex_ + 1].color = color;
	boxData_->vertexData[boxVertexIndex_ + 2].color = color;
	boxData_->vertexData[boxVertexIndex_ + 3].color = color;

	// インデックスデータの設定
	boxData_->indexData[boxIndex_] = boxVertexIndex_ + 0;
	boxData_->indexData[boxIndex_ + 1] = boxVertexIndex_ + 1;
	boxData_->indexData[boxIndex_ + 2] = boxVertexIndex_ + 2;
	boxData_->indexData[boxIndex_ + 3] = boxVertexIndex_ + 0;
	boxData_->indexData[boxIndex_ + 4] = boxVertexIndex_ + 2;
	boxData_->indexData[boxIndex_ + 5] = boxVertexIndex_ + 3;

	// インデックスと頂点インデックスの更新
	boxIndex_ += kBoxIndexCount;
	boxVertexIndex_ += kBoxVertexCount;
}


/// -------------------------------------------------------------
///				　	      AABBを描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawAABB(const AABB& aabb, const Vector4& color)
{
	Vector3 min = aabb.min;
	Vector3 max = aabb.max;

	Vector3 p1 = Vector3(min.x, min.y, min.z);
	Vector3 p2 = Vector3(max.x, min.y, min.z);
	Vector3 p3 = Vector3(max.x, max.y, min.z);
	Vector3 p4 = Vector3(min.x, max.y, min.z);
	Vector3 p5 = Vector3(min.x, min.y, max.z);
	Vector3 p6 = Vector3(max.x, min.y, max.z);
	Vector3 p7 = Vector3(max.x, max.y, max.z);
	Vector3 p8 = Vector3(min.x, max.y, max.z);

	// 底面
	DrawLine(p1, p2, color);
	DrawLine(p2, p3, color);
	DrawLine(p3, p4, color);
	DrawLine(p4, p1, color);

	// 上面
	DrawLine(p5, p6, color);
	DrawLine(p6, p7, color);
	DrawLine(p7, p8, color);
	DrawLine(p8, p5, color);

	// 側面
	DrawLine(p1, p5, color);
	DrawLine(p2, p6, color);
	DrawLine(p3, p7, color);
	DrawLine(p4, p8, color);
}


/// -------------------------------------------------------------
///				　	      OBBを描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawOBB(const OBB& obb, const Vector4& color)
{
	// OBBの各頂点を定義（ローカル座標）
	Vector3 localVertices[8] = {
		{-obb.size.x, -obb.size.y, -obb.size.z}, {obb.size.x, -obb.size.y, -obb.size.z},
		{obb.size.x,  obb.size.y, -obb.size.z}, {-obb.size.x,  obb.size.y, -obb.size.z},
		{-obb.size.x, -obb.size.y,  obb.size.z}, {obb.size.x, -obb.size.y,  obb.size.z},
		{obb.size.x,  obb.size.y,  obb.size.z}, {-obb.size.x,  obb.size.y,  obb.size.z}
	};

	// ワールド座標に変換（回転適用 & 平行移動）
	Vector3 worldVertices[8];
	for (int i = 0; i < 8; i++) {
		worldVertices[i] =
			obb.center +
			obb.orientations[0] * localVertices[i].x +
			obb.orientations[1] * localVertices[i].y +
			obb.orientations[2] * localVertices[i].z;
	}

	// OBBのエッジを結ぶ
	int edges[12][2] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 0}, // 底面
		{4, 5}, {5, 6}, {6, 7}, {7, 4}, // 上面
		{0, 4}, {1, 5}, {2, 6}, {3, 7}  // 側面
	};

	for (int i = 0; i < 12; i++) {
		DrawLine(worldVertices[edges[i][0]], worldVertices[edges[i][1]], color);
	}
}


/// -------------------------------------------------------------
///				　	      球体を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawSphere(const Vector3& center, const float radius, const Vector4& color)
{
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(Vector3(radius, radius, radius), Vector3(0.0f, 0.0f, 0.0f), center);

	for (uint32_t i = 0; i + 2 < spheres_.size(); i += 3)
	{
		Vector3 a = spheres_[i];
		Vector3 b = spheres_[i + 1];
		Vector3 c = spheres_[i + 2];

		a = Vector3::Transform(a, worldMatrix);
		b = Vector3::Transform(b, worldMatrix);
		c = Vector3::Transform(c, worldMatrix);

		// 線描画
		DrawLine(a, b, color);
		//DrawLine(b, c, color);
		DrawLine(a, c, color); // 三角形を完成させるための線を追加
	}
}


/// -------------------------------------------------------------
///				　	      円柱を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawCylinder(const Vector3& baseCenter, float radius, float height, const Vector3& axis, uint32_t segmentCount, const Vector4& color)
{
	constexpr float PI = 3.14159265358979323846f;
	float angleStep = (2.0f * PI) / float(segmentCount);

	std::vector<Vector3> topVertices(segmentCount);
	std::vector<Vector3> bottomVertices(segmentCount);

	// 円の基準軸を決める（axis に直交する2つのベクトルを求める）
	Vector3 up = Vector3::Normalize(axis);
	Vector3 right = Vector3(1.0f, 0.0f, 0.0f);
	if (std::fabs(Vector3::Dot(up, right)) > 0.99f) {
		right = Vector3(0.0f, 1.0f, 0.0f);
	}
	Vector3 forward = Vector3::Normalize(Vector3::Cross(up, right));
	right = Vector3::Normalize(Vector3::Cross(forward, up));

	// 上面・下面の円の頂点を計算
	for (uint32_t i = 0; i < segmentCount; i++) {
		float angle = angleStep * i;
		Vector3 offset = Vector3::Add(Vector3::Multiply(radius * cosf(angle), right),
			Vector3::Multiply(radius * sinf(angle), forward));
		bottomVertices[i] = Vector3::Add(baseCenter, offset);
		topVertices[i] = Vector3::Add(bottomVertices[i], Vector3::Multiply(height, up));
	}

	// 上面の円を描画
	for (uint32_t i = 0; i < segmentCount; i++) {
		DrawLine(topVertices[i], topVertices[(i + 1) % segmentCount], color);
	}

	// 下面の円を描画
	for (uint32_t i = 0; i < segmentCount; i++) {
		DrawLine(bottomVertices[i], bottomVertices[(i + 1) % segmentCount], color);
	}

	// 上面と下面をつなぐ側面の線を描画
	for (uint32_t i = 0; i < segmentCount; i++) {
		DrawLine(bottomVertices[i], topVertices[i], color);
	}
}

void Wireframe::DrawTetrahedron(const Vector3& baseCenter, float baseSize, float height, const Vector3& axis, const Vector4& color)
{
	// 底面の外接円の半径
	float radius = baseSize / sqrtf(3.0f); // 正三角形の外接円半径

	// 基準軸（axis）を正規化
	Vector3 up = Vector3::Normalize(axis);
	Vector3 right = Vector3(1.0f, 0.0f, 0.0f);

	// upと直交する軸を求める
	if (std::fabs(Vector3::Dot(up, right)) > 0.99f) {
		right = Vector3(0.0f, 1.0f, 0.0f);
	}
	Vector3 forward = Vector3::Normalize(Vector3::Cross(up, right));
	right = Vector3::Normalize(Vector3::Cross(forward, up));

	// 底面の3頂点を計算（正三角形）
	Vector3 bottomVertices[3] = {
		Vector3::Add(baseCenter, Vector3::Add(Vector3::Multiply(radius * cosf(0.0f), right), Vector3::Multiply(radius * sinf(0.0f), forward))), // 頂点1
		Vector3::Add(baseCenter, Vector3::Add(Vector3::Multiply(radius * cosf(2.0f * 3.14159265358979323846f / 3.0f), right),
											  Vector3::Multiply(radius * sinf(2.0f * 3.14159265358979323846f / 3.0f), forward))), // 頂点2
		Vector3::Add(baseCenter, Vector3::Add(Vector3::Multiply(radius * cosf(4.0f * 3.14159265358979323846f / 3.0f), right),
											  Vector3::Multiply(radius * sinf(4.0f * 3.14159265358979323846f / 3.0f), forward)))  // 頂点3
	};

	// 頂点（ピラミッドの先端）を計算
	Vector3 topVertex = Vector3::Add(baseCenter, Vector3::Multiply(height, up));

	// 底面の3辺を描画
	for (int i = 0; i < 3; i++) {
		DrawLine(bottomVertices[i], bottomVertices[(i + 1) % 3], color);
	}

	// 側面の3辺を描画
	for (int i = 0; i < 3; i++) {
		DrawLine(bottomVertices[i], topVertex, color);
	}
}


/// -------------------------------------------------------------
///				　	    四角錐を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawPyramid(const Vector3& baseCenter, float baseSize, float height, const Vector3& axis, const Vector4& color)
{
	// ピラミッドの底面の半径
	float halfSize = baseSize * 0.5f;

	// 基準軸（axis）を正規化
	Vector3 up = Vector3::Normalize(axis);
	Vector3 right = Vector3(1.0f, 0.0f, 0.0f);

	// upと直交する軸を求める
	if (std::fabs(Vector3::Dot(up, right)) > 0.99f) {
		right = Vector3(0.0f, 1.0f, 0.0f);
	}
	Vector3 forward = Vector3::Normalize(Vector3::Cross(up, right));
	right = Vector3::Normalize(Vector3::Cross(forward, up));

	// 底面の4頂点を計算
	Vector3 bottomVertices[4] = {
		Vector3::Add(baseCenter, Vector3::Add(Vector3::Multiply(-halfSize, right), Vector3::Multiply(-halfSize, forward))), // 左下
		Vector3::Add(baseCenter, Vector3::Add(Vector3::Multiply(halfSize, right), Vector3::Multiply(-halfSize, forward))), // 右下
		Vector3::Add(baseCenter, Vector3::Add(Vector3::Multiply(halfSize, right), Vector3::Multiply(halfSize, forward))), // 右上
		Vector3::Add(baseCenter, Vector3::Add(Vector3::Multiply(-halfSize, right), Vector3::Multiply(halfSize, forward)))  // 左上
	};

	// 頂点（ピラミッドの先端）を計算
	Vector3 topVertex = Vector3::Add(baseCenter, Vector3::Multiply(height, up));

	// 底面の四辺
	for (int i = 0; i < 4; i++) {
		DrawLine(bottomVertices[i], bottomVertices[(i + 1) % 4], color);
	}

	// 側面の4辺
	for (int i = 0; i < 4; i++) {
		DrawLine(bottomVertices[i], topVertex, color);
	}
}

void Wireframe::DrawOctahedron(const Vector3& center, float size, const Vector4& color)
{
	float halfSize = size * 0.5f;

	// 正八面体の6つの頂点
	Vector3 top = Vector3::Add(center, Vector3(0.0f, halfSize, 0.0f));   // 上の頂点
	Vector3 bottom = Vector3::Add(center, Vector3(0.0f, -halfSize, 0.0f)); // 下の頂点

	Vector3 midVertices[4] = {
		Vector3::Add(center, Vector3(halfSize, 0.0f, 0.0f)),  // X+ 方向
		Vector3::Add(center, Vector3(0.0f, 0.0f, halfSize)),  // Z+ 方向
		Vector3::Add(center, Vector3(-halfSize, 0.0f, 0.0f)), // X- 方向
		Vector3::Add(center, Vector3(0.0f, 0.0f, -halfSize))  // Z- 方向
	};

	// 上面の4つの三角形
	for (int i = 0; i < 4; i++) {
		DrawLine(top, midVertices[i], color);
		DrawLine(midVertices[i], midVertices[(i + 1) % 4], color);
	}

	// 下面の4つの三角形
	for (int i = 0; i < 4; i++) {
		DrawLine(bottom, midVertices[i], color);
		DrawLine(midVertices[i], midVertices[(i + 1) % 4], color);
	}
}

void Wireframe::DrawDodecahedron(const Vector3& center, float size, const Vector4& color)
{
	constexpr float GOLDEN_RATIO = 1.61803398875f;
	float halfSize = size * 0.5f;

	// 正十二面体の20頂点を計算
	std::vector<Vector3> vertices = {
		// 正六面体の8頂点
		Vector3::Add(center, Vector3(-halfSize, -halfSize, -halfSize)), // 0
		Vector3::Add(center, Vector3(halfSize, -halfSize, -halfSize)),  // 1
		Vector3::Add(center, Vector3(halfSize, halfSize, -halfSize)),   // 2
		Vector3::Add(center, Vector3(-halfSize, halfSize, -halfSize)),  // 3
		Vector3::Add(center, Vector3(-halfSize, -halfSize, halfSize)),  // 4
		Vector3::Add(center, Vector3(halfSize, -halfSize, halfSize)),   // 5
		Vector3::Add(center, Vector3(halfSize, halfSize, halfSize)),    // 6
		Vector3::Add(center, Vector3(-halfSize, halfSize, halfSize)),   // 7

		// 正六面体の面の中心に相当する12頂点（黄金比を用いる）
		Vector3::Add(center, Vector3(0, -halfSize * GOLDEN_RATIO, -halfSize / GOLDEN_RATIO)),  // 8
		Vector3::Add(center, Vector3(0, -halfSize * GOLDEN_RATIO, halfSize / GOLDEN_RATIO)),   // 9
		Vector3::Add(center, Vector3(0, halfSize * GOLDEN_RATIO, -halfSize / GOLDEN_RATIO)),   // 10
		Vector3::Add(center, Vector3(0, halfSize * GOLDEN_RATIO, halfSize / GOLDEN_RATIO)),    // 11
		Vector3::Add(center, Vector3(-halfSize * GOLDEN_RATIO, -halfSize / GOLDEN_RATIO, 0)),  // 12
		Vector3::Add(center, Vector3(-halfSize * GOLDEN_RATIO, halfSize / GOLDEN_RATIO, 0)),   // 13
		Vector3::Add(center, Vector3(halfSize * GOLDEN_RATIO, -halfSize / GOLDEN_RATIO, 0)),   // 14
		Vector3::Add(center, Vector3(halfSize * GOLDEN_RATIO, halfSize / GOLDEN_RATIO, 0)),    // 15
		Vector3::Add(center, Vector3(-halfSize / GOLDEN_RATIO, 0, -halfSize * GOLDEN_RATIO)),  // 16
		Vector3::Add(center, Vector3(halfSize / GOLDEN_RATIO, 0, -halfSize * GOLDEN_RATIO)),   // 17
		Vector3::Add(center, Vector3(-halfSize / GOLDEN_RATIO, 0, halfSize * GOLDEN_RATIO)),   // 18
		Vector3::Add(center, Vector3(halfSize / GOLDEN_RATIO, 0, halfSize * GOLDEN_RATIO))     // 19
	};

	// 正十二面体の12の五角形（エッジの接続情報）
	int pentagons[12][5] = {
		{0, 8, 9, 4, 12}, {1, 14, 5, 9, 8}, {2, 10, 11, 6, 15}, {3, 13, 7, 11, 10},
		{0, 12, 13, 3, 16}, {1, 17, 2, 15, 14}, {4, 18, 19, 6, 11}, {5, 15, 6, 19, 9},
		{7, 13, 12, 4, 18}, {3, 16, 17, 2, 10}, {0, 8, 17, 16, 1}, {7, 18, 19, 5, 14}
	};

	// ワイヤーフレームで描画
	for (int i = 0; i < 12; i++) {
		for (int j = 0; j < 5; j++) {
			DrawLine(vertices[pentagons[i][j]], vertices[pentagons[i][(j + 1) % 5]], color);
		}
	}
}

void Wireframe::DrawIcosahedron(const Vector3& center, float size, const Vector4& color)
{
	constexpr float GOLDEN_RATIO = 1.61803398875f;
	float scale = size * 0.5f;

	// 正二十面体の12頂点を計算
	std::vector<Vector3> vertices = {
		Vector3::Add(center, Vector3(-scale, GOLDEN_RATIO * scale, 0)),  // 0
		Vector3::Add(center, Vector3(scale, GOLDEN_RATIO * scale, 0)),   // 1
		Vector3::Add(center, Vector3(-scale, -GOLDEN_RATIO * scale, 0)), // 2
		Vector3::Add(center, Vector3(scale, -GOLDEN_RATIO * scale, 0)),  // 3
		Vector3::Add(center, Vector3(0, -scale, GOLDEN_RATIO * scale)),  // 4
		Vector3::Add(center, Vector3(0, scale, GOLDEN_RATIO * scale)),   // 5
		Vector3::Add(center, Vector3(0, -scale, -GOLDEN_RATIO * scale)), // 6
		Vector3::Add(center, Vector3(0, scale, -GOLDEN_RATIO * scale)),  // 7
		Vector3::Add(center, Vector3(GOLDEN_RATIO * scale, 0, -scale)),  // 8
		Vector3::Add(center, Vector3(GOLDEN_RATIO * scale, 0, scale)),   // 9
		Vector3::Add(center, Vector3(-GOLDEN_RATIO * scale, 0, -scale)), // 10
		Vector3::Add(center, Vector3(-GOLDEN_RATIO * scale, 0, scale))   // 11
	};

	// 正二十面体の20の三角形の接続情報
	int triangles[20][3] = {
		{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
		{1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
		{3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
		{4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
	};

	// ワイヤーフレームで描画
	for (int i = 0; i < 20; i++) {
		DrawLine(vertices[triangles[i][0]], vertices[triangles[i][1]], color);
		DrawLine(vertices[triangles[i][1]], vertices[triangles[i][2]], color);
		DrawLine(vertices[triangles[i][2]], vertices[triangles[i][0]], color);
	}
}

void Wireframe::DrawTorus(const Vector3& center, float R, float r, uint32_t ringSegments, uint32_t tubeSegments, const Vector4& color)
{
	const float PI = 3.14159265358979323846f;

	std::vector<Vector3> points;

	// 頂点を計算
	for (uint32_t i = 0; i <= ringSegments; i++) {
		float u = (2.0f * PI * i) / ringSegments;
		for (uint32_t j = 0; j <= tubeSegments; j++) {
			float v = (2.0f * PI * j) / tubeSegments;

			float x = (R + r * cos(v)) * cos(u);
			float y = (R + r * cos(v)) * sin(u);
			float z = r * sin(v);

			points.push_back(center + Vector3(x, y, z));
		}
	}

	// 頂点を線で結ぶ
	for (uint32_t i = 0; i < ringSegments; i++) {
		for (uint32_t j = 0; j < tubeSegments; j++) {
			uint32_t index0 = i * (tubeSegments + 1) + j;
			uint32_t index1 = index0 + 1;
			uint32_t index2 = (i + 1) * (tubeSegments + 1) + j;
			uint32_t index3 = index2 + 1;

			// 小円方向の線
			DrawLine(points[index0], points[index1], color);

			// 大円方向の線
			DrawLine(points[index0], points[index2], color);
		}
	}
}

void Wireframe::DrawRotatingTorus(const Vector3& center, float R, float r, uint32_t ringSegments, uint32_t tubeSegments, const Vector4& color, float time)
{
	const float PI = 3.14159265358979323846f;
	Matrix4x4 rotationMatrix = Matrix4x4::MakeRotateYMatrix(time * 0.5f); // Y軸回転

	std::vector<Vector3> points;

	// 頂点を計算
	for (uint32_t i = 0; i <= ringSegments; i++) {
		float u = (2.0f * PI * i) / ringSegments;
		for (uint32_t j = 0; j <= tubeSegments; j++) {
			float v = (2.0f * PI * j) / tubeSegments;

			float x = (R + r * cos(v)) * cos(u);
			float y = (R + r * cos(v)) * sin(u);
			float z = r * sin(v);

			Vector3 rotatedPoint = Vector3::Transform(center + Vector3(x, y, z), rotationMatrix);
			points.push_back(rotatedPoint);
		}
	}

	// 頂点を線で結ぶ
	for (uint32_t i = 0; i < ringSegments; i++) {
		for (uint32_t j = 0; j < tubeSegments; j++) {
			uint32_t index0 = i * (tubeSegments + 1) + j;
			uint32_t index1 = index0 + 1;
			uint32_t index2 = (i + 1) * (tubeSegments + 1) + j;
			uint32_t index3 = index2 + 1;

			// 小円方向の線
			DrawLine(points[index0], points[index1], color);

			// 大円方向の線
			DrawLine(points[index0], points[index2], color);
		}
	}
}

void Wireframe::DrawMobiusStrip(const Vector3& center, float R, float w, uint32_t ringSegments, uint32_t tubeSegments, const Vector4& color)
{
	const float PI = 3.14159265358979323846f;
	std::vector<Vector3> points;

	// 頂点を計算
	for (uint32_t i = 0; i <= ringSegments; i++) {
		float t = (2.0f * PI * i) / ringSegments;
		for (uint32_t j = 0; j <= tubeSegments; j++) {
			float u = w * (2.0f * j / tubeSegments - 1.0f);

			float x = (R + u * cos(t / 2.0f)) * cos(t);
			float y = (R + u * cos(t / 2.0f)) * sin(t);
			float z = u * sin(t / 2.0f);

			points.push_back(center + Vector3(x, y, z));
		}
	}

	// 頂点を線で結ぶ
	for (uint32_t i = 0; i < ringSegments; i++) {
		for (uint32_t j = 0; j < tubeSegments; j++) {
			uint32_t index0 = i * (tubeSegments + 1) + j;
			uint32_t index1 = index0 + 1;
			uint32_t index2 = (i + 1) * (tubeSegments + 1) + j;
			uint32_t index3 = index2 + 1;

			// 幅方向の線
			DrawLine(points[index0], points[index1], color);

			// 長さ方向の線
			DrawLine(points[index0], points[index2], color);
		}
	}
}

void Wireframe::DrawLemniscate3D(const Vector3& center, float a, float b, float c, uint32_t segments, const Vector4& color)
{
	const float PI = 3.14159265358979323846f;
	std::vector<Vector3> points;

	// 頂点を計算
	for (uint32_t i = 0; i <= segments; i++) {
		float t = (2.0f * PI * i) / segments;

		float denominator = 1.0f + sin(t) * sin(t);
		float x = a * cos(t) / denominator;
		float y = b * sin(t) * cos(t) / denominator;
		float z = c * sin(t);

		points.push_back(center + Vector3(x, y, z));
	}

	// 線を結ぶ
	for (uint32_t i = 0; i < segments; i++) {
		DrawLine(points[i], points[i + 1], color);
	}
}

void Wireframe::DrawPentagonalPrism(const Vector3& center, float radius, float height, const Vector4& color)
{
	constexpr float PI = 3.14159265358979323846f;
	float angleStep = 2.0f * PI / 5.0f; // 五角形の角度間隔
	float halfHeight = height * 0.5f;

	Vector3 bottomVertices[5];
	Vector3 topVertices[5];

	// 五角形の頂点を計算（底面と上面）
	for (int i = 0; i < 5; i++) {
		float angle = angleStep * i;
		Vector3 offset = Vector3::Add(Vector3::Multiply(radius * cosf(angle), Vector3(1.0f, 0.0f, 0.0f)),
			Vector3::Multiply(radius * sinf(angle), Vector3(0.0f, 0.0f, 1.0f)));

		bottomVertices[i] = Vector3::Add(center, Vector3::Add(offset, Vector3(0.0f, -halfHeight, 0.0f)));
		topVertices[i] = Vector3::Add(center, Vector3::Add(offset, Vector3(0.0f, halfHeight, 0.0f)));
	}

	// 底面の五角形を描画
	for (int i = 0; i < 5; i++) {
		DrawLine(bottomVertices[i], bottomVertices[(i + 1) % 5], color);
	}

	// 上面の五角形を描画
	for (int i = 0; i < 5; i++) {
		DrawLine(topVertices[i], topVertices[(i + 1) % 5], color);
	}

	// 側面の長方形の辺を描画（底面と上面をつなぐ）
	for (int i = 0; i < 5; i++) {
		DrawLine(bottomVertices[i], topVertices[i], color);
	}
}

void Wireframe::DrawPentagonalPyramid(const Vector3& center, float radius, float height, const Vector4& color)
{
	constexpr float PI = 3.14159265358979323846f;
	float angleStep = 2.0f * PI / 5.0f; // 五角形の角度間隔

	Vector3 bottomVertices[5];

	// 五角形の底面の頂点を計算
	for (int i = 0; i < 5; i++) {
		float angle = angleStep * i;
		Vector3 offset = Vector3::Add(Vector3::Multiply(radius * cosf(angle), Vector3(1.0f, 0.0f, 0.0f)),
			Vector3::Multiply(radius * sinf(angle), Vector3(0.0f, 0.0f, 1.0f)));
		bottomVertices[i] = Vector3::Add(center, offset);
	}

	// 頂点（ピラミッドの先端）を計算
	Vector3 topVertex = Vector3::Add(center, Vector3(0.0f, height, 0.0f));

	// 底面の五角形を描画
	for (int i = 0; i < 5; i++) {
		DrawLine(bottomVertices[i], bottomVertices[(i + 1) % 5], color);
	}

	// 各底面の頂点から頂点（先端）へのエッジを描画
	for (int i = 0; i < 5; i++) {
		DrawLine(bottomVertices[i], topVertex, color);
	}
}


/// -------------------------------------------------------------
///				　	    五芒星を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawPentagram(const Vector3& center, float radius, const Vector4& color)
{
	const float PI = 3.14159265358979323846f;
	Vector3 points[5];

	// 五芒星の5頂点を計算
	for (int i = 0; i < 5; i++) {
		float angle = PI / 2.0f + (2.0f * PI / 5.0f) * i * 2; // 星の頂点の角度
		points[i] = center + Vector3(radius * cos(angle), 0.0f, radius * sin(angle));
	}

	// 線を結ぶ（星の交差する部分）
	for (int i = 0; i < 5; i++) {
		DrawLine(points[i], points[(i + 2) % 5], color);
	}
}


/// -------------------------------------------------------------
///				　	    六芒星を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawHexagram(const Vector3& center, float radius, const Vector4& color)
{
	const float PI = 3.14159265358979323846f;
	Vector3 points[6];

	// 六芒星の6頂点を計算
	for (int i = 0; i < 6; i++) {
		float angle = (PI / 6.0f) + (2.0f * PI / 6.0f) * i;
		points[i] = center + Vector3(radius * cos(angle), 0.0f, radius * sin(angle));
	}

	// 正三角形1
	DrawLine(points[0], points[2], color);
	DrawLine(points[2], points[4], color);
	DrawLine(points[4], points[0], color);

	// 正三角形2（逆向き）
	DrawLine(points[1], points[3], color);
	DrawLine(points[3], points[5], color);
	DrawLine(points[5], points[1], color);
}


/// -------------------------------------------------------------
///			星型魔法陣（外円付きの五芒星）を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawMagicPentagram(const Vector3& center, float radius, const Vector4& color)
{
	// 五芒星を描画
	DrawPentagram(center, radius, color);

	// 外円を描画
	DrawCircle(center, radius * 1.1f, 50, color);
}


/// -------------------------------------------------------------
///				 正多角形（Polygon）を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawPolygon(const Vector3& center, float radius, uint32_t sides, const Vector4& color)
{
	if (sides < 3) return; // 三角形以上でないと描画できない

	const float PI = 3.14159265358979323846f;
	float angleStep = (2.0f * PI) / float(sides);

	std::vector<Vector3> points(sides);

	for (uint32_t i = 0; i < sides; i++) {
		float angle = angleStep * i;
		points[i] = center + Vector3(radius * cos(angle), 0.0f, radius * sin(angle));
	}

	// 頂点を線で結ぶ
	for (uint32_t i = 0; i < sides; i++) {
		DrawLine(points[i], points[(i + 1) % sides], color);
	}
}


/// -------------------------------------------------------------
///			同心円（Concentric Circles）を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawConcentricCircles(const Vector3& center, float radius, uint32_t count, float spacing, const Vector4& color)
{
	for (uint32_t i = 0; i < count; i++) {
		DrawCircle(center, radius + (spacing * i), 50, color);
	}
}


/// -------------------------------------------------------------
///						魔法陣を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawAdvancedMagicCircle(const Vector3& center, float radius, const Vector4& color)
{
	// 五芒星
	DrawPentagram(center, radius * 0.8f, color);

	// 外円
	DrawCircle(center, radius, 50, color);

	// 同心円
	DrawConcentricCircles(center, radius * 0.5f, 3, radius * 0.15f, color);

	// 六芒星
	DrawHexagram(center, radius * 0.6f, color);
}


/// -------------------------------------------------------------
///						魔法陣を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawRotatingPentagram(const Vector3& center, float radius, const Vector4& color, float time)
{
	const float PI = 3.14159265358979323846f;
	Vector3 points[5];

	// 回転角度（時間に応じて回転）
	float rotation = time * PI * 0.2f;

	for (int i = 0; i < 5; i++) {
		float angle = PI / 2.0f + (2.0f * PI / 5.0f) * i * 2 + rotation;
		points[i] = center + Vector3(radius * cos(angle), 0.0f, radius * sin(angle));
	}

	// 線を描画
	for (int i = 0; i < 5; i++) {
		DrawLine(points[i], points[(i + 2) % 5], color);
	}
}


/// -------------------------------------------------------------
///				波紋のように広がる魔法陣を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawExpandingMagicCircle(const Vector3& center, float baseRadius, const Vector4& color, float time)
{
	float scale = 1.0f + 0.5f * sin(time * 2.0f); // 半径が時間に応じて振動

	// 拡大縮小する魔法陣
	DrawPentagram(center, baseRadius * scale, color);
	DrawCircle(center, baseRadius * scale * 1.1f, 50, color);
}


/// -------------------------------------------------------------
///						光の輪を描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawFadingMagicCircle(const Vector3& center, float radius, float time)
{
	float alpha = (sin(time) + 1.0f) * 0.5f; // 0.0 ～ 1.0 の範囲でフェード
	Vector4 color = { 1.0f, 0.5f, 0.0f, alpha };

	DrawPentagram(center, radius, color);
	DrawCircle(center, radius * 1.1f, 50, color);
}


/// -------------------------------------------------------------
///			魔法陣のエフェクトを組み合わせる処理
/// -------------------------------------------------------------
void Wireframe::DrawAnimatedMagicCircle(const Vector3& center, float radius, float time)
{
	float rotation = time * 1.5f;    // 回転速度
	float scale = 1.0f + 0.2f * sin(time * 3.0f); // 拡縮
	float alpha = (sin(time) + 1.0f) * 0.5f; // フェード

	Vector4 color = { 1.0f, 0.5f, 0.0f, alpha };

	// 魔法陣の円
	DrawCircle(center, radius * scale, 50, color);

	// 五芒星の回転
	const float PI = 3.14159265358979323846f;
	Vector3 points[5];
	for (int i = 0; i < 5; i++) {
		float angle = PI / 2.0f + (2.0f * PI / 5.0f) * i * 2 + rotation;
		points[i] = center + Vector3(radius * scale * cos(angle), 0.0f, radius * scale * sin(angle));
	}
	for (int i = 0; i < 5; i++) {
		DrawLine(points[i], points[(i + 2) % 5], color);
	}
}

void Wireframe::DrawAnimatedHeart(const Vector3& center, float size, float time)
{
	const float PI = 3.14159265358979323846f;
	const uint32_t segments = 100;

	float progress = fmin(time / 2.0f, 1.0f);  // 0.0 ～ 1.0 で線を増やす
	float alpha = fmin(time / 2.0f, 1.0f);  // フェードイン

	Vector4 color = { 1.0f, 0.0f, 0.2f, alpha };  // 赤色 (フェードイン)

	// スケールのゆらぎ（ポップなアニメーション）
	float scale = size * (1.0f + 0.05f * sin(time * 3.0f));

	std::vector<Vector3> points;

	// ハートのパラメトリック曲線
	for (uint32_t i = 0; i < segments * progress; i++) {
		float t = PI * 2.0f * i / segments;

		float x = 16.0f * pow(sin(t), 3.0f);
		float y = 13.0f * cos(t) - 5.0f * cos(2 * t) - 2.0f * cos(3 * t) - cos(4 * t);

		points.push_back(center + Vector3(x * scale * 0.1f, y * scale * 0.1f, 0.0f));
	}

	// ハートの輪郭を線でつなぐ
	for (size_t i = 1; i < points.size(); i++) {
		DrawLine(points[i - 1], points[i], color);
	}
}

void Wireframe::DrawGlowingHeart(const Vector3& center, float size, float time)
{
	DrawAnimatedHeart(center, size, time);

	float glowAlpha = (sin(time * 2.0f) + 1.0f) * 0.5f; // 明滅
	Vector4 glowColor = { 1.0f, 0.4f, 0.6f, glowAlpha };

	DrawCircle(center, size * 0.6f, 50, glowColor);
	DrawCircle(center, size * 0.7f, 50, glowColor);
}

void Wireframe::DrawPoppingHeart(const Vector3& center, float size, float time)
{
	float popScale = size * (1.0f + 0.1f * sin(time * 5.0f)); // 拡縮

	DrawAnimatedHeart(center, popScale, time);
}

void Wireframe::DrawFloatingHeart(const Vector3& basePosition, float size, float time)
{
	Vector3 floatingCenter = basePosition + Vector3(0.0f, sin(time * 2.0f) * 0.5f, 0.0f);

	DrawAnimatedHeart(floatingCenter, size, time);
}

void Wireframe::DrawMagicCircle(const Vector3& center, float radius, const Vector4& color)
{
	// 外円
	DrawCircle(center, radius, 60, color);

	// 五芒星
	DrawPentagram(center, radius * 0.8f, color);

	// 六芒星
	DrawHexagram(center, radius * 0.6f, color);

	// 中心を囲む小円
	DrawConcentricCircles(center, radius * 0.5f, 3, radius * 0.15f, color);

	// 12本の放射状の線
	const float PI = 3.14159265358979323846f;
	for (int i = 0; i < 12; i++) {
		float angle = (2.0f * PI / 12) * i;
		Vector3 start = center + Vector3(cos(angle) * radius * 0.3f, 0.0f, sin(angle) * radius * 0.3f);
		Vector3 end = center + Vector3(cos(angle) * radius, 0.0f, sin(angle) * radius);
		DrawLine(start, end, color);
	}

	// 円周上の多角形
	DrawPolygon(center, radius * 0.9f, 8, color);
}

void Wireframe::DrawRotatingMagicCircle(const Vector3& center, float radius, const Vector4& color, float time)
{
	float rotation = time * 1.5f;
	DrawMagicCircle(center, radius, color);
	DrawPentagram(center, radius * 0.8f, color);
}

void Wireframe::DrawProgressiveMagicCircle(const Vector3& center, float radius, const Vector4& baseColor, float time)
{
	const float PI = 3.14159265358979323846f;

	// フェードイン効果
	float alpha = (sin(time) + 1.0f) * 0.5f;
	Vector4 color = { baseColor.x, baseColor.y, baseColor.z, alpha };

	// 回転速度
	float rotation = time * 1.5f;

	// 線が描かれる進行度（0.0 ～ 1.0）
	float progress = fmod(time, 5.0f) / 5.0f;

	// **外円をなぞって描く**
	if (progress > 0.0f) {
		float circleProgress = progress * 2.0f; // 0.0 ~ 2.0
		int segmentCount = 60;
		int visibleSegments = static_cast<int>(segmentCount * fmin(circleProgress, 1.0f)); // なぞる部分を計算
		DrawCircle(center, radius, visibleSegments, color);
	}

	// **放射線を1本ずつ描く**
	if (progress > 0.2f) {
		float lineProgress = (progress - 0.2f) * 5.0f; // 0.0 ~ 1.0 の範囲
		int visibleLines = static_cast<int>(12 * fmin(lineProgress, 1.0f));

		for (int i = 0; i < visibleLines; i++) {
			float angle = (2.0f * PI / 12) * i + rotation;
			Vector3 start = center + Vector3(cos(angle) * radius * 0.3f, 0.0f, sin(angle) * radius * 0.3f);
			Vector3 end = center + Vector3(cos(angle) * radius, 0.0f, sin(angle) * radius);
			DrawLine(start, end, color);
		}
	}

	// **五芒星を線をなぞりながら描く**
	if (progress > 0.4f) {
		float starProgress = (progress - 0.4f) * 5.0f;
		DrawPentagramProgressive(center, radius * 0.8f, color, starProgress);
	}

	// **六芒星を線をなぞりながら描く**
	if (progress > 0.6f) {
		float hexProgress = (progress - 0.6f) * 5.0f;
		DrawHexagramProgressive(center, radius * 0.6f, color, hexProgress);
	}

	// **同心円を線をなぞりながら描く**
	if (progress > 0.8f) {
		float concentricProgress = (progress - 0.8f) * 5.0f;
		int visibleCircles = static_cast<int>(3 * fmin(concentricProgress, 1.0f));
		DrawConcentricCircles(center, radius * 0.5f, visibleCircles, radius * 0.15f, color);
	}
}


void Wireframe::DrawPentagramProgressive(const Vector3& center, float radius, const Vector4& color, float progress)
{
	const float PI = 3.14159265358979323846f;
	Vector3 points[5];

	for (int i = 0; i < 5; i++) {
		float angle = PI / 2.0f + (2.0f * PI / 5.0f) * i * 2; // 星の頂点の角度
		points[i] = center + Vector3(radius * cos(angle), 0.0f, radius * sin(angle));
	}

	int visibleLines = static_cast<int>(5 * fmin(progress, 1.0f)); // 5本の線を順番に描く

	for (int i = 0; i < visibleLines; i++) {
		DrawLine(points[i], points[(i + 2) % 5], color);
	}
}


void Wireframe::DrawHexagramProgressive(const Vector3& center, float radius, const Vector4& color, float progress)
{
	const float PI = 3.14159265358979323846f;
	Vector3 points[6];

	for (int i = 0; i < 6; i++) {
		float angle = (PI / 6.0f) + (2.0f * PI / 6.0f) * i;
		points[i] = center + Vector3(radius * cos(angle), 0.0f, radius * sin(angle));
	}

	int visibleLines = static_cast<int>(6 * fmin(progress, 1.0f)); // 6本の線を順番に描く

	for (int i = 0; i < visibleLines; i++) {
		DrawLine(points[i], points[(i + 1) % 6], color);
	}
}


/// -------------------------------------------------------------
///				　	     グリッドを描画する処理
/// -------------------------------------------------------------
void Wireframe::DrawGrid(const float size, const float subdivision, const Vector4& color)
{
	float halfWidth = size * 0.5f;
	float every = size / subdivision;

	for (uint32_t xIndex = 0; xIndex <= subdivision; xIndex++)
	{
		Vector3 start = Vector3(-halfWidth + every * xIndex, 0.0f, halfWidth);
		Vector3 end = Vector3(-halfWidth + every * xIndex, 0.0f, -halfWidth);

		DrawLine(start, end, color);
	}

	for (uint32_t zIndex = 0; zIndex <= subdivision; zIndex++)
	{
		Vector3 start = Vector3(halfWidth, 0.0f, -halfWidth + every * zIndex);
		Vector3 end = Vector3(-halfWidth, 0.0f, -halfWidth + every * zIndex);

		DrawLine(start, end, color);
	}

	// X軸の線
	DrawLine({ -halfWidth,0.0f,0.0f }, { halfWidth,0.0f,0.0f }, { 1.0f, 0.0f,0.0f,1.0f });
	// Y軸の線
	DrawLine({ 0.0, -halfWidth,0.0f }, { 0.0f,halfWidth,0.0f }, { 0.0f, 1.0f,0.0f,1.0f });
	// Z軸の線
	DrawLine({ 0.0f,0.0f,-halfWidth }, { 0.0f,0.0f,halfWidth }, { 0.0f, 0.0f,1.0f,1.0f });
}


/// -------------------------------------------------------------
///				　	   ルートシグネチャの生成
/// -------------------------------------------------------------
void Wireframe::CreateRootSignature(ComPtr<ID3D12RootSignature>& rootSignature)
{
	HRESULT hr{};

	// RootSignatureの生成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameterの設定
	D3D12_ROOT_PARAMETER rootParameters[1] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	 // CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VertexShaderを使う
	rootParameters[0].Descriptor.ShaderRegister = 0;					 // レジスタ番号０とバインド

	descriptionRootSignature.pParameters = rootParameters;			   // ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ

	Microsoft::WRL::ComPtr <ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr <ID3DBlob> errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf()));
	assert(SUCCEEDED(hr));
}


/// -------------------------------------------------------------
///			　　パイプラインステートオブジェクトの生成
/// -------------------------------------------------------------
void Wireframe::CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType, ComPtr<ID3D12RootSignature>& rootSignature, ComPtr<ID3D12PipelineState>& pipelineState)
{
	HRESULT hr{};

	// ルートシグネチャの生成
	CreateRootSignature(rootSignature);

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "COLOR";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendStateの設定
	D3D12_RENDER_TARGET_BLEND_DESC blendDesc{};
	blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.BlendEnable = true;
	blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;

	// RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // 裏面を非表示にしない
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // 三角形の中を塗りつぶす

	// Shaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = ShaderManager::CompileShader(
		L"Resources/Shaders/Wireframe.VS.hlsl", L"vs_6_0",
		dxCommon_->GetIDxcUtils(), dxCommon_->GetIDxcCompiler(), dxCommon_->GetIncludeHandler());
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = ShaderManager::CompileShader(
		L"Resources/Shaders/Wireframe.PS.hlsl", L"ps_6_0",
		dxCommon_->GetIDxcUtils(), dxCommon_->GetIDxcCompiler(), dxCommon_->GetIncludeHandler());
	assert(pixelShaderBlob != nullptr);

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = TRUE;                     // 深度テストを有効にする
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 深度値の書き込みを無効化
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 小さいか等しい場合に描画

	// PSOの生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState.RenderTarget[0] = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

	// レンダーターゲットの設定
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	// プリミティブの種類（指定されたものを設定）
	graphicsPipelineStateDesc.PrimitiveTopologyType = primitiveTopologyType;

	// サンプルマスクとサンプル記述子の設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// DepthStencilステートの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// パイプラインステートオブジェクトの生成
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState));
	assert(SUCCEEDED(hr));
}


/// -------------------------------------------------------------
///			　　	 三角形の頂点データを生成
/// -------------------------------------------------------------
void Wireframe::CreateTriangleVertexData(TriangleData* triangleData)
{
	UINT vertexBufferSize = sizeof(VertexData) * kTriangleVertexCount * kTriangleMaxCount;

	// 頂点リソースを生成
	triangleData->vertexBuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), vertexBufferSize);

	// 頂点バッファビューを生成
	triangleData->vertexBufferView.BufferLocation = triangleData->vertexBuffer->GetGPUVirtualAddress();
	triangleData->vertexBufferView.SizeInBytes = vertexBufferSize;
	triangleData->vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースをマップ
	triangleData->vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&triangleData->vertexData));
}


/// -------------------------------------------------------------
///			　　	 　矩形の頂点データを生成
/// -------------------------------------------------------------
void Wireframe::CreateBoxVertexData(BoxData* boxData)
{
	UINT vertexBufferSize = sizeof(VertexData) * kBoxVertexCount * kBoxMaxCount;
	UINT indexBufferSize = sizeof(uint32_t) * kBoxIndexCount * kBoxMaxCount;

	// 頂点リソースを生成
	boxData->vertexBuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), vertexBufferSize);

	// 頂点バッファビューを生成
	boxData->vertexBufferView.BufferLocation = boxData->vertexBuffer->GetGPUVirtualAddress();
	boxData->vertexBufferView.SizeInBytes = vertexBufferSize;
	boxData->vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースをマップ
	boxData->vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&boxData->vertexData));

	// インデックスリソースを生成
	boxData->indexBuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), indexBufferSize);

	// インデックスバッファビューを生成
	boxData->indexBufferView.BufferLocation = boxData->indexBuffer->GetGPUVirtualAddress();
	boxData->indexBufferView.SizeInBytes = indexBufferSize;
	boxData->indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	// 頂点リソースをマップ
	boxData->indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&boxData->indexData));
}


/// -------------------------------------------------------------
///			　　		線の頂点データを生成
/// -------------------------------------------------------------
void Wireframe::CreateLineVertexData(LineData* lineData)
{
	UINT vertexBufferSize = sizeof(VertexData) * kLineVertexCount * kLineMaxCount_;

	// 頂点バッファビューを作成
	lineData->vertexBuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), vertexBufferSize);

	// 頂点バッファビューを生成
	lineData_->vertexBufferView.BufferLocation = lineData_->vertexBuffer->GetGPUVirtualAddress();
	lineData_->vertexBufferView.SizeInBytes = vertexBufferSize;
	lineData_->vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースをマップ
	lineData_->vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&lineData_->vertexData));
}


/// -------------------------------------------------------------
///			　　	 座標変換行列データを生成
/// -------------------------------------------------------------
void Wireframe::CreateTransformationMatrix()
{
	// 座標変換リソースを生成
	transformationMatrixBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(TransformationMatrix));

	// 座標変換行列リソースをマップ
	transformationMatrixBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

	transformationMatrixData_->WVP = Matrix4x4::Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix());
}


/// -------------------------------------------------------------
///			　　		球の頂点座標を生成
/// -------------------------------------------------------------
void Wireframe::CalcSphereVertexData()
{
	const float pi = 3.1415926535897932f;
	const uint32_t kSubdivision = 32; // 分割数
	const float kLonEvery = 2.0f * pi / float(kSubdivision); // 経度の1分割の角度
	const float kLatEvery = pi / float(kSubdivision); // 緯度の1分割の角度

	// 緯度方向
	for (uint32_t latIndex = 0; latIndex < kSubdivision; latIndex++)
	{
		float lat = -pi / 2.0f + kLatEvery * float(latIndex);
		// 経度方向
		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; lonIndex++)
		{
			float lon = kLonEvery * float(lonIndex);
			// 球の表面上の点を求める
			Vector3 a, b, c;
			a.x = 0.0f + 1.0f * cos(lat) * cos(lon);
			a.y = 0.0f + 1.0f * sin(lat);
			a.z = 0.0f + 1.0f * cos(lat) * sin(lon);

			b.x = 0.0f + 1.0f * cos(lat + kLatEvery) * cos(lon);
			b.y = 0.0f + 1.0f * sin(lat + kLatEvery);
			b.z = 0.0f + 1.0f * cos(lat + kLatEvery) * sin(lon);

			c.x = 0.0f + 1.0f * cos(lat) * cos(lon + kLonEvery);
			c.y = 0.0f + 1.0f * sin(lat);
			c.z = 0.0f + 1.0f * cos(lat) * sin(lon + kLonEvery);

			// 座標を保存
			spheres_.push_back(a);
			spheres_.push_back(b);
			spheres_.push_back(c);
		}
	}
}
