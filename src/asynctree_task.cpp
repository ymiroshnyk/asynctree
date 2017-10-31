#include "asynctree_task.h"
#include "asynctree_service.h"
#include "asynctree_mutex.h"

namespace AST
{

TaskImpl::TaskImpl(AccessKey<Task>, Task& task, Service& service, TaskImpl* parent, TaskWorkFunc workFunc, TaskCallbacks callbacks)
: next_(nullptr)
, mutex_(nullptr)
, task_(task)
, service_(service)
, parent_(parent)
, workFunc_(std::move(workFunc))
, callbacks_(std::move(callbacks))
, state_(S_Created)
, interrupted_(false)
, numChildrenToComplete_(0)
{
	for (auto& buffer : weightBuffers_)
	{
		buffer.firstChild_ = nullptr;
		buffer.lastChild_ = nullptr;
	}
}

TaskImpl::~TaskImpl()
{
	{
		// wait if any mutex lock was waiting before deleting
		std::lock_guard<std::mutex> lock(taskMutex_);
	}
}

Task& TaskImpl::task()
{
	return task_;
}

TaskImpl* TaskImpl::parent()
{
	return parent_;
}

void TaskImpl::exec(EnumTaskWeight weight)
{
	std::unique_lock<std::mutex> lock(taskMutex_);

	assert(state_ != S_Done);

	if (isInterrupted())
	{
		switch (state_)
		{
		case S_Created:
		{
			_interruptFromExec(lock);
			return;
		}

		case S_Working:
			// we will not start child task when parent is working and interrupted
			return;

		default: assert(state_ == S_WaitForChildren);
		{
			// interrupt not started tasks
			for (auto &buf : weightBuffers_)
			{
				for (TaskImpl* task = buf.firstChild_; task != nullptr;)
				{
					// cache next task, because task will be deleted
					TaskImpl* nextTask = task->next_;
					task->_interruptFromParent();
					task = nextTask;

					--numChildrenToComplete_;
				}

				buf.firstChild_ = buf.lastChild_ = nullptr;
			}

			if (numChildrenToComplete_ == 0)
			{
				_interruptFromExec(lock);
			}
			return;
		}
		}
	}

	if (state_ == S_Created)
	{
		state_ = S_Working;
		lock.unlock();

		service_._setCurrentTask(KEY, this);

		workFunc_();
		workFunc_ = TaskWorkFunc();

		lock.lock();

		state_ = S_WaitForChildren;

		if (numChildrenToComplete_ == 0)
		{
			_onFinished(lock);
		}
	}
	else
	{
		auto& buf = weightBuffers_[weight];

		assert(buf.firstChild_);
		assert(buf.lastChild_);

		TaskImpl& task = *buf.firstChild_;
		bool addToQueue;

		if (buf.firstChild_->next_)
		{
			buf.firstChild_ = buf.firstChild_->next_;

			addToQueue = true;
		}
		else
		{
			buf.firstChild_ = buf.lastChild_ = nullptr;

			addToQueue = false;
		}

		assert(task.state_ == S_Created);
		lock.unlock();

		if (addToQueue)
			service_._addToQueue(KEY, weight, *this);

		task.exec(weight);
	}
}

void TaskImpl::destroy()
{
	task_.selfLock_.reset();
}

void TaskImpl::addChildTask(EnumTaskWeight weight, TaskImpl& child)
{
	std::unique_lock<std::mutex> lock(taskMutex_);

	++numChildrenToComplete_;
	_addChildTaskNoIncCounter(weight, child, lock);
}

void TaskImpl::notifyDeferredTask()
{
	std::unique_lock<std::mutex> lock(taskMutex_);
	assert(state_ == S_Working || state_ == S_WaitForChildren);
	++numChildrenToComplete_;
}

void TaskImpl::addDeferredTask(EnumTaskWeight weight, TaskImpl& child)
{
	std::unique_lock<std::mutex> lock(taskMutex_);
	_addChildTaskNoIncCounter(weight, child, lock);
}

void TaskImpl::interrupt()
{
	interrupted_ = true;
}

bool TaskImpl::isInterrupted() const
{
	if (interrupted_)
		return true;

	if (parent_ && parent_->isInterrupted())
	{
		interrupted_ = true;
		return true;
	}

	return false;
}

void TaskImpl::_addChildTaskNoIncCounter(EnumTaskWeight weight, TaskImpl& child, std::unique_lock<std::mutex>& lock)
{
	assert(state_ == S_Working || state_ == S_WaitForChildren);
	assert(child.next_ == nullptr);

	auto& buf = weightBuffers_[weight];

	const bool addToQueue = !buf.firstChild_;

	if (buf.lastChild_)
	{
		assert(buf.firstChild_ != nullptr);
		assert(buf.lastChild_->next_ == nullptr);
		buf.lastChild_->next_ = &child;
	}
	else
	{
		assert(buf.firstChild_ == nullptr);
		buf.firstChild_ = &child;
	}

	buf.lastChild_ = &child;

	lock.unlock();

	if (addToQueue)
	{
		service_._addToQueue(KEY, weight, *this);
	}
}

void TaskImpl::_interruptFromExec(std::unique_lock<std::mutex>& lock)
{
	assert(state_ == S_Created || state_ == S_WaitForChildren);

	state_ = S_Done;

	service_._setCurrentTask(KEY, parent_);

	lock.unlock();

	if (callbacks_.interrupted_)
		callbacks_.interrupted_();
	else if (parent_)
		parent_->interrupt();

	if (callbacks_.finished_)
		callbacks_.finished_();

	if (mutex_)
		mutex_->_taskFinished(KEY);

	if (parent_)
	{
		parent_->_notifyChildFinished();
	}

	destroy();
}

void TaskImpl::_interruptFromParent()
{
	std::unique_lock<std::mutex> lock(taskMutex_);
	assert(state_ == S_Created);

	state_ = S_Done;

	service_._setCurrentTask(KEY, parent_);

	lock.unlock();

	if (callbacks_.interrupted_)
		callbacks_.interrupted_();

	if (callbacks_.finished_)
		callbacks_.finished_();

	destroy();
}

void TaskImpl::_onFinished(std::unique_lock<std::mutex>& lock)
{
	assert(state_ != S_Done);

	state_ = S_Done;

	service_._setCurrentTask(KEY, parent_);

	lock.unlock();

	if (isInterrupted())
	{
		if (callbacks_.interrupted_)
			callbacks_.interrupted_();
		else if (parent_)
			parent_->interrupt();
	}
	else
	{
		if (callbacks_.succeeded_)
			callbacks_.succeeded_();
	}

	if (callbacks_.finished_)
		callbacks_.finished_();

	if (mutex_)
		mutex_->_taskFinished(KEY);

	if (parent_)
	{
		parent_->_notifyChildFinished();
	}

	destroy();
}

void TaskImpl::_notifyChildFinished()
{
	std::unique_lock<std::mutex> lock(taskMutex_);

	assert(state_ != S_Created);
	assert(state_ != S_Done);
	assert(numChildrenToComplete_ > 0);

	--numChildrenToComplete_;

	if (state_ == S_WaitForChildren)
	{
		if (numChildrenToComplete_ == 0)
		{
			_onFinished(lock);
		}
	}
}

Task::Task(Private, Service& service, TaskImpl* parent, TaskWorkFunc workFunc, TaskCallbacks callbacks)
: impl_(KEY, *this, service, parent, std::move(workFunc), std::move(callbacks))
{

}

Task::~Task()
{
	assert(!selfLock_);
}

TaskP Task::_create(AccessKey<Service, Mutex>, Service& service, TaskImpl* parent, TaskWorkFunc workFunc, TaskCallbacks callbacks)
{
	auto task = std::make_shared<Task>(Private(), service, parent, std::move(workFunc), std::move(callbacks));
	task->selfLock_ = task;
	return std::move(task);
}

TaskImpl& Task::_impl(AccessKey<Service, Mutex>)
{
	return impl_;
}

void Task::interrupt()
{
	impl_.interrupt();
}

bool Task::isInterrupted() const
{
	return impl_.isInterrupted();
}

}

