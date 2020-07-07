#pragma once

#include "asynctree_config.h"
#include "asynctree_task_typedefs.h"
#include "asynctree_access_key.h"

#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>

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

	TaskImpl* firstWorkerTask_ = nullptr;
	TaskImpl* lastWorkerTask_ = nullptr;
	uint numWorkingTasks_ = 0;

	bool shuttingDown_ = false;

	std::condition_variable doneCV_;

	Service(const Service&) = delete;
	Service& operator=(const Service&) = delete;

public:
	Service(const uint numThreads = std::thread::hardware_concurrency());
	~Service();

	template <typename TaskWorkFunc, typename T1 = StaticCallback<void>, typename T2 = StaticCallback<void>, typename T3 = StaticCallback<void>>
	inline Task& task(EnumTaskWeight weight, TaskWorkFunc workFunc,
		T1 t1 = T1(), T2 t2 = T2(), T3 t3 = T3());

	template <typename TaskWorkFunc, typename T1 = StaticCallback<void>, typename T2 = StaticCallback<void>, typename T3 = StaticCallback<void>>
	inline Task& topmostTask(EnumTaskWeight weight, TaskWorkFunc workFunc,
		T1 t1 = T1(), T2 t2 = T2(), T3 t3 = T3());

	void waitUtilEverythingIsDone();
	static Task* currentTask();

	void _startTask(AccessKey<TaskImpl>, TaskImpl& taskImpl);
	void _addToQueue(AccessKey<Service, Mutex, TaskImpl>, TaskImpl& task);
	void _setCurrentTask(AccessKey<TaskImpl>, TaskImpl* task);

private:
	uint _syncWorkersQueue();
	void _moveTaskToWorkers(EnumTaskWeight weight);
	void _workerFunc();
};

template <typename TaskWorkFunc, typename T1, typename T2, typename T3>
inline Task& Service::task(EnumTaskWeight weight, TaskWorkFunc workFunc, T1 t1, T2 t2, T3 t3)
{
	return *TaskTyped<TaskWorkFunc, T1, T2, T3>::_create(KEY, *this, currentTask_, weight, std::move(workFunc), std::move(t1), std::move(t2), std::move(t3));
}

template <typename TaskWorkFunc, typename T1, typename T2, typename T3>
inline Task& Service::topmostTask(EnumTaskWeight weight, TaskWorkFunc workFunc, T1 t1, T2 t2, T3 t3)
{
	return *TaskTyped<TaskWorkFunc, T1, T2, T3>::_create(KEY, *this, nullptr, weight, std::move(workFunc), std::move(t1), std::move(t2), std::move(t3));
}

}
