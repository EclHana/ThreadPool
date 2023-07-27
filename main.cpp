#include <iostream>
#include "ThreadPool.h"

size_t sum(int a,int b)
{
	size_t res = 0;
	for(int i = a;i<=b;i++)
	{
		res+=i;
	}
	return res;
}


int main() {
	ThreadPool pool;
	//pool.setMode(PoolMode::MODE_CACHED);
	pool.start(4);
	std::future<size_t> res1 = pool.submitTask(sum,510,2566000);
	size_t r = res1.get();
	std::cout<<"res:"<<r<<std::endl;
	getchar();
}
