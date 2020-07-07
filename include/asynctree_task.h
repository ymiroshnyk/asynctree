#pragma once

#include "asynctree_config.h"
#include "asynctree_task_typedefs.h"
#include "asynctree_access_key.h"
#include "asynctree_callback.h"

#include <mutex>

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
	TaskImpl* const parent_;
	const EnumTaskWeight weight_ : 2;

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

	Task& task() { return task_; }
	TaskImpl* parent() { return parent_; }
	EnumTaskWeight weight() const { return weight_; }
	void exec();
	void destroy();
	void addChildTask(TaskImpl& child);
	void notifyDeferredTask();
	void addDeferredTask(TaskImpl& child);

	void start();

	void interrupt();
	bool isInterrupted() const;

private:
	void _addChildTaskNoIncCounter(TaskImpl& child, std::unique_lock<std::mutex>& lock);
	void _interruptWaitingTaskFromParent();
	void _onFinished(std::unique_lock<std::mutex>& lock);

	void _onChildExec(EnumTaskWeight weight);
	void _onChildFinished();
};

class Task : public std::enable_shared_from_this<Task>
{
	friend class TaskImpl;

	TaskImpl impl_;
	TaskP selfLock_;

	std::unique_ptr<DynamicCallback> succeededCb_;
	std::unique_ptr<DynamicCallback> interruptedCb_;
	std::unique_ptr<DynamicCallback> finishedCb_;

public:
	Task(Service& service, TaskImpl* parent, EnumTaskWeight weight);
	~Task();

	TaskImpl& _impl(AccessKey<Service, Mutex>);

	TaskP start();

	template <typename TFunc>
	Task& succeeded(TFunc func);

	template <typename TFunc>
	Task& interrupted(TFunc func);

	template <typename TFunc>
	Task& finished(TFunc func);
	
	void interrupt();
	bool isInterrupted() const;

protected:
	void setSelfLock(TaskP selfLock);
	virtual void _execWorkFunc() = 0;
	virtual void _execCallback(CallbackType type);

private:
	template <typename TFunc>
	Task& _callback(std::unique_ptr<DynamicCallback>& callback, TFunc func);
};

template <typename TFunc>
Task& Task::succeeded(TFunc func)
{
	return _callback(succeededCb_, std::move(func));
}

template <typename TFunc>
Task& Task::interrupted(TFunc func)
{
	return _callback(interruptedCb_, std::move(func));
}

template <typename TFunc>
Task& Task::finished(TFunc func)
{
	return _callback(finishedCb_, std::move(func));
}

template <typename TFunc>
Task& Task::_callback(std::unique_ptr<DynamicCallback>& callback, TFunc func)
{
	if (callback)
		callback = makeDynamicCallback([_next{ std::move(func) }, _prev{ std::move(callback) }]{
			_prev->exec();
			_next();
		});
	else
		callback = makeDynamicCallback(std::move(func));

	return *this;
}

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
		Task::_execCallback(type);

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

template <typename TaskWorkFunc, typename T1, typename T2>
class TaskTyped<TaskWorkFunc, T1, T2, StaticCallback<void>> : public Task
{
	TaskWorkFunc workFunc_;
	T1 t1_;
	T2 t2_;

public:
	TaskTyped(Service& service, TaskImpl* parent, EnumTaskWeight weight, TaskWorkFunc workFunc,
		T1 t1, T2 t2)
		: Task(service, parent, weight)
		, workFunc_(std::move(workFunc))
		, t1_(std::move(t1))
		, t2_(std::move(t2))
	{
	}

	static TaskP _create(AccessKey<Service, Mutex>, Service& service, TaskImpl* parent,
		EnumTaskWeight weight, TaskWorkFunc workFunc, T1 t1, T2 t2, StaticCallback<void>)
	{
		auto task = std::make_shared<TaskTyped<TaskWorkFunc, T1, T2, StaticCallback<void>>>(
			service, parent, weight, std::move(workFunc), std::move(t1), std::move(t2));
		task->setSelfLock(task);
		return task;
	}

	void _execWorkFunc() override
	{
		workFunc_();
	}

	void _execCallback(CallbackType type) override
	{
		Task::_execCallback(type);

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
	}
};

template <typename TaskWorkFunc, typename T1>
class TaskTyped<TaskWorkFunc, T1, StaticCallback<void>, StaticCallback<void>> : public Task
{
	TaskWorkFunc workFunc_;
	T1 t1_;

public:
	TaskTyped(Service& service, TaskImpl* parent, EnumTaskWeight weight, TaskWorkFunc workFunc, T1 t1)
		: Task(service, parent, weight)
		, workFunc_(std::move(workFunc))
		, t1_(std::move(t1))
	{
	}

	static TaskP _create(AccessKey<Service, Mutex>, Service& service, TaskImpl* parent,
		EnumTaskWeight weight, TaskWorkFunc workFunc, T1 t1, StaticCallback<void>, StaticCallback<void>)
	{
		auto task = std::make_shared<TaskTyped<TaskWorkFunc, T1, StaticCallback<void>, StaticCallback<void>>>(
			service, parent, weight, std::move(workFunc), std::move(t1));
		task->setSelfLock(task);
		return task;
	}

	void _execWorkFunc() override
	{
		workFunc_();
	}

	void _execCallback(CallbackType type) override
	{
		Task::_execCallback(type);

		if (t1_.type_ == type)
		{
			t1_.func_();
			return;
		}
	}
};

template <typename TaskWorkFunc>
class TaskTyped<TaskWorkFunc, StaticCallback<void>, StaticCallback<void>, StaticCallback<void>> : public Task
{
	TaskWorkFunc workFunc_;

public:
	TaskTyped(Service& service, TaskImpl* parent, EnumTaskWeight weight, TaskWorkFunc workFunc)
		: Task(service, parent, weight)
		, workFunc_(std::move(workFunc))
	{
	}

	static TaskP _create(AccessKey<Service, Mutex>, Service& service, TaskImpl* parent,
		EnumTaskWeight weight, TaskWorkFunc workFunc, StaticCallback<void>, StaticCallback<void>, StaticCallback<void>)
	{
		auto task = std::make_shared<TaskTyped<TaskWorkFunc, StaticCallback<void>, StaticCallback<void>, StaticCallback<void>>>(
				service, parent, weight, std::move(workFunc));
		task->setSelfLock(task);
		return task;
	}

	void _execWorkFunc() override
	{
		workFunc_();
	}
};

}
