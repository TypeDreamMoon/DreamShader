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
├── Generate/           # 1.x 资产生成层 (FMaterialGenerator, bTransient), 慢, 要编辑器
└── Lang/               # 2.0 前端 (ParseDreamShaderLang), 主要吃 .dss/.dsh, 纯 Core
    ├── Lexical/        # 注释 / 字符串与转义 / 每种字面量 / 每个运算符 / #pragma 尾注释
    ├── Expressions/    # 优先级 / 命名实参 / 强制转换 / 构造器 / 成员与下标链 / 初始化列表
    ├── Statements/     # 每种语句 / for 的各种形态 / switch 与缺分号负例
    ├── Declarations/   # uniform / static const / 三种 linkage / struct / #include / #pragma / /// 块
    └── Examples/       # 完整可读的整文件（语法提案里的真实例子）
```

| 子树 | 入口点 | runner | 自动化测试名 |
|---|---|---|---|
| `Parse/` | `FTextShaderParser::Parse` | `RunDreamShaderParseCorpusCase` | `DreamShader.Lang.Parse.*` |
| `Generate/` | `FMaterialGenerator::Generate*FromFile`（transient） | `RunDreamShaderGenerateCorpusCase` | `DreamShader.Lang.Generate.*` |
| `Lang/` | `ParseDreamShaderLang`（2.0 前端） | `RunDreamShaderLangCorpusCase` | `DreamShader.Lang2.Corpus.*` |

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

## 更新金样本

跑测试时加 `-DreamShaderUpdateGolden`，runner 会用**实际解析结果**重写每个 `.expected.json`。
仅人工触发，写回后务必 **review diff** 再提交，避免把回归当成新基线接受。
