---
title: "DreamShader 诊断"
description: "DreamShader 诊断"
---

<a id="dreamshader-diagnostics"></a>

# DreamShader 诊断

每个 `DSHnnnn` DreamShader 可以发出的诊断。首位数字是抛出该诊断的阶段：

- **DSH1xxx** --- 驱动、源文件和导入 --- [DSH1xxx.md](DSH1xxx.md)
- **DSH2xxx** --- 词法分析器和语法 --- [DSH2xxx.md](DSH2xxx.md)
- **DSH3xxx** --- 片段和声明 --- [DSH3xxx.md](DSH3xxx.md)
- **DSH4xxx** --- 图语句和表达式 --- [DSH4xxx.md](DSH4xxx.md)
- **DSH5xxx** --- 内置项 -- UE.*、数学、Substrate --- [DSH5xxx.md](DSH5xxx.md)
- **DSH6xxx** --- 函数和 HLSL 代码生成 --- [DSH6xxx.md](DSH6xxx.md)
- **DSH7xxx** --- 属性、参数和设置 --- [DSH7xxx.md](DSH7xxx.md)
- **DSH8xxx** --- 资产生成和保存 --- [DSH8xxx.md](DSH8xxx.md)
- **DSH9xxx** --- 工具、同步和内部不变量 --- [DSH9xxx.md](DSH9xxx.md)

由 `.skill/gen-diagnostics.ps1` 生成；在添加代码后运行它。

[index.md](index.md) 说明诊断出现在哪里以及如何阅读一条诊断，将 1.x 生成器的常见消息映射到取代它们的代码，并列出少数不带代码的消息。

| 代码 | 严重性 | 消息 |
| --- | --- | --- |
| [DSH1030](DSH1xxx.md#dsh1030) | error | {0}({1})：此 '#if' 从未关闭；文件结束时仍有 {2} 个条件块处于打开状态。 |
| [DSH1031](DSH1xxx.md#dsh1031) | error | {0}({1})：'#endif' 没有匹配的 '#if'。 |
| [DSH1032](DSH1xxx.md#dsh1032) | error | {0}({1})：'#{2}' 没有匹配的 '#if'。 |
| [DSH1033](DSH1xxx.md#dsh1033) | error | {0}({1})：'#{2}' 出现在第 {3} 行的 '#else' 之后，而该 '#else' 已关闭此链。 |
| [DSH1034](DSH1xxx.md#dsh1034) | error | {0}({1})：无效的 '{2}' 条件：{3} |
| [DSH1035](DSH1xxx.md#dsh1035) | error | {0}({1})：未知的预处理器指令 '#{2}'。{3} |
| [DSH1036](DSH1xxx.md#dsh1036) | error | {0}({1})：'#{2}' 需要一个定义名称。 |
| [DSH1037](DSH1xxx.md#dsh1037) | error | {0}({1})：'#{2}' 嵌套深度超过了 {3} 的限制。 |
| [DSH1038](DSH1xxx.md#dsh1038) | error | {0}({1})：'#{2}' 需要一个由字母、数字和下划线组成且不以数字开头的名称；得到 '{... |
| [DSH1039](DSH1xxx.md#dsh1039) | error | {0}({1})：'{3}' 是只读内置常量，因此 '#{2}' 无法更改它。'DS_' 前缀为保留... |
| [DSH1040](DSH1xxx.md#dsh1040) | error | {0}({1})：'{2}' 条件中的类型不匹配：{3} |
| [DSH1041](DSH1xxx.md#dsh1041) | error | {0}({1})：此 '{3}' 条件中 '{2}' 的右操作数为零。 |
| [DSH1042](DSH1xxx.md#dsh1042) | error | {0}({1})：'{2}' 在 '{3}' 之前已经完整。指令之后除 '//' 注释外不能有任何内容。 |
| [DSH1043](DSH1xxx.md#dsh1043) | error | 资产引用写为 '{0}'，这不是纹理类；纹理默认值需要 {1}。 |
| [DSH1044](DSH1xxx.md#dsh1044) | error | 资产引用写为 '{0}'，但此属性声明为 {1}。 |
| [DSH1045](DSH1xxx.md#dsh1045) | error | 资产引用写为 '%s'，但此槽位需要 '%s'。 |
| [DSH2101](DSH2xxx.md#dsh2101) | error | 源代码中出现意外字符 '{0}'。 |
| [DSH2102](DSH2xxx.md#dsh2102) | error | 块注释未终止；应为闭合的 '*/'。 |
| [DSH2103](DSH2xxx.md#dsh2103) | error | 字符串字面量未终止；应为闭合的 '"'。 |
| [DSH2104](DSH2xxx.md#dsh2104) | error | 字符串字面量中的未知转义序列 '\{0}'。 |
| [DSH2105](DSH2xxx.md#dsh2105) | error | 格式错误的数字字面量 '{0}'。 |
| [DSH2106](DSH2xxx.md#dsh2106) | error | '#' 指令必须是其行上的第一项。 |
| [DSH2150](DSH2xxx.md#dsh2150) | error | 解析 {0} 时文件意外结束。 |
| [DSH2151](DSH2xxx.md#dsh2151) | error | 应为表达式，却找到 {0}。 |
| [DSH2152](DSH2xxx.md#dsh2152) | error | 应为用于闭合类型转换的 ')'，却找到 {1}。 |
| [DSH2153](DSH2xxx.md#dsh2153) | error | 应为用于闭合索引的 ']'，却找到 {1}。 |
| [DSH2154](DSH2xxx.md#dsh2154) | error | 应为 'for' 初始化器之后的 ';'，却找到 {1}。 |
| [DSH2155](DSH2xxx.md#dsh2155) | error | 应为用于闭合初始化器列表的 '}'，却找到 {1}。 |
| [DSH2157](DSH2xxx.md#dsh2157) | error | 应为 'if' 之后的 '('，却找到 {1}。 |
| [DSH2158](DSH2xxx.md#dsh2158) | error | 位置参数不能跟在命名参数之后；请也给此参数命名。 |
| [DSH2159](DSH2xxx.md#dsh2159) | error | 应为用于补全条件运算符的 ':'，却找到 {0}。 |
| [DSH2160](DSH2xxx.md#dsh2160) | error | 不支持的语句：预处理器行 '#{0}' 不能出现在函数体内部；请将该函数标记为... |
| [DSH2161](DSH2xxx.md#dsh2161) | error | 应为 '.' 之后的成员或组件重排名称，却找到 {0}。 |
| [DSH2162](DSH2xxx.md#dsh2162) | error | 初始化器列表只能用作变量初始化器，不能用作一般表达式。 |
| [DSH2163](DSH2xxx.md#dsh2163) | error | 应为变量名，却找到 {1}。 |
| [DSH2164](DSH2xxx.md#dsh2164) | error | 应为 'do' 语句主体之后的 'while'，却找到 {0}。 |
| [DSH2165](DSH2xxx.md#dsh2165) | error | 应为用于打开块的 '{'，却找到 {1}。 |
| [DSH2200](DSH2xxx.md#dsh2200) | error | （运行时构建） |
| [DSH2201](DSH2xxx.md#dsh2201) | error | （运行时构建） |
| [DSH2202](DSH2xxx.md#dsh2202) | error | 1.x 图表达式中不应出现 '[ ]'，却找到 '{0}'，而 1.x 会静默丢弃索引并且所有... |
| [DSH2203](DSH2xxx.md#dsh2203) | error | 1.x 图表达式中应使用诸如 'float3(x)' 的构造函数，却找到类型转换 '({0})'，而 1.x 从不... |
| [DSH2204](DSH2xxx.md#dsh2204) | error | 赋值只应作为整个 1.x 图语句，却在表达式中找到一处；请将此代码移动... |
| [DSH2205](DSH2xxx.md#dsh2205) | error | 1.x 图赋值中应为 '='，却找到 '{0}'，而 1.x 将其读取为名为... 的变量声明 |
| [DSH2206](DSH2xxx.md#dsh2206) | error | 1.x 图赋值左侧应为变量或 'variable.member'，却找到 '{0}'。 |
| [DSH2207](DSH2xxx.md#dsh2207) | error | 1.x 图表达式中不应出现 '++' 或 '--'，却找到 '{0}'；请将此代码移动到 .dss 文件。 |
| [DSH2208](DSH2xxx.md#dsh2208) | error | 在 1.x Graph 主体中应为声明、赋值、调用或 'if'，但发现 '{0}'，而 1.x 不会 h... |
| [DSH2209](DSH2xxx.md#dsh2209) | error | 1.x Graph 'if' 的主体应使用大括号括起，但发现单个语句。 |
| [DSH2210](DSH2xxx.md#dsh2210) | error | 1.x Graph 'if' 条件中最多应有一个比较，但又发现了 '{0}'，而 1.x 会静默丢弃... |
| [DSH2211](DSH2xxx.md#dsh2211) | error | 1.x Graph 语句应为调用或赋值，但发现其值从未被使用的表达式。 |
| [DSH2212](DSH2xxx.md#dsh2212) | error | 1.x Graph 变量上不应有存储关键字，但发现 '{0}'；请将此代码移到 .dss 文件中。 |
| [DSH2213](DSH2xxx.md#dsh2213) | error | 1.x Graph 声明中应为单个值，但发现数组声明符 '{0}'；请将此代码移到 .... |
| [DSH2214](DSH2xxx.md#dsh2214) | error | 1.x Graph 初始化器应为表达式，但发现初始化列表；请使用构造函数，例如 'fl... |
| [DSH2215](DSH2xxx.md#dsh2215) | error | 类型为 '{1}' 的 1.x Graph 变量 '{0}' 应有初始化器，但未找到，且 1.x 没有零值... |
| [DSH2216](DSH2xxx.md#dsh2216) | error | '#Region' 后应为名称，但未找到。 |
| [DSH2217](DSH2xxx.md#dsh2217) | error | 此 '#EndRegion' 之前应有 '#Region'，但未找到打开的 '#Region'。 |
| [DSH2218](DSH2xxx.md#dsh2218) | error | Graph 主体结束前应用 '#EndRegion' 关闭区域 '{0}'，但未找到。 |
| [DSH2219](DSH2xxx.md#dsh2219) | error | 1.x Graph 主体中唯一的 '#' 行应为 '#Region' 或 '#EndRegion'，但发现 '#{0}'。 |
| [DSH2220](DSH2xxx.md#dsh2220) | error | 1.x Graph 主体中不应有裸块，但发现一个；1.x 仅在 'if' 和 'else' 之后才有块。 |
| [DSH2222](DSH2xxx.md#dsh2222) | error | 1.x Graph 表达式中应为十进制数，但发现十六进制字面量 '{0}'，而 1.x 将其读取为... |
| [DSH2240](DSH2xxx.md#dsh2240) | error | 顶层应为 Shader、ShaderFunction、ShaderLayer、ShaderLayerBlend、Function、GraphFunction、Namesp... |
| [DSH2241](DSH2xxx.md#dsh2241) | error | 应为 '(' 及块的属性，例如 '(Name = "M_Example")'，但发现 {0}。 |
| [DSH2242](DSH2xxx.md#dsh2242) | error | '{0}' 上应有 'Name = "..."' 属性，但未找到。 |
| [DSH2243](DSH2xxx.md#dsh2243) | error | 应为 'Name' 之类的属性名，但发现 {0}。 |
| [DSH2244](DSH2xxx.md#dsh2244) | warning | 属性 '{0}' 被写了两次；后者生效，与 1.x 中一样。 |
| [DSH2245](DSH2xxx.md#dsh2245) | error | 应为 Shader 节（Properties、Settings、Outputs、Graph 或 Layout），但发现 '{0}'。 |
| [DSH2246](DSH2xxx.md#dsh2246) | error | '{0}' 的主体节应为 'Graph'，但发现 'Code'，而 1.x 仅在 Function 内接受 'Code'。 |
| [DSH2247](DSH2xxx.md#dsh2247) | error | VirtualFunction '{0}' 声明现有资源，不应有主体，但发现节 '{1}'。 |
| [DSH2248](DSH2xxx.md#dsh2248) | error | '.{0}' 文件中应为 1.x 块，但发现 {1}，这是 2.0 语法；2.0 声明应放在 .dss 文件... |
| [DSH2249](DSH2xxx.md#dsh2249) | error | '.dsh' 头文件中应只有 Function、GraphFunction、Namespace 和 VirtualFunction 块，但发现 a... |
| [DSH2250](DSH2xxx.md#dsh2250) | error | 文件中应有一个 Shader 块，但发现第二个。 |
| [DSH2251](DSH2xxx.md#dsh2251) | warning | '{0}' 是 '{1}' 的旧拼写；读取方式仍然相同。 |
| [DSH2252](DSH2xxx.md#dsh2252) | error | 'import' 后应为双引号路径，但发现 {0}。 |
| [DSH2253](DSH2xxx.md#dsh2253) | warning | '{0}' 被读取为 'import'；请以小写形式书写。 |
| [DSH2254](DSH2xxx.md#dsh2254) | error | 顶层应为 Shader、ShaderFunction、ShaderLayer、ShaderLayerBlend、Function、GraphFunction、Namesp... |
| [DSH2255](DSH2xxx.md#dsh2255) | error | Shader '{0}' 中应有 Graph 节，但未找到。 |
| [DSH2256](DSH2xxx.md#dsh2256) | warning | Shader '{0}' 没有 Outputs 节，因此其 Graph 计算的内容无法到达材质。 |
| [DSH2257](DSH2xxx.md#dsh2257) | error | 应以 '`{' 打开 '{0}' 块，但发现 {1}。 |
| [DSH2258](DSH2xxx.md#dsh2258) | warning | 节 '{0}' 被写了两次；后者生效，与 1.x 中一样。 |
| [DSH2259](DSH2xxx.md#dsh2259) | error | '.dsf' 文件中应为 ShaderFunction、ShaderLayer 和 ShaderLayerBlend 块，但发现 Shader 块，wh... |
| [DSH2260](DSH2xxx.md#dsh2260) | warning | '{0}' 不是 NoGeneration 的值，该值应为 true 或 false；该块将按 false 来构建。 |
| [DSH2300](DSH2xxx.md#dsh2300) | error | 应以 '`{' 打开通道 '{0}' 的块，但发现 {1}。 |
| [DSH2301](DSH2xxx.md#dsh2301) | error | 通道块中应为设置（'Key = Value;'）、'read'、'write' 或 'param' 行或 'hlsl' 块，... |
| [DSH2302](DSH2xxx.md#dsh2302) | error | 键 '{0}' 后应为 '='，但发现 {1}。 |
| [DSH2303](DSH2xxx.md#dsh2303) | error | 通道语句末尾应为 ';'，但发现 {1}。 |
| [DSH2304](DSH2xxx.md#dsh2304) | error | '{0}' 后应为缓冲区，但发现 {1}。 |
| [DSH2305](DSH2xxx.md#dsh2305) | error | '{0}.{1}'：缓冲区在 '.' 后唯一可用的内容是 'Previous'，即 'History = true' 的上一帧内容... |
| [DSH2306](DSH2xxx.md#dsh2306) | error | 'param' 后应为参数名，但发现 {1}。 |
| [DSH2307](DSH2xxx.md#dsh2307) | error | 'param {0}' 后应为 '='，但发现 {1}。 |
| [DSH2308](DSH2xxx.md#dsh2308) | error | 缓冲区 '{0}' 的参数中应为 'Scale' 之类的键，但发现 {1}。 |
| [DSH2309](DSH2xxx.md#dsh2309) | error | '{0}' 后应为 '='，但发现 {1}。 |
| [DSH2310](DSH2xxx.md#dsh2310) | error | 缓冲区 '{0}' 的参数中应为 ',' 或 ')'，但发现 {1}。 |
| [DSH2311](DSH2xxx.md#dsh2311) | error | 缓冲区 '{0}' 的声明后应为 ';'，但发现 {1}。 |
| [DSH2312](DSH2xxx.md#dsh2312) | error | 行 '#{0}' 不能出现在通道块内；通道包含设置、'read'、'write' 和 'param' 行... |
| [DSH2313](DSH2xxx.md#dsh2313) | error | 'hlsl' 后应为 '{' 以打开 HLSL 块，但发现 {1}。 |
| [DSH2314](DSH2xxx.md#dsh2314) | error | 应在下一个声明之前用 '`}' 关闭通道 '{0}'，但发现 {1}。 |
| [DSH3200](DSH3xxx.md#dsh3200) | error | 文件作用域中出现意外的 {0}；应为声明、'#pragma'、'#include' 或 'import'。 |
| [DSH3201](DSH3xxx.md#dsh3201) | error | 预处理器指令 '#{0}' 到达了解析器；此处只允许 '#pragma' 和 '#include'，而 '#if' / '#... |
| [DSH3202](DSH3xxx.md#dsh3202) | error | '#pragma' 需要一个名称：material、instance、pipeline、layout、region 或 endregion。 |
| [DSH3203](DSH3xxx.md#dsh3203) | error | '#include' 需要一个带引号的路径：#include "/Game/Shared/Common.dsh"。 |
| [DSH3204](DSH3xxx.md#dsh3204) | error | 应为类型名，却找到了 {0}。 |
| [DSH3205](DSH3xxx.md#dsh3205) | error | 'struct' 之后应为名称，却找到了 {1}。 |
| [DSH3206](DSH3xxx.md#dsh3206) | error | '{0}' 的参数列表之后应为 '`{' 或 ';'，却找到了 {1}。 |
| [DSH3207](DSH3xxx.md#dsh3207) | error | '{0}' 是 'extern' 并绑定到现有资产，因此不能有函数体；请编写以 ';' 结尾的原型。 |
| [DSH3208](DSH3xxx.md#dsh3208) | error | '{0}' 没有函数体。只有 'extern' 原型可以以 ';' 结尾；你定义的函数需要 '`{...`}'。 |
| [DSH3210](DSH3xxx.md#dsh3210) | error | '.dsh' 头文件不能导出 '{0}'；只有 '.dss' 文件才会生成资产。 |
| [DSH3211](DSH3xxx.md#dsh3211) | error | 应为表达式结尾，却找到了 {0}。 |
| [DSH3213](DSH3xxx.md#dsh3213) | error | '{0}' 不能与它前面的关键字组合；声明为 'uniform'、'static const'、'static'、... |
| [DSH3214](DSH3xxx.md#dsh3214) | error | 参数 '{0}' 是 'out'，不能有默认值；只有输入才是可选的。 |
| [DSH3215](DSH3xxx.md#dsh3215) | error | 应为 ']' 来闭合数组维度，却找到了 {1}。 |
| [DSH3216](DSH3xxx.md#dsh3216) | error | 导入路径之后应为 ';'，却找到了 {1}。 |
| [DSH3217](DSH3xxx.md#dsh3217) | error | 结构体名称之后应为 '{'，却找到了 {1}。 |
| [DSH3218](DSH3xxx.md#dsh3218) | error | 应为 ')' 来闭合参数列表，却找到了 {1}。 |
| [DSH3219](DSH3xxx.md#dsh3219) | warning | '#pragma nogeneration' 不接受参数，'{0}' 已被忽略：该文件仍然被标记。请移除该行... |
| [DSH3220](DSH3xxx.md#dsh3220) | warning | '///' 行中的 '@' 后面必须跟一个指令名；该文本将作为描述保留。 |
| [DSH3221](DSH3xxx.md#dsh3221) | warning | 此 '///' 块之后没有字段，已被忽略。 |
| [DSH3222](DSH3xxx.md#dsh3222) | error | 应为 2.0 声明，却找到了 1.x 声明 '{0}'；1.x 声明属于 .dsh 头文件或... |
| [DSH3250](DSH3xxx.md#dsh3250) | error | 应为属性类型和名称，却找到了 {0}。 |
| [DSH3251](DSH3xxx.md#dsh3251) | error | 'const' 之后应为属性类型，却找到了 {0}。 |
| [DSH3252](DSH3xxx.md#dsh3252) | error | 应为属性类型，例如 'float'、'float4'、'Texture2D' 或 'ScalarParameter'，却找到了 '{0}'。 |
| [DSH3253](DSH3xxx.md#dsh3253) | error | 应为带有 2.0 拼写的属性类型，却找到了没有 2.0 拼写的 '{0}{1}'；请将此材质移动到 .dss ... |
| [DSH3254](DSH3xxx.md#dsh3254) | error | '{0} =' 之后应为默认值，却找到了 {1}。 |
| [DSH3255](DSH3xxx.md#dsh3255) | error | 应为 ']' 来闭合元数据块，却找到了 {0}。 |
| [DSH3256](DSH3xxx.md#dsh3256) | error | 元数据键 '{0}' 应只出现一次，却又找到了它。 |
| [DSH3257](DSH3xxx.md#dsh3257) | error | 应为带两个数字的 'Slider(min, max)'，却找到了 '{0}'。 |
| [DSH3258](DSH3xxx.md#dsh3258) | error | 应为整数形式的 'SortPriority'，却找到了 '{0}'。 |
| [DSH3259](DSH3xxx.md#dsh3259) | error | 应为 ')' 来闭合 'UE.{0}' 的参数，却找到了 {1}。 |
| [DSH3260](DSH3xxx.md#dsh3260) | error | 'Group("...")' 内应为名称，却找到了空字符串。 |
| [DSH3261](DSH3xxx.md#dsh3261) | error | 应为设置名称，例如 'BlendMode'，却找到了 {0}。 |
| [DSH3262](DSH3xxx.md#dsh3262) | warning | 设置 '{0}' 被写了两次；后写的值生效，与 1.x 中一样。 |
| [DSH3263](DSH3xxx.md#dsh3263) | warning | '{0}' 不是 '{1}' 的某个设置；1.x 忽略了它，此前端也如此。 |
| [DSH3264](DSH3xxx.md#dsh3264) | warning | 'UserExposedCaption' 没有 2.0 拼写，因此不会应用；其值会保留以供迁移。 |
| [DSH3265](DSH3xxx.md#dsh3265) | warning | 'ExposeToLibrary' 应为 'true' 或 'false'，却找到了 '{0}'；1.x 忽略了该设置，此前端也如... |
| [DSH3266](DSH3xxx.md#dsh3266) | error | 输出声明 '{0}' 之后应为 ';'，却找到了 {1}。 |
| [DSH3267](DSH3xxx.md#dsh3267) | error | 对于 Expression(...) 输出目标，应为 'Pin[<index>] = <source>'，却找到了 {0}。 |
| [DSH3268](DSH3xxx.md#dsh3268) | error | Expression(Class = "{0}") 的每个引脚都应只绑定一次，却发现 Pin[{1}] 又被绑定了。 |
| [DSH3269](DSH3xxx.md#dsh3269) | error | Expression(...) 块中应至少有一个 'Pin[<index>] = <source>;'，却一个也没有。 |
| [DSH3270](DSH3xxx.md#dsh3270) | error | 'Pin[{0}] =' 之后应为源，却没有。 |
| [DSH3271](DSH3xxx.md#dsh3271) | error | 应为参数类型和名称，例如 'float Amount'，却找到了 {0}。 |
| [DSH3272](DSH3xxx.md#dsh3272) | warning | 输出 '{0}' 上的 'opt' 没有意义；1.x 忽略了它，此前端也如此。 |
| [DSH3273](DSH3xxx.md#dsh3273) | warning | 输出 '{0}' 上的默认值没有意义；1.x 忽略了它，因此它被丢弃。 |
| [DSH3274](DSH3xxx.md#dsh3274) | error | Layout 部分中应为 'Node(...)' 或 'Comment(...)'，却找到了 {0}。 |
| [DSH3275](DSH3xxx.md#dsh3275) | error | '{1}(...)' 中应为参数 '{0}'，却没有。 |
| [DSH3276](DSH3xxx.md#dsh3276) | error | 'Color' 应为向量字面量，例如 '(0.1, 0.16, 0.22, 0.35)'，却找到了 '{0}'。 |
| [DSH3277](DSH3xxx.md#dsh3277) | warning | 层输入 '{0}' 会成为以输出 '{1}' 命名的 'inout material' 参数，因此输入引脚... |
| [DSH3278](DSH3xxx.md#dsh3278) | error | '{0}' 上应为 MaterialAttributes 输出，却没有。 |
| [DSH3300](DSH3xxx.md#dsh3300) | error | 'buffer' 之后应为缓冲区的名称，却找到了 {1}。 |
| [DSH3301](DSH3xxx.md#dsh3301) | error | 'buffer {0}' 之后应为 ':' 和格式，却找到了 {1}。 |
| [DSH3302](DSH3xxx.md#dsh3302) | error | 应为缓冲区格式，例如 'R8' 或 'RGBA16F'，却找到了 {1}。 |
| [DSH3303](DSH3xxx.md#dsh3303) | error | 'pass' 之后应为通道的名称，却找到了 {1}。 |
| [DSH3304](DSH3xxx.md#dsh3304) | error | 'pass {0}' 之后应为 ':' 和通道类型，却找到了 {1}。 |
| [DSH3305](DSH3xxx.md#dsh3305) | error | 需要指定通道种类：fullscreen、compute、mesh、clear 或 copy，但发现 {1}。 |
| [DSH3310](DSH3xxx.md#dsh3310) | error | 'buffer {0}' 声明了 Custom Pass 管线的 buffer，而只有 '.dsp' 文件才持有此内容；请将其移到... |
| [DSH3311](DSH3xxx.md#dsh3311) | error | '#pragma pipeline' 用于配置 Custom Pass 管线，应放在其专属的 '.dsp' 文件中，而此行... |
| [DSH3312](DSH3xxx.md#dsh3312) | error | 文件作用域的 'hlsl' 块是 Custom Pass 管线的 HLSL，只有 '.dsp' 文件才持有此内容，而 '{... |
| [DSH3313](DSH3xxx.md#dsh3313) | error | '#pragma pipeline' 被写了第二次，而一个 '.dsp' 对应一条管线；第 {0} 行已配置... |
| [DSH3314](DSH3xxx.md#dsh3314) | error | '#pragma material' 用于配置材质，而 '.dsp' 由 '#pragma pipeline(...)' 配置；该材质... |
| [DSH3315](DSH3xxx.md#dsh3315) | warning | '#pragma {0}' 用于将图形节点装箱或放置，而 '.dsp' 没有图形；该行已被忽略。 |
| [DSH3316](DSH3xxx.md#dsh3316) | error | '{0}' 是 '.dsp' 的文件作用域变量，它应是 'uniform'（一种激活可覆盖的参数）或... |
| [DSH3317](DSH3xxx.md#dsh3317) | warning | '@{0}' 在 '.dsp' 中对 {1} 没有效果；请将其移除。 |
| [DSH4200](DSH4xxx.md#dsh4200) | error | '{0}' 未声明；您指的是 '{1}' 吗？名称区分大小写。 |
| [DSH4201](DSH4xxx.md#dsh4201) | error | '{0}' 不是类型；您指的是 '{1}' 吗？类型名称区分大小写。 |
| [DSH4202](DSH4xxx.md#dsh4202) | error | 字符串在表达式中没有值；它只能是反射属性的值，例如 'UE.Expres... |
| [DSH4203](DSH4xxx.md#dsh4203) | error | '{0}' 是命名空间，不是值；请写成 '{0}.SomeNode(...)'。 |
| [DSH4204](DSH4xxx.md#dsh4204) | error | '{0}' 是类型，不是值；请写成 '{0}(...)' 来构造一个。 |
| [DSH4205](DSH4xxx.md#dsh4205) | error | '{0}' 没有名为 '{1}' 的字段。 |
| [DSH4206](DSH4xxx.md#dsh4206) | error | 纹理上的 '{0}' 是一次调用：请写成 'Tex.{0}(UV)'。 |
| [DSH4207](DSH4xxx.md#dsh4207) | error | {0} 类型的值没有成员 '{1}'。 |
| [DSH4208](DSH4xxx.md#dsh4208) | error | {0} 类型的值没有方法 '{1}'。 |
| [DSH4210](DSH4xxx.md#dsh4210) | error | '{0}' 已在此文件中声明；一个名称只声明一个事物。 |
| [DSH4211](DSH4xxx.md#dsh4211) | error | 从 '{1}' 包含 '{0}' 形成了循环；头文件不能包含自身，无论是直接包含还是通过另一个头文件... |
| [DSH4212](DSH4xxx.md#dsh4212) | error | '{0}' 无法读取：此前端启动时没有包含解析器，因此头文件声明的任何内容... |
| [DSH4213](DSH4xxx.md#dsh4213) | error | '{0}' 在结构体 '{1}' 中声明了两次。 |
| [DSH4214](DSH4xxx.md#dsh4214) | error | '{0}' 在 '{1}' 的参数列表中声明了两次。 |
| [DSH4215](DSH4xxx.md#dsh4215) | error | '{1}' 的 '{0}' 参数被提供了两次。 |
| [DSH4216](DSH4xxx.md#dsh4216) | error | '{0}' 按顺序接收其参数，没有名为 '{1}' 的参数。 |
| [DSH4217](DSH4xxx.md#dsh4217) | error | '{0}' 缺少其 '{1}' 参数。 |
| [DSH4218](DSH4xxx.md#dsh4218) | error | '{0}' 会回写 {1}，而此变量是 {2}；out 参数必须完全匹配。 |
| [DSH4220](DSH4xxx.md#dsh4220) | error | '{0}' 已在此代码块中声明。 |
| [DSH4221](DSH4xxx.md#dsh4221) | error | {0}() 没有分量；请写出值，例如 'float3(0.0)'。 |
| [DSH4222](DSH4xxx.md#dsh4222) | error | '{0}' 有 {1} 个字段，而此列表有 {2} 个。 |
| [DSH4223](DSH4xxx.md#dsh4223) | error | {0} 期望 {1}，而这是 {2}。 |
| [DSH4224](DSH4xxx.md#dsh4224) | error | '{0}' 最多接收 {1} 个参数。 |
| [DSH4225](DSH4xxx.md#dsh4225) | error | 构造函数按顺序接收其分量；命名参数应放在 'UE.' 节点和函数调用上。 |
| [DSH4226](DSH4xxx.md#dsh4226) | error | {0} 期望 {1}，而这是 {2}。 |
| [DSH4227](DSH4xxx.md#dsh4227) | error | '{0}' 不是此语言以调用形式表示的操作。 |
| [DSH4228](DSH4xxx.md#dsh4228) | error | {0} 期望 {1}，而这是 {2}。 |
| [DSH4229](DSH4xxx.md#dsh4229) | error | 'uniform' 是输入，而 'static const' 是常量；两者都不能被赋值。请将其复制到局部... |
| [DSH4230](DSH4xxx.md#dsh4230) | error | '.{0}' 不是 swizzle；swizzle 是 'xyzw' 或 'rgba' 中的一到四个字符。 |
| [DSH4231](DSH4xxx.md#dsh4231) | error | '{0}.{1}' 是输出节点，不是值：请将其作为语句写在单独一行。 |
| [DSH4233](DSH4xxx.md#dsh4233) | error | 分量索引必须是编译时常量；请写成 '.z' 之类的 swizzle，或使用 'lerp' 进行选择。 |
| [DSH4234](DSH4xxx.md#dsh4234) | error | {0} 类型的值无法被索引。 |
| [DSH4235](DSH4xxx.md#dsh4235) | error | '++' 和 '--' 会回写到它们读取的内容，因此需要一个变量。 |
| [DSH4236](DSH4xxx.md#dsh4236) | error | 'in' 参数是函数输入引脚，不能被写入；请将其声明为 'out' 或 'inout'，或将其复制... |
| [DSH4237](DSH4xxx.md#dsh4237) | error | 初始化列表只有针对已声明的类型才有意义；它不能独立存在。 |
| [DSH4238](DSH4xxx.md#dsh4238) | error | '{0}' 不返回任何内容，因此其调用没有值；其结果通过其 out 参数返回。 |
| [DSH4239](DSH4xxx.md#dsh4239) | error | '{0}' 是 '{1}' 的 out 参数，因此其参数必须是变量。 |
| [DSH4240](DSH4xxx.md#dsh4240) | error | '#pragma endregion' 关闭了一个从未打开的框。 |
| [DSH4241](DSH4xxx.md#dsh4241) | error | 多维数组没有图形形式；请声明一个维度，或将代码移到 '/// @custom' 函数中... |
| [DSH4242](DSH4xxx.md#dsh4242) | error | 数组索引必须是编译时常量：图形没有数组，因此每个元素都在编译时读取... |
| [DSH4243](DSH4xxx.md#dsh4243) | error | 此 '/' 的两边都是整数，而材质图形没有整数除法；请写成 'float(a) / b'... |
| [DSH4244](DSH4xxx.md#dsh4244) | error | 矩阵行无法读取：图形没有矩阵。请将代码移到 '/// @custom' 函数中，在那里... |
| [DSH4245](DSH4xxx.md#dsh4245) | error | 未指定大小的数组需要初始化列表来确定其长度。 |
| [DSH4246](DSH4xxx.md#dsh4246) | 错误 | 材质图没有双曲节点，因此 '{0}' 无法降级；请在 '/// @custom' 主体中编写它，... |
| [DSH4250](DSH4xxx.md#dsh4250) | 错误 | '{0}' 是 GLSL 的写法；本语言是 HLSL，因此请写 '{1}'。 |
| [DSH4251](DSH4xxx.md#dsh4251) | 警告 | '{0}' 在本函数剩余部分中隐藏了同名的文件作用域声明。 |
| [DSH4260](DSH4xxx.md#dsh4260) | 错误 | {0} 需要 {1}，而这里是 {2}。 |
| [DSH4261](DSH4xxx.md#dsh4261) | 错误 | '{0}' 返回 {1}，因此此 'return' 需要一个值。 |
| [DSH4262](DSH4xxx.md#dsh4262) | 错误 | '{0}' 不返回任何内容，因此此 'return' 不能携带值；额外结果写入 'out' 参数。 |
| [DSH4263](DSH4xxx.md#dsh4263) | 错误 | 'discard' 属于材质入口或层；材质函数没有自己的像素可丢弃。请... |
| [DSH4264](DSH4xxx.md#dsh4264) | 错误 | 'break' 用于离开循环，而此处并不在 'for'、'while' 或 'do' 内。 |
| [DSH4300](DSH4xxx.md#dsh4300) | 错误 | 节点 {0} 从节点 %{2} 读取 {1}，而该节点不存在；图中只有 {3} 个节点。 |
| [DSH4301](DSH4xxx.md#dsh4301) | 错误 | 节点 {0} 从节点 {2} 读取 {1}，而该节点是一条语句，不产生值。 |
| [DSH4302](DSH4xxx.md#dsh4302) | 错误 | 节点 {0} 有 {1} 个操作数；此运算恰好需要 {2} 个。 |
| [DSH4303](DSH4xxx.md#dsh4303) | 错误 | 节点 {0} 的 {1} 没有连接任何内容；此运算的每个操作数都必须携带一个值。 |
| [DSH4304](DSH4xxx.md#dsh4304) | 错误 | 节点 {0} 携带 {1} 个位置操作数；此运算需要 {2} 个。 |
| [DSH4305](DSH4xxx.md#dsh4305) | 错误 | 节点 {0} 有一个具名输入，但没有引脚名称。 |
| [DSH4306](DSH4xxx.md#dsh4306) | 错误 | 节点 {0} 对操作数 {1} 执行算术运算，而它是 {2}；图上只携带 float1 到 float4，因此矩... |
| [DSH4307](DSH4xxx.md#dsh4307) | 错误 | 节点 {0} 是一条语句，必须没有输出，但它声明了 {1}。 |
| [DSH4308](DSH4xxx.md#dsh4308) | 错误 | 节点 {0} 有 {1} 个输出名称，对应 {2} 个输出；这两个列表要么一一对应，要么省略名称。 |
| [DSH4309](DSH4xxx.md#dsh4309) | 错误 | 节点 {0} 是目录索引为 {1} 的反射节点，而该索引不是内置目录的条目。 |
| [DSH4310](DSH4xxx.md#dsh4310) | 错误 | 节点 {0} 连接引脚 '{1}'，而 '{2}' 没有该引脚。 |
| [DSH4311](DSH4xxx.md#dsh4311) | 错误 | 节点 {0} 设置属性 '{1}'，而 '{2}' 没有该属性。 |
| [DSH4312](DSH4xxx.md#dsh4312) | 错误 | 节点 {0} 命名为类 '{1}'，但其目录条目是 '{2}'；两者必须一致。 |
| [DSH4313](DSH4xxx.md#dsh4313) | 错误 | 材质产物 {0} 有 {1} 个 MaterialSink 节点；一个材质恰好只有一个。 |
| [DSH4314](DSH4xxx.md#dsh4314) | 错误 | 产物 {0} 在其 {2} 中列出节点 %{1}，而该节点不存在。 |
| [DSH4315](DSH4xxx.md#dsh4315) | 错误 | 产物 {0} 是 {1} 且不产生任何内容；材质函数至少需要一个 FunctionOutput。 |
| [DSH4316](DSH4xxx.md#dsh4316) | 错误 | 产物 {0} 无法排序：这些节点直接或通过其他节点彼此馈送 -- {1}。 |
| [DSH4317](DSH4xxx.md#dsh4317) | 错误 | 节点 {0} 需要一个保存组件字母的字符串 Mask 属性。 |
| [DSH4318](DSH4xxx.md#dsh4318) | 错误 | 节点 {0} 从 {2} 中屏蔽 '{1}'，而该类型只有 {3} 个组件。 |
| [DSH4319](DSH4xxx.md#dsh4319) | 错误 | 节点 {0} 对 {1} 进行切换；StaticSwitch 条件是一个静态布尔值。 |
| [DSH4320](DSH4xxx.md#dsh4320) | 错误 | 节点 {0} 位于区域 {1}，而该区域不存在；图中有 {2} 个区域。 |
| [DSH4321](DSH4xxx.md#dsh4321) | 错误 | 产物 {0} 在其 {2} 个节点中的 {1} 个节点上有去重键；该键要么为整个图计算，要么为... |
| [DSH4322](DSH4xxx.md#dsh4322) | 错误 | 产物 {2} 的节点 %{0} 和 %{1} 具有相同的去重键，因此去重阶段本应将它们合并为... |
| [DSH4323](DSH4xxx.md#dsh4323) | 错误 | 节点 {0} 需要属性 '{1}'，但它没有该属性。 |
| [DSH4324](DSH4xxx.md#dsh4324) | 错误 | 节点 {0} 携带一个没有名称的属性。 |
| [DSH4325](DSH4xxx.md#dsh4325) | 错误 | 节点 {0} 写入属性 '{1}'，而该属性不在引擎的材质属性表中。 |
| [DSH4326](DSH4xxx.md#dsh4326) | 错误 | 产物 {0} 没有名称；资产名称来自导出的函数或 '/// @name'。 |
| [DSH4327](DSH4xxx.md#dsh4327) | 错误 | 节点 {0} 调用本地产物 {1}，而此模块没有该产物；它具有 {2}。 |
| [DSH4328](DSH4xxx.md#dsh4328) | 警告 | 产物 {1} 的布局提示 {0} 的种类为 '{2}'；提示应恰好拼写为 'Node' 或 'Comment'，因此此提示... |
| [DSH4329](DSH4xxx.md#dsh4329) | 错误 | 产物 {0} 是材质实例，它分配其父级的参数且没有图，并且它携带... |
| [DSH4330](DSH4xxx.md#dsh4330) | 错误 | 材质实例产物 {0} 需要它所实例化的材质，而它的父级引用为空。 |
| [DSH4331](DSH4xxx.md#dsh4331) | 错误 | 产物 {1} 的覆盖 '{0}' 是 {2} 覆盖，持有 {3} 值，而 {2} 覆盖持有 {4}。 |
| [DSH4332](DSH4xxx.md#dsh4332) | 错误 | 产物 {0} 覆盖 '{1}' 两次，而一个实例只设置每个参数一次。 |
| [DSH4333](DSH4xxx.md#dsh4333) | 错误 | 产物 {1} 的覆盖 '{0}' 命名了一个参数，而该产物携带的父级架构没有该参数... |
| [DSH4334](DSH4xxx.md#dsh4334) | 错误 | 此模块持有一个材质实例和 {0} 个其他产物，而材质实例是唯一的产物... |
| [DSH4335](DSH4xxx.md#dsh4335) | 错误 | 材质实例产物 {0} 携带后端 {1}，而实例是带有默认值的普通材质实例... |
| [DSH4350](DSH4xxx.md#dsh4350) | 错误 | IR 构建器在此无法读取 '{0}'：绑定器将其读取来源的类型视为材质，但该... |
| [DSH4352](DSH4xxx.md#dsh4352) | 错误 | 此模块在绑定时没有内置目录，因此无法命名任何 'UE.*' 调用，也无法命名任何材质属性；... |
| [DSH4360](DSH4xxx.md#dsh4360) | 错误 | 此循环运行的次数编译器无法在 {0} 或更少次数内确定，而材质图没有循环... |
| [DSH4361](DSH4xxx.md#dsh4361) | 错误 | 此表达式是 {0}，而材质图没有矩阵；请在自定义主体内部计算它。 |
| [DSH4362](DSH4xxx.md#dsh4362) | 错误 | 'discard' 没有材质图形式；请改为将材质的 OpacityMask 设置为零，或将分支移入... |
| [DSH4363](DSH4xxx.md#dsh4363) | 错误 | 'if' 内部的 '{0}' 无法展开，因为 'if' 的两个分支都会成为节点，而其中只有一个可... |
| [DSH4364](DSH4xxx.md#dsh4364) | 错误 | 类型为 {0} 的参数不能作为材质函数输入；图中没有携带该类型的引脚。 |
| [DSH4365](DSH4xxx.md#dsh4365) | 错误 | 结构体没有图形形式；请传递其某个字段，或将整个结构体移入 '/// @custom' 函数中。 |
| [DSH4366](DSH4xxx.md#dsh4366) | 错误 | '{0}' 是常量纹理，即一个 TextureObject 节点，而此引擎没有这样的材质表达式... |
| [DSH4370](DSH4xxx.md#dsh4370) | 错误 | '{0}' 在没有任何写入之前被读取；材质属性在此函数赋值之前没有值。 |
| [DSH4371](DSH4xxx.md#dsh4371) | 错误 | 一个 'material' 值到达了携带 {0} 的引脚；只有 MaterialAttributes 引脚才接受它。 |
| [DSH4372](DSH4xxx.md#dsh4372) | 错误 | '{0}' 仅在此 'if' 的一个分支中被赋值，且在此之前没有值；请在两个分支中都赋值，或为其提... |
| [DSH4373](DSH4xxx.md#dsh4373) | 错误 | '{0}' 是 {1} 的属性，需要一个字面量；此参数是在运行时计算的。 |
| [DSH4374](DSH4xxx.md#dsh4374) | 错误 | 此处构造了一个 {0} 分量值，但材质图最多携带四个分量。 |
| [DSH4375](DSH4xxx.md#dsh4375) | 错误 | 此分支的两端以不同的完整材质结束，而 DreamShader 在属性... 之间进行选择 |
| [DSH4376](DSH4xxx.md#dsh4376) | 错误 | '{0}' 在此处被读取，但在任何到达此行的路径上都没有任何东西为它赋值；请先赋值，或为其... |
| [DSH4377](DSH4xxx.md#dsh4377) | 错误 | '{0}' 写入属性中持有的材质，而属性接受一个整体值，而不是对... 的写入 |
| [DSH4378](DSH4xxx.md#dsh4378) | 错误 | 一个分支在两个 {0} 值之间或两个数字之间进行选择，而此分支各有一个。 |
| [DSH4379](DSH4xxx.md#dsh4379) | 错误 | 分支只能在数字、布尔值和静态 Substrate 值之间进行选择，而这些是 {0} 对象，它们... |
| [DSH4380](DSH4xxx.md#dsh4380) | 警告 | 此分支会变成一个 'Substrate.Select'，它对其两个输入进行参数混合；它们是一个 '{0}' 和一个 '{1... |
| [DSH4381](DSH4xxx.md#dsh4381) | 错误 | '{0}' 由节点 'Substrate.{1}' 转换，而此引擎没有这样的节点；请为其馈送的引脚提供不同... |
| [DSH4382](DSH4xxx.md#dsh4382) | 错误 | 此材质驱动 FrontMaterial 并声明 'Substrate = Native'，而此项目中 Substrate 已关闭；... |
| [DSH4383](DSH4xxx.md#dsh4383) | 错误 | 当循环回到 '{0}.{1}' 时，'{0}' 不再是正在构建的 Substrate 值：先前的一次... |
| [DSH4390](DSH4xxx.md#dsh4390) | 信息 | '{0}' 已声明但没有任何东西读取它，因此它不在生成的材质中。 |
| [DSH4400](DSH4xxx.md#dsh4400) | 错误 | 缓冲区 '{0}' 被声明了两次；第 {1} 行的声明已经创建了它。 |
| [DSH4401](DSH4xxx.md#dsh4401) | 错误 | 名为 '{0}' 的通道已在第 {1} 行声明；RDG 事件和统计信息以通道命名，因此每个名称... |
| [DSH4402](DSH4xxx.md#dsh4402) | 错误 | '{0}' 是通道绑定而无需声明的内置纹理，不能声明为缓冲区。 |
| [DSH4403](DSH4xxx.md#dsh4403) | 错误 | '{0}' 既不是此管线的缓冲区，也不是内置纹理；请用 'buffer {0} : <Format>;' 声明它。 |
| [DSH4404](DSH4xxx.md#dsh4404) | 错误 | '{0}' 不是此管线的 'uniform' 或 'static const'；'Enabled' 接受 'uniform bool' 或 'true'。 |
| [DSH4405](DSH4xxx.md#dsh4405) | 错误 | '{0}' 是一个 {1}，而 'Enabled' 接受 'uniform bool' 或 'true'。 |
| [DSH4406](DSH4xxx.md#dsh4406) | 错误 | 未找到材质 '{0}'：裸名称是同一源根目录下的 '.dss' 所构建的材质，而 obj... |
| [DSH4407](DSH4xxx.md#dsh4407) | 错误 | '{0}' 不是 '.usf' 或 '.ush' 文件；引擎只编译这两种着色器文件。 |
| [DSH4408](DSH4xxx.md#dsh4408) | 错误 | 着色器文件 '{0}' 不存在；以 '/' 开头的路径是虚拟着色器路径，任何其他路径都是相对... |
| [DSH4410](DSH4xxx.md#dsh4410) | 错误 | '{0}' 未定义函数 '{1}'。 |
| [DSH4411](DSH4xxx.md#dsh4411) | 错误 | '{1}' 中的 '{0}' 没有编译器可读取的 '[numthreads(x, y, z)]'；请在... 中写入 'Threads = uint3(x, y, z)' |
| [DSH4412](DSH4xxx.md#dsh4412) | 错误 | '{0}' 不是此项目的通道层；这些层是 Project Settings > DreamPl... 的前 {1} 个名称 |
| [DSH4413](DSH4xxx.md#dsh4413) | 错误 | 'Threads = uint3({0}, {1}, {2})' 与 '{6}' 的 '[numthreads({3}, {4}, {5})]' 不一致，分派将... |
| [DSH4414](DSH4xxx.md#dsh4414) | 错误 | 'DreamPassWeight' 是管线在视图中的权重，每个通道都可以通过 'param P = DreamPassWeig... 读取它 |
| [DSH4415](DSH4xxx.md#dsh4415) | 错误 | '{0}.Previous'：内置纹理不保留历史记录。 |
| [DSH5200](DSH5xxx.md#dsh5200) | 错误 | 材质没有 '{0}' 引脚；您是指 '{1}' 吗？属性名称区分大小写。 |
| [DSH5201](DSH5xxx.md#dsh5201) | 错误 | 此 Custom 节点未声明名为 '{0}' 的输出；其输出为 '{1}'。输出由 'Addition... 声明 |
| [DSH5202](DSH5xxx.md#dsh5202) | 警告 | 内置目录为空，因此无法解析任何 'UE.' 表达式和任何材质属性；请将其导出... |
| [DSH5210](DSH5xxx.md#dsh5210) | 错误 | '{0}.{1}' 不是节点；您是指 '{0}.{2}' 吗？节点名称区分大小写。 |
| [DSH5211](DSH5xxx.md#dsh5211) | 错误 | '{0}.{1}' 是一个节点，必须被调用：请写入 '{0}.{1}(...)'。 |
| [DSH5212](DSH5xxx.md#dsh5212) | 错误 | '{0}' 不是此引擎拥有的材质表达式类。 |
| [DSH5213](DSH5xxx.md#dsh5213) | 错误 | '{0}.{1}' 没有名为 '{2}' 的引脚或属性。 |
| [DSH5214](DSH5xxx.md#dsh5214) | 错误 | {0} 需要 {1}，而这里是 {2}。 |
| [DSH5215](DSH5xxx.md#dsh5215) | 错误 | '{0}' 不是 '{2}' 上 '{1}' 的值。 |
| [DSH5216](DSH5xxx.md#dsh5216) | 错误 | 'Substrate.' 已经命名了该节点，因此不接受 'Class' 参数。 |
| [DSH5217](DSH5xxx.md#dsh5217) | 错误 | 'Class' 以带引号的字符串形式接收表达式类。 |
| [DSH5218](DSH5xxx.md#dsh5218) | 错误 | 'UE.Expression' 访问了本语言没有名称的节点，因此需要 'Class = "MaterialExpressionName"'。 |
| [DSH5219](DSH5xxx.md#dsh5219) | 警告 | '{0}.{1}' 使其必需的 '{2}' 引脚保持未连接；除非该节点为其读取默认值，否则引擎会报... |
| [DSH5220](DSH5xxx.md#dsh5220) | 错误 | '{0}.{1}' 接收具名参数：请写作 'Pin = value'。 |
| [DSH5221](DSH5xxx.md#dsh5221) | 错误 | '{0}.{1}' 按顺序接收 {2} 个参数；请为其余参数命名。 |
| [DSH5223](DSH5xxx.md#dsh5223) | 错误 | '{0}' 是抽象的，无法构成节点。 |
| [DSH5224](DSH5xxx.md#dsh5224) | 错误 | '{0}' 是枚举属性；请写入其取值之一，例如 'SamplerType = Normal'。 |
| [DSH5230](DSH5xxx.md#dsh5230) | 错误 | 纹理采样的第一个参数是纹理，而这里是 {0}。 |
| [DSH5231](DSH5xxx.md#dsh5231) | 错误 | 'Texture2DSample' 的第二个参数是采样器，而这里是 {0}。 |
| [DSH5250](DSH5xxx.md#dsh5250) | 错误 | 期望 'OutputIndex' 为大于等于零的整数，但发现 '{0}'。 |
| [DSH5251](DSH5xxx.md#dsh5251) | 错误 | 期望 'Output' 以带引号的名称或标识符来指定输出，但发现 '{0}'。 |
| [DSH5252](DSH5xxx.md#dsh5252) | 错误 | 此调用中应存在 'Output' 或 'OutputIndex' 之一，但同时发现了两者。 |
| [DSH5253](DSH5xxx.md#dsh5253) | 错误 | 输出选择器应仅出现在具有输出的调用中，但在构造函数 '{0}' 上发现了它。 |
| [DSH5254](DSH5xxx.md#dsh5254) | 警告 | '{0}' 在此处不接受位置参数；1.x 会忽略它，因此它被丢弃。 |
| [DSH5255](DSH5xxx.md#dsh5255) | 错误 | 期望 'UE.SceneTexture' 恰好接收一个参数 'Id = ...'，但发现 {0} 个参数。 |
| [DSH5256](DSH5xxx.md#dsh5256) | 错误 | 期望 'SampleTexture2D' 恰好接收两个位置参数，即一个纹理和坐标，但发现 {0} 个... |
| [DSH5257](DSH5xxx.md#dsh5257) | 错误 | 期望 'UE.StaticSwitchParameter' 通过 'Name = "..."' 为其参数命名，但未发现名称。 |
| [DSH5258](DSH5xxx.md#dsh5258) | 错误 | 期望静态开关 '{0}' 以 'True = ...' 和 'False = ...' 输入进行调用，但最多发现 ... |
| [DSH5259](DSH5xxx.md#dsh5259) | 错误 | 期望参数调用 '{0}' 的每个参数都指定一个输入引脚，但发现了一个位置参数。 |
| [DSH5260](DSH5xxx.md#dsh5260) | 错误 | 期望 '::' 之后有名称，但发现 {0}。 |
| [DSH5261](DSH5xxx.md#dsh5261) | 错误 | 期望 Custom 表达式的 'OutputType' 为 float1 到 float4 或 MaterialAttributes，但发现 '{0}'。 |
| [DSH5263](DSH5xxx.md#dsh5263) | 错误 | 期望 'UE.StaticSwitchParameter' 的 'Default' 为 true 或 false，但发现 '{0}'。 |
| [DSH5264](DSH5xxx.md#dsh5264) | 错误 | 期望 'UE.StaticSwitchParameter' 的 'SortPriority' 为整数，但发现 '{0}'。 |
| [DSH5265](DSH5xxx.md#dsh5265) | 错误 | 期望参数调用 '{0}' 只接收其输入，但发现了一个输出选择器。 |
| [DSH5275](DSH5xxx.md#dsh5275) | 警告 | '{0}' 仅在与 '{1}' 的大小写上不同；1.x 源在读取时忽略大小写，因此这里是 '{1}'，而 '.dss' 需要 ... |
| [DSH5276](DSH5xxx.md#dsh5276) | 警告 | '{0}' 仅在与引擎名称 '{1}' 的大小写上不同；1.x 在匹配引擎名称时忽略大小写，因此这里是 '{1}'，... |
| [DSH5277](DSH5xxx.md#dsh5277) | 警告 | '{0}' 是 '{1}' 的 GLSL 拼写；1.x 源可能使用它，它会被读作 '{1}'，而 '.dss' 写入 '... |
| [DSH5278](DSH5xxx.md#dsh5278) | 警告 | '{0}' 是 '{2}' 的 '{1}' 空间的 1.x 名称；'.dss' 写入 '{1}'。 |
| [DSH5279](DSH5xxx.md#dsh5279) | 警告 | '{0}.{1}' 使其必需的 '{2}' 引脚保持未连接，这是 1.x 允许的，而引擎会在材质... |
| [DSH5280](DSH5xxx.md#dsh5280) | 错误 | '{0}' 没有可选择的输出，而此调用选择了 '{1}'。 |
| [DSH5281](DSH5xxx.md#dsh5281) | 错误 | 输出应由编译器已知的整数来选择，而此索引是计算得出的。 |
| [DSH5282](DSH5xxx.md#dsh5282) | 错误 | 此 Custom 节点声明了 {0} 个输出（从 0 开始计数），而这里选择了输出 {1}。 |
| [DSH5283](DSH5xxx.md#dsh5283) | 信息 | '{0}' 未声明，与 1.x 一样，此处将其声明为类型为 {1} 的局部变量，接收 '{3}' 的 '{2}'。 |
| [DSH5284](DSH5xxx.md#dsh5284) | 错误 | '{0}' 成为自定义节点的输入，该输入携带数字或纹理，而此参数是 {1}。 |
| [DSH5285](DSH5xxx.md#dsh5285) | 错误 | '{0}.{1}' 有 {2} 个输入引脚（从 0 开始计数），而此参数连接引脚 {3}。 |
| [DSH5286](DSH5xxx.md#dsh5286) | 错误 | 'Pin[{0}] = ...' 按引擎索引连接节点的输入引脚，只有 'UE.' 或 'Substrate.' 节点可以... |
| [DSH5287](DSH5xxx.md#dsh5287) | 信息 | '{0}' 有多个输出，读取时取其第一个 '{1}'，这与 1.x 的行为一致；'.dss' 会指定该输出... |
| [DSH5288](DSH5xxx.md#dsh5288) | 警告 | '{0}.{1}' 没有 'DefaultValue'，因此为该参数写入的默认值被丢弃，正如 1.x 也会丢弃它一样。 |
| [DSH5289](DSH5xxx.md#dsh5289) | 信息 | {0} 需要 {1}，而这里是 {2}：取其前导分量，这与 1.x 的行为一致；'.dss' 会写入 ... |
| [DSH5290](DSH5xxx.md#dsh5290) | 信息 | 此块的资产与本文件可调用的函数都名为 '{0}'，1.x 将它们区分开来；该块... |
| [DSH5291](DSH5xxx.md#dsh5291) | 信息 | '{0}.{1}' 未列出名为 '{2}' 的引脚；节点存在后将按该名称连接，因为节点可能 ... |
| [DSH5292](DSH5xxx.md#dsh5292) | 信息 | '{0}' 未声明，与 1.x 一样，此赋值会将其声明为类型为 {1} 的局部变量。 |
| [DSH5293](DSH5xxx.md#dsh5293) | error | Substrate 值没有复合赋值；请写 'S = S + T' (Substrate.Add) 或 'S = S * w' (Substrate.W... |
| [DSH5294](DSH5xxx.md#dsh5294) | error | 'Substrate.{0}' 是 Unreal Engine 从 {1} 起才拥有的节点；此引擎没有该节点。 |
| [DSH5295](DSH5xxx.md#dsh5295) | error | '{0}.{1}'：'{2}' 和 '{3}' 参数化相同的引脚；请提供其中之一。 |
| [DSH5296](DSH5xxx.md#dsh5296) | error | '{0}.Thickness' 是测量 'Transmittance' 的深度，而在此之前 '{0}' 没有被赋予 'Transmittance'... |
| [DSH5297](DSH5xxx.md#dsh5297) | error | '{0}' 自声明以来被赋值了一个完整的 Substrate 值，而该值没有成员；请构建... |
| [DSH5298](DSH5xxx.md#dsh5298) | error | Substrate 值 '{0}' 的成员不能在 'if' 内写入：那会让一个节点处于两个版本... |
| [DSH5299](DSH5xxx.md#dsh5299) | error | '{0}' 构建了一个 '{1}.{2}'，它没有名为 '{3}' 的引脚。 |
| [DSH5300](DSH5xxx.md#dsh5300) | error | 'UE.{0}' 需要 {1}，而此引擎的节点目录中没有它。 |
| [DSH5301](DSH5xxx.md#dsh5301) | error | 材质 '{0}' 要求使用新的材质转换器（'bEnableNewHLSLGenerator = true'），并且使用了 {1}（第 ... |
| [DSH5302](DSH5xxx.md#dsh5302) | warning | UE.DreamPassOutput 在 '{0}'（一个 {1}）中不起作用：引擎从材质自身的 g... 编译自定义输出 |
| [DSH5303](DSH5xxx.md#dsh5303) | error | 材质 '{0}' 在此处获得了第二个 UE.DreamPassOutput 节点（第一个位于第 {1} 行），而一个材质编译... |
| [DSH5315](DSH5xxx.md#dsh5315) | error | UE.DreamPassBuffer 未指定 Pipeline：请写入 '.dsp' 的名称（Pipeline = "CP_Highlight"）或 pipel... |
| [DSH5316](DSH5xxx.md#dsh5316) | error | '{1}' 下没有由 .dsp 构建的名为 '{0}' 的管线；请写入管线资源的路径，或检查名称。 |
| [DSH5317](DSH5xxx.md#dsh5317) | error | '{0}' 在此源根目录下命名了多个管线（{1}）；请改为写入管线资源的路径。 |
| [DSH5318](DSH5xxx.md#dsh5318) | error | 管线 '{0}' 未命名任何内容：'{1}' 处不存在管线资源，且源根目录下没有 .dsp 构建... |
| [DSH5319](DSH5xxx.md#dsh5319) | error | 管线 '{0}' 来自 '{1}'，后者编译失败，因此此材质读取的缓冲区无法被 c... |
| [DSH5320](DSH5xxx.md#dsh5320) | info | 管线 '{0}' 缺失或比其源文件更旧，因此先编译了 '{1}'。 |
| [DSH5321](DSH5xxx.md#dsh5321) | error | （在运行时构建） |
| [DSH5322](DSH5xxx.md#dsh5322) | error | 管线 '{0}' 未声明缓冲区 '{1}'。 |
| [DSH5323](DSH5xxx.md#dsh5323) | error | 管线 '{0}' 的缓冲区 '{1}' 是 {2}，材质无法采样它。请导出 float 或归一化缓冲... |
| [DSH5324](DSH5xxx.md#dsh5324) | error | UE.DreamPassBuffer 未指定 '{0}' 的 Buffer：请写入所导出缓冲区的名称（Buffer = "Blurred"）。 |
| [DSH5325](DSH5xxx.md#dsh5325) | error | 管线 '{0}' 需要构建此材质——它是该管线的 pass 材质之一，或是... 的材质 |
| [DSH5326](DSH5xxx.md#dsh5326) | error | '{0}' 是一个 {1}，不是 DreamShader pass 管线。 |
| [DSH6200](DSH6xxx.md#dsh6200) | error | '{0}' 是第二个材质条目；其上面的 '{1}' 已经是该条目，而一个文件只能生成一个材质。 |
| [DSH6201](DSH6xxx.md#dsh6201) | error | '{0}' 是从条目为 '{1}' 的文件导出的；一个文件生成材质或生成函数，不能两者兼... |
| [DSH6202](DSH6xxx.md#dsh6202) | error | 'extern {0}' 没有可绑定的对象；请在其上方添加 '/// @asset /Game/.../MF_Name'。 |
| [DSH6203](DSH6xxx.md#dsh6203) | error | '@layer' 使 '{0}' 成为材质层资源，因此它必须是 'export'。 |
| [DSH6204](DSH6xxx.md#dsh6204) | error | '@layer' 函数写作 'export void {0}(inout material m)'。 |
| [DSH6205](DSH6xxx.md#dsh6205) | error | '@layerblend' 函数写作 'export void {0}(material Base, material Top, ..., inout material Resul... |
| [DSH6206](DSH6xxx.md#dsh6206) | error | '{0}' 是内置操作，无法重新声明；请重命名该函数。 |
| [DSH6208](DSH6xxx.md#dsh6208) | error | '{0}' 是一个 {1} 资源，不是此文件可以调用的函数。 |
| [DSH6209](DSH6xxx.md#dsh6209) | error | '{0}' 没有函数体；原型必须是 'extern' 并携带 '/// @asset'。 |
| [DSH6210](DSH6xxx.md#dsh6210) | error | '{0}' 是 '@custom'，因此 '{1}' 成为 Custom 节点的输入引脚，而 Custom 节点不能接受 materia... |
| [DSH6211](DSH6xxx.md#dsh6211) | error | '{0}' 是 '{1}' 的 'out' 参数，但函数体从未对其赋值，因此调用方会读取到没有任何... 的值 |
| [DSH6220](DSH6xxx.md#dsh6220) | error | '{0}' 调用了自身，而内联函数没有可供递归的栈；请将其重写为带有常量... 的循环 |
| [DSH6221](DSH6xxx.md#dsh6221) | error | 解析 '{0}' 的默认值将深入 {1} 层调用，超过 {2} 的限制；请传入显式值或 flat... |
| [DSH6222](DSH6xxx.md#dsh6222) | error | '{0}' 是 {2} 的 '{1}' 参数，因此实参必须是可被赋值的内容；此表达式... |
| [DSH6223](DSH6xxx.md#dsh6223) | error | '{0}' 是此文件的材质条目，由引擎调用，而不是由着色器调用。 |
| [DSH6224](DSH6xxx.md#dsh6224) | error | '{1}' 中 '{0}' 的默认值依赖于自身；请为循环中的某个参数传入显式值。 |
| [DSH6250](DSH6xxx.md#dsh6250) | error | '{0}' 没有逐字 HLSL 函数体，因此无法成为自定义节点；只有 '/// @custom' 函数可以。 |
| [DSH6251](DSH6xxx.md#dsh6251) | error | '{0}' 将 '{1}' 声明为 'inout'，而自定义节点无法携带该参数；请将其拆分为一个 'in' 参数和一个 '... |
| [DSH6252](DSH6xxx.md#dsh6252) | error | '{0}' 将材质 '{1}' 作为输入；自定义节点无法在引脚上接受材质属性，因此 re... |
| [DSH6253](DSH6xxx.md#dsh6253) | error | '{0}' 在 '{1}' 上使用了 Substrate；自定义节点没有 Substrate 引脚，因此请在... 之外构建材质的该部分 |
| [DSH6254](DSH6xxx.md#dsh6254) | error | '{0}' 通过 '{1}' 返回纹理；自定义节点输出仅携带 float1..4 或材质属性，绝不... |
| [DSH6255](DSH6xxx.md#dsh6255) | error | '{0}' 在其生成的 HLSL 中两次声明 '{1}'；一个纹理参数也声称 '{1}Sampler'，这... |
| [DSH6256](DSH6xxx.md#dsh6256) | error | '{0}' 返回 void，但其函数体使用了 'return;'；自定义节点总是返回其第一个输出，因此请... |
| [DSH6257](DSH6xxx.md#dsh6257) | warning | '{0}' 声明了返回类型，但其函数体从未返回任何值；该节点的第一个输出将为 0。 |
| [DSH6258](DSH6xxx.md#dsh6258) | error | '{0}' 的 '#include' 路径为空；请填写该头文件所在的虚拟着色器路径，例如... |
| [DSH6259](DSH6xxx.md#dsh6259) | warning | '{0}' 与函数 '{1}' 仅大小写不同；HLSL 区分大小写，因此此调用被留给着... |
| [DSH6260](DSH6xxx.md#dsh6260) | error | '@custom' 函数 {0} 相互循环调用；HLSL 不支持递归，因此它们的函数体无法被嵌... |
| [DSH6261](DSH6xxx.md#dsh6261) | error | '{0}' 不是 '@custom' 函数，无法从 '{1}' 的 HLSL 函数体中调用；自定义节点看不到任... |
| [DSH6262](DSH6xxx.md#dsh6262) | error | '{0}' 接受 {1} 个参数，但此调用传入了 {2}；携带纹理的调用无法通过... 匹配 |
| [DSH6263](DSH6xxx.md#dsh6263) | error | '{1}' 的 '{0}' 纹理参数必须是纯纹理名称，因为与之配套的采样器... |
| [DSH6264](DSH6xxx.md#dsh6264) | warning | '{0}' 是 'selfcontained'，因此它所调用的 '@custom' 函数 '{1}' 未嵌入其节点；该调用... |
| [DSH6265](DSH6xxx.md#dsh6265) | error | '{0}' 在其变成的 HLSL 中接受 {1} 个参数，但此调用传入了 {2}。 |
| [DSH6266](DSH6xxx.md#dsh6266) | error | '{0}' 在其变成的 HLSL 中不返回任何内容，因此此调用没有可读取的值；请为每个输... 传入一个变量 |
| [DSH6300](DSH6xxx.md#dsh6300) | error | 在 '{0}' 之后应有一个函数名，却发现 '('。 |
| [DSH6301](DSH6xxx.md#dsh6301) | error | 在 '{0}' 的参数列表中应有一个 '[in\|out] Type Name'，却发现 '{1}'。 |
| [DSH6302](DSH6xxx.md#dsh6302) | error | 在 '{1}' 的参数 '{0}' 之前应有一个 'in' 或 'out'，却发现 '{2}'；1.x 函数没有 'inout'。 |
| [DSH6303](DSH6xxx.md#dsh6303) | error | 在 '{0}' 中应有一个除 '__return' 之外的参数名，'__return' 是 1.x 保留的。 |
| [DSH6304](DSH6xxx.md#dsh6304) | error | '{0}' 上应有一个返回类型或 'out' 参数，却发现两者都有。 |
| [DSH6305](DSH6xxx.md#dsh6305) | error | '{0}' 应返回一个值或至少有一个 'out' 参数，却发现两者都没有。 |
| [DSH6306](DSH6xxx.md#dsh6306) | warning | 'Inline' 是 'SelfContained' 的旧拼写，两者含义相同：该函数的节点嵌入了该函... |
| [DSH6307](DSH6xxx.md#dsh6307) | error | 在 'GraphFunction' 之后应有一个返回类型或一个名称，却发现修饰符 '{0}'，它只属于 Function。 |
| [DSH6308](DSH6xxx.md#dsh6308) | error | 在 '{0}' 中的 'return' 之后应有一个值，该函数返回 '{1}'，却发现一个单独的 'return;'。 |
| [DSH6309](DSH6xxx.md#dsh6309) | error | 'Namespace' 上应有一个带名称的 'Name = "..."' 特性，却发现没有。 |
| [DSH6310](DSH6xxx.md#dsh6310) | error | 在命名空间 '{0}' 内应只有 Function 和 GraphFunction 块，却发现 {1}。 |
| [DSH6311](DSH6xxx.md#dsh6311) | error | 'VirtualFunction' 上应有一个带名称的 'Name = "..."' 特性，却发现没有。 |
| [DSH6312](DSH6xxx.md#dsh6312) | error | VirtualFunction '{0}' 上应有一个 'Asset = Path(...)' 选项，却发现没有。 |
| [DSH6313](DSH6xxx.md#dsh6313) | error | VirtualFunction '{0}' 上应至少有一个输出，却发现没有。 |
| [DSH6314](DSH6xxx.md#dsh6314) | error | 在 '{0}' 的函数体中应有一个 ')' 来闭合 'UE.' 调用，却发现函数体结尾。 |
| [DSH6316](DSH6xxx.md#dsh6316) | warning | '{0}' 函数体中的 'Substrate.' 调用未被提升为节点，因为没有自定义节点输入携带... |
| [DSH6319](DSH6xxx.md#dsh6319) | error | '{0}' 的参数列表应以 '(' 开始，却发现 {1}。 |
| [DSH6325](DSH6xxx.md#dsh6325) | error | '{0}' 从其函数体中提升出一个 'UE.' 调用，该调用读取 '{1}'，1.x 从调用者作用域中获取它，而 2.0 则... |
| [DSH6326](DSH6xxx.md#dsh6326) | error | '{0}' 从其函数体中提升出一个 'UE.' 调用，该调用读取 '{1}'，它既不是 '{0}' 的参数也未声明... |
| [DSH6327](DSH6xxx.md#dsh6327) | error | '{0}' 将从其函数体提升出的 'UE.' 调用作为自身自定义节点的输入，因此 '{... 的 HLSL 函数体 |
| [DSH6328](DSH6xxx.md#dsh6328) | error | '{0}' 将其输入 '{1}' 背后的调用从其函数体不存在的位置提升出来，因此其自定义节点的代... |
| [DSH6329](DSH6xxx.md#dsh6329) | error | '{0}' 携带了从其函数体提升出的 'UE.' 调用，而只有带有逐字函数体的 '@custom' 函数才会提升... |
| [DSH6330](DSH6xxx.md#dsh6330) | error | '{0}' 通过从其函数体提升出的 'UE.' 调用到达自身，而每次调用都会新建一个自定义节点，... |
| [DSH6331](DSH6xxx.md#dsh6331) | warning | '{0}' 不是 '{1}' 块的特性，已被忽略；该标头接受 Generate、Library 和 Descrip... |
| [DSH6332](DSH6xxx.md#dsh6332) | error | '{0}' 含有一个非 '`{Name`}' 的占位符，这是 Generate 模板唯一允许的占位符。 |
| [DSH7111](DSH7xxx.md#dsh7111) | error | %s 的布尔值 '%s' 无效。 |
| [DSH7112](DSH7xxx.md#dsh7112) | error | 设置路径段不能为空。 |
| [DSH7113](DSH7xxx.md#dsh7113) | error | 无效的数组设置段 '%s'。 |
| [DSH7114](DSH7xxx.md#dsh7114) | error | 设置段 '%s' 中的数组索引 '%s' 无效。 |
| [DSH7115](DSH7xxx.md#dsh7115) | error | 无效的数组设置段 '%s'。 |
| [DSH7116](DSH7xxx.md#dsh7116) | error | 无效的材质设置目标。 |
| [DSH7117](DSH7xxx.md#dsh7117) | error | 无效的材质设置路径 '%s'。 |
| [DSH7118](DSH7xxx.md#dsh7118) | error | 不支持的材质设置 '%s'。 |
| [DSH7119](DSH7xxx.md#dsh7119) | error | 设置 '%s' 不是索引数组属性。 |
| [DSH7120](DSH7xxx.md#dsh7120) | error | 数组索引 %d 超出设置 '%s' 的范围（最大 %d）。 |
| [DSH7121](DSH7xxx.md#dsh7121) | error | 设置 '%s' 需要显式的 [index]。 |
| [DSH7122](DSH7xxx.md#dsh7122) | error | 设置路径 '%s' 无法继续穿过 '%s'。 |
| [DSH7123](DSH7xxx.md#dsh7123) | error | 无效的材质设置路径 '%s'。 |
| [DSH7124](DSH7xxx.md#dsh7124) | error | 无法为设置验证创建临时材质。 |
| [DSH7125](DSH7xxx.md#dsh7125) | error | 设置 '%2$s' 的值 '%1$s' 无效。%3$s |
| [DSH7126](DSH7xxx.md#dsh7126) | error | 设置 '%2$s' 的值 '%1$s' 无效。%3$s |
| [DSH7127](DSH7xxx.md#dsh7127) | error | 不支持的 BlendMode/RenderType '%s'。 |
| [DSH7128](DSH7xxx.md#dsh7128) | error | ShadingModel="Substrate" 需要 Unreal Engine 5.4 或更高版本。 |
| [DSH7129](DSH7xxx.md#dsh7129) | error | 不支持的 ShadingModel '%s'。 |
| [DSH7130](DSH7xxx.md#dsh7130) | error | 不支持的 MaterialDomain '%s'。 |
| [DSH7131](DSH7xxx.md#dsh7131) | error | 反射属性目标无效。 |
| [DSH7132](DSH7xxx.md#dsh7132) | error | '%s' 不是 '%s' 的有效布尔值。 |
| [DSH7133](DSH7xxx.md#dsh7133) | error | '%s' 不是 '%s' 的有效整数值。 |
| [DSH7134](DSH7xxx.md#dsh7134) | error | '%s' 不是 '%s' 的有效无符号整数值。 |
| [DSH7135](DSH7xxx.md#dsh7135) | error | '%s' 不是 '%s' 的有效数值。 |
| [DSH7136](DSH7xxx.md#dsh7136) | error | '%s' 不是 '%s' 的有效数值。 |
| [DSH7137](DSH7xxx.md#dsh7137) | error | 对象属性 '%s' 需要 Path(...) 或绝对的 Unreal 对象路径。 |
| [DSH7138](DSH7xxx.md#dsh7138) | error | 无法为 '%2$s' 加载资源 '%1$s'。 |
| [DSH7139](DSH7xxx.md#dsh7139) | error | 资源 '%s' 与 '%s' 不兼容。预期为 '%s'。 |
| [DSH7140](DSH7xxx.md#dsh7140) | error | '%s' 不是 '%s' 的有效枚举值。 |
| [DSH7141](DSH7xxx.md#dsh7141) | error | '%s' 不是 '%s' 的有效枚举值。 |
| [DSH7142](DSH7xxx.md#dsh7142) | error | '%s' 不是 '%s' 的有效字节值。 |
| [DSH7143](DSH7xxx.md#dsh7143) | error | '%2$s' 上的属性 '%1$s' 尚不是受支持的文本字面量类型。 |
| [DSH7144](DSH7xxx.md#dsh7144) | error | 反射属性目标无效。 |
| [DSH7200](DSH7xxx.md#dsh7200) | error | '{0}' 被 '#pragma material' 设置了两次；它已在第 {1} 行设置过。 |
| [DSH7201](DSH7xxx.md#dsh7201) | error | 'Backend' 没有值；请写 'Backend = Graph' 或 'Backend = ThinCustom'。在 1.… 中，空值表示 Graph。 |
| [DSH7202](DSH7xxx.md#dsh7202) | error | 'Backend = {0}' 不是后端；后端为 'Graph' 和 'ThinCustom'。 |
| [DSH7203](DSH7xxx.md#dsh7203) | warning | '#pragma material' 配置材质，而此文件没有 'export void Name(inout material m)' 入口… |
| [DSH7204](DSH7xxx.md#dsh7204) | warning | 'Backend = Instance' 是 'Backend = ThinCustom' 的旧写法；请写新写法。 |
| [DSH7205](DSH7xxx.md#dsh7205) | error | '#pragma material' 接受 'Key = Value' 对；'{0}' 没有键。 |
| [DSH7210](DSH7xxx.md#dsh7210) | error | '{0}' 是编译期常量，而此初始化器不是；常量由字面量和其他… 构成。 |
| [DSH7211](DSH7xxx.md#dsh7211) | error | '{0}' 是没有存储类的文件作用域变量；对于材质参数请写 'uniform'，或写 'static c… |
| [DSH7212](DSH7xxx.md#dsh7212) | error | 类型为 {0} 的文件作用域变量没有节点；'uniform' 或 'static const' 必须是数值、布尔、纹理… |
| [DSH7213](DSH7xxx.md#dsh7213) | error | 纹理 uniform 没有 HLSL 初始化器；请将其默认资源写为 '/// @default /Game/...'。 |
| [DSH7214](DSH7xxx.md#dsh7214) | error | '{0}' 是编译期常量，必须在其声明处初始化。 |
| [DSH7215](DSH7xxx.md#dsh7215) | error | '@layer' 和 '@layerblend' 构成两种不同的资源；一个函数只能是其中之一。 |
| [DSH7216](DSH7xxx.md#dsh7216) | error | 'uniform' 数组没有参数节点；请为每个元素声明一个 uniform，或将其设为 'static const'。 |
| [DSH7220](DSH7xxx.md#dsh7220) | error | '@slider' 接受两个数字，即最小值和最大值；'{0}' 不是这样。 |
| [DSH7221](DSH7xxx.md#dsh7221) | error | '@sort' 接受一个整数；'{0}' 不是这样。 |
| [DSH7222](DSH7xxx.md#dsh7222) | error | '@sampler' 后面需要一个采样器类型，例如 'Color'、'Normal' 或 'LinearColor'。 |
| [DSH7223](DSH7xxx.md#dsh7223) | error | '@static' 请求静态开关，只对 'uniform bool' 有意义。 |
| [DSH7224](DSH7xxx.md#dsh7224) | warning | '@{0}' 在这里没有意义；它属于 {1}。 |
| [DSH7225](DSH7xxx.md#dsh7225) | warning | '@static {0}' 未指定 '{1}' 的参数。 |
| [DSH7226](DSH7xxx.md#dsh7226) | warning | '@custom {0}' 不是此语言已知的修饰符；唯一的修饰符是 'selfcontained'。 |
| [DSH7227](DSH7xxx.md#dsh7227) | error | '@{0}' 后面需要一个值。 |
| [DSH7228](DSH7xxx.md#dsh7228) | warning | 对 '{0}' 使用 '@custom' 未能使其主体不透明；该指令必须位于紧邻其上方的 '///' 块中… |
| [DSH7229](DSH7xxx.md#dsh7229) | warning | '@{0}' 在此块中写了两次；最后一个生效。 |
| [DSH7230](DSH7xxx.md#dsh7230) | warning | '#pragma layout' 需要 '{0}' 为整数；'{1}' 被忽略。 |
| [DSH7231](DSH7xxx.md#dsh7231) | error | '@static {0}' 将参数设为静态布尔引脚，只有 'bool' 输入可以这样，而 '{0}' 是 {1}。 |
| [DSH7232](DSH7xxx.md#dsh7232) | error | 'Substrate = {0}' 不是 Substrate 模式；模式有 'Legacy'、'Bridge' 和 'Native'。 |
| [DSH7233](DSH7xxx.md#dsh7233) | warning | 滑块范围（'@slider'，或 1.x 属性中的 'Slider(min, max)'）用于标量参数；而这个是 … |
| [DSH7234](DSH7xxx.md#dsh7234) | warning | '{0}' 请求生成 '{1}'，而只有包的头部会生成材质函数；此文件不在… |
| [DSH7235](DSH7xxx.md#dsh7235) | warning | '{0}' 中的 '#pragma nogeneration' 被忽略：它标记其所在文件，而头部不构建任何内容，… |
| [DSH7236](DSH7xxx.md#dsh7236) | warning | 'disable_warning' 或 'disable_error' 列表未指定任何代码，因此不会禁用任何内容；请在其后写代码，… |
| [DSH7250](DSH7xxx.md#dsh7250) | error | 一个 '.dsi' 需要一个 '#pragma instance(Parent = "...")' 来指定它是其实例的材质，而此文件… |
| [DSH7251](DSH7xxx.md#dsh7251) | error | '#pragma instance' 被写了第二次，而一个 '.dsi' 就是一个材质实例；第 {0} 行已经 … |
| [DSH7252](DSH7xxx.md#dsh7252) | error | '#pragma instance' 中的 'Parent' 指定它是其实例的材质，而它是空的。 |
| [DSH7253](DSH7xxx.md#dsh7253) | error | '{0}' 被 '#pragma instance' 设置了两次；它已在第 {1} 行设置过。 |
| [DSH7254](DSH7xxx.md#dsh7254) | error | '.dsi' 由 '#pragma instance' 配置，而 '#pragma material' 配置材质；请把这些键… |
| [DSH7255](DSH7xxx.md#dsh7255) | error | '#pragma instance' 声明材质实例，应放在它自己的 '.dsi' 文件中，而此行在… |
| [DSH7256](DSH7xxx.md#dsh7256) | 错误 | '{0}' 覆盖了一个 {1} 参数，并且需要它所设置的值，即 'uniform {2} {0} = ...;'，并且它还有 n... |
| [DSH7257](DSH7xxx.md#dsh7257) | 错误 | '{0}' 覆盖了一个 {1} 参数，该参数接受的是资源而不是 HLSL 值；请写 '/// @default /Game/... |
| [DSH7258](DSH7xxx.md#dsh7258) | 错误 | '{0}' 不是父级 '{1}' 的参数。 |
| [DSH7259](DSH7xxx.md#dsh7259) | 错误 | '{0}' 是父级中类型为 '{2}' 的 {1} 参数，而此覆盖声明的是 '{3}'。 |
| [DSH7260](DSH7xxx.md#dsh7260) | 错误 | 实例覆盖上的 '/// @static' 表示静态开关（'uniform bool'）或静态组件掩码（'u... |
| [DSH7261](DSH7xxx.md#dsh7261) | 错误 | '{0}' 被第二次覆盖，而一个实例只设置一次参数；第 {1} 行上的覆盖已经... |
| [DSH7262](DSH7xxx.md#dsh7262) | 警告 | '@{0}' 在 '#pragma instance' 之上没有任何效果，那里只读取 '@name'；请删除它。 |
| [DSH7263](DSH7xxx.md#dsh7263) | 信息 | 此处无法获得父级的参数，因此这些覆盖的名称和类型会被检查... |
| [DSH7264](DSH7xxx.md#dsh7264) | 错误 | '{0}' 仅大小写与父级参数 '{1}' 匹配，而 DreamShader 名称区分大小写；请写 '{1}'。 |
| [DSH7265](DSH7xxx.md#dsh7265) | 错误 | '{0}' 被设置为编译器可以折叠的值，即字面量或字面量表达式，而此初始化... |
| [DSH7266](DSH7xxx.md#dsh7266) | 错误 | '@page' 接受一个大于等于零的整数，而 '{0}' 不是。 |
| [DSH7267](DSH7xxx.md#dsh7267) | 警告 | '#pragma {0}' 用于框选或放置图表节点，而 '.dsi' 没有图表；该行已被忽略。 |
| [DSH7268](DSH7xxx.md#dsh7268) | 错误 | '{0}' 在父级中仅作为层参数或混合参数存在，而 '.dsi' 仅覆盖全局参数。 |
| [DSH7269](DSH7xxx.md#dsh7269) | 错误 | '{0}' 是数组，而实例覆盖只赋值一个参数；请在各自的...上覆盖每个元素的参数 |
| [DSH7270](DSH7xxx.md#dsh7270) | 错误 | '#pragma instance' 接受 'Key = Value' 对，而 '{0}' 没有键。 |
| [DSH7300](DSH7xxx.md#dsh7300) | 错误 | '{0}' 不是 '#pragma pipeline' 的键；其键为 {1}。 |
| [DSH7301](DSH7xxx.md#dsh7301) | 错误 | '{0}' 被 '#pragma pipeline' 设置了两次；它已在第 {1} 行设置。 |
| [DSH7302](DSH7xxx.md#dsh7302) | 错误 | （在运行时构建） |
| [DSH7303](DSH7xxx.md#dsh7303) | 错误 | '{0}' 不是注入点；这些点是 {1}。 |
| [DSH7304](DSH7xxx.md#dsh7304) | 错误 | '{0}' 不是通道类型；通道为 {1}。 |
| [DSH7305](DSH7xxx.md#dsh7305) | 错误 | '{0}' 不是缓冲区格式；这些格式为 {1}。 |
| [DSH7306](DSH7xxx.md#dsh7306) | 错误 | '{0}' 不是缓冲区的键；这些键为 {1}。 |
| [DSH7307](DSH7xxx.md#dsh7307) | 错误 | '{0}' 为缓冲区 '{1}' 设置了两次；它已在第 {2} 行设置。 |
| [DSH7308](DSH7xxx.md#dsh7308) | 错误 | （在运行时构建） |
| [DSH7309](DSH7xxx.md#dsh7309) | 错误 | 'Size' 为缓冲区 '{0}' 指定了固定大小，因此 '{1}' 无事可做；请删除其中之一。 |
| [DSH7310](DSH7xxx.md#dsh7310) | 错误 | '{0}' 不是 {1} 通道的键；其键为 {2}。 |
| [DSH7311](DSH7xxx.md#dsh7311) | 错误 | '{0}' 是 {1} 通道的键，而 '{2}' 是 {3} 通道。 |
| [DSH7312](DSH7xxx.md#dsh7312) | 错误 | '{0}' 在通道 '{1}' 中设置了两次；它已在第 {2} 行设置。 |
| [DSH7313](DSH7xxx.md#dsh7313) | 错误 | （在运行时构建） |
| [DSH7314](DSH7xxx.md#dsh7314) | 错误 | 过滤器只使用 '\|'（任一）和 '&'（两者）连接其条件，不包含其他内容。 |
| [DSH7315](DSH7xxx.md#dsh7315) | 错误 | 网格通道 '{0}' 需要 'Filter = ...' 来指定它绘制哪些图元：'Stencil(1)'、'Layer(Name)'、'List(N... |
| [DSH7316](DSH7xxx.md#dsh7316) | 错误 | 全屏通道 '{0}' 绘制 'Material' 或运行 'Shader'，但它同时指定了两者。 |
| [DSH7317](DSH7xxx.md#dsh7317) | 错误 | 全屏材质通道只写入一个缓冲区，写作 'write Buffer;'，而 '{0}' 写入了 {1}。 |
| [DSH7318](DSH7xxx.md#dsh7318) | 错误 | '{0}.Previous' 是上一帧的内容，现在已不再有内容写入；请写 '{0}'。 |
| [DSH7319](DSH7xxx.md#dsh7319) | 错误 | '{0}' 在通道 '{1}' 中绑定了两次；在通道内部，每个输入、输出和参数都有自己的名称。 |
| [DSH7320](DSH7xxx.md#dsh7320) | 错误 | '{0}' 是数组，而管线没有数组参数或常量；请为每个元素声明一个。 |
| [DSH7321](DSH7xxx.md#dsh7321) | 错误 | '{0}' 是整数格式，目前没有任何通道能读取或写入它：HLSL 通道看到的是 float4 纹理，而材质... |
| [DSH7322](DSH7xxx.md#dsh7322) | 错误 | 'Enabled' 为 false，因此这永远不会运行，而管线资源没有始终关闭的开关；请驱动它... |
| [DSH7325](DSH7xxx.md#dsh7325) | 错误 | 全屏材质通道需要场景纹理，而在 {0} 处这些尚不存在；请在 AfterBaseP... 运行 '{1}' |
| [DSH7326](DSH7xxx.md#dsh7326) | 信息 | 在 AfterBasePass，场景颜色仅包含自发光光照；此时尚无任何光照，因此通道 '{0}' {1} 这一点。 |
| [DSH7327](DSH7xxx.md#dsh7327) | 错误 | 'CustomStencil' 是自定义深度纹理的模板部分，没有任何通道能将其作为纹理绑定... |
| [DSH7328](DSH7xxx.md#dsh7328) | 错误 | （在运行时构建） |
| [DSH7329](DSH7xxx.md#dsh7329) | 信息 | 仅当 r.CustomDepth.Order 在基础通道之前绘制自定义深度时，'{0}' 才会在 AfterBasePass 处存在；其中... |
| [DSH7330](DSH7xxx.md#dsh7330) | 错误 | 'Depth = Own({0})' 针对此管线自身的深度进行测试，而 '{0}' 是内置纹理；请声明 '... |
| [DSH7331](DSH7xxx.md#dsh7331) | 错误 | 通道 '{0}' 读取 '{1}'，但没有任何通道写入它，而 '{1}' 是 'Clear = None'，因此它读取的内容未定义。 |
| [DSH7332](DSH7xxx.md#dsh7332) | 警告 | 通道 '{0}' 在帧中 '{2}' 写入 '{1}' 之前就读取了它，因此它读取的是缓冲区的 'Clear' 值；请将... |
| [DSH7333](DSH7xxx.md#dsh7333) | 错误 | 通道 '{0}' 读取并写入 '{1}'，而一个通道不能将同一纹理同时作为其输入和输出；请写 ... |
| [DSH7334](DSH7xxx.md#dsh7334) | 错误 | '{0}' 没有读取 '{1}' 的任何 UserSceneTexture：其输入为 {2}。 |
| [DSH7335](DSH7xxx.md#dsh7335) | 警告 | '{0}' 读取了 UserSceneTexture '{1}'，而通道 '{2}' 未向其绑定任何内容，因此它会采样到黑色；请添加 'read ... |
| [DSH7336](DSH7xxx.md#dsh7336) | 错误 | 全屏材质通道 '{0}' 读取 {1} 缓冲区，而后处理材质具有 {2} 个输入槽，共享 w... |
| [DSH7337](DSH7xxx.md#dsh7337) | 错误 | '{0}' 是 {1} 材质，而全屏通道绘制的是后处理材质：'#pragma material(Domain = PostPro... |
| [DSH7338](DSH7xxx.md#dsh7338) | 错误 | 通道 '{0}' 写入数据缓冲区，而 '{1}' 会按照曝光缩放其读取和写入的内容；请为其提供 '#pragm... |
| [DSH7339](DSH7xxx.md#dsh7339) | 警告 | '{0}' 是为 BlendableLocation {1} 编译的，而通道 '{2}' 在 {3} 处运行它，{4} 色调映射，因此其颜色... |
| [DSH7340](DSH7xxx.md#dsh7340) | 错误 | '{0}' 的图中没有 UE.DreamPassOutput，因此网格通道没有可写入的内容；请添加 'UE.DreamPassOutput(... |
| [DSH7341](DSH7xxx.md#dsh7341) | 错误 | '{0}' 将其 UE.DreamPassOutput 的 Output{1} 留为未连接，因此 '{2}' 将接收不到任何内容。 |
| [DSH7342](DSH7xxx.md#dsh7342) | 错误 | 网格通道 '{0}' 绘制 {1} 图元，而 '{2}' 未针对它们编译；请为该材质设置其用法标志 ... |
| [DSH7343](DSH7xxx.md#dsh7343) | 错误 | 网格通道 '{0}' 将 '{1}'（场景的一张纹理）与 '{2}'（管道的一个缓冲区）一起写入。网格... |
| [DSH7344](DSH7xxx.md#dsh7344) | 错误 | '{0}' 是 {1} 材质，而网格通道使用表面材质绘制图元。 |
| [DSH7345](DSH7xxx.md#dsh7345) | 错误 | '{0}' 请求新的材质转换器，而 UE.DreamPassOutput 不支持它；请移除 'bEnableNewHL... |
| [DSH7346](DSH7xxx.md#dsh7346) | 错误 | 全屏通道 '{0}' 读取 {1} 并写入 {2} 缓冲区，而 '.usf' 通道运行所在的像素槽在... 中具有 {3} |
| [DSH7347](DSH7xxx.md#dsh7347) | 错误 | '{0}' 是 {1}，而着色器槽的参数块只保存数字，因此没有纹理参数能到达... |
| [DSH7350](DSH7xxx.md#dsh7350) | 错误 | 通道 '{0}' 写入场景颜色，它在 {2} 处为 {1}，并连同写入 '{3}'，其为 {4}；一起绘制的目标... |
| [DSH7351](DSH7xxx.md#dsh7351) | 错误 | 通道 '{0}' 替换了色调映射器，'{1}' 也是如此；一个视图运行一个色调映射器，因此一个管道替换... |
| [DSH7352](DSH7xxx.md#dsh7352) | 错误 | 'param' 是管道的一个参数（乘以一个数字，再加上一个数字）、'DreamPassWeight'（同样如此），或 ... |
| [DSH7353](DSH7xxx.md#dsh7353) | 错误 | 在 'param' 中缩放或偏移参数的是编译器可以折叠的一个数字；向量或值 k... |
| [DSH7354](DSH7xxx.md#dsh7354) | 信息 | 缓冲区 '{0}' 被导出，最后一次写入于 {1}：采样它的不透明与半透明材质会看到 p... |
| [DSH7355](DSH7xxx.md#dsh7355) | 错误 | 缓冲区 '{0}' 被导出，而导出的缓冲区会成为材质采样的渲染目标资产，这... |
| [DSH7356](DSH7xxx.md#dsh7356) | 警告 | 没有通道写入缓冲区 '{0}'，因此读取它的人读取的是其 'Clear' 值。 |
| [DSH7357](DSH7xxx.md#dsh7357) | 警告 | 没有通道读取缓冲区 '{0}' 且它未被导出，因此写入它是徒劳的工作；请读取它、导出它，或 rem... |
| [DSH7359](DSH7xxx.md#dsh7359) | 警告 | 'Mode = Own' 使用每个图元自己的材质来绘制它，因此网格通道 '{0}' 的 'Material' 永远不会被使用。 |
| [DSH7360](DSH7xxx.md#dsh7360) | 信息 | 该管道指定的材质、着色器文件和通道层在此处不可用，因此它们取自 ... |
| [DSH7361](DSH7xxx.md#dsh7361) | 错误 | 通道 '{0}' 包含第二个 'hlsl' 块，而一个通道的代码只有一个块；位于第 {1} 行的那个就是它。 |
| [DSH7362](DSH7xxx.md#dsh7362) | 错误 | 此文件在第 {0} 行已有一个 'hlsl' 块，而一个 '.dsp' 只有一个：请将每个共享函数和 e... 都写入 |
| [DSH7364](DSH7xxx.md#dsh7364) | 错误 | 通道 '{0}' 运行着色器文件 '{1}'，同时还包含一个 'hlsl' 块，而一个通道的代码位于一个 p... |
| [DSH7365](DSH7xxx.md#dsh7365) | 错误 | 通道 '{0}' 的 'hlsl' 块包含其入口的语句，而该函数由编译器编写，因此 'E... |
| [DSH7366](DSH7xxx.md#dsh7366) | 错误 | '#include' 不能位于函数的语句之间，而通道 '{0}' 的 'hlsl' 块包含 st... |
| [DSH7367](DSH7xxx.md#dsh7367) | 错误 | '{0}' 是通道 '{1}' 的名称，而其 'hlsl' 块包含编译器编写的函数的语句，... |
| [DSH7368](DSH7xxx.md#dsh7368) | 错误 | '{0}' 位于文件 'hlsl' 块的共享代码中，同时也是通道 '{1}' 的入口，而 p... |
| [DSH7369](DSH7xxx.md#dsh7369) | 错误 | '{0}' 是文件中 'hlsl' 块里通道 '{1}' 的入口，并且在此处被调用；入口只会 ... 编译 |
| [DSH7370](DSH7xxx.md#dsh7370) | 错误 | {0} 通道不运行自己的 HLSL，因此通道 '{1}' 不能包含 'hlsl' 块。 |
| [DSH8088](DSH8xxx.md#dsh8088) | 错误 | %s 包含无效的文件夹段。 |
| [DSH8089](DSH8xxx.md#dsh8089) | 错误 | DreamShader Root '%s' 引用了项目插件 '%s'，但未找到具有该名称的已启用插件。 |
| [DSH8090](DSH8xxx.md#dsh8090) | 错误 | DreamShader Root '%s' 必须引用 '%s' 下的项目插件。 |
| [DSH8091](DSH8xxx.md#dsh8091) | 错误 | DreamShader Root '%s' 引用了项目插件 '%s'，但该插件未启用。 |
| [DSH8092](DSH8xxx.md#dsh8092) | 错误 | DreamShader Root '%s' 引用了项目插件 '%s'，但该插件不能包含内容。 |
| [DSH8093](DSH8xxx.md#dsh8093) | 错误 | DreamShader Root '%s' 引用了项目插件 '%s'，但其 Content 目录不存在：'%s'。 |
| [DSH8094](DSH8xxx.md#dsh8094) | 错误 | DreamShader Root '%s' 引用了项目插件 '%s'，但插件内容未挂载。 |
| [DSH8095](DSH8xxx.md#dsh8095) | 错误 | DreamShader Root '%s' 的插件名称无效。 |
| [DSH8096](DSH8xxx.md#dsh8096) | 错误 | DreamShader Root '%s' 的插件名称无效。 |
| [DSH8097](DSH8xxx.md#dsh8097) | 错误 | DreamShader Root '%s' 的包根无效。 |
| [DSH8098](DSH8xxx.md#dsh8098) | 错误 | DreamShader 资产名称必须解析为非空资产路径。 |
| [DSH8099](DSH8xxx.md#dsh8099) | 错误 | DreamShader 资产名称必须解析为非空资产路径。 |
| [DSH8100](DSH8xxx.md#dsh8100) | error | DreamShader 资产名称 '%s' 生成了无效的资产名称。 |
| [DSH8101](DSH8xxx.md#dsh8101) | error | 资产 '%s' 已在资产编辑器中打开，因此未重新构建。打开的编辑器操作的是自己的副本。 |
| [DSH8102](DSH8xxx.md#dsh8102) | error | 资产 '%s' 已存在，且不是材质。 |
| [DSH8103](DSH8xxx.md#dsh8103) | error | 资产 '%s' 已存在，且不是由 DreamShader 生成的。请重命名你的着色器或移动/删除现有... |
| [DSH8104](DSH8xxx.md#dsh8104) | error | 无法创建包 '%s'。 |
| [DSH8105](DSH8xxx.md#dsh8105) | error | 无法创建材质 '%s'。 |
| [DSH8106](DSH8xxx.md#dsh8106) | error | 资产 '%s' 已存在，且不是 DreamShader 实例材质。请删除它（或移除 Backend="Instanc... |
| [DSH8107](DSH8xxx.md#dsh8107) | error | 资产 '%s' 已存在，且不是由 DreamShader 生成的。请重命名你的着色器或移动/删除现有... |
| [DSH8108](DSH8xxx.md#dsh8108) | error | 无法创建包 '%s'。 |
| [DSH8109](DSH8xxx.md#dsh8109) | error | 无法创建实例材质 '%s'。 |
| [DSH8110](DSH8xxx.md#dsh8110) | error | 资产 '%s' 已作为 '%s' 存在，但 %s 生成需要 '%s'。请删除或移动现有资产，然后... |
| [DSH8111](DSH8xxx.md#dsh8111) | error | 资产 '%s' 已存在，且不是材质函数资产。 |
| [DSH8112](DSH8xxx.md#dsh8112) | error | 资产 '%s' 已存在，且不是由 DreamShader 生成的。请重命名你的函数或移动/删除现有... |
| [DSH8113](DSH8xxx.md#dsh8113) | error | 无法创建包 '%s'。 |
| [DSH8114](DSH8xxx.md#dsh8114) | error | 无法创建材质函数 '%s'。 |
| [DSH8115](DSH8xxx.md#dsh8115) | error | 资产 '%s' 自 DreamShader 从 '%s' 生成以来已被手动编辑，因此未重新构建（重新构建将... |
| [DSH8116](DSH8xxx.md#dsh8116) | error | 生成的 DreamShader 资产 '%s' 无法保存。 |
| [DSH8117](DSH8xxx.md#dsh8117) | error | 生成的 DreamShader 资产包无法保存。%s |
| [DSH8118](DSH8xxx.md#dsh8118) | error | 资产路径根 '%s' 引用了插件 '%s'，但未找到具有该名称的已启用插件。 |
| [DSH8119](DSH8xxx.md#dsh8119) | error | 资产路径根 '%s' 引用了插件 '%s'，但该插件未启用。 |
| [DSH8120](DSH8xxx.md#dsh8120) | error | 资产路径根 '%s' 引用了插件 '%s'，但该插件无法包含内容。 |
| [DSH8121](DSH8xxx.md#dsh8121) | error | 资产路径根 '%s' 引用了插件 '%s'，但其 Content 目录不存在：'%s'。 |
| [DSH8122](DSH8xxx.md#dsh8122) | error | 资产路径根 '%s' 引用了插件 '%s'，但该插件内容未挂载。 |
| [DSH8123](DSH8xxx.md#dsh8123) | error | 相对资产 Path(...) 引用需要一个根，例如 Game、Engine 或 Plugin.PluginName。 |
| [DSH8124](DSH8xxx.md#dsh8124) | error | 资产路径根 '%s' 的插件名称无效。 |
| [DSH8125](DSH8xxx.md#dsh8125) | error | 资产路径根 '%s' 的插件名称无效。 |
| [DSH8126](DSH8xxx.md#dsh8126) | error | 不支持的资产路径根 '%s'。请使用 Game、Engine 或 Plugin.PluginName。 |
| [DSH8127](DSH8xxx.md#dsh8127) | error | 资产引用不能为空。 |
| [DSH8128](DSH8xxx.md#dsh8128) | error | 资产 Path(...) 引用缺少右括号 ')'。 |
| [DSH8129](DSH8xxx.md#dsh8129) | error | 资产 Path(...) 包含未终止的字符串字面量。 |
| [DSH8130](DSH8xxx.md#dsh8130) | error | 资产 Path(...) 需要 1 个参数（/Game/... 路径）或 2 个参数（Game\|Engine\|Plugin.PluginName，以... |
| [DSH8131](DSH8xxx.md#dsh8131) | error | 资产引用需要一个非空路径。 |
| [DSH8132](DSH8xxx.md#dsh8132) | error | 无效的资产路径 '%s'。 |
| [DSH8133](DSH8xxx.md#dsh8133) | error | DSH8133：'{0}' 由 '{1}' 构建，而后者是已安装包的文件，该包的安装程序会替换... |
| [DSH8149](DSH8xxx.md#dsh8149) | error | DSH8149：'{0}' 使用了条件编译，而 '{1}' 仅保留了实际采用的分支——采用它... |
| [DSH8155](DSH8xxx.md#dsh8155) | warning | 从 '%s' 重新构建 '%s' 时丢弃了 %d 个重建后的材质不再声明的参数覆盖：%s。 |
| [DSH8200](DSH8xxx.md#dsh8200) | error | '{0}' 未解析为有效的资产路径。{1} |
| [DSH8201](DSH8xxx.md#dsh8201) | error | 无法为 '{0}' 创建或复用材质。{1} |
| [DSH8202](DSH8xxx.md#dsh8202) | error | 无法为 '{0}' 创建或复用材质函数。{1} |
| [DSH8203](DSH8xxx.md#dsh8203) | error | 无法为 '{0}' 创建或复用 ThinCustom 实例。{1} |
| [DSH8204](DSH8xxx.md#dsh8204) | error | 发射器需要前端所绑定的内置目录，但发射上下文未携带该目录。 |
| [DSH8205](DSH8xxx.md#dsh8205) | error | 产品索引 {0} 不存在于本模块中，本模块拥有 {1}。 |
| [DSH8206](DSH8xxx.md#dsh8206) | error | '{0}' 已在资产编辑器中打开，因此未重新构建。{1} |
| [DSH8207](DSH8xxx.md#dsh8207) | error | '{0}' 不再包含 DreamShader 生成到其中的内容，因此未重新构建。{1} |
| [DSH8208](DSH8xxx.md#dsh8208) | warning | 在重新构建 '{0}' 之前无法为其创建快照，因此重新构建失败将不会回滚。 |
| [DSH8209](DSH8xxx.md#dsh8209) | info | '{0}' 保持原样：另一个编辑器负责将本工程生成的资产写入磁盘。 |
| [DSH8210](DSH8xxx.md#dsh8210) | warning | '{0}' 没有名为 '{1}' 的属性，因此该值未被写入。 |
| [DSH8211](DSH8xxx.md#dsh8211) | error | '{0}' 不是此引擎拥有的材质表达式类。 |
| [DSH8212](DSH8xxx.md#dsh8212) | error | MakeMaterialAttributes 没有用于属性 '{0}' 的引脚。 |
| [DSH8213](DSH8xxx.md#dsh8213) | error | '{0}' 没有名为 '{1}' 的属性。 |
| [DSH8214](DSH8xxx.md#dsh8214) | error | 无法创建 FunctionInput 节点。 |
| [DSH8215](DSH8xxx.md#dsh8215) | error | '{0}' 上的某项材质设置被拒绝。{1} |
| [DSH8216](DSH8xxx.md#dsh8216) | error | BreakMaterialAttributes 未发布属性 '{0}'，因此无法从来自...的材质中读取 |
| [DSH8217](DSH8xxx.md#dsh8217) | error | 此材质没有属性 '{0}' 的输入；请检查文件所...的材质域和着色模型 |
| [DSH8218](DSH8xxx.md#dsh8218) | error | 无法从 '{1}' 加载参数 '{0}' 的默认纹理。 |
| [DSH8219](DSH8xxx.md#dsh8219) | error | 此函数调用未指定任何材质函数资产。 |
| [DSH8220](DSH8xxx.md#dsh8220) | error | '{0}' 没有名为 '{1}' 的输入。 |
| [DSH8221](DSH8xxx.md#dsh8221) | error | '{0}' 没有名为 '{1}' 的输出。 |
| [DSH8222](DSH8xxx.md#dsh8222) | 错误 | 此调用针对同一文件的产物 {0}，但该产物尚未发出；管线必须... |
| [DSH8223](DSH8xxx.md#dsh8223) | 错误 | 发射器没有针对 IR 操作 '{0}' 的规则。 |
| [DSH8224](DSH8xxx.md#dsh8224) | 错误 | SetMaterialAttributes 节点没有 MaterialAttributes 输入；它没有任何可以修改的内容。 |
| [DSH8225](DSH8xxx.md#dsh8225) | 错误 | 此节点读取节点 {0}，而该节点尚未发出；图的拓扑顺序不一致。 |
| [DSH8226](DSH8xxx.md#dsh8226) | 错误 | Swizzle 节点不携带 Mask 属性。 |
| [DSH8227](DSH8xxx.md#dsh8227) | 错误 | '{0}' 不是 Custom 节点的输出类型；应为 Float1 到 Float4 或 MaterialAttributes。 |
| [DSH8228](DSH8xxx.md#dsh8228) | 错误 | '{0}' 不是材质函数输入类型；请写入 Scalar、Vector2、Vector3、Vector4、Texture2D、Tex... 之一 |
| [DSH8229](DSH8xxx.md#dsh8229) | 错误 | '{0}' 已构建，但无法保存。{1} |
| [DSH8230](DSH8xxx.md#dsh8230) | 错误 | 无法在没有实例的情况下创建 ThinCustom 基础材质。 |
| [DSH8232](DSH8xxx.md#dsh8232) | 错误 | '{0}' 的产物种类是发射器不知道如何具体化的。 |
| [DSH8233](DSH8xxx.md#dsh8233) | 错误 | 此图携带 MaterialSink，只有材质产物才具有此内容；材质函数驱动 FunctionOu... |
| [DSH8234](DSH8xxx.md#dsh8234) | 错误 | 函数输出 '{0}' 有 {1} 个操作数；它需要正好等于它所返回的那一个值。 |
| [DSH8235](DSH8xxx.md#dsh8235) | 错误 | '{0}' 不是采样器类型；请写入 EMaterialSamplerType 名称之一，例如 Color、Normal 或 LinearCo... |
| [DSH8237](DSH8xxx.md#dsh8237) | 信息 | '{0}' 未被改动：其源哈希自上次构建以来未发生变化。 |
| [DSH8240](DSH8xxx.md#dsh8240) | 错误 | 无法为 '{0}' 创建或复用材质实例。{1} |
| [DSH8241](DSH8xxx.md#dsh8241) | 错误 | 资产 '%s' 已作为 '%s' 存在；.dsi 构建的是普通 MaterialInstanceConstant。请重命名实例文... |
| [DSH8242](DSH8xxx.md#dsh8242) | 错误 | 资产 '%s' 已存在，且不是由 DreamShader 生成的。请重命名实例文件，或移动/删除... |
| [DSH8243](DSH8xxx.md#dsh8243) | 错误 | '{1}' 的父级 '{0}' 无法加载；请编译构建它的源文件，或修正 Parent 键。 |
| [DSH8244](DSH8xxx.md#dsh8244) | 信息 | '{0}' 仅存在于内存中，因此先将其保存到磁盘：'{1}' 将其作为父级。 |
| [DSH8245](DSH8xxx.md#dsh8245) | 错误 | '{0}' 无法将 '{1}' 作为父级：该父级已经从此实例派生。 |
| [DSH8246](DSH8xxx.md#dsh8246) | 错误 | 父资产 '{0}' 没有此实例所覆盖种类的参数 {1}；该资产比这... 更旧 |
| [DSH8247](DSH8xxx.md#dsh8247) | 错误 | 引擎在应用静态覆盖时丢弃了 '{0}' 作为 '{1}' 的父级；该父级不... |
| [DSH8248](DSH8xxx.md#dsh8248) | 错误 | 父级 '{0}' 仅存在于内存中，且没有 DreamShader 源文件构建它，因此无法将 '{1}' 保存为... |
| [DSH8249](DSH8xxx.md#dsh8249) | 错误 | '{0}' 不是实例键；实例键是实例可以覆盖的材质属性，例如 BlendMode、Two... |
| [DSH8250](DSH8xxx.md#dsh8250) | 错误 | '{0}' 不是实例键 '{1}' 的有效值。{2} |
| [DSH8251](DSH8xxx.md#dsh8251) | 错误 | '{0}' 无法从实例文件设置：{1} |
| [DSH8252](DSH8xxx.md#dsh8252) | 错误 | 无法应用对 '{0}' 的覆盖：{1} |
| [DSH8253](DSH8xxx.md#dsh8253) | 错误 | 无法创建包 '%s'。 |
| [DSH8254](DSH8xxx.md#dsh8254) | 错误 | 材质参数集 '{0}' 没有名为 '{1}' 的参数。 |
| [DSH8260](DSH8xxx.md#dsh8260) | 错误 | 父源 '{0}' 编译失败，因此此实例没有可用于构建的父级。{1} |
| [DSH8261](DSH8xxx.md#dsh8261) | 错误 | '{1}' 下的源文件未构建名为 '{0}' 的材质或实例；请写入父级的资产路径，或检查... |
| [DSH8262](DSH8xxx.md#dsh8262) | 错误 | '{0}' 在 '{1}' ({2}) 下命名了多个产物；请改为写入父级的资产路径。 |
| [DSH8263](DSH8xxx.md#dsh8263) | 错误 | '{0}' 是它自己的祖先：从它跟踪 Parent 会回到它自身 ({1})。 |
| [DSH8264](DSH8xxx.md#dsh8264) | 信息 | '{0}' 缺失或比其源文件更旧，因此先编译了 '{1}'。 |
| [DSH8265](DSH8xxx.md#dsh8265) | 错误 | '{0}' 上方的 Parent 链深度超过 {1} 个实例；如此长的链几乎总是错误... |
| [DSH8270](DSH8xxx.md#dsh8270) | 错误 | 函数引用 '{0}' 未解析为资产路径。{1} |
| [DSH8271](DSH8xxx.md#dsh8271) | 错误 | 参数 '{1}' 的默认纹理引用 '{0}' 未解析为资产路径。{2} |
| [DSH8273](DSH8xxx.md#dsh8273) | 错误 | '{0}' 持有占位符 '`{Name`}' 之外的占位符，而后者是 generate 模板唯一拥有的占位符。 |
| [DSH8274](DSH8xxx.md#dsh8274) | 错误 | '{0}' 无法作为材质函数 '{1}' 生成：{2}。 |
| [DSH8275](DSH8xxx.md#dsh8275) | 错误 | '{0}' 在此处第二次生成；第 {1} 行已经从另一个函数生成了它。 |
| [DSH8276](DSH8xxx.md#dsh8276) | 警告 | '{0}' 不是 '#pragma generate' 的键，已被忽略；它接受 Functions、Library 和 Desc。 |
| [DSH8277](DSH8xxx.md#dsh8277) | 信息 | '{0}' 未被改动：'{1}' 将其标记为 NoGeneration，只有指定该文件的编译才会构建它。 |
| [DSH8278](DSH8xxx.md#dsh8278) | 警告 | '{0}' 从未被构建：'{1}' 将其标记为 NoGeneration，因此只有指定该文件的编译才会构建它，... |
| [DSH8279](DSH8xxx.md#dsh8279) | 错误 | 父级 '{0}' 来自 '{1}'，后者将其标记为 NoGeneration，且它从未被构建，因此没有... |
| [DSH8290](DSH8xxx.md#dsh8290) | 错误 | 无法读取 '{0}'。 |
| [DSH8291](DSH8xxx.md#dsh8291) | 错误 | '{0}' 未能通过条件编译：{1}：{2} |
| [DSH8292](DSH8xxx.md#dsh8292) | 错误 | 从 '{1}' 包含的 '{0}' 无法解析：{2}。 |
| [DSH8293](DSH8xxx.md#dsh8293) | 错误 | 从 '{1}' 包含的 '{0}' 已解析但无法读取。 |
| [DSH8294](DSH8xxx.md#dsh8294) | error | '{0}'（包含自 '{1}'）无法解析；其自身的错误在上方。 |
| [DSH8295](DSH8xxx.md#dsh8295) | error | '{0}' 不是 DreamShader 头文件；include 引用的应是 '.dsh'（或 '.dss'），而不是 '{1}' 文件。 |
| [DSH8296](DSH8xxx.md#dsh8296) | error | '{0}' 不是编译器可自行构建的源文件；它构建 '.dss'、'.dsi'、'.dsp'、'.dsm' 和 '.dsf' 文... |
| [DSH8297](DSH8xxx.md#dsh8297) | error | 内置表达式目录返回为空，因此任何引用 'UE.*' 节点的内容都无法绑定。反射... |
| [DSH8298](DSH8xxx.md#dsh8298) | error | 构建 '{0}' 已被取消；该资产保持本次编译之前的状态。 |
| [DSH8299](DSH8xxx.md#dsh8299) | error | 导出的函数 {0} 之间存在循环调用，因此无法确定它们的构建顺序；... |
| [DSH8300](DSH8xxx.md#dsh8300) | error | '{0}' 是 Custom Pass 管线，需要 Unreal Engine 5.8 或更高版本；此引擎的 DreamShaderPas... |
| [DSH8301](DSH8xxx.md#dsh8301) | error | 创建包 '%s' 失败。 |
| [DSH8302](DSH8xxx.md#dsh8302) | error | 资产 '%s' 已作为 '%s' 存在；.dsp 会构建 DreamPassPipeline。请重命名该 .dsp 或移动现有的... |
| [DSH8303](DSH8xxx.md#dsh8303) | error | 资产 '%s' 已存在且不是由 DreamShader 生成的。请重命名该 .dsp 或移动/删除现有的... |
| [DSH8304](DSH8xxx.md#dsh8304) | error | '{1}' 不是 Custom Pass 运行时已知的 {0}；该管线未被构建。这是编译器缺口：... |
| [DSH8305](DSH8xxx.md#dsh8305) | error | 通道 '{1}' 的材质 '{0}' 无法加载；请编译构建它的源文件，或修正 Material ... |
| [DSH8306](DSH8xxx.md#dsh8306) | error | 无法应用 '{0}' 的默认值：{1}。 |
| [DSH8307](DSH8xxx.md#dsh8307) | error | 通道 '{0}' 选择的层 '{1}' 不是项目的通道层之一（Project Settings > Dream... |
| [DSH8308](DSH8xxx.md#dsh8308) | error | 缓冲区 '{0}' 是 {1}，无法导出：材质、蓝图和 UMG 会将导出的缓冲区读取为浮点... |
| [DSH8309](DSH8xxx.md#dsh8309) | error | 无法创建或复用导出缓冲区 '{0}' 的渲染目标。{1} |
| [DSH8310](DSH8xxx.md#dsh8310) | warning | '{0}' 被保留在原处，尽管其缓冲区已不再导出：{1}。在没有任何地方读取它后，请手动删除... |
| [DSH8311](DSH8xxx.md#dsh8311) | error | '{0}' 已构建，但无法连同其渲染目标一起保存；其槽位已在注册表中。请... |
| [DSH8312](DSH8xxx.md#dsh8312) | info | '{0}' 已删除：其缓冲区已不再导出。 |
| [DSH8313](DSH8xxx.md#dsh8313) | error | 资产 '%s' 已作为 '%s' 存在；导出的缓冲区需要在那里有一个 TextureRenderTarget2D。请移动现有的... |
| [DSH8314](DSH8xxx.md#dsh8314) | error | 创建包 '%s' 失败。 |
| [DSH8315](DSH8xxx.md#dsh8315) | error | 无法读取 Custom Pass 槽位注册表：{0}。未写入任何内容，因为覆盖它会丢失... |
| [DSH8316](DSH8xxx.md#dsh8316) | error | 通道 '{0}' 需要一个 {1} 槽位，而所有 {2} 均已被占用。请合并通道，运行 'dsc pass-registry -Gc' 以释放槽... |
| [DSH8317](DSH8xxx.md#dsh8317) | error | 通道 '{0}' 无法映射到其 HLSL 槽位：{1} |
| [DSH8318](DSH8xxx.md#dsh8318) | error | [{0}] 通道 '{1}' 在 BeginView 处运行，此时视图 uniform buffer 尚不存在，而其槽位使用 'Vi... |
| [DSH8319](DSH8xxx.md#dsh8319) | error | 通道 '{0}' 在其 '.dsp' 中的 HLSL 无法为其槽位组合起来（其条目不在... |
| [DSH8320](DSH8xxx.md#dsh8320) | error | '{0}' 通过相对路径包含且未指定任何文件，因此无法构建通道 '{1}' 的快照。 |
| [DSH8321](DSH8xxx.md#dsh8321) | warning | '{0}' 通过 /Engine/、/Plugin/ 和 /ThirdParty/ 之外的虚拟路径包含，因此通道 '{... 的快照 |
| [DSH8322](DSH8xxx.md#dsh8322) | error | [{0}] 通道 '{1}' 在其 HLSL 槽位中无法编译：{2} |
| [DSH8323](DSH8xxx.md#dsh8323) | error | 无法预检查 {0} 槽位着色器：全局着色器类型 {1} 未注册，或其源文件不再... |
| [DSH8324](DSH8xxx.md#dsh8324) | warning | 项目面向着色器格式 {0}，而此机器没有该格式的着色器编译器，因此 HLSL 槽位... |
| [DSH8325](DSH8xxx.md#dsh8325) | error | 没有可用于预检查 HLSL 槽位的着色器格式：没有活动的功能级别，也没有目标平台... |
| [DSH8326](DSH8xxx.md#dsh8326) | error | '{0}' 无法写入或删除，因此槽位注册表未发生任何更改：未写入快照，未... |
| [DSH8327](DSH8xxx.md#dsh8327) | info | {0} 槽位 {1}（通道 '{2}'）已释放：该管线不再在此处以 HLSL 运行该通道。 |
| [DSH8330](DSH8xxx.md#dsh8330) | error | '{0}' 在此源根（{1}）下指向多个材质；请改为写入材质的资产路径。 |
| [DSH8331](DSH8xxx.md#dsh8331) | error | 材质 '{0}' 来自 '{1}'，后者编译失败，因此此管线没有可用于检查其... 的材质 |
| [DSH8332](DSH8xxx.md#dsh8332) | info | '{0}' 缺失或早于其源文件，因此先编译了 '{1}'。 |
| [DSH8333](DSH8xxx.md#dsh8333) | error | '{0}' 由 '{1}' 构建，后者通过 UE.DreamPassBuffer（直接或...）读取此管线的导出缓冲区。 |
| [DSH8334](DSH8xxx.md#dsh8334) | error | 材质 '{0}' 未解析为资产路径。{1} |
| [DSH8335](DSH8xxx.md#dsh8335) | error | 无法读取 Custom Pass 槽位注册表：{0}。未发生任何更改；'dsc pass-registry -Rebuild' 会替换... |
| [DSH8336](DSH8xxx.md#dsh8336) | error | 无法读取 Custom Pass 槽位注册表（{0}），也无法将其移至 '{1}'，因此未重新... |
| [DSH8337](DSH8xxx.md#dsh8337) | warning | {0} 槽位 {1}（{2}，通道 '{3}'）指定的快照文件不在磁盘上，因此它现已被保留，且编译... |
| [DSH8338](DSH8xxx.md#dsh8338) | error | HLSL 槽位需要 Unreal Engine 5.8 或更高版本；此引擎没有可为其预检查的 Custom Pass 运行时。 |
| [DSH8339](DSH8xxx.md#dsh8339) | info | '{0}' 没有 HLSL 通道，因此没有需要预检查的槽位；其材质由构建...的源文件检查 |
| [DSH9001](DSH9xxx.md#dsh9001) | error | DSH9001：'{0}' 使用了条件编译，而 VirtualFunction 同步会按字节就地重写源文件... |
| [DSH9011](DSH9xxx.md#dsh9011) | warning | 为 '%s' 编译着色器耗时 %.0f 秒。如此长时间的停顿几乎总是由某个 Custom 节点造成，它的 l... |
| [DSH9012](DSH9xxx.md#dsh9012) | warning | '%s' 在输入 '%s' 上循环，并使用 '%s' 进行采样，后者从屏幕空间导数获取其 mip 级别... |
| [DSH9020](DSH9xxx.md#dsh9020) | error | 无法将 '%s' 备份到 '%s'；其资产引用仍指向旧路径。 |
| [DSH9021](DSH9xxx.md#dsh9021) | error | 在重命名 '%s' 引用的某个资产后，无法写入 '%s'；该文件原来的内容位于 '%s'。 |
| [DSH9022](DSH9xxx.md#dsh9022) | error | 无法写入 IR 转储：{0}。 |
| [DSH9023](DSH9xxx.md#dsh9023) | error | 无法写入符号索引：{0}。 |
| [DSH9024](DSH9xxx.md#dsh9024) | error | 无法写入内置目录清单：{0}。 |
| [DSH9025](DSH9xxx.md#dsh9025) | error | 内置目录返回为空，因此 '{0}' 完全没有描述任何表达式。反射未找到任何 UMaterial... |
| [DSH9026](DSH9xxx.md#dsh9026) | error | '{0}' 不是此引擎已知的着色器平台。请写入 SM6、SM5、ES3_1，或诸如 PC... 之类的着色器格式名称 |
| [DSH9027](DSH9xxx.md#dsh9027) | error | '{0}' 不是材质质量级别。请写入 Low、Medium、High 或 Epic。 |
| [DSH9028](DSH9xxx.md#dsh9028) | error | 对 '{0}' 的着色器编译未在每个材质 {1} 秒内完成。一个永不完成的编译... |
| [DSH9029](DSH9xxx.md#dsh9029) | error | [{0} / {1}] {2} |
| [DSH9030](DSH9xxx.md#dsh9030) | error | DreamShader 无法创建图表转储目录 '%s'。 |
| [DSH9031](DSH9xxx.md#dsh9031) | error | DreamShader 无法写入图表转储 '%s'。 |
| [DSH9032](DSH9xxx.md#dsh9032) | error | DreamShader 无法确定 '%s' 构建哪些资产，因此没有可转储的图表。 |
| [DSH9033](DSH9xxx.md#dsh9033) | error | DreamShader 在生成后无法从 '%s' 解析生成的资产 '%s'。 |
| [DSH9034](DSH9xxx.md#dsh9034) | error | DreamShader 无法转储 '%s'：%s 不是材质、材质函数、材质实例或通道管线。 |
| [DSH9035](DSH9xxx.md#dsh9035) | error | '{0}' 不是可编译的 DreamShader 源文件（.dss、.dsi、.dsp、.dsm 或 .dsf），因此 '{1}' 无... 可做 |
| [DSH9036](DSH9xxx.md#dsh9036) | error | 无法写入诊断 JSON：{0}。 |
| [DSH9037](DSH9xxx.md#dsh9037) | info | '{0}' 未生成任何材质，因此没有需要编译的着色器；函数库由材质... 检查 |
| [DSH9038](DSH9xxx.md#dsh9038) | warning | 在此配置下无法读有着色器错误：'-nullrhi' 会关闭渲染着色器映射，并且 ... |
| [DSH9039](DSH9xxx.md#dsh9039) | error | %s：DSH9039：DreamShader 2.0 管线失败，但未引发诊断。 |
| [DSH9040](DSH9xxx.md#dsh9040) | error | 无法写入布局转储：{0}。 |
| [DSH9041](DSH9xxx.md#dsh9041) | error | '{0}' 不是布局样式；-Style 接受 Blocks、SourceBands、Layered 或 All。 |
| [DSH9042](DSH9xxx.md#dsh9042) | info | '{0}' 含有 1.x 声明，而打印器为这些声明写出的是 2.0 文本；将 1.x 重写为 2.0 是 'dsc... |
| [DSH9043](DSH9xxx.md#dsh9043) | info | '{0}' 在自定义主体之外使用了预处理器；'fmt' 按磁盘上的原样读取文件，将不得不 ... |
| [DSH9044](DSH9xxx.md#dsh9044) | error | '{0}' 的格式化文本未通过其自身检查 -- {1} -- 因此未写入任何内容。这是格式化器的... 故障 |
| [DSH9045](DSH9xxx.md#dsh9045) | error | 无法读取 '{0}'，因此未对其进行格式化。 |
| [DSH9046](DSH9xxx.md#dsh9046) | error | '{0}' 不在格式化器的布局中；'dsc fmt' 会将其重写。 |
| [DSH9047](DSH9xxx.md#dsh9047) | error | 无法写入生成资产的列表：{0}。 |
| [DSH9048](DSH9xxx.md#dsh9048) | error | '-As={0}' 不是列表格式；四种格式为 Packages、Files、GitIgnore 和 Json。 |
| [DSH9049](DSH9xxx.md#dsh9049) | warning | {0} 个生成的资产位于项目目录之外 -- 属于某个引擎插件的内容 -- 并且没有项目... |
| [DSH9050](DSH9xxx.md#dsh9050) | error | reveal-node 需要一个非空的 'file' 和一个大于等于 1 的 'line'；得到的是文件 '{File}' 和行号 {Line}。 |
| [DSH9051](DSH9xxx.md#dsh9051) | error | 此编辑器中加载的任何资产都不是从 '{File}' 生成的，因此没有可从中显示节点的图表；编译... |
| [DSH9052](DSH9xxx.md#dsh9052) | error | 从 '{File}' 生成的 {Count} 个资产不携带 DreamShader.SourceSpans 元数据，因此它们上的任何节点... |
| [DSH9053](DSH9xxx.md#dsh9053) | error | '{Asset}' 的 DreamShader.SourceSpans 元数据不是 JSON 对象，因此其上的任何节点都无法映射回... |
| [DSH9054](DSH9xxx.md#dsh9054) | error | 从 '{File}' 生成的任何节点在第 {Line} 行都没有源范围；该行未生成图表节点（一个声明... |
| [DSH9055](DSH9xxx.md#dsh9055) | error | 材质编辑器无法为 '{Asset}' 打开，因此无法显示该节点。 |
| [DSH9056](DSH9xxx.md#dsh9056) | error | '{Asset}' 在并非材质编辑器的编辑器中打开，其中没有可供选择的节点图表；只有... |
| [DSH9057](DSH9xxx.md#dsh9057) | error | 为行 {Line} 记录的表达式 {Guid} 已不在 '{Asset}' 中，因此无法选择该节点... |
| [DSH9058](DSH9xxx.md#dsh9058) | error | 生成此节点的源文件 '{File}' 已不在磁盘上，因此无法将其打开；请... |
| [DSH9059](DSH9xxx.md#dsh9059) | warning | '{Asset}' 的 DreamShader.SourceSpans 条目 '{Key}' 不是表达式 GUID，已被跳过；该节点... |
| [DSH9060](DSH9xxx.md#dsh9060) | error | '{0}' 既不是材质也不是材质函数，因此它没有可读取的图表。材质实例解... |
| [DSH9061](DSH9xxx.md#dsh9061) | info | '{1}' 的 {0} 个表达式未向任何输出馈送，不属于源文件的一部分；构建会一直将它们全部剪除... |
| [DSH9062](DSH9xxx.md#dsh9062) | warning | 重路由 {0} 馈送给自身；读取它的引脚被视为未连接。 |
| [DSH9063](DSH9xxx.md#dsh9063) | warning | 命名重路由 {0} 没有声明；读取它的引脚被视为未连接。 |
| [DSH9064](DSH9xxx.md#dsh9064) | warning | {0} 调用了一个缺失的材质函数；从它读取的每个值都以零代替。 |
| [DSH9065](DSH9xxx.md#dsh9065) | warning | {0} 属于内置目录未列出的类（抽象、已弃用，或来自在此之后加载的模块... |
| [DSH9066](DSH9xxx.md#dsh9066) | warning | '{1}' 的输入 '{0}' 是动态 bool 引脚，该语言没有为其提供参数；它被写为 '... |
| [DSH9067](DSH9xxx.md#dsh9067) | 警告 | {0} 带有 {1} 个额外 define，而 '/// @custom' 函数无法声明这些 define；重建后的节点有 n... |
| [DSH9068](DSH9xxx.md#dsh9068) | 警告 | {0} 更改了 '{1}'，而 '///' 指令无法携带该更改；重建后的参数具有默认值。 |
| [DSH9069](DSH9xxx.md#dsh9069) | 信息 | {0} 的 Value 未连接任何内容，因此它只是其 {1} 分支，别无其他；重建后的图表没有 switch t... |
| [DSH9070](DSH9xxx.md#dsh9070) | 警告 | {0} 有 {1} 个已连接的输入，而该类并未将其声明为命名引脚，因此调用无法连接它们；reb... |
| [DSH9071](DSH9xxx.md#dsh9071) | 警告 | '{1}' 的输入 '{0}' 的 Preview 引脚已连接。源文件将其表示为默认值，那是一个表达式（'f... |
| [DSH9072](DSH9xxx.md#dsh9072) | 警告 | {0} 通过其输出 {1} 被读取，而该输出不是目录所知的任何材质属性；一个零代替了 ... |
| [DSH9073](DSH9xxx.md#dsh9073) | 信息 | {0} 是一个 StaticSwitchParameter，该语言将其写为 '/// @static' uniform 和一个静态分支；t... |
| [DSH9074](DSH9xxx.md#dsh9074) | 信息 | '{0}' 将其属性作为一个集合读取，因此同时已连接的 {1} 个单独引脚会被忽略，依据... |
| [DSH9075](DSH9xxx.md#dsh9075) | 信息 | uniform '{0}' 被写为 '{1}'：该名称在此文件中已被占用。 |
| [DSH9076](DSH9xxx.md#dsh9076) | 警告 | '{0}' 的某个区域标题为 '{1}'，该标题无法容纳在 '#pragma region' 行上；它被写为 '{2}'。 |
| [DSH9077](DSH9xxx.md#dsh9077) | 信息 | '{1}' 的 {0} 个节点位置属于源文件内联写入的值，而位置由变量保留... |
| [DSH9078](DSH9xxx.md#dsh9078) | 错误 | '{1}' 的输出 '{0}' 没有成为参数；没有内容写入其中。 |
| [DSH9079](DSH9xxx.md#dsh9079) | 信息 | {0} 是一个 If，其各分支之间没有比较进行选择；它被写为 'UE.If(...)'。 |
| [DSH9080](DSH9xxx.md#dsh9080) | 警告 | 参数 '{0}' 出现多次，且其节点在其默认值、分组或种类上不一致；un... |
| [DSH9081](DSH9xxx.md#dsh9081) | 警告 | '{0}' 被调用，但其接口不可用，因此其 'extern' 原型是根据调用写入的... |
| [DSH9082](DSH9xxx.md#dsh9082) | 警告 | 自定义节点 '{0}' 的代码带有 DreamShader 的标记，且读取结果并非编译器所写入的内容... |
| [DSH9083](DSH9xxx.md#dsh9083) | 警告 | '{0}' 是一个 {1}，其引脚并非该语言写入该种类时所使用的引脚；它被写为一个普通... |
| [DSH9084](DSH9xxx.md#dsh9084) | 警告 | '{1}' 的输出 '{0}' 未连接到任何内容；没有内容写入其中。 |
| [DSH9085](DSH9xxx.md#dsh9085) | 错误 | '{0}' 以 '.{1}' 结尾，它被读取为 {2} 源文件，而反编译请求的是 {3} 文本；请命名该文件 ... |
| [DSH9086](DSH9xxx.md#dsh9086) | 错误 | 反编译请求的是 2.0 文本，却得到了 1.x 反编译器，该反编译器只写入 '.dsm' 和 '.dsf'... |
| [DSH9087](DSH9xxx.md#dsh9087) | 错误 | '{0}' 无法解析到它所构建的资产，因此没有可为其反编译的内容。 |
| [DSH9088](DSH9xxx.md#dsh9088) | 错误 | '{0}' 的图表未能读入有效模块；上述错误说明了位置。这是反... 的缺陷。 |
| [DSH9089](DSH9xxx.md#dsh9089) | 警告 | 反编译后的文本无法解析回去：{0}：{1}。它按原样写入；这是反编译器的缺陷。 |
| [DSH9090](DSH9xxx.md#dsh9090) | 错误 | '{0}' 未通过条件编译（{1}：{2}），且含 '#if' 行的源文件在任何情况下都不会被迁移。 |
| [DSH9091](DSH9xxx.md#dsh9091) | 错误 | '{0}' 在路径前命名了源根目录，而 '#include' 无法这样做；请从根目录自身的... 写入路径 |
| [DSH9092](DSH9xxx.md#dsh9092) | 错误 | '{1}' 的 {0} 条注释不会出现在迁移后的文件（{2}）中，因此没有写入任何内容；这是... 的故障 |
| [DSH9093](DSH9xxx.md#dsh9093) | 错误 | '{0}' 不再有 1.x 声明；没有可迁移的内容。 |
| [DSH9094](DSH9xxx.md#dsh9094) | 警告 | '{0}' 保留其 '/// @root'，即其资产去向的 1.x 拼写，因为新文件本应放置的位置... |
| [DSH9095](DSH9xxx.md#dsh9095) | 错误 | '{0}' 不是 1.x 源文件；migrate 接受 '.dsm'、'.dsf' 和 '.dsh' 文件。 |
| [DSH9097](DSH9xxx.md#dsh9097) | 错误 | '{0}' 的迁移后文本无法作为 2.0 源文件构建（{1}），因此没有写入任何内容；文本位于 '{2}'。 |
| [DSH9098](DSH9xxx.md#dsh9098) | 警告 | '{0}' 的迁移后文本无法构建出 1.x 文件所构建的图表：{1} |
| [DSH9099](DSH9xxx.md#dsh9099) | 错误 | '{0}' 已存在且不会被覆盖写入；请将其移走，或使用 -Out 迁移到另一个文件夹。 |
| [DSH9100](DSH9xxx.md#dsh9100) | 错误 | '{0}' 没有父材质，而 '.dsi' 不过是其覆盖。 |
| [DSH9101](DSH9xxx.md#dsh9101) | 警告 | '{0}' 覆盖其材质层或混合的 {1} 个参数，而 '.dsi' 无法处理这些；它们... |
| [DSH9102](DSH9xxx.md#dsh9102) | 信息 | 参数 '{0}' 不是变量可以使用的名称；它被写在另一个名称下，并带 '/// @name {0}'。 |
| [DSH9103](DSH9xxx.md#dsh9103) | 错误 | DSH9103：'{0}' 的 Parent 不再能解析，因此 '{1}' 的覆盖无法写回其中：{2} |
| [DSH9104](DSH9xxx.md#dsh9104) | 警告 | '{0}' 覆盖 '{1}'，而 '#pragma instance' 没有对应的键；重建后的实例具有父级的该值。 |
| [DSH9105](DSH9xxx.md#dsh9105) | 警告 | '{0}' 是曲线图集行，且 '{1}' 选取其曲线；'.dsi' 只能声明该行的编号，不能声明其他... |
| [DSH9106](DSH9xxx.md#dsh9106) | 警告 | '{0}' 被父级源文件声明为 'bool'，且 '{1}' 将其设为 {2}；该覆盖被写为 'flo... |
| [DSH9107](DSH9xxx.md#dsh9107) | 错误 | 预期 '{0}' 被单独声明以重写其值，却发现它位于与其他名称共享的声明中... |
| [DSH9108](DSH9xxx.md#dsh9108) | 错误 | 预期对此文件的每处更改都触及自身的文本段，却发现第 {0} 行的一处编辑重叠... |
| [DSH9109](DSH9xxx.md#dsh9109) | 错误 | 预期此 .dsi 文件中有一行 '#pragma instance(...)'，却未找到；该文件保持未更改。 |
| [DSH9110](DSH9xxx.md#dsh9110) | error | '-{0}={1}' 既不是 on 也不是 off。标志接受 true、1、yes 或 on，或者 false、0、no 或 off，而单独使用 '-{0}'…… |
| [DSH9111](DSH9xxx.md#dsh9111) | warning | '{0}' 位于某个包的资源文件夹中，且不再有源文件构建它：其包已被移除或停止 g…… |
| [DSH9200](DSH9xxx.md#dsh9200) | error | 无法读取 Custom Pass 槽位注册表：{0}。'dsc pass-registry -Rebuild' 会将其移开并给出 ev…… |
| [DSH9201](DSH9xxx.md#dsh9201) | warning | 不再有 .dsp 构建 '{0}'，但该资源仍然存在，且其通道 '{1}' 指向 {2} 槽位 {3}。Dele…… |
| [DSH9202](DSH9xxx.md#dsh9202) | info | {0} 槽位 {1} 已被释放：{2} |
| [DSH9203](DSH9xxx.md#dsh9203) | warning | Registry.json 未能解析，已被移开到 '{0}'；随后的编译会为每个 HLSL 通道提供一个 s…… |
| [DSH9204](DSH9xxx.md#dsh9204) | error | '{0}' 未能编译，因此其管线保留原有槽位：{1} |
| [DSH9205](DSH9xxx.md#dsh9205) | warning | '{0}' 的通道 '{1}' 指向 {2} 槽位 {3}，而注册表 {4}。其源 '{5}' 未能编译，因此 th…… |
| [DSH9206](DSH9xxx.md#dsh9206) | info | 源根目录下没有 .dsp；注册表将按 Registry.json 的现状重写。 |
| [DSH9208](DSH9xxx.md#dsh9208) | warning | {0} 槽位 {1}（'{3}' 的通道 '{2}'）的快照不在磁盘上，而注册表文件包含了它：n…… |
| [DSH9209](DSH9xxx.md#dsh9209) | warning | '{0}' 的编译程度不足以判断它是否仍在 {2} 槽位 {3} 中运行通道 '{1}'，因此该槽位被 …… |
| [DSH9210](DSH9xxx.md#dsh9210) | error | '{0}' 是通道管线，会反编译为 '.dsp'，而 '{1}' 不是。 |
| [DSH9211](DSH9xxx.md#dsh9211) | warning | 通道 '{0}' 既没有材质也没有着色器，因此文本两者都不指定，且在……之前不会构建 |
| [DSH9212](DSH9xxx.md#dsh9212) | warning | 通道 '{0}' 同时拥有材质 '{1}' 和着色器 '{2}'；全屏通道只指定两者之一，而…… |
| [DSH9214](DSH9xxx.md#dsh9214) | info | 通道 '{0}' 的层过滤器未保留其层的任何拼写，因此它们会按项目的层……写入 |
| [DSH9215](DSH9xxx.md#dsh9215) | warning | 通道 '{0}' 的层过滤器选择了层位 {1}，而项目的层表没有对应名称；…… |
| [DSH9216](DSH9xxx.md#dsh9216) | warning | '{0}' 不在任何类型的视图中运行，'.dsp' 无法表达这一点；文本省略了 'Views'，而重建会在 i…… 中运行 |
| [DSH9217](DSH9xxx.md#dsh9217) | warning | 通道 '{0}' 检查其覆盖材质时完全没有使用标志，'.dsp' 无法表达这一点；文本 l…… |
| [DSH9218](DSH9xxx.md#dsh9218) | warning | 参数 '{0}' 默认值为 '{1}'，它不是 2D 纹理。'.dsp' 会声明每个纹理参数…… |
| [DSH9219](DSH9xxx.md#dsh9219) | warning | 通道 '{0}' 将 '{1}' 绑定到纹理常量，'param' 无法表达这一点；该绑定被排除在……之外 |
| [DSH9220](DSH9xxx.md#dsh9220) | warning | 通道 '{0}' 针对自身深度进行测试，但没有为其指定 Depth32 缓冲区；文本将 'Own()' 写为空…… |
| [DSH9221](DSH9xxx.md#dsh9221) | warning | 反编译后的管线无法解析回去：{0}：{1}。它按原样写入；这是反编译器的缺陷…… |
| [DSH9222](DSH9xxx.md#dsh9222) | warning | 反编译后的管线可以解析，但无法绑定回管线：{0}：{1}。它按原样写入；ei…… |
| [DSH9223](DSH9xxx.md#dsh9223) | warning | 反编译后的管线读回后是不同的管线（{0} 处差异；第一个：{1}）。它被写…… |
| [DSH9224](DSH9xxx.md#dsh9224) | warning | '.dsp' 以其文件名命名其管线，且没有 '/// @name'，因此 '{0}' 在其书写处构建 '{1}'…… |
| [DSH9225](DSH9xxx.md#dsh9225) | error | 没有可反编译的通道管线。 |
| [DSH9226](DSH9xxx.md#dsh9226) | error | DSH9226：'{0}' 由管线文件 '{1}' 构建，但不是通道管线，因此没有采纳任何内容。 |
| [DSH9227](DSH9xxx.md#dsh9227) | error | DSH9227：'{0}' 不是 .dsp 管线文件，因此 '{1}' 的设置无法拼接进去。 |
| [DSH9228](DSH9xxx.md#dsh9228) | error | DSH9228：'{0}' 不再构建任何通道管线，因此 '{1}' 没有可拼接进去的对象。 |
