#include "asynctree_mutex.h"
#include "asynctree_task.h"
#include "asynctree_service.h"

namespace AST
{

Mutex::Mutex(Service& service)
: service_(service)
, firstQueuedChild_(nullptr)
, lastQueuedChild_(nullptr)
, sharedTasksInProgress_(false)
, numTasksToBeFinished_(0)
{

}

Mutex::~Mutex()
{
	std::unique_lock<std::mutex> lock(mutex_);

	while (numTasksToBeFinished_)
	{
		destroyCV_.wait(lock);
	}
}

TaskP Mutex::startTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks)
{
	return _startTask(false, weight, std::move(workFunc), std::move(callbacks));
}

TaskP Mutex::startAutoTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks)
{
	return _startAutoTask(false, weight, std::move(workFunc), std::move(callbacks));
}

TaskP Mutex::startChildTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks)
{
	Task* parent = service_.currentTask();
	assert(parent);
	return _startChildTask(false, parent, weight, std::move(workFunc), std::move(callbacks));
}

TaskP Mutex::startSharedTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks)
{
	return _startTask(true, weight, std::move(workFunc), std::move(callbacks));
}

TaskP Mutex::startSharedAutoTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks)
{
	return _startAutoTask(true, weight, std::move(workFunc), std::move(callbacks));
}

TaskP Mutex::startSharedChildTask(EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks)
{
	Task* parent = service_.currentTask();
	assert(parent);
	return _startChildTask(true, parent, weight, std::move(workFunc), std::move(callbacks));
}

void Mutex::taskFinished()
{
	std::unique_lock<std::mutex> lock(mutex_);

	--numTasksToBeFinished_;

	while (checkIfTaskCanBeStartedFromQueueAndStart()) {}

	lock.unlock();
	destroyCV_.notify_one();
}

TaskP Mutex::_startTask(bool shared, EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks)
{
	auto task = Task::create(service_, nullptr, std::move(workFunc), std::move(callbacks));
	task->shared_ = shared;
	task->weight_ = weight;
	task->mutex_ = this;

	std::unique_lock<std::mutex> lock(mutex_);

	if (checkIfTaskCanBeStartedAndIncCounters(shared))
	{
		lock.unlock();
		service_.addToQueue(weight, *task);
	}
	else
	{
		queueTask(*task);
		lock.unlock();
	}

	return std::move(task);
}

TaskP Mutex::_startAutoTask(bool shared, EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks)
{
	if (auto parent = service_.currentTask())
		return _startChildTask(shared, parent, weight, std::move(workFunc), std::move(callbacks));
	else
		return _startTask(shared, weight, std::move(workFunc), std::move(callbacks));
}

TaskP Mutex::_startChildTask(bool shared, Task* parent, EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks)
{
	auto task = Task::create(service_, parent, std::move(workFunc), std::move(callbacks));
	task->shared_ = shared;
	task->weight_ = weight;
	task->mutex_ = this;

	std::unique_lock<std::mutex> lock(mutex_);

	if (checkIfTaskCanBeStartedAndIncCounters(shared))
	{
		lock.unlock();
		parent->addChildTask(weight, *task);
	}
	else
	{
		parent->notifyDeferredTask();
		queueTask(*task);
		lock.unlock();
	}

	return std::move(task);
}

bool Mutex::checkIfTaskCanBeStartedAndIncCounters(bool shared)
{
	if (numTasksToBeFinished_ == 0 || (shared && sharedTasksInProgress_ && !firstQueuedChild_))
	{
		++numTasksToBeFinished_;
		sharedTasksInProgress_ = shared;
		return true;
	}

	return false;
}

void Mutex::queueTask(Task& task)
{
	if (lastQueuedChild_)
	{
		assert(firstQueuedChild_);
		lastQueuedChild_->next_ = &task;
		task.next_ = nullptr;
		lastQueuedChild_ = &task;
	}
	else
	{
		assert(!firstQueuedChild_);
		lastQueuedChild_ = firstQueuedChild_ = &task;
		task.next_ = nullptr;
	}
}

bool Mutex::checkIfTaskCanBeStartedFromQueueAndStart()
{
	if (firstQueuedChild_)
	{
		Task* task = firstQueuedChild_;

		if (numTasksToBeFinished_ == 0 || (task->shared_ && sharedTasksInProgress_))
		{
			firstQueuedChild_ = task->next_;

			if (!firstQueuedChild_)
				lastQueuedChild_ = nullptr;

			sharedTasksInProgress_ = task->shared_;

			++numTasksToBeFinished_;

			if (Task* parent = task->parent())
				parent->addDeferredTask(task->weight_, *task);
			else
				service_.addToQueue(task->weight_, *task);

			return true;
		}
	}

	return false;
}

}

