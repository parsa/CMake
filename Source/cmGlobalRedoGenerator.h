/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <memory>
#include <string>
#include <vector>

#include "cmBuildOptions.h"
#include "cmGlobalCommonGenerator.h"
#include "cmGlobalGeneratorFactory.h"

class cmake;
class cmLocalGenerator;
class cmMakefile;
struct cmDocumentationEntry;

class cmGlobalRedoGenerator : public cmGlobalCommonGenerator
{
public:
  cmGlobalRedoGenerator(cmake* cm);

  static std::unique_ptr<cmGlobalGeneratorFactory> NewFactory();

  void Generate() override;

  std::unique_ptr<cmLocalGenerator> CreateLocalGenerator(
    cmMakefile* makefile) override;

  std::string GetName() const override { return GetActualName(); }
  static std::string GetActualName() { return "Redo"; }

  static cmDocumentationEntry GetDocumentation();
  static bool SupportsToolset() { return false; }
  static bool SupportsPlatform() { return false; }

  bool IsMultiConfig() const override { return false; }

  bool FindMakeProgram(cmMakefile* mf) override;

  std::vector<GeneratedMakeCommand> GenerateBuildCommand(
    std::string const& makeProgram, std::string const& projectName,
    std::string const& projectDir, std::vector<std::string> const& targetNames,
    std::string const& config, int jobs, bool verbose,
    cmBuildOptions buildOptions = cmBuildOptions(),
    std::vector<std::string> const& makeOptions = std::vector<std::string>(),
    BuildTryCompile isInTryCompile = BuildTryCompile::No) override;

  char const* GetAllTargetName() const override { return "all"; }
  char const* GetCleanTargetName() const override { return "clean"; }
  char const* GetInstallTargetName() const override { return "install"; }
  char const* GetTestTargetName() const override { return "test"; }

  char const* GetCMakeCFGIntDir() const override { return "."; }
};

