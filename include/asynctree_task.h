#pragma once

#include "asynctree_config.h"
#include "asynctree_task_callbacks.h"
#include "asynctree_task_typedefs.h"
#include "asynctree_access_key.h"

namespace ast
{

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
	bool shared_;

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
	TaskCallbacks callbacks_;

	std::mutex taskMutex_;

	State state_;

	mutable bool interrupted_;

	// ���-�� �����, ������� ������ ��������� ���������� ��� ���������� �������� �����
	uint numChildrenToComplete_;

	struct WeightBuffer
	{
		// ���� � �������. ��� ��� ������� �� ����������� � �� ����������� � ������� (�� ����� S_Created)
		TaskImpl* firstChild_;
		TaskImpl* lastChild_;
	};

	WeightBuffer weightBuffers_[TW_Quantity];

public:
	TaskImpl(AccessKey<Task>, Task& task, Service& service, TaskImpl* parent, TaskWorkFunc workFunc, TaskCallbacks callbacks);
	~TaskImpl();

	Task& task();
	TaskImpl* parent();
	void exec(EnumTaskWeight weight);
	void destroy();
	void addChildTask(EnumTaskWeight weight, TaskImpl& child);
	void notifyDeferredTask();
	void addDeferredTask(EnumTaskWeight weight, TaskImpl& child);

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
	Task(Private, Service& service, TaskImpl* parent, TaskWorkFunc workFunc, TaskCallbacks callbacks);
	~Task();

	static TaskP _create(AccessKey<Service, Mutex>, Service& service, TaskImpl* parent, TaskWorkFunc workFunc, TaskCallbacks callbacks = TaskCallbacks());
	TaskImpl& _impl(AccessKey<Service, Mutex>);

	void interrupt();
	bool isInterrupted() const;
};

}
