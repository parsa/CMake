/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include "cmLocalCommonGenerator.h"

class cmGlobalGenerator;
class cmMakefile;

class cmLocalRedoGenerator : public cmLocalCommonGenerator
{
public:
  cmLocalRedoGenerator(cmGlobalGenerator* gg, cmMakefile* makefile);

  void Generate() override;
};

