#include "ModelManager.h"

/// ---------- 初期容量の設定 ----- ///
constexpr size_t INITIAL_POSITION_CAPACITY = 1000; // 頂点位置（v）の初期容量
constexpr size_t INITIAL_NORMAL_CAPACITY = 1000;   // 法線（vn）の初期容量
constexpr size_t INITIAL_TEXCOORD_CAPACITY = 1000; // テクスチャ座標（vt）の初期容量
constexpr size_t INITIAL_VERTEX_CAPACITY = 3000;   // 頂点データ全体の初期容量


/// -------------------------------------------------------------
///					シングルトンインスタンス
/// -------------------------------------------------------------
ModelManager* ModelManager::GetInstance()
{
	static ModelManager instance;
	return &instance;
}


/// -------------------------------------------------------------
///					　.objファイルの読み込み
/// -------------------------------------------------------------
ModelData ModelManager::LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
	// モデルデータの格納先
	ModelData modelData;
	std::vector<Vector4> positions; // 頂点位置リスト
	std::vector<Vector3> normals;   // 法線リスト
	std::vector<Vector2> texcoords; // テクスチャリスト

	// ファイルを開く
	std::ifstream file(directoryPath + "/" + filename);
	if (!file.is_open())
	{
		// ファイルが開けなかった場合、例外を投げる
		throw std::runtime_error("Failed to open OBJ file: " + directoryPath + "/" + filename);
	}

	// メモリを事前に確保（処理の効率化）
	positions.reserve(INITIAL_POSITION_CAPACITY);
	normals.reserve(INITIAL_NORMAL_CAPACITY);
	texcoords.reserve(INITIAL_TEXCOORD_CAPACITY);
	modelData.vertices.reserve(INITIAL_VERTEX_CAPACITY);

	std::string line;
	while (std::getline(file, line))
	{
		std::istringstream s(line); // 行を文字列ストリームに変換
		std::string identifier;
		s >> identifier;            // 行の識別子を取得

		if (identifier == "v")
		{
			// 頂点位置情報を解析しリストに追加
			positions.push_back(ParseVertex(s));
		}
		else if (identifier == "vt")
		{
			// テクスチャ座標情報を解析しリストに追加
			texcoords.push_back(ParseTexcoord(s));
		}
		else if (identifier == "vn")
		{
			// 法線情報を解析しリストに追加
			normals.push_back(ParseNormal(s));
		}
		else if (identifier == "f")
		{
			// 面情報を解析し、頂点リストに展開して追加
			ParseFace(s, positions, texcoords, normals, modelData.vertices);
		}
		else if (identifier == "mtllib")
		{
			// マテリアルテンプレートライブラリファイル名を取得しロード
			std::string materialFilename;
			s >> materialFilename;
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}

	// 解析済みのモデルデータを返す
	return modelData;
}


/// -------------------------------------------------------------
///					　モデルファイルの読み込み
/// -------------------------------------------------------------
void ModelManager::LoadModel(const std::string& filePath)
{
	// 読み込み済みモデルを検索
	if (models_.contains(filePath))
	{
		return; // 既にロード済み
	}

	// モデルの生成とファイル読み込み - 初期化
	auto model = std::make_shared<Model>();
	model->Initialize("Resources", filePath);

	// モデルをmapコンテナに格納する
	models_.insert(std::make_pair(filePath, model));
}



/// -------------------------------------------------------------
///					　モデルデータの取得関数
/// -------------------------------------------------------------
std::shared_ptr<Model> ModelManager::FindModel(const std::string& filePath)
{
	auto it = models_.find(filePath);
	if (it != models_.end()) {
		return it->second; // 既に存在するモデルを共有
	}

	// モデルが存在しない場合、新たに作成
	auto newModel = std::make_shared<Model>();
	newModel->Initialize("Resources", filePath);
	models_[filePath] = newModel;
	return newModel;
}


/// -------------------------------------------------------------
///					　頂点データを解析する
/// -------------------------------------------------------------
Vector4 ModelManager::ParseVertex(std::istringstream& s)
{
	Vector4 position{};
	s >> position.x >> position.y >> position.z;
	position.w = 1.0f;
	return position;
}


/// -------------------------------------------------------------
///				テクスチャ座標データを解析する
/// -------------------------------------------------------------
Vector2 ModelManager::ParseTexcoord(std::istringstream& s)
{
	Vector2 texcoord{};
	s >> texcoord.x >> texcoord.y;
	return texcoord;
}


/// -------------------------------------------------------------
///					法線データを解析する
/// -------------------------------------------------------------
Vector3 ModelManager::ParseNormal(std::istringstream& s)
{
	Vector3 normal{};
	s >> normal.x >> normal.y >> normal.z;
	return normal;
}


/// -------------------------------------------------------------
///		　面情報を解析し、頂点データリストに展開して追加する
/// -------------------------------------------------------------
void ModelManager::ParseFace(std::istringstream& s,
	const std::vector<Vector4>& positions,
	const std::vector<Vector2>& texcoords,
	const std::vector<Vector3>& normals,
	std::vector<VertexData>& vertices)
{
	VertexData triangle[3]; // 三角形の3頂点データ
	for (int32_t i = 0; i < 3; ++i)
	{
		std::string vertexDefinition;
		s >> vertexDefinition; // 面の頂点情報を取得
		std::istringstream v(vertexDefinition);
		uint32_t indices[3] = { 0 }; // 頂点位置 - テクスチャ座標 - 法線のインデックス

		for (int32_t j = 0; j < 3; ++j)
		{
			std::string index;
			if (std::getline(v, index, '/'))
			{
				// " / " で区切られたインデックスを解析（空の場合は0）
				indices[j] = index.empty() ? 0 : std::stoi(index);
			}
		}

		// 各インデックスを使って対応するデータを取得
		Vector4 position = positions[indices[0] - 1];						   // 頂点位置
		Vector2 texcoord = indices[1] ? texcoords[indices[1] - 1] : Vector2{}; // テクスチャ座標
		Vector3 normal = indices[2] ? normals[indices[2] - 1] : Vector3{};	   // 法線

		// データの反転処理（座標系調整）
		position.x *= -1;
		texcoord.y = 1.0f - texcoord.y;
		normal.x *= -1;

		// 頂点データを作成
		triangle[i] = { position, texcoord, normal };
	}

	// 頂点データリストに三角形の頂点を追加（頂点順序を反転）
	vertices.insert(vertices.end(), { triangle[2], triangle[1], triangle[0] });
}


/// -------------------------------------------------------------
///					　.mtlファイルの読み取り
/// -------------------------------------------------------------
MaterialData ModelManager::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
	// 1. 必要な変数の宣言
	MaterialData materialData;
	std::string line;

	// 2. ファイルを開く
	std::ifstream file(directoryPath + "/" + filename);
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open material file: " + directoryPath + "/" + filename);
	}

	// 3. 事前準備: ディレクトリパスに"/"を追加
	std::string basePath = directoryPath + "/";

	// 4. ファイルの内容を解析
	while (std::getline(file, line)) {
		// 行の前後の空白を削除（必要に応じて）
		line.erase(0, line.find_first_not_of(" \t"));
		line.erase(line.find_last_not_of(" \t") + 1);

		// 空行やコメント行をスキップ
		if (line.empty() || line[0] == '#') {
			continue;
		}

		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// 識別子に基づいて処理を分岐
		if (identifier == "map_Kd") {
			std::string textureFilename;
			if (s >> textureFilename) {
				// テクスチャファイルのパスを構築
				materialData.textureFilePath = basePath + textureFilename;
			}
			else {
				throw std::runtime_error("Invalid map_Kd format in material file: " + filename);
			}
		}
	}

	// 5. MaterialDataを返す
	return materialData;
}


ModelData ModelManager::LoadModelFile(const std::string& directoryPath, const std::string& filename)
{
	// 1. ファイルの拡張子を取得して判定
	std::string extension = filename.substr(filename.find_last_of('.') + 1);

	// 2. Assimp でモデルを読み込む
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = nullptr;

	if (extension == "obj")
	{
		// OBJ ファイル読み込み
		scene = importer.ReadFile(filePath.c_str(),
			aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
	}
	else if (extension == "gltf" || extension == "glb")
	{
		// glTF ファイル読み込み (追加フラグあり)
		scene = importer.ReadFile(filePath.c_str(),
			aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_PreTransformVertices);
	}
	else
	{
		throw std::runtime_error("Unsupported file format: " + extension);
	}

	// ファイル読み込み結果をチェック
	assert(scene && scene->HasMeshes());

	// 3. モデルデータ構造体を作成
	ModelData modelData;

	// 4. メッシュを解析 (共通処理)
	ParseMeshes(scene, directoryPath, modelData);

	// 5. ノード階層を構築 (共通処理)
	modelData.rootNode = ReadNode(scene->mRootNode);

	// 6. 必要に応じたカスタム処理
	if (extension == "gltf" || extension == "glb")
	{
		// glTF 特有のカスタム処理
		HandleGltfSpecifics(scene);
	}
	else if (extension == "obj")
	{
		// OBJ 特有のカスタム処理
		HandleObjSpecifics(scene);
	}

	return modelData;
}



ModelData ModelManager::LoadObjFile1(const std::string& directoryPath, const std::string& filename)
{
	// 中で必要なる変数の宣言
	ModelData modelData;

	// assimpでobjを読み込む
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
	assert(scene->HasMeshes()); // メッシュがないのは対応しない

	// meshを解析する
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
	{
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals()); // 法線がないMeshは今回は非対応
		assert(mesh->HasTextureCoords(0)); // TexcoordがないMeshは今回は非対応

		// ここからMeshの中身(face)を解析を行っていく

		// faceを解析する
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
		{
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3); // 三角形のみサポート

			// ここからFaceの中身(Vertex)の解析を行っていく

			// Vertexを解析する
			for (uint32_t element = 0; element < face.mNumIndices; ++element)
			{
				uint32_t vertexIndex = face.mIndices[element];
				aiVector3D& position = mesh->mVertices[vertexIndex];
				aiVector3D& normal = mesh->mNormals[vertexIndex];
				aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
				VertexData vertex{};
				vertex.position = { position.x,position.y, position.z, 1.0f };
				vertex.normal = { normal.x,normal.y,normal.z };
				vertex.texcoord = { texcoord.x,texcoord.y };
				// aiProcess_MakeLeftHandledはz *= -1で、右手->左手に変換するので手動で対処
				vertex.position.x *= -1.0f;
				vertex.normal.x *= -1.0f;
				modelData.vertices.push_back(vertex);
			}

			// maretialを解析する
			for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
			{
				aiMaterial* material = scene->mMaterials[materialIndex];
				if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0)
				{
					aiString textureFilePath;
					material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
					modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
				}
			}
		}
	}

	return modelData;
}



Node ModelManager::ReadNode(aiNode* node)
{
	Node result;

	aiVector3D scale, translate;
	aiQuaternion rotate;
	node->mTransformation.Decompose(scale, rotate, translate); // assimpの行列からSRTを抽出する関数を利用
	result.transform.scale = { scale.x,scale.y,scale.z }; // Scaleはそのまま
	result.transform.rotate = { rotate.x,-rotate.y,-rotate.z,rotate.w }; // x軸を反転、さらに回転方向が逆なので軸を反転させる
	result.transform.translate = { -translate.x,translate.y,translate.z }; // x軸を反転
	result.localMatrix = Matrix4x4::MakeAffineMatrix(result.transform.scale, result.transform.rotate, result.transform.translate);
	result.name = node->mName.C_Str();
	result.children.resize(node->mNumChildren);

	// 子ノードを再帰的に処理
	for (unsigned int i = 0; i < node->mNumChildren; ++i)
	{
		result.children[i] = ReadNode(node->mChildren[i]);
	}

	return result;
}

ModelData ModelManager::LoadGLTFFile(const std::string& directoryPath, const std::string& filename)
{
	ModelData modelData;

	// Assimpでファイルを読み込む
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_PreTransformVertices);
	assert(scene && scene->HasMeshes());

	// メッシュを解析する
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
	{
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals()); // 法線がないメッシュは非対応
		assert(mesh->HasTextureCoords(0)); // テクスチャ座標がないメッシュは非対応

		// メッシュの頂点データを解析
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
		{
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3); // 三角形のみ対応

			// 各頂点を解析
			for (uint32_t element = 0; element < face.mNumIndices; ++element)
			{
				uint32_t vertexIndex = face.mIndices[element];
				aiVector3D& position = mesh->mVertices[vertexIndex];
				aiVector3D& normal = mesh->mNormals[vertexIndex];
				aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];

				VertexData vertex{};
				vertex.position = { position.x, position.y, position.z, 1.0f };
				vertex.normal = { normal.x, normal.y, normal.z };
				vertex.texcoord = { texcoord.x, texcoord.y };

				// 右手系 -> 左手系に変換
				vertex.position.x *= -1.0f;
				vertex.normal.x *= -1.0f;

				modelData.vertices.push_back(vertex);
			}
		}

		// マテリアルを解析する
		for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
		{
			aiMaterial* material = scene->mMaterials[materialIndex];

			// Diffuse テクスチャが存在する場合のみ設定
			if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
			{
				aiString textureFilePath;
				material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
				modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
			}
			else
			{
				// Diffuse テクスチャがない場合のデフォルト設定
				modelData.material.textureFilePath = ""; // またはデフォルトのテクスチャパス
			}

			// NOTE: vector型がないため、最初のマテリアルだけを使用
			break; // 最初のマテリアルだけを設定してループを抜ける
		}
	}

	// ノード階層を構築
	modelData.rootNode = ReadNode(scene->mRootNode);

	return modelData;
}

void ModelManager::ParseMeshes(const aiScene* scene, const std::string& directoryPath, ModelData& modelData)
{
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
	{
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals());
		assert(mesh->HasTextureCoords(0));

		// 頂点データの解析
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
		{
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3); // 三角形のみ対応

			for (uint32_t element = 0; element < face.mNumIndices; ++element)
			{
				uint32_t vertexIndex = face.mIndices[element];
				aiVector3D& position = mesh->mVertices[vertexIndex];
				aiVector3D& normal = mesh->mNormals[vertexIndex];
				aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];

				VertexData vertex{};
				vertex.position = { position.x, position.y, position.z, 1.0f };
				vertex.normal = { normal.x, normal.y, normal.z };
				vertex.texcoord = { texcoord.x, texcoord.y };

				// 右手系 -> 左手系変換
				vertex.position.x *= -1.0f;
				vertex.normal.x *= -1.0f;

				modelData.vertices.push_back(vertex);
			}
		}

		// マテリアルの解析
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
		{
			aiString textureFilePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
			modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
		}
		else
		{
			modelData.material.textureFilePath = ""; // デフォルト値
		}
	}
}

void ModelManager::HandleGltfSpecifics(const aiScene* scene)
{
	if (scene->HasAnimations())
	{
		// アニメーション解析処理 (スキンメッシュなど)
		for (unsigned int i = 0; i < scene->mNumAnimations; ++i)
		{
			aiAnimation* animation = scene->mAnimations[i];
			// アニメーションデータを解析
		}
	}
}

void ModelManager::HandleObjSpecifics(const aiScene* scene)
{
	// 必要に応じてOBJ特有の処理を追加
	// 例: グループ情報の解析
}
