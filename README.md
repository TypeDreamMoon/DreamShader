# DreamShader - A Unreal Engine Material DSL

> 该插件处于开发状态 | 但现在已经可以实现大部分功能 请酌情使用

DreamShader 是一个面向 Unreal 材质工作流的插件。它允许你使用 `DreamShaderLang` 编写 `.dsm` / `.dsh` 文本源文件，并自动生成或更新标准 Unreal `UMaterial` / `UMaterialFunction` 资产。

### 目前支持如下特性

- `Dream Shader Package` 类似npm包的Shader包 可以在VSCode拓展从存储库下载Package **我们也非常欢迎您的加入!**
- 非Dream Shader插件项目支持，使用了此插件生成的材质 可以在没有安装DreamShader插件的情况下使用! *Function说明符需要带上 Inline 或 SelfContained 关键字*
- HLSL Lang / GLSL Lang 语言语法支持
- 完善的VSCode支持 例如函数跳转 / Package Store
- 完全不需要在材质编辑器连连看! 也不需要在Detail面板调整材质球的任何属性!
- 完整的UnrealEngine MaterialExpressions节点支持

## 版本信息

- Version：`1.1.0`
- Language：`DreamShaderLang`
- Author：TypeDreamMoon
- GitHub：<https://github.com/TypeDreamMoon>
- Web：<https://dev.64hz.cn>
- Copyright：Copyright (c) 2026 TypeDreamMoon. All rights reserved.

## 文件模型

- `.dsm`：Dream Shader Material，材质实现文件
- `.dsh`：Dream Shader Header，共用函数头文件
- `Function`：推荐的共用函数语法
- `Inline` `SelfContained`: 函数自包含，自动把着色器代码包含在Material中 不需要.ush
- `Namespace`：组织共用函数，例如 `Texture::Sample2DRGB`
- `import`：在 `.dsm` 中引入 `.dsh`
- `Path(...)`：为纹理属性声明默认 Unreal 贴图资产
- `DShader/Packages`：项目安装的 DreamShader Package 共享库

## 快速示例

材质声明

```c
// 此处使用了dreamshader-texture的DreamShaderPackage
import "@typedreammoon/dreamshader-texture/Library/Texture";

// 一个简单的材质声明
Shader(Name="DreamMaterials/M_Sample")
{
	// 在Properties写的任何值 都会暴露成为属性 可以在MaterialInstance中修改
    Properties = {
	    // Path()说明符 可以引用在Engine目录或者Game目录下资源
        Texture2D DefaultTex = Path(Engine, "/EngineResources/DefaultTexture");
        vec3 Tint = vec3(1.0, 1.0, 1.0);
    }

	// 材质设置 支持Unreal Material中的所有属性修改
    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

	// 材质返回值
    Outputs = {
	    // 为了更加规范的返回值 我们需要提前声明一下返回值类型
        float3 Res;
        float Tangent;

		// 调用UE材质属性返回节点 将Res返回到EmissiveColor
        Base.EmissiveColor = Res;
        // 我们也支持直接写常量
        Base.BaseColor = float3(1, 1, 1);
        // 我们也支持类似TangentOutput节点的返回模式 只需要使用Expression(Class="").Pin[N] 会自动连接到节点的第N个引脚
        Expression(Class="TangentOutput").Pin[0] = Tangent;
    }

	// 材质图域
	// 这里就是要生成的材质节点实现的地方
	// 我们支持单个的属性声明
	// 这里支持基础 if / else 图分支；for / while 等复杂流程请写在 Function 里面
    Graph = {
	    float a = 0;
	    // 使用内置的TexCoord节点
        float2 uv = UE.TexCoord(Index=0);
        // Sample2DRGB的返回值
        float3 sampled_rgb;
        // 调用Package中的函数
        Texture::Sample2DRGB(DefaultTex, uv, sampled_rgb); // 
        // 调用定义的函数
        ApplyTint(sampled_rgb, Tint, Res);
        // 此外 我们还支持UE所有的UMatertialExpression节点 拿Sine节点为例
        float s = UE.Expression(Class="Sine", OutputType="float1", Input=UE.Time());
    }
}
```

> Material Function 使用 `ShaderFunction(Name="...")` 声明 写法与 `Shader` 关键字差不多 就是没有Settings *对于Material Function 如果您项目使用我们的插件 且不打算把这个函数功能发送给不用DreamShader插件的用户的话 我们还是建议您直接使用`Function`来声明*

函数声明

```c
// 该函数会定义在ush文件里面
Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
    result = color * tint;
}

// 带有 SelfContained 或 Inline 的函数 会自动包含在材质里面 可以给不使用Dream Shader插件的用户使用
Function SelfContained/Inline Func(in float a, out float b)
{
	b = a;
}
```

## 当前推荐工作流

- 把材质实现写在 `DShader/*.dsm`
- 把共用 `Function` 写在 `DShader/**/*.dsh`
- 通过 `import` 引入项目头文件或插件内置库
- 通过 `import "@scope/package/Library/File.dsh";` 引入已安装 package
- 第三方 package 位于 `项目根目录/DShader/Packages`
- 保存文件后由 DreamShader 自动解析并更新 Unreal 资产
- `.dsh` 变更会通过 import graph 只重编依赖它的 `.dsm`
- 生成资产会写入 `DreamShader.SourceHash`，源内容未变化时会跳过重复生成
- 如果启用了 VSCode 扩展，可以直接获得补全、跳转、格式化和本地语法诊断

## 文档

- [文档总览](Docs/README.md)
- [语法参考](Docs/LanguageReference.md)
- [示例与模式](Docs/Examples.md)
- [Package 系统](Docs/Packages.md)
- [VSCode 支持](Docs/VSCode.md)

## 重要说明

- `Shader` / `ShaderFunction` 中的 `Graph = { ... }` 是 DreamShader 图表达式 DSL，不是原始 HLSL
- `Function Name(...) { ... }` 的函数体是原始 helper 代码块，适合写复用逻辑
- `Namespace(Name="...") { Function ... }` 用来组织函数，调用格式为 `NamespaceName::FunctionName(...)`
- `Function` 调用使用显式 `out` 参数，不再支持 `Res = MyFunction(...)` 这种返回值风格
- Package 支持直接导入，例如 `import "@typedreammoon/dream-noise/Library/Noise.dsh";`
- Unreal 侧 Parser 错误会尽量映射回真实 `.dsm/.dsh` 文件的行列，包含被 `import` 的头文件
- Project Settings > Plugins > DreamShader 可配置源目录、内置库目录、生成目录、自动编译、防抖时间和详细日志
- 纹理默认值现在支持：
  - `Texture2D Tex = Path(Game, "/Textures/T_Mask");`
  - `TextureCube Sky = Path("/Engine/EngineResources/DefaultTextureCube");`

## VSCode 扩展

当前支持：

- `DreamShaderLang` `.dsm` / `.dsh` 语法高亮
- 作用域感知补全
- `Function` / `Namespace::Function` / `import` / `Path(...)` 联想
- 跳转到 `Function` / `Namespace::Function` / `import`
- Signature Help
- Hover 类型/来源提示
- Find References
- 文档格式化
- 本地语法诊断
- Unreal 桥接诊断
- GitHub Package 安装、更新、移除和商店浏览
- 快速创建 Material/Header/Texture Sample/Noise Material 模板

扩展单独说明见 [Docs/VSCode.md](Docs/VSCode.md)。

## RoadMap

- Custom Render Pass
