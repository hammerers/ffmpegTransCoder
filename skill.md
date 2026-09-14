# C++ SDK Engineering Skill

## 0. Skill Identity

**名称：** `cpp-sdk-engineering`
**目标：** 指导 Agent 设计、实现、评审、调试、测试、打包和发布 C/C++ SDK。
**适用范围：** C++ SDK、C ABI SDK、动态库/静态库、跨平台 SDK、设备/网络/算法/系统类 SDK、供第三方程序集成的 C++ 库。

---

# 1. Core Mission

Agent 必须始终把 SDK 视为：

> **“面向外部开发者的长期稳定软件产品，而不是普通内部业务代码。”**

因此，Agent 不得只判断“代码能否运行”，必须同时判断：

1. API 是否清晰、稳定、易用。
2. ABI 是否稳定、边界是否安全。
3. Ownership / Lifetime 是否明确。
4. Thread Safety 是否明确并可证明。
5. Error Handling 是否完整。
6. Build / Install / Package 是否可消费。
7. Dependency 是否可控。
8. Versioning / Compatibility 是否可演进。
9. Documentation / Examples 是否足够。
10. Testing / Sanitizer / CI 是否覆盖关键风险。
11. Security / License / Release 是否满足工程要求。

**默认优先级：**

`正确性 > ABI/API 稳定性 > 安全性 > 生命周期/线程安全 > 可维护性 > 性能 > 代码简洁度`

---

# 2. Non-Negotiable Rules

以下规则视为强制规范。除非项目明确记录了例外，Agent 不得主动违反。

## 2.1 Public API First

任何进入公共 Header 的内容都视为长期合同。

Agent 必须：

- 最小化 Public API。
- 隐藏内部实现。
- 避免把第三方库类型暴露到 Public API。
- 避免把内部类、线程池、数据库、网络层等实现类型暴露出去。
- 所有公开接口都必须定义参数、返回值、错误、线程安全、生命周期、所有权和阻塞语义。

## 2.2 ABI Boundary Must Be Deliberate

Agent 必须识别并明确 SDK 的 ABI Boundary：

`Application <-> SDK Binary`

边界设计应尽量简单、稳定、可验证。

优先考虑：

- 基本整数/浮点类型
- enum
- C-compatible struct
- opaque handle
- pointer + size
- 明确的 allocation/free API

谨慎或避免直接跨边界：

- `std::string`
- `std::vector`
- `std::map`
- `std::unordered_map`
- `std::shared_ptr`
- `std::unique_ptr`
- 自定义 STL 容器
- 复杂模板类型
- 带虚函数的 ABI 类型
- 依赖具体编译器布局的类型
- 由第三方库定义且用户未必拥有相同版本的类型

## 2.3 Ownership Must Be Explicit

所有 pointer / buffer / handle / resource API 必须说明：

- 谁创建？
- 谁拥有？
- 谁销毁？
- 什么时候失效？
- 能否跨线程？
- SDK 是否持有引用？
- 是否可以修改？
- 调用后是否仍有效？

禁止设计含糊接口。

## 2.4 Thread Safety Must Be Explicit

不得只写“thread-safe”。

必须指出：

- 哪些 API thread-safe。
- 哪些 API 只保证不同实例并发。
- 同一个对象是否可并发调用。
- callback 在哪个线程执行。
- callback 内是否允许重入 SDK。
- shutdown / destroy 是否可与其他 API 并发。
- 是否存在后台线程。
- 线程什么时候启动/退出/回收。
- 是否存在阻塞、锁、deadlock 风险。

## 2.5 Lifecycle Must Be Explicit

SDK 对象必须有清晰状态机，例如：

`Created -> Initialized -> Running -> Stopped -> Destroyed`

Agent 必须处理：

- 重复 init
- 重复 start
- 重复 stop
- destroy 后调用
- shutdown 时后台线程
- callback 尚未返回时 destroy
- 异常路径下资源释放
- partial initialization

优先设计幂等的 `stop()/shutdown()/close()`。

## 2.6 Error Handling Must Be Deterministic

每个可能失败的公共 API 必须有明确错误模型：

- return code
- `Result<T>`
- `ErrorCode`
- exception
- callback error

跨 ABI/C API 时优先考虑稳定的 Error Code。

禁止通过无语义的 `-1`、`nullptr` 等返回值让用户猜错误。

## 2.7 Build Must Be Consumer-Friendly

SDK 必须能被外部项目以标准方式消费。

优先支持：

```cmake
find_package(MySDK CONFIG REQUIRED)
target_link_libraries(app PRIVATE MySDK::MySDK)
```

禁止要求用户长期依赖：

- 手工 `include_directories`
- 手工 `link_directories`
- 猜 DLL/SO 路径
- 手工复制大量内部文件
- 直接引用源码目录内部结构

---

# 3. API Design Rules

## 3.1 Public API Design

公共接口必须：

- 小而稳定。
- 命名一致。
- 遵循项目已有风格。
- 不泄露内部实现。
- 提供合理默认值。
- 对非法参数做检查。
- 明确 null / empty / zero / negative 的语义。
- 说明是否 blocking。
- 说明 timeout。
- 说明是否 reentrant。
- 说明线程语义。

## 3.2 Naming

不要混用：

`initSDK()` / `initializeSdk()` / `SDKStart()` / `start_sdk()`

项目必须统一命名风格。

C API 建议使用稳定前缀：

```c
sdk_init();
sdk_shutdown();
sdk_create();
sdk_destroy();
```

C++ API 可以：

```cpp
initialize();
shutdown();
create();
destroy();
```

## 3.3 Namespace

C++ Public API 应使用项目 namespace：

```cpp
namespace company::sdk {
class Client {};
}
```

避免把通用名字直接放到全局命名空间。

## 3.4 Avoid Global State

除非有明确架构需求，否则不要把核心运行状态放入：

- 全局变量
- 静态可变对象
- 单例

原因：

- 多实例困难
- 并发困难
- 测试困难
- 初始化顺序问题
- 资源隔离困难

---

# 4. ABI Rules

## 4.1 Understand ABI

Agent 必须考虑：

- calling convention
- name mangling
- object layout
- struct layout
- alignment
- padding
- vtable
- exception ABI
- RTTI
- compiler ABI
- standard library ABI
- runtime library
- architecture
- optimization / build mode compatibility

## 4.2 ABI Stability

如果 SDK 承诺二进制兼容，必须评估：

- Public class layout 是否改变
- virtual function 是否改变
- struct layout 是否改变
- enum 是否改变
- exported symbols 是否删除/重命名
- exception type 是否改变
- STL ABI 是否暴露
- dependency ABI 是否改变
- compiler/runtime 是否改变

## 4.3 Prefer PImpl

对于大型公开 C++ class，优先：

```cpp
class Client {
public:
    Client();
    ~Client();

    void start();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
```

目的：

- 隐藏实现
- 降低 header 依赖
- 降低 ABI 暴露
- 减少重编译
- 提高版本演进能力

## 4.4 Prefer Opaque Handles for C ABI

例如：

```c
typedef struct sdk_client* sdk_client_t;
```

或者：

```c
typedef void* sdk_handle_t;
```

更推荐有语义的 opaque type，而不是无意义的裸 `void*` 到处传播。

---

# 5. Struct / POD / Data Layout Rules

## 5.1 Prefer Simple Data Layout at ABI Boundary

推荐：

```cpp
struct sdk_point {
    double x;
    double y;
};
```

避免 ABI 边界中的：

- STL 容器
- owning smart pointer
- virtual methods
- complex inheritance
- implementation-defined layout

## 5.2 Alignment and Padding

Agent 必须考虑：

- `sizeof`
- `alignof`
- padding
- member order
- packing
- architecture differences

不得凭“看起来大小正确”判断 ABI。

## 5.3 Struct Versioning

对于长期演进的 C/C++ ABI，优先设计：

```cpp
struct sdk_config {
    uint32_t size;
    uint32_t version;
    uint32_t timeout_ms;
    uint32_t retry_count;
};
```

调用方填入：

```cpp
config.size = sizeof(config);
config.version = SDK_CONFIG_VERSION;
```

SDK 必须根据 `size/version` 判断调用方实际提供了哪些字段。

## 5.4 Do Not Blindly Append Fields

给稳定 ABI struct 直接增加字段可能破坏兼容性。

任何字段新增、删除、重排都必须进行 ABI 评估。

---

# 6. Memory and Resource Management

## 6.1 Ownership Contract

每个返回 pointer / buffer / object 的 API 必须写明：

- borrowed / owned / shared
- valid lifetime
- free method
- mutation rights

## 6.2 Cross-Boundary Allocation

避免：

`SDK allocates -> App frees with unrelated allocator`

推荐：

```c
char* sdk_get_name();
void sdk_free(void* p);
```

或者：

```c
int sdk_get_name(char* buffer, size_t capacity);
```

或者由 caller 提供 buffer。

## 6.3 RAII Internally

SDK 内部优先使用：

- RAII
- `std::unique_ptr`
- `std::lock_guard`
- `std::scoped_lock`
- `std::vector`
- `std::string`

但不意味着这些类型都应该暴露到 ABI Boundary。

## 6.4 No Resource Leak on Error Path

所有：

- file
- socket
- thread
- lock
- device handle
- GPU resource
- memory
- database connection

都必须在异常、错误、取消、destroy 路径中得到正确释放。

---

# 7. Thread Safety and Concurrency

## 7.1 Define a Concurrency Model

可选模型包括：

1. 完全线程安全
2. 同实例线程安全
3. 不同实例可并发
4. 单线程绑定
5. reader/writer 模型
6. callback thread 模型

必须在文档中明确。

## 7.2 Background Thread

如果 SDK 创建后台线程，必须定义：

- 谁启动？
- 谁停止？
- 谁 join？
- shutdown 是否等待？
- callback 是否可能仍执行？
- destroy 后是否仍有工作？
- thread 是否 daemon-like？

## 7.3 Callback Rules

必须明确：

- callback 执行线程
- callback 是否允许阻塞
- callback 是否允许抛异常
- callback 是否允许调用 SDK
- callback 生命周期
- callback 如何取消
- callback 在 shutdown 中的行为

## 7.4 Reentrancy

任何内部调用 callback 的 API，都必须分析：

`lock -> callback -> SDK API -> same lock`

导致 deadlock 的可能性。

## 7.5 Cancellation and Timeout

长时间运行的操作应考虑：

- timeout
- cancellation
- retry
- shutdown interaction

状态必须可区分：

- completed
- failed
- canceled
- timed out

---

# 8. CMake and Build System

## 8.1 Target-Based CMake

优先使用：

```cmake
add_library(MySDK SHARED ...)
target_include_directories(MySDK
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(MySDK
    PRIVATE
        SomeInternalDependency
)
```

避免全局：

```cmake
include_directories(...)
link_directories(...)
add_definitions(...)
```

## 8.2 PUBLIC / PRIVATE / INTERFACE

Agent 必须严格按语义选择：

### PUBLIC
Target 自己需要，使用该 target 的下游也需要。

### PRIVATE
仅 Target 自己需要，不应向下游传播。

### INTERFACE
Target 自己不编译消耗，但下游需要继承。

## 8.3 Dependency Exposure Rule

如果依赖类型出现在 Public Header/API 中，通常需要 PUBLIC/INTERFACE 传播。

如果依赖只存在于 `.cpp` 内部，通常应 PRIVATE。

## 8.4 Compiler Features

不要把编译标准靠全局变量硬塞给消费者。

优先：

```cmake
target_compile_features(MySDK PUBLIC cxx_std_17)
```

只有当项目协议要求时，才让消费者继承。

## 8.5 Build Matrix

SDK 项目应尽可能验证：

- Debug / Release
- Windows / Linux / macOS（按支持范围）
- x64 / arm64（按支持范围）
- GCC / Clang / MSVC（按支持范围）

---

# 9. Install and Package Rules

## 9.1 Install Layout

标准安装通常包含：

```text
include/
lib/
bin/
cmake/
docs/
examples/
licenses/
```

## 9.2 Export CMake Targets

应提供：

- `MySDKConfig.cmake`
- `MySDKTargets.cmake`

典型用户体验：

```cmake
find_package(MySDK CONFIG REQUIRED)
target_link_libraries(app PRIVATE MySDK::MySDK)
```

## 9.3 No Hardcoded Paths

不要在 SDK 包中依赖：

- 开发机绝对路径
- `/home/someone/...`
- `C:\Users\...`
- 临时 build 路径

## 9.4 Runtime Dependency Handling

动态库 SDK 必须说明：

- DLL/SO/ dylib 在哪里
- 运行时如何找到
- 第三方依赖如何部署
- Debug/Release 是否需要不同依赖

---

# 10. Static vs Shared Library Rules

## 10.1 Static

适合：

- 依赖简单
- 希望简化部署
- ABI 不需要独立升级

关注：

- 最终二进制大小
- ODR
- 重复依赖
- 编译时间
- License

## 10.2 Shared

适合：

- 隐藏实现
- 共享代码
- 独立更新
- 降低最终应用体积

必须重点关注：

- ABI
- exported symbols
- runtime dependency
- DLL/SO 搜索路径
- 版本兼容
- crash symbolization

---

# 11. Windows DLL Export Rules

推荐集中定义：

```cpp
#if defined(_WIN32)
  #if defined(MYSDK_BUILD)
    #define MYSDK_API __declspec(dllexport)
  #else
    #define MYSDK_API __declspec(dllimport)
  #endif
#else
  #define MYSDK_API
#endif
```

规则：

1. 只导出 Public API。
2. 内部实现尽量隐藏。
3. 需要时配合 symbol visibility。
4. 不随意让用户依赖内部符号。
5. 如果采用 C ABI，优先 `extern "C"`。
6. 明确 Windows `.lib` 是 import library 还是 static library。
7. 发布 DLL 时同时考虑 debug symbols / PDB。

---

# 12. Exception and Error Rules

## 12.1 Choose One Consistent Public Error Model

禁止在不同 API 中随意混搭：

- `bool`
- `-1`
- exception
- null
- error code

而没有文档解释。

## 12.2 C ABI

优先：

```c
typedef enum {
    SDK_OK = 0,
    SDK_INVALID_ARGUMENT = 1,
    SDK_TIMEOUT = 2,
    SDK_NOT_INITIALIZED = 3,
    SDK_INTERNAL_ERROR = 4
} sdk_error_t;
```

## 12.3 C++ ABI

可使用：

- exception
- `std::error_code`
- `Result<T>`
- project-specific error class

但必须评估 ABI 边界与编译器一致性。

## 12.4 Destructors

公共析构函数、RAII destructor 和 shutdown 路径必须尽量 `noexcept`，避免异常跨析构/边界传播。

---

# 13. Configuration Rules

Config 必须定义：

- 默认值
- 合法范围
- 必填项
- 无效输入行为
- 版本
- 是否线程安全
- 是否可以在运行中修改

优先合理默认配置。

配置验证应在“尽早”阶段完成。

---

# 14. String and Encoding Rules

必须明确：

- UTF-8 / UTF-16 / platform-specific encoding
- null 是否允许
- 是否 NUL-terminated
- length 是否包含终止符
- string ownership
- lifetime

跨平台 SDK 优先采用明确、统一的编码策略。

不要写模糊文档：

> “字符串编码取决于平台。”

除非 SDK 明确就是如此设计。

---

# 15. Dependency Management

Agent 必须分析：

- 第三方库版本
- ABI
- transitive dependencies
- symbol conflicts
- license
- runtime deployment

常见方案：

1. vendor / bundled
2. package manager
3. system dependency
4. statically linked dependency
5. dynamically linked dependency

不能只回答“能编译”，还要回答：

> **最终用户如何部署？**

---

# 16. Third-Party API Exposure

不要轻易这样设计：

```cpp
cv::Mat process(const cv::Mat&);
SSL_CTX* getContext();
grpc::Channel* channel();
```

除非 SDK 明确把对应第三方库作为 Public Contract。

否则优先提供自有抽象：

```cpp
Image process(const Image&);
```

内部再使用 OpenCV / OpenSSL / gRPC。

---

# 17. Logging and Diagnostics

SDK 不应随意：

```cpp
printf(...)
std::cout << ...
```

推荐提供可配置 logger：

- log level
- sink/callback
- enable/disable
- thread behavior

禁止把以下敏感数据无保护写入日志：

- password
- token
- private key
- credential
- personal data
- secret payload

---

# 18. Versioning and Compatibility

## 18.1 Semantic Versioning

默认理解：

`MAJOR.MINOR.PATCH`

通常：

- MAJOR：可能破坏兼容
- MINOR：新增向后兼容功能
- PATCH：修复问题

如项目有自己的版本规则，以项目规则为准。

## 18.2 Compatibility Types

必须区分：

1. Source Compatibility
2. Binary Compatibility
3. Behavioral Compatibility

## 18.3 Deprecation

不要轻易删除旧 API。

推荐：

`keep -> deprecate -> migration -> remove`

必须给出替代 API 和迁移文档。

## 18.4 Changelog

每个 release 应说明：

- Added
- Changed
- Fixed
- Deprecated
- Removed
- Breaking Changes
- Known Issues

---

# 19. Documentation Rules

最低文档集：

```text
README
Getting Started
Installation
API Reference
Examples
Configuration
Thread Safety
Memory Ownership
Error Codes
Versioning
Troubleshooting
Changelog
Migration Guide
License / Third-party licenses
```

每个重要 Public API 至少说明：

- What
- Parameters
- Return
- Errors
- Ownership
- Thread safety
- Blocking
- Timeout
- Lifetime
- Reentrancy

---

# 20. Examples

必须优先提供最小可运行示例：

```cpp
#include <mysdk/sdk.h>

int main() {
    mysdk::Client client;

    if (!client.initialize()) {
        return 1;
    }

    client.start();
    client.stop();
    return 0;
}
```

示例应：

- 可以真实编译。
- 使用推荐 API。
- 不依赖内部路径。
- 尽量与文档保持一致。
- 覆盖常见工作流。

推荐额外提供：

- basic
- async
- multithread
- advanced

---

# 21. Testing Rules

最低测试层次：

1. Unit Test
2. Integration Test
3. API Test
4. ABI Compatibility Test（如承诺 ABI）
5. Performance / Benchmark
6. Stress Test
7. Concurrency Test

## 21.1 Sanitizers

优先使用：

- AddressSanitizer (ASan)
- ThreadSanitizer (TSan)
- UndefinedBehaviorSanitizer (UBSan)

## 21.2 Static Analysis

可使用：

- clang-tidy
- clang static analyzer
- cppcheck

## 21.3 Failure Cases

必须测试：

- null
- empty
- zero
- boundary values
- invalid state
- repeated init/start/stop
- cancellation
- timeout
- resource exhaustion
- disconnect
- callback failure
- concurrent calls
- destruction while busy

---

# 22. Security Rules

SDK 必须对外部输入做验证。

至少考虑：

- buffer overflow
- integer overflow
- use-after-free
- double free
- path traversal
- command injection
- unsafe deserialization
- TLS verification
- certificate handling
- credential storage
- permissions
- malformed network data

网络/文件/设备 SDK 尤其不能假设输入总是可信。

---

# 23. Performance Rules

性能优化必须基于测量。

重点指标：

- latency
- throughput
- CPU
- memory
- allocation count
- lock contention
- startup time
- p50/p95/p99

高性能 SDK 可考虑：

- zero-copy
- buffer reuse
- memory pool
- batching
- async I/O

但：

> **不得为了性能破坏 ABI、ownership、安全性或可维护性。**

---

# 24. Platform Rules

必须明确支持矩阵：

```text
OS
Compiler
Compiler version
C++ standard
Architecture
Runtime
GPU/Driver（如适用）
```

平台相关代码应收敛在 Platform Abstraction Layer。

避免整个项目到处：

```cpp
#ifdef _WIN32
...
#elif __linux__
...
#endif
```

优先：

```text
Platform Interface
├── WindowsImpl
├── LinuxImpl
└── MacImpl
```

---

# 25. Crash / Diagnostics

发布 SDK 时考虑：

- symbols
- PDB / debug symbols
- build ID
- version
- stack trace
- crash dump/core dump
- symbolication

SDK 应能够帮助用户回答：

> “这个崩溃发生在哪个 SDK 版本、哪个函数、哪个 build？”

---

# 26. CI/CD

推荐 pipeline：

```text
commit
  ↓
configure
  ↓
build
  ↓
unit test
  ↓
integration test
  ↓
sanitizers
  ↓
static analysis
  ↓
package
  ↓
compatibility checks
  ↓
release
```

Release Build 必须尽可能可复现。

---

# 27. License Rules

发布 SDK 必须审查：

- 自有 License
- 第三方 License
- NOTICE
- THIRD_PARTY_LICENSES
- GPL/LGPL/Apache/BSD/MIT 等义务
- 静态/动态链接带来的许可证影响

不得把“代码能编译”视为 license compliance 已完成。

---

# 28. Agent Review Procedure

当 Agent 被要求：

- 编写 SDK API
- 修改 SDK
- 设计公共类
- 修改 public header
- 修改 CMake
- 修改导出符号
- 修改 struct
- 修改依赖
- 修改线程模型
- 修改生命周期

必须按以下顺序检查：

### Step 1: Identify Boundary

先找：

`Application <-> SDK`

并判断：

- C ABI 还是 C++ ABI？
- 静态还是动态？
- 是否承诺 ABI 稳定？

### Step 2: Inspect Public Surface

检查：

- Public headers
- exported symbols
- Public classes
- public structs
- public enums
- third-party types

### Step 3: Evaluate Ownership

明确：

- alloc
- free
- lifetime
- borrow
- retain
- release

### Step 4: Evaluate Concurrency

明确：

- thread safety
- callback
- reentrancy
- shutdown
- background thread

### Step 5: Evaluate Compatibility

判断修改是否影响：

- API
- ABI
- struct layout
- versioning
- source compatibility
- binary compatibility
- behavioral compatibility

### Step 6: Evaluate Build / Install

确认：

- target
- include paths
- PUBLIC/PRIVATE/INTERFACE
- exported targets
- package config
- runtime dependencies

### Step 7: Evaluate Tests

至少思考：

- normal path
- invalid input
- boundary
- concurrent path
- lifetime failure
- sanitizer path
- compatibility path

### Step 8: Documentation

如果改变 Public Behavior，必须同步考虑：

- README
- API docs
- examples
- changelog
- migration guide

---

# 29. Mandatory Questions Before Approving a Public API

Agent 在评审公共接口时，必须至少回答：

1. 谁调用？
2. 参数允许什么？
3. 返回什么？
4. 失败怎么表示？
5. 谁拥有数据？
6. 数据什么时候失效？
7. 是否线程安全？
8. 是否阻塞？
9. timeout 怎么办？
10. callback 在什么线程？
11. callback 是否允许重入？
12. destroy 是否可并发？
13. ABI 是否稳定？
14. 下一版本怎么扩展？
15. 是否暴露第三方依赖？
16. CMake 用户怎么链接？
17. Windows/Linux/macOS 怎么部署？
18. 如何测试？
19. 如何升级？
20. 文档怎么告诉用户？

如果这些问题无法回答，不应轻易批准 Public API。

---

# 30. Forbidden Patterns

以下模式默认视为高风险，需要 Agent 主动指出：

## 30.1 无明确 ownership

```cpp
char* get_data();
```

但没有 `free`/lifetime 说明。

## 30.2 ABI 边界随意暴露 STL

```cpp
std::vector<std::string> get_items();
```

## 30.3 跨模块随意 new/delete

```text
SDK new -> App delete
App new -> SDK delete
```

没有统一 allocator/contract。

## 30.4 Public Header 暴露内部依赖

```cpp
#include "internal/thread_pool.h"
```

## 30.5 全局 include/link path

```cmake
include_directories(...)
link_directories(...)
```

## 30.6 仅用裸 -1 表示所有错误

```cpp
return -1;
```

## 30.7 文档只写 “thread safe”

没有对象级/函数级定义。

## 30.8 callback 中持锁调用用户代码

高概率 deadlock 风险。

## 30.9 destroy 后后台线程继续访问对象

高概率 use-after-free。

## 30.10 未处理非法状态

例如：

```cpp
sdk.start(); // SDK 尚未 initialize
```

却直接崩溃。

## 30.11 无版本/迁移策略地修改公共 struct

可能导致 ABI break。

## 30.12 要求用户手工猜 DLL 路径

说明 package/deployment 设计不足。

---

# 31. Preferred Architecture

推荐整体结构：

```text
                    User Application
                           │
                           ▼
                    CMake Package
                           │
                           ▼
                  Public API / ABI
                           │
             ┌─────────────┴─────────────┐
             │                           │
      C API / C++ Facade           Handles / Structs
             │                           │
             └─────────────┬─────────────┘
                           ▼
                     Core Layer
                           │
            ┌──────────────┼──────────────┐
            │              │              │
       Services        Threading       Resources
            │              │              │
            └──────────────┼──────────────┘
                           ▼
                   Platform Abstraction
                           │
             ┌─────────────┼─────────────┐
             │             │             │
          Windows        Linux         macOS
```

---

# 32. Preferred Public/Private Split

推荐：

```text
include/
└── mysdk/
    ├── sdk.h
    ├── config.h
    ├── error.h
    ├── types.h
    └── version.h

src/
├── sdk.cpp
├── internal/
├── core/
├── platform/
└── services/
```

原则：

- `include/` 是合同。
- `src/` 是实现。
- `internal/` 不属于用户合同。
- Public Header 不应依赖内部路径。

---

# 33. Definition of Done for SDK Changes

任何涉及 SDK 的修改，默认只有同时满足以下条件才算完成：

- [ ] Public API 已确认。
- [ ] ABI 风险已评估。
- [ ] Ownership/Lifetime 已确认。
- [ ] Thread Safety 已确认。
- [ ] Error Handling 已确认。
- [ ] CMake target 正确。
- [ ] PUBLIC/PRIVATE/INTERFACE 正确。
- [ ] Install/Package 可用。
- [ ] Runtime dependencies 可部署。
- [ ] Windows/Linux/macOS 支持范围已确认。
- [ ] Tests 已更新。
- [ ] Sanitizer/Static Analysis 已考虑。
- [ ] Documentation 已更新。
- [ ] Changelog/Migration 已评估。
- [ ] License impact 已评估（如涉及依赖）。
- [ ] Backward/Source/Binary compatibility 已评估。
- [ ] Security impact 已评估。
- [ ] Performance impact 已评估。

---

# 34. Agent Response Policy

当无法确定某个 SDK 规范时：

1. **不要假装规范不存在。**
2. 明确说明“这是项目约定 / 常见做法 / 强制标准”中的哪一种。
3. 优先尊重现有仓库的：
   - CONTRIBUTING
   - CMake conventions
   - public headers
   - CI
   - ABI policy
   - style guide
   - release policy
4. 如果项目已有规范，与本 Skill 冲突时：
   - 以项目明确规范为准。
   - 说明冲突点。
   - 不擅自扩大公共 API 或 ABI。
5. 对重大 ABI/API 变更，默认按“breaking change”审查，而不是按普通重构处理。

---

# 35. Golden Rules

如果 Agent 只能记住 15 条，必须记住：

1. **Public Header 就是合同。**
2. **ABI Boundary 必须主动设计。**
3. **跨 ABI 边界越简单越好。**
4. **Ownership 必须明确。**
5. **Lifetime 必须明确。**
6. **Thread Safety 必须明确。**
7. **Callback 的线程和重入必须明确。**
8. **不要把内部实现或第三方依赖轻易暴露。**
9. **优先 PImpl / opaque handle 隐藏实现。**
10. **CMake 必须让用户能够 `find_package + target_link_libraries`。**
11. **PUBLIC / PRIVATE / INTERFACE 必须按真实依赖传播关系使用。**
12. **稳定 API 不要轻易删除；先 deprecate，再 migration。**
13. **每次 Public/ABI 修改都必须考虑 compatibility。**
14. **SDK 必须经过测试、Sanitizer、打包和文档验证。**
15. **永远不要因为“能编译”就认为 SDK 工程是正确的。**

---

# 36. Final Agent Mental Model

每当面对一个 C++ SDK 问题，Agent 应按这个思维模型工作：

```text
                “这个改动最终会被谁使用？”
                              │
                              ▼
                       Public Contract
                              │
                     ┌────────┴────────┐
                     │                 │
                    API               ABI
                     │                 │
                     └────────┬────────┘
                              ▼
                        Ownership
                              │
                              ▼
                       Thread Safety
                              │
                              ▼
                         Lifecycle
                              │
                              ▼
                       Error Handling
                              │
                              ▼
                    Build / Install / CMake
                              │
                              ▼
                    Compatibility / Version
                              │
                              ▼
                    Test / Security / Docs
                              │
                              ▼
                           Release
```

最终目标不是：

> “代码能跑。”

而是：

> **“任何合理的第三方用户，都能够以可预期、可诊断、可维护、可升级、跨版本尽可能稳定的方式使用这个 SDK。”**
