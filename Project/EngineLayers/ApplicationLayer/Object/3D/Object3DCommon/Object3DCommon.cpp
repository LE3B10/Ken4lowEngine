#include "Object3DCommon.h"

Object3DCommon* Object3DCommon::GetInstance()
{
	static Object3DCommon instance;
    return &instance;
}
