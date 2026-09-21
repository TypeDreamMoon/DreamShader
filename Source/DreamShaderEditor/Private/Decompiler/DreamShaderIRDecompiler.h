// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The 2.0 decompiler: graph -> IR -> AST -> printer.
//
//   ImportDreamShaderGraphToIR     (Decompiler/DreamShaderGraphImport.h)    the asset's graph as an IR product
//   RunDreamShaderIRPasses         fold off, dedupe off, prune on           nodes nothing reads go
//   ValidateDreamShaderIR                                                   an importer bug is a DSH43xx, not a bad file
//   RaiseDreamShaderIR             (Decompile/IRToAst.h)                    the emitter's lowered shapes, read back
//   BuildDreamShaderAstFromIR      (Decompile/IRToAst.h)                    declarations, statements, names, regions
//   PrintDreamShaderLang                                                    the text
//
// It answers a whole request (IDreamShaderDecompiler::DecompileRequest): one asset, or every product of a source in
// one module; a plain material instance goes to the instance decompiler and comes back as `.dsi` text.

#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor
{
	class IDreamShaderDecompiler;
}

namespace UE::DreamShader::Editor::Private
{
	/** The 2.0 decompiler (graph -> IR -> AST -> printer) behind Format = Dss. Process-wide, like GetGraphDecompiler(). */
	UE::DreamShader::Editor::IDreamShaderDecompiler& GetIRDecompiler();
}
