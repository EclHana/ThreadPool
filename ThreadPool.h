#ifndef THREADPOOL_CLION_THREADPOOL_H
#define THREADPOOL_CLION_THREADPOOL_H

#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <future>

class Thread
{
public:
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc func);
	~Thread()=default;
	void start(int threadId);
	int getId()const;
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;
};

enum class PoolMode
{
	MODE_FIXED,
	MODE_CACHED,
};

class ThreadPool {
public:
	using ThreadFunc = std::function<void(int)>;
	ThreadPool();
	~ThreadPool();
	void setMode(PoolMode mode_);
	void setTaskQueThreshHold(size_t threshHold);
	void setThreadSizeThreshHold(size_t threadhold);
	void start(size_t initThreadSize = std::thread::hardware_concurrency());

	//使用可变参数模板编程，使函数可以接收任意函数和任意数量的参数
	template<typename Func,typename...Args>
	auto submitTask(Func&& func,Args&&... args)->std::future<decltype(func(args...))>
	{
		using RType = decltype(func(args...));
		//用绑定器把参数全部绑定到传入的func上，然后生成一个packaged_task存储这个函数对象
		auto task = std::make_shared<std::packaged_task<RType()>>(std::bind(
				std::forward<Func>(func),std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();

		//参数生成完毕，开始往taskQue临界区中塞任务
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		if(!notFull_.wait_for(lock,std::chrono::seconds(1),
				[&](){return taskQue.size()<taskQueThreshHold_;}))
		{
			std::cerr<<"Task queue is full, submit task fail!"<<std::endl;
			//因为返回了一个future对象，不能不管这个返回值，返回一个RType的0值即可
			// 不然用户调用future的get就直接阻塞到死，因为task根本没执行自然也没有资源可以get
			auto task = std::make_shared<std::packaged_task<RType()>>([](){return RType();});
			(*task)();//运行一下,不然外面是get不到这个0值的
			return task->get_future();
		}
		//using Task = std::function<void()>;
		//task : std::packaged_task<RType()>;
		//两个类型不相容需要封装一下,用一个无返回值的lambda表达式装task函数的执行，
		taskQue.emplace([task](){//这个地方一定要用值去获取task，不能用引用，因为task是局部变量，出函数就析构了
			(*task)();
		});
		taskSize_++;
		notEmpty_.notify_all();
		//动态模式要根据task数量来增加线程数量，但不能无限增加
		if(poolMode_==PoolMode::MODE_CACHED
		&& taskSize_>idleThreadSize_
		&& currThreadSize_<threadSizeThreshHold_)
		{
			auto thPtr = std::make_unique<Thread>(
					std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1));
			int threadId = thPtr->getId();
			threads_[threadId] = std::move(thPtr);
			threads_[threadId]->start(threadId);
			idleThreadSize_++;
			currThreadSize_++;
		}
		return result;
	}

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	void threadFunc(int threadId);
private:
	size_t initThreadSize_;
	std::unordered_map<int,std::unique_ptr<Thread>> threads_;
	PoolMode poolMode_;
	//任务队列，内装函数对象
	using Task = std::function<void()>;//这个地方因为不知道传入的任务参数和返回值都是什么，所以都是void
	std::queue<Task> taskQue;
	//任务数量参数
	std::atomic_uint taskSize_;
	size_t taskQueThreshHold_;
	//线程数量参数
	std::atomic_uint currThreadSize_;
	size_t threadSizeThreshHold_;
	std::atomic_uint idleThreadSize_;

	std::atomic_bool isPoolRunning_ = false;
	//线程通信变量,任务队列是临界资源访问加锁
	std::mutex taskQueMtx_;
	std::condition_variable notFull_;
	std::condition_variable notEmpty_;
	std::condition_variable exitCond_;

};


#endif //THREADPOOL_CLION_THREADPOOL_H
