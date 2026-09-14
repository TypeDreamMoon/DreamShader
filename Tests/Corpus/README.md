# DreamShader 测试语料库 (Corpus)

数据驱动单元测试的固定 fixture 树。一个通用 runner（C++）在运行时枚举本目录、对每个源文件跑对应入口点、与同名 `.expected.json` 金样本比对。

> **加一个关键字 = 往对应 `<Layer>/` 目录丢一个 `.dsm/.dsf/.dsh/.dss`（可选配一个 `.expected.json`）。不写 C++、不重新编译。**

## 目录约定

```
Corpus/
├── Parse/              # 1.x 纯解析层 (FTextShaderParser::Parse), 快, 无资产 I/O
│   ├── Lexical/        # 注释 / 字符串转义 / 字面量 / 括号平衡 ...
│   ├── TopLevel/       # Shader / ShaderFunction / Namespace / VirtualFunction ...
│   ├── Sections/       # Properties / Settings / Outputs / Inputs / Options / Graph
│   └── Types/          # float/vec/int/bool 家族 / Texture* / MaterialAttributes / Substrate
├── Generate/           # 1.x 资产生成层 (FMaterialGenerator, 允许 Ephemeral ThinCustom), 慢, 要编辑器
└── Lang/               # 2.0 前端 (ParseDreamShaderLang), 主要吃 .dss/.dsh, 纯 Core
    ├── Lexical/        # 注释 / 字符串与转义 / 每种字面量 / 每个运算符 / #pragma 尾注释
    ├── Expressions/    # 优先级 / 命名实参 / 强制转换 / 构造器 / 成员与下标链 / 初始化列表
    ├── Statements/     # 每种语句 / for 的各种形态 / switch 与缺分号负例
    ├── Declarations/   # uniform / static const / 三种 linkage / struct / #include / #pragma / /// 块
    └── Examples/       # 完整可读的整文件（语法提案里的真实例子）
├── IR/                 # 2.0 中端 (bind -> build -> passes -> validate), 纯 Core, 无资产 I/O
│   ├── Parameters/     # uniform 各种类型 / static const / 裸全局负例         (契约 §6.1)
│   ├── Material/       # material 字段图 / 不出 MakeMaterialAttributes / 边界 (§6.2, §6.13)
│   ├── Branches/       # static if -> StaticSwitch, dynamic if -> Select     (§6.3)
│   ├── Inlining/       # Helper 内联 / out 回写 / 递归负例                    (§6.4)
│   ├── Textures/       # 四种采样拼写归一                                     (§6.5)
│   ├── Swizzle/        # 规范掩码 xyzw / v[3] / 重复分量负例                  (§6.6)
│   ├── Dedupe/         # 两次相同调用合一（配一个 passes:false 的对照）       (§6.7)
│   ├── Regions/        # 文件级 + 体内 #pragma region / layout 提示            (§6.8)
│   ├── Products/       # 产物种类 / 函数 IO 密集 SortPriority / 入口规则       (§6.9)
│   ├── Reflected/      # 实参匹配 / const 属性优先 / Node 多输出 / Substrate   (§6.10)
│   ├── Includes/       # #include 合表（Shared.dsh 被 runner 跳过）           (§6.11)
│   ├── Matrices/       # 矩阵在图里没有形态                                   (DSH4361)
│   ├── Loops/          # 可展开 for / 动态上界负例                            (DSH4360)
│   ├── Custom/         # @custom 节点与 H 的标记行                            (§6.13)
│   └── Examples/       # Lang/Examples 三个文件的**副本**（见下方"两处已知的重复"）
└── Compile/            # 2.0 全链路 (FMaterialGenerator::GenerateAssetsFromFile), 慢, 要编辑器
    ├── Material/       # 最小材质 / 内联+swizzle / 静态开关 / ThinCustom / region
    ├── Function/       # 函数库（一个 export 一个资产）/ 纹理函数
    └── Errors/         # 端到端的拒绝（码要走到 1.x wire form）
```

| 子树 | 入口点 | runner | 自动化测试名 |
|---|---|---|---|
| `Parse/` | `FTextShaderParser::Parse` | `RunDreamShaderParseCorpusCase` | `DreamShader.Lang.Parse.*` |
| `Generate/` | `FMaterialGenerator::Generate*FromFile`（允许 Ephemeral ThinCustom） | `RunDreamShaderGenerateCorpusCase` | `DreamShader.Lang.Generate.*` |
| `Lang/` | `ParseDreamShaderLang`（2.0 前端） | `RunDreamShaderLangCorpusCase` | `DreamShader.Lang2.Corpus.*` |
| `IR/` | `Bind` → `BuildDreamShaderIR` → `RunDreamShaderIRPasses` → `ValidateDreamShaderIR` | `RunDreamShaderIRCorpusCase` | `DreamShader.Lang2.CorpusIR.*` |
| `Compile/` | `FMaterialGenerator::GenerateAssetsFromFile`（`.dss` 走 2.0 管线） | `RunDreamShaderCompileCorpusCase` | `DreamShader.Compiler2.Corpus.*` |

三个 runner 都在 `Source/DreamShaderEditor/Private/Tests/DreamShaderTestCommon.h`，各自的
`IMPLEMENT_COMPLEX_AUTOMATION_TEST` 在同目录的 `DreamShaderCorpus<Layer>Tests.cpp`。
后续层（`Diagnostics/`、`Roundtrip/` 等）照此平行新增，各配自己的 runner。

## 命名

| 形式 | 含义 |
|---|---|
| `<前缀>_<名字>.<ext>` | 正例。前缀编码层级：`Parse/` 用 `L_`/`T_`/`S_`/`Ty_`，`Lang/` 用 `L_`（Lexical）/`E_`（Expressions）/`S_`（Statements）/`D_`（Declarations），`Examples/` 直接用资产名 `M_*`/`MF_*`。 |
| `<名字>.bad.<ext>` | 负例。runner 见 `.bad.` 默认期望 **解析失败**（即使没有 json）。 |
| `<同名>.expected.json` | 可选金样本。缺失时用默认期望（正例=解析成功，`.bad.`=解析失败）。 |

## `.expected.json` 字段（全部可选、声明式）

```json
{
  "entryPoint": "parse",
  "outcome": "ok",                                  // ok | error
  "errorContains": ["Unterminated block"],          // error 用例: 错误串子串(全部需命中, 大小写不敏感)
  "warningsContain": ["deprecated"],                // Definition.Warnings 子串
  "definition": {                                   // outcome=ok 时对 FTextShaderDefinition 的字段断言
    "name": "DreamMaterials/M_X",
    "settings": { "Domain": "UI" },                 // 经 TryGetSetting 比对: 键大小写不敏感, 值精确
    "outputDeclarations": 1,
    "outputs": 1,
    "materialFunctions": 1,
    "materialFunction0Kind": "ShaderFunction",      // ShaderFunction | ShaderLayer | ShaderLayerBlend
    "virtualFunctions": 0,
    "codeNotEmpty": true
  }
}
```

## `lang` 金样本字段（2.0 前端，`Lang/` 子树）

`Lang/` 下每个 fixture 只跑一次 `ParseDreamShaderLang`，再按金样本断言。全部字段可选；
没有金样本时的默认期望是：正例解析成功且零错误，`.bad.` 至少一个错误。

```json
{
  "entryPoint": "lang",
  "outcome": "ok",                    // ok | error
  "errorContains":   ["DSH2105"],     // Error 级诊断的子串（每条都要有某条诊断命中）
  "warningsContain": ["DSH3220"],     // Warning 级诊断的子串
  "module": {                         // outcome=ok 时对 FModule 的结构断言
    "declarations": 6,                // Declarations.Num() —— 含 #pragma 与 #include
    "pragmas": 1,                     // FPragmaDecl
    "includes": 0,                    // FIncludeDecl（`#include` 与 `import` 同计）
    "structs": 0,                     // FStructDecl
    "uniforms": 3,                    // FVariableDecl 且 Storage == Uniform
    "constants": 0,                   // FVariableDecl 且 Storage == StaticConst
    "functions": 2,                   // FFunctionDecl（三种 linkage 全算）
    "exports": 1,                     // Linkage == Export
    "externs": 0,                     // Linkage == Extern
    "opaqueBodies": 0,                // bOpaqueBody（`/// @custom` 原样捕获的函数体）
    "entry": "M_TeleportGlow",        // 首个 IsMaterialEntry() 的函数名；没有则 ""
    "roundtrip": true                 // print -> parse -> print 逐字节一致
  }
}
```

要点：

- **`errorContains` / `warningsContain` 匹配的是 `FLangDiagnosticSink::ToWireString`**，
  形如 `DSHnnnn: message`，取不变式（源）英文，与编辑器语言无关。`Lang/` 层的
  子串比较**大小写敏感** —— 写 `DSHnnnn` 码，别写英文句子：码是契约，措辞不是。
- `entry` 是**结构判定**：返回 `void`、恰好一个 `inout material` 形参的第一个函数，
  与 linkage 无关。空串表示这个文件不是材质。
- `roundtrip` 是打印器契约的可执行版：`parse -> print -> parse -> print` 两次输出逐字节
  相同。`///` 以外的注释不进 AST，因此**不会**被打印回来 —— 这不影响 roundtrip。
- 只写你在意的字段。计数是整型精确比对，不是“至少”。
- `.bad.` 用例只断言 `outcome` 与 `errorContains`，`module` 会被跳过。
- `errorContains` / `warningsContain` 只断言**存在**：列出的每条都得命中某条诊断，但多出来的诊断
  不会失败。一个负例只写它想钉住的那一个码，别把恢复路径上的连带错误也写进去。
- **`/// @custom` 体里的词法诊断会被丢掉**（`ParseDreamShaderLang` 把 span 落在 `BodySpan`
  内的 lexical 诊断过滤掉），所以不可能写一个“体内字符报错”的 fixture。反过来的两个夹具是
  `Declarations/D_CustomBodyOpaqueToLexer.dss`（体内敌意字符 = 全绿）与
  `Lexical/L_HashNotAtLineStart.bad.dss`（体外同一个 `#` = DSH2106）。
- `Lang/` 里允许一个 `.dsm`：`Declarations/D_LegacyFrontendDsm.bad.dsm` 钉的是**前端选择**
  （`Auto` 按扩展名选 legacy，而 legacy 到 M4 才有 ⇒ DSH2199）。它不走 `Parse/` runner：
  三个 runner 各自只枚举自己那一层目录。

## `ir` 金样本字段（2.0 中端，`IR/` 子树）

`IR/` 下每个 `.dss` 只跑一次完整中端：parse → bind → build → passes → validate。全部字段可选。

```json
{
  "entryPoint": "ir",
  "outcome": "ok",                  // ok | error
  "errorContains":   ["DSH4210"],   // Error 级诊断的子串（**跨阶段**：parse/bind/lower/validate 合在一起）
  "warningsContain": ["DSH8210"],
  "passes": true,                   // false = 不跑 passes（用来做 dedupe 前后的对照）
  "irPending": true,                // 跳过 `ir` 文本比对，只校 outcome / 码 / module 计数
  "ir": "<DumpDreamShaderIRText 的输出，逐字节>",
  "module": {
    "products": 1,                  // FIRModule::Products.Num()
    "includes": 0,                  // FIRModule::Includes.Num()
    "sinks": 1,                     // Graph.Sink != INDEX_NONE 的产物数
    "productKinds": ["Material"],   // LexToString(EIRProductKind)，按产物顺序
    "productNames": ["M_X"],
    "nodeCounts": [12]              // 每个产物的 Graph.Nodes.Num()
  }
}
```

要点：

- **这一层用的 builtin 目录是手写的那一份**（`MakeDreamShaderTestBuiltinCatalog()`，在
  `DreamShaderTestCommon.h`），不是引擎反射。所以 `IR/` 的夹具只能用那份目录里声明的 builtin：
  `UE.TextureCoordinate`（别名 `TexCoord`）、`UE.VertexColor`（故意只有一个 float4 输出）、`UE.VertexColorViews`（引擎 VertexColor 的真实形状：RGB / R / G / B / A 五个通道视图输出）、`UE.Time`、`UE.SceneTexture`（三输出）、
  `UE.LinearInterpolate`（三个 pin 各带 `Const*` 孪生属性）、`UE.BlendMaterialAttributes`、
  `UE.ClearCoatNormalCustomOutput`（custom output）、`Substrate.Unlit`；材质属性只有
  BaseColor / EmissiveColor（别名 Emissive）/ Roughness / Normal / Opacity / WorldPositionOffset /
  FrontMaterial / MaterialAttributes（整组，给整体赋值用）八个。**要用真反射的用例放 `Compile/`。**
  理由是金样本的可比性：引擎多一个 pin 就整棵语料变红的金样本，说的不是编译器的事。
- **`irPending: true` 是"这条金样本的文本部分还没填"**。批次 1 写语料时 I1 的
  `DumpDreamShaderIRText` 还没定行格式，所以先把 outcome、DSHnnnn 码和结构计数钉住；等格式落地，
  跑一次 `-DreamShaderUpdateGolden`、**人工 review diff**、再删掉这个标记，夹具就变成逐字节金样本。
- **金样本不带机器和版本信息**：`ir` 文本里的语料根目录写成 `<corpus>/`（`@custom` 的源码标记会带文件路径），Compile 金样本里 fixture 自己的临时包路径写成 `<package>/`，graph dump 去掉 `asset` / `source` / `plugin` 三个根键。换机器、换检出目录、升插件版本号都不该让金样本变红。
- `.dsh` 不会被当成用例跑（头文件没有产物可断言），只作为 `#include` 的目标存在。
- **`IR/` 的 runner 不做预处理**：夹具就是已经预处理完的文本，和 `Lang/` 一样。要写 `#if` 的用例放 `Compile/`。
- `#include` 只解析到**同目录的同名文件**；真正的 include resolver（P 写的那个）由 `Compile/` 层覆盖。

## `compile` 金样本字段（2.0 全链路，`Compile/` 子树）

`Compile/` 下每个 `.dss` 会被**复制到项目 DShader 根下它自己的目录**再编译——2.0 管线里 Graph 材质和
材质函数一律落盘（没有“transient 请求”这回事；Ephemeral / Materialized 两态只属于 ThinCustom），
资产落点跟着源文件路径走，而语料目录不是源根。复制件和它产出的所有资产都由 runner 清掉。

```json
{
  "entryPoint": "compile",
  "outcome": "ok",                  // ok | error
  "errorContains": ["DSH8290"],     // 断言 1.x wire form（`DSHnnnn: message`）
  "graphPending": true,             // 跳过 graphDump 比对，只校 outcome / 码 / assets
  "assets": [
    { "name": "M_X", "kind": "Material", "nodeCount": 7, "path": "M_X.M_X" }
  ],
  "graphDump": "<每个资产的规范化 dump-graph JSON，按资产名排序拼接>"
}
```

要点：

- `name` 是**资产叶名**，不是包路径：runner 把夹具复制到哪个 scratch 目录是 runner 的实现细节，
  金样本写整条 object path 就是在断言那个细节。`path` 可选，按**后缀**匹配（给 `/// @name /Game/...` 用）。
- `kind` 用 dump 里的那一套：`Material` / `MaterialFunction` / `MaterialLayer` / `MaterialLayerBlend` /
  `ThinCustomInstance`。
- `graphDump` 的规范化只去掉两个根级键：`asset`（object path）和 `source`（root + 相对路径）——
  坐标、颜色、guid、引擎重建时追加的名字后缀，dump 本身就排除了（见 `Commandlet/DreamShaderGraphDump.h`）。
- `graphPending` 同 `irPending`：先钉 outcome / 码 / 资产清单，第一次跑通后再 `-DreamShaderUpdateGolden`
  填 dump、review diff、删标记。

## 两处已知的重复

- `IR/Examples/` 下的三个 `.dss` 是 `Lang/Examples/` 的**逐字节副本**。三个 runner 各自只枚举自己
  那一层目录，所以同一个文件没法同时属于两层；改了一边记得改另一边（`diff` 一下就知道）。
- `IR/` 与 `Compile/` 有几组同题材的夹具（字段图、静态开关、函数库、§6.13 边界）。这是故意的：
  `IR/` 钉的是节点形状，`Compile/` 钉的是它变成资产之后还是那个形状，两者会在不同的地方坏掉。

## 运行

编辑器内：`Tools > Test Automation`，筛 `DreamShader.Lang.Parse`。

命令行（headless）：

```
"F:\UnrealEngine\UE_Moon\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "I:\UnrealProject_Moon\DEV_58\MoonEngineSample\MoonEngineSample.uproject" ^
  -ExecCmds="Automation RunTests DreamShader.Lang.Parse; Quit" ^
  -nullrhi -unattended -nopause -nosplash -log
```

只跑 2.0 前端语料，把过滤器换成 `DreamShader.Lang2.Corpus`；整个 2.0 前端（语料 +
各单元测试）是 `DreamShader.Lang2`。编辑器内同理，在 `Tools > Test Automation` 里筛。

批次 1（M2+M3）新增的两层：

| 想跑什么 | 过滤器 | 快慢 |
|---|---|---|
| 中端语料 | `DreamShader.Lang2.CorpusIR` | 快（纯 Core） |
| 绑定器单元测试 | `DreamShader.Lang2.Binder` | 快 |
| IR 单元测试 | `DreamShader.Lang2.IR` | 快 |
| 前端 + 中端全部 | `DreamShader.Lang2` | 快 |
| 全链路语料 | `DreamShader.Compiler2.Corpus` | 慢（写 /Game 资产） |
| 全链路 + 与 1.x 对拍 | `DreamShader.Compiler2` | 慢 |

`DreamShader.Compiler2.Parity.*` 是 plan §8 的对拍 oracle：同一个材质分别走 1.x 的 `.dsm/.dsf`
孪生文件和 2.0 的 `.dss`，两边都编译、都 dump、都规范化，然后 diff。**diff 就是新管线的 bug，除非
另行证明。** 两边源文件写法上的差异（1.x 给了 SortPriority、Group 里带空格、描述措辞不同）在测试里
按**键名**成对剔除，每一条都在调用处写了理由；结构性的东西（节点、连线、类、默认值、材质设置、
引脚名）一条都不在剔除名单上。

## 更新金样本

跑测试时加 `-DreamShaderUpdateGolden`，runner 会用**实际解析结果**重写每个 `.expected.json`。
仅人工触发，写回后务必 **review diff** 再提交，避免把回归当成新基线接受。
