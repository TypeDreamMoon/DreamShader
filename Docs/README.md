# DreamShader 文档

这份文档面向 DreamShader `1.1.0`，覆盖 `DreamShaderLang` 的文件结构、语法模型、VSCode 工作流，以及从旧语法迁移到新语法的方式。

## 发布信息

- 插件版本：`1.1.0`
- 语言名称：`DreamShaderLang`
- 源文件：`.dsm` / `.dsh`
- 开发者：TypeDreamMoon
- GitHub：<https://github.com/TypeDreamMoon>
- Web：<https://dev.64hz.cn>
- Copyright：Copyright (c) 2026 TypeDreamMoon. All rights reserved.

## 文档索引

- [语法参考](LanguageReference.md)
  - `.dsm` / `.dsh`
  - `Shader`
  - `Function`
  - `Namespace`
  - `ShaderFunction`
  - `import`
  - 插件内置库
  - `Path(...)`
  - `Graph` 图 DSL
- [示例与模式](Examples.md)
  - 完整材质示例
  - 共用头文件示例
  - `ShaderFunction` 示例
  - 纹理默认值示例
- [VSCode 支持](VSCode.md)
  - 补全
  - 跳转
  - 格式化
  - 本地诊断
  - Unreal 桥接诊断
- [Package 系统](Packages.md)
  - `dreamshader.package.json`
  - GitHub 安装
  - package import
  - package store

## 当前推荐模型

DreamShaderLang 推荐把职责拆成两类文件：

- `.dsm`
  - 负责真正生成 `UMaterial` 或 `UMaterialFunction`
  - 包含 `Shader(...)` 或 `ShaderFunction(...)`
- `.dsh`
  - 只放共用 `Function` / `Namespace` 和 `import`
  - 类似 C/C++ 里的 header
- `Plugins/DreamShader/Library/**/*.dsh`
  - 插件内置头文件
  - 可直接通过 `import "Builtin/Texture.dsh";` 使用
- `DShader/Packages/**/*.dsh`
  - 项目安装的 DreamShader Package
  - 可直接通过 `import "@scope/package/Library/File.dsh";` 使用

推荐目录结构：

```text
Moon_Dev/
├─ DShader/
│  ├─ Sample.dsm
│  ├─ Shared/
│     ├─ Common.dsh
│     └─ Noise.dsh
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

## 核心设计原则

- 材质主流程写在 `Shader` / `ShaderFunction` 的 `Graph` 里
- 可复用逻辑写成顶层 `Function`
- 一组相关 helper 用 `Namespace(Name="...")` 组织
- 共享函数通过 `.dsh + import` 组织
- 通用能力可以直接复用插件内置 `Builtin/*.dsh`
- 第三方共享库可以通过 DreamShader Package 安装到 `DShader/Packages`
- `Graph` 负责图构建，支持基础 `if` / `else`，复杂流程仍建议写进 `Function`
- 函数 helper 体才适合写更自由的原始代码逻辑

## 一句话区分

- `Graph = { ... }`：DreamShader 图 DSL
- `Function Foo(...) { ... }`：共用 helper 代码
- `NamespaceName::FunctionName(...)`：命名空间 helper 调用
- `Scalar` / `Color` / `Vector`：旧别名，已移除

## 示例文件

当前仓库内可直接参考：

- `DShader/Sample.dsm`
- `DShader/Shared/common.dsh`
- `Plugins/DreamShader/Library/Builtin/Texture.dsh`

## 当前版本重点能力

- `.dsm` / `.dsh` 文件模型
- `Function Name(in ..., out ...) { ... }`
- `Namespace(Name="Texture") { Function Sample2DRGB(...) { ... } }`
- `import "Shared/Common.dsh";`
- `import "Builtin/Texture.dsh";`
- `import "@typedreammoon/dream-noise/Library/Noise.dsh";`
- HLSL 风格基础类型与 GLSL 风格别名混用，例如 `float3` 与 `vec3`
- `Graph` 中的声明、赋值、构造、基础 `if` / `else`、独立函数调用
- `float4 c = {rgb, alpha};` 这类 brace initializer
- 纹理默认值 `Path(Game|Engine, "...")`
- VSCode 作用域补全和本地语法检查
- VSCode Signature Help / Hover / Find References
- Unreal 侧 source map 错误定位，包含 import 后真实 `.dsh` 行列
- `.dsh` import graph 依赖追踪，只重编受影响的 `.dsm`
- 生成资产 source hash 缓存，未变化时跳过重复生成
- Project Settings > Plugins > DreamShader 配置源目录、内置库目录、生成目录、自动编译、防抖和详细日志

## 仍然需要注意的边界

- `Shader` / `ShaderFunction` 的 `Graph` 不是通用编程语言；支持基础 `if` / `else`，但不支持 `for` / `while`
- `Function` 调用必须显式传 `out` 目标变量
- `Path(...)` 当前主要面向 `Game` 和 `Engine` 根路径
- VSCode 诊断已经比较强，但还不是 clangd 那种完整编译器级语义分析
