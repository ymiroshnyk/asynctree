#pragma once

#include "asynctree_config.h"
#include "asynctree_task_callbacks.h"
#include "asynctree_task_typedefs.h"
#include "asynctree_access_key.h"

namespace ast
{

class Service;
class TaskImpl;

class Mutex
{
	Service& service_;

	std::mutex mutex_;
	TaskImpl* firstQueuedChild_;
	TaskImpl* lastQueuedChild_;

	bool sharedTasksInProgress_;
	uint numTasksToBeFinished_;

	std::condition_variable destroyCV_;

public:
	Mutex(Service& service);
	~Mutex();

	template <typename TaskWorkFunc, typename T1, typename T2, typename T3>
	Task& rootTask(EnumTaskWeight weight, TaskWorkFunc workFunc, T1 t1, T2 t2, T3 t3);
	template <typename TaskWorkFunc, typename T1, typename T2, typename T3>
	Task& task(EnumTaskWeight weight, TaskWorkFunc workFunc, T1 t1, T2 t2, T3 t3);
	template <typename TaskWorkFunc, typename T1, typename T2, typename T3>
	Task& sharedRootTask(EnumTaskWeight weight, TaskWorkFunc workFunc, T1 t1, T2 t2, T3 t3);
	template <typename TaskWorkFunc, typename T1, typename T2, typename T3>
	Task& sharedTask(EnumTaskWeight weight, TaskWorkFunc workFunc, T1 t1, T2 t2, T3 t3);

	void _startTask(AccessKey<TaskImpl>, TaskImpl& taskImpl);
	void _taskFinished(AccessKey<TaskImpl>);

private:
	template <typename TaskWorkFunc, typename T1, typename T2, typename T3>
	TaskP _task(bool shared, EnumTaskWeight weight, Task* parent, TaskWorkFunc workFunc, T1 t1, T2 t2, T3 t3);

	bool _checkIfTaskCanBeStartedAndIncCounters(bool shared);
	void _queueTask(TaskImpl& task);
	bool _checkIfTaskCanBeStartedFromQueueAndStart();
};

template <typename TaskWorkFunc, typename T1, typename T2, typename T3>
Task& Mutex::rootTask(EnumTaskWeight weight, TaskWorkFunc workFunc, T1 t1, T2 t2, T3 t3)
{
	return _task(false, weight, nullptr, std::move(workFunc), 
		std::move(t1), std::move(t2), std::move(t3))
}

template <typename TaskWorkFunc, typename T1, typename T2, typename T3>
Task& Mutex::task(EnumTaskWeight weight, TaskWorkFunc workFunc, T1 t1, T2 t2, T3 t3)
{
	return _task(false, weight, Service::currentTask(), std::move(workFunc),
		std::move(t1), std::move(t2), std::move(t3))
}

template <typename TaskWorkFunc, typename T1, typename T2, typename T3>
Task& Mutex::sharedRootTask(EnumTaskWeight weight, TaskWorkFunc workFunc, T1 t1, T2 t2, T3 t3)
{
	return _task(true, weight, nullptr, std::move(workFunc),
		std::move(t1), std::move(t2), std::move(t3))
}

template <typename TaskWorkFunc, typename T1, typename T2, typename T3>
Task& Mutex::sharedTask(EnumTaskWeight weight, TaskWorkFunc workFunc, T1 t1, T2 t2, T3 t3)
{
	return _task(true, weight, Service::currentTask(), std::move(workFunc),
		std::move(t1), std::move(t2), std::move(t3))
}

template <typename TaskWorkFunc, typename T1, typename T2, typename T3>
TaskP Mutex::_task(bool shared, EnumTaskWeight weight, Task* parent, TaskWorkFunc workFunc, T1 t1, T2 t2, T3 t3)
{
	auto task = TaskTyped<TaskWorkFunc, T1, T2, T3>::_create(KEY, service_, 
		parent ? &parent->_impl(KEY) : nullptr, weight, std::move(workFunc), std::move(t1),
		std::move(t2), std::move(t3));
	auto& taskImpl = task->_impl(KEY);
	taskImpl.shared_ = shared;
	taskImpl.mutex_ = this;
	return task;
}

}
