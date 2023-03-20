/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include "fboss/agent/state/VlanMap.h"

#include "fboss/agent/state/SwitchState.h"
#include "fboss/agent/state/Vlan.h"

#include "fboss/agent/state/ArpResponseTable.h"
#include "fboss/agent/state/ArpTable.h"
#include "fboss/agent/state/NdpResponseTable.h"
#include "fboss/agent/state/NdpTable.h"
#include "fboss/agent/state/NodeMap-defs.h"

#include <boost/container/flat_map.hpp>
#include <cstdio>

using std::shared_ptr;
using std::string;

namespace facebook::fboss {

VlanMap::VlanMap() {}

VlanMap::~VlanMap() {}

VlanMap* VlanMap::modify(std::shared_ptr<SwitchState>* state) {
  if (!isPublished()) {
    CHECK(!(*state)->isPublished());
    return this;
  }

  SwitchState::modify(state);
  auto newVlans = clone();
  auto* ptr = newVlans.get();
  (*state)->resetVlans(std::move(newVlans));
  return ptr;
}

const shared_ptr<Vlan>& VlanMap::getVlanSlow(const string& name) const {
  for (auto& iter : std::as_const(*this)) {
    const auto& vlan = iter.second;
    if (vlan->getName() == name) {
      return vlan;
    }
  }
  throw FbossError("Cannot find Vlan : ", name);
}

shared_ptr<Vlan> VlanMap::getVlanSlowIf(const string& name) const {
  for (auto iter : std::as_const(*this)) {
    const auto& vlan = iter.second;
    if (vlan->getName() == name) {
      return vlan;
    }
  }
  return nullptr;
}

void VlanMap::addVlan(const std::shared_ptr<Vlan>& vlan) {
  addNode(vlan);
}

void VlanMap::updateVlan(const std::shared_ptr<Vlan>& vlan) {
  updateNode(vlan);
}

VlanID VlanMap::getFirstVlanID() const {
  return cbegin()->second->getID();
}

template class ThriftMapNode<VlanMap, VlanMapTraits>;

} // namespace facebook::fboss
