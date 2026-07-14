#include "DebugActorRegistration.h"

#include <ActorFactory.h>
#include <ComponentFactory.h>

#include "ApplicationLayer/Character/Enemy/Actor/EnemyActor.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyAIComponent.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyAttackComponent.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyEffectComponent.h"
#include "ApplicationLayer/Character/Boss/Actor/BossActor.h"
#include "ApplicationLayer/Character/Boss/Actor/BossActorAttackComponent.h"
#include "ApplicationLayer/Character/Boss/Actor/BossBrainComponent.h"
#include "ApplicationLayer/Character/Boss/Actor/BossPresentationComponent.h"
#include "ApplicationLayer/Character/Boss/Components/BossPhaseComponent.h"
#include "ApplicationLayer/Character/Boss/Components/BossWeakPointComponent.h"
#include "ApplicationLayer/Character/Player/Actor/PlayerActor.h"
#include "TestActor.h"
#include "TestGroundActor.h"

using namespace Ken4lowEngine;

namespace
{
	/// ApplicationLayer専用ComponentをEditor追加とActor JSON復元へ登録する情報を生成する。
	template<class T>
	ComponentFactory::ComponentTypeInfo MakeApplicationComponentTypeInfo(const char* className, const char* displayName, const char* category, const char* description)
	{
		ComponentFactory::ComponentTypeInfo typeInfo{};
		typeInfo.className = className;
		typeInfo.displayName = displayName;
		typeInfo.category = category;
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
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<EnemyAIComponent>("EnemyAIComponent", "通常敵AI", "通常敵", "A*追跡判断と移動速度出力を管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<EnemyAttackComponent>("EnemyAttackComponent", "通常敵攻撃", "通常敵", "攻撃間隔とダメージ適用を管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<EnemyEffectComponent>("EnemyEffectComponent", "通常敵Effect", "通常敵", "被弾・死亡Effectと死亡表示を管理します。"));

	ActorFactory::RegisterActorClass<BossActor>("BossActor");
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<BossBrainComponent>("BossBrainComponent", "ボス行動判断", "ボス", "Target追跡と攻撃要求を判断します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<BossAttackComponent>("BossAttackComponent", "ボス攻撃", "ボス", "フェーズ別攻撃選択と共通攻撃実行を管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<BossPhaseComponent>("BossPhaseComponent", "ボスフェーズ", "ボス", "共通HPからフェーズを判定します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<BossWeakPointComponent>("BossWeakPointComponent", "ボス弱点", "ボス", "人型部位IDを参照して弱点倍率を管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<BossPresentationComponent>("BossPresentationComponent", "ボス演出", "ボス", "フェーズ遷移と死亡演出を管理します。"));

	ActorFactory::RegisterActorClass<PlayerActor>("PlayerActor"); // PlayerActorをEditor配置とJSON復元へ接続する。
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<PlayerInputComponent>("PlayerInputComponent", "プレイヤー入力", "プレイヤー", "入力を具体処理へ変換せず、各専用Componentへ要求だけ配送します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<PlayerMovementComponent>("PlayerMovementComponent", "プレイヤー移動", "プレイヤー", "移動入力を速度へ変換し、共通Character移動へ委譲します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<WeaponComponent>("WeaponComponent", "武器", "プレイヤー", "射撃・リロード要求と弾薬状態を管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<InventoryComponent>("InventoryComponent", "Inventory", "プレイヤー", "武器スロットと選択中の装備を管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<PlayerCameraComponent>("PlayerCameraComponent", "プレイヤーカメラ", "プレイヤー", "視点要求を角度へ反映し、共通CameraComponentへ同期します。"));
}
