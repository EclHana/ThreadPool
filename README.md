# ThreadPool
1. 基于可变参模板编程和引用折叠原理，实现线程池**submitTask**接口，支持任意任务函数和任意参数的转递
2. 使用**packaged_task**打包任务，并使用**future**类型定制**submitTask**提交任务的返回值
3. 使用**unordered_map**和**queue**容器管理线程对象和任务，并且保证线程安全
4. 基于条件变量**condition_variable**和互斥锁**mutex**实现 任务提交线程 和 任务执行线程 间的通信机制
5. 支持线程数量 **Fixed** 和 **Cached** 模式的线程池定制
