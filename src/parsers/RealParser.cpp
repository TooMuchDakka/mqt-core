#include "Definitions.hpp"
#include "QuantumComputation.hpp"
#include "operations/Control.hpp"
#include "operations/OpType.hpp"
#include "operations/StandardOperation.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <ios>
#include <iostream>
#include <istream>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

std::optional<qc::Qubit> getQubitForVariableIdentFromAnyLookup(
    const std::string& variableIdent, const qc::QuantumRegisterMap& dataQubits,
    const qc::QuantumRegisterMap& ancillaryQubits) {
  if (const auto& matchingEntryInDataQubits = dataQubits.find(variableIdent);
      matchingEntryInDataQubits != dataQubits.end())
    return matchingEntryInDataQubits->second.first;

  if (const auto& matchingEntryInAncillaryQubits =
          ancillaryQubits.find(variableIdent);
      matchingEntryInAncillaryQubits != ancillaryQubits.end())
    return matchingEntryInAncillaryQubits->second.first;

  return std::nullopt;
}

/// Determine whether the given io name value that is not enclosed in quotes
/// consists of only letters, digits and underscore characters.
/// @param ioName The name to valid
/// @return Whether the given io name is valid
bool isValidIoName(const std::string_view& ioName) {
  return !ioName.empty() &&
         std::all_of(
             ioName.cbegin(), ioName.cend(), [](const char ioNameCharacter) {
               return std::isalnum(ioNameCharacter) || ioNameCharacter == '_';
             });
}

std::unordered_map<std::string, qc::Qubit>
parseIoNames(std::size_t lineInRealFileDefiningIoNames,
             std::size_t expectedNumberOfIos,
             const std::string& ioNameIdentsRawValues) {
  std::unordered_map<std::string, qc::Qubit> foundIoNames;
  std::size_t ioNameStartIdx = 0;
  std::size_t ioNameEndIdx;
  std::size_t ioIdx = 0;

  while (ioNameStartIdx < ioNameIdentsRawValues.size() &&
         foundIoNames.size() <= expectedNumberOfIos) {
    bool searchingForWhitespaceCharacter =
        ioNameIdentsRawValues.at(ioNameStartIdx) != '"';
    if (searchingForWhitespaceCharacter)
      ioNameEndIdx = ioNameIdentsRawValues.find_first_of(' ', ioNameStartIdx);
    else
      ioNameEndIdx = ioNameIdentsRawValues.find_first_of('"', ioNameStartIdx);

    if (ioNameEndIdx == std::string::npos) {
      ioNameEndIdx = ioNameIdentsRawValues.size();
      if (!searchingForWhitespaceCharacter) {
        throw qc::QFRException(
            "[real parser] l: " +
            std::to_string(lineInRealFileDefiningIoNames) +
            " no matching closing quote found for name of io: " +
            std::to_string(ioIdx));
      }
    } else {
      ioNameEndIdx += !searchingForWhitespaceCharacter;
    }

    std::size_t ioNameLength = ioNameEndIdx - ioNameStartIdx;
    // On windows the line ending could be the character sequence \r\n while on
    // linux system it would only be \n
    if (ioNameLength &&
        ioNameIdentsRawValues.at(
            std::min(ioNameEndIdx, ioNameIdentsRawValues.size() - 1)) == '\r')
      --ioNameLength;

    const std::string ioName =
        ioNameIdentsRawValues.substr(ioNameStartIdx, ioNameLength);

    if (!isValidIoName(ioName)) {
      throw qc::QFRException(
          "[real parser] l: " + std::to_string(lineInRealFileDefiningIoNames) +
          " invalid io name: " + ioName);
    }

    ioNameStartIdx = ioNameEndIdx + searchingForWhitespaceCharacter;
    /*
     * We offer the user the use of some special literals to denote either
     * constant inputs or garbage outputs instead of finding unique names for
     * said ios, otherwise check that the given io name is unique.
     */
    if (!(ioName == "0" || ioName == "1" || ioName == "g")) {
      if (const auto& ioNameInsertionIntoLookupResult =
              foundIoNames.emplace(ioName, static_cast<qc::Qubit>(ioIdx++));
          !ioNameInsertionIntoLookupResult.second) {
        throw qc::QFRException("[real parser] l: " +
                               std::to_string(lineInRealFileDefiningIoNames) +
                               "duplicate io name: " + ioName);
      }
    }
  }
  return foundIoNames;
}

void qc::QuantumComputation::importReal(std::istream& is) {
  auto line = readRealHeader(is);
  readRealGateDescriptions(is, line);
}

int qc::QuantumComputation::readRealHeader(std::istream& is) {
  std::string cmd;
  std::string variable;
  int line = 0;

  /*
   * We could reuse the QuantumRegisterMap type defined in the qc namespace but
   * to avoid potential errors due to any future refactoring of said type, we
   * use an std::unordered_map instead
   */
  std::unordered_map<std::string, qc::Qubit> userDefinedInputIdents;

  while (true) {
    if (!static_cast<bool>(is >> cmd)) {
      throw QFRException("[real parser] l:" + std::to_string(line) +
                         " msg: Invalid file header");
    }
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char ch) {
      return static_cast<char>(toupper(ch));
    });
    ++line;

    // skip comments
    if (cmd.front() == '#') {
      is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      continue;
    }

    // valid header commands start with '.'
    if (cmd.front() != '.') {
      throw QFRException("[real parser] l:" + std::to_string(line) +
                         " msg: Invalid file header");
    }

    /*
     * TODO: Parsing currently assumes that expected entries are defined only
     * once and in the expected definition order as defined in the .real header
     * specification. (https://www.revlib.org/doc/docu/revlib_2_0_1.pdf
     * Chapter 3.2)
     */
    if (cmd == ".BEGIN") {
      // header read complete
      return line;
    }

    if (cmd == ".NUMVARS") {
      std::size_t nq{};
      if (!static_cast<bool>(is >> nq)) {
        nqubits = 0;
      } else {
        nqubits = nq;
      }
      nclassics = nqubits;
    } else if (cmd == ".VARIABLES") {
      for (std::size_t i = 0; i < nclassics; ++i) {
        if (!static_cast<bool>(is >> variable) || variable.at(0) == '.') {
          throw QFRException(
              "[real parser] l:" + std::to_string(line) +
              " msg: Invalid or insufficient variables declared");
        }
        const auto qubit = static_cast<Qubit>(i);
        qregs.insert({variable, {qubit, 1U}});
        cregs.insert({"c_" + variable, {qubit, 1U}});
        initialLayout.insert({qubit, qubit});
        outputPermutation.insert({qubit, qubit});
        ancillary.resize(nqubits);
        garbage.resize(nqubits);
      }

      // TODO: Check whether more than the declared number of variables was
      // defined
    } else if (cmd == ".CONSTANTS") {
      is >> std::ws;
      for (std::size_t i = 0; i < nclassics; ++i) {
        char readConstantFlagValue;
        if (!is.get(readConstantFlagValue)) {
          throw QFRException("[real parser] l:" + std::to_string(line) +
                             " msg: Failed read in '.constants' line");
        }

        if (const bool isCurrentQubitMarkedAsAncillary =
                readConstantFlagValue == '0' || readConstantFlagValue == '1';
            isCurrentQubitMarkedAsAncillary) {
          const Qubit ancillaryQubit = static_cast<Qubit>(i);
          setLogicalQubitAncillary(ancillaryQubit);

          /*
           * Since the call to setLogicalQubitAncillary does not actually
           * transfer the qubit from the data qubit lookup into the ancillary
           * lookup we will 'manually' perform this transfer.
           */
          const std::string& associatedVariableNameForQubitRegister =
              getQubitRegister(ancillaryQubit);
          qregs.erase(associatedVariableNameForQubitRegister);
          ancregs.insert_or_assign(
              associatedVariableNameForQubitRegister,
              qc::QuantumRegister(std::make_pair(ancillaryQubit, 1U)));
        } else if (readConstantFlagValue != '-') {
          throw QFRException("[real parser] l:" + std::to_string(line) +
                             " msg: Invalid value in '.constants' header: '" +
                             std::to_string(readConstantFlagValue) + "'");
        }
      }
      is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    } else if (cmd == ".GARBAGE") {
      is >> std::ws;
      for (std::size_t i = 0; i < nclassics; ++i) {
        char readGarbageStatusFlagValue;
        if (!is.get(readGarbageStatusFlagValue)) {
          throw QFRException("[real parser] l:" + std::to_string(line) +
                             " msg: Failed read in '.garbage' line");
        }

        if (const bool isCurrentQubitMarkedAsGarbage =
                readGarbageStatusFlagValue == '1';
            isCurrentQubitMarkedAsGarbage) {
          setLogicalQubitGarbage(static_cast<Qubit>(i));
        } else if (readGarbageStatusFlagValue != '-') {
          throw QFRException("[real parser] l:" + std::to_string(line) +
                             " msg: Invalid value in '.garbage' header: '" +
                             std::to_string(readGarbageStatusFlagValue) + "'");
        }
      }
      is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    } else if (cmd == ".INPUTS") {
      // .INPUT: specifies initial layout
      is >> std::ws;
      const std::size_t expectedNumInputIos = nclassics;
      std::string ioNameIdentsLine;
      std::getline(is, ioNameIdentsLine);

      userDefinedInputIdents =
          parseIoNames(line, expectedNumInputIos, ioNameIdentsLine);

      if (userDefinedInputIdents.size() != expectedNumInputIos) {
        std::cout << userDefinedInputIdents.size() << "\n";
        throw QFRException(
            "[real parser} l: " + std::to_string(line) + "msg: Expected " +
            std::to_string(expectedNumInputIos) + " inputs to be declared!");
      }
    } else if (cmd == ".OUTPUTS") {
      // .OUTPUTS: specifies output permutation
      is >> std::ws;
      const std::size_t expectedNumOutputIos = nclassics;
      std::string ioNameIdentsLine;
      std::getline(is, ioNameIdentsLine);

      const std::unordered_map<std::string, qc::Qubit> userDefinedOutputIdents =
          parseIoNames(line, expectedNumOutputIos, ioNameIdentsLine);

      if (userDefinedOutputIdents.size() != expectedNumOutputIos) {
        std::cout << userDefinedOutputIdents.size() << "\n";
        throw QFRException(
            "[real parser} l: " + std::to_string(line) + "msg: Expected " +
            std::to_string(expectedNumOutputIos) + " outputs to be declared!");
      }

      for (const auto& [outputIoIdent, outputIoQubit] :
           userDefinedOutputIdents) {
        /*
         * We assume that a permutation of a given input qubit Q at index i
         * is performed in the circuit if an entry in both in the .output
         * as well as the .input definition using the same literal is found
         * with the input literal being defined at position i in the .input
         * definition. If no such matching is found, we assume that the output
         * is marked as garbage and thus remove the entry from the output
         * permutation.
         *
         * The outputPermutation map will use be structured as shown in the
         * documentation
         * (https://mqt.readthedocs.io/projects/core/en/latest/quickstart.html#layout-information)
         * with the output qubit being used as the key while the input qubit
         * serves as the map entries value.
         */
        if (!userDefinedInputIdents.count(outputIoIdent)) {
          outputPermutation.erase(outputIoQubit);
        } else if (const qc::Qubit matchingInputQubitForOutputLiteral =
                       userDefinedInputIdents.at(outputIoIdent);
                   matchingInputQubitForOutputLiteral != outputIoQubit) {
          /*
           * Only if the matching entries where defined at different indices
           in their respective IO declaration
           * do we update the existing 1-1 mapping for the given output qubit
           */
          outputPermutation.insert_or_assign(
              outputIoQubit, matchingInputQubitForOutputLiteral);
        }
      }
    } else if (cmd == ".VERSION" || cmd == ".INPUTBUS" || cmd == ".OUTPUTBUS") {
      is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      continue;
    } else if (cmd == ".DEFINE") {
      // TODO: Defines currently not supported
      std::cerr << "[WARN] File contains 'define' statement, which is "
                   "currently not supported and thus simply skipped.\n";
      while (cmd != ".ENDDEFINE") {
        is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        is >> cmd;
        std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                       [](const unsigned char c) {
                         return static_cast<char>(toupper(c));
                       });
      }
    } else {
      throw QFRException("[real parser] l:" + std::to_string(line) +
                         " msg: Unknown command: " + cmd);
    }
  }
}

void qc::QuantumComputation::readRealGateDescriptions(std::istream& is,
                                                      int line) {
  const std::regex gateRegex =
      std::regex("(r[xyz]|i[df]|q|[0a-z](?:[+ip])?)(\\d+)?(?::([-+]?[0-9]+[.]?["
                 "0-9]*(?:[eE][-+]?[0-9]+)?))?");
  std::smatch m;
  std::string cmd;

  static const std::map<std::string, OpType> IDENTIFIER_MAP{
      {"0", I},     {"id", I},    {"h", H},        {"n", X},        {"c", X},
      {"x", X},     {"y", Y},     {"z", Z},        {"s", S},        {"si", Sdg},
      {"sp", Sdg},  {"s+", Sdg},  {"v", V},        {"vi", Vdg},     {"vp", Vdg},
      {"v+", Vdg},  {"rx", RX},   {"ry", RY},      {"rz", RZ},      {"f", SWAP},
      {"if", SWAP}, {"p", Peres}, {"pi", Peresdg}, {"p+", Peresdg}, {"q", P}};

  while (!is.eof()) {
    if (!static_cast<bool>(is >> cmd)) {
      throw QFRException("[real parser] l:" + std::to_string(line) +
                         " msg: Failed to read command");
    }
    std::transform(
        cmd.begin(), cmd.end(), cmd.begin(),
        [](const unsigned char c) { return static_cast<char>(tolower(c)); });
    ++line;

    if (cmd.front() == '#') {
      is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      continue;
    }

    if (cmd == ".end") {
      break;
    }

    // match gate declaration
    if (!std::regex_match(cmd, m, gateRegex)) {
      throw QFRException("[real parser] l:" + std::to_string(line) +
                         " msg: Unsupported gate detected: " + cmd);
    }

    // extract gate information (identifier, #controls, divisor)
    OpType gate{};
    if (m.str(1) == "t") { // special treatment of t(offoli) for real format
      gate = X;
    } else {
      auto it = IDENTIFIER_MAP.find(m.str(1));
      if (it == IDENTIFIER_MAP.end()) {
        throw QFRException("[real parser] l:" + std::to_string(line) +
                           " msg: Unknown gate identifier: " + m.str(1));
      }
      gate = (*it).second;
    }
    auto ncontrols =
        m.str(2).empty() ? 0 : std::stoul(m.str(2), nullptr, 0) - 1;
    const fp lambda = m.str(3).empty() ? static_cast<fp>(0L)
                                       : static_cast<fp>(std::stold(m.str(3)));

    if (gate == V || gate == Vdg || m.str(1) == "c" || gate == SWAP) {
      ncontrols = 1;
    } else if (gate == Peres || gate == Peresdg) {
      ncontrols = 2;
    }

    if (ncontrols >= getNqubits()) {
      throw QFRException("[real parser] l:" + std::to_string(line) +
                         " msg: Gate acts on " + std::to_string(ncontrols + 1) +
                         " qubits, but only " + std::to_string(getNqubits()) +
                         " qubits are available.");
    }

    std::string qubits;
    std::string label;
    getline(is, qubits);

    std::vector<Control> controls{};
    std::istringstream iss(qubits);

    // get controls and target
    for (std::size_t i = 0; i < ncontrols; ++i) {
      if (!(iss >> label)) {
        throw QFRException("[real parser] l:" + std::to_string(line) +
                           " msg: Too few variables for gate " + m.str(1));
      }

      const bool negativeControl = (label.at(0) == '-');
      if (negativeControl) {
        label.erase(label.begin());
      }

      // Since variable qubits can either be data or ancillary qubits our search
      // will have to be conducted in both lookups
      const std::optional<qc::Qubit> controlLineQubit =
          getQubitForVariableIdentFromAnyLookup(label, qregs, ancregs);
      if (!controlLineQubit) {
        throw QFRException("[real parser] l:" + std::to_string(line) +
                           " msg: Label " + label + " not found!");
      }
      controls.emplace_back(*controlLineQubit, negativeControl
                                                   ? Control::Type::Neg
                                                   : Control::Type::Pos);
    }

    if (!(iss >> label)) {
      throw QFRException("[real parser] l:" + std::to_string(line) +
                         " msg: Too few variables (no target) for gate " +
                         m.str(1));
    }

    // Since variable qubits can either be data or ancillary qubits our search
    // will have to be conducted in both lookups
    const std::optional<qc::Qubit> targetLineQubit =
        getQubitForVariableIdentFromAnyLookup(label, qregs, ancregs);
    if (!targetLineQubit) {
      throw QFRException("[real parser] l:" + std::to_string(line) +
                         " msg: Label " + label + " not found!");
    }

    switch (gate) {
    case I:
    case H:
    case Y:
    case Z:
    case S:
    case Sdg:
    case T:
    case Tdg:
    case V:
    case Vdg:
      emplace_back<StandardOperation>(
          Controls{controls.cbegin(), controls.cend()}, *targetLineQubit, gate);
      break;
    case X:
      mcx(Controls{controls.cbegin(), controls.cend()}, *targetLineQubit);
      break;
    case RX:
    case RY:
    case RZ:
    case P:
      emplace_back<StandardOperation>(
          Controls{controls.cbegin(), controls.cend()}, *targetLineQubit, gate,
          std::vector{PI / (lambda)});
      break;
    case SWAP:
    case Peres:
    case Peresdg:
    case iSWAP: {
      const auto target1 = controls.back().qubit;
      controls.pop_back();
      emplace_back<StandardOperation>(
          Controls{controls.cbegin(), controls.cend()}, target1,
          *targetLineQubit, gate);
      break;
    }
    default:
      std::cerr << "Unsupported operation encountered:  " << gate << "!\n";
      break;
    }
  }
}
