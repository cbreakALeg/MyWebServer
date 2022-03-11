#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>

// 线程池类
// 线程池是一个生产者消费者模型
class ThreadPool {
public:
    // explicit 作用：防止构造函数在进行操作时会进行隐式转换
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {
            assert(threadCount > 0);

            // 创建 threadCount 个子线程
            for(size_t i = 0; i < threadCount; i++) {
                
                std::thread([pool = pool_] {
                    std::unique_lock<std::mutex> locker(pool->mtx);

                    while(true) {
                        if(!pool->tasks.empty()) {
                            
                            // 从任务队列中取第一个任务
                            auto task = std::move(pool->tasks.front());
                            // 移除掉队列中第一个元素
                            pool->tasks.pop();

                            locker.unlock();
                            task();         // 执行任务
                            locker.lock();
                        } 
                        else if(pool->isClosed) 
                            break;
                        else 
                            pool->cond.wait(locker);   // 如果任务队列为空，等待
                    }
                }).detach();// 线程分离
            }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() {
        if(static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;     // 关闭线程池
            }
            pool_->cond.notify_all();   // 唤醒一个等待线程
        }
    }

    template<class F>
    void AddTask(F&& task) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));    // 把任务添加到任务队列中
        }
        pool_->cond.notify_one();   // 唤醒一个等待的线程，唤醒的线程是随机的
    }

private:
    // 结构体，池子
    struct Pool {
        std::mutex mtx;     // 互斥锁
        std::condition_variable cond;   // 条件变量
        bool isClosed;          // 是否关闭
        std::queue<std::function<void()>> tasks;    // 队列（保存的是任务）
    };
    std::shared_ptr<Pool> pool_;  //  池子
};


#endif //THREADPOOL_H