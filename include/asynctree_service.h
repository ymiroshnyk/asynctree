#pragma once

#include "asynctree_config.h"
#include "asynctree_task_callbacks.h"
#include "asynctree_task_typedefs.h"
#include "asynctree_access_key.h"

namespace ast
{

class Mutex;
class TaskImpl;

class Service
{
	const uint numThreads_;

	struct WeightQueue
	{
		uint mask_;
		uint overloadWorkersLimit_;
		uint numActiveWorkers_;

		TaskImpl* firstInQueue_;
		TaskImpl* lastInQueue_;

#ifdef ASYNCTREE_DEBUG
		uint numTasksFinished_;
#endif
	};

	std::mutex mutex_;

	WeightQueue queues_[TW_Quantity];

	static thread_local TaskImpl* currentTask_;
	std::vector<std::thread> workers_;
	std::condition_variable workersCV_;

	TaskImpl* firstWorkerTask_;
	TaskImpl* lastWorkerTask_;
	uint numWorkingTasks_;

	bool shuttingDown_;

	std::condition_variable doneCV_;

	Service(const Service&) = delete;
	Service& operator=(const Service&) = delete;

public:
	Service(const uint numThreads = std::thread::hardware_concurrency());
	~Service();

	Task& task(EnumTaskWeight weight, TaskWorkFunc workFunc);
	Task& topmostTask(EnumTaskWeight weight, TaskWorkFunc workFunc);
	Task& childTask(EnumTaskWeight weight, TaskWorkFunc workFunc);

	void waitUtilEverythingIsDone();
	Task* currentTask();

	void _startTask(AccessKey<TaskImpl>, TaskImpl& taskImpl);
	void _addToQueue(AccessKey<Service, Mutex, TaskImpl>, EnumTaskWeight weight, TaskImpl& task);
	void _setCurrentTask(AccessKey<TaskImpl>, TaskImpl* task);

private:
	uint _syncWorkersQueue();
	void _moveTaskToWorkers(EnumTaskWeight weight);
	void _workerFunc();
};

}
