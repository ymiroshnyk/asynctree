#pragma once

#include "asynctree_config.h"
#include "asynctree_task_callbacks.h"
#include "asynctree_task_typedefs.h"

namespace AST
{

class Service;

class Mutex
{
	Service& service_;

	std::mutex mutex_;
	Task* firstQueuedChild_;
	Task* lastQueuedChild_;

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

	void taskFinished();
private:
	TaskP _startTask(bool shared, EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks);
	TaskP _startAutoTask(bool shared, EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks);
	TaskP _startChildTask(bool shared, Task* parent, EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks);
	bool checkIfTaskCanBeStartedAndIncCounters(bool shared);
	void queueTask(Task& task);
	bool checkIfTaskCanBeStartedFromQueueAndStart();
};

}
