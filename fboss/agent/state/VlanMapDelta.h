/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#pragma once

#include "fboss/agent/state/ArpTable.h"
#include "fboss/agent/state/NdpTable.h"
#include "fboss/agent/state/NodeMapDelta.h"
#include "fboss/agent/state/Vlan.h"
#include "fboss/agent/state/VlanMap.h"

namespace facebook::fboss {

class VlanDelta;
namespace thrift_cow {
template <>
struct ThriftMapNodeDeltaTraits<VlanMap> {
  using mapped_type = typename VlanMap::mapped_type;
  using ExtractorT = ThriftMapNodeExtractor<VlanMap>;
  using DeltaValueT = VlanDelta;
};
} // namespace thrift_cow
/*
 * VlanMapDelta is a small wrapper on top of NodeMapDelta<VlanMap>.
 *
 * The main difference is that it's iterator also provides a getArpDelta()
 * helper function.
 */
class VlanDelta : public DeltaValue<Vlan> {
 public:
  using ArpTableDelta = thrift_cow::ThriftMapDelta<ArpTable>;
  using NdpTableDelta = thrift_cow::ThriftMapDelta<NdpTable>;
  using MacTableDelta = thrift_cow::ThriftMapDelta<MacTable>;

  using DeltaValue<Vlan>::DeltaValue;

  ArpTableDelta getArpDelta() const {
    return ArpTableDelta(
        getOld() ? getOld()->getArpTable().get() : nullptr,
        getNew() ? getNew()->getArpTable().get() : nullptr);
  }
  NdpTableDelta getNdpDelta() const {
    return NdpTableDelta(
        getOld() ? getOld()->getNdpTable().get() : nullptr,
        getNew() ? getNew()->getNdpTable().get() : nullptr);
  }
  template <typename NTableT>
  thrift_cow::ThriftMapDelta<NTableT> getNeighborDelta() const;

  MacTableDelta getMacDelta() const {
    return MacTableDelta(
        getOld() ? getOld()->getMacTable().get() : nullptr,
        getNew() ? getNew()->getMacTable().get() : nullptr);
  }
};

using VlanMapDelta = thrift_cow::ThriftMapDelta<VlanMap>;

template <>
inline thrift_cow::ThriftMapDelta<ArpTable> VlanDelta::getNeighborDelta()
    const {
  return getArpDelta();
}

template <>
inline thrift_cow::ThriftMapDelta<NdpTable> VlanDelta::getNeighborDelta()
    const {
  return getNdpDelta();
}

} // namespace facebook::fboss
