# DreamShaderLang Builtin Library

DreamShader 插件目录下自带一套可直接在 DreamShaderLang 中 `import` 的内置头文件。

## 位置

- `Plugins/DreamShader/Library/Builtin/Common.dsh`
- `Plugins/DreamShader/Library/Builtin/Texture.dsh`

## 用法

```c
import "Builtin/Texture.dsh";

Shader(Name="DreamMaterials/M_BuiltinSample")
{
    Properties = {
        Texture2D MainTex = Path(Engine, "/EngineResources/DefaultTexture");
    }

    Outputs = {
        float3 Res;
        Base.EmissiveColor = Res;
    }

    Code = {
        float2 uv = UE.TexCoord(Index=0);
        Texture::Sample2DRGB(MainTex, uv, Res);
    }
}
```

## 当前内置函数

- `Texture::Sample2D`
- `Texture::Sample2DRGB`
- `Texture::Sample2DAlpha`
- `Texture::SampleCube`
- `Texture::SampleCubeRGB`
- `Texture::Sample2DArray`
- `Texture::Sample2DArrayRGB`
- `Texture::Sample2DArrayAlpha`

这些头文件会和项目 `DShader/**/*.dsh` 一起参与 `import` 解析与 VSCode 补全。
