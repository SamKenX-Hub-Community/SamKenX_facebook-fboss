/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include "fboss/agent/state/UdfGroup.h"
#include "fboss/agent/gen-cpp2/switch_config_fatal.h"
#include "fboss/agent/gen-cpp2/switch_config_fatal_types.h"

namespace facebook::fboss {

UdfGroup::UdfGroup(const std::string& name)
    : ThriftStructNode<UdfGroup, cfg::UdfGroup>() {
  set<switch_config_tags::name>(name);
}

std::string UdfGroup::getName() const {
  return get<switch_config_tags::name>()->cref();
}

std::shared_ptr<UdfGroup> UdfGroup::fromFollyDynamic(
    const folly::dynamic& entry) {
  auto node = std::make_shared<UdfGroup>();
  static_cast<std::shared_ptr<BaseT>>(node)->fromFollyDynamic(entry);
  return node;
}

// THRIFT_COPY: avoid returning std::vector
std::vector<std::string> UdfGroup::getUdfPacketMatcherIds() const {
  return get<switch_config_tags::udfPacketMatcherIds>()->toThrift();
}

cfg::UdfBaseHeaderType UdfGroup::getUdfBaseHeader() const {
  return get<switch_config_tags::header>()->cref();
}

int UdfGroup::getStartOffsetInBytes() const {
  return get<switch_config_tags::startOffsetInBytes>()->cref();
}

int UdfGroup::getFieldSizeInBytes() const {
  return get<switch_config_tags::fieldSizeInBytes>()->cref();
}

template class ThriftStructNode<UdfGroup, cfg::UdfGroup>;
} // namespace facebook::fboss
