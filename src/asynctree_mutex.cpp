#include "asynctree_mutex.h"
#include "asynctree_task.h"
#include "asynctree_service.h"

namespace ast
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

void Mutex::_taskFinished(AccessKey<TaskImpl>)
{
	std::unique_lock<std::mutex> lock(mutex_);

	--numTasksToBeFinished_;

	while (_checkIfTaskCanBeStartedFromQueueAndStart()) {}

	lock.unlock();
	destroyCV_.notify_one();
}

TaskP Mutex::_startTask(bool shared, EnumTaskWeight weight, TaskWorkFunc workFunc, TaskCallbacks callbacks)
{
	auto task = Task::_create(KEY, service_, nullptr, std::move(workFunc), std::move(callbacks));
	auto& taskImpl = task->_impl(KEY);
	taskImpl.shared_ = shared;
	taskImpl.weight_ = weight;
	taskImpl.mutex_ = this;

	std::unique_lock<std::mutex> lock(mutex_);

	if (_checkIfTaskCanBeStartedAndIncCounters(shared))
	{
		lock.unlock();
		service_._addToQueue(KEY, weight, taskImpl);
	}
	else
	{
		_queueTask(taskImpl);
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
	auto parentImpl = parent ? &parent->_impl(KEY) : nullptr;
	auto task = Task::_create(KEY, service_, parentImpl, std::move(workFunc), std::move(callbacks));
	auto& taskImpl = task->_impl(KEY);
	taskImpl.shared_ = shared;
	taskImpl.weight_ = weight;
	taskImpl.mutex_ = this;

	std::unique_lock<std::mutex> lock(mutex_);

	if (_checkIfTaskCanBeStartedAndIncCounters(shared))
	{
		lock.unlock();
		parentImpl->addChildTask(weight, taskImpl);
	}
	else
	{
		parentImpl->notifyDeferredTask();
		_queueTask(taskImpl);
		lock.unlock();
	}

	return std::move(task);
}

bool Mutex::_checkIfTaskCanBeStartedAndIncCounters(bool shared)
{
	if (numTasksToBeFinished_ == 0 || (shared && sharedTasksInProgress_ && !firstQueuedChild_))
	{
		++numTasksToBeFinished_;
		sharedTasksInProgress_ = shared;
		return true;
	}

	return false;
}

void Mutex::_queueTask(TaskImpl& task)
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

bool Mutex::_checkIfTaskCanBeStartedFromQueueAndStart()
{
	if (firstQueuedChild_)
	{
		TaskImpl* task = firstQueuedChild_;

		if (numTasksToBeFinished_ == 0 || (task->shared_ && sharedTasksInProgress_))
		{
			firstQueuedChild_ = task->next_;

			if (!firstQueuedChild_)
				lastQueuedChild_ = nullptr;

			sharedTasksInProgress_ = task->shared_;

			++numTasksToBeFinished_;

			if (TaskImpl* parent = task->parent())
				parent->addDeferredTask(task->weight_, *task);
			else
				service_._addToQueue(KEY, task->weight_, *task);

			return true;
		}
	}

	return false;
}

}

