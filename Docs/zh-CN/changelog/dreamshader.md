---
title: "DreamShader 插件"
description: "DreamShader Unreal 插件的版本更新记录，取自插件自带的 CHANGELOG，新版本在前。"
---

Unreal 插件的版本更新记录，新版本在前。这里记录会影响使用者的语言、生成器、编辑器工具和引擎兼容性变化。

| | |
| :-- | :-- |
| 当前版本 | **2.2.0** |
| 引擎 | Unreal Engine `5.3` 到 `5.8`，Win64；Custom Pass 需要 `5.8` |
| 记录来源 | 插件目录内自带的 `CHANGELOG.md` |

<a id="220---2026-10-15"></a>

## 2.2.0 - 2026-10-15

一次功能版本。安装的包现在可以为材质编辑器提供真正的材质函数，源文件可以不被任何未点名它的构建收录，源文件也可以按代码静默诊断。**升级后每个生成资产会重建一次** —— 插件版本是构建键的一部分 —— 在此之前读作 `Unstamped`。

<a id="added"></a>

### 新增

- **包可以随附材质函数。** 一个包会像工程的源文件那样，在其 `Assets/` 文件夹下构建 `.dss`、`.dsf` 和 `.dsi`，触发时机包括启动时、`compile -All` 时以及 cook 时，产物进入它自己的资产根（`/Game/DreamShaderPackages/<scope>/<name>`）。头文件可以要求用它自己的 helper 生成材质函数 —— 2.0 里是 `/// @generate MF_X` 或 `#pragma generate(Functions = "MF_{Name}")`，1.x 里是 `Function(Generate = ...)` 或 `Namespace(Name = "BL", Generate = "MF_BL_{Name}")` —— 由插件写出包装。*Adopt Into Source* 会拒绝包的资产，`list-generated -All` 会指出包遗留了什么。见[包 ▸ 包资产](../tools/packages.md#package-assets)。
- **NoGeneration。** `#pragma nogeneration`、某个导出上的 `/// @nogeneration`，或 1.x 块上的 `NoGeneration = true`，会让资产不参与启动扫描、保存、依赖重建、*Recompile All*、`compile -All` 和 cook；只有点名该源文件的编译才会构建它。见 [NoGeneration](../generation/no-generation.md)。
- **按代码禁用诊断。** 文件级用 `#pragma disable_warning(...)` / `disable_error(...)`，行级用 `// disable_warning DSHnnnn`，两种语言都支持；被禁用的错误不会让编译失败。见[禁用诊断](../language-v2/disable-diagnostics.md)。
- **`@custom` 函数体里的调用会对照被调用方的 HLSL 检查。** 参数个数写错是 `DSH6265`，读取没有返回值的函数的值是 `DSH6266`，报在调用处而不是着色器编译器里。

<a id="changed"></a>

### 行为变化

- **只构建包内 `Assets/` 源文件和资产头文件。** 包里其他位置的可构建源文件
  过去只在编辑器会话中编译，启动、`compile -All` 和 cook 都不会编译它；现在每条路径构建的文件
  都一致了。请把这类源文件移到 `Assets/` 下。
- **输出指纹现在是 `DSD5`。** 它像 Unreal 一样忽略大小写比较名字，所以资产不会再因为某个包把同一个名字
  写成两种拼法而被读作手改。每个资产会读作一次 `Unstamped`，并在下次重建时重新盖戳。见[分歧](../generation/divergence.md#what-counts-as-a-hand-edit)。

<a id="fixed"></a>

### 修复

- **`check -Shaders` 会编译 ThinCustom 材质的 shader。** 默认后端上的 `.dss` 过去被报告为
  不产生材质，并且无论其 HLSL 写了什么都会通过。
- **`.dss` 会把 1.x 的 `Namespace` 成员当作 `Ns::Fn(...)` 调用**，与 1.x 前端一致；过去它被当作 `DSH2154`。
- **`.dsi` 不再捡起它从未设置过的 `TranslucencySectionPriority`**，这是 UE 5.8.3 的一个未初始化覆盖问题。
- **1.x 中不返回任何值的函数（规则 L9）。** 同时传入第一个 `out` 的调用重新可以编译，并且
  `dsc migrate` 会声明这类函数返回其第一个 `out` 并改写其调用方；其证明现在会检查
  它写入的每个函数体中的每一次调用。见[迁移](../tools/migrate.md#functions-that-return-nothing-l9)。
- 插件可以在 UE `5.5`、`5.6` 和 `5.8` 上单独构建（`RunUAT BuildPlugin`）。

<a id="212---2026-10-11"></a>

## 2.1.2 - 2026-10-11

一个修正语义的版本：常量折叠、条件运算符和默认参数现在
和图、和 HLSL 的行为一致，所以**升级后每个生成资产会重建一次**。它还修复了
两种会让 cook 中止的情况，并且发布归档终于带上了中文本地化。

<a id="changed-1"></a>

### 行为变化

- **每个生成资产重建一次。** 构建键的格式标记是 `DSK7`（2.1.1 是 `DSK4`）：下面的修正会改变部分源文件生成的内容，所以旧版本写下的东西一律不再算最新。
  源码不用改。见[重新生成](../generation/regeneration.md)。
- **`round` 把正好一半的值舍入到偶数，不论参数是什么。** `round(0.5)` 是 0，`round(2.5)` 是 2，
  和 HLSL 一致。原生 Round 节点对逐像素输入也是这样，但它的 preshader 对 uniform 输入向上舍入，
  常量折叠也是如此——同一个值来路不同、舍入结果不同。内置函数现在
  降级为一个生成的 Custom 节点，反编译回 `round`；`UE.Round(Input = x)` 则保留原生节点。
  见[数学内置](../builtins/math.md)。
- **常量 `sin` 和 `cos` 按周期算，和图节点一致。** Sine 和 Cosine 节点保留默认的
  `Period = 1`，但常量折叠按弧度算，于是 `sin(0.25)` 和 `sin(T)` 配合 `T = 0.25` 时结果不一致。现在两者都是
  1；`@custom` 函数体是 HLSL，按弧度算。
- **图里的 cast 在常量里也保值。** `(int)0.5` 和 `(bool)0.5` 是 0.5，和图里、经变量时一样；
  折叠不再截断它们（截断会让常量条件翻转）。
  要截断请写 `trunc(...)` 或 `x != 0`。
- **`c ? a : b` 只有被选中的一侧会写。** 条件是动态的时候，两侧的赋值、`++` 以及
  `out` / `inout` 写入都会传到后面的代码，假分支的最后生效。现在每一侧都从
  条件之后的状态开始，只有选中的那一侧生效，和常量条件、和 `if` 一样。
- **默认参数在被调用方求值。** `float F(float x, float y = x)` 以 `F(7)` 调用时给出
  `y = 7`；过去读的是调用方的参数槽。互相引用的默认值按需求值，
  成环是 `DSH6224`。调用导出的或 `extern` 材质函数时，省略的引脚留空，
  交给资产自己的默认值。
- **Custom Pass 缓冲区只有一层 mip。** `Mips` 大于 1 过去被接受，但只写 mip 0、其余
  未定义；现在是 `DSH7308`。已有的管线请把 `Mips` 改成 1。
  见 [Custom Pass 管线](../language-v2/passes.md)。
- **`dsc -CleanNew` 只删命令行自己保存的东西。** 过去它把运行期间 Content 里新出现的每个 `.uasset`
  都当成自己的，编辑器导入的也算。现在命令行记录它保存的每个包和
  MD5，驱动脚本只删记录里有、内容没变的新文件；没有
有效记录时什么都不删、本来成功的运行以 1 退出。更新驱动时请一并重编插件。

<a id="fixed-1"></a>

### 修复

- **插件自带的 `DShader` 目录不再让宿主工程的 cook 失败。** 引擎或
  商城插件里写 `Root="Plugin.<Name>"` 的源文件，在只是装了它的工程里无法生成
  （`DSH8090`），过去会让整个 cook 中止。这类失败现在记作
  `[Cook] Not this project's to build (plugin '<Name>')` 并继续 —— 这些资产随插件一起发布。
  工程自己的源文件，以及 `<Project>/Plugins` 下插件的源文件，生成失败仍然中止 cook。
  见[内存材质](../generation/ephemeral.md)。
- **干净工作区上的 cook 不再取决于文件顺序。** 材质用 `VirtualFunction` 调用另一个源文件生成的函数时，
  如果它的路径排在前面，会以 `DSH8219` 失败并中止 cook。现在 cook 和 `compile -All`
  共用一套批量编译：先 `.dsf`，然后分轮编译，某个文件失败而同一轮里别的文件编成功了，下一轮就重试它；
  一轮下来没有新的成功，剩下的才是真失败。函数链、`.dss` 导出的函数同样适用。见[命令行](../tools/commandlet.md)。
- **发布包带上 `Content/`，也就带上了 `zh-Hans` 翻译。** 从 1.6.0 到 2.1.1 的发布 zip
  都缺它，装出来的编辑器界面只有英文。旧安装可以从对应 tag 把 `Content/Localization/` 拷进去。
- **生成的材质实例重新加载后不再被当成手改。** UE 在加载时会给每个参数覆盖补上
  `ExpressionGUID`，指纹过去把它算进去，所以 `.dsi` 实例第一次重载后就读作 `Diverged`，
  拒绝重建、cook 也停在它上面。现在指纹不含这个 GUID，其余字段照算；格式仍是 `DSD4`。
  感谢 [@Yu2erer](https://github.com/Yu2erer) —— PR [#38](https://github.com/TypeDreamMoon/DreamShader/pull/38)。
- **一次对 2.1.1 的审查修掉三十处问题**（PR [#49](https://github.com/TypeDreamMoon/DreamShader/pull/49)，每处
  带回归测试）：链式 swizzle 写入 `v.zyx.x = 1` 写对它指名的分量；嵌套 `if`
  各分支都 return 后仍可达的外层 return 被保留；保存被 include 的 `.dss` 会重建
  包含它的源文件；编译实例会刷新过期的祖先；`TextureCollection`、`double4`，以及 Cube、
  Volume 和 Array 纹理在实例、反编译和 Adopt 里不再丢；第二个打开同一工程的编辑器不再
  改写持有 bridge 的那个编辑器的文件；Custom Pass 的池化材质实例不再带着上一个
  pass 的参数、Nanite 模板过滤的 `AND` 不再当成 `OR`、`Depth32` 缓冲区按配置的
的值；放大预览窗口不再让 D3D12 崩溃。
- 在 UE `RunUAT BuildPlugin`、`5.5`、`5.6` 和 `5.8` 上单独构建插件（`RunUAT BuildPlugin`）验证通过。

<a id="211---2026-10-04"></a>

## 2.1.1 - 2026-10-04

1.x 语言的文档和行为对齐到真正在构建 1.x 源文件的 2.0 编译器，外加一批 1.x 文本兼容修复。

- **1.x 各页描述的是构建 1.x 源文件的编译器。** 自 2.0.0 起，`.dsm`、`.dsf` 或 `.dsh` 由 legacy 前端读取、由 2.0 编译器构建，但语言、图、内置、参数、设置、生成和工具各页仍在引用已删除的 1.x 生成器的消息和行为。现在每页都写出报告该情况的 `DSHnnnn` 码，并说明今天的实际行为；[诊断](../diagnostics/index.md)一页讲诊断出现在哪里、怎么读一条诊断，并把常见的 1.x 消息对到各自的错误码。
- **构建键不再取决于工程在哪里检出。** 指纹过去按绝对路径记录源文件和头文件，于是同事或 CI 代理换个目录检出就会重建、重存每个生成资产。现在按工程相对路径记录。格式是 `DSK4`，所以升级后每个资产都会重建一次。
- **重编一个材质不会影响另一个材质在共享头文件上的诊断。** 一条记录属于产生它的源文件和那一次运行；编译成功时的警告在材质 shader 编译完后也会保留。
- **命令行开关带值也能读。** `-Force=true`、`-Check=1` 或 `-DryRun=yes` 过去被读成没写——`migrate -Check=true` 会真的改写文件。现在 `true` / `1` / `yes` / `on` 为开，`false` / `0` / `no` / `off` 为关，其他值（包括空的 `-Force=`）是 `DSH9110`，此后命令什么都不做、以 `1` 退出。不属于任何文件的错误不再显示成假位置。
- **1.x 文本兼容：** `Function SelfContained`（以及旧写法 `Inline`）重新把被调用的函数嵌进 Custom 节点；`UE.CollectionParam` 重新可用，1.x 才有的变换空间（`AbsoluteWorld` 等）写成引擎的名字并警告 `DSH5278`，没写 `Destination` 的 `UE.TransformPosition` 变换到 `World`；块属性列表 `)` 前的逗号、`Properties` 默认值里的指数写法（`1e3`）重新可读；1.x `Graph` 里的 `7 / 2` 重新是 3.5（只有两侧都是整数构造调用才是整数除法）；多输出节点直接传给引脚时取第一个输出（`DSH5287`）。
- **一个 `.dsm` 会构建它的 `Shader` 以及旁边的函数与层块**；材质按名字调用它们，函数资产先建。`.dsf` 里的一个 `Shader` 块是 `DSH2259`。
- **Agent 技能写 2.0 源文件**，并且新增了第六个：`dream-shader-migrate`。

<a id="210---2026-10-04"></a>

## 2.1.0 - 2026-10-04

- **Custom Pass：用你自己的渲染 pass，写成 `.dsp` 管线（UE 5.8）。** 一个 `.dsp` 文件声明具名缓冲区和一串有序的 pass，每个 pass 挂在渲染器的某个注入点上——从什么都还没画的 `BeginView`，经过 `AfterBasePass`、`AfterOpaque` 和 `BeforePostProcess`，到后处理链内部的各点以及 `EndOfView`——并编译成一个 `UDreamPassPipeline` 资产，也能反编译回 `.dsp`。
  pass 可以是全屏后处理材质（写在 `.dss` 里，通过 `UE.UserSceneTexture` 读该 pass 的缓冲区）、写在 `.dsp` 里或工程 `.usf` 里的全屏或计算 HLSL、按模板值 / pass 层 / 具名列表挑选并绘制图元的网格 pass，或者清除、拷贝。缓冲区可以跨帧保留历史，导出的缓冲区是普通材质（`UE.DreamPassBuffer`）、蓝图、UMG 和 Niagara 都能读的渲染目标。管线由工程设置、`ADreamPassVolume`、`UDreamPassComponent` 或蓝图 / C++ API 激活，参数覆盖像后处理体积那样混合。见 [Custom Pass 管线](../language-v2/passes.md)、[运行时](../language-v2/passes.md)和[示例](../examples/custom-pass.md)。
- **pass 的 HLSL 可以直接写在它的 `.dsp` 里**（`hlsl { }`）：写整个函数，或者只写入口的语句、由编译器替你补上函数；文件级的 `hlsl { }` 放所有这类 pass 共用的代码。预检错误报在 `.dsp` 的对应行。见 [HLSL pass](../builtins/hlsl-library.md)。
- **生成的 VS Code 工作区认识 2.0 源文件**：`.dss` 和 `.dsi` 关联到 `dreamshader-dss`，`.dsp` 关联到 `dreamshader-dsp`（扩展 2.1.0）。
- **修复：** 给没有滑条的参数写滑条范围（1.x `vec3` 属性上的 `Slider`、`float3` 或 `bool` uniform 上的 `@slider`）现在是一条 `DSH7233` 警告、报在范围那里，范围被丢弃；编译成功时的警告会写进 `diagnostics.json`，扩展里看得到。

<a id="202---2026-10-02"></a>

## 2.0.2 - 2026-10-02

- **反编译器按名字写出每个 Substrate 节点。** 过去 Transmittance-To-MFP、Coverage Weight、Add、Horizontal Mixing 会被写成语法糖（`Slab * w`、`+`、`lerp`）——图是对的，但在文本里搜不到图上那个节点。现在它们是各自的调用，如 `Substrate.Weight(A = ..., Weight = ...)`；语法糖只在可读形式（`-Readable`）里出现。用语法糖写的源文件照常编译。
- **修复：** 带具名分支的 Switch 反编译回 Switch 节点本身（分支未命名时仍是分支链，并以 `DSH9073` 提示命名分支）；未连线引脚的常量写在该引脚自己的位置；Landscape 层混合、Grass 输出、物理材质输出带着引脚反编译（`Layer_Grass = ...`、`Height_Rock = ...`）。

<a id="201---2026-09-27"></a>

## 2.0.1 - 2026-09-27

- **修复反编译器的几处遗漏：** Convert（Make / Break FloatN）按它实际计算的通道写回（`DXY.xy`、`float3(Gain, 0.25, 0.75)`）；Switch 写成它的分支链；读 VertexInterpolator 的材质能反编译（`UE.VertexInterpolator(Input = ...)`）；目录不认识的 SetMaterialAttributes 引脚会点名（`DSH9072`）。
- **`Substrate.ShadingModels` 有了 `ShadingModel` 引脚**，`Substrate = Bridge` 下算出来的着色模型能接上去。

<a id="200---2026-09-21"></a>

## 2.0.0 - 2026-09-21

2.0 这条线：与其说是功能版本，不如说是一次编译器重写。一个只依赖 Core 的语言前端、夹在解析器与材质图之间的图 IR、作为同一个编译器第二前端保留下来的 1.x 语法，以及与之并列的 HLSL 形状的 `.dss` 语言。`1.8.0` 之后的一切——包括 `1.9.x` 的条件编译与图断点——都在里面。

- **[DreamShaderLang 2.0](../language-v2/index.md)** —— `.dss` 源文件：`uniform`、`///` 指令、
  `#pragma material(...)`、每个 `export` 一个资产、真正的表达式与控制流、会内联的 helper、
  `/// @custom` HLSL 函数体、给已存在资产用的 `extern` 原型。
- **Material Content Browser 能新建 `.dss` 源文件。** *New ▾* 菜单多了 DreamShaderLang 2.0 一节 ——
  **Material (.dss)**、**Material function (.dss)**、**Instance (.dsi)** —— 排在三个 1.x
  模板前面；填的名字就是 `export` 的名字，也就是资产名，文件夹决定它落在
  `/Game` 下的哪里。
- **一个编译器，两个前端。** `.dsm` / `.dsf` 由 legacy 前端读成同一棵树、
  由同一个发射器构建；拿四个真实源码根的 1.x 冻结图对拍，每个源文件都逐节点重建出同一张图。
  1.x 的生成器与解析器已经删除。
- **[把材质实例写成源码](../language-v2/instances.md)** —— `.dsi`，对照父材质真实的参数表检查。
- **[`dsc migrate`](../tools/migrate.md)** —— 1.x 到 `.dss`，落盘之前先证明注释一条不丢、生成的图一模一样。
- **会写 2.0 文本的反编译器**，做法是把编译器倒着跑；语言说不出来的东西会写在文件头部点名，绝不悄悄丢掉。
- **[Substrate 语法糖](../language-v2/substrate.md)** —— `A + B`、`A * w`、`lerp(A, B, t)`；Slab 上的旧参数，
  转换节点替你加好；运行时分支变成 `Substrate.Select`；按成员逐步构建的值；`#pragma material(Substrate = Legacy | Bridge | Native)`。
- **读源码的图布局**，做成工程设置 **Graph Layout Style**。默认的 `Blocks` 把图排成一页按源码顺序的框 ——
  一个 `#pragma region` 或一段连续语句一个框，框里是一小张从左到右的图 —— **框与框之间没有连线**：
  别的框要读的值，在它旁边放一个具名 reroute 声明（`DS_<variable>`），读它的每个框里各放一个 usage；
  常量在读它的地方再写一遍。另两种不插入任何节点：`SourceBands` 一条语句一带，
  `Layered` 整图分层。`dsc dump-layout` 不构建任何东西就能为任意源文件把三种都画出来，供你先看再选。
  1.x 的布局改名 `Classic`，仍可选用。已有的图在其源码下次重建时换成新的排法；节点位置
  不进手改指纹，所以不会有资产因此被当成「手改过」。
- **工具** —— `check`（以及把 HLSL 错误映射回源码行的 `check -Shaders`）、`fmt`、
  `list-generated` 与一页[版本控制](../generation/source-control.md)、`dump-ir`。
`index`、`export-catalog`；节点 ↔ 源码行双向跳转；为语言服务写出的符号索引与节点目录。
- **Graph 材质与材质函数总是落盘。** 只有 ThinCustom 材质还保留纯内存状态。
- **修复：换一种语言的编辑器，生成的图层 / 混合 / 属性函数就被当成「手改过」。**
  [手改检测](../generation/divergence.md)的指纹用引擎的
  `GetInputName` 给输入起名，而 `BreakMaterialAttributes`、`GetMaterialAttributes` 和 `SetMaterialAttributes`
  给出的是**翻译过的文字** —— 英文编辑器盖的戳，中文编辑器算出来对不上，中英混用的团队会互相看到对方的资产是 `Diverged` 并被
  拒绝重建。现在这些输入按属性本名命名；指纹还多覆盖了一项：
  一个 `GetMaterialAttributes` 发布哪些属性。格式从 `DSD3` 升到 `DSD4`，所以
  升级后每个生成资产会读作一次 `Unstamped`，下次重建时重新盖戳。
  `dump-graph` 同步改为按属性命名这三种节点的引脚。
- **修复：图层混合写不了 `FrontMaterial`。** 混合的结果从空开始，它的写入会变成
  一个 `MakeMaterialAttributes`，而它没有 `FrontMaterial` 引脚（`DSH8212`）。现在 Make 没有
  引脚的属性由叠在它上面的 `SetMaterialAttributes` 设置。

<a id="180---2026-08-21"></a>

## 1.8.0 - 2026-08-21

一个关于**不把东西弄丢**的版本。生成器过去是无条件推倒重建的：手改过的资产被悄悄覆盖、失败的重建把资产
清空、开着材质编辑器的资产在下一次 Apply 时被悄悄回滚。这一版把这几条路全部堵上 —— 手改指纹、原子重建、
编辑器占用闸门、按依赖排序的批编译，以及一个真正覆盖「什么决定编译产物」的构建键。

<a id="divergence--three-ways-out"></a>

### 手改检测 —— 三个出口

- 每次成功生成都会给资产实际持有的内容盖上一枚 **`DreamShader.OutputDigest`** 指纹，下一次重建在清空任何东西**之前**先比对它。对不上就**编译失败**，资产保持你离开时的样子 —— 没有清空，也没有半成品。见
  [重新生成](../generation/regeneration.md)。
- 指纹覆盖的**恰好是重建会摧毁的东西**：节点、连线、节点属性、
  reset 属性集、函数的资产级字段，以及 ThinCustom 实例的参数覆盖。重建本来就不碰的东西被刻意排除在外 —— 节点位置、你自己写的注释框、
  pin GUID，以及 reset 列表之外的属性。护着一个本来就没危险的属性，只会白白拒掉一次重建。
- 三个出口都在资产右键菜单的 **DreamShader** 下：**Revert to Source**
  （丢掉手改、按源码重建）、**Adopt Into Source**（用资产反写 `.dsm` / `.dsf`，
  旧文件先备份成 `<source>.bak`）、以及 **Detach From DreamShader**（保留资产、从此不再管它）。
- `-Force` **绕不过它**。`bForce` 回答的是「源码哈希过期了吗」，而编辑器在它自己的启动扫描里对每个文件都断言了这个标志 ——
  认它就等于在编辑器唯一常驻的模式里让闸门直接失效。只有 Revert 能覆盖一次手改判定，因为只有人能做这个决定。
- 纯内存资产也会被打上指纹和源码**路径**（但不打源码哈希，那会把[跳过重建](../generation/index.md)
  也一起打开）。没有这条路径，内存资产会被判成「外来资产」，闸门在编辑器的默认模式里根本不会触发。

<a id="a-rebuild-is-atomic"></a>

### 重建是原子的

- 这些步骤要么全部生效，要么一步都不生效。旧图现在是被**摘下**而不是被销毁，只有在重建完整成功之后才真正销毁；一旦失败就从一份序列化快照恢复资产——渲染状态、材质函数 usage、节点图、连线，以及 `FunctionInput` / `FunctionOutput` 的 pin GUID 全都会回来，所以既有调用点仍然连着。
- 这件事非做不可，是因为**不是所有失败都能提前拦住**：整文件解析、`Settings` 校验和 `Outputs` 校验都在资产被碰之前运行，但一个 `Graph` 块是由图构建器一句一句编译的，而它跑在拆除**之后**。在 `1.8.0` 之前，这类失败会留下一个**被清空的**资产——对材质来说已经很糟，对材质函数则糟得多：它的调用点是从活着的资产上读 pin 的，一个坏掉的 `.dsf` 会把每一个调用它的材质一起带走，而且没有 undo（生成资产刻意不是 `RF_Transactional`）。
- 由此得出两件事。拆除的两套删除策略合并成了一套——逐节点删除路径的存在是为了断开入边，而当图整体离开时这件事毫无意义——所以原来的 1200 个表达式阈值消失了，每次重建都走快路径。还有 `DependentFunctionExpressionCandidates`，那是第二份序列化节点列表，它的陈旧条目过去会在函数编译失败后给整个会话装上一颗空指针雷，现在改在提交时重置，而不是在拆除时。

<a id="a-rebuild-is-refused-while-the-asset-is-open-in-an-asset-editor"></a>

### 资产在某个资产编辑器里开着时，重建会被拒绝

- 资产编辑器**并不编辑资产本身**：`FMaterialEditor` 把它复制成一个临时的 `UPreviewMaterial`，在 Apply 或 Save 时再把那份副本盖回原件；材质实例编辑器则通过一个 `UMaterialEditorInstanceConstant` 包装写回。因此，一个跨越了重建仍然开着的编辑器，手里握的是**重建之前**的副本，而下一次 Apply 会把重建所做的一切静默还原——接下来以一次分歧报告的形式浮出水面，出现在再下一次编译里，离病因已经很远。
- 现在编译会停下来并给出 `Asset '{ObjectPath}' is open in an asset editor, so it was NOT rebuilt. …`，并提示关闭它。
- **Revert to Source** 和 **Adopt Into Source** 是例外：它们自己关掉编辑器、动手、再打开。两者都挂在那个编辑器自己的工具栏上，而一个在最有用的地方永远是灰的菜单项不是保护，是 bug。

<a id="the-build-key-replaces-the-plain-source-hash"></a>

### 构建键取代了单纯的源码哈希

- `DreamShader.SourceHash` 现在是一把**构建键**：预处理后的源码文本、**Default Compiler
  Backend**、三张映射表、插件版本加一个手动递增的格式标签，以及引擎版本。它以前只哈希源码文本，因此任何*其他*会改变输出的变化都会让每个已生成的资产看起来仍是最新的。
- 因此，之前硬塞到 backend 设置上的强制全量重扫被移除了：每个受影响的资产各自无法通过跳过检查，而该设置**不**影响的资产仍然被跳过，而不是被无谓地重建。
- 改变构建键的组成会使所有已有标记失效 —— 每个资产重建一次，仅一次，这正是预期效果。

<a id="a-batch-of-source-files-compiles-in-dependency-order"></a>

### 一批源文件按依赖顺序编译

- 一个调用 `ShaderFunction` 的 `.dsm` 会把它的调用节点绑定到**实时的** `UMaterialFunction`
  资产上 —— `SetMaterialFunction` 从对象而非源码上读取引脚 —— 所以先编译调用方会把它绑定到该函数接口的*上一个*版本上。重命名一个函数输入并保存两个文件就足以触发该问题，而哪个版本胜出取决于一个 `TMap` 的迭代顺序。
- 两个处理点 —— watcher 的待处理文件批和整个项目的扫描 —— 现在都按导入图进行拓扑排序。只有批内的边被遵循；循环则留给导入加载器用它自己的诊断来拒绝。

<a id="one-editor-owns-the-bridge-for-a-project"></a>

### 一个项目的 bridge 只由一个编辑器持有

- bridge 目录是按**项目**而非按进程的：一个 `Requests` 文件夹、一个 `status.json`、
  一个心跳。因此两个编辑器打开同一个项目时，都会轮询同一个队列，并都用各自的 pid 覆写 `status.json`。
- 所有权现在由一个锁文件 `Bridge/owner.lock` 表示，携带持有者的 pid 和一个心跳：只有在持有者的进程存活**且**其心跳不超过 30 秒时，才认为它仍是持有者。两个测试缺一不可 —— 只看 pid 会在持有者正在编译时（编译会阻塞游戏线程）就把 bridge 交出去，而只看心跳会在硬崩溃后让它处于无主状态。它在关闭时释放。
- 非持有者编辑器仍然会编译自己的内存材质，但不消费请求、不写
  `status.json`，也**不把生成的资产写入磁盘**。最后这一点直接源自存储决定一次重建如何持久化：没有它，两个编辑器会同时 `SavePackage` 同一个文件，而在这场竞争中落败的一次保存不是合并 —— 而是一个损坏的包或一个死掉的编辑器。commandlet 不受影响。见 [编辑器工具](../tools/editor-integration.md)。

<a id="an-asset-that-exists-on-disk-is-rebuilt-on-disk"></a>

### 磁盘上存在的资产会在磁盘上重建

- **决定一次重建如何持久化的是存储，而不是发起编译的那一方。** 编辑器每次要的都是纯内存编译，而当它落在一个已经存在的 package 上时，生成过程就地重建了它，然后清掉了脏标记——产生了一个既不匹配磁盘上的文件、也不匹配任何将来会被写出的东西、还自称干净的对象。一次 Save All 就能落下一个谁也没选过的状态，而编辑器里可见的版本会在重启后消失。唯一的信号只有一条日志 warning。
- 现在由 `IsGeneratedAssetPersisted` 决定：背后有文件的资产走持久化路径——打戳、保存、脏标记绝不作假——没有文件的则留在内存中。“这个资产到底是什么”重新只有一个答案。见 [In-memory Materials](../generation/ephemeral.md)。
- 作为这一改动的一部分，启动扫描不再强制重建，而且必须如此。在每个内存资产无论其源码哈希如何都会重建时，强制是免费的；而一旦磁盘资产开始被*保存*，它就不再免费了，因为每次启动编辑器都会重写每一个持久化的生成资产。修改 **Default Compiler Backend** 仍然会强制重建，因为哈希看不见它。

<a id="fixed-2"></a>

### 修复

- **一个 `Domain="Volume"` 材质会携带 `bUsedWithVolumetricCloud`，反编译它也不再会把该标志重新打开。** 该标志现在从 domain 推导，而显式指定的 `bUsedWithVolumetricCloud = "false";` 仍然胜出——一个只喂给体积雾的 Volume 材质可以拒绝云的 shader 排列。它通过 `UMaterial::SetUsageByFlag` 写入（UE 5.8 弃用了所有 `bUsedWith*` 字段），并且赋值移出了 `Domain` 分支，改为从材质回读 domain，因为 `ResetMaterialToDefaults` 不会清除 usage 标志。反编译器经过同一个 `GetDefaultUsedWithVolumetricCloud`，所以 `UMaterial` → `.dsm` → `UMaterial` 重新成为恒等变换。见 [Material Settings](../settings/material.md)。在 [#27]（https://github.com/TypeDreamMoon/DreamShader/pull/27）中报告。
- **swizzle 能活着穿过材质编辑器。** 像 `CustomStencil.r` 这样的分量选择过去被写成连接上的内联 mask（`OutputIndex=0` 加 `Mask/MaskR`），HLSL 翻译器认它，但材质图编辑器画不出来：pin 对应的是表达式的 `Outputs` 条目，而没有 pin 就意味着“输出 0，只取红通道”。`UMaterialGraph::GetValidOutputIndex` 一见到 mask 存在就不信任 `OutputIndex 0`，找不到匹配的 pin，便回退到节点的**最后一个**输出——然后第一次 Apply/Save 会把它写回去。在 `SceneTexture` 节点（`Color / Size / InvSize`）上，这把 `CustomStencil.r` 变成了 `InvSize`，静默地发生，任何地方都没有报错：stencil 比较对每个像素都失败，该函数所 gate 的效果直接不再出现。任何经材质编辑器碰过的生成资产都会中招，而
先复制一份也没用。
  现在分量选择按图能 round-trip 的形式发射。只有当编辑器能把内联 mask 解析回它已经指名的
  那个 pin 时才保留内联 mask；当表达式通过多个带 mask 的输出发布同一个值时（`TextureSample` 的
  `RGB/R/G/B/A/RGBA`、`VertexColor`、……）直接指名匹配的那个输出；否则发射一个真正的 `ComponentMask` 节点。
  像 `SceneTexture` 这样输出集合混杂的永远不改指，因为那里的各个输出是不同的值，
  而不是同一个值的不同视图。构建键标签升到 `DSK2`，所以每个生成资产都会按新形式重建一次。
- **`UE.CollectionParam(...) Name;` 在 `Properties` 里的宽度跟着集合参数走。**
  解析器把声明形式记成标量（它打不开集合），而生成器过去照单全收，于是一个 vector MPC 参数喂给
  `float4` 输入时会被三次 `AppendVector` 拼接加宽，材质以 `Can't append float4 to float4` 编译失败。
  现在宽度从加载好的集合读取（vector → 4，scalar → 1）；显式写的 `OutputType=` 仍然胜出。见
  [Properties](../language/properties.md)。
- **render target 可以在期待对应维度纹理的地方被接受。** 维度检查过去比较的是资产类；
  现在读 `UTexture::GetMaterialType()` —— 与材质编译器在 texture-object pin 上给出的答案相同 ——
  所以 `UTextureRenderTargetVolume`、`UTextureRenderTarget2D`、`UTextureRenderTargetCube` 和 `UTextureRenderTarget2DArray`
  都能通过各自维度的检查，texture-object pin 会接受的任何其他东西也一样。
- **cook 生成的资产会注册进 AssetRegistry，因此能进包。** cook 请求是经 `IAssetRegistry::DoesPackageExistOnDisk` 解析的，
  它只查 registry 的内存状态、没有文件系统兜底；生成跑在 post-engine-init，也就是 registry 枚举完内容目录之后，
  于是刚写出的 package 对那次查询是不可见的 —— 材质根本没进 pak，`LoadObject` 在 Shipping 构建的运行时失败，
  全程没有任何报错。现在生成过程记录自己实际写入了哪些 package，并在 commandlet 的 `Main`
  收集初始请求之前把文件名交给 `IAssetRegistry::ScanModifiedAssetFiles`。在 [#26]（https://github.com/TypeDreamMoon/DreamShader/issues/26）中报告。
- **ThinCustom 实例路径会跑归属检查。** 它过去只看目标路径上那个对象的类、不看来历，
  于是往一个手写的 `UDreamShaderMaterialInstance` 上生成会接管它、并清掉它的参数覆盖。
  由于 ThinCustom 是默认 backend，这是三条创建路径里覆盖面最广、也是唯一没有这个检查的一条。
- **`DreamShaderGraphDecompilerHelpers.h` 能单独编译** —— 它声明了吃 `EMaterialDomain` 和 `EBlendMode` 的函数
  却没 include 任何一个枚举的头，unity 构建只是靠邻居的翻译单元蒙混过关。
曾通过相邻的翻译单元。

<a id="added-1"></a>

### 新增

- **位于 `Function` 体顶部的 `#include` 会被提升到文件作用域。** `Function` 块的 HLSL
  过去原样发射在生成的 `DreamShaderFn_*` 定义的花括号之间，这
  使任何 `#include` 落在*函数内部*——只有宏的头文件没事，定义了函数的任何东西都是编译错误。
  开头的 `#include "…"` / `#include <…>` 指令（前面只有空白
  和注释）现在会被从函数体中剥去，按首次出现顺序发射一次，
  就在生成的 `.ush` 的 guard 之后；`SelfContained` 内嵌或 `GraphFunction` 体则把它们
  放到 Custom 节点的 `IncludeFilePaths` 上，排在生成的 include 之前。第一条
  语句之后的指令保持旧行为。这正是让一个 C++ 与 HLSL 共用的头文件
  能被 `.dsf` 消费、而无需把它的函数拷贝进 DreamShaderLang 的东西。见
  [函数](../language/functions.md)。

<a id="171---2026-08-16"></a>

## 1.7.1 - 2026-08-16

<a id="fixed-3"></a>

### 修复

- **用过 Graph 断点之后关编辑器会崩。** 在 `UMaterial::GetExpressionInputDescription` 里读 `0x3b0` 的访问违例，
  来自预览探针自己的析构函数，在模块卸载期间经由
  `FDreamShaderPreviewWebSocketServer::Shutdown` 触发。
  模块卸载发生在退出路径内部：`EngineExit()` 抬起退出请求，`FEngineLoop::Exit()`
  跑 purge，之后才调 `UnloadModulesAtShutdown()`——所以此时拆自己状态的模块，
  拆的是它指向的对象已经消失*之后*。`TStrongObjectPtr` 能让
  预览材质躲开垃圾回收器的可达集扫描，却不能豁免它
  于退出 purge：指针还非空、它背后的对象已经没了，这就是
  既有的空指针检查全部通过、解引用却仍然爆掉的原因。
  值得点名的是它爆在哪，因为调用点没有任何地方暗示这一点——
  `UMaterial::GetExpressionInputDescription` 和 `GetExpressionCollection` 都不加检查地解引用
  `GetEditorOnlyData()`，所以 purge 之后去够材质的输入或表达式，是一次
  无条件的空指针加偏移读取，而不是一次可恢复的失败。拆除
  现在会在引擎开始退出时跳过，这不仅是安全，更是正确：释放共享
  表达式集合的目的是让预览材质*之后*碰不到图材质的节点，
  而在退出时没有之后。客户端断开而编辑器继续运行的那条
  常规路径不变。

<a id="170---2026-08-16"></a>

## 1.7.0 - 2026-08-16

<a id="added-2"></a>

### 新增

- **实时预览中的 Graph 断点 —— 为文本源提供「Start Previewing Node」。** 在 `Graph` 某一行上打断点，
  预览网格就显示**那一行绑定的值**，而不是成品材质。这和在材质编辑器里右键节点选
  *Start Previewing Node* 是同一个想法，只不过这里的「节点」是 DreamShaderLang 的一行。
  实现是复用引擎自己的机制：生成器为每个源文件发布一张**调试表**，把每个 `(line, name)` 绑定映射到它
  产生的那个 `UMaterialExpression` / 输出 / 通道 mask；一个临时 `UPreviewMaterial` —— 共享生成材质的表达式
  集合，把被探测的节点接到它的 emissive（值是 `MaterialAttributes` / `FrontMaterial` 时接到对应输入，
  纹理对象则用默认坐标采样一次）—— 经 `FMaterialUpdateContext` 重新编译。`UPreviewMaterial::ShouldCache`
  把这次重编译限制在少数几个 shader 上，和节点缩略图走的是同一条路。
  空行或注释行上的断点会向前吸附到下一个绑定了值的行；在源码还没生成过时打的断点会被记住，并在下一次编译时
  挂上；重新编译之后探针自动重解析，并把落在哪一行告诉客户端。协议是
  `setProbe` / `clearProbe` / `probeState`。见 [编辑器工具](../tools/editor-integration.md)。

<a id="changed-2"></a>

### 变更

- **流式预览现在发原始 RGBA8 帧，而不是每帧一张 PNG。** 新的 `encoding: "raw"` 会话把 render target 读回来、
  把像素作为一个自描述的二进制帧发出（24 字节头 —— 尺寸、标志位、相机、解析后的探针行号 —— 之后是像素），
  编辑器侧完全没有 PNG 编码。浏览器 / webview 客户端把它直接画到 canvas 上。这就是流畅 30–60 FPS 的
  绝大部分成本所在：每 tick 给一张 512² 的帧做 PNG 编码，是几十毫秒的游戏线程时间。旧的
  `encoding: "png"` 路径对老客户端保持不变。
- **画面不动时流式预览变便宜了。** 相同的帧会被丢弃，连续几帧都相同之后渲染时钟退到一个很低的空闲速率，
  直到一次编辑、一次相机 / 网格变化或一次探针变化把它重新唤醒（shader 还在编译时渲出的帧不会触发退避）。
  省略了某个字段的 `previewControl` —— 包括 `frameRate` —— 现在保持当前值而不是重置回 2 FPS，
  并且可以携带 `width` / `height` / `mesh`，客户端不必重发完整请求就能驱动
  渲染尺寸和形状。`previewMaterial` 上的 `force` 标志用来区分
  显式刷新（重新生成）和相机 / 网格重新请求（复用上次生成结果）。

<a id="160---2026-08-15"></a>

## 1.6.0 - 2026-08-15

<a id="language--ten-more-math-builtins"></a>

### 语言 —— 十个新的数学内置

- `Graph` 新增 `step`、`smoothstep`、`length`、`cross`、`asin`、`acos`、`atan`、`atan2`、`reflect` 和
  `refract`。它们每一个原本都有节点 —— 对 `reflect` 和 `refract` 则是四行定义 ——
  却都没有写法，所以想写 `step(0.5, x)` 只能写成
  `UE.Expression(Class="Step", OutputType="float1", Y=0.5, X=x)`，而按 HLSL 的写法写出来时，失败信息是 `Unknown Graph function 'step'` 包在参数所属的那次调用里面。
  内置写法从 19 个增加到 29 个。见 [数学内置](../builtins/math.md)。
- 其中三个新名字的输入 pin 不叫 `Input`，这正是这份映射值得写清楚的原因：`Step` 和 `Arctangent2` 把 pin 叫 `Y` 和 `X`，而 `SmoothStep` 把它们叫
  `Min` / `Max` / `Value`。参数顺序在每种情况下都保持 HLSL 的顺序，所以 `step(edge, x)` 把参数
  1 连到 `Y`、参数 2 连到 `X`，含义仍然是 `x >= edge`。
- `length` 和 `cross` 与 `dot` 一样是返回宽度固定的内置 —— 1 和 3 分量，
  且是权威宽度。
- `reflect` 和 `refract` 是**头两个不是一个节点**的内置。Unreal 两者都没有对应表达式，
  所以它们被降级成 HLSL 对它们的定义：`i - 2 * dot(i, n) * n` 是四个节点，折射连同处理全内反射时返回零的
  `If` 一共十四个。两者都精确，也都很贵；周围代码本来就是 HLSL 时，
  写在 `Function` 体里一个节点就够了。

**警告：**

**这拓宽了一个保留命名空间。** 这十个名字现在和原来那十九个一样会**静默遮蔽**用户代码 —— 叫 `length`、`step` 或 `cross` 的 `Function`、property 或 `ShaderFunction`，
从 `Graph` 块里将无法被调用到，而且没有任何诊断。既有源码里声明了同名符号的需要改名。

无法加入，也无法加入：矩阵。Unreal 材质图**根本没有矩阵值类型**，因此无论 DSL 怎么做，
`mul(M, v)` 都没有 `Graph` 写法 ——
`UE.Expression(Class="Transform"/"TransformPosition", …)` 覆盖空间转换，一段 `Function` HLSL
代码体覆盖其余部分。仍然缺席但可以经 `UE.Expression` 触达、各自都有节点的是：
指数、对数、`tan`、`sign`、`round`、`trunc` 和 `distance`。反编译器未做改动，
所以新节点导出成通用的 `UE.Expression(Class="…", …)` 调用，而不是回到内置写法 —— 和
`Fmod` 已有的不对称一样。`reflect` 和 `refract` 则根本无法
round-trip，因为它们留下的是普通算术节点，没有任何东西标记它们的来历。

<a id="diagnostics--dshnnnn-codes"></a>

### 诊断 —— `DSHnnnn` 错误码

- 在此之前，唯一标识一条 DreamShader 失败的东西是它的**英文措辞**。现在 parser 的全部 120 个抛出点都改为
  携带一个错误码：`DSH1xxx` 路径解析、`DSH2xxx` 词法与语法、`DSH3xxx`
  section 与声明、`DSH7xxx` properties / 参数 / 设置。码随
  `FDreamShaderTextError` 一起挂在 `FText` 上。见 [诊断](../diagnostics/index.md)。
- **线上格式没有任何变化** —— `FDreamShaderDiagnosticRecord::Code` 和
  `diagnostics.json` 里的 `"code"` 字段本来就存在。
- 生成器还不会抛出错误码：文档覆盖 112 个码，而全部 659 条消息按阶段分组的完整目录一直是权威的那份清单，
  直到生成器剩下的约 561 个抛出点也被标注。

<a id="localization--the-editor-ui-speaks-your-language-the-wire-format-does-not"></a>

### 本地化 —— 编辑器 UI 说你的语言，线上格式不说

- 面向编辑器的文本 —— *DreamShader Gen* 页、设置分区、慢任务进度、parser 与反编译诊断 —— 从 `FString` 字面量迁到 `LOCTEXT` / `NSLOCTEXT`，承载它们的诊断记录也改成 `FText`。简体中文随之附带：
  `Content/Localization/DreamShader/zh-Hans/DreamShader.locres`，经 `DreamShader.uplugin` 中的
  `LocalizationTargets` 条目以 `Editor` 加载策略载入。每个改过的签名旁边都保留了 `FString`
  重载。感谢
  [@youli42](https://github.com/youli42) —— PR [#24](https://github.com/TypeDreamMoon/DreamShader/pull/24)。
- **`diagnostics.json`、`diagnostics/*.json`、`bridge.db` 和预览 WebSocket 在任何编辑器语言下都保持英文。** VSCode 和 Rider 扩展要解析它们，所以一个被翻译的编辑器不能改动其中的任何一个字节。`FTextInspector::GetSourceString` 只解决了一半问题：它对普通 `LOCTEXT` 返回源字符串，但对 `FText::Format` 的结果返回的是*已本地化*的替换显示结果 —— 只要有一个消息点改用 `FText::Format`，翻译过的文本就会漏到线上。`ToInvariantWireString` 改为重放 `FText` 的历史 format 数据 —— 源模板、每个参数按 invariant 文化渲染、嵌套 format 递归处理、关掉数字分组以免 `12345` 变成 `12,345` —— 每一次 JSON / SQLite / WebSocket 写入都过它。

<a id="plugin-source-roots"></a>

### 插件源码根

- 每个已启用且附带 `DShader` 文件夹的插件现在都会贡献自己的源代码根，因此插件可以携带构建其材质的 `.dsm` / `.dsf` / `.dsh` 文件，而不必把它们停在项目树里。自 `1.2.0` 起 `Root="Plugin.<Name>"` 已经能够把资产*写入*插件；缺的是源代码那一半。发现、依赖图和全量生成都会自动认到插件根，无需额外配置。见[项目布局](../getting-started/project-layout.md)。
- *Project Settings ▸ DreamPlugin ▸ Dream Shader ▸ Paths ▸ **Scan Plugin Source Directories***（`bScanPluginSourceDirectories`，默认开启）可以关闭插件扫描。
- **插件根下的文件默认落到它自己插件的挂载点。** `Plugins/MoonToon/DShader` 下没有 `Root=` 属性的 `.dsm` 现在生成到 `/MoonToon`，而不是 `/Game`——源代码和资产留在同一个发布它们的插件里。只有缺失或全为空白的 `Root` 才会被默认化，因此 `Root="/"` 可以显式选回 `/Game`。
- **导入永不跨根。** 一个文件只针对自己所在的根及该根的 `Packages` 文件夹解析其导入。两个插件携带相同的相对路径不再会互相遮蔽，禁用一个插件也不会静默改变另一个根里导入的含义。
- **带根限定的导入**——`import "Plugin.MoonToon:Shared/Toon.dsh";`——是刻意跨根的唯一方式。限定符是 `Project`、`Plugin.<Name>` 或 `Plugins.<Name>`（`/` 与 `.` 写法相同），大小写不敏感。那个 `:` 是承重的：`Plugin.MoonToon/Shared/Common.dsh` 无法与经过一个同名文件夹的相对路径区分开。见[导入](../language/import.md)。
- 源代码目录监视器为每个根注册一个监视，`DreamShader.code-workspace` 为每个根列出一个 `folders` 条目，*DreamShader Gen* 页在行副标题里用根名标出插件根文件——输入插件名即可筛选出它自带的全部内容。

<a id="editor-and-generation"></a>

### 编辑器与生成

- **图布局现在也会作用于内存材质。** 交互式编译是纯内存的，而摆放环节过去对它们完全跳过 —— 因此保存之后你看到的图不是一张布局糟糕的图，而是一张*未经布局*的图：一根由构造坐标堆成的高大单列，连线横穿其间。*Project Settings ▸ … ▸ Compiler ▸ **Lay Out In-Memory Graphs***（`bLayoutInMemoryGraphs`，默认开启）可以把它重新关掉。见[项目设置](../settings/project.md)。
- **自动布局按节点的真实尺寸摆放节点。** 过去每个节点都被假定为 `320 × 150`，并按固定的 `420 × 220` 网格排布，于是任何绘制高度超过 220 的节点都放不下 —— 光是 `TextureSample` 的预览缩略图就有 106，`Custom` 节点每多一个引脚就长出一行 —— 这些节点会与邻居重叠，并从本应框住它们的注释框中顶出去。现在尺寸根据节点控件实际组装的内容估算。
- **长边拥有专用车道，链路会被拉直。** 一条跨越多个层级的边现在会在它穿过的每一列中预留一个占位槽。交叉消减只比较相邻层级的节点，所以没有车道就完全看不到这些边 —— 这正是让一张图即便每一列单独看都很整齐、整体却如同一团乱线的原因。随后摆放会运行四遍拉直，每遍用保序回归修复该列。
- **块按列打包，而不是堆成一摞。** 一个带有多个连接输出的材质过去会变成一条数万单位高的带状物。现在块按 `max(2400, sqrt(totalArea / 1.6))` 的高度预算打包成列。
- ***DreamShader Gen* 页从 bridge 读取诊断**，而不是重新解析 bridge 刚写完的 `diagnostics.json`。一个值得知道的后果：这些记录只在编辑器会话期间存在，所以在你重新编译之前，该页不再显示*上一次*会话遗留的错误。

<a id="fixed-4"></a>

### 修复

- **插件在 UE 5.5 和 5.6 上重新能编了。** 五个 UE 5.7 API 用得没有版本门，于是插件
  只能在它被写出来的那个引擎上编译。五个现在都走
  `DreamShaderVersionCompat.h`：`Materials/MaterialParameters.h`（5.7 及之后）、
  `UMaterialExpression::ShouldShowPreview()`（5.7）、`UMaterialExpressionScalarParameter::ControlType` 及
  其两个邻居（5.7）、`UMaterialExpressionCustomOutput::GetInputValueType`（5.6），以及六个引擎
  表达式类 —— `SceneDepth`、`SceneColor`、`ObjectRadius`、`ObjectBounds`、`PerInstanceRandom`、
  `PerInstanceFadeAmount` —— 它们 `UCLASS()` 且没有导出宏，在 5.6 之前其 `StaticClass()` 在
  插件里根本解析不到。最后这个是 `LNK2019`，而不是编译错误：它能通过
  每一项编译期检查，只有一次完整的 `RunUAT BuildPlugin` 能看见它。5.6 以下改为按
  script path 查类，所以 `UE.*` 内置行为不变。
- 两处 unity 构建掩盖的缺失 include（`UObject/Package.h`、
  `Engine/EngineTypes.h`），非 unity 编译才暴露。

<a id="tools"></a>

### 工具

- **编辑器 bridge 现在会回答，也会说明自己活着。** 它过去只有入站的一半，别的什么都没有：一个请求不产生回复、不产生错误文件、也没有日志行 —— 格式错误的请求直接消失 —— 而客户端想知道编辑器是否在运行，唯一的办法是找 `bridge.db` 然后祈祷。现在 bridge 发布 `Bridge/status.json`（protocol、pid、工程、插件版本、`busy` /
  `busyAction`、`lastResult`、心跳），每 2 秒重写一次，并在**退出时删除** —— 所以文件缺失确定地意味着「没在运行」，而不是「超时了」。任何带
  `requestId` 的请求都会在 `Bridge/Responses/<requestId>.json` 里得到 `ok`、`durationMs`、`message` 和本次
  编译的诊断。新增了 `ping` 动作；错误的 scope、缺失的 `sourceFile` 和未知动作
  现在是错误响应而不是静默无操作。见 [编辑器工具](../tools/editor-integration.md)。
- **`.skill/build-plugin.ps1`** —— 在一串引擎根上跑 `RunUAT BuildPlugin`，每个一行 `PASS` / `FAIL`，退出码等于失败的个数。上面那五个没加版本门的新引擎 API 就是它抓到的：一次针对单一引擎的编辑器构建做不到，任何编译期检查也做不到，因为五个里有一个是链接错误。
- **`Tools/Localization/localization_lint.ps1`** —— 检查 `LOCTEXT_NAMESPACE` 的 define / undef 配对，拒绝它出现在头文件里，并标出 gather 看不见的字面量，带一个 `I18N-EXEMPT` 逃生口，用于那些刻意不是显示文本的文本。

<a id="151---2026-08-02"></a>

## 1.5.1 - 2026-08-02

只有文档和工具变化。插件代码没有改动，所以停在 `1.5.0` 的工程不需要任何迁移。

<a id="added-3"></a>

### 新增

- `.skill/` —— 一套 DreamShaderLang 的 agent skill，采用 Claude Code `SKILL.md` 格式：
  `dream-shader-create`（一段描述变成一个能证明编译通过的 `.dsm`）、
  `dream-shader-optimize`（反编译输出变成一份人会写的源码）、
  `dream-shader-decompile`、`dream-shader-verify` 和 `dream-shader-diagnose`。
- `.skill/dsc.ps1` —— 包在 `-run=DreamShader` 外面的无头驱动。它从
  `.uproject` 的 `EngineAssociation` 解析引擎、向上走找到工程、去掉每条 `LogInit`
  被重复回显的那一份 `LogDreamShader` 行、并对照 git 分类本次运行写出的每个资产。`-CleanNew`
  随后精确删除其中未被跟踪的那些，并清掉空掉的文件夹。
- `.skill/sync-skills.ps1` —— 把这棵树发布到 `.claude/skills`，同时按目标位置改写相对的 `Docs/`
  链接和驱动路径。`-Check` 在有漂移时以 `1` 退出。

<a id="changed-3"></a>

### 变更

- 两份 README 围绕参考手册重构。那些已经变成
  `Docs/` 页面缩略副本的章节 —— Properties、Graph、MaterialAttributes、Substrate、Material Layers、VirtualFunction、
  配置、发布 —— 改为链接到真正拥有它们、也真正会被维护的那一页。459 行 → 279 行，两种语言保持结构一致。

<a id="fixed-5"></a>

### 修复

- 发布归档现在会带上 `Shaders/`、`README.zh-CN.md` 和 `.skill/`。到 `1.5.0` 为止，打包
  步骤复制的是一份七项白名单，并且**静默**跳过缺失的条目，所以归档安装出来的插件没有
  `Shaders/DreamShaderBuiltins.ush` 可以给 `/Plugin/DreamShader` 虚拟着色器目录解析。

<a id="150---2026-08-02"></a>

## 1.5.0 - 2026-08-02

统一编译路径的一个版本。两条旧 backend 合并成唯一的 **ThinCustom** 路径，
编辑器里生成的材质默认只活在内存里，新增的 **Material Content Browser** 页签就是
查看它们的地方。语言侧拿到了可省略的 section `=`、`Group()` 作用域和
`Slider()`。

<a id="language-dreamshaderlang-15"></a>

### 语言（DreamShaderLang 1.5）

- `=` section 现在是**可选的**：写 `Properties { … }`、`Settings { … }` 和 `Graph { … }`
  时可以不写赋值号。两种写法都有效 —— 见
  [Section](../language/sections.md)。
- `Properties Group("Name") { … }` 把一个 group 打在它包住的每个参数上。group 可以嵌套、可以组合，
  因此 `Group("Outer") { Group("Inner") { … } }` 得到 `Outer|Inner` —— 见
  [元数据与分组](../parameters/metadata.md)。
- `Slider(min, max)` 简写用于设置标量参数的 UI 范围。它展开成反射属性
  `SliderMin` / `SliderMax`，也是唯一一个不带 `=` 的 metadata 条目。
- 资产路径可以直接跟在 `=` 后面，裸的带引号路径也被接受 —— 见
  [资产引用](../parameters/path.md)。
- 单输出函数可以作为返回值使用（`x = Fn(…)`），`Graph` 内置现在也与
  `Function` 路径一致：`fract`、`mod` 和 `fmod` —— 见 [数学内置](../builtins/math.md)。
- live preview 流式推送让编辑器和语言服务器的预览在你输入时保持同步。
- `true` / `false` 是 graph 字面量，会落成 `StaticBool` 节点，因此
  `opt StaticBool X = false` 输入默认值能生成 Unreal 所要求的 Preview pin 节点 —— 它
  对 static-bool 输入会忽略 `PreviewValue`。
- `StaticBool` 在调用点按单分量类型解析。
- token 本身不带维度的纹理参数类型 —— `TextureObjectParameter` —— 从赋给它的默认资产上取
  维度，因此 `Texture2DArray`、`TextureCube` 或 `VolumeTexture`
  默认值都能被接受。`Texture2D`、`Texture2DArray` 这类显式 token 仍然严格
  校验。

<a id="backend--one-unified-compilation-path"></a>

### Backend —— 统一为一条编译路径

- `Graph` 和实验性的 `Instance` 两条 backend 合并成唯一的 **ThinCustom**
  路径：DreamShaderLang 编译成一张真实的节点图，构建在隐藏的 base `UMaterial` 上，外面包一层
  薄的 `UDreamShaderMaterialInstance`。引擎按原生方式编译和枚举这个材质，因此
  Substrate、static switch、virtual texture、MaterialAttributes 和 cook 正确性全部
  来自那张真实的图。见 [Backend](../settings/backend.md)。
- 与旧 `Graph` backend 的 SM6 渲染结果逐位一致，已在 Unlit、
  带纹理和 DefaultLit MaterialAttributes 三类用例上验证。
- 隐藏 base 是实例的**子对象** —— 一个资产、一个 package，在
  Content Browser 里不可见，没有独立的 `MB_DreamThinBase_*` 兄弟资产，cook 时也没有跨 package 的父级 import
  可丢。见 [内存材质](../generation/ephemeral.md)。
- 单个 **Default Compiler Backend** 项目设置取代了旧的 In-Memory 开关。见
  [项目设置](../settings/project.md)。

**自 1.5.0 起弃用。**请改用 `ThinCustom`。

`Backend = "Instance"` 和 `DefaultBackend = Instance` 保留为
`ThinCustom` 的**别名**。旧的无图 instance backend 已经退役，运行时不再存在 `Instance`
backend。这个写法保留一个弃用窗口，让既有源码继续编译，
并且不产生任何诊断。

<a id="editor--material-content-browser"></a>

### 编辑器 —— Material Content Browser

- 在 *Tools ▸ DreamShader* 下新增 **DreamShader Material Content Browser** 页签，包含两个页面：
  **Project** 用于浏览、过滤和检视 `/Game` 下的每个材质与材质实例，
  含完整继承链；**Dream Shader Gen** 是源文件列表，带实时预览、
  搜索、过滤、全量编译和加载期错误展示。见
  [编辑器工具](../tools/editor-integration.md)。
- 可以从任意材质出发、通过文件夹选择器创建材质实例，也可以按需把
  纯内存（仅预览）材质 **materialize** 到磁盘。
- 新增 Content Browser 右键菜单操作，以及显示 / 隐藏 DreamShader 纯内存
  材质的开关。

<a id="decompiler"></a>

### 反编译导出

- Substrate 材质和被重命名的 graph 通道现在能忠实 round-trip：导出器从**写入 mask** 推导
  通道 swizzle，而不是从通道名，因此重新编译出的材质与源码逐位一致。见 [反编译导出](../tools/decompiler.md)。

反编译一个手写材质、再生成回去，在一些 Unreal 本身完全接受的图形状上会失败。这批问题是在 LGUI 的
`LexUI_ImageAndFont`、`LexUI_RectBlock` 和 `MF_LexUI_SDF_Font` 上发现的。

- switch 类节点 —— `StaticSwitch`、`FeatureLevelSwitch`、`QualitySwitch`、`ShadingPathSwitch`、
  `VertexInterpolator` 等 —— 不报告输出值类型，于是「按 float4 处理」的兜底把它们连同整条下游一起放大。
  由 float3 材质函数输出喂入的 `AppendVector` 会被写成 `float5(...)`；现在 append 会钳到 float4，
  分量数仍然对不上时发出警告。
- `VertexColor` 按 float4 导出，alpha pin 的 swizzle 才成立。此前它按
  RGB pin 定型，会在三分量值上写出 `.a`。
- 输入自身的通道 mask 现在**替换**所连 pin 的 mask，而不是叠加在上面，
  因此图里连的是 RGB pin 却 mask alpha 时，不会再出现 `.rgb.a`。
- `StaticBool` 函数输入保留 `StaticBool` 类型 token。`bool` 声明的是标量 pin，
  此前这种输入会变回 float，并拒绝传给它的每一个 static-bool 值。
- 注释、`#Region` 和 description 文本里的换行与制表符会被转义，多行
  注释不再把指令拆到多行。
- Custom 节点的 additional output 现在在该节点的**每一次**发射中都会声明，读取其中一个也不再改写节点自身的
  返回类型。此前没有选中额外输出的那次发射会产出一个缺少该输出的节点，而给它赋值的代码体会在
  shader 编译阶段以 `use of undeclared identifier` 失败 —— 远在生成报告成功之后。

<a id="fixed-6"></a>

### 修复

- Cook：资产只在 cook **director** 上落盘，生成错误现在会让 cook 失败，而不是发出一个陈旧资产。
- 生成过程拒绝覆盖不是 DreamShader 生成的资产，并在清空目标材质之前先校验图语法。
- 生成 include 的身份改为对**项目相对**源路径取哈希；记录的源路径也是项目相对的，不再携带生成时间戳。
- 运行时构建：给编辑器专用的 `UEnum::HasMetaData` 调用加了保护，非编辑器和 Shipping
  （商店）构建可以编译（#12）。
- Bridge：为 UE 5.8 改用 `FCoreDelegates::GetOnPostEngineInit`，并把 *Clean Generated
  Shaders* 限制在 `Intermediate` 内、改成逐文件删除。

<a id="compatibility"></a>

### 兼容性

- Unreal Engine `5.3` 到 `5.8`（Win64）。

<a id="source-snapshot---2026-06-10"></a>

## 源码快照 - 2026-06-10

介于 `1.4.0` 和 `1.5.0` 之间进入源码树的改动。

<a id="automation-baseline"></a>

### Automation 基线

- 新增 DreamShader automation 测试基线，覆盖最小材质解析、最小材质生成、`.dsf` 加 `import` 生成、Substrate 材质生成、source-hash skip 和 commandlet 单源编译 smoke 测试。
- 测试会在 `DShader/Tests/Automation` 下写入临时 `.dsm` / `.dsf` / `.dsh` 文件，
  在 `/Game/DreamShaderTests/Automation/…` 下生成临时资产，运行后清理。
- `source hash is unchanged` 是未变更源文件跳过重复生成的稳定断言文本。
- 针对 automation 运行新增显式桥接关闭开关 `-NoDreamShaderEditorBridge`，避免 Bridge 启动扫描
  干扰测试日志。

<a id="140---2026-06-06"></a>

## 1.4.0 - 2026-06-06

<a id="compatibility-1"></a>

### 兼容性

- 新增 Unreal Engine `5.3` 到 `5.7` 的兼容覆盖。
- 已在 Win64 上通过单插件 `RunUAT BuildPlugin` 验证 UE `5.3`、`5.4`、`5.5`、`5.6` 和 `5.7` 的构建。
- 仍以 UE `5.7` 作为主要开发版本；UE `5.3` 和 `5.4` 在 Windows 上可能需要 MSVC `14.38`
  工具链。

<a id="substrate"></a>

### Substrate

- 补齐 Substrate 生成链路：`Substrate` 图值、`Base.FrontMaterial`
  输出绑定和 `Substrate.*` wrapper。Substrate 本身需要 **UE 5.4 或更新版本**并在项目中启用
  Substrate —— 见 [Substrate 节点](../builtins/substrate.md)。
- 生成器、反编译器、类型传播和编辑器 manifest 都会识别
  Substrate 类型及其 wrapper。

<a id="material-preview"></a>

### 材质预览

- 编辑器 Bridge 可以解析 `.dsm` 源文件、生成预览材质，并写入
  `Saved/DreamShader/Bridge/preview.json` 与 `Preview/*.png`。
- 新增本机 WebSocket 预览服务，默认监听 `127.0.0.1:17864`，向
  VSCode 预览面板提供结果和连续帧。

<a id="139---2026-05-29"></a>

## 1.3.9 - 2026-05-29

<a id="maintenance"></a>

### 维护

- 插件版本元数据更新到 `1.3.9`。
- README、发布说明和文档引用同步到当前语言
  能力。

<a id="138---2026-05-25"></a>

## 1.3.8 - 2026-05-25

<a id="texture-support"></a>

### 纹理支持

- 新增 `VolumeTexture` property 解析、代码生成和默认纹理处理。
- 生成代码时保留纹理对象子类型，因此 `Texture2D`、`Texture2DArray`
  和 `VolumeTexture` 输入会以正确的 Unreal 纹理类型传入生成的 HLSL。

<a id="plugin-cleanup"></a>

### 插件清理

- 移除项目设置和文档中的内置 shader library path 支持。

<a id="137---2026-05-18"></a>

## 1.3.7 - 2026-05-18

<a id="decompiler-1"></a>

### 反编译器

- 泛型 `UE.Expression(...)` 反编译会导出反映的字面属性，因此不受支持的
  `MaterialExpression` 节点能保留更多可编辑状态。
- 连接了图输入的 `TextureSampleParameter2D` 节点——例如 UV 坐标——会
  导出为图表达式，而不是简单的 `Properties` 声明。
- 修复了带动态命名输入和自定义输出类型元数据的 `MaterialExpressionCustom` 节点的反编译。

<a id="performance"></a>

### 性能

- 改进了极大反编译材质的导入性能：减少逐节点 package 变脏、限制进度文本刷新频率，并在大型生成图上跳过自动布局。

<a id="136---2026-05-12"></a>

## 1.3.6 - 2026-05-12

<a id="build-fixes"></a>

### 构建修复

- `DreamShaderSettings.h` 显式包含 `MaterialDomain.h`，因此直接包含
  设置头的项目能可靠解析 `EMaterialDomain`。

<a id="135---2026-05-11"></a>

## 1.3.5 - 2026-05-11

<a id="shaderfunction-calls"></a>

### ShaderFunction 调用

- `Graph` 接受语句风格的多输出 `ShaderFunction` 和 `VirtualFunction` 调用：
  位置输入在前，输出目标变量在后。

<a id="dream-shader-function-files"></a>

### Dream Shader Function 文件

- 为可复用的生成式 `ShaderFunction` 资产新增 `.dsf` Dream Shader Function 文件。
- `.dsm` 和 `.dsf` 文件可以导入 `.dsf` 文件，因此生成的函数可在
  不同 DreamShader 源文件之间复用。
- 新增 `.dsf` 源文件发现、依赖追踪以及 VSCode workspace 文件关联。

<a id="decompiler-2"></a>

### 反编译器

- 新增 Content Browser 导出操作：`UMaterial` 导出为 `.dsm`，`UMaterialFunction` 导出为 `.dsf`。
- 反编译文件写入 `DShader/Decompiled/Materials` 或
  `DShader/Decompiled/Functions` 下，并使用唯一的文件名。
- 常见常量、参数、算术节点、swizzle、纹理采样、Custom 节点和
  MaterialFunction 调用会导出为 DreamShader 图文本；较少见的反射节点会回退为
  `UE.Expression(...)`。

<a id="134---2026-05-11"></a>

## 1.3.4 - 2026-05-11

<a id="output-initializers"></a>

### 输出初始化器

- `Outputs` 接受带初始化的声明，例如 `vec3 Color = Tint;`。
- `Shader` 块可以配合空的 `Graph = {}` 块使用带初始化的输出声明。

<a id="133---2026-05-11"></a>

## 1.3.3 - 2026-05-11

<a id="graph-swizzles"></a>

### 图 Swizzle

- 修复了向量 property 的分量数问题，因此 `vec2` / `vec3` property 会通过 `RG` / `RGB`
  绑定，而不再总是使用 `RGBA`。
- 通过生成显式的 `ComponentMask` 和
  `AppendVector` 节点，修复了 `.gbr` 这类非顺序 swizzle。

<a id="132---2026-05-11"></a>

## 1.3.2 - 2026-05-11

<a id="material-function-generation"></a>

### 材质函数生成

- 插件版本元数据更新到 `1.3.2`。
- 重新生成时会保留生成的 `ShaderFunction` 输入和输出 ID，因此普通 Unreal 材质中的既有
  `MaterialFunctionCall` 节点会保持其连接。
- Graph 和 Custom/HLSL 生成路径会跳过未使用的生成 property 节点。
- 改进了生成节点的摆放，并避免对 DreamShader 生成的材质图执行 Unreal 的全量自动布局。
- 修复了重新生成已打开、且其表达式仍被编辑器持有的材质函数资产时发生的崩溃。

<a id="131---2026-05-09"></a>

## 1.3.1 - 2026-05-09

<a id="function-calls"></a>

### 函数调用

- 单输出 `Function` 和 `GraphFunction` 调用可以用作值表达式，例如
  `Color = Texture::Sample2DRGB(BaseTex, UV0);` —— 见[调用](../graph/calls.md)。
- 多输出 `Function` 和 `GraphFunction` 调用仍然需要显式的 out 变量，例如
  `Texture::Sample2D(BaseTex, UV0, Color, Alpha);`。

<a id="graph-functions"></a>

### Graph 函数

- 新增顶层和 namespaced `GraphFunction` 块，用于可复用的 HLSL Custom 节点逻辑。
- `GraphFunction` 仍为 HLSL，但其主体内的 `UE.*` 调用会被转换为材质节点，
  并作为生成的输入传入 Custom 节点。
- 新增 GraphFunction 参数验证、递归调用检测以及显式 out 变量写回。

<a id="130---2026-05-08"></a>

## 1.3.0 - 2026-05-08

<a id="shader-layer-functions"></a>

### Shader Layer 函数

- 新增顶层 `ShaderLayer(Name="...", Root="...")` 和 `ShaderLayerBlend(Name="...", Root="...")`
  块。
- 生成的 layer 资产使用 Unreal 原生的 `UMaterialFunctionMaterialLayer` /
  `UMaterialFunctionMaterialLayerBlend` 类。
- `ShaderLayer` / `ShaderLayerBlend` 复用现有的 `Properties`、`Inputs`、`Outputs`、
  `Settings` 和 `Graph` 节。
- 新增验证：Shader Layer 块必须恰好输出一个 `MaterialAttributes` 值，
  且 Shader Layer Blend 块必须声明至少两个 `MaterialAttributes` 输入。
- 向量参数 property 在 `Graph` 中保留其 RGBA 输出，因此 `.a` / `.w` 读取
  alpha 以及向较低分量数赋值时会自动使用前置通道。

**自 1.3.0 起弃用。**请改用 `ShaderLayer`。

`MaterialLayer` 和 `MaterialLayerBlend` 保留为兼容别名并发出警告。新源码
应使用 `ShaderLayer` 和 `ShaderLayerBlend`。

<a id="1210---2026-05-08"></a>

## 1.2.10 - 2026-05-08

<a id="vscode-materialexpression-manifest"></a>

### VSCode MaterialExpression Manifest

- 新增编辑器侧导出，将反射得到的 `UMaterialExpression` 元数据写入
  `Saved/DreamShader/Bridge/material-expressions.json`。
- manifest 会在编辑器 bridge 启动时以及打开 DreamShader VSCode workspace 时
  刷新。
- 导出的元数据涵盖表达式类名、可编辑的反射属性、表达式
  输入、输出引脚以及推断出的 DreamShader `OutputType` 提示。
- Release workflow 会下载最新的 `dreamshader-language-support` GitHub Release 资产，
  并将其附加到 DreamShader 发布中。

<a id="128---2026-05-05"></a>

## 1.2.8 - 2026-05-05

<a id="project-settings-and-editor-entry-points"></a>

### 项目设置与编辑器入口点

- 插件版本元数据更新到 `1.2.8`。
- `DreamShader.uplugin` 文档链接现在指向 `https://lang.64hz.cn/`，
  支持链接指向 GitHub 主页。
- Unreal 项目设置分类从 `Plugins` 移动到 **`DreamPlugin`**，该节
  显示为 **Dream Shader**，描述为 `Dream Shader Settings` —— 见
  [项目设置](../settings/project.md)。
- 在 `Editor` 分类下新增 `OpenInNewWindow` 设置。默认开启时，从 Unreal 打开
  DreamShader VSCode workspace 会打开新窗口；关闭时会追加 `--reuse-window`，
  并复用已有的 VSCode 窗口。

<a id="127---2026-05-05"></a>

## 1.2.7 - 2026-05-05

<a id="unreal-57-compatibility"></a>

### Unreal 5.7 兼容性

- 插件版本元数据更新到 `1.2.7`。
- 生成器会获取 `UMaterialExpressionFunctionInput` /
  `UMaterialExpressionFunctionOutput` 的头文件依赖，与更新的 Unreal 材质函数节点构建
  环境保持一致。
- Moon Engine 特有的 `MooaEncodedAttribute0` 到 `MooaEncodedAttribute4` 输出现在
  被包裹在 `MOON_ENGINE` 条件编译中，因此普通引擎构建不再依赖
  这些自定义材质属性。
- 此版本不改变 `.dsm` / `.dsh` 语法；它涉及的是输出目标和引擎
  兼容性。

<a id="126---2026-04-30"></a>

## 1.2.6 - 2026-04-30

<a id="shaderfunction-properties"></a>

### ShaderFunction 属性

- 为 `ShaderFunction` 新增 `Properties` 节，用于声明生成的材质函数
  局部使用的 property 和辅助节点。
- 新增 `const` property 声明，用于那些不可外部调整的标量、向量和纹理辅助节点。
- `ShaderFunction` `Inputs` 预览默认值可以引用同一函数的 `Properties`，
  例如 `opt Texture2D BaseColorTex = PreviewTex;`。

<a id="125---2026-04-30"></a>

## 1.2.5 - 2026-04-30

<a id="material-attributes"></a>

### 材质属性

- 新增 `MaterialAttributes` 作为 `Shader`、`ShaderFunction` 和
  `VirtualFunction` 签名的图值类型。
- 新增类结构体成员写入，例如 `Attrs.BaseColor = Color;` 和
  `Attrs.Roughness = Roughness;`。
- 新增 `Base.MaterialAttributes = Attrs;` 输出绑定，会自动在生成的材质上启用 *Use Material Attributes*。
- `MaterialAttributes` 值可以从生成或虚拟材质函数返回，并
  通过 `Graph` 赋值传递。

<a id="124---2026-04-30"></a>

## 1.2.4 - 2026-04-30

<a id="parameter-reflection"></a>

### 参数反射

- 文档中记载的逗号风格元数据后缀已替换为声明尾部的分号分隔反射
  块。
- 参数反射块可以设置生成的参数节点所暴露的任意可反射 `UMaterialExpression` 属性。
- 基础的 `float`、向量和纹理简写声明使用与显式参数节点声明相同的反射
  路径。
- 纹理采样参数可以配置可反射属性，例如 `SamplerType`、
  `SamplerSource`、`MipValueMode`、`AutomaticViewMipBias`、`ConstCoordinate` 和 `ConstMipValue`。

<a id="123---2026-04-29"></a>

## 1.2.3 - 2026-04-29

<a id="parameters"></a>

### 参数

- 为材质 `Properties` 和函数输入/输出声明新增声明元数据 `[Group="...", SortPriority=32, Description="..."]`。
- 新增显式参数节点声明：`ScalarParameter`、`VectorParameter`、
  `TextureObjectParameter`、纹理采样参数系列、`StaticBoolParameter` 和
  `StaticSwitchParameter` —— 见 [属性类型](../language/properties.md)。
- 新增 `StaticSwitchParameter` 图调用，例如
  `UseDetail(True=detailColor, False=baseColor)`。
- 为材质参数集合读取新增 `UE.CollectionParam(Collection=Path(...), Parameter="...")`。

<a id="function-defaults"></a>

### 函数默认值

- 为 `ShaderFunction` 和 `VirtualFunction` 新增 `opt` 输入。
- 为可选材质函数输入新增 `default` 调用参数，保留 Unreal
  FunctionInput 预览默认值。
- 生成的 `ShaderFunction` 资产会将输入和输出描述以及排序优先级写入
  FunctionInput / FunctionOutput 节点。
- VirtualFunction 复制、创建和同步在可用时发出可选输入、预览默认值和 pin 元数据。

<a id="122---2026-04-29"></a>

## 1.2.2 - 2026-04-29

<a id="virtualfunction-workflow"></a>

### VirtualFunction 工作流

- `CreateVirtualFunction` 会复用所选材质函数的现有声明，
  而不是创建重复的 `.dsh` 文件。
- 当匹配的声明已存在时，材质函数 `DreamShader` 菜单会显示
  `OpenVirtualFunction` 和 `Copy Virtual Function Reference`，而不是创建和
  复制定义操作。
- `OpenVirtualFunction` 会在 VSCode 中打开现有声明，并尽可能跳转到声明
  位置。
- 为 `DShader` 下的 `VirtualFunction` 声明新增启动时验证和刷新，
  报告缺失的源 `UMaterialFunction` 资产并更新已更改的签名。

<a id="import-compatibility"></a>

### 导入兼容性

- `import "File.dsh"` 在 Unreal 生成器导入环节中，无论是否带结尾分号
  均可工作。

<a id="121---2026-04-29"></a>

## 1.2.1 - 2026-04-29

<a id="editor-workflow"></a>

### 编辑器工作流

- 将单个材质函数工具栏操作替换为 `DreamShader` 下拉菜单。
- 向材质函数编辑器工具栏和材质函数资产上下文菜单新增 `CopyVirtualFunction`、`CreateVirtualFunction` 和 `CopyVirtualFunctionCall`。
- `CreateVirtualFunction` 会在配置的 `DShader/VirtualFunctions` 目录下写入一个 `.dsh` 声明，
  并在默认外部编辑器中打开它。
- `CopyVirtualFunctionCall` 会使用生成的输入名称和
  第一个输出复制一个可直接粘贴的 `Graph` 调用。
- 在编辑器 Tools 菜单和 DreamShader 工具栏
  区域新增 *Open Dream Shader Workspace (VSCode)*。它会写入 `DShader/DreamShader.code-workspace`，
  在可用时用 VSCode 打开，并回退到默认编辑器或记事本。

<a id="release"></a>

### 发布

- 新增 GitHub Actions 发布工作流，从版本标签或手动工作流调度打包插件源码并发布 GitHub
  Release。

<a id="120---2026-04-28"></a>

## 1.2.0 - 2026-04-28

<a id="virtualfunction"></a>

### VirtualFunction

- 为现有的 Unreal `UMaterialFunction` 资产新增 `VirtualFunction(Name="...")` 声明。
- `VirtualFunction` 调用可以像 `ShaderFunction` 调用一样从 `Graph` 中工作，
  而不会生成或
  覆盖所引用的资产。
- `Options.Asset` 支持 `Path(Game, "...")`、`Path(Engine, "...")`、
  `Path(Plugin.PluginName, "...")` / `Path(Plugins.PluginName, "...")`，以及完整的 Unreal 对象
  路径。
- 新增材质函数上下文菜单和材质编辑器工具栏操作，可复制包含输入、输出和选项的完整
  `VirtualFunction` 声明。

<a id="asset-roots"></a>

### 资产根

- `Root="Plugin.PluginName"` 映射到项目插件内容根，将生成的
  资产物理保存到 `[Project]/Plugins/PluginName/Content` 下。
- `Plugins.PluginName` 和 `Plugins/PluginName` 保留为兼容写法。

<a id="tooling"></a>

### 工具

- VSCode 扩展新增 `VirtualFunction` 高亮、补全、悬停、代码片段、
  签名帮助和诊断。
- 项目内容插件名称也会在 `Path(Plugins.)` 内完成补全。

<a id="114---2026-04-28"></a>

## 1.1.4 - 2026-04-28

<a id="asset-generation"></a>

### 资产生成

- 支持 `Root="Plugins.PluginName"` / `Root="Plugins/PluginName"` 作为兼容
  写法，解析结果与 `Root="Plugin.PluginName"` 完全相同。
- 这避免 `Plugins.MoonToon` 被当作普通的 `/Game/Plugins_MoonToon`
  子目录 —— 见 [资产路径](../generation/asset-paths.md)。

<a id="113---2026-04-28"></a>

## 1.1.3 - 2026-04-28

<a id="asset-generation-1"></a>

### 资产生成

- 明确了 `Root="Plugin.PluginName"` 仅指向**已启用的项目内容插件**。
- `Plugin.MoonToon` 解析到 UE 包根 `/MoonToon`，物理保存位置为
  `[Project]/Plugins/MoonToon/Content`。
- 生成器会验证目标插件位于项目 `Plugins` 目录、
  已启用、可以包含内容，并且具有 `Content` 目录。

<a id="112---2026-04-28"></a>

## 1.1.2 - 2026-04-28

<a id="language-and-generation"></a>

### 语言与生成

- 为 `Shader` 和 `ShaderFunction` 新增 `Root` 顶层属性，
  用于选择生成
  资产的根路径。
- `Root` 默认为 `Game`，也接受 `Plugin.PluginName`，将生成的 `UMaterial`
  或 `UMaterialFunction` 保存到已启用的项目内容插件根。
- 当从 `Graph` 调用 `ShaderFunction` 时，生成的 `UMaterialFunction` 资产会
  通过该函数自身的 `Root` 解析并加载。

<a id="documentation"></a>

### 文档

- 更新了插件 README、语言参考和网站中的资产生成说明。

<a id="111---2026-04-26"></a>

## 1.1.1 - 2026-04-26

<a id="fixed-7"></a>

### 修复

- 修复了 `Graph` 中带有多个 `out` 参数的 `Function` 调用，
  Custom 节点的
  尾部输出引脚收不到值的问题。
- 多输出 `Function` 的第一个 `out` 仍然通过 Custom 节点的主
  `return` 返回；第二个及后续 `out` 参数现在会生成显式赋值，把
  `__ds_*_outN` 临时变量写回对应的 Additional Output，例如
  `Output_ToonA = __ds_xxx_out1;`。

<a id="compatibility-2"></a>

### 兼容性

- 这是一个补丁版本，不需要修改 `.dsm` / `.dsh`。

<a id="110---2026-04-22"></a>

## 1.1.0 - 2026-04-22

<a id="language-and-graph"></a>

### 语言与 Graph

- 将材质和材质函数中的 `Code = { ... }` 区块重命名为 `Graph = { ... }`，
  从而把图 DSL 与 HLSL helper 层区分开来。
- 为 `Graph` 增加了基础的 `if` / `else` 支持 —— 见[控制流](../graph/if.md)。
- 改进输出表达式处理：输出绑定记录的是源文本，而不再只是
  变量名，从而可以更灵活地解析图输出。
- 改进类型转换和泛型 `UE.*` MaterialExpression 参数处理，减少
  手动补节点时的摩擦。

<a id="function-and-generation"></a>

### Function 与生成

- 新增 `Function SelfContained` 和 `Function Inline`，将 helper 依赖嵌入 Custom
  节点，这样生成的材质可以交付给未安装 DreamShader 插件的项目使用。
- 普通 `Function` 仍然生成一个从 Custom 节点引用的 `.ush` helper include。
- 新增生成 shader 文件的清理工作流，减少历史 include 残留。

<a id="packages-and-workflow"></a>

### Package 与工作流

- 新增 Package 系统，通过 GitHub 分发和安装可复用的 `.dsh` 共享库
  —— 见 [Package](../tools/packages.md)。
- 新增 import 依赖图：保存共享 `.dsh` 时会找出受影响的 `.dsm` 文件，
  并增量刷新它们。后续版本把这一机制扩展到 `.dsf` 依赖。
- 缓存配置目录，降低反复解析项目设置的开销。

<a id="diagnostics-and-compatibility"></a>

### 诊断与兼容性

- 诊断信息会携带阶段、平台和质量上下文，以区分解析、生成和材质
  编译问题 —— 见[诊断](../diagnostics/index.md)。
- 移除了 Toon 着色模型的默认映射，使生成结果不会
  意外依赖项目自定义的着色模型。
- 同步更新 README、语言参考和示例文档。

<a id="100---2026-04-21"></a>

## 1.0.0 - 2026-04-21

<a id="initial-version"></a>

### 初始版本

- `.dsm` / `.dsh` 文件模型：用文本源文件描述 Unreal 材质和共享 helper。
- `Shader(Name="...")` 生成 `UMaterial`；`ShaderFunction(Name="...")` 生成
  `UMaterialFunction`。
- `Properties`、`Settings`、`Outputs` 以及早期的图构建语法。
- HLSL 风格的 `Function` helper，生成 Custom 节点代码。
- 基线文档和示例，作为 DreamShaderLang 的第一个公开发布版本。
