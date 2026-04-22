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

    Graph = {
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

    Graph = {
        vec2 uv = UE.TexCoord(Index=0);
        float t = UE.Time();
        vec3 pulse;
        Common::BuildPulse(t, uv, pulse);
        Common::ApplyTint(pulse, Tint, Res);
    }
}
```

## 4. 使用外部库采样纹理

1. 先在VSCode中安装 `dreamshaderlang-language-support-[version].vsix`
2. 然后在VSCode中按下`Ctrl` + `Shift` + `P` 打开命令面板
3. 搜索 `Dream Shader Lang: Browse Package Store` 打开
4. 下载任意ShaderPackage

> 如下代码使用了 `dreamshader-texture` 包

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

    Graph = {
        vec2 uv = UE.TexCoord(Index=0);
        vec3 sampled;
        Texture::Sample2DRGB(MainTex, uv, sampled);
        Res = sampled * Tint;
    }
}
```

## 5. `Function` 显式 out 调用

### 定义

#### 普通模式

> 普通模式下 会把编译后的Shader代码 写到`[ProjectDir]/Intermediate/DreamShader/GeneratedShaders` 文件目录下的 `[ShaderName]_[Hash].ush` 文件中 然后在 Material 的 Custom 节点中 include 这个ush 然后调用函数

```c
Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
    result = color * tint;
}
```

#### 自包含模式

> 自包含模式下 会将编译后的Shader代码写到Material中的Custom节点

### 推荐调用

```c
Graph = {
    vec3 src = vec3(1.0, 0.4, 0.2);
    vec3 tint = vec3(0.5, 1.0, 1.0);
    vec3 res;
    ApplyTint(src, tint, res);
}
```
## 7. 变量声明与 brace initializer

```c
Graph = {
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
Graph = {
    float2 uv = UE.TexCoord(Index=0);
    float time = UE.Time();
    float pulse = UE.Expression(
        Class="MaterialExpressionSine",
        OutputType="float1",
        Input=time);

    Res = vec3(pulse, pulse, pulse);
}
```

## 10. `Graph` 中使用 `if` / `else`

```c
Graph = {
    float2 uv = UE.TexCoord(Index=0);
    float mask = UE.Expression(Class="ComponentMask", OutputType="float1", Input=uv, R=true);

    if (mask > 0.5) {
        Res = vec3(1.0, 0.2, 0.2);
    } else {
        Res = vec3(0.0, 0.0, 0.0);
    }
}
```

## 11. `ShaderFunction` 示例

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

    Graph = {
        OutColor = InColor * InTint;
    }
}
```
