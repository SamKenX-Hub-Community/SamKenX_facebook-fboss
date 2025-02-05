/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/agent/StaticL2ForNeighborObserver.h"

#include "fboss/agent/StaticL2ForNeighborSwSwitchUpdater.h"
#include "fboss/agent/SwSwitch.h"
namespace facebook::fboss {

StaticL2ForNeighborObserver::StaticL2ForNeighborObserver(SwSwitch* sw)
    : sw_(sw) {
  sw_->registerStateObserver(this, "StaticL2ForNeighborObserver");
}
StaticL2ForNeighborObserver::~StaticL2ForNeighborObserver() {
  sw_->unregisterStateObserver(this);
}
void StaticL2ForNeighborObserver::stateUpdated(const StateDelta& stateDelta) {
  StaticL2ForNeighborSwSwitchUpdater updater(sw_);
  updater.stateUpdated(stateDelta);
}

} // namespace facebook::fboss
