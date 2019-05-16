#pragma once

#include "asynctree_config.h"

namespace ast
{

typedef std::function<void()> TaskSucceededCallback;
typedef std::function<void()> TaskInterruptedCallback;
typedef std::function<void()> TaskFinishedCallback;

struct TaskCallbacks
{
	TaskSucceededCallback succeeded_;
	TaskInterruptedCallback interrupted_;
	TaskFinishedCallback finished_;

	TaskCallbacks& succeeded(TaskSucceededCallback succeeded) { succeeded_ = std::move(succeeded); return *this; }
	TaskCallbacks& interrupted(TaskInterruptedCallback interrupted) { interrupted_ = std::move(interrupted); return *this; }
	TaskCallbacks& finished(TaskFinishedCallback finished) { finished_ = std::move(finished); return *this; }
};

}
