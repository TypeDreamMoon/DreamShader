---
title: Custom Pass 材质节点
description: UE.DreamPassOutput 写 mesh pass 画出的数据，UE.DreamPassBuffer 让普通材质读导出的缓冲区；以及 pass 材质怎么用 UE.UserSceneTexture 读 pass 的缓冲区。需要 UE 5.8。
---

DreamShaderPass 模块的两个材质表达式，在 `.dss` 里和别的表达式一样用 `UE.<Name>` 调用：一个写 [mesh pass](../language-v2/passes.md#mesh) 画出的数据，
另一个让普通材质读一个[导出的缓冲区](../runtime/index.md#导出的缓冲区)。

| 项目 | 值 |
| :-- | :-- |
| 类 | `UMaterialExpressionDreamPassOutput`、`UMaterialExpressionDreamPassBuffer` |
| 引擎 | 5.8 及以上；更老的引擎上调用它们是一条明确的错误，而不是「未知节点」 |
| 翻译器 | 只支持默认（旧版）材质翻译器：用了其中任何一个的材质不能设 `bEnableNewHLSLGenerator` |
| 起始版本 | （自 2.1.0 起） |

<a id="uedreampassoutput"></a>

## UE.DreamPassOutput

```hlsl
// M_HighlightMask.dss —— 一个 mesh pass 的覆盖材质
#pragma material(ShadingModel = Unlit, bUsedWithSkeletalMesh = true, bUsedWithInstancedStaticMeshes = true)

export void M_HighlightMask(inout material m)
{
    m.EmissiveColor = float3(0, 0, 0);
    UE.DreamPassOutput(Output0 = float4(1, 0, 0, 0));
}
```

它是一条语句，不是一个值：材质的自定义输出，有四个引脚 `Output0`…`Output3`，mesh pass 用 `write Output0 = <buffer>;` 写它们。
每个引脚接受 `float` 到 `float4`，写出 `float4` —— `float` 会被复制到四个分量，`float2`、`float3` 补零。没接的引脚写 0。

- 它也是让材质**能够**被 mesh pass 绘制的东西：这个节点给材质打一个 shader 标签，mesh pass 的 shader 只为带标签的材质编译 —— 所以工程里其他材质不为 Custom Pass 付出任何代价。
- **物体自己的材质也可以带它。** `Mode = Own` 的 mesh pass 用每个选中物体自己的材质来画，读那个材质的 `UE.DreamPassOutput` —— 不用覆盖材质就能拿到逐物体数据（湿度、id、遮罩）。
- **只能在顶层。** 引擎只从材质自己的图编译自定义输出，不从材质函数里编译。放在函数里的这个节点什么都不做 —— 没有标签、没有输出 —— 所以 DreamShader 会警告，材质编辑器在函数里也不提供它。
- **每个材质一个。**
- 开了 Substrate 也一样工作：它的输入是普通值，不是 BSDF。

<a id="uedreampassbuffer"></a>

## UE.DreamPassBuffer

```hlsl
// M_Ground.dss —— 高亮物体下面的一圈光
export void M_Ground(inout material m)
{
    float Glow = UE.DreamPassBuffer(Pipeline = "CP_Highlight", Buffer = "Blurred").r;
    m.EmissiveColor = Glow * float3(1.0, 0.6, 0.0);
}
```

采样一个导出缓冲区的渲染目标，返回 `float4`。

| 参数 | 说明 |
| :-- | :-- |
| `Pipeline` | 管线：裸名字（同一源码根下某个 `.dsp` 的产物）或对象路径 |
| `Buffer` | 它的一个 `Export = true` 的缓冲区 |
| `Coordinates` | 可选；不写时是视口 UV，屏幕空间缓冲区要的就是它。按别的方式布局的缓冲区（示例里在世界 XY 上平铺的风场）传自己的 UV |
| `SamplerSource` | 可选；默认 `FromTextureAsset` —— 渲染目标自己的采样器，编译器给随视图定尺寸的缓冲区设成 Clamp、给固定尺寸的设成 Wrap。`Wrap_WorldGroupSettings` 或 `Clamp_WorldGroupSettings` 改用共享采样器 |

- 在顶点着色器里（World Position Offset）采样 mip 0。
- 管线不存在、管线没有这个缓冲区或没导出它、内置缓冲区、整数或深度缓冲区，都是错误 —— `.dss` 编译时在 DreamShader 里报，在材质编辑器里也报。
- 读到的是本帧还是上一帧的画面，取决于缓冲区最后一个写者何时运行；见[导出的缓冲区](../runtime/index.md#导出的缓冲区)里的表。
- 材质依赖 `.dsp`：改了缓冲区的导出会让它重编。
- 材质编辑器的预览显示渲染目标最后装的内容。

<a id="pass-材质读-pass-的缓冲区"></a>

## pass 材质读 pass 的缓冲区

由 `fullscreen` pass 运行的材质用引擎自己的 `UE.UserSceneTexture(UserSceneTexture = "<name>", Coordinates = UV)` 节点读 pass 的缓冲区，
名字由 pass 的 `read <name> = <buffer>;` 绑定 —— 不涉及任何 Custom Pass 节点，材质预览起来和别的后处理材质一样。

<a id="接下来"></a>

## 接下来

- [Custom Pass 管线](../language-v2/passes.md) — .dsp 的写法：缓冲区、pass、注入点、参数。
  - [Custom Pass 运行时](../runtime/index.md) — 激活、内置与导出的缓冲区、视图、调试、限制。
  - [通用 UE.Expression](ue-expression.md) — 反射表达式是怎么调用的。
