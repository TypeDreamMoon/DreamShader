# DreamShaderLang 语法参考

DreamShaderLang 是 DreamShader 插件使用的文本语言。它用 `.dsm` / `.dsh` 源文件描述 Unreal 材质、材质函数和共享 helper，并由插件生成标准 Unreal 资产。

| 项目 | 内容 |
| --- | --- |
| 插件版本 | `1.2.0` |
| 源文件 | `.dsm` / `.dsh` |
| 主要产物 | `UMaterial` / `UMaterialFunction` |
| 开发者 | TypeDreamMoon |

## 1. 文件类型

### 1.1 `.dsm`

Dream Shader Material。用于生成资产，通常包含：

- `Shader(Name="...")`
- `ShaderFunction(Name="...")`
- `VirtualFunction(Name="...")`
- `import "Shared/Common.dsh";`

一个 `.dsm` 可以包含共享 `Function` / `Namespace`，但推荐把可复用逻辑放入 `.dsh`，让材质文件更聚焦。

### 1.2 `.dsh`

Dream Shader Header。用于存放共享代码，通常包含：

- `import "OtherHeader.dsh";`
- `Function Name(...) { ... }`
- `Namespace(Name="...") { ... }`
- `VirtualFunction(Name="...")`

`.dsh` 不建议包含 `Shader(...)` 或 `ShaderFunction(...)`，但可以包含 `VirtualFunction(...)` 这种只声明现有资产的签名。

## 2. 顶层声明

### 2.1 `Shader(Name="...", Root="...")`

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
        float3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = float3(Strength, Strength, Strength);
    }
}
```

规则：

- `Name` 必填，建议使用 Unreal package 风格路径。
- `Root` 可选，默认 `Game`。`Root="Game"` 生成到 `/Game`，`Root="Plugin.PluginName"` 生成到已启用的项目内容插件根 `/PluginName`，物理路径位于 `[Project]/Plugins/PluginName/Content`。
- `Root` 可以追加子目录，例如 `Root="Game/Generated"` 或 `Root="Plugin.PluginName/Generated"`。
- 例如 `Shader(Name="Mat/Test", Root="Plugin.MoonToon")` 会生成 `/MoonToon/Mat/Test.Test`，并保存到 `[Project]/Plugins/MoonToon/Content/Mat/Test.uasset`。
- `Plugins.MoonToon` / `Plugins/MoonToon` 也作为兼容写法支持，解析结果与 `Plugin.MoonToon` 相同。
- 插件目标需要是已启用且可包含内容的项目 Unreal 插件，否则生成器会报错。
- `Properties` / `Settings` / `Outputs` / `Graph` 都是 section。
- `Graph` 是材质图实现区域。

### 2.2 `ShaderFunction(Name="...", Root="...")`

生成 Unreal `UMaterialFunction`。

```c
ShaderFunction(Name="Functions/F_Tint", Root="Plugin.MyPlugin")
{
    Inputs = {
        vec3 InColor;
        vec3 InTint;
    }

    Outputs = {
        vec3 OutColor;
    }

    Settings = {
        Description = "Tint helper";
        ExposeToLibrary = true;
    }

    Graph = {
        OutColor = InColor * InTint;
    }
}
```

规则：

- `Name` 必填。
- `Root` 可选，规则同 `Shader`。
- `Inputs` 声明输入 pin。
- `Outputs` 声明输出 pin。
- `Graph` 负责生成材质函数内部图。

### 2.3 `VirtualFunction(Name="...")`

声明一个已经存在的 Unreal `UMaterialFunction`，让 `Graph` 可以像调用 `ShaderFunction` 一样调用它。`VirtualFunction` 不会生成、保存或覆盖对应资产。

```c
VirtualFunction(Name="BufferWriter")
{
    Options = {
        Asset = Path(Plugins.MoonToon, "MaterialFunctions/Buffer/Writer");
        Description = "Generated from /MoonToon/MaterialFunctions/Buffer/Writer";
    }

    Inputs = {
        float3 Color;
        float Alpha;
    }

    Outputs = {
        float3 Result;
    }
}
```

规则：

- `Name` 必填，作为 `Graph` 中的调用名。
- `Options.Asset` 必填，指向现有 `UMaterialFunction` 资产。
- `Options.Asset` 支持 `Path(Game, "...")`、`Path(Engine, "...")`、`Path(Plugin.PluginName, "...")` / `Path(Plugins.PluginName, "...")`，也支持完整 `/Game/...`、`/Engine/...` 或插件挂载 object path。
- `Inputs` / `Outputs` 必须与现有材质函数的输入输出顺序和名称对应；自动生成按钮会从资产读取完整签名。
- 不支持 `Graph` / `Code` section。

调用示例：

```c
Graph = {
    Result = BufferWriter(Color, 1.0, Output="Result");
}
```

### 2.4 `Function [Inline|SelfContained] Name(...) { ... }`

定义可复用 helper。函数体是 HLSL 风格代码。

```c
Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
    result = color * tint;
}
```

自包含写法：

```c
Function SelfContained ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
    result = color * tint;
}
```

规则：

- 参数支持 `in` / `out`。
- 至少声明一个 `out` 参数。
- 调用时必须显式传入 `out` 目标变量。
- `Inline` 是 `SelfContained` 的别名。
- 普通 `Function` 会生成 `.ush` 并由 Custom 节点 include。
- `SelfContained` / `Inline` 会把依赖代码嵌入 Custom 节点，便于生成资产脱离 DreamShader 插件使用。

### 2.5 `Namespace(Name="...")`

组织一组共享 helper。

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
Texture::Sample2DRGB(MainTex, uv, sampledColor);
```

规则：

- `Namespace` 内只能包含 `Function`。
- namespace 名必须是合法标识符。
- 生成 HLSL 时会把 `Texture::Sample2DRGB` 映射为安全的内部符号。

## 3. Section

### 3.1 `Properties`

`Shader` 的材质输入参数。

```c
Properties = {
    float Strength = 1.0;
    vec3 Tint = vec3(1.0, 1.0, 1.0);
    Texture2D MainTex = Path(Game, "/Textures/T_Main");
}
```

### 3.2 `Inputs`

`ShaderFunction` / `VirtualFunction` 的输入 pin。

```c
Inputs = {
    vec3 InColor;
    float Strength = 1.0;
}
```

### 3.3 `Outputs`

`Shader` 中既能声明输出变量，也能绑定 Unreal 材质属性。

```c
Outputs = {
    float3 Color;
    float OpacityValue;

    Base.EmissiveColor = Color;
    Base.Opacity = OpacityValue;
}
```

`ShaderFunction` / `VirtualFunction` 中用于声明输出 pin：

```c
Outputs = {
    vec3 OutColor;
}
```

### 3.4 `Settings`

配置 Unreal 材质或 Material Function 属性。

常用设置：

| 设置 | 示例 |
| --- | --- |
| `Domain` / `MaterialDomain` | `"Surface"` / `"UI"` / `"PostProcess"` |
| `ShadingModel` | `"Unlit"` / `"DefaultLit"` |
| `BlendMode` / `RenderType` | `"Opaque"` / `"Translucent"` |
| `TwoSided` | `true` / `false` |
| `Wireframe` | `true` / `false` |
| `Description` | `"Tint helper"` |
| `ExposeToLibrary` | `true` |
| `LibraryCategories` | `"DreamShader,Color"` |

### 3.5 `Options`

`VirtualFunction` 中用于描述外部资产引用：

```c
Options = {
    Asset = Path(Plugins.MoonToon, "MaterialFunctions/Buffer/Writer");
}
```

`Settings` 也可作为兼容别名使用，但推荐新代码使用 `Options`。

### 3.6 `Graph`

`Graph` 是 `Shader` / `ShaderFunction` 内的图 DSL，负责生成 Unreal 材质节点。

支持：

- 变量声明和赋值。
- 标量、向量构造。
- Brace initializer。
- `UE.*` builtin 调用。
- `Function(...)` / `Namespace::Function(...)` 独立调用。
- `ShaderFunction(...)` / `VirtualFunction(...)` 值调用。
- 基础 `if` / `else` 图分支。
- 将结果绑定到输出变量。

限制：

- 不支持 `for` / `while`。
- 不适合写复杂流程控制。
- 条件分支会转换为 Material `If` 节点，而不是运行时普通 CPU 分支。

## 4. `import`

在 `.dsm` 或 `.dsh` 顶部引入头文件：

```c
import "Shared/Common.dsh";
import "Builtin/Texture.dsh";
import "@typedreammoon/dream-noise/Library/Noise.dsh";
```

解析顺序：

| 路径形式 | 解析位置 |
| --- | --- |
| `"Shared/Common.dsh"` | 当前文件目录和项目 `DShader` 根目录。 |
| `"Builtin/Texture.dsh"` | 插件内置库目录。 |
| `"@scope/package/Library/File.dsh"` | 项目 `DShader/Packages`。 |

规则：

- 支持递归导入。
- 会检测循环导入。
- `.dsh` 变更后只刷新依赖它的 `.dsm`。

Package 相关说明见 [Packages.md](Packages.md)。

## 5. 类型系统

### 5.1 标量与向量

| 类型族 | 支持类型 |
| --- | --- |
| float | `float` / `float1` / `float2` / `float3` / `float4` |
| half | `half` / `half1` / `half2` / `half3` / `half4` |
| int | `int` / `int2` / `int3` / `int4` |
| uint | `uint` / `uint2` / `uint3` / `uint4` |
| bool | `bool` / `bool2` / `bool3` / `bool4` |

### 5.2 GLSL 风格别名

| 别名 | 等价类型 |
| --- | --- |
| `vec2` / `vec3` / `vec4` | `float2` / `float3` / `float4` |
| `ivec2` / `ivec3` / `ivec4` | `int2` / `int3` / `int4` |
| `uvec2` / `uvec3` / `uvec4` | `uint2` / `uint3` / `uint4` |
| `bvec2` / `bvec3` / `bvec4` | `bool2` / `bool3` / `bool4` |
| `mat2` / `mat3` / `mat4` | `float2x2` / `float3x3` / `float4x4` |

### 5.3 纹理与采样相关类型

- `Texture2D`
- `TextureCube`
- `Texture2DArray`
- `SamplerState`

### 5.4 已移除别名

`Scalar` / `Color` / `Vector` 已移除。推荐使用：

- `float`
- `float2` / `vec2`
- `float3` / `vec3`
- `float4` / `vec4`

## 6. `Path(...)`

纹理属性支持通过 `Path(...)` 绑定默认 Unreal 资产。

```c
Properties = {
    Texture2D MainTex = Path(Game, "/Textures/T_Main");
    Texture2D DefaultTex = Path("/Engine/EngineResources/DefaultTexture");
    TextureCube SkyTex = Path(Engine, "/EngineResources/DefaultTextureCube");
}
```

规则：

- 单参数形式必须使用 `/Game/...` 或 `/Engine/...`。
- 双参数形式的根名支持 `Game` / `Engine`。
- 如果未显式写 `.AssetName`，会自动补成合法 Unreal object path。
- 会校验声明类型和实际资产类型是否一致。

## 7. `Function` 调用语义

DreamShader 使用显式 `out` 调用。

定义：

```c
Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
    result = color * tint;
}
```

调用：

```c
Graph = {
    float3 base = vec3(1.0, 0.5, 0.2);
    float3 tint = vec3(0.5, 1.0, 1.0);
    float3 result;

    ApplyTint(base, tint, result);
}
```

不支持返回值风格：

```c
result = ApplyTint(base, tint);
```

## 8. `Graph` 语法

### 8.1 声明

```c
float a;
float2 uv;
float3 color;
float4 sampleValue;
```

标量和向量只声明时会自动初始化为 `0`。

### 8.2 构造

```c
float4 colorA = float4(rgb, 1.0);
float3 colorB = float3(sampleValue.r, sampleValue.g, sampleValue.b);
```

### 8.3 Brace initializer

```c
float4 colorA = {rgb, 1.0};
float3 colorB = {r, g, b};
```

### 8.4 赋值

```c
Color = Tint;
OpacityValue = 0.75;
```

### 8.5 `if` / `else`

```c
if (Mask > 0.5) {
    Color = Tint;
} else {
    Color = vec3(0.0, 0.0, 0.0);
}
```

条件规则：

- 条件两侧必须是标量。
- 支持 `>` / `<` / `>=` / `<=` / `==` / `!=`。
- `if (Mask)` 等价于 `Mask > 0`。
- 分支中给同一变量或输出赋值时，生成器会用 Material `If` 节点合并两侧结果。
- 不能用 `if` 选择 `Texture2D` 值。

## 9. `UE.*` builtin

`Graph` 中可以通过 `UE.*` 创建 Unreal 材质节点。

常用 builtin：

| 调用 | 说明 |
| --- | --- |
| `UE.TexCoord(Index=0)` | Texture Coordinate。 |
| `UE.Time()` | Time。 |
| `UE.Panner(...)` | Panner。 |
| `UE.WorldPosition()` | World Position。 |
| `UE.ObjectPositionWS()` | Object Position WS。 |
| `UE.CameraVectorWS()` | Camera Vector WS。 |
| `UE.ScreenPosition()` | Screen Position。 |
| `UE.VertexColor()` | Vertex Color。 |
| `UE.TransformVector(...)` | Vector Transform。 |
| `UE.TransformPosition(...)` | Position Transform。 |
| `UE.Expression(...)` | 泛型 MaterialExpression 创建入口。 |

泛型示例：

```c
float pulse = UE.Expression(
    Class="Sine",
    OutputType="float1",
    Input=UE.Time());
```

## 10. 输出绑定

`Shader` 的 `Outputs` 支持材质属性绑定：

```c
Outputs = {
    float3 Color;
    float Alpha;

    Base.BaseColor = Color;
    Base.Opacity = Alpha;
}
```

辅助输出节点可以使用 `Expression(...).Pin[n]`：

```c
Outputs = {
    float Tangent;
    Expression(Class="TangentOutput").Pin[0] = Tangent;
}
```

## 11. 编译与缓存

DreamShader 会维护源文件和资产之间的关系：

- `.dsm` 直接生成资产。
- `.dsh` 不直接生成资产。
- `.dsh` 保存后只重编依赖它的 `.dsm`。
- Parser 错误会尽量通过 source map 映射回真实 `.dsm` / `.dsh` 行列。
- 生成资产会写入 `DreamShader.SourceFile`、`DreamShader.SourceHash`、`DreamShader.GeneratedAtUtc`。
- source hash 未变化时会跳过重复生成。

## 12. Project Settings

Project Settings > Plugins > DreamShader：

| 设置 | 默认值 | 说明 |
| --- | --- | --- |
| `SourceDirectory` | `DShader` | 源文件根目录。 |
| `GeneratedShaderDirectory` | `Intermediate/DreamShader/GeneratedShaders` | 生成 `.ush` 目录。 |
| `AutoCompileOnSave` | `true` | 保存时自动生成资产。 |
| `SaveDebounceSeconds` | `0.25` | 保存防抖时间。 |
| `VerboseLogs` | `false` | 输出详细日志。 |

## 13. 当前限制

- `Graph` 不是完整通用语言。
- `Graph` 支持基础 `if` / `else`，不支持 `for` / `while`。
- 复杂流程建议放进 `Function`。
- `Function` 调用必须显式传 `out` 目标变量。
- `Namespace` 当前只用于组织 `Function`。
- `Path(...)` 当前主要面向 `Game` / `Engine` 根路径。
- VSCode 诊断是开发辅助，不等同于完整编译器语义系统。
