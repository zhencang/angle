//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WEBGL_multiview_test.cpp:
//   Test that shaders with gl_ViewID_OVR are validated correctly.
//

#include "GLSLANG/ShaderLang.h"
#include "compiler/translator/IntermTraverse.h"
#include "tests/test_utils/ShaderCompileTreeTest.h"
#include "tests/test_utils/compiler_test.h"

using namespace sh;

namespace
{

class SymbolOccurrenceCounter : public TIntermTraverser
{
  public:
    SymbolOccurrenceCounter() : TIntermTraverser(true, false, false), mNumberOfOccurrences(0u) {}

    void visitSymbol(TIntermSymbol *node) override
    {
        if (shouldCountSymbol(node))
        {
            ++mNumberOfOccurrences;
        }
    }

    virtual bool shouldCountSymbol(const TIntermSymbol *node) const = 0;

    unsigned getNumberOfOccurrences() const { return mNumberOfOccurrences; }

  private:
    unsigned mNumberOfOccurrences;
};

class SymbolOccurrenceCounterByQualifier : public SymbolOccurrenceCounter
{
  public:
    SymbolOccurrenceCounterByQualifier(TQualifier symbolQualifier)
        : mSymbolQualifier(symbolQualifier)
    {
    }

    bool shouldCountSymbol(const TIntermSymbol *node) const override
    {
        return node->getQualifier() == mSymbolQualifier;
    }

  private:
    TQualifier mSymbolQualifier;
};

class SymbolOccurrenceCounterByName : public SymbolOccurrenceCounter
{
  public:
    SymbolOccurrenceCounterByName(const TString &symbolName) : mSymbolName(symbolName) {}

    bool shouldCountSymbol(const TIntermSymbol *node) const override
    {
        return node->getName().getString() == mSymbolName;
    }

  private:
    TString mSymbolName;
};

class SymbolOccurrenceCounterByNameAndQualifier : public SymbolOccurrenceCounter
{
  public:
    SymbolOccurrenceCounterByNameAndQualifier(const TString &symbolName, TQualifier qualifier)
        : mSymbolName(symbolName), mSymbolQualifier(qualifier)
    {
    }

    bool shouldCountSymbol(const TIntermSymbol *node) const override
    {
        return node->getName().getString() == mSymbolName &&
               node->getQualifier() == mSymbolQualifier;
    }

  private:
    TString mSymbolName;
    TQualifier mSymbolQualifier;
};

class WEBGLMultiviewVertexShaderTest : public ShaderCompileTreeTest
{
  public:
    WEBGLMultiviewVertexShaderTest() {}
  protected:
    ::GLenum getShaderType() const override { return GL_VERTEX_SHADER; }
    ShShaderSpec getShaderSpec() const override { return SH_WEBGL3_SPEC; }
    void initResources(ShBuiltInResources *resources) override
    {
        resources->OVR_multiview = 1;
        resources->MaxViewsOVR   = 4;
    }
};

class WEBGLMultiviewFragmentShaderTest : public ShaderCompileTreeTest
{
  public:
    WEBGLMultiviewFragmentShaderTest() {}
  protected:
    ::GLenum getShaderType() const override { return GL_FRAGMENT_SHADER; }
    ShShaderSpec getShaderSpec() const override { return SH_WEBGL3_SPEC; }
    void initResources(ShBuiltInResources *resources) override
    {
        resources->OVR_multiview = 1;
        resources->MaxViewsOVR   = 4;
    }
};

class WEBGLMultiviewVertexShaderOutputCodeTest : public MatchOutputCodeTest
{
  public:
    WEBGLMultiviewVertexShaderOutputCodeTest()
        : MatchOutputCodeTest(GL_VERTEX_SHADER, 0, SH_ESSL_OUTPUT)
    {
        addOutputType(SH_GLSL_COMPATIBILITY_OUTPUT);
#if defined(ANGLE_ENABLE_HLSL)
        addOutputType(SH_HLSL_4_1_OUTPUT);
#endif
        getResources()->OVR_multiview = 1;
        getResources()->MaxViewsOVR   = 4;
    }
    bool foundInAllGLSLCode(const char *str)
    {
        return foundInCode(SH_GLSL_COMPATIBILITY_OUTPUT, str) && foundInCode(SH_ESSL_OUTPUT, str);
    }

    bool foundInHLSLCode(const char *stringToFind) const
    {
#if defined(ANGLE_ENABLE_HLSL)
        return foundInCode(SH_HLSL_4_1_OUTPUT, stringToFind);
#else
        return true;
#endif
    }
};

void VariableOccursNTimes(TIntermBlock *root,
                          const TString &varName,
                          const TQualifier varQualifier,
                          unsigned n)
{
    // Check that there are n occurrences of the variable with the given name and qualifier.
    SymbolOccurrenceCounterByNameAndQualifier viewIDByNameAndQualifier(varName, varQualifier);
    root->traverse(&viewIDByNameAndQualifier);
    EXPECT_EQ(n, viewIDByNameAndQualifier.getNumberOfOccurrences());

    // Check that there are n occurrences of the variable with the given name. By this we guarantee
    // that there are no other occurrences of the variable with the same name but different
    // qualifier.
    SymbolOccurrenceCounterByName viewIDByName(varName);
    root->traverse(&viewIDByName);
    EXPECT_EQ(n, viewIDByName.getNumberOfOccurrences());
}

// Invalid combination of extensions (restricted in the WEBGL_multiview spec).
TEST_F(WEBGLMultiviewVertexShaderTest, InvalidBothMultiviewAndMultiview2)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "#extension GL_OVR_multiview2 : enable\n"
        "layout(num_views = 2) in;\n"
        "void main()\n"
        "{\n"
        "    gl_Position.x = (gl_ViewID_OVR == 0u) ? 1.0 : 0.0;\n"
        "    gl_Position.yzw = vec3(0, 0, 1);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Invalid combination of non-matching num_views declarations.
TEST_F(WEBGLMultiviewVertexShaderTest, InvalidNumViewsMismatch)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "layout(num_views = 1) in;\n"
        "void main()\n"
        "{\n"
        "    gl_Position.x = (gl_ViewID_OVR == 0u) ? 1.0 : 0.0;\n"
        "    gl_Position.yzw = vec3(0, 0, 1);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Invalid value zero for num_views.
TEST_F(WEBGLMultiviewVertexShaderTest, InvalidNumViewsZero)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 0) in;\n"
        "void main()\n"
        "{\n"
        "    gl_Position.x = (gl_ViewID_OVR == 0u) ? 1.0 : 0.0;\n"
        "    gl_Position.yzw = vec3(0, 0, 1);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Too large value for num_views.
TEST_F(WEBGLMultiviewVertexShaderTest, InvalidNumViewsGreaterThanMax)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 5) in;\n"
        "void main()\n"
        "{\n"
        "    gl_Position.x = (gl_ViewID_OVR == 0u) ? 1.0 : 0.0;\n"
        "    gl_Position.yzw = vec3(0, 0, 1);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Valid use of gl_ViewID_OVR in a ternary operator.
TEST_F(WEBGLMultiviewVertexShaderTest, ValidViewIDInTernary)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "layout(num_views = 2) in;  // Duplicated on purpose\n"
        "void main()\n"
        "{\n"
        "    gl_Position.x = (gl_ViewID_OVR == 0u) ? 1.0 : 0.0;\n"
        "    gl_Position.yzw = vec3(0, 0, 1);\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Valid use of gl_ViewID_OVR in an if statement.
TEST_F(WEBGLMultiviewVertexShaderTest, ValidViewIDInIf)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "precision highp float;\n"
        "in vec4 pos;\n"
        "void main()\n"
        "{\n"
        "    if (gl_ViewID_OVR == 0u)\n"
        "    {\n"
        "        gl_Position.x = pos.x;\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_Position.x = 1.0;\n"
        "    }\n"
        "    gl_Position.yzw = pos.yzw;\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Valid normal write of gl_Position in addition to the write that's dependent on gl_ViewID_OVR.
TEST_F(WEBGLMultiviewVertexShaderTest, ValidWriteOfGlPosition)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "void main()\n"
        "{\n"
        "    if (0u == gl_ViewID_OVR)\n"
        "    {\n"
        "        gl_Position.x = 1.0;\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_Position.x = 1.0;\n"
        "    }\n"
        "    gl_Position = vec4(1, 1, 1, 1);\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Invalid assignment to gl_Position.y inside if dependent on gl_ViewID_OVR.
TEST_F(WEBGLMultiviewVertexShaderTest, InvalidGlPositionAssignmentInIf)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "void main()\n"
        "{\n"
        "    if (gl_ViewID_OVR == 0u)\n"
        "    {\n"
        "        gl_Position.y = 1.0;\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_Position.y = 1.0;\n"
        "    }\n"
        "    gl_Position.xzw = vec3(0, 0, 1);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Invalid multiple assignments inside if dependent on gl_ViewID_OVR.
TEST_F(WEBGLMultiviewVertexShaderTest, InvalidMultipleGlPositionXAssignmentsInIf)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "void main()\n"
        "{\n"
        "    if (gl_ViewID_OVR == 0u)\n"
        "    {\n"
        "        gl_Position.x = 1.0;\n"
        "        gl_Position.x = 2.0;\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_Position.x = 1.0;\n"
        "    }\n"
        "    gl_Position.yzw = vec3(0, 0, 1);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Invalid read of gl_Position
TEST_F(WEBGLMultiviewVertexShaderTest, InvalidReadOfGlPosition)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "void main()\n"
        "{\n"
        "    if (gl_ViewID_OVR == 0u) {\n"
        "        gl_Position.x = 1.0;\n"
        "    } else {\n"
        "        gl_Position.x = 1.0;\n"
        "    }\n"
        "    gl_Position.yzw = vec3(0, 0, 1);\n"
        "    float f = gl_Position.y;\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Read gl_Position when the shader does not refer to gl_ViewID_OVR.
TEST_F(WEBGLMultiviewVertexShaderTest, ValidReadOfGlPosition)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "uniform float u;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(0, 0, 0, 1);\n"
        "    gl_Position.y = gl_Position.x * u;\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Read gl_FragCoord in a OVR_multiview fragment shader.
TEST_F(WEBGLMultiviewFragmentShaderTest, InvalidReadOfFragCoord)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision highp float;\n"
        "out vec4 outColor;\n"
        "void main()\n"
        "{\n"
        "    outColor = vec4(gl_FragCoord.xy, 0, 1);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Read gl_ViewID_OVR in an OVR_multiview fragment shader.
TEST_F(WEBGLMultiviewFragmentShaderTest, InvalidReadOfViewID)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision highp float;\n"
        "out vec4 outColor;\n"
        "void main()\n"
        "{\n"
        "    outColor = vec4(gl_ViewID_OVR, 0, 0, 1);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Tricky invalid read of view ID.
TEST_F(WEBGLMultiviewVertexShaderTest, InvalidConsumingExpressionForAssignGLPositionX)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "void main()\n"
        "{\n"
        "    float f = (gl_Position.x = (gl_ViewID_OVR == 0u) ? 1.0 : 0.0);\n"
        "    gl_Position.yzw = vec3(f, f, f);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Using the OVR_multiview2 extension directive lifts restrictions of OVR_multiview.
TEST_F(WEBGLMultiviewVertexShaderTest, RestrictionsLiftedMultiview2)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "out float out_f;\n"
        "void main()\n"
        "{\n"
        "    if (gl_ViewID_OVR == 0u)\n"
        "    {\n"
        "        gl_Position.x = 1.0;\n"
        "        gl_Position.x = 2.0;\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_Position.x = 1.0;\n"
        "    }\n"
        "    gl_Position.yzw = vec3(0, 0, 1);\n"
        "    gl_Position += vec4(1, 0, 0, 1);\n"
        "    out_f = float(gl_ViewID_OVR * 2u);\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Correct use of GL_OVR_multiview macros.
TEST_F(WEBGLMultiviewVertexShaderTest, ValidUseOfExtensionMacros)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#ifdef GL_OVR_multiview\n"
        "#ifdef GL_OVR_multiview2\n"
        "#if (GL_OVR_multiview == 1) && (GL_OVR_multiview2 == 1)\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "}\n"
        "#endif\n"
        "#endif\n"
        "#endif\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that the parent node is tracked correctly when validating assignment to gl_Position.
TEST_F(WEBGLMultiviewVertexShaderTest, AssignmentWithViewIDInsideAssignment)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "void main()\n"
        "{\n"
        "    gl_Position.y = (gl_Position.x = (gl_ViewID_OVR == 0u) ? 1.0 : 0.0);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that gl_ViewID_OVR can't be used as an l-value.
TEST_F(WEBGLMultiviewVertexShaderTest, ViewIdAsLValue)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "void foo(out uint u)\n"
        "{\n"
        "    u = 3u;\n"
        "}\n"
        "void main()\n"
        "{\n"
        "    foo(gl_ViewID_OVR);\n"
        "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that compiling an ESSL 1.00 shader with multiview support succeeds.
TEST_F(WEBGLMultiviewVertexShaderTest, ESSL1Shader)
{
    const std::string &shaderString =
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "void main()\n"
        "{\n"
        "    if (gl_ViewID_OVR == 0)\n"
        "    {\n"
        "        gl_Position = vec4(-1.0, 0.0, 0.0, 1.0);\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_Position = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "    }\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that compiling an ESSL 1.00 shader with an unsupported global layout qualifier fails.
TEST_F(WEBGLMultiviewVertexShaderTest, ESSL1ShaderUnsupportedGlobalLayoutQualifier)
{
    const std::string &shaderString =
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "layout(std140) uniform;\n"
        "void main()\n"
        "{\n"
        "    if (gl_ViewID_OVR == 0)\n"
        "    {\n"
        "        gl_Position = vec4(-1.0, 0.0, 0.0, 1.0);\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_Position = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "    }\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that compiling an ESSL 1.00 vertex shader with an unsupported input storage qualifier fails.
TEST_F(WEBGLMultiviewVertexShaderTest, ESSL1ShaderUnsupportedInputStorageQualifier)
{
    const std::string &shaderString =
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "in vec4 pos;\n"
        "void main()\n"
        "{\n"
        "    if (gl_ViewID_OVR == 0)\n"
        "    {\n"
        "        gl_Position = vec4(-1.0, 0.0, 0.0, 1.0);\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_Position = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "    }\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that compiling an ESSL 1.00 fragment shader with an unsupported input storage qualifier
// fails.
TEST_F(WEBGLMultiviewFragmentShaderTest, ESSL1ShaderUnsupportedInStorageQualifier)
{
    const std::string &shaderString =
        "#extension GL_OVR_multiview2 : require\n"
        "precision highp float;\n"
        "in vec4 color;\n"
        "void main()\n"
        "{\n"
        "    if (gl_ViewID_OVR == 0)\n"
        "    {\n"
        "        gl_FragColor = color;\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_FragColor = color + vec4(1.0, 0.0, 0.0, 1.0);\n"
        "    }\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that gl_InstanceID gets correctly replaced by InstanceID. gl_InstanceID should only be used
// twice: once to initialize ViewID_OVR and once for InstanceID. The number of occurrences of
// InstanceID in the AST should be the sum of two and the number of occurrences of gl_InstanceID
// before any renaming.
TEST_F(WEBGLMultiviewVertexShaderTest, GLInstanceIDIsRenamed)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "flat out int myInstance;\n"
        "out float myInstanceF;\n"
        "out float myInstanceF2;\n"
        "void main()\n"
        "{\n"
        "   gl_Position.x = gl_ViewID_OVR == 0u ? 0. : 1.;\n"
        "   gl_Position.yzw = vec3(0., 0., 1.);\n"
        "   myInstance = gl_InstanceID;\n"
        "   myInstanceF = float(gl_InstanceID) + .5;\n"
        "   myInstanceF2 = float(gl_InstanceID) + .1;\n"
        "}\n";
    mExtraCompileOptions |= SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW;
    compileAssumeSuccess(shaderString);

    SymbolOccurrenceCounterByName glInstanceIDByName("gl_InstanceID");
    mASTRoot->traverse(&glInstanceIDByName);
    EXPECT_EQ(2u, glInstanceIDByName.getNumberOfOccurrences());

    SymbolOccurrenceCounterByQualifier glInstanceIDByQualifier(EvqInstanceID);
    mASTRoot->traverse(&glInstanceIDByQualifier);
    EXPECT_EQ(2u, glInstanceIDByQualifier.getNumberOfOccurrences());

    SymbolOccurrenceCounterByName instanceIDByName("InstanceID");
    mASTRoot->traverse(&instanceIDByName);
    EXPECT_EQ(5u, instanceIDByName.getNumberOfOccurrences());
}

// Test that gl_ViewID_OVR gets correctly replaced by ViewID_OVR. gl_ViewID_OVR should not be found
// by either name or qualifier. The number of occurrences of ViewID_OVR in the AST should be the sum
// of two and the number of occurrences of gl_ViewID_OVR before any renaming.
TEST_F(WEBGLMultiviewVertexShaderTest, GLViewIDIsRenamed)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "flat out uint a;\n"
        "void main()\n"
        "{\n"
        "   gl_Position.x = gl_ViewID_OVR == 0u ? 0. : 1.;\n"
        "   gl_Position.yzw = vec3(0., 0., 1.);\n"
        "   a = gl_ViewID_OVR == 0u ? (gl_ViewID_OVR+2u) : gl_ViewID_OVR;\n"
        "}\n";
    mExtraCompileOptions |= SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW;
    compileAssumeSuccess(shaderString);

    SymbolOccurrenceCounterByName glViewIDOVRByName("gl_ViewID_OVR");
    mASTRoot->traverse(&glViewIDOVRByName);
    EXPECT_EQ(0u, glViewIDOVRByName.getNumberOfOccurrences());

    SymbolOccurrenceCounterByQualifier glViewIDOVRByQualifier(EvqViewIDOVR);
    mASTRoot->traverse(&glViewIDOVRByQualifier);
    EXPECT_EQ(0u, glViewIDOVRByQualifier.getNumberOfOccurrences());

    SymbolOccurrenceCounterByNameAndQualifier viewIDByNameAndQualifier("ViewID_OVR", EvqFlatOut);
    mASTRoot->traverse(&viewIDByNameAndQualifier);
    EXPECT_EQ(6u, viewIDByNameAndQualifier.getNumberOfOccurrences());
}

// The test checks that ViewID_OVR and InstanceID have the correct initializers based on the
// number of views.
TEST_F(WEBGLMultiviewVertexShaderOutputCodeTest, ViewIDAndInstanceIDHaveCorrectValues)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 3) in;\n"
        "flat out int myInstance;\n"
        "void main()\n"
        "{\n"
        "   gl_Position.x = gl_ViewID_OVR == 0u ? 0. : 1.;\n"
        "   gl_Position.yzw = vec3(0., 0., 1.);\n"
        "   myInstance = gl_InstanceID;\n"
        "}\n";
    compile(shaderString, SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW);

    EXPECT_TRUE(foundInAllGLSLCode("webgl_angle_ViewID_OVR = (uint(gl_InstanceID) % 3u)"));
    EXPECT_TRUE(foundInAllGLSLCode("webgl_angle_InstanceID = (gl_InstanceID / 3)"));

    EXPECT_TRUE(foundInHLSLCode("ViewID_OVR = (uvec1(gl_InstanceID) % 3)"));
    EXPECT_TRUE(foundInHLSLCode("InstanceID = (gl_InstanceID / 3)"));
}

// The test checks that the directive enabling GL_OVR_multiview is not outputted if the extension is
// emulated.
TEST_F(WEBGLMultiviewVertexShaderOutputCodeTest, StrippedOVRMultiviewDirective)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 3) in;\n"
        "void main()\n"
        "{\n"
        "}\n";
    // The directive must not be present if any of the multiview emulation options are set.
    compile(shaderString, SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW);
    EXPECT_FALSE(foundInESSLCode("GL_OVR_multiview"));
    EXPECT_FALSE(foundInGLSLCode("GL_OVR_multiview"));

    compile(shaderString, SH_TRANSLATE_VIEWID_OVR_TO_UNIFORM);
    EXPECT_FALSE(foundInESSLCode("GL_OVR_multiview"));
    EXPECT_FALSE(foundInGLSLCode("GL_OVR_multiview"));

    // The directive should be outputted from the ESSL translator with none of the options being
    // set.
    compile(shaderString);
    EXPECT_TRUE(foundInESSLCode("GL_OVR_multiview"));
}

// Test that gl_InstanceID is collected in an ESSL1 shader if the
// SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW option is set.
TEST_F(WEBGLMultiviewVertexShaderTest, InstaceIDCollectedESSL1)
{
    const std::string &shaderString =
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "void main()\n"
        "{\n"
        "   gl_Position.x = gl_ViewID_OVR == 0 ? 0. : 1.;\n"
        "   gl_Position.yzw = vec3(0., 0., 1.);\n"
        "}\n";
    mExtraCompileOptions |= SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW;
    mExtraCompileOptions |= SH_VARIABLES;
    compileAssumeSuccess(shaderString);

    const std::vector<Attribute> &attributes = getAttributes();
    bool isGLInstanceIDFound                 = false;
    for (size_t i = 0u; i < attributes.size() && !isGLInstanceIDFound; ++i)
    {
        isGLInstanceIDFound = (attributes[i].name == "gl_InstanceID");
    }
    EXPECT_TRUE(isGLInstanceIDFound);
}

// Test that ViewID_OVR is declared as a flat input variable in an ESSL 3.00 fragment shader.
TEST_F(WEBGLMultiviewFragmentShaderTest, ViewIDDeclaredAsFlatInput)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "void main()\n"
        "{\n"
        "}\n";
    mExtraCompileOptions |= SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW;
    compileAssumeSuccess(shaderString);
    VariableOccursNTimes(mASTRoot, "ViewID_OVR", EvqFlatIn, 1u);
}

// Test that ViewID_OVR is declared as a flat output variable in an ESSL 1.00 vertex shader.
TEST_F(WEBGLMultiviewVertexShaderTest, ViewIDDeclaredAsFlatOutput)
{
    const std::string &shaderString =
        "#extension GL_OVR_multiview2 : require\n"
        "void main()\n"
        "{\n"
        "}\n";
    mExtraCompileOptions |= SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW;
    compileAssumeSuccess(shaderString);
    VariableOccursNTimes(mASTRoot, "ViewID_OVR", EvqFlatOut, 2u);
}

}  // namespace
