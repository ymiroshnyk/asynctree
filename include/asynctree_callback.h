#pragma once

namespace ast
{

enum class CallbackType : unsigned char
{
	Succeeded = 0,
	Interrupted,
	Finished
};

template <typename TFunc>
struct StaticCallback
{
	CallbackType type_ : 2;
	TFunc func_;

	StaticCallback(CallbackType type, TFunc func)
		: type_(type), func_(std::move(func)) {}
};

template <>
struct StaticCallback<void> {};

template <typename TFunc>
StaticCallback<TFunc> succeeded(TFunc func)
{
	return StaticCallback<TFunc>(CallbackType::Succeeded, std::move(func));
}

template <typename TFunc>
StaticCallback<TFunc> interrupted(TFunc func)
{
	return StaticCallback<TFunc>(CallbackType::Interrupted, std::move(func));
}

template <typename TFunc>
StaticCallback<TFunc> finished(TFunc func)
{
	return StaticCallback<TFunc>(CallbackType::Finished, std::move(func));
}

class DynamicCallback
{
public:
	virtual ~DynamicCallback() {}
	virtual void exec() = 0;
};

template <typename T>
class DynamicCallbackTyped : public DynamicCallback
{
	T func_;
public:
	DynamicCallbackTyped(T func) : func_(std::move(func)) {}

	void exec() override
	{
		func_();
	}
};

template <typename TFunc>
std::unique_ptr<DynamicCallbackTyped<TFunc>> makeDynamicCallback(TFunc func)
{
	return std::make_unique<DynamicCallbackTyped<TFunc>>(std::move(func));
}

}