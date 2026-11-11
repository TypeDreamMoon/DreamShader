# 参与 DreamShader

[English](CONTRIBUTING.md) · [简体中文](CONTRIBUTING.zh-CN.md)

感谢参与。本页说明怎么报问题、怎么提改动,以及 PR 合并前要满足什么。

## 参与方式

- **问题反馈**——结果不对、崩溃,或与文档不符。
- **功能与语法提案**——DreamShaderLang 现在还表达不了的东西。
- **文档与翻译修正**——页面有错、看不懂、缺内容,或翻译有问题。
- **小修**——错别字、错误提示写错、单文件的 bug 修复。直接开 PR。
- **功能 PR**——先开提案 issue,等设计谈妥再写。这样不会白写一份合不进来的代码。

## 报告问题

1. 先搜一下[现有 issue](https://github.com/TypeDreamMoon/DreamShader/issues)。
2. 在工程根目录运行下面这条命令,复制输出。它不启动引擎,也不打印路径:

   ```powershell
   pwsh -File Plugins/DreamShader/.skill/dsc.ps1 info
   ```

3. 打开[新建 issue](https://github.com/TypeDreamMoon/DreamShader/issues/new/choose),选一张表单。
   把第 2 步的输出贴进 **Environment / 环境**。
4. 给出能复现问题的最小 `.dss` / `.dsi` / `.dsp` / `.dsm` / `.dsf` 源码。它应当脱离你工程的其余部分也能编译。

如果是 AI 工具写的源码编不过、或者用了不存在的名字,请用 **AI wrote it wrong / AI 写错了** 表单。
这类反馈用来改进 agent 指南和技能。

## 提问与想法

用法提问和想法请发到 [GitHub Discussions](https://github.com/TypeDreamMoon/DreamShader/discussions)。
issue 只用于 bug 和已经谈妥的工作。

## 开发环境

- Unreal Engine 5.3–5.8,Win64。开发以 5.8 为准;Custom Pass(`.dsp`)需要 5.8。
- 仓里所有脚本都用 PowerShell 7(`pwsh`)。
- 在测试宿主里干活。测试宿主是一个小工程,里面放着本仓的一个 git worktree:

  ```powershell
  pwsh -NoProfile -File Tools/TestHost/New-DreamShaderTestHost.ps1 -Root <宿主目录> -Branch <分支>
  ```

- 下面的测试脚本会先编译宿主的编辑器目标,再跑测试。

细节见 [Tools/TestHost/README.md](Tools/TestHost/README.md) 和
[贡献手册](Docs/contributing/index.md)(模块、构建规则、引擎矩阵)。

## 开 PR 之前

在宿主的 `Plugins/DreamShader/` 下跑全量测试。先关掉开着这个宿主的编辑器:

```powershell
pwsh -NoProfile -File Tools/Tests/Invoke-DreamShaderTests.ps1 -Preset Suite
```

退出码是意外失败的个数,`0` 为通过。改了 `.skill/dsc.ps1` 的,再跑
`Tools/Tests/Test-DscAssetCleanup.ps1` 和 `Tools/Tests/Test-DscInfo.ps1`(几秒,不需要引擎)。

改动涉及下面这些时,在同一个 PR 里一并更新:

- **文档**——`Docs/` 下描述这个行为的英文页。
- **CHANGELOG**——用户能感知的改动,在 `CHANGELOG.md` 的 `## Unreleased` 下记一条。
- **诊断码**——增删 `DSHnnnn` 码后,运行 `pwsh -File .skill/gen-diagnostics.ps1`。
- **本地化**——改了面向用户的 C++ 文本后,运行 `pwsh -File Tools/Localization/localization_lint.ps1`。
- **引擎版本**——版本门要在它影响的每个引擎上编过:`.skill/build-plugin.ps1`。

测试的更多说明:[Docs/contributing/testing.md](Docs/contributing/testing.md)。

## 提交与 PR

- 每个提交要小,只讲一件事。
- 提交说明照历史的写法:`fix(scope): what changed`、`docs: …`、`feat(tools): …`。
- 分支命名 `<类型>/<主题>`,例如 `fix/static-switch-bool-branches`、`docs/unify-labels`。
- 按 PR 模板填写:改了什么、为什么、测试命令和结果行。
- 入库文件里不要有本机绝对路径、用户名或邮箱。

## AI 辅助的贡献

欢迎用 AI 辅助的 PR,规矩如下:

- 先让 agent 读 [`AGENTS.md`](AGENTS.md)。
- 在 PR 里写明用了什么工具和模型。
- 贴上测试命令的结果行。没有测试证据的 PR 不会被审。
- 你对提交的每一行负责。像自己写的一样读一遍、测一遍。
- 没有测试证据的批量 AI PR 会被关闭。

## 文档与翻译

英文是源。改 `Docs/` 下的英文页。

`Docs/zh-CN/` 是英文页的镜像。改动进入 `main` 后,CI 用 [dreamdocs](https://github.com/TypeDreamMoon/dreamdocs)
翻译改过的段落,并开一个 PR 供审阅。你不需要自己翻译自己的英文改动。要修正中文译文,直接改中文页;
下一次运行会保留你的修改。`README.zh-CN.md` 和 `CONTRIBUTING.zh-CN.md` 是手工维护的。

发布的手册在 <https://shader.toolchain.64hz.cn>。

## 许可证

DreamShader 以 [MIT 许可证](LICENSE)发布。参与贡献即表示你同意你的贡献按同一许可证授权。
不需要签贡献者许可协议(CLA)。

## 行为准则与安全

- [行为准则](https://github.com/TypeDreamMoon/.github/blob/main/CODE_OF_CONDUCT.md)
- [安全策略](https://github.com/TypeDreamMoon/.github/blob/main/SECURITY.md)——漏洞请私下报告,不要发在 issue 里。
