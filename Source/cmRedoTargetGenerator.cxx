/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */

#include "cmRedoTargetGenerator.h"

#include <cctype>
#include <cstddef>
#include <algorithm>
#include <map>
#include <set>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "cmComputeLinkInformation.h"
#include "cmCryptoHash.h"
#include "cmCustomCommand.h"
#include "cmCustomCommandGenerator.h"
#include "cmGeneratedFileStream.h"
#include "cmGeneratorTarget.h"
#include "cmGeneratorExpression.h"
#include "cmGlobalGenerator.h"
#include "cmLinkLineComputer.h"
#include "cmList.h"
#include "cmLocalGenerator.h"
#include "cmMakefile.h"
#include "cmOutputConverter.h"
#include "cmRulePlaceholderExpander.h"
#include "cmSourceFile.h"
#include "cmState.h"
#include "cmStringAlgorithms.h"
#include "cmSystemTools.h"
#include "cmake.h"

#include "cmLocalRedoGenerator.h"

namespace {

std::string QuoteForSh(std::string const& s)
{
  std::string out;
  out.reserve(s.size() + 2);
  out.push_back('\'');
  for (char c : s) {
    if (c == '\'') {
      out.append("'\\''");
    } else {
      out.push_back(c);
    }
  }
  out.push_back('\'');
  return out;
}

void EmitRedoIfchange(std::vector<std::string>& lines,
                      std::vector<std::string> const& deps,
                      std::size_t maxArgsPerLine = 128)
{
  if (deps.empty()) {
    return;
  }

  std::size_t i = 0;
  while (i < deps.size()) {
    std::string cmd = "redo-ifchange";
    std::size_t n = 0;
    for (; i < deps.size() && n < maxArgsPerLine; ++i, ++n) {
      cmd.push_back(' ');
      cmd.append(QuoteForSh(deps[i]));
    }
    lines.emplace_back(std::move(cmd));
  }
}

void ReplaceShellArgument(std::string& cmd, std::string const& arg,
                          std::string const& replacement)
{
  if (arg.empty()) {
    return;
  }

  std::string::size_type pos = 0;
  while ((pos = cmd.find(arg, pos)) != std::string::npos) {
    std::string::size_type const end = pos + arg.size();

    bool const beforeOk =
      (pos == 0) ||
      std::isspace(static_cast<unsigned char>(cmd[pos - 1])) != 0;
    bool const afterOk =
      (end == cmd.size()) ||
      std::isspace(static_cast<unsigned char>(cmd[end])) != 0;

    if (beforeOk && afterOk) {
      cmd.replace(pos, arg.size(), replacement);
      pos += replacement.size();
    } else {
      pos = end;
    }
  }
}

void ReplaceShellPath(std::string& cmd, std::string const& path,
                      std::string const& replacement)
{
  if (path.empty()) {
    return;
  }

  std::string::size_type pos = 0;
  while ((pos = cmd.find(path, pos)) != std::string::npos) {
    std::string::size_type const end = pos + path.size();

    auto const isBeforeBoundary = [&](char c) -> bool {
      return std::isspace(static_cast<unsigned char>(c)) != 0 || c == '=' ||
        c == '"' || c == '\'';
    };
    auto const isAfterBoundary = [&](char c) -> bool {
      return std::isspace(static_cast<unsigned char>(c)) != 0 || c == '"' ||
        c == '\'';
    };

    bool const beforeOk = (pos == 0) || isBeforeBoundary(cmd[pos - 1]);
    bool const afterOk = (end == cmd.size()) || isAfterBoundary(cmd[end]);

    if (beforeOk && afterOk) {
      cmd.replace(pos, path.size(), replacement);
      pos += replacement.size();
    } else {
      pos = end;
    }
  }
}

void WriteDoFile(std::string const& path, std::vector<std::string> const& lines)
{
  cmSystemTools::MakeDirectory(cmSystemTools::GetFilenamePath(path));
  cmGeneratedFileStream file(path);
  file.SetCopyIfDifferent(true);
  for (auto const& line : lines) {
    file << line << '\n';
  }
}

std::string ToBuildRelPath(std::string const& topBinary,
                           std::string const& path)
{
  if (path.empty()) {
    return path;
  }
  if (cmSystemTools::FileIsFullPath(path)) {
    return cmSystemTools::RelativePath(topBinary, path);
  }
  return path;
}

} // namespace

cmRedoTargetGenerator* cmRedoTargetGenerator::New(cmGeneratorTarget* target,
                                                  std::string config)
{
  switch (target->GetType()) {
    case cmStateEnums::EXECUTABLE:
    case cmStateEnums::SHARED_LIBRARY:
    case cmStateEnums::STATIC_LIBRARY:
    case cmStateEnums::MODULE_LIBRARY:
    case cmStateEnums::OBJECT_LIBRARY:
    case cmStateEnums::UTILITY:
    case cmStateEnums::GLOBAL_TARGET:
    case cmStateEnums::INTERFACE_LIBRARY:
      return new cmRedoTargetGenerator(target, std::move(config));
    default:
      return nullptr;
  }
}

cmRedoTargetGenerator::cmRedoTargetGenerator(cmGeneratorTarget* target,
                                             std::string configParam)
  : cmCommonTargetGenerator(target)
  , LocalGenerator(
      static_cast<cmLocalRedoGenerator*>(target->GetLocalGenerator()))
  , Config(std::move(configParam))
{
}

cmRedoTargetGenerator::~cmRedoTargetGenerator() = default;

void cmRedoTargetGenerator::Generate()
{
  std::string const topBinary =
    this->LocalGenerator->GetGlobalGenerator()->GetCMakeInstance()->GetHomeOutputDirectory();

  // We emit shell scripts, not makefile fragments. Avoid make-style escaping.
  this->LocalGenerator->SetLinkScriptShell(true);

  auto const resolvePath = [&](std::string const& p) -> std::string {
    if (p.empty()) {
      return p;
    }
    if (cmSystemTools::FileIsFullPath(p)) {
      return p;
    }
    std::string const candBin =
      cmSystemTools::CollapseFullPath(p, this->Makefile->GetCurrentBinaryDirectory());
    std::string const candSrc =
      cmSystemTools::CollapseFullPath(p, this->Makefile->GetCurrentSourceDirectory());
    return cmSystemTools::FileExists(candBin, true) ? candBin : candSrc;
  };

  // Job pool definitions (mirrors Ninja generator's JOB_POOLS parsing).
  std::map<std::string, unsigned int> poolDepthByName;
  {
    cmValue jobpools =
      this->LocalGenerator->GetGlobalGenerator()->GetCMakeInstance()->GetState()->GetGlobalProperty(
        "JOB_POOLS");
    if (!jobpools) {
      jobpools = this->Makefile->GetDefinition("CMAKE_JOB_POOLS");
    }
    if (jobpools) {
      cmList pools{ *jobpools };
      for (std::string const& pool : pools) {
        std::string::size_type const eq = pool.find('=');
        unsigned int jobs;
        if (eq != std::string::npos &&
            sscanf(pool.c_str() + eq, "=%u", &jobs) == 1) {
          poolDepthByName[pool.substr(0, eq)] = jobs;
        } else {
          cmSystemTools::Error(
            cmStrCat("Invalid pool defined by property 'JOB_POOLS': ", pool));
        }
      }
    }
  }
  std::set<std::string> missingPoolsReported;
  auto const poolDepthFor = [&](std::string const& poolName) -> unsigned int {
    if (poolName.empty()) {
      return 0;
    }
    // Built-in console pool (also used for USES_TERMINAL).
    if (poolName == "console") {
      return 1;
    }
    auto it = poolDepthByName.find(poolName);
    if (it != poolDepthByName.end()) {
      return it->second;
    }
    if (missingPoolsReported.insert(poolName).second) {
      cmSystemTools::Error(cmStrCat(
        "Job pool '", poolName,
        "' was requested but not defined by global property JOB_POOLS."));
    }
    return 0;
  };

  // Emit .do files for custom command OUTPUT files. Returns the absolute output
  // file paths.
  auto const emitCustomCommandOutputDofiles =
    [&](std::vector<cmSourceFile const*> const& ccSources) {
      std::vector<std::string> outputsAbs;

      // Dedupe custom commands: multi-output commands appear once per output
      // source file.
      std::set<cmCustomCommand const*> uniqueCCs;
      for (cmSourceFile const* sf : ccSources) {
        if (!sf) {
          continue;
        }
        if (cmCustomCommand const* cc = sf->GetCustomCommand()) {
          uniqueCCs.insert(cc);
        }
      }

      auto const resolveOutputPath = [&](std::string const& p) -> std::string {
        if (p.empty()) {
          return p;
        }
        if (cmSystemTools::FileIsFullPath(p)) {
          return p;
        }
        // Custom command outputs are interpreted relative to the current binary dir.
        return cmSystemTools::CollapseFullPath(p, this->Makefile->GetCurrentBinaryDirectory());
      };

      for (cmCustomCommand const* cc : uniqueCCs) {
        cmCustomCommandGenerator ccg(*cc, this->Config, this->LocalGenerator);

        std::vector<std::string> outsAbs;
        for (auto const& o : ccg.GetOutputs()) {
          std::string const outAbs = resolveOutputPath(o);
          if (!outAbs.empty()) {
            outsAbs.emplace_back(outAbs);
          }
        }
        std::vector<std::string> bypsAbs;
        for (auto const& b : ccg.GetByproducts()) {
          std::string const outAbs = resolveOutputPath(b);
          if (!outAbs.empty()) {
            bypsAbs.emplace_back(outAbs);
          }
        }

        if (outsAbs.empty() && bypsAbs.empty()) {
          continue;
        }

        std::sort(outsAbs.begin(), outsAbs.end());
        outsAbs.erase(std::unique(outsAbs.begin(), outsAbs.end()), outsAbs.end());
        std::sort(bypsAbs.begin(), bypsAbs.end());
        bypsAbs.erase(std::unique(bypsAbs.begin(), bypsAbs.end()), bypsAbs.end());

        std::vector<std::string> allOutAbs;
        allOutAbs.reserve(outsAbs.size() + bypsAbs.size());
        allOutAbs.insert(allOutAbs.end(), outsAbs.begin(), outsAbs.end());
        allOutAbs.insert(allOutAbs.end(), bypsAbs.begin(), bypsAbs.end());
        std::sort(allOutAbs.begin(), allOutAbs.end());
        allOutAbs.erase(std::unique(allOutAbs.begin(), allOutAbs.end()),
                        allOutAbs.end());

        // Detect directory outputs: if one declared output is a prefix of another,
        // treat the prefix as a directory output. Nested outputs would require
        // `.do` files inside the output directory, which would be clobbered when
        // the directory output is rebuilt, so skip emitting nested `.do` files.
        std::set<std::string> dirOutputsAbs;
        for (auto const& p : allOutAbs) {
          std::string const prefix = cmStrCat(p, '/');
          for (auto const& q : allOutAbs) {
            if (q.size() > prefix.size() && cmHasPrefix(q, prefix)) {
              dirOutputsAbs.insert(p);
              break;
            }
          }
        }

        auto const isNestedUnderDirOutput = [&](std::string const& p) -> bool {
          for (auto const& d : dirOutputsAbs) {
            std::string const prefix = cmStrCat(d, '/');
            if (cmHasPrefix(p, prefix)) {
              return true;
            }
          }
          return false;
        };

        std::vector<std::string> buildableOutAbs;
        buildableOutAbs.reserve(allOutAbs.size());
        for (auto const& o : allOutAbs) {
          if (!isNestedUnderDirOutput(o)) {
            buildableOutAbs.emplace_back(o);
          }
        }
        if (buildableOutAbs.empty()) {
          continue;
        }

        // Dependencies (shared by both phony and real commands).
        std::vector<std::string> depsAbs;
        for (auto const& dep : ccg.GetDepends()) {
          std::string const depAbs = resolvePath(dep);
          if (!depAbs.empty()) {
            depsAbs.emplace_back(depAbs);
          }
        }
        std::sort(depsAbs.begin(), depsAbs.end());
        depsAbs.erase(std::unique(depsAbs.begin(), depsAbs.end()), depsAbs.end());

        // Phony custom commands: no commands, only (order) dependencies.
        if (ccg.HasOnlyEmptyCommandLines()) {
          for (auto const& outAbs : buildableOutAbs) {
            outputsAbs.emplace_back(outAbs);

            bool const isDirOutput = dirOutputsAbs.count(outAbs) != 0;
            std::string const outRel = ToBuildRelPath(topBinary, outAbs);
            std::string const doPath =
              (!outRel.empty() && outRel.rfind("..", 0) != 0)
              ? cmStrCat(topBinary, '/', outRel, ".do")
              : cmStrCat(outAbs, ".do");

            std::vector<std::string> lines;
            lines.emplace_back("# Generated by CMake Redo generator.");
            lines.emplace_back("exec 1>&2");
            EmitRedoIfchange(lines, depsAbs);
            if (isDirOutput) {
              lines.emplace_back("rm -rf \"$3\"");
              lines.emplace_back("mkdir -p \"$3\"");
              lines.emplace_back("rm -rf \"$1\"");
            } else {
              lines.emplace_back(": >\"$3\"");
            }
            WriteDoFile(doPath, lines);
          }
          continue;
        }

        std::string const mainOutAbs = !outsAbs.empty() ? outsAbs.front()
          : bypsAbs.front();

        cmCryptoHash hash(cmCryptoHash::AlgoSHA256);
        std::string const id = hash.HashString(cmStrCat(mainOutAbs, '|', this->Config)).substr(0, 12);
        std::string const stampAbs =
          cmStrCat(topBinary, "/CMakeFiles/RedoCustom/", id, ".stamp");
        std::string const stageRootAbs =
          cmStrCat(topBinary, "/CMakeFiles/RedoCustom/", id, ".stage");

        struct OutMap
        {
          std::string OutAbs;
          std::string OutShell;
          std::string StageAbs;
          std::string StageShell;
        };
        std::vector<OutMap> outMaps;
        outMaps.reserve(allOutAbs.size());

        auto const stagePathFor = [&](std::string const& outAbs) -> std::string {
          std::string const rel = ToBuildRelPath(topBinary, outAbs);
          if (rel.empty() || rel.rfind("..", 0) == 0) {
            cmCryptoHash h2(cmCryptoHash::AlgoSHA256);
            return cmStrCat(stageRootAbs, '/', h2.HashString(outAbs).substr(0, 12));
          }
          return cmStrCat(stageRootAbs, '/', rel);
        };

        for (auto const& outAbs : allOutAbs) {
          OutMap m;
          m.OutAbs = outAbs;
          m.OutShell = this->LocalGenerator->ConvertToOutputFormat(
            outAbs, cmOutputConverter::SHELL);
          m.StageAbs = stagePathFor(outAbs);
          m.StageShell = this->LocalGenerator->ConvertToOutputFormat(
            m.StageAbs, cmOutputConverter::SHELL);
          outMaps.emplace_back(std::move(m));
        }

        // Stamp/driver target: runs the custom command once to populate the staging area.
        {
          std::vector<std::string> lines;
          lines.emplace_back("# Generated by CMake Redo generator.");
          if (cc->GetUsesTerminal()) {
            lines.emplace_back("# redo-pool: console 1");
          } else if (!cc->GetJobPool().empty()) {
            unsigned int const depth = poolDepthFor(cc->GetJobPool());
            if (depth > 0) {
              lines.emplace_back(
                cmStrCat("# redo-pool: ", cc->GetJobPool(), ' ', depth));
            }
          }
          lines.emplace_back("exec 1>&2");
          lines.emplace_back(cmStrCat("stage_root=", QuoteForSh(stageRootAbs)));
          lines.emplace_back("rm -rf \"$stage_root\"");
          lines.emplace_back("mkdir -p \"$stage_root\"");
          for (auto const& m : outMaps) {
            std::string const parent = cmSystemTools::GetFilenamePath(m.StageAbs);
            if (!parent.empty()) {
              lines.emplace_back(cmStrCat("mkdir -p ", QuoteForSh(parent)));
            }
          }

          EmitRedoIfchange(lines, depsAbs);

          // Working directory.
          std::string wd = ccg.GetWorkingDirectory();
          if (wd.empty()) {
            wd = this->Makefile->GetCurrentBinaryDirectory();
          }
          if (!wd.empty()) {
            lines.emplace_back(cmStrCat("cd ", QuoteForSh(wd)));
          }

          // Commands: rewrite declared outputs/byproducts to the staging paths.
          for (unsigned int i = 0; i < ccg.GetNumberOfCommands(); ++i) {
            std::string cmd = ccg.GetCommand(i);
            ccg.AppendArguments(i, cmd);
            if (!cmd.empty()) {
              for (auto const& m : outMaps) {
                ReplaceShellPath(cmd, m.OutShell, m.StageShell);
                ReplaceShellPath(cmd, m.OutAbs, m.StageAbs);
              }
              lines.emplace_back(std::move(cmd));
            }
          }

          lines.emplace_back(": >\"$3\"");
          std::string const stampRel = ToBuildRelPath(topBinary, stampAbs);
          std::string const stampDoPath =
            (!stampRel.empty() && stampRel.rfind("..", 0) != 0)
            ? cmStrCat(topBinary, '/', stampRel, ".do")
            : cmStrCat(stampAbs, ".do");
          WriteDoFile(stampDoPath, lines);
        }

        // Output/byproduct targets: depend on the stamp and copy from staging to $3.
        for (auto const& outAbs : buildableOutAbs) {
          outputsAbs.emplace_back(outAbs);

          std::string stageAbs;
          bool isDirOutput = false;
          for (auto const& m : outMaps) {
            if (m.OutAbs == outAbs) {
              stageAbs = m.StageAbs;
              break;
            }
          }
          if (stageAbs.empty()) {
            continue;
          }
          isDirOutput = dirOutputsAbs.count(outAbs) != 0;

          std::string const outRel = ToBuildRelPath(topBinary, outAbs);
          std::string const doPath =
            (!outRel.empty() && outRel.rfind("..", 0) != 0)
            ? cmStrCat(topBinary, '/', outRel, ".do")
            : cmStrCat(outAbs, ".do");

          std::vector<std::string> lines;
          lines.emplace_back("# Generated by CMake Redo generator.");
          lines.emplace_back("exec 1>&2");
          lines.emplace_back(cmStrCat("redo-ifchange ", QuoteForSh(stampAbs)));

          if (isDirOutput) {
            lines.emplace_back(cmStrCat("src=", QuoteForSh(stageAbs)));
            lines.emplace_back("if [ ! -d \"$src\" ]; then");
            lines.emplace_back(
              cmStrCat("  echo \"missing staged directory: ", stageAbs, "\" >&2"));
            lines.emplace_back("  exit 1");
            lines.emplace_back("fi");
            lines.emplace_back("rm -rf \"$3\"");
            lines.emplace_back("cp -R \"$src\" \"$3\"");
            lines.emplace_back("rm -rf \"$1\"");
          } else {
            lines.emplace_back(cmStrCat("src=", QuoteForSh(stageAbs)));
            lines.emplace_back("if [ ! -e \"$src\" ]; then");
            lines.emplace_back(
              cmStrCat("  echo \"missing staged file: ", stageAbs, "\" >&2"));
            lines.emplace_back("  exit 1");
            lines.emplace_back("fi");
            lines.emplace_back("cp -f \"$src\" \"$3\"");
          }

          WriteDoFile(doPath, lines);
        }
      }

      std::sort(outputsAbs.begin(), outputsAbs.end());
      outputsAbs.erase(std::unique(outputsAbs.begin(), outputsAbs.end()),
                       outputsAbs.end());
      return outputsAbs;
    };

  // Utility target support: treat as a stamp that depends on utility items.
  if (this->GeneratorTarget->GetType() == cmStateEnums::UTILITY) {
    std::vector<std::string> lines;
    lines.emplace_back("# Generated by CMake Redo generator.");
    lines.emplace_back("exec 1>&2");

    // Source-associated custom commands (codegen outputs).
    std::vector<cmSourceFile const*> ccSources;
    this->GeneratorTarget->GetCustomCommands(ccSources, this->Config);
    EmitRedoIfchange(lines, emitCustomCommandOutputDofiles(ccSources));

    // Target dependencies.
    std::vector<std::string> utilDeps;
    for (auto const& item : this->GeneratorTarget->GetUtilityItems()) {
      if (item.Target) {
        utilDeps.emplace_back(item.Target->GetName());
      } else if (!item.AsStr().empty()) {
        utilDeps.emplace_back(item.AsStr());
      }
    }
    std::sort(utilDeps.begin(), utilDeps.end());
    utilDeps.erase(std::unique(utilDeps.begin(), utilDeps.end()), utilDeps.end());
    EmitRedoIfchange(lines, utilDeps);

    // Utility targets store their rules in pre/post build commands.
    auto emitCommands = [&](std::vector<cmCustomCommand> const& cmds) {
      for (auto const& c : cmds) {
        cmCustomCommandGenerator ccg(c, this->Config, this->LocalGenerator);
        for (unsigned int i = 0; i < ccg.GetNumberOfCommands(); ++i) {
          std::string cmd = ccg.GetCommand(i);
          ccg.AppendArguments(i, cmd);
          if (!cmd.empty()) {
            lines.emplace_back(std::move(cmd));
          }
        }
      }
    };
    emitCommands(this->GeneratorTarget->GetPreBuildCommands());
    emitCommands(this->GeneratorTarget->GetPostBuildCommands());

    // Stamp the target.
    lines.emplace_back(": >\"$3\"");
    WriteDoFile(cmStrCat(topBinary, '/', this->GeneratorTarget->GetName(), ".do"),
                lines);
    return;
  }

  // Phase_1: handle targets that produce a linkable artifact.
  switch (this->GeneratorTarget->GetType()) {
    case cmStateEnums::EXECUTABLE:
    case cmStateEnums::SHARED_LIBRARY:
    case cmStateEnums::STATIC_LIBRARY:
    case cmStateEnums::MODULE_LIBRARY:
      break;
    default:
      return;
  }

  std::string const outputAbs =
    this->GeneratorTarget->GetFullPath(this->Config,
                                       cmStateEnums::RuntimeBinaryArtifact,
                                       /*realname=*/true);
  std::string const outputRel = ToBuildRelPath(topBinary, outputAbs);

  // Alias target: <targetName>.do depends on the primary output file.
  {
    std::vector<std::string> lines;
    lines.emplace_back("# Generated by CMake Redo generator.");
    lines.emplace_back(cmStrCat("redo-ifchange ", QuoteForSh(outputAbs)));
    lines.emplace_back(": >\"$3\"");
    WriteDoFile(cmStrCat(topBinary, '/', this->GeneratorTarget->GetName(), ".do"),
                lines);
  }

  // Custom commands associated with this target's sources.
  std::vector<cmSourceFile const*> ccSources;
  this->GeneratorTarget->GetCustomCommands(ccSources, this->Config);
  std::vector<std::string> const customOutputsAbs =
    emitCustomCommandOutputDofiles(ccSources);

  // Object compilation: emit one .do per object file.
  std::vector<cmSourceFile const*> sources;
  this->GeneratorTarget->GetObjectSources(sources, this->Config);

  std::map<std::string, std::string> compileRuleByLang;
  for (std::string const& lang : { std::string("C"), std::string("CXX") }) {
    std::string const var = cmStrCat("CMAKE_", lang, "_COMPILE_OBJECT");
    std::string const rule = this->Makefile->GetSafeDefinition(var);
    if (!rule.empty()) {
      compileRuleByLang.emplace(lang, rule);
    }
  }

  std::vector<std::string> objectRelPaths;
  objectRelPaths.reserve(sources.size());

  std::string const targetName = this->GeneratorTarget->GetName();
  std::string const targetTypeName =
    cmState::GetTargetTypeName(this->GeneratorTarget->GetType());

  std::string const supportDir =
    ToBuildRelPath(topBinary, this->GeneratorTarget->GetSupportDirectory());

  for (cmSourceFile const* source : sources) {
    if (!source) {
      continue;
    }
    std::string const language = source->GetLanguage();
    if (language != "C" && language != "CXX") {
      continue;
    }
    auto it = compileRuleByLang.find(language);
    if (it == compileRuleByLang.end()) {
      continue;
    }

    std::string const& objectName = this->GeneratorTarget->GetObjectName(source);
    std::string const objectRel =
      supportDir.empty() ? objectName : cmStrCat(supportDir, '/', objectName);
    objectRelPaths.emplace_back(objectRel);

    std::string sourceAbs = source->GetFullPath();
    if (sourceAbs.empty()) {
      sourceAbs = source->GetLocation().GetFullPath();
    }
    if (sourceAbs.empty()) {
      continue;
    }
    if (!cmSystemTools::FileIsFullPath(sourceAbs)) {
      std::string const candBin = cmSystemTools::CollapseFullPath(
        sourceAbs, this->Makefile->GetCurrentBinaryDirectory());
      std::string const candSrc = cmSystemTools::CollapseFullPath(
        sourceAbs, this->Makefile->GetCurrentSourceDirectory());
      sourceAbs = cmSystemTools::FileExists(candBin, true) ? candBin : candSrc;
    }
    std::string const sourceShell =
      this->LocalGenerator->ConvertToOutputFormat(sourceAbs, cmOutputConverter::SHELL);

    std::string flags = this->GetFlags(language, this->Config);
    // Add source file specific flags and options.
    cmGeneratorExpressionInterpreter genexInterpreter(
      this->LocalGenerator, this->Config, this->GeneratorTarget, language);
    {
      std::string const COMPILE_FLAGS("COMPILE_FLAGS");
      if (cmValue cflags = source->GetProperty(COMPILE_FLAGS)) {
        this->LocalGenerator->AppendFlags(
          flags, genexInterpreter.Evaluate(*cflags, COMPILE_FLAGS));
      }
    }
    {
      std::string const COMPILE_OPTIONS("COMPILE_OPTIONS");
      if (cmValue coptions = source->GetProperty(COMPILE_OPTIONS)) {
        this->LocalGenerator->AppendCompileOptions(
          flags, genexInterpreter.Evaluate(*coptions, COMPILE_OPTIONS));
      }
    }
    std::string defines = this->GetDefines(language, this->Config);
    std::string includes = this->GetIncludes(language, this->Config);

    cmRulePlaceholderExpander::RuleVariables vars;
    vars.CMTargetName = targetName.c_str();
    vars.CMTargetType = targetTypeName.c_str();
    vars.Language = language.c_str();
    vars.Source = sourceShell.c_str();
    vars.Object = "\"$3\"";
    vars.ObjectDir = supportDir.c_str();
    vars.Flags = flags.c_str();
    vars.Defines = defines.c_str();
    vars.Includes = includes.c_str();
    vars.Config = this->Config.c_str();

    std::string compileCmd = it->second;
    auto rulePlaceholderExpander =
      this->LocalGenerator->CreateRulePlaceholderExpander(cmBuildStep::Compile);
    rulePlaceholderExpander->ExpandRuleVariables(this->LocalGenerator, compileCmd,
                                                 vars);
    // Emit a Makefile-style depfile. We store it next to the object as $1.d.
    compileCmd = cmStrCat(compileCmd, " -MMD -MF \"$3.d\"");

    std::vector<std::string> lines;
    lines.emplace_back("# Generated by CMake Redo generator.");
    std::string poolName;
    if (cmValue p = source->GetProperty("JOB_POOL_COMPILE")) {
      poolName = *p;
    }
    if (poolName.empty()) {
      if (cmValue p = this->GeneratorTarget->GetProperty("JOB_POOL_COMPILE")) {
        poolName = *p;
      }
    }
    if (!poolName.empty()) {
      unsigned int const depth = poolDepthFor(poolName);
      if (depth > 0) {
        lines.emplace_back(cmStrCat("# redo-pool: ", poolName, ' ', depth));
      }
    }
    lines.emplace_back(cmStrCat("redo-ifchange ", QuoteForSh(sourceAbs)));
    for (auto const& outAbs : customOutputsAbs) {
      lines.emplace_back(cmStrCat("redo-ifchange ", QuoteForSh(outAbs)));
    }
    std::string const cmakeCommand = cmSystemTools::GetCMakeCommand();
    if (!cmakeCommand.empty()) {
      lines.emplace_back(cmStrCat("cmake_cmd=", QuoteForSh(cmakeCommand)));
    } else {
      // Fall back to PATH lookup.
      lines.emplace_back("cmake_cmd=cmake");
    }
    lines.emplace_back("dep=\"$1.d\"");
    lines.emplace_back("deps=\"$dep.deps\"");
    lines.emplace_back("scan_deps() {");
    lines.emplace_back("  if [ -f \"$dep\" ]; then");
    lines.emplace_back(
      "    \"$cmake_cmd\" -E cmake_depfile_deps \"$(pwd)\" \"$dep\" \"$deps\"");
    lines.emplace_back("    redo-ifchange --from-file \"$deps\"");
    lines.emplace_back("  fi");
    lines.emplace_back("}");
    lines.emplace_back("scan_deps");
    lines.emplace_back(compileCmd);
    lines.emplace_back("mv -f \"$3.d\" \"$dep\"");
    lines.emplace_back("scan_deps");
    WriteDoFile(cmStrCat(topBinary, '/', objectRel, ".do"), lines);
  }

  std::sort(objectRelPaths.begin(), objectRelPaths.end());
  objectRelPaths.erase(
    std::unique(objectRelPaths.begin(), objectRelPaths.end()),
    objectRelPaths.end());

  // Link step: emit <output>.do that depends on objects and runs the link rule.
  {
    std::vector<std::string> lines;
    lines.emplace_back("# Generated by CMake Redo generator.");
    if (cmValue p = this->GeneratorTarget->GetProperty("JOB_POOL_LINK")) {
      std::string const& poolName = *p;
      unsigned int const depth = poolDepthFor(poolName);
      if (depth > 0) {
        lines.emplace_back(cmStrCat("# redo-pool: ", poolName, ' ', depth));
      }
    }
    // Keep a stable reference to redo's temp output path even if we `cd`.
    lines.emplace_back("do_dir=\"$(pwd)\"");
    lines.emplace_back("out=\"$do_dir/$3\"");

    std::vector<std::string> objectAbsPaths;
    objectAbsPaths.reserve(objectRelPaths.size());
    for (auto const& obj : objectRelPaths) {
      std::string const objAbs = cmSystemTools::FileIsFullPath(obj)
        ? obj
        : cmSystemTools::CollapseFullPath(obj, topBinary);
      objectAbsPaths.emplace_back(objAbs);
    }
    EmitRedoIfchange(lines, objectAbsPaths);

    // Best-effort: ensure linked target artifacts are built first.
    std::vector<std::string> targetDeps;
    if (cmComputeLinkInformation* cli =
          this->GeneratorTarget->GetLinkInformation(this->Config)) {
      for (auto const& item : cli->GetItems()) {
        if (item.Target && !item.Target->IsImported()) {
          // Only depend on targets that produce a build artifact.
          // Interface libraries and other non-linkable targets have no output.
          switch (item.Target->GetType()) {
            case cmStateEnums::EXECUTABLE:
            case cmStateEnums::SHARED_LIBRARY:
            case cmStateEnums::STATIC_LIBRARY:
            case cmStateEnums::MODULE_LIBRARY: {
              std::string const depAbs = item.Target->GetFullPath(
                this->Config, cmStateEnums::RuntimeBinaryArtifact, true);
              if (!depAbs.empty()) {
                targetDeps.emplace_back(depAbs);
              }
            } break;
            default:
              break;
          }
        }
      }
    }
    std::sort(targetDeps.begin(), targetDeps.end());
    targetDeps.erase(std::unique(targetDeps.begin(), targetDeps.end()),
                     targetDeps.end());
    std::vector<std::string> targetDepsFiltered;
    for (auto const& dep : targetDeps) {
      // Don't depend on ourselves.
      if (!dep.empty() && dep != outputAbs) {
        targetDepsFiltered.emplace_back(dep);
      }
    }
    EmitRedoIfchange(lines, targetDepsFiltered);

    // Compute link variables using CMake's link line computation.
    std::unique_ptr<cmLinkLineComputer> linkLineComputer =
      this->LocalGenerator->GetGlobalGenerator()->CreateLinkLineComputer(
        this->LocalGenerator,
        this->LocalGenerator->GetStateSnapshot().GetDirectory());

    std::string linkLibs;
    std::string flags;
    std::string linkFlags;
    std::string frameworkPath;
    std::string linkPath;
    this->LocalGenerator->GetTargetFlags(linkLineComputer.get(), this->Config,
                                         linkLibs, flags, linkFlags,
                                         frameworkPath, linkPath,
                                         this->GeneratorTarget);

    auto appendSep = [](std::string& out, std::string const& piece) {
      if (piece.empty()) {
        return;
      }
      if (!out.empty() && out.back() != ' ') {
        out.push_back(' ');
      }
      out.append(piece);
    };

    std::string linkLibraries;
    appendSep(linkLibraries, frameworkPath);
    appendSep(linkLibraries, linkPath);
    appendSep(linkLibraries, linkLibs);

    std::string objects;
    for (auto const& obj : objectAbsPaths) {
      if (!objects.empty()) {
        objects.push_back(' ');
      }
      objects.append(QuoteForSh(obj));
    }

    // Pick a link rule template.
    cmList linkCmds;
    std::string const linkLanguage =
      this->GeneratorTarget->GetLinkerLanguage(this->Config);
    std::string const linkCmdVar =
      this->GeneratorTarget->GetCreateRuleVariable(linkLanguage, this->Config);
    if (cmValue linkCmd = this->Makefile->GetDefinition(linkCmdVar)) {
      linkCmds.assign(*linkCmd);
    } else if (this->GeneratorTarget->GetType() ==
               cmStateEnums::STATIC_LIBRARY) {
      linkCmds.assign(this->Makefile->GetRequiredDefinition(
        cmStrCat("CMAKE_", linkLanguage, "_ARCHIVE_CREATE")));
      linkCmds.append(this->Makefile->GetRequiredDefinition(
        cmStrCat("CMAKE_", linkLanguage, "_ARCHIVE_FINISH")));
    }

    cmRulePlaceholderExpander::RuleVariables vars;
    vars.CMTargetName = targetName.c_str();
    vars.CMTargetType = targetTypeName.c_str();
    vars.Language = linkLanguage.c_str();

    // Some platform link rule templates use <LANGUAGE_COMPILE_FLAGS>.
    // Ensure it expands (otherwise the placeholder name leaks into the
    // command line and breaks the build).
    std::string langCompileFlags;
    this->LocalGenerator->AddLanguageFlagsForLinking(
      langCompileFlags, this->GeneratorTarget, linkLanguage, this->Config);
    vars.LanguageCompileFlags = langCompileFlags.c_str();

    vars.Target = "\"$out\"";
    vars.Objects = objects.c_str();
    vars.LinkLibraries = linkLibraries.c_str();
    vars.Flags = flags.c_str();
    vars.LinkFlags = linkFlags.c_str();
    vars.TargetSupportDir = supportDir.c_str();
    vars.ObjectDir = supportDir.c_str();
    vars.Config = this->Config.c_str();

    std::string linker = this->GeneratorTarget->GetLinkerTool(linkLanguage, this->Config);
    vars.Linker = linker.c_str();

    auto rulePlaceholderExpander =
      this->LocalGenerator->CreateRulePlaceholderExpander(cmBuildStep::Link);
    // Many CMake link rule templates use relative paths (e.g. to sibling libs)
    // that are intended to be evaluated from the target's binary directory.
    lines.emplace_back(
      cmStrCat("cd ", QuoteForSh(this->Makefile->GetCurrentBinaryDirectory())));
    for (auto& cmd : linkCmds) {
      std::string expanded = cmd;
      rulePlaceholderExpander->ExpandRuleVariables(this->LocalGenerator,
                                                   expanded, vars);
      if (!expanded.empty() && expanded.front() != ':') {
        lines.emplace_back(std::move(expanded));
      }
    }

    WriteDoFile(cmStrCat(topBinary, '/', outputRel, ".do"), lines);
  }
}

void cmRedoTargetGenerator::AddIncludeFlags(std::string& flags,
                                           std::string const& language,
                                           std::string const& config)
{
  std::vector<std::string> includes;
  this->LocalGenerator->GetIncludeDirectories(includes, this->GeneratorTarget,
                                              language, config);
  std::string includeFlags = this->LocalGenerator->GetIncludeFlags(
    includes, this->GeneratorTarget, language, config, false);
  this->LocalGenerator->AppendFlags(flags, includeFlags);
}

std::string cmRedoTargetGenerator::GetClangTidyReplacementsFilePath(
  std::string const& directory, cmSourceFile const& source,
  std::string const& /*config*/) const
{
  std::string const& objectName = this->GeneratorTarget->GetObjectName(&source);
  std::string objectDir = this->GeneratorTarget->GetSupportDirectory();
  std::string path = cmStrCat(directory, '/', objectDir, '/', objectName, ".yaml");
  return path;
}

