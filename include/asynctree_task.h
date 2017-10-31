#pragma once

#include "asynctree_config.h"
#include "asynctree_task_callbacks.h"
#include "asynctree_task_typedefs.h"

namespace AST
{

class Service;
class Mutex;

class Task : public std::enable_shared_from_this<Task>
{
public:
	// ������������ ��� � �������, ��� � ��� �����. �� ������ ������������ � ���, � ���
	Task* next_;
	// ��� worker �������. ��������� � ����� ����� ��������� ����
	EnumTaskWeight weight_;
	// ��� mutex
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

	Service& service_;

	Task* parent_;

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
		Task* firstChild_;
		Task* lastChild_;
	};

	WeightBuffer weightBuffers_[TW_Quantity];

	TaskP selfLock_;

	struct Private {};
public:
	Task(Private, Service& service, Task* parent, TaskWorkFunc workFunc, TaskCallbacks callbacks);
	~Task();

	// Service-only interface
	static TaskP create(Service& service, Task* parent, TaskWorkFunc workFunc, TaskCallbacks callbacks = TaskCallbacks());
	Task* parent();
	void exec(EnumTaskWeight weight);
	void destroy();
	void addChildTask(EnumTaskWeight weight, Task& child);
	void notifyDeferredTask();
	void addDeferredTask(EnumTaskWeight weight, Task& child);

	// public interface
	void interrupt();
	bool isInterrupted() const;

private:
	void _addChildTaskNoIncCounter(EnumTaskWeight weight, Task& child, std::unique_lock<std::mutex>& lock);
	void interruptFromExec(std::unique_lock<std::mutex>& lock);
	void interruptFromParent();

	void onFinished(std::unique_lock<std::mutex>& lock);
	void notifyChildFinished();
};

}
