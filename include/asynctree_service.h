#pragma once

#include "asynctree_config.h"
#include "asynctree_task_callbacks.h"
#include "asynctree_task_typedefs.h"

namespace AST
{

class Task;

class Service : boost::noncopyable
{
	const uint numThreads_;

	struct WorkerData
	{
		Task* currentTask_;
	};

	struct WeightQueue
	{
		uint mask_;
		uint overloadWorkersLimit_;
		uint numActiveWorkers_;

		Task* firstInQueue_;
		Task* lastInQueue_;

#ifdef _DEBUG
		uint numTasksFinished_;
#endif
	};

	std::mutex mutex_;

	WeightQueue queues_[TW_Quantity];

	boost::thread_specific_ptr<WorkerData> workerData_;
	std::vector<std::thread> workers_;
	std::condition_variable workersCV_;

	Task* firstWorkerTask_;
	Task* lastWorkerTask_;
	uint numWorkingTasks_;

	bool shuttingDown_;

	std::condition_variable doneCV_;

public:
	Service(const uint numThreads = std::thread::hardware_concurrency());
	~Service();

	TaskP startTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks = TaskCallbacks());
	TaskP startAutoTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks = TaskCallbacks());
	TaskP startChildTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks = TaskCallbacks());

	void addToQueue(EnumTaskWeight weight, Task& task);
	void setCurrentTask(Task* task);

	Task* currentTask();

	void waitUtilEverythingIsDone();

private:
	uint syncWorkersQueue();
	void moveTaskToWorkers(EnumTaskWeight weight);
	void workerFunc();
};

}
