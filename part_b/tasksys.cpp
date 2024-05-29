#include "tasksys.h"


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
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemSerial::sync() {
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
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

TaskSystemParallelThreadPoolSleeping::Task::Task(TaskID task_id, int num_total_tasks, IRunnable *task_ptr,
                                                 std::vector <TaskID> deps) : deps(deps) {
    this->task_id = task_id;
    this->num_total_tasks = num_total_tasks;
    this->task_ptr = task_ptr;
    num_allocated_tasks = num_done_tasks = 0;
}

void TaskSystemParallelThreadPoolSleeping::entry() {
    while (!stop) {
        std::unique_lock<std::mutex> lock(mutex_ready_queue);
        cv_work.wait(lock, [&](){
            return stop || !ready_queue.empty();
        });

        if (stop) {
            lock.unlock();
            break;
        }

        Task* task_ptr = ready_queue.front();
        if (!task_ptr->num_total_tasks) {
            ready_queue.pop();
            lock.unlock();
            check_done();
            break;
        }

        TaskID task_id = task_ptr->num_allocated_tasks;
        task_ptr->num_allocated_tasks++;
        if (task_ptr->num_allocated_tasks == task_ptr->num_total_tasks) {
            ready_queue.pop();
            mutex_working_set.lock();
            working_set.insert(task_ptr);
            mutex_working_set.unlock();
        }
        lock.unlock();

        task_ptr->task_ptr->runTask(task_id, task_ptr->num_total_tasks);

        task_ptr->mtx.lock();
        task_ptr->num_done_tasks++;
        if (task_ptr->num_done_tasks == task_ptr->num_total_tasks) {
            mutex_working_set.lock();
            working_set.erase(task_ptr);
            mutex_working_set.unlock();

            mutex_completed_set.lock();
            completed_set.insert(task_ptr->task_id);
            mutex_completed_set.unlock();

            task_ptr->mtx.unlock();
            delete task_ptr;

            check_waiting_list();
            check_done();
        } else {
            task_ptr->mtx.unlock();
        }

    }
}

void TaskSystemParallelThreadPoolSleeping::check_waiting_list() {
    std::lock_guard<std::mutex> lock_waiting_list(mutex_waiting_list);
    waiting_list.remove_if([&](Task* task_ptr){
        for (TaskID task_id : task_ptr->deps) {
            std::lock_guard<std::mutex> lock_completed_set(mutex_completed_set);
            if (completed_set.find(task_id) == completed_set.end())
                return false;
        }

        mutex_ready_queue.lock();
        ready_queue.push(task_ptr);
        mutex_ready_queue.unlock();
        cv_work.notify_all();
        return true;
    });
}

void TaskSystemParallelThreadPoolSleeping::check_done() {
    std::lock_guard<std::mutex> lock_waiting_list(mutex_waiting_list);
    std::lock_guard<std::mutex> lock_ready_queue(mutex_ready_queue);
    std::lock_guard<std::mutex> lock_working_list(mutex_working_set);
    if (waiting_list.empty() && ready_queue.empty() && working_set.empty())
        cv_done.notify_all();
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

    next_task_id = 0;
    this->num_threads = num_threads;
    stop = false;
    workers = new std::thread[num_threads];

    for (int i = 0; i < num_threads; ++i)
        workers[i] = std::thread([this](){entry();});
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // Done: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //

    stop = true;
    cv_work.notify_all();
    for (int i = 0; i < num_threads; ++i)
        workers[i].join();
    delete[] workers;
    while (!ready_queue.empty()) {
        delete ready_queue.front();
        ready_queue.pop();
    }
    for (auto i : waiting_list)
        delete i;
    for (auto i : working_set)
        delete i;
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // Done: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    std::vector<TaskID> noDeps;
    runAsyncWithDeps(runnable, num_total_tasks, noDeps);
    sync();
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // Done: CS149 students will implement this method in Part B.
    //

    TaskID task_id = next_task_id++;

    auto task_ptr = new Task(task_id, num_total_tasks, runnable, deps);

    mutex_waiting_list.lock();
    waiting_list.push_back(task_ptr);
    mutex_waiting_list.unlock();
    check_waiting_list();

    return task_id;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // Done: CS149 students will modify the implementation of this method in Part B.
    //

    std::unique_lock<std::mutex> lock(mutex_done);
    cv_done.wait(lock, [&](){
        std::lock_guard<std::mutex> lock_waiting_list(mutex_waiting_list);
        std::lock_guard<std::mutex> lock_ready_queue(mutex_ready_queue);
        std::lock_guard<std::mutex> lock_working_list(mutex_working_set);
        return waiting_list.empty() && ready_queue.empty() && working_set.empty();
    });
}
