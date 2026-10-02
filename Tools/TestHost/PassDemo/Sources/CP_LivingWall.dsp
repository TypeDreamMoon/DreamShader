// CP_LivingWall.dsp -- the wall grows a coral of light: a Gray-Scott reaction-diffusion field simulated on the GPU,
// sixteen steps a frame, kept from one frame to the next and exported for the wall's material (M_DemoLivingWall).
// Four compute passes run one entry of the file's `hlsl` block, each with buffers of its own, and the entry does four
// steps at once in groupshared memory. At BeginView, so the wall draws this frame's field.
#pragma pipeline(Order = 10, Views = Game | Editor)

/// @group Growth   @slider 0.01 0.1
uniform float Feed = 0.0545;
/// @group Growth   @slider 0.03 0.08
uniform float Kill = 0.062;
/// @group Growth   @slider 2 12
uniform float SporeRadius = 6.0;

/// @desc x = U, the substrate; y = V, what grows. 448 x 192 cells over the wall's 7 x 3 m, wrapping at the edges.
buffer Field : RG32F(Size = int2(448, 192), History = true, Export = true, Clear = float4(1, 0, 0, 0));
buffer StepA : RG32F(Size = int2(448, 192), Clear = float4(1, 0, 0, 0));
buffer StepB : RG32F(Size = int2(448, 192), Clear = float4(1, 0, 0, 0));

hlsl
{
    // A tile of 16 x 16 cells and 4 cells around it: every step makes the exact region one cell smaller, so after four
    // steps the tile itself is still exact.
    #define LW_TILE 16
    #define LW_HALO 4
    #define LW_SPAN (LW_TILE + 2 * LW_HALO)
    #define LW_COUNT (LW_SPAN * LW_SPAN)

    groupshared float2 LwCellsA[LW_COUNT];
    groupshared float2 LwCellsB[LW_COUNT];

    // dU = Du lap U - U V^2 + F (1 - U),  dV = Dv lap V + U V^2 - (K + F) V,  with Du = 1, Dv = 0.5 and a step of 1.
    float2 LwReact(float2 Cell, float2 Lap, float F, float K)
    {
        const float Reaction = Cell.x * Cell.y * Cell.y;
        return saturate(Cell + float2(Lap.x - Reaction + F * (1.0 - Cell.x), 0.5 * Lap.y + Reaction - (K + F) * Cell.y));
    }

    // One step of the tile, from A into B and from B into A; the outermost ring has no neighbours and is copied.
    void LwStepAToB(uint Thread, float F, float K)
    {
        for (uint I = Thread; I < LW_COUNT; I += LW_TILE * LW_TILE)
        {
            const uint X = I % LW_SPAN;
            const uint Y = I / LW_SPAN;
            if (X == 0 || Y == 0 || X == LW_SPAN - 1 || Y == LW_SPAN - 1)
            {
                LwCellsB[I] = LwCellsA[I];
                continue;
            }
            const float2 Here = LwCellsA[I];
            const float2 Lap = 0.2 * (LwCellsA[I - 1] + LwCellsA[I + 1] + LwCellsA[I - LW_SPAN] + LwCellsA[I + LW_SPAN])
                + 0.05 * (LwCellsA[I - LW_SPAN - 1] + LwCellsA[I - LW_SPAN + 1] + LwCellsA[I + LW_SPAN - 1] + LwCellsA[I + LW_SPAN + 1])
                - Here;
            LwCellsB[I] = LwReact(Here, Lap, F, K);
        }
    }

    void LwStepBToA(uint Thread, float F, float K)
    {
        for (uint I = Thread; I < LW_COUNT; I += LW_TILE * LW_TILE)
        {
            const uint X = I % LW_SPAN;
            const uint Y = I / LW_SPAN;
            if (X == 0 || Y == 0 || X == LW_SPAN - 1 || Y == LW_SPAN - 1)
            {
                LwCellsA[I] = LwCellsB[I];
                continue;
            }
            const float2 Here = LwCellsB[I];
            const float2 Lap = 0.2 * (LwCellsB[I - 1] + LwCellsB[I + 1] + LwCellsB[I - LW_SPAN] + LwCellsB[I + LW_SPAN])
                + 0.05 * (LwCellsB[I - LW_SPAN - 1] + LwCellsB[I - LW_SPAN + 1] + LwCellsB[I + LW_SPAN - 1] + LwCellsB[I + LW_SPAN + 1])
                - Here;
            LwCellsA[I] = LwReact(Here, Lap, F, K);
        }
    }

    // Where the coral starts: ten spores and a brush that wanders over the wall. A spore is a disc of half substrate,
    // half coral -- a smaller or weaker one loses more to diffusion than the reaction makes, and dies. Only on bare
    // cells, so the coral is never cut back where it has grown over them.
    float2 LwPlant(float2 Value, float2 Cell, float2 Extent, float Seconds, float SpotRadius)
    {
        const float2 Spores[10] = { float2(0.06, 0.30), float2(0.17, 0.70), float2(0.28, 0.25), float2(0.38, 0.62), float2(0.49, 0.18),
                                    float2(0.58, 0.80), float2(0.67, 0.42), float2(0.77, 0.15), float2(0.86, 0.66), float2(0.95, 0.35) };
        float Nearest = 1e9;
        [unroll] for (int S = 0; S < 10; ++S)
        {
            Nearest = min(Nearest, length(Cell - Spores[S] * Extent));
        }
        const float2 Brush = float2(0.5 + 0.45 * sin(Seconds * 0.31), 0.5 + 0.40 * sin(Seconds * 0.47 + 1.3)) * Extent;
        Nearest = min(Nearest, length(Cell - Brush));
        const bool bBare = Value.x > 0.99 && Value.y < 0.01;
        return bBare && Nearest < SpotRadius ? float2(0.5, 0.5) : Value;
    }

    // Four steps of the field: the tile and its halo into groupshared memory, the tile back out. The first pass of a
    // frame plants as well.
    [numthreads(16, 16, 1)]
    void GrowCS(uint3 GroupId : SV_GroupID, uint3 LocalId : SV_GroupThreadID, uint LocalIndex : SV_GroupIndex)
    {
        const int2 Size = int2(OutFieldSize.xy);
        const int2 Origin = int2(GroupId.xy) * LW_TILE - LW_HALO;
        for (uint I = LocalIndex; I < LW_COUNT; I += LW_TILE * LW_TILE)
        {
            const int2 Cell = (Origin + int2(I % LW_SPAN, I / LW_SPAN) + Size) % Size;
            float2 Value = InField.Load(int3(Cell, 0)).xy;
            if (Plant > 0.5)
            {
                Value = LwPlant(Value, float2(Cell), float2(Size), DP_Time.x, Spore);
            }
            LwCellsA[I] = Value;
        }
        GroupMemoryBarrierWithGroupSync();

        [unroll] for (int Pair = 0; Pair < LW_HALO / 2; ++Pair)
        {
            LwStepAToB(LocalIndex, FeedRate, KillRate);
            GroupMemoryBarrierWithGroupSync();
            LwStepBToA(LocalIndex, FeedRate, KillRate);
            GroupMemoryBarrierWithGroupSync();
        }

        const int2 Cell = int2(GroupId.xy) * LW_TILE + int2(LocalId.xy);
        if (all(Cell < Size))
        {
            const uint2 Local = LocalId.xy + LW_HALO;
            OutField[Cell] = float4(LwCellsA[Local.y * LW_SPAN + Local.x], 0.0, 0.0);
        }
    }
}

/// @desc Last frame's field, four steps on, planted.
pass Grow1 : compute
{
    Injection = BeginView;
    Entry     = GrowCS;
    Dispatch  = Field;
    read  InField  = Field.Previous;
    write OutField = StepA;
    param FeedRate = Feed;
    param KillRate = Kill;
    param Spore    = SporeRadius;
    param Plant    = 1.0;
}

pass Grow2 : compute
{
    Injection = BeginView;
    Entry     = GrowCS;
    Dispatch  = Field;
    read  InField  = StepA;
    write OutField = StepB;
    param FeedRate = Feed;
    param KillRate = Kill;
    param Spore    = SporeRadius;
    param Plant    = 0.0;
}

pass Grow3 : compute
{
    Injection = BeginView;
    Entry     = GrowCS;
    Dispatch  = Field;
    read  InField  = StepB;
    write OutField = StepA;
    param FeedRate = Feed;
    param KillRate = Kill;
    param Spore    = SporeRadius;
    param Plant    = 0.0;
}

/// @desc The fourth four steps, into the field the wall reads.
pass Grow4 : compute
{
    Injection = BeginView;
    Entry     = GrowCS;
    Dispatch  = Field;
    read  InField  = StepA;
    write OutField = Field;
    param FeedRate = Feed;
    param KillRate = Kill;
    param Spore    = SporeRadius;
    param Plant    = 0.0;
}
