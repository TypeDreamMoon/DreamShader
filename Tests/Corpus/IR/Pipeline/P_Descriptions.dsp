// Descriptions from `@desc` and from free text -- two lines of it kept as two -- and texture parameters with and
// without a default asset.
/// The noise the grade reads.
/// Two lines of it.
uniform Texture2D Noise;
/// @default /Engine/EngineResources/DefaultTexture
uniform Texture2D Fallback;

/// Free text above a buffer.
buffer Mood : RGBA8(Export = true);

/// @desc A pass described by its directive.
pass Grade : copy
{
    read SceneColor;
    write Mood;
}
