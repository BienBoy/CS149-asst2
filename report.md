# 实验报告

## Part A

### Step 1

只需要实现一个简单的任务系统，在`run()`的开始生成工作线程，并在`run()`返回之前从主线程合并这些线程。任务的分配方式采用动态分配，即每个线程每次取一个任务完成，能者多劳。

每个线程的核心实现为：

```c++
while (true) {
    int taskID = done++;
    if (taskID >= num_total_tasks)
        return;
    runnable->runTask(taskID, num_total_tasks);
}
```

### Step 2

Step 1 实现的任务系统，每次调用`run()`都会重新生成线程，线程管理开销大。为了减少线程创建开销，引入线程池，即：在任务系统初始化时创建所有线程，无任务时各线程执行忙等待，有任务时，每个线程每次取一个任务完成。

### Step 3

Step 2 中，线程忙等待期间也会占用 CPU 核心的执行资源，这会影响性能。Step 3 中要使用环境变量让空闲线程进入睡眠态，直到有任务要执行，在实现时，需要使用两个条件变量：一个工作线程等待有任务到来，另一个用于主线程等待所有任务完成。

## Part B

在 Part A 中实现的任务系统是同步的，每次调用`run()`都会等到所有任务完成才返回。Part B 要求实现异步的任务系统，这样在执行任务期间，主线程可以执行其他操作，此外，在同步任务系统中，调用`run()`后工作线程只能执行一种任务，在任务数较少或大多数任务完成时，会有一些工作线程处于空闲；异步任务系统，可以多次调用`runAsyncWithDeps()`添加多种任务，工作线程可以同时执行多种任务，因此可以提高性能。

实现异步任务系统需要考虑任务调度存在依赖的问题，某些任务必须在前面某些任务执行后才能开始执行。分析任务调度的过程，可以发现任务有四种状态：

- 未就绪：需要等待依赖任务完成
- 就绪：依赖的任务都已完成，可以随时被调度执行
- 分配完成：已将任务的`num_total_tasks`项工作全部分给各线程
- 已完成：任务的`num_total_tasks`项工作全部完成

对应任务的四种状态，定义四种数据结构：

- `waiting_list`：存储未就绪的任务，由于需要在有任务完成后需要遍历所有未就绪的任务，将依赖满足的任务转为就绪态，因此涉及到遍历及频繁的数据添加、删除，可以使用链表`std::list`实现
- `ready_queue`：存储就绪的任务，使用队列`std::queue`实现即可，工作线程每次从队首的任务中取一项工作完成，直到对任务的工作全部分配给线程后，将队首任务移出，转为分配完成状态
- `working_set`：存储分配完成的任务，等到其所有工作完成后，转为已完成态。由于只涉及在将任务加入及在条件满足后将任务移出，可以使用集合`std::unordered_set`实现
- `completed_set`：存储已完成的任务，只用于判断任务是否完成，使用集合`std::unordered_set`实现即可， 也可以用一个大数组作为哈希表实现

这四种状态的转移条件为：

- 未就绪：为了简化实现，添加的任务**无论有无依赖**都先添加到`waiting_list`中
- 就绪：在**添加的任务后，以及有任务完成时**，遍历`waiting_list`，将依赖满足的任务移出并加入`ready_queue`
- 分配完成：工作线程不断从`ready_queue`取工作，**队首任务的工作分配完成后**将其出队，加入`working_set`
- 已完成：**每次工作线程完成一项工作后**，都检查该工作所在的任务是否完成，若已完成，将其从`working_set`移除，加入`completed_set`

在具体实现时，为了防止并发访问造成数据冲突，还要为这四种数据结构分别声明一个互斥量，保护访问。

基于`runAsyncWithDeps`和`sync`可以实现`run`：

```c++
void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {
    std::vector<TaskID> noDeps;
    runAsyncWithDeps(runnable, num_total_tasks, noDeps);
    sync();
}
```

## 结果

异步任务系统的性能测试结果如下：

```
================================================================================
Running task system grading harness... (22 total tests)
  - Detected CPU with 8 execution contexts
  - Task system configured to use at most 8 threads
================================================================================
================================================================================
Executing test: super_super_light...
Reference binary: ./runtasks_ref_linux
Results for: super_super_light
                                        STUDENT   REFERENCE   PERF?
[Serial]                                8.519     12.275      0.69  (OK)
[Parallel + Thread Pool + Sleep]        29.887    46.432      0.64  (OK)
================================================================================
Executing test: super_super_light_async...
Reference binary: ./runtasks_ref_linux
Results for: super_super_light_async
                                        STUDENT   REFERENCE   PERF?
[Serial]                                8.413     12.287      0.68  (OK)
[Parallel + Thread Pool + Sleep]        15.982    24.708      0.65  (OK)
================================================================================
Executing test: super_light...
Reference binary: ./runtasks_ref_linux
Results for: super_light
                                        STUDENT   REFERENCE   PERF?
[Serial]                                79.056    90.503      0.87  (OK)
[Parallel + Thread Pool + Sleep]        60.706    59.286      1.02  (OK)
================================================================================
Executing test: super_light_async...
Reference binary: ./runtasks_ref_linux
Results for: super_light_async
                                        STUDENT   REFERENCE   PERF?
[Serial]                                78.752    93.645      0.84  (OK)
[Parallel + Thread Pool + Sleep]        46.706    48.718      0.96  (OK)
================================================================================
Executing test: ping_pong_equal...
Reference binary: ./runtasks_ref_linux
Results for: ping_pong_equal
                                        STUDENT   REFERENCE   PERF?
[Serial]                                1324.494  1506.428    0.88  (OK)
[Parallel + Thread Pool + Sleep]        398.311   441.353     0.90  (OK)
================================================================================
Executing test: ping_pong_equal_async...
Reference binary: ./runtasks_ref_linux
Results for: ping_pong_equal_async
                                        STUDENT   REFERENCE   PERF?
[Serial]                                1550.31   1726.468    0.90  (OK)
[Parallel + Thread Pool + Sleep]        536.815   615.742     0.87  (OK)
================================================================================
Executing test: ping_pong_unequal...
Reference binary: ./runtasks_ref_linux
Results for: ping_pong_unequal
                                        STUDENT   REFERENCE   PERF?
[Serial]                                2680.887  2732.8      0.98  (OK)
[Parallel + Thread Pool + Sleep]        886.001   910.192     0.97  (OK)
================================================================================
Executing test: ping_pong_unequal_async...
Reference binary: ./runtasks_ref_linux
Results for: ping_pong_unequal_async
                                        STUDENT   REFERENCE   PERF?
[Serial]                                2687.128  2764.332    0.97  (OK)
[Parallel + Thread Pool + Sleep]        875.282   893.719     0.98  (OK)
================================================================================
Executing test: recursive_fibonacci...
Reference binary: ./runtasks_ref_linux
Results for: recursive_fibonacci
                                        STUDENT   REFERENCE   PERF?
[Serial]                                1726.216  2264.539    0.76  (OK)
[Parallel + Thread Pool + Sleep]        583.149   775.575     0.75  (OK)
================================================================================
Executing test: recursive_fibonacci_async...
Reference binary: ./runtasks_ref_linux
Results for: recursive_fibonacci_async
                                        STUDENT   REFERENCE   PERF?
[Serial]                                1717.786  2190.635    0.78  (OK)
[Parallel + Thread Pool + Sleep]        582.04    765.999     0.76  (OK)
================================================================================
Executing test: math_operations_in_tight_for_loop...
Reference binary: ./runtasks_ref_linux
Results for: math_operations_in_tight_for_loop
                                        STUDENT   REFERENCE   PERF?
[Serial]                                1042.031  1008.215    1.03  (OK)
[Parallel + Thread Pool + Sleep]        639.439   719.384     0.89  (OK)
================================================================================
Executing test: math_operations_in_tight_for_loop_async...
Reference binary: ./runtasks_ref_linux
Results for: math_operations_in_tight_for_loop_async
                                        STUDENT   REFERENCE   PERF?
[Serial]                                945.107   989.953     0.95  (OK)
[Parallel + Thread Pool + Sleep]        585.924   602.104     0.97  (OK)
================================================================================
Executing test: math_operations_in_tight_for_loop_fewer_tasks...
Reference binary: ./runtasks_ref_linux
Results for: math_operations_in_tight_for_loop_fewer_tasks
                                        STUDENT   REFERENCE   PERF?
[Serial]                                949.832   993.658     0.96  (OK)
[Parallel + Thread Pool + Sleep]        600.664   673.685     0.89  (OK)
================================================================================
Executing test: math_operations_in_tight_for_loop_fewer_tasks_async...
Reference binary: ./runtasks_ref_linux
Results for: math_operations_in_tight_for_loop_fewer_tasks_async
                                        STUDENT   REFERENCE   PERF?
[Serial]                                973.477   988.745     0.98  (OK)
[Parallel + Thread Pool + Sleep]        299.371   368.859     0.81  (OK)
================================================================================
Executing test: math_operations_in_tight_for_loop_fan_in...
Reference binary: ./runtasks_ref_linux
Results for: math_operations_in_tight_for_loop_fan_in
                                        STUDENT   REFERENCE   PERF?
[Serial]                                513.92    532.803     0.96  (OK)
[Parallel + Thread Pool + Sleep]        194.392   258.495     0.75  (OK)
================================================================================
Executing test: math_operations_in_tight_for_loop_fan_in_async...
Reference binary: ./runtasks_ref_linux
Results for: math_operations_in_tight_for_loop_fan_in_async
                                        STUDENT   REFERENCE   PERF?
[Serial]                                484.185   497.827     0.97  (OK)
[Parallel + Thread Pool + Sleep]        140.66    184.051     0.76  (OK)
================================================================================
Executing test: math_operations_in_tight_for_loop_reduction_tree...
Reference binary: ./runtasks_ref_linux
Results for: math_operations_in_tight_for_loop_reduction_tree
                                        STUDENT   REFERENCE   PERF?
[Serial]                                524.883   524.995     1.00  (OK)
[Parallel + Thread Pool + Sleep]        153.145   201.542     0.76  (OK)
================================================================================
Executing test: math_operations_in_tight_for_loop_reduction_tree_async...
Reference binary: ./runtasks_ref_linux
Results for: math_operations_in_tight_for_loop_reduction_tree_async
                                        STUDENT   REFERENCE   PERF?
[Serial]                                544.436   542.025     1.00  (OK)
[Parallel + Thread Pool + Sleep]        148.916   212.23      0.70  (OK)
================================================================================
Executing test: spin_between_run_calls...
Reference binary: ./runtasks_ref_linux
Results for: spin_between_run_calls
                                        STUDENT   REFERENCE   PERF?
[Serial]                                617.51    775.054     0.80  (OK)
[Parallel + Thread Pool + Sleep]        339.995   484.251     0.70  (OK)
================================================================================
Executing test: spin_between_run_calls_async...
Reference binary: ./runtasks_ref_linux
Results for: spin_between_run_calls_async
                                        STUDENT   REFERENCE   PERF?
[Serial]                                599.196   761.259     0.79  (OK)
[Parallel + Thread Pool + Sleep]        331.343   453.562     0.73  (OK)
================================================================================
Executing test: mandelbrot_chunked...
Reference binary: ./runtasks_ref_linux
Results for: mandelbrot_chunked
                                        STUDENT   REFERENCE   PERF?
[Serial]                                563.182   577.487     0.98  (OK)
[Parallel + Thread Pool + Sleep]        89.698    93.976      0.95  (OK)
================================================================================
Executing test: mandelbrot_chunked_async...
Reference binary: ./runtasks_ref_linux
Results for: mandelbrot_chunked_async
                                        STUDENT   REFERENCE   PERF?
[Serial]                                579.105   565.807     1.02  (OK)
[Parallel + Thread Pool + Sleep]        93.104    94.314      0.99  (OK)
================================================================================
Overall performance results
[Serial]                                : All passed Perf
[Parallel + Thread Pool + Sleep]        : All passed Perf
```

