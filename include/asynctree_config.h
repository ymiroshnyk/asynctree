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

class VoidFunc
{
public:
	virtual ~VoidFunc() {}
	virtual void exec() = 0;
};

template <typename T>
class VoidFuncTyped : public VoidFunc
{
	T func_;
public:
	VoidFuncTyped(T func) : func_(std::move(func)) {}

	void exec() override
	{
		func_();
	}
};

enum EnumTaskWeight : unsigned char
{
	Light = 0,
	Middle,
	Heavy,

	TW_Quantity
};

}
