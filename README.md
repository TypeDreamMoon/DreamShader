# DreamShader

DreamShader 是一个面向 Unreal 材质工作流的插件。它允许你使用 `DreamShaderLang` 编写 `.dsm` / `.dsh` 文本源文件，并自动生成或更新标准 Unreal `UMaterial` / `UMaterialFunction` 资产。

> 后续可能支持RenderPass的编写

## 版本信息

- Version：`1.0.0`
- Language：`DreamShaderLang`
- Author：TypeDreamMoon
- GitHub：<https://github.com/TypeDreamMoon>
- Web：<https://dev.64hz.cn>
- Copyright：Copyright (c) 2026 TypeDreamMoon. All rights reserved.

## 文件模型

- `.dsm`：Dream Shader Material，材质实现文件
- `.dsh`：Dream Shader Header，共用函数头文件
- `Function`：推荐的共用函数语法
- `Namespace`：组织共用函数，例如 `Texture::Sample2DRGB`
- `import`：在 `.dsm` 中引入 `.dsh`
- `Path(...)`：为纹理属性声明默认 Unreal 贴图资产
- `Plugins/DreamShader/Library`：插件内置 DreamShader 函数库

## 快速示例

```c
import "Builtin/Texture.dsh";

Shader(Name="DreamMaterials/M_Sample")
{
    Properties = {
        Texture2D DefaultTex = Path(Engine, "/EngineResources/DefaultTexture");
        vec3 Tint = vec3(1.0, 1.0, 1.0);
    }

    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        float3 Res;

        Base.EmissiveColor = Res;
    }

    Code = {
        float2 uv = UE.TexCoord(Index=0);
        float3 sampled_rgb;
        Texture::Sample2DRGB(DefaultTex, uv, sampled_rgb);
        ApplyTint(sampled_rgb, Tint, Res);
    }
}
```

```c
Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
    result = color * tint;
}
```

## 当前推荐工作流

- 把材质实现写在 `DShader/*.dsm`
- 把共用 `Function` 写在 `DShader/**/*.dsh`
- 通过 `import` 引入项目头文件或插件内置库
- 常用内置库位于 `Plugins/DreamShader/Library/Builtin/*.dsh`
- 保存文件后由 DreamShader 自动解析并更新 Unreal 资产
- 如果启用了 VSCode 扩展，可以直接获得补全、跳转、格式化和本地语法诊断

## 文档

- [文档总览](Docs/README.md)
- [语法参考](Docs/LanguageReference.md)
- [示例与模式](Docs/Examples.md)
- [VSCode 支持](Docs/VSCode.md)
- [迁移说明](Docs/Migration.md)

## 重要说明

- `Shader` / `ShaderFunction` 中的 `Code = { ... }` 是 DreamShader 图表达式 DSL，不是原始 HLSL
- `Function Name(...) { ... }` 的函数体是原始 helper 代码块，适合写复用逻辑
- `Namespace(Name="...") { Function ... }` 用来组织函数，调用格式为 `NamespaceName::FunctionName(...)`
- `Function` 调用使用显式 `out` 参数，不再支持 `Res = MyFunction(...)` 这种返回值风格
- 旧的 `Scalar` / `Color` / `Vector` 类型别名已经移除，请改用 `float` / `float2` / `float3` / `float4` 或 `vec*`
- 内置库支持直接导入，例如 `import "Builtin/Texture.dsh";`
- 纹理默认值现在支持：
  - `Texture2D Tex = Path(Game, "/Textures/T_Mask");`
  - `TextureCube Sky = Path("/Engine/EngineResources/DefaultTextureCube");`

## VSCode 扩展

VSCode 扩展位于：

- `I:/UnrealProject_Moon/VSCodeExt/dreamshader-language-support`

它当前支持：

- `DreamShaderLang` `.dsm` / `.dsh` 语法高亮
- 作用域感知补全
- `Function` / `Namespace::Function` / `import` / `Path(...)` 联想
- 跳转到 `Function` / `Namespace::Function` / `import`
- 文档格式化
- 本地语法诊断
- Unreal 桥接诊断

扩展单独说明见 [Docs/VSCode.md](Docs/VSCode.md)。
