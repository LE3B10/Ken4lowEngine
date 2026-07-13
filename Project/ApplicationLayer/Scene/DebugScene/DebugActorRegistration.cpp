#include "DebugActorRegistration.h"

#include <ActorFactory.h>
#include <ComponentFactory.h>

#include "ApplicationLayer/Character/Enemy/Actor/EnemyActor.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyAIComponent.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyAttackComponent.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyEffectComponent.h"
#include "TestActor.h"
#include "TestGroundActor.h"

using namespace Ken4lowEngine;

namespace
{
	/// 通常敵専用ComponentをEditor追加とActor JSON復元へ登録する情報を生成する。
	template<class T>
	ComponentFactory::ComponentTypeInfo MakeEnemyComponentTypeInfo(const char* className, const char* displayName, const char* description)
	{
		ComponentFactory::ComponentTypeInfo typeInfo{};
		typeInfo.className = className;
		typeInfo.displayName = displayName;
		typeInfo.category = "通常敵";
		typeInfo.description = description;
		typeInfo.allowMultiple = false;
		typeInfo.canBeRoot = false;
		typeInfo.createFunc = [](Actor* owner) -> ActorComponent*
		{
			return owner ? &owner->AddComponent<T>() : nullptr;
		};
		return typeInfo;
	}
}

void RegisterDebugActors()
{
	// Json Spawnで生成できるDebug用Actorを登録する
	ActorFactory::RegisterActorClass<TestActor>("TestActor");
	ActorFactory::RegisterActorClass<TestGroundActor>("TestGroundActor");
	ActorFactory::RegisterActorClass<EnemyActor>("EnemyActor"); // 通常敵ActorをEditor/JSON生成候補へ接続する。
	ComponentFactory::RegisterComponentType(MakeEnemyComponentTypeInfo<EnemyAIComponent>("EnemyAIComponent", "通常敵AI", "A*追跡判断と移動速度出力を管理します。"));
	ComponentFactory::RegisterComponentType(MakeEnemyComponentTypeInfo<EnemyAttackComponent>("EnemyAttackComponent", "通常敵攻撃", "攻撃間隔とダメージ適用を管理します。"));
	ComponentFactory::RegisterComponentType(MakeEnemyComponentTypeInfo<EnemyEffectComponent>("EnemyEffectComponent", "通常敵Effect", "被弾・死亡Effectと死亡表示を管理します。"));
}
