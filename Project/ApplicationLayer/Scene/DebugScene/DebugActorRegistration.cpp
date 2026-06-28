#include "DebugActorRegistration.h"

#include <ActorFactory.h>

#include "TestActor.h"
#include "TestGroundActor.h"

using namespace Ken4lowEngine;

void RegisterDebugActors()
{
	// Json Spawnで生成できるDebug用Actorを登録する
	ActorFactory::RegisterActorClass<TestActor>("TestActor");
	ActorFactory::RegisterActorClass<TestGroundActor>("TestGroundActor");
}
