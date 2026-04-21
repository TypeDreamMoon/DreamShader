# DreamShader Package

DreamShader Package 是 DreamShaderLang 的共享库分发格式。它的目标类似轻量 npm：把可复用 `.dsh` 函数库、示例 `.dsm`、README 和 License 放在一个 GitHub 仓库里，用户可以通过 VSCode 扩展安装到项目的 `DShader/Packages`。

## 1. Package 目录结构

推荐结构：

```text
dream-noise/
├─ dreamshader.package.json
├─ README.md
├─ LICENSE
├─ Library/
│  ├─ Noise.dsh
│  ├─ FBM.dsh
│  └─ Voronoi.dsh
└─ Examples/
   └─ M_NoisePreview.dsm
```

## 2. `dreamshader.package.json`

每个包根目录必须包含 `dreamshader.package.json`：

```json
{
  "name": "@typedreammoon/dream-noise",
  "version": "1.0.0",
  "displayName": "Dream Noise",
  "description": "Reusable noise functions for DreamShaderLang.",
  "author": "TypeDreamMoon",
  "repository": "https://github.com/TypeDreamMoon/dream-noise",
  "license": "MIT",
  "dreamshader": {
    "language": "DreamShaderLang",
    "version": ">=1.0.0",
    "entry": "Library/Noise.dsh"
  },
  "keywords": ["noise", "fbm", "voronoi"]
}
```

规则：

- `name` 必填，支持 `name` 或 `@scope/name`
- `version` 推荐使用 SemVer
- `repository` 用于更新 package
- `dreamshader.entry` 是推荐入口头文件，供文档和商店展示使用
- 仓库建议添加 GitHub topic：`dreamshader-package`

## 3. 安装位置

VSCode 扩展会把包安装到：

```text
DShader/Packages/<package name>/
```

例如：

```text
DShader/Packages/@typedreammoon/dream-noise/
```

安装时会生成或更新锁文件：

```text
DShader/dreamshader.lock.json
```

锁文件记录 package 名称、版本、仓库、commit 和安装路径，方便团队协作时确认依赖来源。

`Examples/**/*.dsm` 可以放示例材质，但安装 package 后不会被自动全量编译。需要使用示例时，建议复制到项目自己的 `DShader/Materials` 或其他非 `DShader/Packages` 目录。

## 4. Package Import

安装后可以直接用 package 名称导入：

```c
import "@typedreammoon/dream-noise/Library/Noise.dsh";
```

也可以省略 `.dsh`：

```c
import "@typedreammoon/dream-noise/Library/Noise";
```

解析顺序：

- 当前文件相对路径
- 项目 `DShader/`
- 项目 `DShader/Packages/`
- 插件内置 `Plugins/DreamShader/Library/`

## 5. VSCode 命令

命令面板中可用：

- `DreamShaderLang: Install Package from GitHub`
- `DreamShaderLang: Browse Package Store`
- `DreamShaderLang: Update Installed Packages`
- `DreamShaderLang: Remove Installed Package`
- `DreamShaderLang: Open Packages Folder`
- `DreamShaderLang: Add Package Store Index Source`
- `DreamShaderLang: Remove Package Store Index Source`
- `DreamShaderLang: Create Package Step by Step`

安装支持两种输入：

```text
TypeDreamMoon/dream-noise
https://github.com/TypeDreamMoon/dream-noise
```

安装、更新需要本机可用 `git` 命令。

`Create Package Step by Step` 会按步骤询问 package 名称、显示名、描述、namespace、作者、仓库地址、目标目录和是否生成示例材质，然后创建标准 package 骨架。

## 6. Package Store

当前商店是轻量实现，由两部分组成：

- 配置项 `dreamshader.packageStoreIndexUrls` 指向一个或多个 JSON index
- GitHub topic 搜索 `dreamshader-package`
- VSCode 命令 `DreamShaderLang: Browse Package Store` 会打开一个 Webview 商店面板，可搜索、安装、打开仓库、添加或移除 index 源

index 可以是数组：

```json
[
  {
    "name": "@typedreammoon/dream-noise",
    "displayName": "Dream Noise",
    "description": "Noise, FBM and Voronoi helpers.",
    "repository": "https://github.com/TypeDreamMoon/dream-noise",
    "path": "../dreamshader-packages/dream-noise",
    "tags": ["noise", "procedural"]
  }
]
```

也可以是：

```json
{
  "packages": []
}
```

默认 index 地址：

```text
https://raw.githubusercontent.com/TypeDreamMoon/dreamshader-package-index/main/packages.json
```

VSCode 设置示例：

```json
"dreamshader.packageStoreIndexUrls": [
  "https://raw.githubusercontent.com/TypeDreamMoon/dreamshader-package-index/main/packages.json",
  "I:/UnrealProject_Moon/VSCodeExt/dreamshader-package-index/packages.json"
]
```

`dreamshader.packageStoreIndexUrl` 旧单源配置仍兼容，但推荐使用 `dreamshader.packageStoreIndexUrls` 列表。

`path` 是可选字段，主要给本地开发 index 使用。VSCode 从本地 `packages.json` 加载 index 时，会把相对 `path` 按 index 文件所在目录解析；如果本地路径不存在，会回退使用 `repository`。

## 7. Package 作者建议

- 所有公共函数建议放在 `Library/**/*.dsh`
- 使用 `Namespace(Name="...")` 避免函数名冲突
- 示例材质放在 `Examples/**/*.dsm`
- README 写清楚 import 路径和示例调用
- 给 GitHub 仓库添加 `dreamshader-package` topic，方便商店发现

## 8. 默认 Package 草案

本地默认 package 草案位于：

```text
I:/UnrealProject_Moon/VSCodeExt/dreamshader-packages
```

当前包含：

- `@typedreammoon/dreamshader-math`
- `@typedreammoon/dreamshader-color`
- `@typedreammoon/dreamshader-uv`
- `@typedreammoon/dreamshader-noise`
- `@typedreammoon/dreamshader-sdf`
- `@typedreammoon/dreamshader-normal`
- `@typedreammoon/dreamshader-pbr`
- `@typedreammoon/dreamshader-postprocess`

这些条目已经写入本地 index：

```text
I:/UnrealProject_Moon/VSCodeExt/dreamshader-package-index/packages.json
```
