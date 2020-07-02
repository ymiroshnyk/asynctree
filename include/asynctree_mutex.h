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

	Task& rootTask(EnumTaskWeight weight, TaskWorkFunc workFunc);
	Task& task(EnumTaskWeight weight, TaskWorkFunc workFunc);
	Task& childTask(EnumTaskWeight weight, TaskWorkFunc workFunc);

	Task& sharedRootTask(EnumTaskWeight weight, TaskWorkFunc workFunc);
	Task& sharedTask(EnumTaskWeight weight, TaskWorkFunc workFunc);
	Task& sharedChildTask(EnumTaskWeight weight, TaskWorkFunc workFunc);

	void _startTask(AccessKey<TaskImpl>, TaskImpl& taskImpl);
	void _taskFinished(AccessKey<TaskImpl>);

private:
	TaskP _rootTask(bool shared, EnumTaskWeight weight, TaskWorkFunc workFunc);
	TaskP _task(bool shared, EnumTaskWeight weight, TaskWorkFunc workFunc);
	TaskP _childTask(bool shared, Task* parent, EnumTaskWeight weight, TaskWorkFunc workFunc);
	bool _checkIfTaskCanBeStartedAndIncCounters(bool shared);
	void _queueTask(TaskImpl& task);
	bool _checkIfTaskCanBeStartedFromQueueAndStart();
};

}
