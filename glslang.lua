project "glslang"

--kind "ConsoleApp"
--kind "WindowedApp"
kind "StaticLib"
--kind "SharedLib"
--language "C"
language "C++"

objdir "%{prj.name}"

flags{
    --"WinMain"
    --"Unicode",
    --"StaticRuntime",
}
files {
    "glslang/glslang/Public/ShaderLang.h",
    "glslang/glslang/Include/arrays.h",
    "glslang/glslang/Include/BaseTypes.h",
    "glslang/glslang/Include/Common.h",
    "glslang/glslang/Include/ConstantUnion.h",
    "glslang/glslang/Include/InfoSink.h",
    "glslang/glslang/Include/InitializeGlobals.h",
    "glslang/glslang/Include/intermediate.h",
    "glslang/glslang/Include/PoolAlloc.h",
    "glslang/glslang/Include/ResourceLimits.h",
    "glslang/glslang/Include/revision.h",
    "glslang/glslang/Include/ShHandle.h",
    "glslang/glslang/Include/Types.h",
    "glslang/glslang/MachineIndependent/glslang_tab.cpp.h",
    "glslang/glslang/MachineIndependent/gl_types.h",
    "glslang/glslang/MachineIndependent/Initialize.h",
    "glslang/glslang/MachineIndependent/localintermediate.h",
    "glslang/glslang/MachineIndependent/ParseHelper.h",
    "glslang/glslang/MachineIndependent/reflection.h",
    "glslang/glslang/MachineIndependent/RemoveTree.h",
    "glslang/glslang/MachineIndependent/Scan.h",
    "glslang/glslang/MachineIndependent/ScanContext.h",
    "glslang/glslang/MachineIndependent/SymbolTable.h",
    "glslang/glslang/MachineIndependent/Versions.h",
    "glslang/glslang/MachineIndependent/parseVersions.h",
    "glslang/glslang/MachineIndependent/propagateNoContraction.h",
    "glslang/glslang/MachineIndependent/preprocessor/PpContext.h",
    "glslang/glslang/MachineIndependent/preprocessor/PpTokens.h",
    "glslang/glslang/MachineIndependent/glslang_tab.cpp",
    "glslang/glslang/MachineIndependent/Constant.cpp",
    "glslang/glslang/MachineIndependent/InfoSink.cpp",
    "glslang/glslang/MachineIndependent/Initialize.cpp",
    "glslang/glslang/MachineIndependent/IntermTraverse.cpp",
    "glslang/glslang/MachineIndependent/Intermediate.cpp",
    "glslang/glslang/MachineIndependent/ParseContextBase.cpp",
    "glslang/glslang/MachineIndependent/ParseHelper.cpp",
    "glslang/glslang/MachineIndependent/PoolAlloc.cpp",
    "glslang/glslang/MachineIndependent/RemoveTree.cpp",
    "glslang/glslang/MachineIndependent/Scan.cpp",
    "glslang/glslang/MachineIndependent/ShaderLang.cpp",
    "glslang/glslang/MachineIndependent/SymbolTable.cpp",
    "glslang/glslang/MachineIndependent/Versions.cpp",
    "glslang/glslang/MachineIndependent/intermOut.cpp",
    "glslang/glslang/MachineIndependent/limits.cpp",
    "glslang/glslang/MachineIndependent/linkValidate.cpp",
    "glslang/glslang/MachineIndependent/parseConst.cpp",
    "glslang/glslang/MachineIndependent/reflection.cpp",
    "glslang/glslang/MachineIndependent/preprocessor/Pp.cpp",
    "glslang/glslang/MachineIndependent/preprocessor/PpAtom.cpp",
    "glslang/glslang/MachineIndependent/preprocessor/PpContext.cpp",
    "glslang/glslang/MachineIndependent/preprocessor/PpMemory.cpp",
    "glslang/glslang/MachineIndependent/preprocessor/PpScanner.cpp",
    "glslang/glslang/MachineIndependent/preprocessor/PpSymbols.cpp",
    "glslang/glslang/MachineIndependent/preprocessor/PpTokens.cpp",
    "glslang/glslang/MachineIndependent/propagateNoContraction.cpp",
    "glslang/glslang/GenericCodeGen/CodeGen.cpp",
    "glslang/glslang/GenericCodeGen/Link.cpp",
    "glslang/glslang/MachineIndependent/glslang.y",

    "glslang/glslang/OSDependent/osinclude.h",
    "glslang/glslang/OSDependent/Windows/ossource.cpp",

    "glslang/SPIRV/spirv.hpp",
    "glslang/SPIRV/GLSL.std.450.h",
    "glslang/SPIRV/GlslangToSpv.h",
    "glslang/SPIRV/Logger.h",
    "glslang/SPIRV/SpvBuilder.h",
    "glslang/SPIRV/spvIR.h",
    "glslang/SPIRV/doc.h",
    "glslang/SPIRV/disassemble.h",
    "glslang/SPIRV/GlslangToSpv.cpp",
    "glslang/SPIRV/InReadableOrder.cpp",
    "glslang/SPIRV/Logger.cpp",
    "glslang/SPIRV/SpvBuilder.cpp",
    "glslang/SPIRV/doc.cpp",
    "glslang/SPIRV/disassemble.cpp",

    "glslang/hlsl/hlslParseHelper.h",
    "glslang/hlsl/hlslTokens.h",
    "glslang/hlsl/hlslScanContext.h",
    "glslang/hlsl/hlslOpMap.h",
    "glslang/hlsl/hlslTokenStream.h",
    "glslang/hlsl/hlslGrammar.h",
    "glslang/hlsl/hlslParseables.h",
    "glslang/hlsl/hlslParseHelper.cpp",
    "glslang/hlsl/hlslScanContext.cpp",
    "glslang/hlsl/hlslOpMap.cpp",
    "glslang/hlsl/hlslTokenStream.cpp",
    "glslang/hlsl/hlslGrammar.cpp",
    "glslang/hlsl/hlslParseables.cpp",

    "glslang/OGLCompilersDLL/InitializeDll.h",
    "glslang/OGLCompilersDLL/InitializeDll.cpp",
}
includedirs {
}
defines {
    "WIN32",
    "_WINDOWS",
    "_DEBUG",
    "GLSLANG_OSINCLUDE_WIN32",
}
buildoptions { }
libdirs { }
links { }

