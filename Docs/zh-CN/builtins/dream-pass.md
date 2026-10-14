---
title: "Custom Pass 材质节点"
description: "Custom Pass 材质节点"
---

<a id="custom-pass-material-nodes"></a>

# Custom Pass 材质节点

> [DreamShader](../index.md) » [内置节点](index.md) » **Custom Pass 材质节点**

DreamShaderPass 模块的两个材质表达式，可以像其他表达式一样从 `.dss` 访问，
通过 `UE.<Name>`：一个写入 [mesh pass](../language-v2/passes.md#mesh) 绘制的数据，另一个让
普通材质读取[导出的缓冲区](../../runtime/index.md#exported-buffers)。

| | |
| :-- | :-- |
| 类 | `UMaterialExpressionDreamPassOutput`、`UMaterialExpressionDreamPassBuffer` |
| 引擎 | 5.8 及以后；在更早的引擎上调用会报明确的错误，而不是当作未知节点 |
| 翻译器 | 仅支持默认（旧版）材质翻译器：使用其中任意一个的材质无法设置 `bEnableNewHLSLGenerator` |

<a id="uedreampassoutput"></a>

## `UE.DreamPassOutput`

```hlsl
// M_HighlightMask.dss -- the override material of a mesh pass
#pragma material(ShadingModel = Unlit, bUsedWithSkeletalMesh = true, bUsedWithInstancedStaticMeshes = true)

export void M_HighlightMask(inout material m)
{
    m.EmissiveColor = float3(0, 0, 0);
    UE.DreamPassOutput(Output0 = float4(1, 0, 0, 0));
}
```

它是一条语句，而不是一个值：材质的自定义输出，拥有四个引脚 `Output0`…`Output3`，mesh pass
以 `write Output0 = <buffer>;` 写入它们。每个引脚接受 `float` 到 `float4` 并写入 `float4` —— `float` 会被
复制，`float2` 或 `float3` 用零填充。未连接的引脚写入 0。

- 它也是让材质**能够**被 mesh pass 绘制的东西：该节点给材质一个着色器
  标签，而 mesh pass 的着色器只为带有标签的材质编译 —— 因此项目中的其他材质
  不会为 Custom Pass 付出任何代价。
- **对象自身的材质也可以携带它。** 使用 `Mode = Own` 的 mesh pass 会以每个选中对象
  自身的材质来绘制，并读取该材质的 `UE.DreamPassOutput` —— 无需覆盖即可获得逐对象数据（湿度、id、
  遮罩）。
- **仅限顶层。** 引擎从材质自身的图编译自定义输出，而不是从材质
  函数中编译。在函数内部该节点不起作用 —— 没有标签，没有输出 —— 因此 DreamShader 会发出警告，
  材质编辑器也不会在那里提供它。
- **每个材质一个。**
- 在开启 Substrate 时工作方式相同：其输入是普通值，而不是 BSDF。

<a id="uedreampassbuffer"></a>

## `UE.DreamPassBuffer`

```hlsl
// M_Ground.dss -- a glow under the highlighted objects
export void M_Ground(inout material m)
{
    float Glow = UE.DreamPassBuffer(Pipeline = "CP_Highlight", Buffer = "Blurred").r;
    m.EmissiveColor = Glow * float3(1.0, 0.6, 0.0);
}
```

对导出缓冲区的渲染目标进行采样。`float4`。

| 参数 | |
| :-- | :-- |
| `Pipeline` | 管线：一个裸名称（同一源码根下 `.dsp` 的产物）或对象路径 |
| `Buffer` | 它的一个带有 `Export = true` 的缓冲区 |
| `Coordinates` | 可选；省略时为视口 UV，这正是屏幕空间缓冲区所需要的。以其他方式布局的缓冲区（示例中的风场，在世界 XY 上平铺）会传入自己的 UV |
| `SamplerSource` | 可选；默认为 `FromTextureAsset` —— 渲染目标自身的采样器，编译器会为按视图确定尺寸的缓冲区将其设置为 Clamp，为固定尺寸的缓冲区设置为 Wrap。`Wrap_WorldGroupSettings` 或 `Clamp_WorldGroupSettings` 则改用共享采样器 |

- 在顶点着色器（World Position Offset）中采样 mip 0。
- 缺失的管线、管线没有或未导出的缓冲区、内置缓冲区，以及
  整数或深度缓冲区都是错误 —— 在 `.dss` 编译时由 DreamShader 报出，材质编辑器中也会报出。
- 它读取的内容取决于缓冲区最后一个写入者何时运行 —— 是这一帧的画面还是上一帧的；请参阅
  [导出的缓冲区](../../runtime/index.md#exported-buffers)中的表格。
- 该材质依赖 `.dsp`：更改缓冲区的导出会使其重新编译。
- 材质编辑器的预览会显示渲染目标的最后内容。

<a id="reading-a-passs-buffers-in-a-pass-material"></a>

## 在 pass 材质中读取 pass 的缓冲区

由 `fullscreen` pass 运行的材质使用引擎自带的
`UE.UserSceneTexture(UserSceneTexture = "<name>", Coordinates = UV)` 节点读取该 pass 的缓冲区，其名称由 pass 通过
`read <name> = <buffer>;` 绑定 —— 不涉及任何 Custom Pass 节点，并且该材质会像任何后处理
材质一样进行预览。

<a id="see-also"></a>

## 另请参阅

- [Custom Pass 管线 —— `.dsp`](../language-v2/passes.md)
- [Custom Pass 运行时](../runtime/index.md)
- [UE.Expression](ue-expression.md) —— 反射表达式是如何被调用的
