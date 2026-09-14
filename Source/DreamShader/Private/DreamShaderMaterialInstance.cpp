#include "DreamShaderMaterialInstance.h"

#include "DreamShaderSettings.h"
#include "Materials/Material.h"
#include "UObject/Package.h"

bool UDreamShaderMaterialInstance::HasOverridenBaseProperties() const
{
	// Force a self-contained static permutation (an own shader map) only at the ROOT instance — the one
	// whose immediate parent is the hidden base UMaterial. A child MIC parented to a DreamShader
	// instance falls through to stock behavior so it DEFERS to and SHARES the root's shader map: many
	// parameter/color variants off one root reuse a single compiled map instead of each compiling its
	// own (bounds the shader-map count). InitStaticPermutation recomputes
	// bHasStaticPermutationResource from this on every load, so the test must be durable —
	// Cast<UMaterial>(Parent) is (a base material is the parent ⟺ this is a root), not transient
	// state. Mirrors ULandscapeMaterialInstanceConstant and UShaderLab's root-vs-child ownership scheme.
	if (Cast<UMaterial>(Parent) != nullptr)
	{
		return true;
	}
	return Super::HasOverridenBaseProperties();
}

bool UDreamShaderMaterialInstance::IsAsset() const
{
	// Ephemeral instances hide from asset enumeration — the Content Browser and the asset registry
	// discover assets that have no file by iterating live objects and asking IsAsset()
	// (AssetRegistry.cpp object-iterator path), so returning false here removes them from the
	// browser and from save pickers (which also prevents an accidental explicit Save from
	// materializing them). The source file is the authoring surface; references still resolve
	// through the object path. Materialized instances behave like normal assets.
	if (GetPackage()->HasAnyPackageFlags(PKG_NewlyCreated)
		&& !GetDefault<UDreamShaderSettings>()->bShowEphemeralMaterials)
	{
		return false;
	}
	return Super::IsAsset();
}
