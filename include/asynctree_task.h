#pragma once

#include "asynctree_config.h"
#include "asynctree_task_typedefs.h"
#include "asynctree_access_key.h"

namespace ast
{

typedef std::function<void()> TaskSucceededCallback;
typedef std::function<void()> TaskInterruptedCallback;
typedef std::function<void()> TaskFinishedCallback;

class Service;
class Mutex;
class Task;

class TaskImpl 
{
public:
	// hooks and parameters for different queues in service, mutexes and tasks
	TaskImpl* next_;
	EnumTaskWeight weight_;
	Mutex* mutex_;
	uint shared_ : 1;

private:
	enum State
	{
		S_Created,
		S_Working,
		S_WaitForChildren,
		S_Done,
	};

	Task& task_;
	Service& service_;

	TaskImpl* parent_;

	TaskWorkFunc workFunc_;
	TaskSucceededCallback succeededCb_;
	TaskInterruptedCallback interruptedCb_;
	TaskFinishedCallback finishedCb_;

	std::mutex taskMutex_;

	State state_;

	mutable uint interrupted_ : 1;

	uint numChildrenToComplete_ : 20;

	struct WeightBuffer
	{
		TaskImpl* firstChild_ = nullptr;
		TaskImpl* lastChild_ = nullptr;
	};

	WeightBuffer weightBuffers_[TW_Quantity];

public:
	TaskImpl(AccessKey<Task>, Task& task, Service& service, TaskImpl* parent, 
		EnumTaskWeight weight, TaskWorkFunc workFunc);
	~TaskImpl();

	Task& task();
	TaskImpl* parent();
	void exec(EnumTaskWeight weight);
	void destroy();
	void addChildTask(EnumTaskWeight weight, TaskImpl& child);
	void notifyDeferredTask();
	void addDeferredTask(EnumTaskWeight weight, TaskImpl& child);

	void succeeded(TaskSucceededCallback succeeded);
	void interrupted(TaskInterruptedCallback interrupted);
	void finished(TaskFinishedCallback finished);
	void start();

	void interrupt();
	bool isInterrupted() const;

private:
	void _addChildTaskNoIncCounter(EnumTaskWeight weight, TaskImpl& child, std::unique_lock<std::mutex>& lock);
	void _interruptFromExec(std::unique_lock<std::mutex>& lock);
	void _interruptFromParent();

	void _onFinished(std::unique_lock<std::mutex>& lock);
	void _notifyChildFinished();
};

class Task : public std::enable_shared_from_this<Task>
{
	friend class TaskImpl;

	TaskImpl impl_;
	TaskP selfLock_;

	struct Private {};
public:
	Task(Private, Service& service, TaskImpl* parent, EnumTaskWeight weight, TaskWorkFunc workFunc);
	~Task();

	static TaskP _create(AccessKey<Service, Mutex>, Service& service, TaskImpl* parent, 
		EnumTaskWeight weight, TaskWorkFunc workFunc);
	TaskImpl& _impl(AccessKey<Service, Mutex>);

	Task& succeeded(TaskSucceededCallback succeeded);
	Task& interrupted(TaskInterruptedCallback interrupted);
	Task& finished(TaskFinishedCallback finished);
	TaskP start();

	void interrupt();
	bool isInterrupted() const;
};

}
