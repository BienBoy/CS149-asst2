#include "tasksys.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>


IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads): ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemSerial::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    //
    // Done: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    this->num_threads = num_threads;
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {


    //
    // Done: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    std::atomic<int> done(0);

    auto entry = [&done, num_total_tasks, runnable](){
        while (true) {
            int taskID = done++;
            if (taskID >= num_total_tasks)
                return;
            runnable->runTask(taskID, num_total_tasks);
        }
    };

    auto workers = new std::thread[num_threads];
    for (int i = 1; i < num_threads; i++)
        workers[i] = std::thread(entry);

    entry();

    for (int i = 1; i < num_threads; i++)
        workers[i].join();

    delete[] workers;
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

void TaskSystemParallelThreadPoolSpinning::entry() {
    while (!stop) {
        mtx.lock();
        if (num_left_task <= 0) {
            mtx.unlock();
            continue;
        }
        int task_id = num_total_tasks - num_left_task;
        num_left_task--;
        mtx.unlock();
        task->runTask(task_id, num_total_tasks);
        num_done_task++;
    }
}

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
    //
    // Done: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    this->num_threads = num_threads;
    stop = false;
    num_total_tasks = num_left_task = 0;

    workers = new std::thread[num_threads];
    for (int i = 0; i < num_threads; ++i)
        workers[i] = std::thread([this]() { entry(); });
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    this->stop = true;
    for (int i = 0; i < num_threads; ++i)
        workers[i].join();
    delete[] workers;
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {


    //
    // Done: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    num_done_task = 0;
    this->num_total_tasks = num_total_tasks;
    task = runnable;
    mtx.lock();
    num_left_task = num_total_tasks;
    mtx.unlock();
    while (num_done_task < num_total_tasks)
        continue;
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

void TaskSystemParallelThreadPoolSleeping::entry() {
    while (!stop) {
        std::unique_lock<std::mutex> lock(mutex_left);
        cv_worker.wait(lock, [this](){return stop || num_left_task > 0;});
        if (stop) {
            lock.unlock();
            break;
        }
        int task_id = num_total_tasks - num_left_task;
        num_left_task--;
        lock.unlock();
        task->runTask(task_id, num_total_tasks);

        mutex_done.lock();
        num_done_task++;
        if (num_done_task == num_total_tasks) {
            mutex_done.unlock();
            cv_main.notify_one();
        } else {
            mutex_done.unlock();
        }
    }
}

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): ITaskSystem(num_threads) {
    //
    // Done: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    this->num_threads = num_threads;
    stop = false;
    num_total_tasks = num_left_task = 0;

    workers = new std::thread[num_threads];
    for (int i = 0; i < num_threads; ++i)
        workers[i] = std::thread([this]() { entry(); });
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // Done: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    this->stop = true;
    cv_worker.notify_all();
    for (int i = 0; i < num_threads; ++i)
        workers[i].join();
    delete[] workers;
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // Done: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    task = runnable;
    num_done_task = 0;
    this->num_total_tasks = num_total_tasks;

    mutex_left.lock();
    num_left_task = num_total_tasks;
    mutex_left.unlock();

    cv_worker.notify_all();

    std::unique_lock<std::mutex> lock(mutex_done);
    cv_main.wait(lock, [&](){return num_done_task == num_total_tasks;});
    lock.unlock();
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // Done: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // Done: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}
