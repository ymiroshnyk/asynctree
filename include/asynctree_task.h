#pragma once

#include "asynctree_config.h"
#include "asynctree_task_typedefs.h"
#include "asynctree_access_key.h"
#include "asynctree_callback.h"

namespace ast
{

class Service;
class Mutex;
class Task;

class TaskImpl 
{
public:
	// hooks and parameters for different queues in service, mutexes and tasks
	TaskImpl* next_;
	EnumTaskWeight weight_ : 2;
	Mutex* mutex_;
	uint shared_ : 1;

private:
	enum class State : unsigned char
	{
		Created = 0,
		Working,
		WaitForChildren,
		Done,
	};

	Task& task_;
	Service& service_;

	TaskImpl* parent_;

	std::mutex taskMutex_;

	State state_ : 2;

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
		EnumTaskWeight weight);
	~TaskImpl();

	Task& task();
	TaskImpl* parent();
	void exec(EnumTaskWeight weight);
	void destroy();
	void addChildTask(EnumTaskWeight weight, TaskImpl& child);
	void notifyDeferredTask();
	void addDeferredTask(EnumTaskWeight weight, TaskImpl& child);

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

public:
	Task(Service& service, TaskImpl* parent, EnumTaskWeight weight);
	~Task();

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

protected:
	void setSelfLock(TaskP selfLock);

private:
	virtual void _execWorkFunc() = 0;
	virtual void _execCallback(CallbackType type) = 0;
};


template <typename TaskWorkFunc, typename T1, typename T2, typename T3>
class TaskTyped : public Task
{
	TaskWorkFunc workFunc_;
	T1 t1_;
	T2 t2_;
	T3 t3_;

public:
	TaskTyped(Service& service, TaskImpl* parent, EnumTaskWeight weight, TaskWorkFunc workFunc,
		T1 t1, T2 t2, T3 t3)
		: Task(service, parent, weight)
		, workFunc_(std::move(workFunc))
		, t1_(std::move(t1))
		, t2_(std::move(t2))
		, t3_(std::move(t3))
	{
	}

	static TaskP _create(AccessKey<Service, Mutex>, Service& service, TaskImpl* parent,
		EnumTaskWeight weight, TaskWorkFunc workFunc, T1 t1, T2 t2, T3 t3)
	{
		auto task = std::make_shared<TaskTyped<TaskWorkFunc, T1, T2, T3>>(service, parent, weight, std::move(workFunc),
			std::move(t1), std::move(t2), std::move(t3));
		task->setSelfLock(task);
		return task;
	}

	void _execWorkFunc() override
	{
		workFunc_();
	}

	void _execCallback(CallbackType type) override
	{
		if (t1_.type_ == type)
		{
			t1_.func_();
			return;
		}

		if (t2_.type_ == type)
		{
			t2_.func_();
			return;
		}

		if (t3_.type_ == type)
		{
			t3_.func_();
			return;
		}
	}
};

}
