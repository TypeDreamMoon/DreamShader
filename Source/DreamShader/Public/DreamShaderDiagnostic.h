#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader
{
	/**
	 * A DreamShader compile failure: a stable DSHnnnn code plus the message a human reads.
	 *
	 * The code is the identity. Tests, the diagnose skill, the corpus expectations and the editor
	 * extensions all key off it, so it must not change once published -- which is precisely what
	 * frees the message text to be reworded and, eventually, translated. Today the generator's
	 * messages are pinned to English by `I18N-EXEMPT` markers for exactly the opposite reason:
	 * nothing else identifies them, so the text *is* the contract.
	 *
	 * Code ranges -- the leading digit is the stage that raises it, and each one owns the doc page
	 * of the same name under Docs/diagnostics/:
	 *
	 *   DSH1xxx  driver, source files and imports
	 *   DSH2xxx  lexer and syntax
	 *   DSH3xxx  sections and declarations
	 *   DSH4xxx  Graph statements and expressions
	 *   DSH5xxx  builtins -- UE.*, math, Substrate
	 *   DSH6xxx  functions and HLSL codegen
	 *   DSH7xxx  properties, parameters and settings
	 *   DSH8xxx  asset generation and saving
	 *   DSH9xxx  tools -- commandlet, VirtualFunction sync, and internal invariants (DSH99xx)
	 *
	 * An empty Code means the site has not been tagged yet. That is a deliberate state, not a bug:
	 * the migration tags stage by stage, and an untagged message must keep working and must not
	 * inherit a neighbour's code. Every assignment path below therefore clears the code unless it
	 * is explicitly a wrap.
	 */
	struct FDreamShaderError
	{
		FString Code;
		FString Message;

		FDreamShaderError() = default;

		/**
		 * Implicit so an untagged `OutError = FString::Printf(...)` site keeps compiling during the
		 * migration. It clears Code on purpose -- see the note above.
		 */
		FDreamShaderError(const FString& InMessage) : Message(InMessage) {}
		FDreamShaderError(const TCHAR* InMessage) : Message(InMessage) {}

		FDreamShaderError& operator=(const FString& InMessage)
		{
			Code.Reset();
			Message = InMessage;
			return *this;
		}

		FDreamShaderError& operator=(const TCHAR* InMessage)
		{
			Code.Reset();
			Message = InMessage;
			return *this;
		}

		/**
		 * Lets the 76 existing `FString::Printf(TEXT("... %s"), ..., *OutError)` wrap sites keep
		 * reading the message with no edit. Pair it with WrapError so the code survives the wrap.
		 */
		const TCHAR* operator*() const { return *Message; }

		operator const FString&() const { return Message; }

		bool IsEmpty() const { return Message.IsEmpty(); }
		bool HasCode() const { return !Code.IsEmpty(); }

		void Reset()
		{
			Code.Reset();
			Message.Reset();
		}
	};

	/**
	 * The parser's half of the same contract. It keeps FText because its message sites are real
	 * LOCTEXT entries that the localization gather must still see; collapsing them to FString would
	 * silently drop them from the zh-Hans target.
	 */
	struct FDreamShaderTextError
	{
		FString Code;
		FText Message;

		FDreamShaderTextError() = default;
		FDreamShaderTextError(const FText& InMessage) : Message(InMessage) {}

		FDreamShaderTextError& operator=(const FText& InMessage)
		{
			Code.Reset();
			Message = InMessage;
			return *this;
		}

		bool IsEmpty() const { return Message.IsEmpty(); }
		bool HasCode() const { return !Code.IsEmpty(); }
	};

	/** Carries a generator error across the module boundary without losing its code. */
	inline FDreamShaderTextError ToTextError(const FDreamShaderError& InError)
	{
		FDreamShaderTextError Result;
		Result.Code = InError.Code;
		// FromString, not a LOCTEXT lookup: generator messages are built at runtime and the wire
		// path (ToInvariantWireString) reads dynamic text back as its raw input, unchanged.
		Result.Message = FText::FromString(InError.Message);
		return Result;
	}

	/** ...and back, for the entry points that still hand an FString down. */
	inline FDreamShaderError ToStringError(const FDreamShaderTextError& InError)
	{
		FDreamShaderError Result;
		Result.Code = InError.Code;
		Result.Message = InError.Message.ToString();
		return Result;
	}

	// -------------------------------------------------------------------------------------------
	// Raise helpers.
	//
	// All of them return false so the ubiquitous `{ OutError = ...; return false; }` pair collapses
	// to a single `return FailWith(...)`. That is not just brevity: it makes the raise sites
	// grep-able as one shape, which is what .skill/gen-diagnostics.ps1 scans to build the docs.
	// -------------------------------------------------------------------------------------------

	inline bool FailWith(FDreamShaderError& OutError, const TCHAR* Code, const TCHAR* Message)
	{
		OutError.Code = Code;
		OutError.Message = Message;
		return false;
	}

	template <typename... TArgs>
	bool FailWith(FDreamShaderError& OutError, const TCHAR* Code, const TCHAR* Format, TArgs... Args)
	{
		OutError.Code = Code;
		OutError.Message = FString::Printf(Format, Args...);
		return false;
	}

	inline bool FailWith(FDreamShaderTextError& OutError, const TCHAR* Code, const FText& Message)
	{
		OutError.Code = Code;
		OutError.Message = Message;
		return false;
	}

	/**
	 * Replaces the message while keeping the code that the inner failure already set.
	 *
	 * DreamShader reports errors by wrapping text as the stack unwinds -- "In Graph statement 'x':
	 * <inner>" -- so without this the outermost wrapper would be the thing that names the failure,
	 * and every Graph error in the plugin would share one useless code. The innermost raise knows
	 * what actually went wrong, so its code is the one that survives.
	 *
	 * Read the inner message *before* calling: `WrapError(E, TEXT("In X: %s"), *E)` is well-defined
	 * because the argument is evaluated first.
	 */
	template <typename... TArgs>
	bool WrapError(FDreamShaderError& OutError, const TCHAR* Format, TArgs... Args)
	{
		OutError.Message = FString::Printf(Format, Args...);
		return false;
	}

	inline bool WrapError(FDreamShaderTextError& OutError, const FText& Message)
	{
		OutError.Message = Message;
		return false;
	}
}
