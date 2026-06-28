#include "ActorFactory.h"

#include <string>
#include <unordered_map>

namespace Ken4lowEngine
{
	namespace
	{
		/// <summary>
		/// Actor生成関数の登録テーブルを取得する
		/// </summary>
		std::unordered_map<std::string, ActorFactory::CreatorFunc>& GetActorFactoryRegister()
		{
			static std::unordered_map<std::string, ActorFactory::CreatorFunc> registry; // Actor生成関数の登録マップ
			return registry;
		}
	}

	void ActorFactory::RegisterActor(std::string_view className, CreatorFunc creator)
	{
		if (className.empty() || !creator)
		{
			return; // 無効なクラス名または生成関数なら登録しない
		}

		GetActorFactoryRegister()[std::string(className)] = std::move(creator); // クラス名と生成関数を登録する
	}

	std::unique_ptr<Actor> Ken4lowEngine::ActorFactory::CreateActor(std::string_view className)
	{
		const auto& registry = GetActorFactoryRegister();

		const auto it = registry.find(std::string(className));
		if (it == registry.end())
		{
			return nullptr; // 登録されていないクラス名ならnullptrを返す
		}

		return it->second(); // 登録された生成関数を呼び出してActorを生成する
	}

	bool ActorFactory::IsRegistered(std::string_view className)
	{
		const auto& registry = GetActorFactoryRegister();
		return registry.contains(std::string(className)); // 登録済みかどうかを返す
	}

}