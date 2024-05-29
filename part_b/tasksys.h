#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <thread>
#include <mutex>
#include <queue>
#include <vector>
#include <list>
#include <unordered_set>
#include <condition_variable>

/*
 * TaskSystemSerial: This class is the student's implementation of a
 * serial task execution engine.  See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemSerial: public ITaskSystem {
    public:
        TaskSystemSerial(int num_threads);
        ~TaskSystemSerial();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelSpawn: This class is the student's implementation of a
 * parallel task execution engine that spawns threads in every run()
 * call.  See definition of ITaskSystem in itasksys.h for documentation
 * of the ITaskSystem interface.
 */
class TaskSystemParallelSpawn: public ITaskSystem {
    public:
        TaskSystemParallelSpawn(int num_threads);
        ~TaskSystemParallelSpawn();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelThreadPoolSpinning: This class is the student's
 * implementation of a parallel task execution engine that uses a
 * thread pool. See definition of ITaskSystem in itasksys.h for
 * documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSpinning: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSpinning(int num_threads);
        ~TaskSystemParallelThreadPoolSpinning();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping: public ITaskSystem {
    private:
        int num_threads;
        std::thread* workers;
        bool stop;

        TaskID next_task_id;

        struct Task {
            TaskID task_id;
            IRunnable* task_ptr;
            std::vector<TaskID> deps;
            int num_total_tasks;
            int num_allocated_tasks;
            int num_done_tasks;
            std::mutex mtx;

            Task(TaskID task_id, int num_total_tasks,
                 IRunnable* task_ptr, std::vector<TaskID> deps);
        };

        std::queue<Task*> ready_queue;
        std::list<Task*> waiting_list;
        std::unordered_set<Task*> working_set;
        std::unordered_set<TaskID> completed_set;
        std::mutex mutex_ready_queue, mutex_waiting_list;
        std::mutex mutex_working_set, mutex_completed_set;
        std::condition_variable cv_work, cv_done;
        std::mutex mutex_done;

        void entry();
        void check_waiting_list();
        void check_done();
    public:
        TaskSystemParallelThreadPoolSleeping(int num_threads);
        ~TaskSystemParallelThreadPoolSleeping();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

#endif
