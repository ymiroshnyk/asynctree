#include "asynctree_mutex.h"
#include "asynctree_task.h"
#include "asynctree_service.h"

#include <cassert>

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

void Mutex::_startTask(AccessKey<TaskImpl>, TaskImpl& taskImpl)
{
	std::unique_lock<std::mutex> lock(mutex_);

	auto* parentImpl = taskImpl.parent();

	if (_checkIfTaskCanBeStartedAndIncCounters(taskImpl.shared_))
	{
		lock.unlock();

		if (parentImpl)
		{
			parentImpl->addChildTask(taskImpl);
		}
		else
		{
			service_._addToQueue(KEY, taskImpl);
		}
	}
	else
	{
		if (parentImpl)
		{
			_queueTask(taskImpl);
		}
		else
		{
			parentImpl->notifyDeferredTask();
			_queueTask(taskImpl);
		}

		lock.unlock();
	}
}

void Mutex::_taskFinished(AccessKey<TaskImpl>)
{
	std::unique_lock<std::mutex> lock(mutex_);

	--numTasksToBeFinished_;

	while (_checkIfTaskCanBeStartedFromQueueAndStart()) {}

	lock.unlock();
	destroyCV_.notify_one();
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
				parent->addDeferredTask(*task);
			else
				service_._addToQueue(KEY, *task);

			return true;
		}
	}

	return false;
}

}

