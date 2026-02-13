/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <memory>
#include <string>

#include "cmCommonTargetGenerator.h"

class cmGeneratorTarget;
class cmLocalRedoGenerator;
class cmSourceFile;

class cmRedoTargetGenerator : public cmCommonTargetGenerator
{
public:
  static cmRedoTargetGenerator* New(cmGeneratorTarget* target,
                                   std::string config);

  cmRedoTargetGenerator(cmGeneratorTarget* target, std::string config);
  ~cmRedoTargetGenerator() override;

  virtual void Generate();

  std::string const& GetConfig() const { return this->Config; }

protected:
  void AddIncludeFlags(std::string& flags, std::string const& lang,
                       std::string const& config) override;

  std::string GetClangTidyReplacementsFilePath(
    std::string const& directory, cmSourceFile const& source,
    std::string const& config) const override;

  cmLocalRedoGenerator* GetLocalGenerator() const { return this->LocalGenerator; }

  cmLocalRedoGenerator* LocalGenerator = nullptr;
  std::string const Config;
};

