#pragma once

#include "asynctree_config.h"

namespace AST
{

typedef std::shared_ptr<class QtConnector> QtConnectorP;
typedef std::weak_ptr<QtConnector> QtConnectorW;

class QtConnector : public QObject, public std::enable_shared_from_this<QtConnector>
{
	struct Private {};

	struct Event : public QEvent
	{
		std::function<void()> func_;

		Event(std::function<void()> func)
			: QEvent(QEvent::User), func_(std::move(func)) {}
	};

	std::mutex mutex_;
	QObject* object_;

public:
	// created in Qt thread
	QtConnector(Private, QObject* object)
		: QObject(nullptr), object_(object)
	{
		object_->installEventFilter(this);
	}

	~QtConnector()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (object_)
			object_->removeEventFilter(this);
	}

	static QtConnectorP create(QObject* object)
	{
		return std::make_shared<QtConnector>(Private(), object);
	}

	void notifyObjectDeleted()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		object_ = nullptr;
	}

	bool postFunc(std::function<void()> func)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (object_)
		{
			QCoreApplication::postEvent(object_, new Event(std::move(func)));
			return true;
		}

		return false;
	}

	bool eventFilter(QObject* obj, QEvent* evt) override
	{
		if (obj == object_ && evt->type() == QEvent::User)
		{
			if (auto* funcEvt = dynamic_cast<Event*>(evt))
			{
				funcEvt->func_();
				return true;
			}
		}

		return QObject::eventFilter(obj, evt);
	}
};

class ScopedQtConnector : boost::noncopyable
{
	QtConnectorP connector_;
public:
	ScopedQtConnector(QObject* object)
		: connector_(QtConnector::create(object))
	{
	}

	~ScopedQtConnector()
	{
		connector_->notifyObjectDeleted();
	}

	QtConnectorP operator* ()
	{
		return connector_;
	}
};

}

