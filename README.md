# LinglongAMG 通用 C 测试样例

## 文件

```text
LinglongAMG_sample/
|-- linglongamg_test.c          # 通用测试主程序，按 C11 编译
|-- sample_runtime.h           # 平台适配的 C 接口
|-- sample_runtime.cpp         # GPU 内存/同步适配，不包含 AMG 算法
|-- Makefile
|-- README.md
|-- spe10.cbd                  # 1,094,421 行，7,478,141 个非零元
`-- linglongamg_config.json
```

本包不包含 `.so`、授权文件或 AMG 内核源码。测试主程序始终用 C 编译器编译。平台运行时适配文件单独用 C++ 编译器编译（SYCL 本身是 C++ API），最后链接对应平台已有的 `liblinglongamg.so`。不要将主程序当作 C++ 源码编译。

## 准备

使用目标平台、架构、运行时版本匹配的 LinglongAMG SDK，目录如下：

```text
work/
|-- LinglongAMG/
|   |-- lib/linglongamg.h
|   |-- lib/liblinglongamg.so
|   `-- ...                    # 保留完整 SDK 的授权及其他运行文件
`-- LinglongAMG_sample/
```

授权按对应 SDK 的说明配置，保留其完整运行环境；样例不会绕过库内鉴权。需要该 GPU 的有效 license 及 SDK 所要求的其他鉴权材料。所有路径均可通过变量指定，没有开发机器的固定绝对路径。

进入样例目录，加载匹配平台工具链，再编译。`AMG_ROOT` 默认为 `../LinglongAMG`。头文件必须使用同一 SDK 的版本；本样例支持公共接口 `license_check/create_from_config/read_csr/setup/solve/get_solve_info` 及 FP64 类型。

## 平台选择及编译

| PLATFORM | 硬件/工具链 | 传给库的数组 |
|---|---|---|
| CUDA | NVIDIA，CUDA 12.4/12.9/13.1 对应 SDK | CUDA 设备数组 |
| HIP | 海光 Z100、BW1000（对应 DTK），AMD（对应 ROCm） | HIP 设备数组 |
| HCR | X201 / HPCC 3.5.3.18，aarch64 | HPCC CUDA bridge 设备数组 |
| MUSA | 摩尔线程 / MUSA 5.1 | MUSA 设备数组 |
| SYCL | Intel / oneAPI 2026.1 | SYCL shared USM |
| OPENCL | 砺算 / 对应 OpenCL SDK | 主机数组，由 AMG 库管理 GPU 数据 |

NVIDIA（先设置/加载 `CUDA_HOME`）：

```bash
make PLATFORM=CUDA CUDA_ROOT="$CUDA_HOME"
```

Z100、BW1000 和 AMD（加载对应 DTK/ROCm，使 hipcc 可用）：

```bash
make PLATFORM=HIP HIPCXX=hipcc
```

X201（在 aarch64 环境编译，先设置/加载 `HPCC_PATH`）：

```bash
make PLATFORM=HCR HPCC_ROOT="$HPCC_PATH"
```

摩尔线程（先设置/加载 `MUSA_HOME`）：

```bash
make PLATFORM=MUSA MUSA_ROOT="$MUSA_HOME"
```

Intel（先加载 oneAPI 环境）：

```bash
make PLATFORM=SYCL SYCLCXX=icpx
```

砺算（先加载 OpenCL/驱动和库所需依赖）：

```bash
make PLATFORM=OPENCL
```

按需追加 `AMG_ROOT=../YourSDK`、`CC=gcc`、`CXX=g++`。`CC` 必须是目标平台 C 编译器，不能设置成 hipcc/icpx 等 C++ 驱动。X201 的对象文件和 SDK 都必须是 aarch64。CUDA 库目录不是 `lib64` 时指定 `CUDA_LIBDIR`；MUSA 不是 `lib` 时指定 `MUSA_LIBDIR`。拆分安装的工具链可通过 `CPPFLAGS`、`LDFLAGS`、`LIBRARY_PATH` 补充搜索目录。

一次编译只链接一个平台的库，不存在跨 GPU 厂商通用的单个可执行文件。DTK 三个版本分别加载对应运行时、链接对应 SDK 后编译；Z100 和 BW1000 的库不能混用。

## 运行

保留已加载的厂商运行时环境，并添加 SDK 库目录：

```bash
export LD_LIBRARY_PATH="../LinglongAMG/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./linglongamg_test_CUDA
./linglongamg_test_CUDA -A ./spe10.cbd -c ./linglongamg_config.json --rtol 1e-6 --device 0
```

其他平台把可执行文件后缀换成 `HIP`、`HCR`、`MUSA`、`SYCL` 或 `OPENCL`，例如：

```bash
./linglongamg_test_HIP -A ./spe10.cbd -c ./linglongamg_config.json
```

默认矩阵和配置从当前工作目录读取；`-A`、`-c` 可以指定其他路径。CUDA/HIP/MUSA/HCR 的 `--device` 是进程可见的 GPU 序号。SYCL 使用与现有 SDK 示例一致的 GPU selector，保持 `--device 0`，需指定 GPU 时用 oneAPI 环境的 `ONEAPI_DEVICE_SELECTOR` 限定可见设备；OPENCL 库内部选卡，样例只接受 `--device 0`。

只检查 CBD 文件而不执行求解：

```bash
./linglongamg_test_CUDA --check-input
```

此模式不代表 GPU、鉴权或数值求解已通过。可执行文件仍需要对应动态库可加载。

## 配置与求解流程

附带配置采用共同支持的 GMRES(10) + AMG、L1-Jacobi、V-cycle，前/后/最粗平滑 2/2/2，最多 50 层、粗层阈值 2、强连接阈值 0.25，求解容差 1e-6。SpMV 和向量操作显式选用 manual，以避免跨平台依赖不同厂商稀疏算子。它不是各发行包原默认配置的逐字副本；比较历史性能时必须使用相同配置与硬件。

1. 读取完整 CSR CBD，校验索引和数值，排序每行列号；重复列号需预先合并。
2. 构造 `b=A*ones` 和零初值，准备该平台要求的数组。
3. `linglongamg_create_from_config` 读取 JSON，并执行库的前置鉴权。
4. `linglongamg_read_csr` 建立描述符；执行 setup、solve。
5. 求解完成后调用 `linglongamg_get_solve_info`；成功后才读取 x。
6. 使用原矩阵及原 b 独立计算 `||b-Ax||2/||b||2`，检查 b 未被修改。

CBD 数据布局为小端 int32 行数、int32 列数、int64 非零数、int64 行指针（行数+1）、int32 列号、FP64 数值。索引从 0 开始，矩阵必须为非空方阵，当前接口要求索引和非零数在 int32 范围内。b 为测试程序生成，不从 CBD 读取；JSON 的 RHS 默认值不覆盖显式传入的 b。

`--rtol` 控制样例的独立 L2 相对残差验收，不改变 JSON 求解精度。若更换求解容差或范数，应同时考虑独立验收阈值。零 RHS 的相对残差显示 NaN，仅绝对残差为零时通过。

## 输出与退出码

正常运行的输出与参考 SDK 测试文件保持一致：先显示 `Licensee: ...` 和 `Status: Valid.`，随后保留库自身输出的层级、迭代和最终状态信息。样例不额外打印矩阵摘要、setup/solve/query 时间、独立残差摘要或 verification 行，也不重复打印库的最终状态。库自身表格中的耗时不做过滤。

独立残差验证仍然执行，通过时保持静默；不通过时向标准错误输出 `Solution check failed` 和残差。授权失败输出 `Licensee: Unknown`、`Status: Invalid.` 并停止。`--check-input` 是单独的输入检查模式，仍打印检查结果。
退出码 0：独立残差通过（或仅输入检查通过）；2：未达到独立残差标准；1：输入、运行时、接口或授权错误。

## 本次验证范围

CUDA 分支通过真实 C 编译、C++ 适配编译、共享库链接和实际 spe10 输入检查；OpenCL 主机数组适配通过编译。其他分支依据现有各平台 SDK 示例接入，尚未完成该通用样例在全部厂商工具链上的编译与实机求解验收。不能据此声称所有平台已求解通过或给出性能成绩。
