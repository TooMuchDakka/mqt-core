/*
 * Copyright (c) 2025 Chair for Design Automation, TUM
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "Definitions.hpp"
#include "dd/DDDefinitions.hpp"
#include "dd/Edge.hpp"
#include "dd/GateMatrixDefinitions.hpp"
#include "dd/Package.hpp"
#include "ir/Permutation.hpp"
#include "ir/operations/ClassicControlledOperation.hpp"
#include "ir/operations/CompoundOperation.hpp"
#include "ir/operations/Control.hpp"
#include "ir/operations/NonUnitaryOperation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "ir/operations/StandardOperation.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace dd {
// single-target Operations
template <class Config>
qc::MatrixDD
getStandardOperationDD(const qc::StandardOperation& op, Package<Config>& dd,
                       const qc::Controls& controls, const qc::Qubit target,
                       const bool inverse) {
  GateMatrix gm;

  const auto type = op.getType();
  const auto& parameter = op.getParameter();

  switch (type) {
  case qc::I:
    gm = I_MAT;
    break;
  case qc::H:
    gm = H_MAT;
    break;
  case qc::X:
    gm = X_MAT;
    break;
  case qc::Y:
    gm = Y_MAT;
    break;
  case qc::Z:
    gm = Z_MAT;
    break;
  case qc::S:
    gm = inverse ? SDG_MAT : S_MAT;
    break;
  case qc::Sdg:
    gm = inverse ? S_MAT : SDG_MAT;
    break;
  case qc::T:
    gm = inverse ? TDG_MAT : T_MAT;
    break;
  case qc::Tdg:
    gm = inverse ? T_MAT : TDG_MAT;
    break;
  case qc::V:
    gm = inverse ? VDG_MAT : V_MAT;
    break;
  case qc::Vdg:
    gm = inverse ? V_MAT : VDG_MAT;
    break;
  case qc::U:
    gm = inverse ? uMat(-parameter[1U], -parameter[2U], -parameter[0U])
                 : uMat(parameter[2U], parameter[1U], parameter[0U]);
    break;
  case qc::U2:
    gm = inverse ? u2Mat(-parameter[0U] + PI, -parameter[1U] - PI)
                 : u2Mat(parameter[1U], parameter[0U]);
    break;
  case qc::P:
    gm = inverse ? pMat(-parameter[0U]) : pMat(parameter[0U]);
    break;
  case qc::SX:
    gm = inverse ? SXDG_MAT : SX_MAT;
    break;
  case qc::SXdg:
    gm = inverse ? SX_MAT : SXDG_MAT;
    break;
  case qc::RX:
    gm = inverse ? rxMat(-parameter[0U]) : rxMat(parameter[0U]);
    break;
  case qc::RY:
    gm = inverse ? ryMat(-parameter[0U]) : ryMat(parameter[0U]);
    break;
  case qc::RZ:
    gm = inverse ? rzMat(-parameter[0U]) : rzMat(parameter[0U]);
    break;
  default:
    std::ostringstream oss{};
    oss << "DD for gate" << op.getName() << " not available!";
    throw qc::QFRException(oss.str());
  }
  return dd.makeGateDD(gm, controls, target);
}

// two-target Operations
template <class Config>
qc::MatrixDD
getStandardOperationDD(const qc::StandardOperation& op, Package<Config>& dd,
                       const qc::Controls& controls, qc::Qubit target0,
                       qc::Qubit target1, const bool inverse) {
  const auto type = op.getType();
  const auto& parameter = op.getParameter();

  if (type == qc::DCX && inverse) {
    // DCX is not self-inverse, but the inverse is just swapping the targets
    std::swap(target0, target1);
  }

  TwoQubitGateMatrix gm;
  switch (type) {
  case qc::SWAP:
    gm = SWAP_MAT;
    break;
  case qc::iSWAP:
    gm = inverse ? ISWAPDG_MAT : ISWAP_MAT;
    break;
  case qc::iSWAPdg:
    gm = inverse ? ISWAP_MAT : ISWAPDG_MAT;
    break;
  case qc::Peres:
    gm = inverse ? PERESDG_MAT : PERES_MAT;
    break;
  case qc::Peresdg:
    gm = inverse ? PERES_MAT : PERESDG_MAT;
    break;
  case qc::DCX:
    gm = DCX_MAT;
    break;
  case qc::ECR:
    gm = ECR_MAT;
    break;
  case qc::RXX:
    gm = inverse ? rxxMat(-parameter[0U]) : rxxMat(parameter[0U]);
    break;
  case qc::RYY:
    gm = inverse ? ryyMat(-parameter[0U]) : ryyMat(parameter[0U]);
    break;
  case qc::RZZ:
    gm = inverse ? rzzMat(-parameter[0U]) : rzzMat(parameter[0U]);
    break;
  case qc::RZX:
    gm = inverse ? rzxMat(-parameter[0U]) : rzxMat(parameter[0U]);
    break;
  case qc::XXminusYY:
    gm = inverse ? xxMinusYYMat(-parameter[0U], parameter[1U])
                 : xxMinusYYMat(parameter[0U], parameter[1U]);
    break;
  case qc::XXplusYY:
    gm = inverse ? xxPlusYYMat(-parameter[0U], parameter[1U])
                 : xxPlusYYMat(parameter[0U], parameter[1U]);
    break;
  default:
    std::ostringstream oss{};
    oss << "DD for gate " << op.getName() << " not available!";
    throw qc::QFRException(oss.str());
  }

  return dd.makeTwoQubitGateDD(gm, controls, target0, target1);
}

// The methods with a permutation parameter apply these Operations according to
// the mapping specified by the permutation, e.g.
//      if perm[0] = 1 and perm[1] = 0
//      then cx 0 1 will be translated to cx perm[0] perm[1] == cx 1 0
// An empty permutation marks the identity permutation.

template <class Config>
qc::MatrixDD getDD(const qc::Operation& op, Package<Config>& dd,
                   const qc::Permutation& permutation = {},
                   const bool inverse = false) {
  const auto type = op.getType();

  if (type == qc::Barrier) {
    return dd.makeIdent();
  }

  if (type == qc::GPhase) {
    auto phase = op.getParameter()[0U];
    if (inverse) {
      phase = -phase;
    }
    auto id = dd.makeIdent();
    id.w = dd.cn.lookup(std::cos(phase), std::sin(phase));
    return id;
  }

  if (op.isStandardOperation()) {
    const auto& standardOp = dynamic_cast<const qc::StandardOperation&>(op);
    const auto& targets = permutation.apply(standardOp.getTargets());
    const auto& controls = permutation.apply(standardOp.getControls());

    if (qc::isTwoQubitGate(type)) {
      assert(targets.size() == 2);
      return getStandardOperationDD(standardOp, dd, controls, targets[0U],
                                    targets[1U], inverse);
    }
    assert(targets.size() == 1);
    return getStandardOperationDD(standardOp, dd, controls, targets[0U],
                                  inverse);
  }

  if (op.isCompoundOperation()) {
    const auto& compoundOp = dynamic_cast<const qc::CompoundOperation&>(op);
    auto e = dd.makeIdent();
    if (inverse) {
      for (const auto& operation : compoundOp) {
        e = dd.multiply(e, getInverseDD(*operation, dd, permutation));
      }
    } else {
      for (const auto& operation : compoundOp) {
        e = dd.multiply(getDD(*operation, dd, permutation), e);
      }
    }
    return e;
  }

  if (op.isClassicControlledOperation()) {
    const auto& classicOp =
        dynamic_cast<const qc::ClassicControlledOperation&>(op);
    return getDD(*classicOp.getOperation(), dd, permutation, inverse);
  }

  assert(op.isNonUnitaryOperation());
  throw qc::QFRException("DD for non-unitary operation not available!");
}

template <class Config>
qc::MatrixDD getInverseDD(const qc::Operation& op, Package<Config>& dd,
                          const qc::Permutation& permutation = {}) {
  return getDD(op, dd, permutation, true);
}

template <class Config, class Node>
Edge<Node> applyUnitaryOperation(const qc::Operation& op, const Edge<Node>& in,
                                 Package<Config>& dd,
                                 const qc::Permutation& permutation = {}) {
  static_assert(std::is_same_v<Node, dd::vNode> ||
                std::is_same_v<Node, dd::mNode>);
  return dd.applyOperation(getDD(op, dd, permutation), in);
}

template <class Config>
qc::VectorDD applyMeasurement(const qc::NonUnitaryOperation& op,
                              qc::VectorDD in, Package<Config>& dd,
                              std::mt19937_64& rng,
                              std::vector<bool>& measurements,
                              const qc::Permutation& permutation = {}) {
  assert(op.getType() == qc::Measure);
  const auto& qubits = permutation.apply(op.getTargets());
  const auto& bits = op.getClassics();
  for (size_t j = 0U; j < qubits.size(); ++j) {
    measurements.at(bits.at(j)) =
        dd.measureOneCollapsing(in, static_cast<dd::Qubit>(qubits.at(j)),
                                rng) == '1';
  }
  return in;
}

template <class Config>
qc::VectorDD applyReset(const qc::NonUnitaryOperation& op, qc::VectorDD in,
                        Package<Config>& dd, std::mt19937_64& rng,
                        const qc::Permutation& permutation = {}) {
  assert(op.getType() == qc::Reset);
  const auto& qubits = permutation.apply(op.getTargets());
  for (const auto& qubit : qubits) {
    const auto bit =
        dd.measureOneCollapsing(in, static_cast<dd::Qubit>(qubit), rng);
    // apply an X operation whenever the measured result is one
    if (bit == '1') {
      const auto x = qc::StandardOperation(qubit, qc::X);
      in = applyUnitaryOperation(x, in, dd);
    }
  }
  return in;
}

template <class Config>
qc::VectorDD
applyClassicControlledOperation(const qc::ClassicControlledOperation& op,
                                const qc::VectorDD& in, Package<Config>& dd,
                                std::vector<bool>& measurements,
                                const qc::Permutation& permutation = {}) {
  const auto& expectedValue = op.getExpectedValue();
  const auto& comparisonKind = op.getComparisonKind();

  // determine the actual value from measurements
  auto actualValue = 0ULL;
  if (const auto& controlRegister = op.getControlRegister();
      controlRegister.has_value()) {
    assert(!op.getControlBit().has_value());
    const auto regStart = controlRegister->getStartIndex();
    const auto regSize = controlRegister->getSize();
    for (std::size_t j = 0; j < regSize; ++j) {
      if (measurements[regStart + j]) {
        actualValue |= 1ULL << j;
      }
    }
  }
  if (const auto& controlBit = op.getControlBit(); controlBit.has_value()) {
    assert(!op.getControlRegister().has_value());
    actualValue = measurements[*controlBit] ? 1U : 0U;
  }

  // check if the actual value matches the expected value according to the
  // comparison kind
  const auto control = [actualValue, expectedValue, comparisonKind]() {
    switch (comparisonKind) {
    case qc::ComparisonKind::Eq:
      return actualValue == expectedValue;
    case qc::ComparisonKind::Neq:
      return actualValue != expectedValue;
    case qc::ComparisonKind::Lt:
      return actualValue < expectedValue;
    case qc::ComparisonKind::Leq:
      return actualValue <= expectedValue;
    case qc::ComparisonKind::Gt:
      return actualValue > expectedValue;
    case qc::ComparisonKind::Geq:
      return actualValue >= expectedValue;
    }
    qc::unreachable();
  }();

  if (!control) {
    return in;
  }

  return applyUnitaryOperation(op, in, dd, permutation);
}

// apply swaps 'on' DD in order to change 'from' to 'to'
// where |from| >= |to|
template <class DDType, class Config>
void changePermutation(DDType& on, qc::Permutation& from,
                       const qc::Permutation& to, Package<Config>& dd,
                       const bool regular = true) {
  assert(from.size() >= to.size());
  if (on.isZeroTerminal()) {
    return;
  }

  // iterate over (k,v) pairs of second permutation
  for (const auto& [i, goal] : to) {
    // search for key in the first map
    auto it = from.find(i);
    if (it == from.end()) {
      throw qc::QFRException(
          "[changePermutation] Key " + std::to_string(it->first) +
          " was not found in first permutation. This should never happen.");
    }
    auto current = it->second;

    // permutations agree for this key value
    if (current == goal) {
      continue;
    }

    // search for goal value in first permutation
    qc::Qubit j = 0;
    for (const auto& [key, value] : from) {
      if (value == goal) {
        j = key;
        break;
      }
    }

    // swap i and j
    auto saved = on;
    const auto swapDD = dd.makeTwoQubitGateDD(SWAP_MAT, from.at(i), from.at(j));
    if constexpr (std::is_same_v<DDType, qc::VectorDD>) {
      on = dd.multiply(swapDD, on);
    } else {
      // the regular flag only has an effect on matrix DDs
      if (regular) {
        on = dd.multiply(swapDD, on);
      } else {
        on = dd.multiply(on, swapDD);
      }
    }

    dd.incRef(on);
    dd.decRef(saved);
    dd.garbageCollect();

    // update permutation
    from.at(i) = goal;
    from.at(j) = current;
  }
}

} // namespace dd
