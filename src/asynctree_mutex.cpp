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

Task& Mutex::rootTask(EnumTaskWeight weight, TaskWorkFunc workFunc)
{
	return *_rootTask(false, weight, std::move(workFunc));
}

Task& Mutex::task(EnumTaskWeight weight, TaskWorkFunc workFunc)
{
	return *_task(false, weight, std::move(workFunc));
}

Task& Mutex::childTask(EnumTaskWeight weight, TaskWorkFunc workFunc)
{
	Task* parent = service_.currentTask();
	assert(parent);
	return *_childTask(false, parent, weight, std::move(workFunc));
}

Task& Mutex::sharedRootTask(EnumTaskWeight weight, TaskWorkFunc workFunc)
{
	return *_rootTask(true, weight, std::move(workFunc));
}

Task& Mutex::sharedTask(EnumTaskWeight weight, TaskWorkFunc workFunc)
{
	return *_task(true, weight, std::move(workFunc));
}

Task& Mutex::sharedChildTask(EnumTaskWeight weight, TaskWorkFunc workFunc)
{
	Task* parent = service_.currentTask();
	assert(parent);
	return *_childTask(true, parent, weight, std::move(workFunc));
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
			parentImpl->addChildTask(taskImpl.weight_, taskImpl);
		}
		else
		{
			service_._addToQueue(KEY, taskImpl.weight_, taskImpl);
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

TaskP Mutex::_rootTask(bool shared, EnumTaskWeight weight, TaskWorkFunc workFunc)
{
	auto task = Task::_create(KEY, service_, nullptr, weight, std::move(workFunc));
	auto& taskImpl = task->_impl(KEY);
	taskImpl.shared_ = shared;
	taskImpl.mutex_ = this;

	return std::move(task);
}

TaskP Mutex::_task(bool shared, EnumTaskWeight weight, TaskWorkFunc workFunc)
{
	if (auto parent = service_.currentTask())
		return _childTask(shared, parent, weight, std::move(workFunc));
	else
		return _rootTask(shared, weight, std::move(workFunc));
}

TaskP Mutex::_childTask(bool shared, Task* parent, EnumTaskWeight weight, TaskWorkFunc workFunc)
{
	auto parentImpl = parent ? &parent->_impl(KEY) : nullptr;
	auto task = Task::_create(KEY, service_, parentImpl, weight, std::move(workFunc));
	auto& taskImpl = task->_impl(KEY);
	taskImpl.shared_ = shared;
	taskImpl.mutex_ = this;

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

