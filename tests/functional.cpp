#include "asynctree.h"

#include <gtest/gtest.h>

#include <memory>
#include <future>
#include <atomic>

class Functional : public ::testing::Test
{
protected:
	std::unique_ptr<ast::Service> service_;

	void SetUp() override
	{
		service_ = std::make_unique<ast::Service>(4);
	}

	void TearDown() override
	{
		service_->waitUtilEverythingIsDone();
		service_.reset();
	}
};

TEST_F(Functional, TaskFuncIsCalledOnlyOnce)
{
	int numCalls = 0;
	service_->task(ast::Light, [&]() {
		++numCalls;
	}).start();

	service_->waitUtilEverythingIsDone();
	EXPECT_EQ(numCalls, 1);
}

TEST_F(Functional, OnSuccess)
{
	int succeeded = 0;
	int finished = 0;
	int interrupted = 0;

	service_->task(ast::Light, [&]() {})
		.succeeded([&]() { ++succeeded;  })
		.finished([&]() { ++finished;  })
		.interrupted([&]() { ++interrupted;  })
		.start();

	service_->waitUtilEverythingIsDone();
	EXPECT_EQ(succeeded, 1);
	EXPECT_EQ(finished, 1);
	EXPECT_EQ(interrupted, 0);
}

TEST_F(Functional, OnInterruptTask)
{
	int succeeded = 0;
	int finished = 0;
	int interrupted = 0;
	bool isInterruptedInsideTheTask = false;

	std::promise<bool> onSucceeded;
	auto onSucceededFuture = onSucceeded.get_future();
	std::promise<bool> onInterrupted;
	auto onInterruptedFuture = onInterrupted.get_future();

	auto task = service_->task(ast::Light, [&]() {
		onSucceeded.set_value(true);
		onInterruptedFuture.wait();
		isInterruptedInsideTheTask = ast::Service::currentTask()->isInterrupted();
	})
	.succeeded([&]() { ++succeeded; })
	.finished([&]() { ++finished;  })
	.interrupted([&]() { ++interrupted;  })
	.start();

	onSucceededFuture.wait();
	task->interrupt();
	onInterrupted.set_value(true);

	service_->waitUtilEverythingIsDone();
	EXPECT_EQ(succeeded, 0);
	EXPECT_EQ(finished, 1);
	EXPECT_EQ(interrupted, 1);
	EXPECT_EQ(task->isInterrupted(), true);
	EXPECT_EQ(isInterruptedInsideTheTask, true);
}

TEST_F(Functional, OnInterruptParentTask)
{
	int succeeded = 0;
	int finished = 0;
	int interrupted = 0;
	bool isInterruptedInsideTheTask = false;

	std::promise<bool> onSucceeded;
	auto onSucceededFuture = onSucceeded.get_future();
	std::promise<bool> onInterrupted;
	auto onInterruptedFuture = onInterrupted.get_future();

	auto task = service_->task(ast::Light, [&]() {
		auto childTask = service_->task(ast::Light, [&]() {
			onSucceeded.set_value(true);
			onInterruptedFuture.wait();
			isInterruptedInsideTheTask = ast::Service::currentTask()->isInterrupted();
		})
		.succeeded([&]() { ++succeeded; })
		.finished([&]() { ++finished;  })
		.interrupted([&]() { ++interrupted;  })
		.start();
	})
	.start();

	onSucceededFuture.wait();
	task->interrupt();
	onInterrupted.set_value(true);

	service_->waitUtilEverythingIsDone();
	EXPECT_EQ(succeeded, 0);
	EXPECT_EQ(finished, 1);
	EXPECT_EQ(interrupted, 1);
	EXPECT_EQ(task->isInterrupted(), true);
	EXPECT_EQ(isInterruptedInsideTheTask, true);
}

TEST_F(Functional, OnInterruptChildTask)
{
	int childSucceeded = 0;
	int childFinished = 0;
	int childInterrupted = 0;
	bool isInterruptedInsideTheTask = false;
	int parentSucceeded = 0;
	int parentFinished = 0;
	int parentInterrupted = 0;

	std::promise<bool> onSucceeded;
	auto onSucceededFuture = onSucceeded.get_future();
	std::promise<bool> onInterrupted;
	auto onInterruptedFuture = onInterrupted.get_future();

	auto task = service_->task(ast::Light, [&]() {
		auto childTask = service_->task(ast::Light, [&]() {
			onSucceeded.set_value(true);
			onInterruptedFuture.wait();
			isInterruptedInsideTheTask = ast::Service::currentTask()->isInterrupted();
		})
		.succeeded([&]() { ++childSucceeded; })
		.finished([&]() { ++childFinished;  })
		.interrupted([&]() { ++childInterrupted;  })
		.start();

		onSucceededFuture.wait();
		childTask->interrupt();
		onInterrupted.set_value(true);
	})
	.succeeded([&]() { ++parentSucceeded; })
	.finished([&]() { ++parentFinished;  })
	.interrupted([&]() { ++parentInterrupted;  })
	.start();

	service_->waitUtilEverythingIsDone();
	EXPECT_EQ(childSucceeded, 0);
	EXPECT_EQ(childFinished, 1);
	EXPECT_EQ(childInterrupted, 1);
	EXPECT_EQ(parentSucceeded, 1);
	EXPECT_EQ(parentFinished, 1);
	EXPECT_EQ(parentInterrupted, 0);
	EXPECT_EQ(task->isInterrupted(), false);
	EXPECT_EQ(isInterruptedInsideTheTask, true);
}

TEST_F(Functional, StartTaskFromCallback)
{
	std::vector<int> sequence;

	auto task = service_->task(ast::Light, [&]() {
		sequence.push_back(0);
	})
	.succeeded([&]() {
		service_->task(ast::Light, [&]() {
			sequence.push_back(1);
		})
		.finished([&]() {
			service_->task(ast::Light, [&]() {
				sequence.push_back(2);
			})
			.succeeded([&]() { sequence.push_back(3); })
			.finished([&]() { sequence.push_back(4); })
			.start();
		})
		.start();
	})
	.start();

	service_->waitUtilEverythingIsDone();
	EXPECT_EQ(sequence, std::vector<int>({ 0, 1, 2, 3, 4 }));
}

TEST_F(Functional, OneMillionTasks)
{
	std::atomic<int> counter(0);

	const auto size = sizeof(ast::Task);

	for (int a = 0; a < 10; ++a)
	{
		service_->task(ast::Light, [&] {
			for (int b = 0; b < 10; ++b)
			{
				service_->task(ast::Light, [&] {
					for (int c = 0; c < 10; ++c)
					{
						service_->task(ast::Light, [&] {
							for (int d = 0; d < 10; ++d)
							{
								service_->task(ast::Light, [&] {
									for (int e = 0; e < 10; ++e)
									{
										service_->task(ast::Light, [&] {
											for (int f = 0; f < 100; ++f)
											{
												service_->task(ast::Light, [&] {
													counter.fetch_add(1);
												})
												.start();
											}
										})
										.start();
									}
								})
								.start();
							}
						})
						.start();
					}
				})
				.start();
			}
		})
		.start();
	}

	service_->waitUtilEverythingIsDone();
	EXPECT_EQ(counter.load(), 1000000);
}