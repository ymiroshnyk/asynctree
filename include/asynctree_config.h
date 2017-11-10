#pragma once

#include <boost/noncopyable.hpp>
#include <boost/thread/tss.hpp>

#include <thread>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <memory>
#include <functional>
#include <cassert>
#include <cmath>

namespace AST
{

typedef unsigned int uint;

typedef std::function<void()> TaskWorkFunc;

enum EnumTaskWeight
{
	TW_Light = 0,
	TW_Middle,
	TW_Heavy,

	TW_Quantity
};

}
