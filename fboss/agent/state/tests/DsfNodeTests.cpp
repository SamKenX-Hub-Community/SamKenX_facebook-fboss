/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/agent/state/DsfNodeMap.h"
#include "fboss/agent/state/SwitchState.h"
#include "fboss/agent/test/TestUtils.h"

#include <gtest/gtest.h>

using namespace facebook::fboss;

std::shared_ptr<DsfNode> makeDsfNode(
    int64_t switchId = 0,
    cfg::DsfNodeType type = cfg::DsfNodeType::INTERFACE_NODE) {
  auto dsfNode = std::make_shared<DsfNode>(SwitchID(switchId));
  dsfNode->fromThrift(makeDsfNodeCfg(switchId, type));
  return dsfNode;
}

TEST(DsfNode, SerDeserDsfNode) {
  auto dsfNode = makeDsfNode();
  auto serialized = dsfNode->toFollyDynamic();
  auto dsfNodeBack = DsfNode::fromFollyDynamic(serialized);
  EXPECT_TRUE(*dsfNode == *dsfNodeBack);
}

TEST(DsfNode, SerDeserSwitchState) {
  auto state = std::make_shared<SwitchState>();

  auto dsfNode1 = makeDsfNode(1);
  auto dsfNode2 = makeDsfNode(2);

  auto dsfNodeMap = std::make_shared<DsfNodeMap>();
  dsfNodeMap->addNode(dsfNode1);
  dsfNodeMap->addDsfNode(dsfNode2);
  state->resetDsfNodes(dsfNodeMap);

  auto serialized = state->toThrift();
  auto stateBack = SwitchState::fromThrift(serialized);

  // Check all dsfNodes should be there
  for (auto switchID : {SwitchID(1), SwitchID(2)}) {
    EXPECT_TRUE(
        *state->getDsfNodes()->getDsfNodeIf(switchID) ==
        *stateBack->getDsfNodes()->getDsfNodeIf(switchID));
  }
  EXPECT_EQ(state->getDsfNodes()->size(), 2);
}

TEST(DsfNode, addRemove) {
  auto state = std::make_shared<SwitchState>();
  auto dsfNode1 = makeDsfNode(1);
  auto dsfNode2 = makeDsfNode(2);
  state->getDsfNodes()->addNode(dsfNode1);
  state->getDsfNodes()->addNode(dsfNode2);
  EXPECT_EQ(state->getDsfNodes()->size(), 2);

  state->getDsfNodes()->removeNode(1);
  EXPECT_EQ(state->getDsfNodes()->size(), 1);
  EXPECT_EQ(state->getDsfNodes()->getNodeIf(1), nullptr);
  EXPECT_NE(state->getDsfNodes()->getNodeIf(2), nullptr);
}

TEST(DsfNode, update) {
  auto state = std::make_shared<SwitchState>();
  auto dsfNode = makeDsfNode(1);
  state->getDsfNodes()->addNode(dsfNode);
  EXPECT_EQ(state->getDsfNodes()->size(), 1);
  auto newDsfNode = makeDsfNode(1, cfg::DsfNodeType::FABRIC_NODE);
  state->getDsfNodes()->updateNode(newDsfNode);

  EXPECT_NE(*dsfNode, *state->getDsfNodes()->getNode(1));
}

TEST(DsfNode, publish) {
  auto state = std::make_shared<SwitchState>();
  state->publish();
  EXPECT_TRUE(state->getDsfNodes()->isPublished());
}

TEST(DsfNode, dsfNodeApplyConfig) {
  auto platform = createMockPlatform(cfg::SwitchType::VOQ, 0);
  auto stateV0 = std::make_shared<SwitchState>();
  auto config = testConfigA(cfg::SwitchType::VOQ);
  auto stateV1 = publishAndApplyConfig(stateV0, &config, platform.get());
  ASSERT_NE(nullptr, stateV1);
  EXPECT_EQ(stateV1->getDsfNodes()->size(), 2);
  // Add node
  config.dsfNodes()->insert({5, makeDsfNodeCfg(5)});
  auto stateV2 = publishAndApplyConfig(stateV1, &config, platform.get());

  ASSERT_NE(nullptr, stateV2);
  EXPECT_EQ(stateV2->getDsfNodes()->size(), 3);
  EXPECT_EQ(
      stateV2->getRemoteSystemPorts()->size(),
      stateV1->getRemoteSystemPorts()->size() + 1);
  EXPECT_EQ(
      stateV2->getRemoteInterfaces()->size(),
      stateV1->getRemoteInterfaces()->size() + 1);

  // Update node
  config.dsfNodes()->erase(5);
  auto updatedDsfNode = makeDsfNodeCfg(5);
  updatedDsfNode.name() = "newName";
  config.dsfNodes()->insert({5, updatedDsfNode});
  auto stateV3 = publishAndApplyConfig(stateV2, &config, platform.get());
  ASSERT_NE(nullptr, stateV3);
  EXPECT_EQ(stateV3->getDsfNodes()->size(), 3);
  EXPECT_EQ(
      stateV3->getRemoteSystemPorts()->size(),
      stateV2->getRemoteSystemPorts()->size());
  EXPECT_EQ(
      stateV3->getRemoteInterfaces()->size(),
      stateV2->getRemoteInterfaces()->size());

  // Erase node
  config.dsfNodes()->erase(5);
  auto stateV4 = publishAndApplyConfig(stateV3, &config, platform.get());
  ASSERT_NE(nullptr, stateV4);
  EXPECT_EQ(stateV4->getDsfNodes()->size(), 2);
  EXPECT_EQ(
      stateV4->getRemoteSystemPorts()->size(),
      stateV3->getRemoteSystemPorts()->size() - 1);
  EXPECT_EQ(
      stateV4->getRemoteInterfaces()->size(),
      stateV3->getRemoteInterfaces()->size() - 1);
}

TEST(DsfNode, dsfNodeUpdateLocalDsfNodeConfig) {
  auto platform = createMockPlatform(cfg::SwitchType::VOQ, 0);
  auto stateV0 = std::make_shared<SwitchState>();
  auto config = testConfigA(cfg::SwitchType::VOQ);
  auto stateV1 = publishAndApplyConfig(stateV0, &config, platform.get());
  ASSERT_NE(nullptr, stateV1);
  EXPECT_EQ(stateV1->getDsfNodes()->size(), 2);
  EXPECT_GT(config.dsfNodes()[1].loopbackIps()->size(), 0);
  config.dsfNodes()[1].loopbackIps()->clear();
  EXPECT_EQ(config.dsfNodes()[1].loopbackIps()->size(), 0);
  auto stateV2 = publishAndApplyConfig(stateV1, &config, platform.get());
  ASSERT_NE(nullptr, stateV2);
  EXPECT_EQ(stateV1->getDsfNodes()->size(), 2);
}

TEST(DSFNode, loopackIpsSorted) {
  auto dsfNode = makeDsfNode();
  std::vector<std::string> loopbacks{"100.1.1.0/24", "201::1/64", "300::4/64"};
  dsfNode->setLoopbackIps(loopbacks);
  std::set<folly::CIDRNetwork> loopbacksSorted;
  std::for_each(
      loopbacks.begin(),
      loopbacks.end(),
      [&loopbacksSorted](const auto& loopback) {
        loopbacksSorted.insert(
            folly::IPAddress::createNetwork(loopback, -1, false));
      });
  EXPECT_EQ(loopbacksSorted, dsfNode->getLoopbackIpsSorted());
}
