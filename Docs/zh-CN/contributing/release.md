---
title: "发布"
description: "发布"
---

<a id="release"></a>

# 发布

> [DreamShader](../index.md) » [贡献](index.md) » **发布**

发布工作流强制执行的标签约定、工作流执行的操作，以及已发布归档中确切包含的内容。

| | |
| :-- | :-- |
| 声明于 | `.github/workflows/release.yml` — 仓库中唯一的工作流 |
| 类型 | GitHub Actions 工作流 |
| 运行器 | `windows-latest`；每个步骤均为 `pwsh` |
| 权限 | `contents: write` |
| 版本真实来源 | `DreamShader.uplugin` → `VersionName` 和 `IsBetaVersion` |

<a id="synopsis"></a>

## 概要

```powershell
git tag v<major>.<minor>.<patch>[b]
git push origin v<major>.<minor>.<patch>[b]
```

末尾的 `b` 标志着 beta 版本。该工作流也接受手动运行：

```text
workflow_dispatch(version = [<slug>], prerelease = { true | false })
```

`version` 是可选的；若提供，则必须等于描述符所隐含的 slug（比较前会去除开头的 `v`）。`prerelease` 默认为 `false`，且只能添加预发布标志，绝不能移除它。

<a id="tag-conventions"></a>

## 标签约定

| 类型 | 标签模式 | 示例 | Slug | 预发布 |
| :-- | :-- | :-- | :-- | :-- |
| 稳定版 | `v*.*.*` | `v1.5.0` | `1.5.0` | 否，除非 dispatch 输入强制要求 |
| Beta | `v*.*.*b` | `v1.5.0b` | `1.5.0b` | 是 |

标签是**派生而非选定的**：工作流根据描述符计算标签，如果推送的标签不一致则拒绝运行。因此，打标签需要两步变更——先编辑 `DreamShader.uplugin`，提交，然后再打标签。

<a id="version-derivation"></a>

## 版本派生

| 步骤 | 规则 |
| :-- | :-- |
| 1 | 从 `DreamShader.uplugin` 读取 `VersionName`。为空则报错。 |
| 2 | 它必须匹配 `^\s*([0-9]+\.[0-9]+\.[0-9]+)`。带修饰的名称也可以——`"1.5.0 - Beta"` 会得出基础版本 `1.5.0`。 |
| 3 | 当 `IsBetaVersion` 为 `true` **或** `VersionName` 匹配 `(?i)beta` 时，为 beta。 |
| 4 | Slug = 基础版本，若为 beta 则加上 `b` 后缀。 |
| 5 | 标签 = `v` + slug。 |
| 6 | 在标签推送时，推送的标签必须等于派生出的标签。在手动运行时，非空的 `version` 输入必须等于 slug。 |
| 7 | 为后续步骤导出 `DREAMSHADER_VERSION`、`DREAMSHADER_BASE`、`DREAMSHADER_SLUG`、`DREAMSHADER_TAG` 和 `DREAMSHADER_PRERELEASE`。 |

对于当前描述符——`VersionName` `2.1.2`，`IsBetaVersion` `false`，`Version` `212`——这会得出基础版本 `2.1.2`、slug `2.1.2`、标签 `v2.1.2`、预发布 `false`，以及归档 `DreamShader-2.1.2.zip`。

> [!NOTE]
> `Version`（即整数 `180`）不会被工作流读取。它是描述符自身的数字版本，仅对 Unreal 有意义。请随 `VersionName` 一起手动递增。

<a id="workflow-steps"></a>

## 工作流步骤

| # | 步骤 | 作用 |
| :-- | :-- | :-- |
| 1 | 检出 | `actions/checkout@v4`，使用 `fetch-depth: 0` |
| 2 | 解析发布版本 | 上述七条规则；任何不匹配都会抛出错误 |
| 3 | 打包插件 | 暂存随附项目并压缩为 zip（见下文） |
| 4 | 下载最新的 VSCode 扩展发布资源 | 将 `gh release download --repo TypeDreamMoon/dreamshader-language-support --pattern '*'` 下载到 `artifacts/vscode-extension`；若未下载到任何内容则抛出错误 |
| 5 | 构建发布说明 | 提取本版本的 `CHANGELOG.md` 章节并组装正文 |
| 6 | 发布 GitHub Release | 创建发布，若该标签已有发布则更新它 |

<a id="archive-contents"></a>

## 归档内容

暂存目录为 `<RUNNER_TEMP>/DreamShaderRelease/DreamShader/`，它会被压缩为 `artifacts/DreamShader-<slug>.zip`。归档的唯一顶层条目是文件夹 `DreamShader`，安装说明会告诉用户将其放入 `Plugins/`。

共复制十一个项目。缺失的项目会被跳过而非导致发布失败，但如今它会在运行摘要上发出 `::warning::` 注释——正是这种沉默导致其中三项直到 `1.5.0` 都未出现在任何归档中，而无人察觉。

| 随附 | |
| :-- | :-- |
| `Source/` | |
| `Resources/` | |
| `Shaders/` | *（自 1.5.1 起）* `DreamShaderBuiltins.ush`，因此 `/Plugin/DreamShader/…` 在归档安装中可被解析；`Pass/` 下的 Custom Pass 着色器，若无它们，`DreamShaderPass` 模块的全局和网格通道着色器将无法编译 |
| `Content/` | *（自 2.1.2 起）* `Localization/DreamShader/`——`.locmeta` 和 `zh-Hans` `.locres`——因此在归档安装中，描述符的 `LocalizationTargets` 条目有可加载的翻译 |
| `Docs/` | |
| `.skill/` | *（自 1.5.1 起）* 智能体技能集及其驱动程序 |
| `DreamShader.uplugin` | |
| `README.md` | |
| `README.zh-CN.md` | *（自 1.5.1 起）*——`README.md` 链接到它，因此归档中的链接以前是失效的 |
| `CHANGELOG.md` | |
| `LICENSE` | |

仓库中其他所有内容均不包含：

| 不随附 | 对归档安装的影响 |
| :-- | :-- |
| `Tests/` | 夹具语料库缺失；两个数据驱动的运行器将枚举零个子测试 |
| `Tools/` | 本地化和一致性脚本缺失：构建后的 `.locres` 会随附，而重建它的链条不会 |
| `Config/` | `FilterPlugin.ini` 缺失。它仅包含原样的注释模板，且未声明任何打包文件 |
| `Images/` | README 图示缺失，因此 readme 中的图像在本地无法渲染 |
| `.github/` | 工作流本身不会被重新分发 |
| `Binaries/`、`Intermediate/` | 归档为**仅源代码**；使用方首次启动编辑器时会编译这四个模块 |

> [!NOTE]
> 直到并包括 `1.5.0`，归档都未随附 `Shaders/` 文件夹，因此任何解析 `/Plugin/DreamShader/...` 的内容在由发布 zip 安装的环境中都没有文件可读。由这些归档进行的安装仍需要手动复制入 `Shaders/DreamShaderBuiltins.ush`。参见
> [HLSL 库](../builtins/hlsl-library.md)。

> [!NOTE]
> 从 `1.6.0`（首个具有 `zh-Hans` 翻译的版本）直到并包括 `2.1.1`，归档都未随附 `Content/` 文件夹。它从未出现在列表中，因此也没有任何警告标记它。在由这些 zip 之一安装的环境中，描述符的 `LocalizationTargets` 条目找不到可加载的内容，DreamShader 的编辑器文本即使在 `zh-Hans` 编辑器中仍保持英文。从匹配标签的仓库中手动复制入 `Content/Localization/` 即可修复。自 `2.1.2` 起，工作流会像 `BuildPlugin` 一样完整复制 `Content/`，因此之后在其下添加的任何内容也会随附。参见 [本地化](index.md#localization)。
>
> 与上面的列表不同，这一点不依赖警告。压缩后，该步骤会打开 zip，并对描述符中每一个 `LocalizationTargets` 条目，要求存在 `Content/Localization/<Name>/<Name>.locmeta` 以及仓库为其提供的每一个 `<culture>/<Name>.locres`；缺失任何一个都会导致发布失败。

> [!NOTE]
> 归档不随附 `Binaries/`，因此插件由使用方项目编译。这需要 C++ 项目（或已转换为 C++ 的项目）以及匹配的引擎工具链——关于 UE 5.3 和 5.4 的 MSVC 注意事项参见 [贡献](index.md#engine-versions)。

<a id="release-notes"></a>

## 发布说明

正文按以下顺序组装。

| 部分 | 内容 |
| :-- | :-- |
| 预发布提示 | 仅在 beta 版中添加：警告 API 和生成的资源布局在稳定基础版本之前仍可能变化，并附有指向 issue 跟踪器的链接 |
| 安装 | 三个步骤：下载 `DreamShader-<slug>.zip`，将 `DreamShader` 文件夹解压到 `Plugins/`，在 *Edit ▸ Plugins* 中启用 **DreamShader** 并重启 |
| 兼容性行 | 字面字符串 `Compatible with Unreal Engine 5.3 - 5.8 (Win64).` |
| 变更内容 | `CHANGELOG.md` 中的 `## <VersionName>` 部分，逐字引用 |
| 资源 | 一个两行表格：插件 zip，以及 VSCode 扩展的 `*.vsix` |
| 链接 | 文档 `https://shader.toolchain.64hz.cn/`、发布标签处的变更日志，以及 VSCode 扩展仓库 |

变更日志提取详情：

- 该部分通过匹配 `^##\s+<VersionName>\b` 来查找，因此变更日志标题必须*以* `VersionName` *开头*，
  且与所写的完全一致，包括装饰内容。尾随文本没问题，这正是
  `## 1.9.1 - 2026-09-08` 能匹配当前 `VersionName` `1.9.1` 的原因。失败则相反：
  像 `1.5.0 - Beta` 这样带装饰的名称**不会**被裸 `## 1.5.0` 标题匹配。
- 它在下一个 `##` 标题处结束。
- 开头的块引用行会被去除，因此变更日志自身的 beta 说明不会重复
  工作流的预发布提示。
- 当不存在匹配部分时，正文会回退为发布标签处指向 `CHANGELOG.md` 的单个链接。
  **这是静默的**——标题拼写错误会产生没有说明的发布，而不是失败。

<a id="publication"></a>

## 发布

| 情况 | 操作 |
| :-- | :-- |
| 该标签不存在发布 | 使用 zip 和每个已下载的 VSCode 资源执行 `gh release create` |
| 该运行是手动分发，而非标签推送 | 添加 `--target <commit sha>`，以便在分发的提交上创建标签 |
| 该标签已存在发布 | 对资源执行 `gh release upload --clobber`，然后对标题和说明执行 `gh release edit` |
| Beta，或分发输入 `prerelease` 为 `true` | 创建时执行 `--prerelease`，或更新时再次执行 `gh release edit --prerelease` |

发布标题为 `DreamShader <VersionName>`——名称逐字引用，包括装饰内容，例如
`DreamShader 1.9.1`。资源为插件 zip，后跟从 VSCode 扩展最新发布下载的每个文件，
按名称排序。

<a id="diagnostics"></a>

## 诊断

工作流可能抛出的每条消息。运行时替换显示为 `{Placeholder}`。

| 消息 | 步骤 | 原因 |
| :-- | :-- | :-- |
| `DreamShader.uplugin VersionName is empty.` | 解析发布版本 | `VersionName` 缺失或为空白 |
| `DreamShader.uplugin VersionName '{VersionName}' must start with an X.Y.Z version.` | 解析发布版本 | `VersionName` 不以三个点分隔的整数开头 |
| `Pushed tag '{PushedTag}' does not match the tag derived from DreamShader.uplugin ('{DerivedTag}'). Fix VersionName / IsBetaVersion, or retag.` | 解析发布版本 | 标签与描述符不一致——通常是忘记执行 `b`，或在提交前就已打标签的版本提升 |
| `Requested version '{Input}' does not match DreamShader.uplugin ('{Slug}').` | 解析发布版本 | 手动运行传入了不是派生 slug 的 `version` 输入 |
| `No assets were downloaded from TypeDreamMoon/dreamshader-language-support latest release.` | 下载 VSCode 资源 | 扩展仓库的最新发布没有资源，或下载失败 |

每个步骤都会设置 `$ErrorActionPreference = 'Stop'`，因此任何其他 PowerShell 失败也会中止任务。

<a id="notes"></a>

## 说明

- 工作流从不构建插件。它暂存文件并发布它们；编译发生在
  使用者的机器上。在打标签前自行运行 `RunUAT BuildPlugin`——参见
  [Contributing](index.md#building)。
- 工作流也从不会运行自动化测试套件。参见 [Testing](testing.md)。
- 对现有标签重新运行工作流是安全的：资源会被覆盖，说明会从当前 `CHANGELOG.md` 重写。
- **删除已有发布的标签会将该发布变为草稿。** 重新推送标签会重新运行工作流，
  但工作流会*编辑*现有发布而不是创建新发布，因此它会保持草稿状态——对用户不可见，
  而上一版本会保留 *Latest* 徽章。最后用 `gh release edit v<slug> --draft=false --latest` 收尾。在重新发布
  `v1.5.1` 时观察到。
- VSCode 扩展从其自己的仓库发布。工作流始终拉取该仓库的**最新**发布，
  因此如果在扩展发布过程中发布插件版本，会附带先前的 `.vsix`。
- 描述符中的 `DocsURL` 指向 <https://shader.toolchain.64hz.cn/>，后者由 `Docs/` 构建。
  `Docs/` 树会随归档一起发布，因此手册也可在插件旁边离线使用。

<a id="example"></a>

## 示例

发布 `1.5.1` 稳定版：

```powershell
# 1. Bump the descriptor: VersionName = "1.5.1", IsBetaVersion = false, Version = 151.
# 2. Add a matching "## 1.5.1 - 2026-08-02" section to CHANGELOG.md, and the version's section
#    to Docs/changelog/dreamshader.md (the manual's release notes, written by hand).
git add DreamShader.uplugin CHANGELOG.md Docs/changelog/dreamshader.md
git commit -m "Release 1.5.1"
git push

# 3. Tag exactly what the descriptor implies: base 1.5.1, no beta 'b'.
git tag v1.5.1
git push origin v1.5.1
```

发布结果：

```text
release      DreamShader 1.5.1
tag          v1.5.1
assets       DreamShader-1.5.1.zip
             <every asset from the latest dreamshader-language-support release, e.g. *.vsix>
zip layout   DreamShader/Source/  DreamShader/Resources/  DreamShader/Shaders/
             DreamShader/Docs/  DreamShader/.skill/
             DreamShader/DreamShader.uplugin
             DreamShader/README.md  DreamShader/README.zh-CN.md
             DreamShader/CHANGELOG.md  DreamShader/LICENSE
```

<a id="see-also"></a>

## 另请参见

- [Contributing](index.md) — 构建插件、源码树和引擎版本矩阵
- [Testing](testing.md) — 打标签前要运行的测试套件，以及为什么语料库不包含在归档中
- [HLSL library](../builtins/hlsl-library.md) — `DreamShaderBuiltins.ush`，自 `1.5.1` 起随归档发布
- [Workspace and editor extensions](../../tools/workspace.md) — 随插件一起发布的 VSCode 扩展
- [DreamShader module](../api/dreamshader-module.md) — 启动时注册的 `/Plugin/DreamShader` 着色器映射
- [DreamShader reference](../index.md) — `Docs/` 文件夹所变成的手册
</content>
