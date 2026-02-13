/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */

#include "cmLocalRedoGenerator.h"

#include <memory>
#include <utility>

#include "cmGeneratorTarget.h"
#include "cmMakefile.h"

#include "cmRedoTargetGenerator.h"

cmLocalRedoGenerator::cmLocalRedoGenerator(cmGlobalGenerator* gg,
                                           cmMakefile* makefile)
  : cmLocalCommonGenerator(gg, makefile)
{
}

void cmLocalRedoGenerator::Generate()
{
  auto const& targets = this->GetGeneratorTargets();
  for (auto const& target : targets) {
    if (!target->IsInBuildSystem()) {
      continue;
    }
    for (std::string config : this->GetConfigNames()) {
      std::unique_ptr<cmRedoTargetGenerator> tg(
        cmRedoTargetGenerator::New(target.get(), std::move(config)));
      if (tg) {
        tg->Generate();
      }
    }
  }
}

