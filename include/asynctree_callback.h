#pragma once

namespace ast
{

enum class CallbackType : unsigned char
{
	Succeeded = 0,
	Interrupted,
	Finished,
	Void
};

template <typename TFunc>
struct Callback
{
	CallbackType type_ : 2;
	TFunc func_;

	Callback(CallbackType type, TFunc func)
		: type_(type), func_(std::move(func)) {}
};

template <>
struct Callback<void>
{
	CallbackType type_ = CallbackType::Void;
	inline void func_() const {}
};

template <typename TFunc>
Callback<TFunc> succeeded(TFunc func)
{
	return Callback<TFunc>(CallbackType::Succeeded, std::move(func));
}

template <typename TFunc>
Callback<TFunc> interrupted(TFunc func)
{
	return Callback<TFunc>(CallbackType::Interrupted, std::move(func));
}

template <typename TFunc>
Callback<TFunc> finished(TFunc func)
{
	return Callback<TFunc>(CallbackType::Finished, std::move(func));
}

}