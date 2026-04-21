# DreamShaderLang VSCode 支持

DreamShaderLang 的 VSCode 扩展位于：

- `I:/UnrealProject_Moon/VSCodeExt/dreamshader-language-support`

它服务于 `.dsm` / `.dsh` 文件开发，目标是让 DreamShaderLang 工作流尽量接近常见代码语言服务体验。

## 1. 已支持的能力

### 语法高亮

- `Shader`
- `Function`
- `Namespace`
- `import`
- `ShaderFunction`
- HLSL 风格基础类型与 GLSL 风格别名
- 常见流程关键字

### 补全

- 顶层 block 关键字
- section 名称
- `Function` / `Namespace::Function` 名称
- `UE.*` builtin
- 类型名
- 设置项
- 材质输出名
- `Path(...)` helper
- `Builtin/*.dsh` import 路径
- `DShader/Packages` package import 路径

### 作用域感知补全

当前扩展已经做了基础作用域隔离：

- 在 `Function` 体里，只补当前函数参数和局部变量
- 在 `Shader` 的 `Code` 里，只补当前 block 可见的 `Properties` / `Outputs` / 局部变量
- 不会再把 `Shader` 的 `Properties` 乱补到无关的 `Function` 体里

### 跳转

- `import` 跳转到 `.dsh`
- `Function` / `Namespace::Function` 跳转到定义
- 项目头文件和插件内置头文件都会参与解析

### Signature Help / Hover / References

- 输入 `Texture::Sample2DRGB(` 会提示参数签名
- 鼠标悬停 `Function`、`Property`、`Output`、`UE.TexCoord`、局部变量会显示类型或来源
- `Find References` 可查找 `Function` / `Namespace::Function` 在工作区中的引用

### 格式化

- 文档级格式化

### 诊断

本地诊断会直接在编辑时标记：

- 花括号不匹配
- `import` 无法解析
- `Function` 参数声明错误
- `Function` 重复定义
- `.dsm` / `.dsh` 顶层结构错误
- `Code` 中的非法语句
- 未知变量 / 未知函数
- `Function` 调用参数数量不匹配
- `out` 参数不是合法变量名
- 纹理默认值 `Path(...)` 语法错误

另外还会读取 Unreal 桥接诊断：

- `Saved/DreamShader/Bridge/diagnostics.json`

## 2. 扩展命令

命令面板中可用：

- `DreamShaderLang: Recompile Current Source`
- `DreamShaderLang: Recompile All DSM`
- `DreamShaderLang: Install Package from GitHub`
- `DreamShaderLang: Browse Package Store`：打开 VSCode 风格 Webview 商店面板
- `DreamShaderLang: Update Installed Packages`
- `DreamShaderLang: Remove Installed Package`
- `DreamShaderLang: Open Packages Folder`
- `DreamShaderLang: Add Package Store Index Source`
- `DreamShaderLang: Remove Package Store Index Source`
- `DreamShaderLang: Create Package Step by Step`
- `DreamShaderLang: Create DreamShader Material`
- `DreamShaderLang: Create DreamShader Header`
- `DreamShaderLang: Create DreamShader Texture Sample`
- `DreamShaderLang: Create DreamShader Noise Material`

说明：

- 对 `.dsh` 执行当前文件重编时，会自动扩展成相关 `.dsm` 刷新
- Unreal 侧的错误会尽量回灌到 VSCode
- Package 命令会把 GitHub 仓库安装到 `DShader/Packages`，并维护 `DShader/dreamshader.lock.json`

## 3. 安装

```powershell
cd I:\UnrealProject_Moon\VSCodeExt\dreamshader-language-support
npm install
npm run package
```

生成的扩展包：

- `dreamshaderlang-language-support-1.0.0.vsix`

安装方式：

```powershell
code --install-extension .\dreamshaderlang-language-support-1.0.0.vsix
```

或在 VSCode 中通过 `Install from VSIX...` 安装。

## 4. 工作区设置

如果 VSCode 打开的不是 Unreal 项目根目录，可以在设置里指定：

```json
"dreamshader.projectRoot": "I:/UnrealProject_Moon/Moon_Dev"
```

Package store 配置：

```json
"dreamshader.packageStoreIndexUrls": [
    "https://raw.githubusercontent.com/TypeDreamMoon/dreamshader-package-index/main/packages.json"
],
"dreamshader.enableGitHubPackageSearch": true
```

`dreamshader.packageStoreIndexUrl` 旧单源配置仍兼容，但推荐使用 `dreamshader.packageStoreIndexUrls` 列表。

## 5. 当前边界

- 本地诊断已经够开发使用，但还不是 clangd 级别的完整编译器
- Unreal Parser 错误会尽量通过 source map 精确到真实 `.dsm/.dsh` 文件行列；材质编译器内部错误仍可能只有 Unreal 原始信息
- `Code` 语言服务主要面向图 DSL，不等同于完整 HLSL 语言服务器

## 6. 推荐使用方式

- `.dsm` 里写主逻辑
- `.dsh` 里写共用 `Function` 或 `Namespace`
- 常用共用能力可直接 `import "Builtin/Texture.dsh";`
- 也可以直接导入 `Builtin/Math.dsh`、`Builtin/Color.dsh`、`Builtin/UV.dsh`、`Builtin/Noise.dsh`、`Builtin/SDF.dsh`、`Builtin/Normal.dsh`、`Builtin/PBR.dsh`、`Builtin/PostProcess.dsh`
- 第三方 package 可直接 `import "@scope/package/Library/File.dsh";`
- 尽量让 `Function` 名、参数名、输出名清晰稳定
- 写 `Texture2D` 默认值时直接用 `Path(...)`，这样扩展和 Unreal 两侧都能识别
