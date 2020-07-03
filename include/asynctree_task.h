#pragma once

#include "asynctree_config.h"
#include "asynctree_task_typedefs.h"
#include "asynctree_access_key.h"

namespace ast
{

typedef std::function<void()> TaskSucceededCallback;
typedef std::function<void()> TaskInterruptedCallback;
typedef std::function<void()> TaskFinishedCallback;

class Service;
class Mutex;
class Task;

class TaskImpl 
{
public:
	// hooks and parameters for different queues in service, mutexes and tasks
	TaskImpl* next_;
	EnumTaskWeight weight_;
	Mutex* mutex_;
	uint shared_ : 1;

private:
	enum State
	{
		S_Created,
		S_Working,
		S_WaitForChildren,
		S_Done,
	};

	Task& task_;
	Service& service_;

	TaskImpl* parent_;

	TaskWorkFunc workFunc_;
	std::unique_ptr<VoidFunc> succeededCb_;
	std::unique_ptr<VoidFunc> interruptedCb_;
	std::unique_ptr<VoidFunc> finishedCb_;

	std::mutex taskMutex_;

	State state_;

	mutable uint interrupted_ : 1;

	uint numChildrenToComplete_ : 20;

	struct WeightBuffer
	{
		TaskImpl* firstChild_ = nullptr;
		TaskImpl* lastChild_ = nullptr;
	};

	WeightBuffer weightBuffers_[TW_Quantity];

public:
	TaskImpl(AccessKey<Task>, Task& task, Service& service, TaskImpl* parent, 
		EnumTaskWeight weight, TaskWorkFunc workFunc);
	~TaskImpl();

	Task& task();
	TaskImpl* parent();
	void exec(EnumTaskWeight weight);
	void destroy();
	void addChildTask(EnumTaskWeight weight, TaskImpl& child);
	void notifyDeferredTask();
	void addDeferredTask(EnumTaskWeight weight, TaskImpl& child);

	void succeeded(std::unique_ptr<VoidFunc> succeeded);
	void interrupted(std::unique_ptr<VoidFunc> interrupted);
	void finished(std::unique_ptr<VoidFunc> finished);
	void start();

	void interrupt();
	bool isInterrupted() const;

private:
	void _addChildTaskNoIncCounter(EnumTaskWeight weight, TaskImpl& child, std::unique_lock<std::mutex>& lock);
	void _interruptFromParent();

	void _onFinished(std::unique_lock<std::mutex>& lock);
	void _notifyChildFinished();
};

class Task : public std::enable_shared_from_this<Task>
{
	friend class TaskImpl;

	TaskImpl impl_;
	TaskP selfLock_;

	struct Private {};
public:
	Task(Private, Service& service, TaskImpl* parent, EnumTaskWeight weight, TaskWorkFunc workFunc);
	~Task();

	static TaskP _create(AccessKey<Service, Mutex>, Service& service, TaskImpl* parent, 
		EnumTaskWeight weight, TaskWorkFunc workFunc);
	TaskImpl& _impl(AccessKey<Service, Mutex>);

	template <typename TFunc>
	inline Task& succeeded(TFunc succeeded)
	{
		auto voidFunc = std::make_unique<VoidFuncTyped<TFunc>>(std::move(succeeded));
		impl_.succeeded(std::move(voidFunc));
		return *this;
	}

	template <typename TFunc>
	inline Task& interrupted(TFunc interrupted)
	{
		auto voidFunc = std::make_unique<VoidFuncTyped<TFunc>>(std::move(interrupted));
		impl_.interrupted(std::move(voidFunc));
		return *this;
	}

	template <typename TFunc>
	inline Task& finished(TFunc finished)
	{
		auto voidFunc = std::make_unique<VoidFuncTyped<TFunc>>(std::move(finished));
		impl_.finished(std::move(voidFunc));
		return *this;
	}

	TaskP start();

	void interrupt();
	bool isInterrupted() const;
};

}
