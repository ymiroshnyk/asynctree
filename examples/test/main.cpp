#include "asynctree.h"

#include <boost/atomic.hpp>
#include <boost/chrono.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <iostream>

using namespace AST;

Service service;

class Timestamp
{
	boost::chrono::high_resolution_clock::time_point time_;

public:
	Timestamp()
		: time_(boost::chrono::high_resolution_clock::now())
	{
	}

	long long operator - (const Timestamp& other) const
	{
		return boost::chrono::duration_cast<boost::chrono::microseconds>(
			time_ - other.time_).count();
	}
};

///////////////////////////////////////////////////////////////////////////////
// simple example

void simpleGrandChildTask1(int a, int b)
{
	//boost::this_thread::sleep(boost::posix_time::milliseconds(rand() % 10));
}

void simpleGrandChildTask(int a, int b)
{
	//boost::this_thread::sleep(boost::posix_time::milliseconds(rand() % 10));
}

void simpleChildTask(int a, int b)
{
	for (uint i = 0; i < 100; ++i)
	{
		service.startAutoTask(TW_Light, boost::bind(simpleGrandChildTask, a + 1, b + 1));
	}

	//boost::this_thread::sleep(boost::posix_time::milliseconds(rand() % 5));
}

void simpleTask(int a, int b)
{
	for (uint i = 0; i < 100; ++i)
	{
		service.startAutoTask(TW_Light, boost::bind(simpleChildTask, a + 1, b + 1));
	}
}

void simpleExample()
{
	const Timestamp startTime = Timestamp();

	service.startAutoTask(TW_Light,
		[]()
		{
			int a = 2;
			int b = 3;

			for (uint i = 0; i < 1; ++i)
			{
				service.startAutoTask(TW_Light,
					boost::bind(simpleTask, a++, b++), // task
					TaskCallbacks().succeeded([] () { 
						// successfully finished
					}).finished(
					[] () {
						// all task tree finished
					}));
				
			}

		},
		TaskCallbacks().succeeded([&startTime]()
		{
			const long long elapsedTime = (Timestamp() - startTime) / 1000000;
			std::cout << "Elapsed time: " << elapsedTime << std::endl;
		}));
}

///////////////////////////////////////////////////////////////////////////////
// sample with logging

boost::mutex printMutex;

void sampleGrandChildTask(std::string caption)
{
	{
		boost::mutex::scoped_lock lock(printMutex);
		std::cout << caption << " started" << std::endl;
	}

	for (uint i = 0; i < 10; ++i)
	{
		if (service.currentTask()->isInterrupted())
			return;

		boost::this_thread::sleep(boost::posix_time::milliseconds(rand() % 4));
	}

	{
		boost::mutex::scoped_lock lock(printMutex);
		std::cout << caption << " finished" << std::endl;
	}
}

void sampleChildTask(std::string caption)
{
	{
		boost::mutex::scoped_lock lock(printMutex);
		std::cout << caption << " started" << std::endl;
	}

	for (uint i = 0; i < 3; ++i)
	{
		std::stringstream s;
		s << "\t" << caption << " - " << i;
		std::string childCaption = s.str();

		service.startAutoTask(TW_Light,
			boost::bind(sampleGrandChildTask, s.str()),
			TaskCallbacks().succeeded([childCaption] () {
					boost::mutex::scoped_lock lock(printMutex);
					std::cout << childCaption << " tree SUCCESS <---" << std::endl;
			}).finished([childCaption] () {
					boost::mutex::scoped_lock lock(printMutex);
					std::cout << childCaption << " tree finished" << std::endl;
			}));
	}

	for (uint i = 0; i < 10; ++i)
	{
		if (service.currentTask()->isInterrupted())
			return;

		boost::this_thread::sleep(boost::posix_time::milliseconds(rand() % 2));
	}

	{
		boost::mutex::scoped_lock lock(printMutex);
		std::cout << caption << " finished" << std::endl;
	}
}

void sampleTask(std::string caption)
{
	{
		boost::mutex::scoped_lock lock(printMutex);
		std::cout << caption << " started" << std::endl;
	}

	for (uint i = 0; i < 3; ++i)
	{
		std::stringstream s;
		s << "\t" << caption << " - " << i;
		std::string childCaption = s.str();

		service.startAutoTask(TW_Light,
			boost::bind(sampleChildTask, s.str()),
			TaskCallbacks().succeeded([childCaption] () {
					boost::mutex::scoped_lock lock(printMutex);
					std::cout << childCaption << " tree SUCCESS <---" << std::endl;
			}).finished([childCaption] () {
					boost::mutex::scoped_lock lock(printMutex);
					std::cout << childCaption << " tree finished" << std::endl;
			}));
	}

	{
		boost::mutex::scoped_lock lock(printMutex);
		std::cout << caption << " finished" << std::endl;
	}
}

void exampleWithLogging()
{
	for (uint i = 1; i < 2; ++i)
	{
		std::string caption = "task0";

		TaskP task = service.startAutoTask(TW_Light, 
			boost::bind(sampleTask, caption),
			TaskCallbacks().succeeded([caption] () {
				boost::mutex::scoped_lock lock(printMutex);
				std::cout << caption << " tree SUCCESS <---" << std::endl;
			}).finished([caption] () {
				boost::mutex::scoped_lock lock(printMutex);
				std::cout << caption << " tree finished" << std::endl;
			}));

		/*if (i > 0)
		{
			std::shared_ptr<basio::deadline_timer> timer = std::make_shared<basio::deadline_timer>(*service, boost::posix_time::milliseconds(100));

			timer->async_wait([taskHandler, timer] (const boost::system::error_code&)
			{
				taskHandler->abort();
			});
		}*/
	}
}

///////////////////////////////////////////////////////////////////////////////
// shared mutex example

void childCheckMutex(Mutex& mutex)
{
}

void readTask(int i, Mutex& mutex)
{
	{
		boost::mutex::scoped_lock lock(printMutex);
		std::cout << "Read " << i << std::endl;
	}

	service.startAutoTask(TW_Light, boost::bind(childCheckMutex, boost::ref(mutex)));

	boost::this_thread::sleep(boost::posix_time::milliseconds((rand() % 100) + 500));
}

void writeTask(int i, Mutex& mutex)
{
	{
		boost::mutex::scoped_lock lock(printMutex);
		std::cout << "Write " << i << std::endl;
	}

	service.startAutoTask(TW_Light, boost::bind(childCheckMutex, boost::ref(mutex)));

	boost::this_thread::sleep(boost::posix_time::milliseconds((rand() % 100) + 500));
}

void exampleWithSharedMutex()
{
	Mutex mutex(service);

	for (int i = 0; i < 10; ++i)
		mutex.startSharedAutoTask(TW_Light, boost::bind(readTask, i, boost::ref(mutex)));
	
	for (int i = 0; i < 10; ++i)
		mutex.startAutoTask(TW_Light, boost::bind(writeTask, i, boost::ref(mutex)));

	for (int i = 0; i < 10; ++i)
		mutex.startSharedAutoTask(TW_Light, boost::bind(readTask, i, boost::ref(mutex)));

	for (int i = 0; i < 2; ++i)
		mutex.startAutoTask(TW_Light, boost::bind(writeTask, i, boost::ref(mutex)));
}

///////////////////////////////////////////////////////////////////////////////

int main(void)
{
	simpleExample();
	service.waitUtilEverythingIsDone();

	exampleWithLogging();	
	service.waitUtilEverythingIsDone();

	exampleWithSharedMutex();
	service.waitUtilEverythingIsDone();

	return 0;
}

///////////////////////////////////////////////////////////////////////////////