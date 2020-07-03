#include "asynctree_task.h"
#include "asynctree_service.h"
#include "asynctree_mutex.h"

#include <atomic>
#include <algorithm>
#include <cassert>

namespace ast
{
namespace 
{

}

TaskImpl::TaskImpl(AccessKey<Task>, Task& task, Service& service, TaskImpl* parent, 
	EnumTaskWeight weight)
: next_(nullptr)
, weight_(weight)
, mutex_(nullptr)
, task_(task)
, service_(service)
, parent_(parent)
, state_(State::Created)
, interrupted_(false)
, numChildrenToComplete_(0)
{
}

TaskImpl::~TaskImpl()
{
}

void TaskImpl::exec()
{
	if (parent_)
		parent_->_onChildExec(weight_);

	std::unique_lock<std::mutex> lock(taskMutex_);

	assert(state_ == State::Created);

	if (isInterrupted())
	{
		_onFinished(lock);
		return;
	}

	state_ = State::Working;
	lock.unlock();

	service_._setCurrentTask(KEY, this);

	task_._execWorkFunc();

	lock.lock();

	state_ = State::WaitForChildren;

	if (numChildrenToComplete_ == 0)
	{
		_onFinished(lock);
	}
}

void TaskImpl::destroy()
{
	task_.selfLock_.reset();
}

void TaskImpl::addChildTask(TaskImpl& child)
{
	std::unique_lock<std::mutex> lock(taskMutex_);

	++numChildrenToComplete_;
	_addChildTaskNoIncCounter(child, lock);
}

void TaskImpl::notifyDeferredTask()
{
	std::unique_lock<std::mutex> lock(taskMutex_);
	assert(state_ == State::Working || state_ == State::WaitForChildren);
	++numChildrenToComplete_;
}

void TaskImpl::addDeferredTask(TaskImpl& child)
{
	std::unique_lock<std::mutex> lock(taskMutex_);
	_addChildTaskNoIncCounter(child, lock);
}

void TaskImpl::start()
{
	if (mutex_) {
		mutex_->_startTask(KEY, *this);
	}
	else
	{
		service_._startTask(KEY, *this);
	}
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

void TaskImpl::_addChildTaskNoIncCounter(TaskImpl& child, std::unique_lock<std::mutex>& lock)
{
	assert(state_ == State::Working || state_ == State::WaitForChildren);
	assert(child.next_ == nullptr);

	auto& buf = weightBuffers_[child.weight()];

	if (!buf.firstChild_)
	{
		lock.unlock();
		service_._addToQueue(KEY, child);
		return;
	}

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
}

void TaskImpl::_interruptWaitingTaskFromParent()
{
	std::unique_lock<std::mutex> lock(taskMutex_);
	assert(state_ == State::Created);

	state_ = State::Done;

	service_._setCurrentTask(KEY, parent_);

	lock.unlock();

	task_._execCallback(CallbackType::Interrupted);
	task_._execCallback(CallbackType::Finished);

	destroy();
}

void TaskImpl::_onFinished(std::unique_lock<std::mutex>& lock)
{
	assert(state_ != State::Done);

	state_ = State::Done;

	service_._setCurrentTask(KEY, parent_);

	lock.unlock();

	if (isInterrupted())
		task_._execCallback(CallbackType::Interrupted);
	else
		task_._execCallback(CallbackType::Succeeded);

	task_._execCallback(CallbackType::Finished);

	if (mutex_)
		mutex_->_taskFinished(KEY);

	if (parent_)
		parent_->_onChildFinished();

	destroy();
}

void TaskImpl::_onChildExec(EnumTaskWeight weight)
{
	std::unique_lock<std::mutex> lock(taskMutex_);

	if (isInterrupted())
	{
		for (auto& buf : weightBuffers_)
		{
			for (TaskImpl* task = buf.firstChild_; task != nullptr;)
			{
				// cache next task, because task will be deleted
				TaskImpl* nextTask = task->next_;
				task->_interruptWaitingTaskFromParent();
				task = nextTask;

				--numChildrenToComplete_;
			}

			buf.firstChild_ = buf.lastChild_ = nullptr;
		}

		// We have at least one task executing
		assert(numChildrenToComplete_ > 0);

		return;
	}

	auto& buf = weightBuffers_[weight];

	if (!buf.firstChild_) return;

	TaskImpl& task = *buf.firstChild_;

	if (buf.firstChild_->next_)
	{
		buf.firstChild_ = buf.firstChild_->next_;
	}
	else
	{
		buf.firstChild_ = buf.lastChild_ = nullptr;
	}

	lock.unlock();

	service_._addToQueue(KEY, task);
}

void TaskImpl::_onChildFinished()
{
	std::unique_lock<std::mutex> lock(taskMutex_);

	assert(state_ != State::Created);
	assert(state_ != State::Done);
	assert(numChildrenToComplete_ > 0);

	--numChildrenToComplete_;

	if (state_ == State::WaitForChildren)
	{
		if (numChildrenToComplete_ == 0)
		{
			_onFinished(lock);
		}
	}
}

Task::Task(Service& service, TaskImpl* parent, EnumTaskWeight weight)
: impl_(KEY, *this, service, parent, weight)
{

}

Task::~Task()
{
	assert(!selfLock_);
}

TaskImpl& Task::_impl(AccessKey<Service, Mutex>)
{
	return impl_;
}

TaskP Task::start()
{
	impl_.start();
	return shared_from_this();
}

void Task::interrupt()
{
	impl_.interrupt();
}

bool Task::isInterrupted() const
{
	return impl_.isInterrupted();
}

void Task::setSelfLock(TaskP selfLock)
{
	selfLock_ = std::move(selfLock);
}

void Task::_execCallback(CallbackType type)
{
	const auto callAndDiscard = [](std::unique_ptr<DynamicCallback>& cb)
	{
		if (!cb) return;
		cb->exec();
		cb.reset();
	};

	switch (type)
	{
	case CallbackType::Succeeded: callAndDiscard(succeededCb_); break;
	case CallbackType::Interrupted: callAndDiscard(interruptedCb_); break;
	case CallbackType::Finished: callAndDiscard(finishedCb_); break;
	}
}

}

