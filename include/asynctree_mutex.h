#pragma once

#include "asynctree_config.h"
#include "asynctree_task_callbacks.h"
#include "asynctree_task_typedefs.h"
#include "asynctree_access_key.h"

namespace AST
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

	TaskP startTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks = TaskCallbacks());
	TaskP startAutoTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks = TaskCallbacks());
	TaskP startChildTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks = TaskCallbacks());

	TaskP startSharedTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks = TaskCallbacks());
	TaskP startSharedAutoTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks = TaskCallbacks());
	TaskP startSharedChildTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks = TaskCallbacks());

	void _taskFinished(AccessKey<TaskImpl>);
private:
	TaskP _startTask(bool shared, EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks);
	TaskP _startAutoTask(bool shared, EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks);
	TaskP _startChildTask(bool shared, Task* parent, EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks);
	bool _checkIfTaskCanBeStartedAndIncCounters(bool shared);
	void _queueTask(TaskImpl& task);
	bool _checkIfTaskCanBeStartedFromQueueAndStart();
};

}
