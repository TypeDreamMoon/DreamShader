# DreamShaderLang 示例与模式

## 1. 最小材质示例

```c
Shader(Name="DreamMaterials/M_Minimal")
{
    Properties = {
        vec3 Tint = vec3(1.0, 0.2, 0.2);
    }

    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Res;
        Base.EmissiveColor = Res;
    }

    Code = {
        Res = Tint;
    }
}
```

## 2. `.dsh` 头文件示例

```c
Namespace(Name="Common")
{
    Function BuildPulse(in float t, in vec2 uv, out vec3 result) {
        vec2 p = uv - 0.5;
        float ring = sin(t * 2.0 + length(p) * 12.0) * 0.5 + 0.5;
        result = vec3(ring, ring * 0.5 + 0.1, 1.0 - ring * 0.35);
    }

    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

## 3. `.dsm` 引入 `.dsh`

```c
import "Shared/Common.dsh";

Shader(Name="DreamMaterials/M_Imported")
{
    Properties = {
        vec3 Tint = vec3(1.0, 1.0, 1.0);
    }

    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Res;
        Base.EmissiveColor = Res;
    }

    Code = {
        vec2 uv = UE.TexCoord(Index=0);
        float t = UE.Time();
        vec3 pulse;
        Common::BuildPulse(t, uv, pulse);
        Common::ApplyTint(pulse, Tint, Res);
    }
}
```

## 4. 使用外部库采样纹理

先在VSCode中安装 `dreamshaderlang-language-support-1.1.0.vsix`

```c
import "@typedreammoon/dreamshader-texture/Library/Texture.dsh";

Shader(Name="DreamMaterials/M_TextureBuiltin")
{
    Properties = {
        Texture2D MainTex = Path(Engine, "/EngineResources/DefaultTexture");
        vec3 Tint = vec3(1.0, 1.0, 1.0);
    }

    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Res;
        Base.EmissiveColor = Res;
    }

    Code = {
        vec2 uv = UE.TexCoord(Index=0);
        vec3 sampled;
        Texture::Sample2DRGB(MainTex, uv, sampled);
        Res = sampled * Tint;
    }
}
```

## 5. `Function` 显式 out 调用

### 定义

```c
Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
    result = color * tint;
}
```

### 推荐调用

```c
Code = {
    vec3 src = vec3(1.0, 0.4, 0.2);
    vec3 tint = vec3(0.5, 1.0, 1.0);
    vec3 res;
    ApplyTint(src, tint, res);
}
```

### 不再推荐

```c
Code = {
    Res = ApplyTint(src, tint);
}
```

## 7. 变量声明与 brace initializer

```c
Code = {
    float a;
    float b = 1.0;
    float3 rgb = vec3(1.0, 0.5, 0.2);

    float4 c0 = float4(rgb, b);
    float4 c1 = {rgb, b};
}
```

## 8. 纹理默认值示例

```c
Properties = {
    Texture2D MainTex = Path(Game, "/Textures/T_Main");
    Texture2D DefaultTex = Path("/Engine/EngineResources/DefaultTexture");
    TextureCube SkyTex = Path(Engine, "/EngineResources/DefaultTextureCube");
}
```

## 9. 使用 `UE.*` 构图

```c
Code = {
    float2 uv = UE.TexCoord(Index=0);
    float time = UE.Time();
    float pulse = UE.Expression(
        Class="MaterialExpressionSine",
        OutputType="float1",
        Input=time);

    Res = vec3(pulse, pulse, pulse);
}
```

## 10. `ShaderFunction` 示例

```c
ShaderFunction(Name="Functions/F_Tint")
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
        LibraryCategories = "DreamShader,Color";
    }

    Code = {
        OutColor = InColor * InTint;
    }
}
```

## 11. 当前仓库可直接参考

- `DShader/Sample.dsm`
- `DShader/Shared/common.dsh`
