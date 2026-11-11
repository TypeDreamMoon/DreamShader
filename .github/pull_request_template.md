## What / 改了什么

## Why / 为什么

<!-- Link the issue: Fixes #123 -->

## Test evidence / 测试证据

<!-- The command you ran and its result line. 贴你跑的命令和结果行。 -->

## Checklist / 自查

- [ ] Ran `pwsh -NoProfile -File Tools/Tests/Invoke-DreamShaderTests.ps1 -Preset Suite` in a test host / 已在测试宿主里跑过全量测试
- [ ] Docs updated, or not needed / 文档已更新,或无需更新
- [ ] CHANGELOG updated, or not needed / CHANGELOG 已更新,或无需更新
- [ ] Diagnostics regenerated with `.skill/gen-diagnostics.ps1`, or no `DSHnnnn` code changed / 已用 `.skill/gen-diagnostics.ps1` 重新生成诊断码页,或没有改动诊断码
- [ ] `Tools/Localization/localization_lint.ps1` is clean, or no user-facing C++ text changed / 本地化 lint 干净,或没有改动面向用户的 C++ 文本
- [ ] `Tools/Tests/Test-DscAssetCleanup.ps1` and `Tools/Tests/Test-DscInfo.ps1` pass, or `.skill/dsc.ps1` is unchanged / 两个驱动脚本测试通过,或没有改动 `.skill/dsc.ps1`
- [ ] No absolute local paths or personal data / 没有本机绝对路径或个人信息

## AI assistance / AI 辅助

- [ ] Parts of this PR were written with an AI tool, named here: ____ . I reviewed and tested every change. / 本 PR 部分内容由 AI 工具编写(工具名:____),我已逐项审阅并测试。
