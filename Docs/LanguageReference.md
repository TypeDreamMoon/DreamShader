# DreamShaderLang 语法参考

DreamShaderLang 是 DreamShader 插件使用的 `.dsm` / `.dsh` 文本语言。

- 插件版本：`1.0.0`
- 开发者：TypeDreamMoon
- GitHub：<https://github.com/TypeDreamMoon>
- Web：<https://dev.64hz.cn>
- Copyright：Copyright (c) 2026 TypeDreamMoon. All rights reserved.

## 1. 文件类型

### `.dsm`

Dream Shader Material，实现文件。用于生成 `UMaterial` 或 `UMaterialFunction`，通常包含：

- `Shader(Name="...")`
- `ShaderFunction(Name="...")`
- `import "Shared/Common.dsh";`

### `.dsh`

Dream Shader Header，共用头文件，类似 C/C++ header。当前推荐只包含：

- `import "OtherHeader.dsh";`
- `Function Name(...) { ... }`
- `Namespace(Name="...") { ... }`

`.dsh` 中不要放 `Shader(...)` 或 `ShaderFunction(...)`。

## 2. 顶层 Block

### 2.1 `Shader(Name="...")`

生成 Unreal `UMaterial`。

```c
Shader(Name="DreamMaterials/M_Sample")
{
    Properties = {
        float Strength = 1.0;
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
        Res = float3(Strength, Strength, Strength);
    }
}
```

### 2.2 `ShaderFunction(Name="...")`

生成 Unreal `UMaterialFunction`。

```c
ShaderFunction(Name="Functions/TintColor")
{
    Inputs = {
        vec3 InColor;
        vec3 InTint;
    }

    Outputs = {
        vec3 OutColor;
    }

    Code = {
        OutColor = InColor * InTint;
    }
}
```

### 2.3 `Function Name(...) { ... }`

定义可复用 helper，是当前唯一推荐的 helper 语法。

```c
Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
    result = color * tint;
}
```

规则：

- 参数支持 `in` / `out`
- 至少声明一个 `out`
- 调用时显式传入 `out` 目标变量
- 函数体是原始 helper 代码，可以写 `if` / `for` 等流程逻辑

### 2.4 `Namespace(Name="...")`

命名空间用于组织一组共用 `Function`，避免再靠 `DS_` 这类前缀规避命名冲突。

```c
Namespace(Name="Texture")
{
    Function Sample2DRGB(in Texture2D texture, in float2 uv, out float3 color) {
        color = Texture2DSample(texture, textureSampler, uv).rgb;
    }
}
```

调用方式：

```c
Texture::Sample2DRGB(MainTex, uv, sampled_rgb);
```

规则：

- `Namespace` 只能包含 `Function`
- namespace 名必须是合法标识符
- 生成 HLSL 时会自动把 `Texture::Sample2DRGB` 映射为安全的内部符号

## 3. `import`

在 `.dsm` 或 `.dsh` 顶部引入头文件：

```c
import "Shared/Common.dsh";
import "Shared/Noise/FBM.dsh";
import "Builtin/Texture.dsh";
```

规则：

- 路径相对当前文件目录、项目 `DShader/` 根目录，或插件 `Plugins/DreamShader/Library/` 解析
- 支持递归导入
- 会检测循环导入

常见内置头文件：

- `Builtin/Common.dsh`
- `Builtin/Texture.dsh`

## 4. Section 说明

### 4.1 `Properties`

`Shader` 中声明材质输入参数或 UE builtin 属性。

```c
Properties = {
    float Strength = 1.0;
    vec3 Tint = vec3(1.0, 1.0, 1.0);
    Texture2D MainTex = Path(Game, "/Textures/T_Main");
}
```

### 4.2 `Inputs`

`ShaderFunction` 输入 pin。

### 4.3 `Outputs`

两种用途：

- 在 `Shader` 中声明输出变量并绑定到 Unreal 材质输出
- 在 `ShaderFunction` 中声明函数输出 pin

```c
Outputs = {
    float3 Res;
    float OpacityValue;

    Base.EmissiveColor = Res;
    Base.Opacity = OpacityValue;
}
```

### 4.4 `Settings`

配置 Unreal 材质 / MaterialFunction 设置。

常用项：

- `Domain` / `MaterialDomain`
- `ShadingModel`
- `BlendMode` / `RenderType`
- `TwoSided`
- `Wireframe`
- `Description`
- `ExposeToLibrary`
- `UserExposedCaption`
- `LibraryCategories`

### 4.5 `Code`

在 `Shader` / `ShaderFunction` 里，`Code` 是 DreamShader 图 DSL。

它支持：

- 变量声明
- 变量赋值
- 标量和向量构造
- brace initializer
- `UE.*` builtin 调用
- 独立 `Function(...)` 或 `Namespace::Function(...)` 调用
- 将结果绑定到输出变量

不建议在 `Code` 中写：

- `if`
- `for`
- `while`
- 复杂流程控制

这些逻辑应放进 `Function`。

## 5. 类型系统

### 5.1 基础类型

- `float` / `float2` / `float3` / `float4`
- `half` / `half2` / `half3` / `half4`
- `int` / `int2` / `int3` / `int4`
- `uint` / `uint2` / `uint3` / `uint4`
- `bool` / `bool2` / `bool3` / `bool4`

### 5.2 GLSL 风格别名

- `vec2` = `float2`
- `vec3` = `float3`
- `vec4` = `float4`
- `ivec*` / `uvec*` / `bvec*` 同理
- `mat2` / `mat3` / `mat4` 对应 `float2x2` / `float3x3` / `float4x4`

### 5.3 纹理

- `Texture2D`
- `TextureCube`
- `Texture2DArray`
- `SamplerState`

### 5.4 已移除旧别名

`Scalar` / `Color` / `Vector` 已移除。请改用：

- `float`
- `float2` / `vec2`
- `float3` / `vec3`
- `float4` / `vec4`

## 6. 纹理默认值：`Path(...)`

纹理属性支持引用 Unreal 贴图资产：

```c
Properties = {
    Texture2D MainTex = Path(Game, "/Textures/T_Main");
    Texture2D DefaultTex = Path("/Engine/EngineResources/DefaultTexture");
    TextureCube SkyTex = Path(Engine, "/EngineResources/DefaultTextureCube");
}
```

规则：

- 单参数时必须是 `/Game/...` 或 `/Engine/...`
- 双参数时根名支持 `Game` / `Engine`
- 如果未显式写 `.AssetName`，会自动补成合法 Unreal object path
- 会校验声明类型和实际资产类型是否一致

## 7. 插件内置库

DreamShader 插件目录下自带一组可直接导入的头文件：

```text
Plugins/DreamShader/Library/Builtin/Common.dsh
Plugins/DreamShader/Library/Builtin/Texture.dsh
```

示例：

```c
import "Builtin/Texture.dsh";

Code = {
    float2 uv = UE.TexCoord(Index=0);
    float3 sampled_rgb;
    Texture::Sample2DRGB(MainTex, uv, sampled_rgb);
    Res = sampled_rgb;
}
```

当前 `Texture.dsh` 提供：

- `Texture::Sample2D`
- `Texture::Sample2DRGB`
- `Texture::Sample2DAlpha`
- `Texture::SampleCube`
- `Texture::SampleCubeRGB`
- `Texture::Sample2DArray`
- `Texture::Sample2DArrayRGB`
- `Texture::Sample2DArrayAlpha`

## 8. `Function` 调用语义

当前使用显式 `out` 调用：

```c
Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
    result = color * tint;
}
```

调用方式：

```c
Code = {
    float3 base = vec3(1.0, 0.5, 0.2);
    float3 tint = vec3(0.5, 1.0, 1.0);
    float3 res;
    ApplyTint(base, tint, res);
}
```

不支持返回值风格：

```c
Res = ApplyTint(base, tint);
```

## 9. `Code` 中支持的声明与构造

### 9.1 仅声明

```c
float a;
float2 uv;
float3 color;
float4 sample_v4;
```

标量和向量只声明时会自动初始化为 0。

### 9.2 普通构造

```c
float4 c = float4(color, 1.0);
float3 rgb = float3(sample_v4.r, sample_v4.g, sample_v4.b);
```

### 9.3 Brace initializer

```c
float4 c = {color, 1.0};
float3 d = {a, b, c};
```

## 10. `UE.*` builtin

`Code` 中可直接生成 Unreal 材质节点，例如：

- `UE.TexCoord(Index=0)`
- `UE.Time()`
- `UE.Panner(...)`
- `UE.WorldPosition()`
- `UE.ObjectPositionWS()`
- `UE.CameraVectorWS()`
- `UE.ScreenPosition()`
- `UE.VertexColor()`
- `UE.TransformVector(...)`
- `UE.TransformPosition(...)`
- `UE.Expression(...)`

## 11. 输出绑定

`Shader` 的 `Outputs` 里既可以写变量声明，也可以写绑定：

```c
Outputs = {
    float3 Res;
    float AlphaValue;

    Base.EmissiveColor = Res;
    Base.Opacity = AlphaValue;
}
```

## 12. 当前限制

- `Code` 不是完整通用语言
- `Function` 调用必须显式传 `out`
- `Namespace` 当前只用于组织 `Function`
- `Path(...)` 目前主要面向 `Game` / `Engine`
- VSCode 诊断已经够日常开发，但仍不是完整编译器语义系统
