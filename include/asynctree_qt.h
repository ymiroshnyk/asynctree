#pragma once

#include <QCoreApplication>
#include <QEvent>

namespace ast
{
namespace qt
{

template <typename TFunc>
void contextCall(QObject* obj, TFunc func)
{
    struct Event : public QEvent 
    {
        TFunc func_;

        Event(TFunc func) : QEvent(QEvent::None), func_(std::move(func)) {}
        ~Event() { func_(); }
   };

    QCoreApplication::postEvent(obj, new Event(std::move(func)));
}

template <typename TFunc>
struct ContextFunc
{
    QObject* obj_;
    TFunc func_;
    ContextFunc(QObject* obj, TFunc func) : obj_(obj), func_(std::move(func)) {}
    void operator ()() { contextCall(obj_, std::move(func_)); }
};

template <typename TFunc>
ContextFunc<TFunc> contextFunc(QObject* obj, TFunc func)
{
    return ContextFunc<TFunc>(obj, std::move(func));
}

}
}
