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
	/*{
		// wait if any mutex lock was waiting before deleting
		std::lock_guard<std::mutex> lock(taskMutex_);
	}*/
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

	assert(state_ != State::Done);

	if (isInterrupted())
	{
		switch (state_)
		{
		case State::Created:
		{
			_onFinished(lock);
			return;
		}

		case State::Working:
			// we will not start child task when parent is working and interrupted
			return;

		default: assert(state_ == State::WaitForChildren);
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
				_onFinished(lock);
			}
			return;
		}
		}
	}

	if (state_ == State::Created)
	{
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

		assert(task.state_ == State::Created);
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
	assert(state_ == State::Working || state_ == State::WaitForChildren);
	++numChildrenToComplete_;
}

void TaskImpl::addDeferredTask(EnumTaskWeight weight, TaskImpl& child)
{
	std::unique_lock<std::mutex> lock(taskMutex_);
	_addChildTaskNoIncCounter(weight, child, lock);
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

void TaskImpl::_addChildTaskNoIncCounter(EnumTaskWeight weight, TaskImpl& child, std::unique_lock<std::mutex>& lock)
{
	assert(state_ == State::Working || state_ == State::WaitForChildren);
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

void TaskImpl::_interruptFromParent()
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

	// Further don't use 'this'.

	if (isInterrupted())
		task_._execCallback(CallbackType::Interrupted);
	else
		task_._execCallback(CallbackType::Succeeded);

	task_._execCallback(CallbackType::Finished);

	if (mutex_)
		mutex_->_taskFinished(KEY);

	if (parent_)
		parent_->_notifyChildFinished();

	destroy();
}

void TaskImpl::_notifyChildFinished()
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
	switch (type)
	{
	case CallbackType::Succeeded: if (succeededCb_) succeededCb_->exec(); break;
	case CallbackType::Interrupted: if (interruptedCb_) interruptedCb_->exec(); break;
	case CallbackType::Finished: if (finishedCb_) finishedCb_->exec(); break;
	}
}

}

