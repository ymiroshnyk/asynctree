#include "asynctree_service.h"
#include "asynctree_task.h"

namespace ast
{

Service::Service(const uint numThreads)
	: shuttingDown_(false)
	, numThreads_(numThreads)
{
	for (uint weight = 0; weight < TW_Quantity; ++weight)
	{
		auto& queue = queues_[weight];

		queue.mask_ = 1 << weight;
		const uint desiredLimit = uint(float(numThreads) / float(TW_Quantity + 1) * float(TW_Quantity - weight));
		queue.overloadWorkersLimit_ = desiredLimit >= 1 ? desiredLimit : 1;

		queue.firstInQueue_ = nullptr;
		queue.lastInQueue_ = nullptr;
		queue.numActiveWorkers_ = 0;

#ifdef ASYNCTREE_DEBUG
		queue.numTasksFinished_ = 0;
#endif
	}

	const uint numWorkers = numThreads * TW_Quantity;

	for (uint i = 0; i < numWorkers; ++i)
		workers_.push_back(std::thread([&]() { _workerFunc(); }));
}

Service::~Service()
{
	shuttingDown_ = true;

	workersCV_.notify_all();
	for (auto& worker : workers_)
		worker.join();

	for (auto& queue : queues_)
	{
		for (TaskImpl* task = queue.firstInQueue_; task;)
		{
			TaskImpl* temp = task;
			task = task->next_;
			temp->destroy();
		}
	}

	for (TaskImpl* task = firstWorkerTask_; task;)
	{
		TaskImpl* temp = task;
		task = task->next_;
		temp->destroy();
	}
}

Task& Service::task(EnumTaskWeight weight, TaskWorkFunc workFunc)
{
	auto workerData = workerData_.get();

	if (workerData && workerData->currentTask_)
		return childTask(weight, std::move(workFunc));
	else
		return topmostTask(weight, std::move(workFunc));
}

Task& Service::topmostTask(EnumTaskWeight weight, TaskWorkFunc workFunc)
{
	return *Task::_create(KEY, *this, nullptr, weight, std::move(workFunc));
}

Task& Service::childTask(EnumTaskWeight weight, TaskWorkFunc workFunc)
{
	auto workerData = workerData_.get();
	assert(workerData);

	TaskImpl* parentTask = workerData->currentTask_;
	return *Task::_create(KEY, *this, parentTask, weight, std::move(workFunc));
}

void Service::_startTask(AccessKey<TaskImpl>, TaskImpl& taskImpl)
{
	if (auto parentTask = taskImpl.parent())
	{
		parentTask->addChildTask(taskImpl.weight_, taskImpl);
	}
	else
	{
		_addToQueue(KEY, taskImpl.weight_, taskImpl);
	}
}

void Service::_addToQueue(AccessKey<Service, Mutex, TaskImpl>, EnumTaskWeight weight, TaskImpl& task)
{
	auto& queue = queues_[weight];

	std::unique_lock<std::mutex> lock(mutex_);

	if (queue.lastInQueue_)
	{
		assert(queue.firstInQueue_ != nullptr);
		queue.lastInQueue_->next_ = &task;
		task.next_ = nullptr;
		queue.lastInQueue_ = &task;
	}
	else
	{
		assert(queue.firstInQueue_ == nullptr);
		task.next_ = nullptr;
		queue.firstInQueue_ = queue.lastInQueue_ = &task;
	}

	uint numToNotify = _syncWorkersQueue();

	lock.unlock();

	while (numToNotify--)
		workersCV_.notify_one();
}

void Service::_setCurrentTask(AccessKey<TaskImpl>, TaskImpl* task)
{
	assert(workerData_.get());
	workerData_->currentTask_ = task;
}

Task* Service::currentTask()
{
	if (auto workerData = workerData_.get())
		if (auto taskImpl = workerData->currentTask_)
		{
			return &taskImpl->task();
		}

	return nullptr;
}

void Service::waitUtilEverythingIsDone()
{
	std::unique_lock<std::mutex> lock(mutex_);

	if (numWorkingTasks_ > 0)
		doneCV_.wait(lock);
}

uint Service::_syncWorkersQueue()
{
	uint numToNotify = 0;

	for (;;)
	{
		const bool overloaded = numWorkingTasks_ >= numThreads_;

		uint activeTasksMask = 0;
		for (auto& queue : queues_)
		{
			if (queue.firstInQueue_ && (!overloaded || queue.numActiveWorkers_ < queue.overloadWorkersLimit_))
				activeTasksMask |= queue.mask_;
		}

#ifdef ASYNCTREE_DEBUG
		static uint maskCounter[8] = { 0 };

		++maskCounter[activeTasksMask];
#endif

		if (!activeTasksMask)
		{
			break;
		}

		static float limitsLight = (float)queues_[Light].overloadWorkersLimit_;
		static float limitsMiddle = (float)queues_[Middle].overloadWorkersLimit_;
		static float limitsHeavy = (float)queues_[Heavy].overloadWorkersLimit_;

		auto selectAndMoveTask = [&](float limits0, float limits1, float activeWorkers0, float activeWorkers1,
			EnumTaskWeight weight0, EnumTaskWeight weight1)
		{
			const float currNormDeltaWeight0 = std::fabs(1.f - activeWorkers0 / limits0);
			const float currNormDeltaWeight1 = std::fabs(1.f - activeWorkers1 / limits1);

			const float newNormDeltaWeight0 = std::fabs(1.f - activeWorkers0 / limits0);
			const float newNormDeltaWeight1 = std::fabs(1.f - activeWorkers1 / limits1);

			const float weightDeviat0 = newNormDeltaWeight0 + currNormDeltaWeight1;
			const float weightDeviat1 = currNormDeltaWeight0 + newNormDeltaWeight1;

			if (weightDeviat0 <= weightDeviat1)
				_moveTaskToWorkers(weight0);
			else
				_moveTaskToWorkers(weight1);
		};

		switch (activeTasksMask)
		{
		case 1: _moveTaskToWorkers(Light); break;
		case 2: _moveTaskToWorkers(Middle); break;
		case 4: _moveTaskToWorkers(Heavy); break;
		case 7:
		{
			const float activeWorkersLight = (float)queues_[Light].numActiveWorkers_;
			const float activeWorkersMiddle = (float)queues_[Middle].numActiveWorkers_;
			const float activeWorkersHeavy = (float)queues_[Heavy].numActiveWorkers_;

			const float currNormDeltaWeightLight = fabs(1.f - activeWorkersLight / limitsLight);
			const float currNormDeltaWeightMiddle = fabs(1.f - activeWorkersMiddle / limitsMiddle);
			const float currNormDeltaWeightHeavy = fabs(1.f - activeWorkersHeavy / limitsHeavy);

			const float newNormDeltaWeightLight = fabs(1.f - (activeWorkersLight + 1) / limitsLight);
			const float newNormDeltaWeightMiddle = fabs(1.f - (activeWorkersMiddle + 1) / limitsMiddle);
			const float newNormDeltaWeightHeavy = fabs(1.f - (activeWorkersHeavy + 1) / limitsHeavy);

			const float sumWeightDeviatLight = newNormDeltaWeightLight + currNormDeltaWeightMiddle + currNormDeltaWeightHeavy;
			const float sumWeightDeviatMiddle = currNormDeltaWeightLight + newNormDeltaWeightMiddle + currNormDeltaWeightHeavy;
			const float sumWeightDeviatHeavy = currNormDeltaWeightLight + currNormDeltaWeightMiddle + newNormDeltaWeightHeavy;

			if (sumWeightDeviatLight <= sumWeightDeviatMiddle)
			{
				if (sumWeightDeviatLight <= sumWeightDeviatHeavy)
					_moveTaskToWorkers(Light);
				else
					_moveTaskToWorkers(Heavy);
			}
			else
			{
				if (sumWeightDeviatMiddle <= sumWeightDeviatHeavy)
					_moveTaskToWorkers(Middle);
				else
					_moveTaskToWorkers(Heavy);
			}

			break;
		}
		case 3: selectAndMoveTask(limitsLight, limitsMiddle, (float)queues_[Light].numActiveWorkers_,
			(float)queues_[Middle].numActiveWorkers_, Light, Middle); break;

		case 5: selectAndMoveTask(limitsLight, limitsHeavy, (float)queues_[Light].numActiveWorkers_,
			(float)queues_[Heavy].numActiveWorkers_, Light, Heavy); break;

		case 6: selectAndMoveTask(limitsMiddle, limitsHeavy, (float)queues_[Middle].numActiveWorkers_,
			(float)queues_[Heavy].numActiveWorkers_, Middle, Heavy); break;
		}

		++numToNotify;
	}

	return numToNotify;
}

void Service::_moveTaskToWorkers(EnumTaskWeight weight)
{
	auto& queue = queues_[weight];

	TaskImpl* task = queue.firstInQueue_;
	assert(task);

	queue.firstInQueue_ = task->next_;

	if (!queue.firstInQueue_)
	{
		queue.lastInQueue_ = nullptr;
	}

	++queue.numActiveWorkers_;

	task->weight_ = weight;

	if (lastWorkerTask_)
	{
		assert(firstWorkerTask_);

		lastWorkerTask_->next_ = task;
		task->next_ = nullptr;
		lastWorkerTask_ = task;
	}
	else
	{
		assert(!firstWorkerTask_);

		task->next_ = nullptr;
		lastWorkerTask_ = firstWorkerTask_ = task;
	}

	++numWorkingTasks_;
}

void Service::_workerFunc()
{
	workerData_.reset(new WorkerData());

	std::unique_lock<std::mutex> lock(mutex_);

	while (!shuttingDown_)
	{
		int numToNotify = (int)_syncWorkersQueue();

		if (!firstWorkerTask_)
		{
			numToNotify = 0;

			if (numWorkingTasks_ == 0)
				doneCV_.notify_all();

			workersCV_.wait(lock);
		}

		// if thread is woke up by shutting down
		if (shuttingDown_)
			break;

		if (firstWorkerTask_)
		{
			TaskImpl* task = firstWorkerTask_;
			firstWorkerTask_ = task->next_;

			if (!firstWorkerTask_)
			{
				lastWorkerTask_ = nullptr;
			}

			lock.unlock();

			while (--numToNotify > 0)
				workersCV_.notify_one();

			const EnumTaskWeight weight = task->weight_;
			task->exec(weight);

			lock.lock();

			--numWorkingTasks_;
			assert(numWorkingTasks_ != (uint)-1);
			--queues_[weight].numActiveWorkers_;
			assert(queues_[weight].numActiveWorkers_ != (uint)-1);
#ifdef ASYNCTREE_DEBUG
			++queues_[weight].numTasksFinished_;
#endif
		}
	}
}

}
