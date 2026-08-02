<p align="center">
  <img alt="DreamShader banner" src="./Images/banner.png" />
</p>

<table>
  <tr>
    <td width="64%" valign="top">
      <h1>DreamShader</h1>
      <p><strong>用 DreamShaderLang 以文本优先的方式编写 Unreal Engine 材质。</strong></p>
      <p>
        DreamShader 把 <code>.dsm</code>、<code>.dsf</code>、<code>.dsh</code> 源文件编译成标准的 Unreal
        <code>UMaterial</code>、<code>UMaterialFunction</code>、Material Layer 和 Material Layer Blend 资产。
        源文件是唯一的编辑面，资产是构建产物，随时可以丢掉重建。
      </p>
      <p>
        <img alt="Unreal Engine 5.3-5.8" src="https://img.shields.io/badge/Unreal%20Engine-5.3--5.8-313131" />
        <img alt="Version 1.5.1" src="https://img.shields.io/badge/version-1.5.1-blue" />
        <img alt="License MIT" src="https://img.shields.io/badge/license-MIT-green" />
      </p>
      <p>
        <a href="README.md">English</a> &nbsp;·&nbsp;
        <a href="Docs/index.md">文档</a> &nbsp;·&nbsp;
        <a href="Docs/getting-started.md">快速上手</a> &nbsp;·&nbsp;
        <a href="Docs/language/index.md">语法参考</a> &nbsp;·&nbsp;
        <a href="Docs/examples/index.md">示例</a> &nbsp;·&nbsp;
        <a href=".skill/README.md">AI 技能</a> &nbsp;·&nbsp;
        <a href="CHANGELOG.md">Changelog</a>
      </p>
      <p>
        <a href="https://github.com/TypeDreamMoon/DreamShader/issues">
          <img alt="Issues" src="https://img.shields.io/github/issues/TypeDreamMoon/DreamShader" />
        </a>
        <a href=".skill/README.md">
          <img alt="Agent skills" src="https://img.shields.io/badge/Agent%20skills-5-8A2BE2" />
        </a>
        <a href="https://github.com/TypeDreamMoon/dreamshader-language-support/releases">
          <img alt="VSCode Extension" src="https://img.shields.io/badge/VSCode-DreamShaderLang-007ACC" />
        </a>
        <a href="https://github.com/tsdaer/dreamshader-language-support">
          <img alt="Rider Plugin" src="https://img.shields.io/badge/Rider-DreamShaderLang-7F52FF" />
        </a>
      </p>
      <p><strong>QQ 群：</strong><a href="https://qm.qq.com/q/X9uCLjVcY">466585194</a></p>
    </td>
    <td width="36%" align="center" valign="middle">
      <img src="./Images/character.png" width="260" alt="DreamShader character" />
    </td>
  </tr>
</table>

> [!TIP]
> 把所有 `.dsm`、`.dsf`、`.dsh` 文件纳入版本管理。生成的 Unreal 资产随时可以从源文件重建，不需要提交。

---

## 长什么样

```c
Shader(Name="DreamMaterials/M_Minimal")
{
    Properties = {
        vec3 Tint = vec3(1.0, 0.2, 0.2);
    }

    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = Tint;
    }
}
```

保存文件，DreamShader 就会生成 `/Game/DreamMaterials/M_Minimal`。`Name` 是相对 `Root` 的资产路径，
`Root` 默认为 `Game`；写 `Root="Plugin.MyPlugin"` 则生成到内容插件里。

默认情况下材质是**在内存中生成**的——不写 `.uasset`，Content Browser 里也看不到，因为要编辑的是源文件。
Cook 时会自动落盘，也可以在 Material Content Browser 里手动把某个材质实体化。

<p align="center">
  <img alt="DreamShader workflow overview" src="./Images/workflow-overview.png" />
</p>

## 快速开始

1. 把插件复制到项目里，在 *Edit ▸ Plugins* 中启用 **DreamShader** 并重启编辑器。依赖的引擎插件
   `WebSocketNetworking` 和 `SQLiteCore` 会自动启用。
2. 在项目根目录创建源文件目录，新建一个 `.dsm`：

   ```text
   MyProject/
   ├─ DShader/
   │  ├─ Materials/   *.dsm   材质实现
   │  ├─ Functions/   *.dsf   可复用的材质函数资产
   │  ├─ Shared/      *.dsh   头文件：Function、GraphFunction、Namespace、VirtualFunction
   │  └─ Packages/           已安装的共享库
   └─ Plugins/
      └─ DreamShader/
   ```

3. 保存。*Auto Compile On Save* 默认开启，源文件会在短暂 debounce 后被解析并生成资产。

配置位于 *Project Settings ▸ DreamPlugin ▸ Dream Shader*，每一项及其默认值见
[项目设置](Docs/settings/project.md)。完整上手流程见[快速上手](Docs/getting-started.md)。

## 能生成什么

<p align="center">
  <img alt="DreamShader language model" src="./Images/language-model.png" />
</p>

| 块 | 产物 | 参考 |
| :-- | :-- | :-- |
| `Shader` | 一个 `UMaterial` | [Shader](Docs/language/shader.md) |
| `ShaderFunction` | 一个 `UMaterialFunction` | [ShaderFunction](Docs/language/shader-function.md) |
| `ShaderLayer` | 原生 `UMaterialFunctionMaterialLayer` | [ShaderLayer](Docs/language/shader-layer.md) |
| `ShaderLayerBlend` | 原生 `UMaterialFunctionMaterialLayerBlend` | [ShaderLayer](Docs/language/shader-layer.md) |
| `VirtualFunction` | 不生成资产——声明一个**已存在**的资产，让 `Graph` 能调用它 | [VirtualFunction](Docs/language/virtual-function.md) |
| `Function` | 一个 HLSL `Custom` 节点，经由生成的 `.ush` helper | [Function](Docs/language/function.md) |
| `GraphFunction` | 一个 `Custom` 节点，且可以把 `UE.*` 节点拉进它的输入 | [GraphFunction](Docs/language/graph-function.md) |
| `Namespace` | 不生成资产——把 helper 归组为 `Ns::Name` | [Namespace](Docs/language/namespace.md) |

三种源文件不能互换：`.dsm` 最多含一个 `Shader`，`.dsf` 只生成函数资产、不能声明 `Shader`，`.dsh` 是通过
`import` 消费的头文件。详见[源文件](Docs/language/source-files.md)。

`Graph = { … }` 是写节点图的地方——声明、算术、swizzle、`UE.*` 材质节点、数学 builtin、函数调用以及
`if` / `else`。typed `Properties` 覆盖 scalar、vector、texture、switch、MPC 以及反射节点属性。
`MaterialAttributes` 和 `Substrate`（UE 5.4+）都是一等值，可以在 graph 代码、函数签名和输出绑定之间传递。

## 文档

完整参考在 [`Docs/`](Docs/index.md)，并发布在 **<https://lang.64hz.cn/docs>**（中英双语）。

| | |
| :-- | :-- |
| **[语法参考](Docs/language/index.md)** | 源文件、词法、顶层块、Section、类型、`import` |
| **[Graph 语言](Docs/graph/index.md)** | 语句、表达式、类型转换、swizzle、`if` / `else`、调用，以及 [`Graph` 不支持什么](Docs/graph/unsupported.md) |
| **[Builtins](Docs/builtins/index.md)** | `UE.*` 目录、[数学 builtin](Docs/builtins/math.md)、[`Substrate.*`](Docs/builtins/substrate.md)、逃生舱 [`UE.Expression`](Docs/builtins/ue-expression.md) |
| **[参数](Docs/parameters/index.md)** | 21 个参数节点 token、紧凑类型、[metadata 键](Docs/parameters/metadata.md)、[`SamplerType`](Docs/parameters/sampler-type.md) |
| **[设置](Docs/settings/index.md)** | [材质设置](Docs/settings/material.md)及其[枚举值](Docs/settings/material-enums.md)、函数设置、[项目设置](Docs/settings/project.md) |
| **[生成](Docs/generation/index.md)** | [资产路径](Docs/generation/asset-paths.md)、[内存材质](Docs/generation/in-memory.md)、[缓存](Docs/generation/caching.md)、[节点布局](Docs/generation/graph-layout.md) |
| **[编辑器工具](Docs/tools/index.md)** | 浏览器、预览、反编译器、workspace、Package、Bridge、[commandlet](Docs/tools/commandlet.md) |
| **[诊断信息](Docs/diagnostics/index.md)** | 编译器可能发出的每一条消息，按阶段归类 |
| **[示例](Docs/examples/index.md)** | 可以直接复制的完整源文件 |
| **[C++ API](Docs/api/index.md)** | 公开头文件，用于扩展插件 |

## 编辑器与工具链

<p align="center">
  <img alt="DreamShader editor tools" src="./Images/editor-tools.png" />
</p>

| | |
| :-- | :-- |
| **[Material Content Browser](Docs/tools/material-browser.md)** | *Tools ▸ DreamShader*。**Project** 页浏览 `/Game` 下所有材质及其完整继承链，一键创建实例；**Dream Shader Gen** 页列出源文件、实时预览、全量编译并暴露错误 |
| **[反编译器](Docs/tools/decompiler.md)** | 右键 `Material` / `Material Function` ▸ *DreamShader ▸ Export DSM/DSF*。定位是迁移起点——常见节点转成 graph 文本，其余回退到 `UE.Expression(…)`，保证结构仍可重新生成 |
| **[Package](Docs/tools/packages.md)** | 安装在 `DShader/Packages/@scope/name/` 下的可复用 `.dsh` 库，用 `import "@typedreammoon/dream-noise/Library/Noise.dsh";` 引入 |
| **[Workspace](Docs/tools/workspace.md)** | 生成的 `DShader/DreamShader.code-workspace`，可从编辑器工具栏用 VSCode 打开 |
| **[Commandlet](Docs/tools/commandlet.md)** | `-run=DreamShader compile \| decompile`——供 CI 使用的无头生成 |

### 编辑器语言扩展

| 编辑器 | 仓库 | 功能 |
| :-- | :-- | :-- |
| VSCode | [TypeDreamMoon/dreamshader-language-support](https://github.com/TypeDreamMoon/dreamshader-language-support/releases) | 高亮、代码片段、补全、跳转定义、查找引用、Hover、Signature Help、本地与 Bridge 诊断、材质预览、Package 命令、模板 |
| Rider | [tsdaer/dreamshader-language-support](https://github.com/tsdaer/dreamshader-language-support) | `.dsm` / `.dsf` / `.dsh` 文件类型、语法与 PSI 解析、高亮、补全、导航、诊断、Bridge 集成、语义 token、inlay hint、Package 工具 |

## AI 支持

DreamShaderLang 是文本格式，编码 agent 本来就能写——前提是它能**验证自己写的东西**。
[`.skill/`](.skill/README.md) 提供的就是闭合这个环的工具：一个无头驱动脚本，加五个
[Claude Code](https://claude.com/claude-code) 技能格式的 skill。

| 技能 | 参数 | 作用 |
| :-- | :-- | :-- |
| [`dream-shader-create`](.skill/dream-shader-create/SKILL.md) | `<描述>` | 从自然语言描述写出材质或函数，并编译验证它确实能生成 |
| [`dream-shader-optimize`](.skill/dream-shader-optimize/SKILL.md) | `<文件>` | 对反编译产物去重、重命名、改回原资产路径，并补上丢失的状态 |
| [`dream-shader-decompile`](.skill/dream-shader-decompile/SKILL.md) | `<资产>` | 把已有的 `UMaterial` / `UMaterialFunction` 导出成源文件 |
| [`dream-shader-verify`](.skill/dream-shader-verify/SKILL.md) | `<文件>` \| `-All` | 无头编译，exit `0` / `1` |
| [`dream-shader-diagnose`](.skill/dream-shader-diagnose/SKILL.md) | `<诊断信息>` | 把诊断路由到对应的编译阶段，解释成因并修掉 |

`dsc.ps1` 包装了 [commandlet](Docs/tools/commandlet.md)：从 `.uproject` 的 `EngineAssociation`
解析引擎、向上走找到项目、只打印 `LogDreamShader` 那几行；并且——因为无头编译会真的写 `.uasset`，
而编辑器里是内存生成——它会把本次写盘的资产逐个对照 git 分类，避免临时探针资产变成未跟踪的垃圾留在仓库里。

```powershell
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/Materials/M_Panel.dsm -Force -CleanNew
```

把技能发布到 `.claude/skills/`，之后 agent 在项目任意位置都能按名字加载：

```powershell
pwsh -File Plugins/DreamShader/.skill/sync-skills.ps1
```

> [!NOTE]
> 只有「自动加载」这一步是 Claude Code 专有的。驱动就是一个普通的 PowerShell 脚本，每个 `SKILL.md`
> 也只是普通 Markdown——任何 agent，或者人，都可以照着读、照着跑。
> [`.skill/reference/dreamshaderlang.md`](.skill/reference/dreamshaderlang.md) 把写材质真正要用的语法
> 浓缩成一页，包括那些只在编译期才暴露的坑：19 个会静默遮蔽用户代码的保留数学 builtin、`Function`
> 函数体内按整词生效的 GLSL 标识符改写，以及根本不存在的矩阵类型。

## 版本兼容

Unreal Engine `5.3` – `5.8`，Win64，每个版本都通过单插件 `RunUAT BuildPlugin` 构建验证过。
当前开发主线针对 `5.8`。

<details>
<summary>不编译完整项目目标，只验证插件</summary>

```powershell
& "<EngineDir>\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="<ProjectDir>\Plugins\DreamShader\DreamShader.uplugin" `
  -Package="<OutputDir>\DreamShader" `
  -TargetPlatforms=Win64 `
  -Rocket
```

Windows 上 UE `5.3` 和 `5.4` 可能需要 MSVC `14.38` 工具链——更新的编译器在编译旧引擎头文件时就可能失败，
根本走不到插件代码。

</details>

> [!NOTE]
> 反编译器是迁移辅助工具，不是双向往返的保证。它能处理很多常见材质和部分 Lyra 大型材质案例，并会对
> 复现不了的部分留下 `// Warning:` 注释。删掉原始资产之前，建议先读一遍
> [已知的往返缺口](Docs/tools/decompiler.md#known-round-trip-gaps)。

## 项目信息

| | |
| :-- | :-- |
| 版本 | `1.5.1` |
| 语言 | `DreamShaderLang` |
| Unreal Engine | `5.3` – `5.8` |
| 模块 | `DreamShader`、`DreamShaderCompiler`（Runtime），`DreamShaderEditor`（Editor） |
| 作者 | TypeDreamMoon |
| GitHub | <https://github.com/TypeDreamMoon> |
| 文档 | <https://lang.64hz.cn/> |
| 主页 | <https://dev.64hz.cn> |
| 许可证 | [MIT](LICENSE) |
| 版权 | Copyright (c) 2026 TypeDreamMoon. All rights reserved. |

发版流程见 [Release](Docs/contributing/release.md)，从源码构建插件见 [Contributing](Docs/contributing/index.md)。

## 路线图

- 支持自定义全屏渲染 Pass。
- 更完整的 VSCode 语义诊断。
- 更深入的 Material Layer Stack 与 Layer Instance 工作流支持。
- 更深入的 Moon Engine 集成——参考：<https://zhuanlan.zhihu.com/p/21979494450>

## 许可证

DreamShader 以 [MIT 许可证](LICENSE)发布。Bug 报告和功能建议请提
[issue](https://github.com/TypeDreamMoon/DreamShader/issues/new)。
