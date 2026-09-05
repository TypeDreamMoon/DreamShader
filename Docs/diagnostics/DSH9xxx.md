# DSH9xxx --- Tools, sync and internal invariants

> The block between the generated markers is written by `.skill/gen-diagnostics.ps1`.
> Everything below a marker is written by hand and survives a regeneration.

## DSH9001

<!-- generated:begin DSH9001 -->
**Severity** error

**Message**

```
DSH9001: '{0}' uses conditional compilation, and VirtualFunction sync rewrites a source in place at byte offsets taken from the file as written -- it cannot tell which of your branches a definition belongs to, and refuses rather than risk writing one branch over the others. Refresh these definitions by moving them into a source without directives, or edit them by hand.
```

**Raised by** `Source/DreamShaderEditor/Private/VirtualFunction/DreamShaderVirtualFunctionSyncService.cpp:339`
<!-- generated:end DSH9001 -->

**Cause.** _Not written yet._

**Fix.** _Not written yet._

