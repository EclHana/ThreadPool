#include "ThreadPool.h"

/*-------------Thread-------------*/
int Thread::generateId_ = 0;

Thread::Thread(Thread::ThreadFunc func)
	: func_(func),threadId_(generateId_++)
{

}

void Thread::start(int threadId) {
	std::thread t1(func_,threadId);
	t1.detach();
}

int Thread::getId() const {
	return threadId_;
}

/*-------------ThreadPool-------------*/

constexpr size_t TASK_MAX_THRSHHOLD = 1024;
constexpr size_t THREAD_MAX_THRSHHOLD = 10;
constexpr int THREAD_IDLE_TIME = 60;    //单位秒


ThreadPool::ThreadPool()
	:initThreadSize_(0),
	poolMode_(PoolMode::MODE_FIXED),
	taskSize_(0),
	taskQueThreshHold_(TASK_MAX_THRSHHOLD),
	currThreadSize_(0),
	threadSizeThreshHold_(THREAD_MAX_THRSHHOLD),
	idleThreadSize_(0)
{

}

ThreadPool::~ThreadPool() {
	isPoolRunning_ = false;
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();//唤醒所有线程使之进入回收资源工作区代码
	exitCond_.wait(lock,[&](){return threads_.size()==0;});
}

void ThreadPool::setMode(PoolMode mode_) {
	if(isPoolRunning_) return;
	poolMode_ = mode_;
}

void ThreadPool::setTaskQueThreshHold(size_t threshHold) {
	taskQueThreshHold_ = threshHold;
}

void ThreadPool::setThreadSizeThreshHold(size_t threadhold) {
	threadSizeThreshHold_ = threadhold;
}

void ThreadPool::start(size_t initThreadSize)
{
	initThreadSize_ = initThreadSize;
	currThreadSize_ = initThreadSize;
	isPoolRunning_ = true;
	for(int i = 0;i<initThreadSize_;++i)
	{
		std::unique_ptr<Thread> threadPtr =
				std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1));
		int threadId = threadPtr->getId();
		threads_[threadId] = std::move(threadPtr);
	}
	for(auto&[i,th]:threads_)
	{
		th->start(i);
		idleThreadSize_++;
	}
}

void ThreadPool::threadFunc(int threadId)
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	while(1)
	{
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		std::cout<<std::this_thread::get_id()<<": is finding a job..."<<std::endl;
		while(taskQue.size()==0)
		{
			if(!isPoolRunning_)
			{
				//析构那里通过notEmpty_上唤醒了所有线程，进入资源回收阶段
				threads_.erase(threadId);
				exitCond_.notify_all();
				std::cout<<std::this_thread::get_id()<<": has exited!"<<std::endl;
				return;
			}
			//两种处理方式，Cached是动态处理空余线程，Fixed就是死等
			if(poolMode_==PoolMode::MODE_CACHED)
			{
				if(notEmpty_.wait_for(lock,std::chrono::seconds(1)) == std::cv_status::timeout)
				{
					auto now = std::chrono::high_resolution_clock().now();
					auto dur = std::chrono::duration_cast<std::chrono::seconds>(now-lastTime);
					if(dur.count()>=THREAD_IDLE_TIME && currThreadSize_>initThreadSize_)
					{
						threads_.erase(threadId);
						idleThreadSize_--;
						currThreadSize_--;
						std::cout<<std::this_thread::get_id()<<": has exited!"<<std::endl;
						return;
					}
				}
			}
			else
			{
				notEmpty_.wait(lock);
			}
		}
		idleThreadSize_--;
		auto task = taskQue.front();
		taskQue.pop();
		std::cout<<std::this_thread::get_id()<<": access a mission."<<std::endl;
		//获取到任务就要释放锁
		lock.unlock();

		notEmpty_.notify_all();
		notFull_.notify_all();

		task();
		idleThreadSize_++;
		//更新Cached模式的空闲线程初始时间
		lastTime = std::chrono::high_resolution_clock().now();
	}
}
