# DreamShader 文档总览

这份文档覆盖 DreamShader `1.1.0` 的核心工作流、DreamShaderLang 语法、VSCode 扩展、Package 系统和常见示例。

DreamShader 的推荐用法是：用 `.dsm` 描述材质资产，用 `.dsh` 组织共享 helper，用 `Graph` 生成材质节点，用 `Function` 编写可复用 HLSL 风格逻辑。

## 阅读路径

| 目标 | 推荐阅读 |
| --- | --- |
| 快速了解插件能力 | [../README.md](../README.md) |
| 学习完整语法 | [LanguageReference.md](LanguageReference.md) |
| 复制可运行示例 | [Examples.md](Examples.md) |
| 使用或制作共享库 | [Packages.md](Packages.md) |
| 配置 VSCode 开发体验 | [VSCode.md](VSCode.md) |

## 核心概念

| 概念 | 说明 |
| --- | --- |
| `.dsm` | Dream Shader Material。材质实现文件，用于生成 `UMaterial` 或 `UMaterialFunction`。 |
| `.dsh` | Dream Shader Header。共享头文件，用于存放 `Function` / `Namespace`。 |
| `Shader` | 顶层材质声明，生成 Unreal `UMaterial`。 |
| `ShaderFunction` | 顶层材质函数声明，生成 Unreal `UMaterialFunction`。 |
| `Graph` | `Shader` / `ShaderFunction` 中的图 DSL，负责创建和连接材质节点。 |
| `Function` | 可复用 helper，函数体按 HLSL 风格编写。 |
| `Namespace` | 对 helper 函数分组，调用形式为 `NamespaceName::FunctionName(...)`。 |
| `Path(...)` | 为纹理属性或对象设置声明 Unreal 资产路径。 |
| Package | 可安装的 `.dsh` 共享库，位于 `DShader/Packages`。 |

## 推荐目录结构

```text
Moon_Dev/
├─ DShader/
│  ├─ Materials/
│  │  └─ M_Sample.dsm
│  ├─ Functions/
│  │  └─ F_Tint.dsm
│  ├─ Shared/
│  │  ├─ Common.dsh
│  │  └─ Color.dsh
│  └─ Packages/
│     └─ @typedreammoon/
│        └─ dream-noise/
│           ├─ dreamshader.package.json
│           └─ Library/
│              └─ Noise.dsh
└─ Plugins/
   └─ DreamShader/
      └─ Library/
         └─ Builtin/
            ├─ Common.dsh
            ├─ Texture.dsh
            ├─ Math.dsh
            ├─ Color.dsh
            ├─ UV.dsh
            ├─ Noise.dsh
            ├─ SDF.dsh
            ├─ Normal.dsh
            ├─ PBR.dsh
            └─ PostProcess.dsh
```

## 基本工作流

1. 在 `DShader` 中创建 `.dsm` / `.dsh`。
2. 在 `.dsh` 中编写共享 `Function` 和 `Namespace`。
3. 在 `.dsm` 中通过 `import` 引入共享头文件或 Package。
4. 在 `Shader` / `ShaderFunction` 的 `Graph` 中构建材质节点。
5. 保存源文件，插件自动生成或更新 Unreal 资产。
6. 如果改动 `.dsh`，DreamShader 会通过 import graph 只刷新依赖它的 `.dsm`。

## 语言分层

`Graph` 和 `Function` 的职责不同：

| 层级 | 用途 | 适合内容 |
| --- | --- | --- |
| `Graph` | 创建 Unreal 材质节点 | 变量声明、赋值、构造、`UE.*` 节点、函数调用、基础 `if` / `else`。 |
| `Function` | 生成 helper HLSL | 复用计算、复杂流程、循环、自包含函数。 |
| `Namespace` | 组织 helper | 纹理、颜色、噪声、SDF 等函数集合。 |

迁移规则：

- `Shader` / `ShaderFunction` 的图逻辑写在 `Graph = { ... }`。
- `Function` helper 代码保持 `Function Name(...) { ... }` 写法。
- `Function` 调用使用显式 `out` 参数，例如 `ApplyTint(Color, Tint, Result);`。
- 旧 `Scalar` / `Color` / `Vector` 别名已移除，请使用 `float` / `float2` / `float3` / `float4` 或 `vec2` / `vec3` / `vec4`。

## 当前能力

- `.dsm` / `.dsh` 文件模型。
- `Shader` / `ShaderFunction` 资产生成。
- `Function Name(in ..., out ...) { ... }` helper。
- `Function Inline` / `Function SelfContained` 自包含模式。
- `Namespace(Name="...") { Function ... }` 命名空间。
- `import "Shared/Common.dsh";`
- `import "Builtin/Texture.dsh";`
- `import "@scope/package/Library/File.dsh";`
- HLSL 风格类型与 GLSL 风格别名混用，例如 `float3` 与 `vec3`。
- `Graph` 中的声明、赋值、构造、brace initializer、基础 `if` / `else`、独立函数调用。
- `UE.*` builtin 和泛型 `UE.Expression(...)`。
- `Path(Game|Engine, "...")` 纹理默认值。
- Unreal source map 错误定位，包含 import 后真实 `.dsh` 行列。
- 生成资产 source hash 缓存，源内容未变化时跳过重复生成。
- VSCode 补全、跳转、Hover、Signature Help、Find References、本地诊断和桥接诊断。

## 项目设置

Project Settings > Plugins > DreamShader：

| 设置 | 默认值 | 说明 |
| --- | --- | --- |
| `SourceDirectory` | `DShader` | DreamShader 源目录。 |
| `GeneratedShaderDirectory` | `Intermediate/DreamShader/GeneratedShaders` | helper `.ush` 输出目录。 |
| `AutoCompileOnSave` | `true` | 保存时自动生成资产。 |
| `SaveDebounceSeconds` | `0.25` | 文件保存防抖时间。 |
| `VerboseLogs` | `false` | 输出详细日志。 |

## 边界说明

- `Graph` 不是完整通用语言；支持基础 `if` / `else`，不支持 `for` / `while`。
- 复杂流程建议放进 `Function`。
- `Function` 调用必须显式传入 `out` 目标变量。
- `Path(...)` 当前主要面向 `Game` / `Engine` 根路径。
- VSCode 诊断是开发辅助，不等同于完整编译器语义检查。
