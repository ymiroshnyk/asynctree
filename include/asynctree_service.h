#pragma once

#include "asynctree_config.h"
#include "asynctree_task_callbacks.h"
#include "asynctree_task_typedefs.h"
#include "asynctree_access_key.h"

namespace AST
{

class Mutex;
class TaskImpl;

class Service : boost::noncopyable
{
	const uint numThreads_;

	struct WorkerData
	{
		TaskImpl* currentTask_;
	};

	struct WeightQueue
	{
		uint mask_;
		uint overloadWorkersLimit_;
		uint numActiveWorkers_;

		TaskImpl* firstInQueue_;
		TaskImpl* lastInQueue_;

#ifdef _DEBUG
		uint numTasksFinished_;
#endif
	};

	std::mutex mutex_;

	WeightQueue queues_[TW_Quantity];

	boost::thread_specific_ptr<WorkerData> workerData_;
	std::vector<std::thread> workers_;
	std::condition_variable workersCV_;

	TaskImpl* firstWorkerTask_;
	TaskImpl* lastWorkerTask_;
	uint numWorkingTasks_;

	bool shuttingDown_;

	std::condition_variable doneCV_;

public:
	Service(const uint numThreads = std::thread::hardware_concurrency());
	~Service();

	TaskP startTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks = TaskCallbacks());
	TaskP startAutoTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks = TaskCallbacks());
	TaskP startChildTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks = TaskCallbacks());

	void waitUtilEverythingIsDone();
	Task* currentTask();

	void _addToQueue(AccessKey<Service, Mutex, TaskImpl>, EnumTaskWeight weight, TaskImpl& task);
	void _setCurrentTask(AccessKey<TaskImpl>, TaskImpl* task);

private:
	uint _syncWorkersQueue();
	void _moveTaskToWorkers(EnumTaskWeight weight);
	void _workerFunc();
};

}
