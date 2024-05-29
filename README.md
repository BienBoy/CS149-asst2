# 作业 2：从零开始构建一个任务执行库

**总分：100分**

## 概述

每个人都喜欢快速完成任务，在这个作业中，我们要求你做到这一点！你将实现一个 C++ 库，该库能够在多核 CPU 上尽可能高效地执行应用程序提供的任务。

在作业的第一部分中，你将实现一个任务执行库，该版本支持批量（数据并行）启动相同任务的多个实例。这一功能类似于你在作业 1 中使用的 [ISPC 任务启动行为](http://ispc.github.io/ispc.html#task-parallelism-launch-and-sync-statements)，用于在多个核心上并行化代码。

在作业的第二部分中，你将扩展你的任务运行时系统，以执行更复杂的_任务图_，其中任务的执行可能依赖于其他任务生成的结果。这些依赖关系限制了任务调度系统可以安全地并行运行哪些任务。在并行机器上调度数据并行任务图的执行是许多流行的并行运行时系统的一个特性，从流行的 [Thread Building Blocks](https://github.com/intel/tbb) 库，到 [Apache Spark](https://spark.apache.org/)，再到 [PyTorch](https://pytorch.org/) 和 [TensorFlow](https://www.tensorflow.org/) 等现代深度学习框架，都有这个特性。

这个作业将要求你：

* 使用线程池管理任务执行
* 使用同步原语（如互斥锁和条件变量）协调工作线程的执行
* 实现反映任务图定义的依赖关系的任务调度程序
* 理解工作负载特性以做出高效的任务调度决策

### 等等，我以前做过这个吗？

你可能已经在 CS107 或 CS111 课程中创建过线程池和任务执行库。然而，目前的作业是一个更好地理解这些系统的独特机会。你将实现多个任务执行库，其中一些没有线程池，一些有不同类型的线程池。通过实现多种任务调度策略并比较它们在不同工作负载上的性能，你将更好地理解在创建并行系统时关键设计选择的影响。

## 环境设置

**我们将在 Amazon AWS `m6i.2xlarge` 实例上评分此作业——我们提供了设置虚拟机的说明，请参阅 [这里](https://github.com/stanford-cs149/asst2/blob/master/cloud_readme.md)。请确保你的代码在这个虚拟机上可以运行，因为我们将使用它进行性能测试和评分。**

作业的起始代码可在 [Github](https://github.com/stanford-cs149/asst2) 上获得。请使用以下命令克隆作业 2 的起始代码：

    git clone https://github.com/stanford-cs149/asst2.git

**重要：** 请不要修改提供的 `Makefile`。这样做可能会破坏我们的评分脚本。

## A 部分：同步批量任务启动

在作业 1 中，你使用了 ISPC 的任务启动原语来启动 N 个 ISPC 任务实例（`launch[N] myISPCFunction()`）。在本作业的第一部分中，你将在你的任务执行库中实现类似的功能。

首先，熟悉`itasksys.h`中的`ITaskSystem`定义。这个[抽象类](https://www.tutorialspoint.com/cplusplus/cpp_interfaces.htm)定义了任务执行系统的接口。该接口具有一个名为`run()`的方法，其签名如下：

```cpp
virtual void run(IRunnable* runnable, int num_total_tasks) = 0;
```

`run()`方法执行指定任务的`num_total_tasks`个实例。由于这个单一函数调用会导致多个任务的执行，我们将调用`run()`称为_批量任务启动_。

`tasksys.cpp`中的起始代码包含一个正确的串行`TaskSystemSerial::run()`实现，该实现用作任务系统使用`IRunnable`接口执行批量任务启动的示例。（`IRunnable`的定义在`itasksys.h`中）请注意，在每次调用`IRunnable::runTask()`时，任务系统会为任务提供一个当前任务标识符（一个介于 0 和`num_total_tasks`之间的整数）以及批量任务启动中的总任务数。任务的实现将使用这些参数来确定任务应执行的工作。

`run()`的一个重要细节是它必须相对于调用线程同步地执行任务。换句话说，当调用`run()`返回时，应用程序可以确保任务系统已经完成了批量任务启动中的**所有任务**。起始代码中提供的串行实现在调用线程上执行所有任务，因此满足此要求。

### 运行测试

起始代码包含一个使用你的任务系统的测试应用程序套件。有关测试工具的描述，请参见`tests/README.md`，有关测试定义本身，请参见`tests/tests.h`。要运行测试，请使用`runtasks`脚本。例如，名为`mandelbrot_chunked`的测试使用批量任务启动计算一个 Mandelbrot 分形图，每个任务处理图像的一个连续块，要运行该测试请使用如下命令：

```bash
./runtasks -n 8 mandelbrot_chunked
```

不同的测试具有不同的性能特征——有些任务执行的工作量很小，有些任务则会进行大量的处理。有些测试在每次启动中创建大量任务，有些测试则很少。某些批量启动中的任务都具有相似的计算成本，某些批量启动中的任务成本则有所不同。我们在`tests/README.md`中描述了大多数测试，但我们鼓励你检查`tests/tests.h`中的代码，以更详细地了解所有测试的行为。

在实现你的解决方案时，一个可能有助于调试正确性的测试是`simple_test_sync`，这是一个非常小的测试，不应用于测量性能，但足够小，可以使用打印语句或调试器进行调试。参见`tests/tests.h`中的`simpleTest`函数。

我们鼓励你创建自己的测试。请参考`tests/tests.h`中的现有测试以获取灵感。如果你选择这样做，我们还提供了一个由`class YourTask`和`yourTest()`函数组成的框架测试，可以在其基础上进行构建。对于你创建的测试，请确保将它们添加到`tests/main.cpp`中的测试列表和测试名称中，并相应地调整变量`n_tests`。请注意，虽然你可以使用你的解决方案运行自己的测试，但你将无法编译参考解决方案以运行你的测试。

`-n`命令行选项指定任务系统实现可以使用的最大线程数。在上面的示例中，我们选择`-n 8`是因为 AWS 实例中的 CPU 具有 8 个执行上下文。可通过命令行帮助（`-h`命令行选项）获得可运行的测试的完整列表。

`-i`命令行选项指定在性能测量期间运行测试的次数。为了准确测量性能，`./runtasks`多次运行测试并记录几次运行中的_最小_运行时间；一般来说，默认值就足够了——较大的值可能会提供更准确的测量，但代价是测试运行时间更长。

此外，我们还为你提供了用于评分性能的测试工具：

```bash
>>> python ../tests/run_test_harness.py
```

测试工具具有以下命令行参数，

```bash
>>> python run_test_harness.py -h
usage: run_test_harness.py [-h] [-n NUM_THREADS]
                           [-t TEST_NAMES [TEST_NAMES ...]] [-a]

Run task system performance tests

optional arguments:
  -h, --help            show this help message and exit
  -n NUM_THREADS, --num_threads NUM_THREADS
                        Max number of threads that the task system can use. (8
                        by default)
  -t TEST_NAMES [TEST_NAMES ...], --test_names TEST_NAMES [TEST_NAMES ...]
                        List of tests to run
  -a, --run_async       Run async tests
```

它会生成一个详细的性能报告，如下所示：

```bash
>>> python ../tests/run_test_harness.py -t super_light super_super_light
python ../tests/run_test_harness.py -t super_light super_super_light
================================================================================
Running task system grading harness... (2 total tests)
  - Detected CPU with 8 execution contexts
  - Task system configured to use at most 8 threads
================================================================================
================================================================================
Executing test: super_super_light...
Reference binary: ./runtasks_ref_linux
Results for: super_super_light
                                        STUDENT   REFERENCE   PERF?
[Serial]                                9.053     9.022       1.00  (OK)
[Parallel + Always Spawn]               8.982     33.953      0.26  (OK)
[Parallel + Thread Pool + Spin]         8.942     12.095      0.74  (OK)
[Parallel + Thread Pool + Sleep]        8.97      8.849       1.01  (OK)
================================================================================
Executing test: super_light...
Reference binary: ./runtasks_ref_linux
Results for: super_light
                                        STUDENT   REFERENCE   PERF?
[Serial]                                68.525    68.03       1.01  (OK)
[Parallel + Always Spawn]               68.178    40.677      1.68  (NOT OK)
[Parallel + Thread Pool + Spin]         67.676    25.244      2.68  (NOT OK)
[Parallel + Thread Pool + Sleep]        68.464    20.588      3.33  (NOT OK)
================================================================================
Overall performance results
[Serial]                                : All passed Perf
[Parallel + Always Spawn]               : Perf did not pass all tests
[Parallel + Thread Pool + Spin]         : Perf did not pass all tests
[Parallel + Thread Pool + Sleep]        : Perf did not pass all tests
```

在上述输出中，`PERF`是你的实现运行时间与参考解决方案运行时间的比值。因此，值小于 1 表示你的任务系统实现比参考实现更快。

**Mac 用户：虽然我们提供了用于 A 部分和 B 部分的参考解决方案的二进制文件，但我们将使用 Linux 二进制文件测试你的代码。因此，我们建议你在提交前检查你的实现是否能在 AWS 实例中运行。如果你使用的是带有 M1 芯片的新款 Mac，请在本地测试时使用`runtasks_ref_osx_arm`二进制文件。否则，请使用`runtasks_ref_osx_x86`二进制文件。**

### 你需要做什么

你的任务是实现一个能够高效利用多核 CPU 的任务执行引擎。你的评分将基于实现的正确性（它必须正确运行所有任务）以及其性能。这将是一个有趣的编码挑战，但它是一项不小的工作。为了帮助你保持正确的方向——完成作业的 A 部分，我们将让你实现多个版本的任务系统，逐步增加你的实现的复杂性和性能。你的三个实现将在`tasksys.cpp/.h`中定义的类中完成。

- `TaskSystemParallelSpawn`
- `TaskSystemParallelThreadPoolSpinning`
- `TaskSystemParallelThreadPoolSleeping`

**请在`part_a/`子目录中实现你的 A 部分实现，以与正确的参考实现（`part_a/runtasks_ref_*`）进行比较。**

_提示：注意以下说明如何采用“try the simplest improvement first”的方法。每一步都增加了任务执行系统实现的复杂性，但在每一步中你应该都有一个可运行（完全正确）的任务运行时系统。_

我们还期望你创建至少一个测试，该测试可以测试正确性或性能。有关更多信息，请参阅上面的运行测试部分。

#### 步骤 1：迁移到并行任务系统

**在此步骤中，请实现`TaskSystemParallelSpawn`类。**

起始代码为你提供了一个工作正常的串行任务系统实现`TaskSystemSerial`。在此步骤中，你将扩展起始代码以并行执行批量任务启动。

- 你需要创建额外的控制线程以执行批量任务启动的工作。请注意，`TaskSystem`的构造函数提供了一个参数`num_threads`，这是你的实现可用于运行任务的**最大工作线程数**。
- 本着“do the simplest thing first”的精神，我们建议你在`run()`的开始生成工作线程，并在`run()`返回之前从主线程合并这些线程。这将是一个正确的实现，但将因频繁的线程创建而产生显著的开销。
- 如何将任务分配给工作线程？应该考虑静态还是动态分配任务给线程？
- 是否有共享变量（任务执行系统的内部状态）需要保护，以免多个线程同时访问？你可能需要查看我们的 [C++ 同步教程](https://chatgpt.com/c/tutorial/README.md)以获取有关 C++ 标准库中同步原语的更多信息。

#### 步骤 2：使用线程池避免频繁的线程创建

**在此步骤中，请实现`TaskSystemParallelThreadPoolSpinning`类。**

步骤 1 中的实现会因为每次调用`run()`时创建线程而产生开销。当任务计算成本较低时，这种开销尤为明显。此时，建议采用“线程池”实现，其中任务执行系统在前期创建所有工作线程（例如，在`TaskSystem`构造期间，或在第一次调用`run()`时）。

- 作为初始实现，建议将工作线程设计为连续循环，始终检查是否有更多工作要执行。（线程进入 while 循环直到条件为真，通常称为“自旋/忙等（spinning/busy-waiting）”。）工作线程如何确定是否有工作要做？

- 现在，确保 `run()` 实现所需的同步行为并不简单。你需要如何更改`run()`的实现，以确定批量任务启动中的所有任务都已完成？

#### 步骤 3：在没有工作时让线程进入休眠

**在此步骤中，请实现`TaskSystemParallelThreadPoolSleeping`类。**

步骤 2 中的实现有一个缺点，即线程在“自旋”等待有工作要做时，会占用 CPU 核心的执行资源。例如，工作线程可能会循环等待新任务到来。另一个例子是，主线程可能会循环等待工作线程完成所有任务，以便从`run()`调用返回。这会影响性能，因为即使这些线程没有执行有用的工作，也会占用 CPU 资源。

在这部分作业中，我们希望您让线程进入睡眠状态，直到满足它们正在等待的条件，从而提高任务系统的效率。

- 你的实现可以选择使用条件变量来实现此行为。条件变量是一种同步原语，使线程在等待条件存在时进入休眠（不占用 CPU 处理资源）。其他线程向等待线程发出“信号”唤醒它们，以查看它们等待的条件是否已得到满足。例如，如果没有工作要做，你的工作线程可以进入睡眠状态（这样它们不会从尝试执行有用工作的线程中夺走 CPU 资源）。另一个例子是，调用`run()`的主应用线程可能希望在等待批量任务启动中的所有任务完成时进入休眠（否则自旋的主线程会占用工作线程的CPU资源！）。有关 C++ 中条件变量的更多信息，请参阅我们的 [C++ 同步教程](tutorial/README.md)。

- 在本作业部分的实现中，你可能需要考虑棘手的竞争条件。你需要考虑多种可能的线程行为交错。

- 你可能需要考虑编写额外的测试用例来测试你的系统。**作业起始代码包括评分脚本将用于对代码性能进行评分的工作负载，但我们还将使用更广泛的工作负载集来测试你的实现的正确性，这些工作负载集未包含在起始代码中！**

## B 部分：支持任务图的执行

在作业的 B 部分，你将扩展 A 部分的任务系统实现，以支持可能依赖于先前任务的异步任务启动。这些任务之间的依赖关系会创建你的任务执行库必须遵守的调度约束。

`ITaskSystem`接口有一个额外的方法：

```cpp
virtual TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps) = 0;
```

`runAsyncWithDeps()`类似于`run()`，它也用于执行`num_total_tasks`个任务的批量启动。然而，它在多个方面不同于`run()`……

#### 异步任务启动

首先，使用`runAsyncWithDeps()`创建的任务与调用线程_异步_执行。这意味着`runAsyncWithDeps()`应立即返回给调用者，即使任务尚未完成执行。该方法返回与此批量任务启动相关的唯一标识符。

调用线程可以通过调用`sync()`确定批量任务启动何时实际完成。

```cpp
virtual void sync() = 0;
```

仅在所有先前批量任务启动相关的任务都完成时，`sync()`才返回调用者。例如，考虑以下代码：

```cpp
// 假设 taskA 和 taskB 是有效的 IRunnable 实例...

std::vector<TaskID> noDeps;  // 空向量

ITaskSystem *t = new TaskSystem(num_threads);

// 批量启动 4 个任务
TaskID launchA = t->runAsyncWithDeps(taskA, 4, noDeps);

// 批量启动 8 个任务
TaskID launchB = t->runAsyncWithDeps(taskB, 8, noDeps);

// 此时与 launchA 和 launchB 相关的任务可能仍在运行

t->sync();

// 此时与 launchA 和 launchB 相关的所有 12 个任务都已终止
```

如上面的注释所述，在调用线程调用`sync()`之前，不能保证之前调用`runAsyncWithDeps()`的任务已经完成。确切地说，`runAsyncWithDeps()`告诉你的任务系统执行新的批量任务启动，但你的实现可以在下次调用`sync()`之前的任何时间执行这些任务。注意，这意味着你的实现不保证在启动`launchB`任务之前执行`launchA`任务！

#### 支持显式依赖关系

`runAsyncWithDeps()`的第二个关键细节是它的第三个参数：一个`TaskID`标识符 vector，它必须引用之前使用`runAsyncWithDeps()`启动的批量任务。该 vector 指定当前批量任务启动中的任务依赖于哪些先前任务。**因此，你的任务运行时在开始执行当前批量任务启动中的任何任务之前，必须完成依赖 vector 中指定的所有任务！**例如，考虑以下示例：

```cpp
std::vector<TaskID> noDeps;  // 空向量
std::vector<TaskID> depOnA;
std::vector<TaskID> depOnBC;

ITaskSystem *t = new TaskSystem(num_threads);

TaskID launchA = t->runAsyncWithDeps(taskA, 128, noDeps);
depOnA.push_back(launchA);

TaskID launchB = t->runAsyncWithDeps(taskB, 2, depOnA);
TaskID launchC = t->runAsyncWithDeps(taskC, 6, depOnA);
depOnBC.push_back(launchB);
depOnBC.push_back(launchC);

TaskID launchD = t->runAsyncWithDeps(taskD, 32, depOnBC);
t->sync();
```

上面的代码包含四个批量任务启动（taskA：128个任务，taskB：2个任务，taskC：6个任务，taskD：32个任务）。注意，taskB 和 task C的启动依赖于 taskA。`launchD`（taskD 的批量启动）依赖于`launchB`和`launchC`的结果。因此，尽管你的任务运行时可以以任何顺序（包括并行）处理与`launchB`和`launchC`相关的任务，但这些任务必须在`launchA`的任务完成后开始执行，并且必须在运行时开始执行`launchD`的任何任务之前完成。

我们可以将这些依赖关系可视化为一个**任务图**。任务图是一个有向无环图（DAG），图中的结点对应批量任务启动，节点 X 到节点 Y 的边表示 Y 依赖于 X 的输出。上面代码的任务图如下：

<p align="center">
    <img src="figs/task_graph.png" width=400>
</p>

注意，如果你在具有八个执行上下文的机器上运行上面的示例，能够并行调度`launchB`和`launchC`的任务可能非常有用，因为单独一个批量任务启动不足以使用机器的所有执行资源。

### 测试

所有后缀为`Async`的测试都应该用于测试 B 部分。评分工具中包含的测试子集在`tests/README.md`中描述，所有测试都可以在`tests/tests.h`中找到，并在`tests/main.cpp`中列出。为了调试正确性，我们提供了一个小测试`simple_test_async`。请查看`tests/tests.h`中的`simpleTest`函数。`simple_test_async`应该足够小，可以使用 print 语句或在`simpleTest`中设置断点进行调试。

我们鼓励你创建自己的测试。请参考`tests/tests.h`中的现有测试以获取灵感。如果你选择这样做，我们还提供了一个由`class YourTask`和`yourTest()`函数组成的框架测试，可以在其基础上进行构建。对于你创建的测试，请确保将它们添加到`tests/main.cpp`中的测试列表和测试名称中，并相应地调整变量`n_tests`。请注意，虽然你可以使用你的解决方案运行自己的测试，但你将无法编译参考解决方案以运行你的测试。

### 你需要做什么

你必须扩展你在 A 部分使用线程池（和休眠）的任务系统实现，以正确实现`TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps()`和`TaskSystemParallelThreadPoolSleeping::sync()`。我们还期望你创建至少一个测试，可以测试正确性或性能。有关更多信息，请参阅上面的`测试`部分。作为澄清，你*需要*在报告中描述你自己的测试，但我们的自动评分程序将*不会*测试你的测试。

**你不需要在 B 部分中实现其他`TaskSystem` 类。**

与 A 部分一样，我们为你提供以下提示以帮助你开始：

- 将`runAsyncWithDeps()`的行为视为将批量任务启动对应的记录，或将批量任务启动中的每个任务对应的记录推送到“工作队列”中可能会有所帮助。一旦工作记录进入队列，`runAsyncWithDeps()`就可以返回给调用者。

- 本作业的难点在于执行适当的记录来跟踪依赖关系。在批量任务启动中的所有任务完成时必须做什么？（此时可能有新任务可以运行。）

- 在你的实现中有两个数据结构可能会有所帮助：（1）一个结构体，用于表示通过调用`runAsyncWithDeps()`添加到系统中的任务，但该任务由于依赖于仍在运行的任务而尚未准备好执行（这些任务正在“等待”其他任务完成），（2）一个“就绪队列”，其中的任务不依赖于任何先前任务的完成，可以在有可用的工作线程处理时安全地运行。

- 生成唯一的任务启动 ID 时不必担心整数溢出。我们不会对你的任务系统进行超过 $2^31$ 次批量任务启动的测试。

- 你可以假设所有程序要么只调用 `run()`，要么只调用`runAsyncWithDeps()`；也就是说，你不需要处理`run()`调用需要等待所有前面的`runAsyncWithDeps()`调用完成的情况。注意，这一假设意味着你可以使用适当的`runAsyncWithDeps()`和`sync()`调用来实现 `run()`。

- 你可以假设唯一的多线程操作是由你的实现创建/使用的多个线程。也就是说，我们不会生成其他线程并从这些线程调用你的实现。

**请在 `part_b/` 子目录中实现你的 B 部分实现，以与正确的参考实现（`part_b/runtasks_ref_*`）进行比较。**

## 评分标准

本次作业的分数分配如下：

**A部分（50分）**

- `TaskSystemParallelSpawn::run()`的正确性 5 分 + 性能 5 分。（共 10 分）
- `TaskSystemParallelThreadPoolSpinning::run()`和`TaskSystemParallelThreadPoolSleeping::run()`的正确性各 10 分 + 这些方法的性能各 10 分。（共40分）

**B部分（40分）**

- `TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps()`、`TaskSystemParallelThreadPoolSleeping::run()`和`TaskSystemParallelThreadPoolSleeping::sync()`的正确性 30 分。
- `TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps()`、`TaskSystemParallelThreadPoolSleeping::run()`和`TaskSystemParallelThreadPoolSleeping::sync()`的性能 10 分。对于 B 部分，你可以忽略`Parallel + Always Spawn`和`Parallel + Thread Pool + Spin`的结果。也就是说，你只需要通过每个测试用例的`Parallel + Thread Pool + Sleep`部分即可。

**报告（10分）**

- 请参阅“提交”部分以获取更多详细信息。

对于每个测试，若实现性能在提供的参考实现的20%以内，将授予满分的性能分数。只有返回正确答案的实现才能获得性能分数。如前所述，我们可能还会使用起始代码中未提供的更广泛的工作负载集来测试你实现的*正确性*。

## 提交

请使用 [Gradescope](https://www.gradescope.com/) 提交你的作业。你的提交应包括任务系统代码和描述你实现的报告。需要提交以下五个文件：

- part_a/tasksys.cpp
- part_a/tasksys.h
- part_b/tasksys.cpp
- part_b/tasksys.h
- 你的报告 PDF（提交到 Gradescope 的报告作业）

#### 代码提交

我们要求你提交`part_a/tasksys.cpp|.h`和`part_b/tasksys.cpp|.h`的源文件压缩包。你可以创建一个目录（例如命名为`asst2_submission`），其中包含子目录`part_a`和`part_b`，将相关文件放入其中，然后运行`tar -czvf asst2.tar.gz asst2_submission`压缩目录并上传。请将**压缩文件**`asst2.tar.gz`提交到 Gradescope 的 *Assignment 2 (Code)* 部分。

在提交源文件之前，确保所有代码可以编译和运行！我们应该能够将这些文件放入一个干净的起始代码树中，输入 `make`，然后无需人工干预地执行你的程序。

我们的评分脚本将运行起始代码中提供的检查代码以确定性能分数。*我们还将在起始代码中未提供的其他应用程序上运行您的代码，以进一步测试其正确性！*评分脚本将在作业截止后运行。

#### 报告提交

请提交一份简短的报告到 Gradescope 的 *Assignment 2 (Write-up)* 部分，内容包括：

1. 描述你的任务系统实现（1 页即可）。除了对其工作原理的一般描述外，请确保回答以下问题：
   - 你是如何管理线程的？（例如，你是否实现了一个线程池？）
   - 你的系统如何将任务分配给工作线程？你使用了静态还是动态分配？
   - 你在 B 部分如何跟踪依赖关系以确保任务图的正确执行？

2. 在 A 部分，你可能注意到更简单的任务系统实现（例如，完全串行实现或每次启动都生成线程的实现）表现得和更高级的实现一样好，有时甚至更好。请解释为什么会出现这种情况，并引用某些测试作为例子。例如，在什么情况下，串行任务系统实现表现最好？为什么？在什么情况下，每次启动生成线程的实现表现得和使用线程池的更高级的并行实现一样好？何时不是这样？

3. 描述你为本次作业实现的一个测试。测试的内容是什么，它要检查什么，以及你如何验证你的作业解决方案在你的测试中表现良好？你添加的测试结果是否导致你更改了作业实现？
