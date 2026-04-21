# DreamShaderLang 迁移说明

这份说明用于把旧 DreamShader 写法迁移到 DreamShader `1.0.0` 推荐的 DreamShaderLang 模型。

## 1. 文件模型迁移

### 旧模型

- `.dsh` 同时承担材质实现和共用 helper

### 新模型

- `.dsm`：材质实现
- `.dsh`：共用头文件

### 推荐迁移

把原来用于生成材质的 `.dsh` 改名为 `.dsm`：

```text
DShader/SampleShader.dsh
```

迁移为：

```text
DShader/SampleShader.dsm
```

把共用 helper 拆到单独头文件：

```text
DShader/Shared/Common.dsh
```

## 2. helper 语法迁移

旧式 helper block 已移除。新代码统一使用 `Function Name(...) { ... }`。

### 新写法

```c
Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
    result = color * tint;
}
```

如果一组 helper 原来靠 `DS_`、`Common_` 这类前缀区分，建议迁移到 `Namespace`：

```c
Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

## 3. 调用风格迁移

### 旧写法

```c
Code = {
    Res = ApplyTint(Color, Tint);
}
```

### 新写法

```c
Code = {
    vec3 res;
    ApplyTint(Color, Tint, res);
    Res = res;
}
```

或者更常见：

```c
Code = {
    ApplyTint(Color, Tint, Res);
}
```

## 4. 共享函数迁移

### 旧模型

同一个材质文件里既写主材质，又写一堆 helper。

### 新模型

把 helper 拆到 `.dsh`：

```c
// DShader/Shared/Common.dsh
Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

在 `.dsm` 中导入：

```c
import "Shared/Common.dsh";
```

## 5. `Code` 声明能力迁移

当前 `Code` 已支持：

- 无初始化声明
- 普通构造
- brace initializer

例如：

```c
Code = {
    float4 sample_v4;
    float3 rgb = float3(sample_v4.r, sample_v4.g, sample_v4.b);
    float alpha = 1.0;
    float4 packed0 = float4(rgb, alpha);
    float4 packed1 = {rgb, alpha};
}
```

如果你旧代码里为了绕过限制写了很多 `float4()` 占位初始化，现在通常可以直接改回正常声明。

## 6. 纹理默认值迁移

### 旧模型

- 纹理参数通常没有 DSL 级默认资产引用能力

### 新模型

```c
Properties = {
    Texture2D MainTex = Path(Game, "/Textures/T_Main");
    TextureCube SkyTex = Path("/Engine/EngineResources/DefaultTextureCube");
}
```

这意味着你现在可以直接在 DSL 里指定默认 Unreal 贴图资产。

## 7. 类型别名迁移

### 旧模型

- `Scalar`
- `Color`
- `Vector`

### 新模型

这些旧别名已经移除，请直接改成明确的基础类型：

- `Scalar` -> `float`
- `Color` -> `float4` 或 `vec4`
- `Vector` -> `float2` / `float3` / `float4` 或对应 `vec*`

推荐按真实分量数改，不要继续保留语义化旧名。

## 8. 内置库迁移

现在可以直接从插件目录导入通用函数：

```c
import "Builtin/Texture.dsh";
```

例如把常见采样逻辑改为复用内置库：

```c
Code = {
    float2 uv = UE.TexCoord(Index=0);
    float3 sampled;
    Texture::Sample2DRGB(MainTex, uv, sampled);
    Res = sampled;
}
```

内置库已经移除 `DS_` 前缀，当前使用命名空间形式：

- `DS_SampleTexture2D` -> `Texture::Sample2D`
- `DS_SampleTexture2DRGB` -> `Texture::Sample2DRGB`
- `DS_SampleTexture2DAlpha` -> `Texture::Sample2DAlpha`
- `DS_SampleTextureCube` -> `Texture::SampleCube`
- `DS_SampleTextureCubeRGB` -> `Texture::SampleCubeRGB`
- `DS_SampleTexture2DArray` -> `Texture::Sample2DArray`
- `DS_SampleTexture2DArrayRGB` -> `Texture::Sample2DArrayRGB`
- `DS_SampleTexture2DArrayAlpha` -> `Texture::Sample2DArrayAlpha`

## 9. VSCode 工作流迁移

当前推荐：

- 用 `.dsm` / `.dsh` 文件关联
- 用 `Function` + `import`
- 优先复用 `Builtin/*.dsh`
- 利用扩展的作用域补全、跳转、格式化、本地语法诊断

如果你之前还在按旧 `.dsh = material` 的方式工作，建议优先把文件职责切开，这样编辑器体验会更稳定。
