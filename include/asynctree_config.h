#pragma once

#include <thread>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <memory>
#include <functional>
#include <cassert>
#include <cmath>

namespace ast
{

typedef unsigned int uint;

typedef std::function<void()> TaskWorkFunc;

enum EnumTaskWeight
{
	Light = 0,
	Middle,
	Heavy,

	TW_Quantity
};

}
