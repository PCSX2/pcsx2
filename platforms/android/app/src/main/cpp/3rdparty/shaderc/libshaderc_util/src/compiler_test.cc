// Copyright 2015 The Shaderc Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "libshaderc_util/compiler.h"

#include <gmock/gmock.h>

#include <sstream>

#include "death_test.h"
#include "libshaderc_util/counting_includer.h"
#include "libshaderc_util/spirv_tools_wrapper.h"

namespace {

using shaderc_util::Compiler;
using shaderc_util::GlslangClientInfo;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Not;

// A trivial vertex shader
const char kVertexShader[] =
    "#version 140\n"
    "void main() {}";

// A shader that parses under OpenGL compatibility profile rules.
// It does not compile because Glslang does not support SPIR-V
// code generation for OpenGL compatibility profile.
const char kOpenGLCompatibilityFragShader[] =
    R"(#version 140
       uniform highp sampler2D tex;
       void main() {
         gl_FragColor = texture2D(tex, vec2(0.0,0.0));
       })";

// A shader that compiles under OpenGL core profile rules.
const char kOpenGLVertexShader[] =
    R"(#version 330
       void main() { int t = gl_VertexID; })";

// A shader that compiles under OpenGL core profile rules, even when
// deducing the stage.
const char kOpenGLVertexShaderDeducibleStage[] =
    R"(#version 330
       #pragma shader_stage(vertex)
       void main() { int t = gl_VertexID; })";

// A shader that compiles under Vulkan rules.
// See the GL_KHR_vuklan_glsl extension to GLSL.
const char kVulkanVertexShader[] =
    R"(#version 310 es
       void main() { int t = gl_VertexIndex; })";

// A shader that needs valueless macro predefinition E, to be compiled
// successfully.
const std::string kValuelessPredefinitionShader =
    "#version 140\n"
    "#ifdef E\n"
    "void main(){}\n"
    "#else\n"
    "#error\n"
    "#endif";

// An HLSL vertex shader.
const char kHlslVertexShader[] =
    R"(float4 EntryPoint(uint index : SV_VERTEXID) : SV_POSITION
       { return float4(1.0, 2.0, 3.0, 4.0); })";

// A GLSL fragment shader without bindings for its uniforms.
// This also can be compiled as a vertex or compute shader.
const char kGlslFragShaderNoExplicitBinding[] =
    R"(#version 450
       #extension GL_ARB_sparse_texture2: enable
       uniform texture2D my_tex;
       uniform sampler my_sam;
       layout(rgba32f) uniform image2D my_img;
       layout(rgba32f) uniform imageBuffer my_imbuf;
       uniform block { float x; float y; } my_ubo;
       void main() {
         texture(sampler2D(my_tex,my_sam),vec2(1.0));
         vec4 t = vec4(1.0);
         sparseImageLoadARB(my_img,ivec2(0),t);
         imageLoad(my_imbuf,2);
         float x = my_ubo.x;
       })";

// A GLSL vertex shader with the location defined for its non-opaque uniform
// variable.
const char kGlslVertShaderExplicitLocation[] =
    R"(#version 450
       layout(location = 10) uniform mat4 my_mat;
       layout(location = 0) in vec4 my_vec;
       void main(void) {
         gl_Position = my_mat * my_vec;
       })";

// A GLSL fragment shader with the location defined for its non-opaque uniform
// variable.
const char kGlslFragShaderOpaqueUniforms[] =
    R"(#version 320 es
       precision lowp float;

       layout(location = 0) out vec4 oColor;
       layout(location = 0) uniform float a;
       void main(void) {
         oColor = vec4(1.0, 0.0, 0.0, a);
       })";

// A GLSL vertex shader without the location defined for its non-opaque uniform
// variable.
const char kGlslVertShaderNoExplicitLocation[] =
    R"(#version 450
       uniform mat4 my_mat;
       layout(location = 0) in vec4 my_vec;
       void main(void) {
         gl_Position = my_mat * my_vec;
       })";

// A GLSL vertex shader with a weirdly packed block.
const char kGlslShaderWeirdPacking[] =
    R"(#version 450
       layout(set = 0, binding = 0)
       buffer B { float x; vec3 foo; } my_ssbo;
       void main() { my_ssbo.x = 1.0; })";

const char kHlslShaderForLegalizationTest[] = R"(
struct CombinedTextureSampler {
 Texture2D tex;
 SamplerState sampl;
};

float4 sampleTexture(CombinedTextureSampler c, float2 loc) {
 return c.tex.Sample(c.sampl, loc);
};

[[vk::binding(0,0)]]
Texture2D gTex;
[[vk::binding(0,1)]]
SamplerState gSampler;

float4 main(float2 loc: A) : SV_Target {
 CombinedTextureSampler cts;
 cts.tex = gTex;
 cts.sampl = gSampler;

 return sampleTexture(cts, loc);
})";

const char kHlslShaderWithCounterBuffer[] = R"(
[[vk::binding(0,0)]]
RWStructuredBuffer<float4> Ainc;
float4 main() : SV_Target0 {
  return float4(Ainc.IncrementCounter(), 0, 0, 0);
}
)";

const char kGlslShaderWithClamp[] = R"(#version 450
layout(location=0) in vec4 i;
layout(location=0) out vec4 o;
void main() { o = clamp(i, vec4(0.5), vec4(1.0)); }
)";

// Returns the disassembly of the given SPIR-V binary, as a string.
// Assumes the disassembly will be successful when targeting Vulkan.
std::string Disassemble(const std::vector<uint32_t> binary) {
  std::string result;
  shaderc_util::SpirvToolsDisassemble(Compiler::TargetEnv::Vulkan,
                                      Compiler::TargetEnvVersion::Vulkan_1_3,
                                      binary, &result);
  return result;
}

// A CountingIncluder that never returns valid content for a requested
// file inclusion.
class DummyCountingIncluder : public shaderc_util::CountingIncluder {
 private:
  // Returns a pair of empty strings.
  virtual glslang::TShader::Includer::IncludeResult* include_delegate(
      const char*, const char*, IncludeType, size_t) override {
    return nullptr;
  }
  virtual void release_delegate(
      glslang::TShader::Includer::IncludeResult*) override {}
};

// A test fixture for compiling GLSL shaders.
class CompilerTest : public testing::Test {
 public:
  // Returns true if the given compiler successfully compiles the given shader
  // source for the given shader stage to the specified output type.  No
  // includes are permitted, and shader stage deduction falls back to an invalid
  // shader stage.
  bool SimpleCompilationSucceedsForOutputType(
      std::string source, EShLanguage stage, Compiler::OutputType output_type) {
    shaderc_util::GlslangInitializer initializer;
    std::stringstream errors;
    size_t total_warnings = 0;
    size_t total_errors = 0;
    bool result = false;
    DummyCountingIncluder dummy_includer;
    std::tie(result, std::ignore, std::ignore) = compiler_.Compile(
        source, stage, "shader", "main", dummy_stage_callback_, dummy_includer,
        Compiler::OutputType::SpirvBinary, &errors, &total_warnings,
        &total_errors);
    errors_ = errors.str();
    return result;
  }

  // Returns the result of SimpleCompilationSucceedsForOutputType, where
  // the output type is a SPIR-V binary module.
  bool SimpleCompilationSucceeds(std::string source, EShLanguage stage) {
    return SimpleCompilationSucceedsForOutputType(
        source, stage, Compiler::OutputType::SpirvBinary);
  }

  // Returns the SPIR-V binary for a successful compilation of a shader.
  std::vector<uint32_t> SimpleCompilationBinary(std::string source,
                                                EShLanguage stage) {
    shaderc_util::GlslangInitializer initializer;
    std::stringstream errors;
    size_t total_warnings = 0;
    size_t total_errors = 0;
    bool result = false;
    DummyCountingIncluder dummy_includer;
    std::vector<uint32_t> words;
    std::tie(result, words, std::ignore) = compiler_.Compile(
        source, stage, "shader", "main", dummy_stage_callback_, dummy_includer,
        Compiler::OutputType::SpirvBinary, &errors, &total_warnings,
        &total_errors);
    errors_ = errors.str();
    EXPECT_TRUE(result) << errors_;
    return words;
  }

 protected:
  Compiler compiler_;
  // The error string from the most recent compilation.
  std::string errors_;
  std::function<EShLanguage(std::ostream*, const shaderc_util::string_piece&)>
      dummy_stage_callback_ =
          [](std::ostream*, const shaderc_util::string_piece&) {
            return EShLangCount;
          };
};

TEST_F(CompilerTest, SimpleVertexShaderCompilesSuccessfullyToBinary) {
  EXPECT_TRUE(SimpleCompilationSucceeds(kVertexShader, EShLangVertex));
}

TEST_F(CompilerTest, SimpleVertexShaderCompilesSuccessfullyToAssembly) {
  EXPECT_TRUE(SimpleCompilationSucceedsForOutputType(
      kVertexShader, EShLangVertex, Compiler::OutputType::SpirvAssemblyText));
}

TEST_F(CompilerTest, SimpleVertexShaderPreprocessesSuccessfully) {
  EXPECT_TRUE(SimpleCompilationSucceedsForOutputType(
      kVertexShader, EShLangVertex, Compiler::OutputType::PreprocessedText));
}

TEST_F(CompilerTest, BadVertexShaderFailsCompilation) {
  EXPECT_FALSE(SimpleCompilationSucceeds(" bogus ", EShLangVertex));
}

TEST_F(CompilerTest, SimpleVulkanShaderCompilesWithDefaultCompilerSettings) {
  EXPECT_TRUE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
}

TEST_F(CompilerTest, OpenGLCompatibilityProfileNotSupported) {
  const EShLanguage stage = EShLangVertex;

  compiler_.SetTargetEnv(Compiler::TargetEnv::OpenGLCompat);
  EXPECT_FALSE(SimpleCompilationSucceeds(kOpenGLVertexShader, stage));
  EXPECT_EQ(errors_, "error: OpenGL compatibility profile is not supported");
}

TEST_F(CompilerTest, RespectTargetEnvOnOpenGLShaderForOpenGLShader) {
  const EShLanguage stage = EShLangVertex;

  compiler_.SetTargetEnv(Compiler::TargetEnv::OpenGL);
  EXPECT_TRUE(SimpleCompilationSucceeds(kOpenGLVertexShader, stage));
}

TEST_F(CompilerTest, RespectTargetEnvOnOpenGLShaderWhenDeducingStage) {
  const EShLanguage stage = EShLangVertex;
  compiler_.SetTargetEnv(Compiler::TargetEnv::OpenGL);
  EXPECT_TRUE(
      SimpleCompilationSucceeds(kOpenGLVertexShaderDeducibleStage, stage));
}

TEST_F(CompilerTest, RespectTargetEnvOnVulkanShader) {
  compiler_.SetTargetEnv(Compiler::TargetEnv::Vulkan);
  EXPECT_TRUE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
}

TEST_F(CompilerTest, VulkanSpecificShaderFailsUnderOpenGLCompatibilityRules) {
  compiler_.SetTargetEnv(Compiler::TargetEnv::OpenGLCompat);
  EXPECT_FALSE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
}

TEST_F(CompilerTest, VulkanSpecificShaderFailsUnderOpenGLRules) {
  compiler_.SetTargetEnv(Compiler::TargetEnv::OpenGL);
  EXPECT_FALSE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
}

TEST_F(CompilerTest, OpenGLSpecificShaderFailsUnderDefaultRules) {
  EXPECT_FALSE(SimpleCompilationSucceeds(kOpenGLVertexShader, EShLangVertex));
}

TEST_F(CompilerTest,
       OpenGLCompatibilitySpecificShaderFailsUnderOpenGLCompatibilityRules) {
  // OpenGLCompat mode now errors out.  It's been deprecated for a long time.
  compiler_.SetTargetEnv(Compiler::TargetEnv::OpenGLCompat);
  EXPECT_FALSE(SimpleCompilationSucceeds(kOpenGLCompatibilityFragShader,
                                         EShLangFragment));
}

TEST_F(CompilerTest, OpenGLCompatibilitySpecificShaderFailsUnderOpenGLRules) {
  compiler_.SetTargetEnv(Compiler::TargetEnv::OpenGL);
  EXPECT_FALSE(SimpleCompilationSucceeds(kOpenGLCompatibilityFragShader,
                                         EShLangFragment));
}

TEST_F(CompilerTest, OpenGLCompatibilitySpecificShaderFailsUnderVulkanRules) {
  compiler_.SetTargetEnv(Compiler::TargetEnv::Vulkan);
  EXPECT_FALSE(SimpleCompilationSucceeds(kOpenGLCompatibilityFragShader,
                                         EShLangFragment));
}

TEST_F(CompilerTest, OpenGLSpecificShaderFailsUnderVulkanRules) {
  compiler_.SetTargetEnv(Compiler::TargetEnv::Vulkan);
  EXPECT_FALSE(SimpleCompilationSucceeds(kOpenGLVertexShader, EShLangVertex));
}

TEST_F(CompilerTest, BadTargetEnvFails) {
  compiler_.SetTargetEnv(static_cast<Compiler::TargetEnv>(32767));
  EXPECT_FALSE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
  EXPECT_THAT(errors_, HasSubstr("Invalid target client environment 32767"));
}

TEST_F(CompilerTest, BadTargetEnvVulkanVersionFails) {
  compiler_.SetTargetEnv(Compiler::TargetEnv::Vulkan,
                         static_cast<Compiler::TargetEnvVersion>(123));
  EXPECT_FALSE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
  EXPECT_THAT(
      errors_,
      HasSubstr("Invalid target client version 123 for Vulkan environment 0"));
}

TEST_F(CompilerTest, BadTargetEnvOpenGLVersionFails) {
  compiler_.SetTargetEnv(Compiler::TargetEnv::OpenGL,
                         static_cast<Compiler::TargetEnvVersion>(123));
  EXPECT_FALSE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
  EXPECT_THAT(
      errors_,
      HasSubstr("Invalid target client version 123 for OpenGL environment 1"));
}

TEST_F(CompilerTest, SpirvTargetVersion1_0Succeeds) {
  compiler_.SetTargetSpirv(Compiler::SpirvVersion::v1_0);
  EXPECT_TRUE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
  EXPECT_THAT(errors_, Eq(""));
}

TEST_F(CompilerTest, SpirvTargetVersion1_1Succeeds) {
  compiler_.SetTargetSpirv(Compiler::SpirvVersion::v1_1);
  EXPECT_TRUE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
  EXPECT_THAT(errors_, Eq(""));
}

TEST_F(CompilerTest, SpirvTargetVersion1_2Succeeds) {
  compiler_.SetTargetSpirv(Compiler::SpirvVersion::v1_2);
  EXPECT_TRUE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
  EXPECT_THAT(errors_, Eq(""));
}

TEST_F(CompilerTest, SpirvTargetVersion1_3Succeeds) {
  compiler_.SetTargetSpirv(Compiler::SpirvVersion::v1_3);
  EXPECT_TRUE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
  EXPECT_THAT(errors_, Eq(""));
}

TEST_F(CompilerTest, SpirvTargetVersion1_4Succeeds) {
  compiler_.SetTargetSpirv(Compiler::SpirvVersion::v1_4);
  EXPECT_TRUE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
  EXPECT_THAT(errors_, Eq(""));
}

TEST_F(CompilerTest, SpirvTargetVersion1_5Succeeds) {
  compiler_.SetTargetSpirv(Compiler::SpirvVersion::v1_5);
  EXPECT_TRUE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
  EXPECT_THAT(errors_, Eq(""));
}

TEST_F(CompilerTest, SpirvTargetVersion1_6Succeeds) {
  compiler_.SetTargetSpirv(Compiler::SpirvVersion::v1_6);
  EXPECT_TRUE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
  EXPECT_THAT(errors_, Eq(""));
}

TEST_F(CompilerTest, SpirvTargetBadVersionFails) {
  compiler_.SetTargetSpirv(static_cast<Compiler::SpirvVersion>(0x090900));
  EXPECT_FALSE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
  EXPECT_THAT(errors_, HasSubstr(": Unknown SPIR-V version 90900"));
}

TEST_F(CompilerTest, AddMacroDefinition) {
  const std::string kMinimalExpandedShader = "#version 140\nvoid E(){}";
  compiler_.AddMacroDefinition("E", 1u, "main", 4u);
  EXPECT_TRUE(SimpleCompilationSucceeds(kMinimalExpandedShader, EShLangVertex));
}

TEST_F(CompilerTest, AddValuelessMacroDefinitionNullPointer) {
  compiler_.AddMacroDefinition("E", 1u, nullptr, 100u);
  EXPECT_TRUE(
      SimpleCompilationSucceeds(kValuelessPredefinitionShader, EShLangVertex));
}

TEST_F(CompilerTest, AddValuelessMacroDefinitionZeroLength) {
  compiler_.AddMacroDefinition("E", 1u, "something", 0u);
  EXPECT_TRUE(
      SimpleCompilationSucceeds(kValuelessPredefinitionShader, EShLangVertex));
}

TEST_F(CompilerTest, AddMacroDefinitionNotNullTerminated) {
  const std::string kMinimalExpandedShader = "#version 140\nvoid E(){}";
  compiler_.AddMacroDefinition("EFGH", 1u, "mainnnnnn", 4u);
  EXPECT_TRUE(SimpleCompilationSucceeds(kMinimalExpandedShader, EShLangVertex));
}

// A convert-string-to-vector test case consists of 1) an input string; 2) an
// expected vector after the conversion.
struct ConvertStringToVectorTestCase {
  std::string input_str;
  std::vector<uint32_t> expected_output_vec;
};

// Test the shaderc_util::ConvertStringToVector() function. The content of the
// input string, including the null terminator, should be packed into uint32_t
// cells and stored in the returned vector of uint32_t. In case extra bytes are
// required to complete the ending uint32_t element, bytes with value 0x00
// should be used to fill the space.
using ConvertStringToVectorTestFixture =
    testing::TestWithParam<ConvertStringToVectorTestCase>;

TEST_P(ConvertStringToVectorTestFixture, VariousStringSize) {
  const ConvertStringToVectorTestCase& test_case = GetParam();
  EXPECT_EQ(test_case.expected_output_vec,
            shaderc_util::ConvertStringToVector(test_case.input_str))
      << "test_case.input_str: " << test_case.input_str << std::endl;
}

INSTANTIATE_TEST_SUITE_P(
    ConvertStringToVectorTest, ConvertStringToVectorTestFixture,
    testing::ValuesIn(std::vector<ConvertStringToVectorTestCase>{
        {"", {0x00000000}},
        {"1", {0x00000031}},
        {"12", {0x00003231}},
        {"123", {0x00333231}},
        {"1234", {0x34333231, 0x00000000}},
        {"12345", {0x34333231, 0x00000035}},
        {"123456", {0x34333231, 0x00003635}},
        {"1234567", {0x34333231, 0x00373635}},
        {"12345678", {0x34333231, 0x38373635, 0x00000000}},
        {"123456789", {0x34333231, 0x38373635, 0x00000039}},
    }));

TEST_F(CompilerTest, SetSourceLanguageToGLSLSucceeds) {
  compiler_.SetSourceLanguage(Compiler::SourceLanguage::GLSL);
  EXPECT_TRUE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
}

TEST_F(CompilerTest, SetSourceLanguageToGLSLFailsOnHLSL) {
  compiler_.SetSourceLanguage(Compiler::SourceLanguage::GLSL);
  EXPECT_FALSE(SimpleCompilationSucceeds(kHlslVertexShader, EShLangVertex));
}

TEST_F(CompilerTest, SetSourceLanguageToHLSLSucceeds) {
  compiler_.SetSourceLanguage(Compiler::SourceLanguage::HLSL);
  EXPECT_TRUE(SimpleCompilationSucceeds(kHlslVertexShader, EShLangVertex))
      << errors_;
}

TEST_F(CompilerTest, SetSourceLanguageToHLSLFailsOnGLSL) {
  compiler_.SetSourceLanguage(Compiler::SourceLanguage::HLSL);
  EXPECT_FALSE(SimpleCompilationSucceeds(kVulkanVertexShader, EShLangVertex));
}

TEST_F(CompilerTest, EntryPointParameterTakesEffectForHLSL) {
  compiler_.SetSourceLanguage(Compiler::SourceLanguage::HLSL);
  std::stringstream errors;
  size_t total_warnings = 0;
  size_t total_errors = 0;
  shaderc_util::GlslangInitializer initializer;
  bool result = false;
  DummyCountingIncluder dummy_includer;
  std::vector<uint32_t> words;
  std::tie(result, words, std::ignore) =
      compiler_.Compile(kHlslVertexShader, EShLangVertex, "shader",
                        "EntryPoint", dummy_stage_callback_, dummy_includer,
                        Compiler::OutputType::SpirvAssemblyText, &errors,
                        &total_warnings, &total_errors);
  EXPECT_TRUE(result);
  std::string assembly(reinterpret_cast<char*>(words.data()));
  EXPECT_THAT(assembly,
              HasSubstr("OpEntryPoint Vertex %EntryPoint \"EntryPoint\""))
      << assembly;
}

// A test case for setting resource limits.
struct SetLimitCase {
  Compiler::Limit limit;
  int default_value;
  int value;
};

using LimitTest = testing::TestWithParam<SetLimitCase>;

TEST_P(LimitTest, Sample) {
  Compiler compiler;
  EXPECT_THAT(compiler.GetLimit(GetParam().limit),
              Eq(GetParam().default_value));
  compiler.SetLimit(GetParam().limit, GetParam().value);
  EXPECT_THAT(compiler.GetLimit(GetParam().limit), Eq(GetParam().value));
}

#define CASE(LIMIT, DEFAULT, NEW) \
  { Compiler::Limit::LIMIT, DEFAULT, NEW }
INSTANTIATE_TEST_SUITE_P(CompilerTest, LimitTest,
                         // See resources.cc for the defaults.
                         testing::ValuesIn(std::vector<SetLimitCase>{
                             // clang-format off
        // This is just a sampling of the possible values.
        CASE(MaxLights, 8, 99),
        CASE(MaxClipPlanes, 6, 10929),
        CASE(MaxTessControlAtomicCounters, 0, 72),
        CASE(MaxSamples, 4, 8),
                             // clang-format on
                         }));
#undef CASE

// Returns a fragment shader accessing a texture with the given
// offset.
std::string ShaderWithTexOffset(int offset) {
  std::ostringstream oss;
  oss << "#version 450\n"
         "layout (binding=0) uniform sampler1D tex;\n"
         "void main() { vec4 x = textureOffset(tex, 1.0, "
      << offset << "); }\n";
  return oss.str();
}

// Ensure compilation is sensitive to limit setting.  Sample just
// two particular limits.  The default minimum texel offset is -8,
// and the default maximum texel offset is 7.
TEST_F(CompilerTest, TexelOffsetDefaults) {
  const EShLanguage stage = EShLangFragment;
  EXPECT_FALSE(SimpleCompilationSucceeds(ShaderWithTexOffset(-9), stage));
  EXPECT_TRUE(SimpleCompilationSucceeds(ShaderWithTexOffset(-8), stage));
  EXPECT_TRUE(SimpleCompilationSucceeds(ShaderWithTexOffset(7), stage));
  EXPECT_FALSE(SimpleCompilationSucceeds(ShaderWithTexOffset(8), stage));
}

TEST_F(CompilerTest, TexelOffsetLowerTheMinimum) {
  const EShLanguage stage = EShLangFragment;
  compiler_.SetLimit(Compiler::Limit::MinProgramTexelOffset, -99);
  EXPECT_FALSE(SimpleCompilationSucceeds(ShaderWithTexOffset(-100), stage));
  EXPECT_TRUE(SimpleCompilationSucceeds(ShaderWithTexOffset(-99), stage));
}

TEST_F(CompilerTest, TexelOffsetRaiseTheMaximum) {
  const EShLanguage stage = EShLangFragment;
  compiler_.SetLimit(Compiler::Limit::MaxProgramTexelOffset, 100);
  EXPECT_TRUE(SimpleCompilationSucceeds(ShaderWithTexOffset(100), stage));
  EXPECT_FALSE(SimpleCompilationSucceeds(ShaderWithTexOffset(101), stage));
}

TEST_F(CompilerTest, GeneratorWordIsShadercOverGlslang) {
  const auto words = SimpleCompilationBinary(kVertexShader, EShLangVertex);
  const uint32_t shaderc_over_glslang = 13;  // From SPIR-V XML Registry
  const uint32_t generator_word_index = 2;   // From SPIR-V binary layout
  EXPECT_EQ(shaderc_over_glslang, words[generator_word_index] >> 16u);
}

TEST_F(CompilerTest, NoBindingsAndNoAutoMapBindingsFailsCompile) {
  compiler_.SetAutoBindUniforms(false);
  EXPECT_FALSE(SimpleCompilationSucceeds(kGlslFragShaderNoExplicitBinding,
                                         EShLangFragment));
  EXPECT_THAT(errors_,
              HasSubstr("sampler/texture/image requires layout(binding=X)"));
}

TEST_F(CompilerTest, AutoMapBindingsSetsBindings) {
  compiler_.SetAutoBindUniforms(true);
  const auto words = SimpleCompilationBinary(kGlslFragShaderNoExplicitBinding,
                                             EShLangFragment);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_tex Binding 0"))
      << disassembly;
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_sam Binding 1"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_img Binding 2"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_imbuf Binding 3"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_ubo Binding 4"));
}

TEST_F(CompilerTest, SetBindingBaseForTextureAdjustsTextureBindingsOnly) {
  compiler_.SetAutoBindUniforms(true);
  compiler_.SetAutoBindingBase(Compiler::UniformKind::Texture, 42);
  const auto words = SimpleCompilationBinary(kGlslFragShaderNoExplicitBinding,
                                             EShLangFragment);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_tex Binding 42"))
      << disassembly;
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_sam Binding 0"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_img Binding 1"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_imbuf Binding 2"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_ubo Binding 3"));
}

TEST_F(CompilerTest, SetBindingBaseForSamplersAdjustsSamplerBindingsOnly) {
  compiler_.SetAutoBindUniforms(true);
  compiler_.SetAutoBindingBase(Compiler::UniformKind::Sampler, 42);
  const auto words = SimpleCompilationBinary(kGlslFragShaderNoExplicitBinding,
                                             EShLangFragment);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_tex Binding 0"))
      << disassembly;
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_sam Binding 42"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_img Binding 1"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_imbuf Binding 2"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_ubo Binding 3"));
}

TEST_F(CompilerTest, SetBindingBaseForImagesAdjustsImageBindingsOnly) {
  compiler_.SetAutoBindUniforms(true);
  compiler_.SetAutoBindingBase(Compiler::UniformKind::Image, 42);
  const auto words = SimpleCompilationBinary(kGlslFragShaderNoExplicitBinding,
                                             EShLangFragment);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_tex Binding 0"))
      << disassembly;
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_sam Binding 1"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_img Binding 42"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_imbuf Binding 43"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_ubo Binding 2"));
}

TEST_F(CompilerTest, SetBindingBaseForBufferAdjustsBufferBindingsOnly) {
  compiler_.SetAutoBindUniforms(true);
  compiler_.SetAutoBindingBase(Compiler::UniformKind::Buffer, 42);
  const auto words = SimpleCompilationBinary(kGlslFragShaderNoExplicitBinding,
                                             EShLangFragment);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_tex Binding 0"))
      << disassembly;
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_sam Binding 1"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_img Binding 2"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_imbuf Binding 3"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_ubo Binding 42"));
}

TEST_F(CompilerTest,
       AutoMapBindingsSetsBindingsSetFragTextureBindingBaseCompiledAsFrag) {
  compiler_.SetAutoBindUniforms(true);
  compiler_.SetAutoBindingBaseForStage(Compiler::Stage::Fragment,
                                       Compiler::UniformKind::Texture, 100);
  const auto words = SimpleCompilationBinary(kGlslFragShaderNoExplicitBinding,
                                             EShLangFragment);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_tex Binding 100"))
      << disassembly;
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_sam Binding 0"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_img Binding 1"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_imbuf Binding 2"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_ubo Binding 3"));
}

TEST_F(CompilerTest,
       AutoMapBindingsSetsBindingsSetFragImageBindingBaseCompiledAsVert) {
  compiler_.SetAutoBindUniforms(true);
  // This is ignored because we're compiling the shader as a vertex shader, not
  // as a fragment shader.
  compiler_.SetAutoBindingBaseForStage(Compiler::Stage::Fragment,
                                       Compiler::UniformKind::Image, 100);
  const auto words =
      SimpleCompilationBinary(kGlslFragShaderNoExplicitBinding, EShLangVertex);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_tex Binding 0"))
      << disassembly;
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_sam Binding 1"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_img Binding 2"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_imbuf Binding 3"));
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_ubo Binding 4"));
}

TEST_F(CompilerTest, NoAutoMapLocationsFailsCompilationOnOpenGLShader) {
  compiler_.SetTargetEnv(Compiler::TargetEnv::OpenGL);
  compiler_.SetAutoMapLocations(false);

  const auto words =
      SimpleCompilationBinary(kGlslVertShaderExplicitLocation, EShLangVertex);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, HasSubstr("OpDecorate %my_mat Location 10"))
      << disassembly;

  EXPECT_FALSE(SimpleCompilationSucceeds(kGlslVertShaderNoExplicitLocation,
                                         EShLangVertex));
}

TEST_F(CompilerTest, AutoMapLocationsSetsLocationsOnOpenGLShader) {
  compiler_.SetTargetEnv(Compiler::TargetEnv::OpenGL);
  compiler_.SetAutoMapLocations(true);

  const auto words_no_auto =
      SimpleCompilationBinary(kGlslVertShaderExplicitLocation, EShLangVertex);
  const auto disassembly_no_auto = Disassemble(words_no_auto);
  EXPECT_THAT(disassembly_no_auto, HasSubstr("OpDecorate %my_mat Location 10"))
      << disassembly_no_auto;

  const auto words_auto =
      SimpleCompilationBinary(kGlslVertShaderNoExplicitLocation, EShLangVertex);
  const auto disassembly_auto = Disassemble(words_auto);
  EXPECT_THAT(disassembly_auto, HasSubstr("OpDecorate %my_mat Location 0"))
      << disassembly_auto;
}

TEST_F(CompilerTest, EmitMessageTextOnlyOnce) {
  // Emit a warning by compiling a shader without a default entry point name.
  // The warning should only be emitted once even though we do parsing, linking,
  // and IO mapping.
  Compiler c;
  std::stringstream errors;
  size_t total_warnings = 0;
  size_t total_errors = 0;
  shaderc_util::GlslangInitializer initializer;
  bool result = false;
  DummyCountingIncluder dummy_includer;
  std::tie(result, std::ignore, std::ignore) = c.Compile(
      "#version 150\nvoid MyEntryPoint(){}", EShLangVertex, "shader", "",
      dummy_stage_callback_, dummy_includer, Compiler::OutputType::SpirvBinary,
      &errors, &total_warnings, &total_errors);
  const std::string errs = errors.str();
  EXPECT_THAT(errs, Eq("shader: error: Linking vertex stage: Missing entry "
                       "point: Each stage requires one entry point\n"))
      << errs;
}

TEST_F(CompilerTest, GlslDefaultPackingUsed) {
  const auto words =
      SimpleCompilationBinary(kGlslShaderWeirdPacking, EShLangVertex);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, HasSubstr("OpMemberDecorate %B 1 Offset 16"))
      << disassembly;
}

TEST_F(CompilerTest, HlslOffsetsOptionDisableRespected) {
  compiler_.SetHlslOffsets(false);
  const auto words =
      SimpleCompilationBinary(kGlslShaderWeirdPacking, EShLangVertex);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, HasSubstr("OpMemberDecorate %B 1 Offset 16"))
      << disassembly;
}

TEST_F(CompilerTest, HlslOffsetsOptionEnableRespected) {
  compiler_.SetHlslOffsets(true);
  const auto words =
      SimpleCompilationBinary(kGlslShaderWeirdPacking, EShLangVertex);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, HasSubstr("OpMemberDecorate %B 1 Offset 4"))
      << disassembly;
}

TEST_F(CompilerTest, HlslLegalizationEnabledNoSizeOpt) {
  compiler_.SetSourceLanguage(Compiler::SourceLanguage::HLSL);
  const auto words =
      SimpleCompilationBinary(kHlslShaderForLegalizationTest, EShLangFragment);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, Not(HasSubstr("OpFunctionCall"))) << disassembly;
  EXPECT_THAT(disassembly, HasSubstr("OpName")) << disassembly;
}

TEST_F(CompilerTest, HlslLegalizationEnabledWithSizeOpt) {
  compiler_.SetSourceLanguage(Compiler::SourceLanguage::HLSL);
  compiler_.SetOptimizationLevel(Compiler::OptimizationLevel::Size);
  const auto words =
      SimpleCompilationBinary(kHlslShaderForLegalizationTest, EShLangFragment);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, Not(HasSubstr("OpFunctionCall"))) << disassembly;
  EXPECT_THAT(disassembly, Not(HasSubstr("OpName"))) << disassembly;
}

TEST_F(CompilerTest, HlslLegalizationDisabled) {
  compiler_.SetSourceLanguage(Compiler::SourceLanguage::HLSL);
  compiler_.EnableHlslLegalization(false);
  const auto words =
      SimpleCompilationBinary(kHlslShaderForLegalizationTest, EShLangFragment);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, HasSubstr("OpFunctionCall")) << disassembly;
}

TEST_F(CompilerTest, HlslFunctionality1Enabled) {
  compiler_.SetSourceLanguage(Compiler::SourceLanguage::HLSL);
  compiler_.EnableHlslFunctionality1(true);
  compiler_.SetAutoBindUniforms(true);  // Counter variable needs a binding.
  const auto words =
      SimpleCompilationBinary(kHlslShaderWithCounterBuffer, EShLangFragment);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly,
              HasSubstr("OpExtension \"SPV_GOOGLE_hlsl_functionality1\""))
      << disassembly;
  EXPECT_THAT(disassembly, HasSubstr("OpDecorateString %_entryPointOutput "
                                     "UserSemantic \"SV_TARGET0\""))
      << disassembly;
}

TEST_F(CompilerTest, RelaxedVulkanRulesEnabled) {
  compiler_.SetSourceLanguage(Compiler::SourceLanguage::GLSL);
  compiler_.SetAutoBindUniforms(true);  // Uniform variable needs a binding
  compiler_.SetVulkanRulesRelaxed(true);
  const auto words =
      SimpleCompilationBinary(kGlslFragShaderOpaqueUniforms, EShLangFragment);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly,
              HasSubstr("OpMemberName %gl_DefaultUniformBlock 0 \"a\""))
      << disassembly;
}

TEST_F(CompilerTest, ClampMapsToFClampByDefault) {
  const auto words =
      SimpleCompilationBinary(kGlslShaderWithClamp, EShLangFragment);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, HasSubstr("OpExtInst %v4float %1 FClamp"))
      << disassembly;
}

TEST_F(CompilerTest, ClampMapsToFClampWithNanClamp) {
  compiler_.SetNanClamp(true);
  const auto words =
      SimpleCompilationBinary(kGlslShaderWithClamp, EShLangFragment);
  const auto disassembly = Disassemble(words);
  EXPECT_THAT(disassembly, HasSubstr("OpExtInst %v4float %1 NClamp"))
      << disassembly;
}

// A test coase for Glslang
// expected vector after the conversion.
struct GetGlslangClientInfoCase {
  std::string prefix;
  Compiler::TargetEnv env;
  Compiler::TargetEnvVersion env_version;
  Compiler::SpirvVersion spv_version;
  bool spv_forced;
  // Expected results.  The error field is matched as a substring.
  GlslangClientInfo expected;
};

// Test the shaderc_util::GetGlslangClientInfo function.
using GetGlslangClientInfoTest =
    testing::TestWithParam<GetGlslangClientInfoCase>;

TEST_P(GetGlslangClientInfoTest, Sample) {
  const auto& c = GetParam();
  const auto& expected = c.expected;
  auto result = shaderc_util::GetGlslangClientInfo(
      c.prefix, c.env, c.env_version, c.spv_version, c.spv_forced);

  EXPECT_THAT(result.error.empty(), Eq(expected.error.empty()));
  if (result.error.empty()) {
    EXPECT_THAT(result.client, Eq(expected.client));
    EXPECT_THAT(result.client_version, Eq(expected.client_version));
    EXPECT_THAT(result.target_language, Eq(expected.target_language));
    EXPECT_THAT(result.target_language_version,
                Eq(expected.target_language_version));
  } else {
    EXPECT_THAT(result.error, HasSubstr(expected.error));
  }
}

#define CASE_VK(VKVER, SPVVER)                                                 \
  "", Compiler::TargetEnv::Vulkan, Compiler::TargetEnvVersion::Vulkan_##VKVER, \
      Compiler::SpirvVersion::v##SPVVER

#define BADCASE_VK(STR, VKVER, SPVVER)                \
  STR, Compiler::TargetEnv::Vulkan,                   \
      static_cast<Compiler::TargetEnvVersion>(VKVER), \
      static_cast<Compiler::SpirvVersion>(SPVVER)

#define CASE_GL(GLVER, SPVVER)                                                 \
  "", Compiler::TargetEnv::OpenGL, Compiler::TargetEnvVersion::OpenGL_##GLVER, \
      Compiler::SpirvVersion::v##SPVVER

#define BADCASE_GL(STR, GLVER, SPVVER)                \
  STR, Compiler::TargetEnv::OpenGL,                   \
      static_cast<Compiler::TargetEnvVersion>(GLVER), \
      static_cast<Compiler::SpirvVersion>(SPVVER)

#define GCASE_VK(STR, VKVER, SPVVER)                             \
  shaderc_util::GlslangClientInfo {                              \
    std::string(STR), glslang::EShClientVulkan,                  \
        glslang::EShTargetVulkan_##VKVER, glslang::EShTargetSpv, \
        glslang::EShTargetSpv_##SPVVER                           \
  }

#define GCASE_GL(STR, GLVER, SPVVER)                             \
  shaderc_util::GlslangClientInfo {                              \
    std::string(STR), glslang::EShClientOpenGL,                  \
        glslang::EShTargetOpenGL_##GLVER, glslang::EShTargetSpv, \
        glslang::EShTargetSpv_##SPVVER                           \
  }

INSTANTIATE_TEST_SUITE_P(
    UnforcedSpirvSuccess, GetGlslangClientInfoTest,
    testing::ValuesIn(std::vector<GetGlslangClientInfoCase>{
        // Unforced SPIR-V version. Success cases.
        {CASE_VK(1_0, 1_4), false, GCASE_VK("", 1_0, 1_0)},
        {CASE_VK(1_1, 1_4), false, GCASE_VK("", 1_1, 1_3)},
        {CASE_VK(1_3, 1_6), false, GCASE_VK("", 1_3, 1_6)},
        {CASE_VK(1_4, 1_6), false, GCASE_VK("", 1_4, 1_6)},
        {CASE_GL(4_5, 1_4), false, GCASE_GL("", 450, 1_0)},
    }));

INSTANTIATE_TEST_SUITE_P(
    ForcedSpirvSuccess, GetGlslangClientInfoTest,
    testing::ValuesIn(std::vector<GetGlslangClientInfoCase>{
        // Forced SPIR-V version. Success cases.
        {CASE_VK(1_0, 1_0), true, GCASE_VK("", 1_0, 1_0)},
        {CASE_VK(1_0, 1_1), true, GCASE_VK("", 1_0, 1_1)},
        {CASE_VK(1_0, 1_2), true, GCASE_VK("", 1_0, 1_2)},
        {CASE_VK(1_0, 1_3), true, GCASE_VK("", 1_0, 1_3)},
        {CASE_VK(1_1, 1_0), true, GCASE_VK("", 1_1, 1_0)},
        {CASE_VK(1_1, 1_1), true, GCASE_VK("", 1_1, 1_1)},
        {CASE_VK(1_1, 1_2), true, GCASE_VK("", 1_1, 1_2)},
        {CASE_VK(1_1, 1_3), true, GCASE_VK("", 1_1, 1_3)},
        {CASE_VK(1_3, 1_4), true, GCASE_VK("", 1_3, 1_4)},
        {CASE_VK(1_3, 1_5), true, GCASE_VK("", 1_3, 1_5)},
        {CASE_VK(1_3, 1_6), true, GCASE_VK("", 1_3, 1_6)},
        {CASE_VK(1_4, 1_4), true, GCASE_VK("", 1_4, 1_4)},
        {CASE_VK(1_4, 1_5), true, GCASE_VK("", 1_4, 1_5)},
        {CASE_VK(1_4, 1_6), true, GCASE_VK("", 1_4, 1_6)},
        {CASE_GL(4_5, 1_0), true, GCASE_GL("", 450, 1_0)},
        {CASE_GL(4_5, 1_1), true, GCASE_GL("", 450, 1_1)},
        {CASE_GL(4_5, 1_2), true, GCASE_GL("", 450, 1_2)},
    }));

INSTANTIATE_TEST_SUITE_P(
    Failure, GetGlslangClientInfoTest,
    testing::ValuesIn(std::vector<GetGlslangClientInfoCase>{
        // Failure cases.
        {BADCASE_VK("foo", 999, Compiler::SpirvVersion::v1_0), false,
         GCASE_VK("error:foo: Invalid target client version 999 for Vulkan "
                  "environment 0",
                  1_0, 1_0)},
        {BADCASE_GL("foo", 999, Compiler::SpirvVersion::v1_0), false,
         GCASE_GL("error:foo: Invalid target client version 999 for OpenGL "
                  "environment 1",
                  450, 1_0)},
        // For bad SPIR-V versions, have to force=true to make it pay attention.
        {BADCASE_VK("foo", Compiler::TargetEnvVersion::Vulkan_1_0, 999), true,
         GCASE_VK("error:foo: Unknown SPIR-V version 3e7", 1_0, 1_0)},
        {BADCASE_GL("foo", Compiler::TargetEnvVersion::OpenGL_4_5, 999), true,
         GCASE_GL("error:foo: Unknown SPIR-V version 3e7", 450, 1_0)},
    }));

#undef CASE_VK
#undef CASE_GL
#undef BADCASE_VK
#undef BADCASE_GL
#undef GCASE_VK
#undef GCASE_GL
}  // anonymous namespace
