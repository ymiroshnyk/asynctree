#pragma once

#include <memory>

namespace ast
{

typedef std::shared_ptr<class Task> TaskP;
typedef std::weak_ptr<Task> TaskW;

}