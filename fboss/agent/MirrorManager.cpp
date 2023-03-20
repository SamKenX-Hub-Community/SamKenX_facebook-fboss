// Copyright 2004-present Facebook. All Rights Reserved.
#include "fboss/agent/MirrorManager.h"

#include <boost/container/flat_set.hpp>
#include <tuple>
#include "fboss/agent/SwSwitch.h"
#include "fboss/agent/state/DeltaFunctions.h"
#include "fboss/agent/state/Interface.h"
#include "fboss/agent/state/Mirror.h"
#include "fboss/agent/state/Route.h"

#include "fboss/agent/state/SwitchState.h"

using boost::container::flat_set;
using facebook::fboss::DeltaFunctions::isEmpty;
using folly::IPAddress;
using std::optional;

namespace facebook::fboss {

MirrorManager::MirrorManager(SwSwitch* sw)
    : sw_(sw),
      v4Manager_(std::make_unique<MirrorManagerV4>(sw)),
      v6Manager_(std::make_unique<MirrorManagerV6>(sw)) {
  sw_->registerStateObserver(this, "MirrorManager");
}
MirrorManager::~MirrorManager() {
  sw_->unregisterStateObserver(this);
}
void MirrorManager::stateUpdated(const StateDelta& delta) {
  if (!hasMirrorChanges(delta)) {
    return;
  }

  auto updateMirrorsFn = [this](const std::shared_ptr<SwitchState>& state) {
    return resolveMirrors(state);
  };
  sw_->updateState("Updating mirrors", updateMirrorsFn);
}

std::shared_ptr<SwitchState> MirrorManager::resolveMirrors(
    const std::shared_ptr<SwitchState>& state) {
  auto mirrors = state->getMirrors()->clone();
  bool mirrorsUpdated = false;

  for (auto iter : std::as_const(*state->getMirrors())) {
    auto mirror = iter.second;
    if (!mirror->getDestinationIp()) {
      /* SPAN mirror does not require resolving */
      continue;
    }
    const auto destinationIp = mirror->getDestinationIp().value();
    std::shared_ptr<Mirror> updatedMirror = destinationIp.isV4()
        ? v4Manager_->updateMirror(mirror)
        : v6Manager_->updateMirror(mirror);
    if (updatedMirror) {
      XLOG(DBG2) << "Mirror: " << updatedMirror->getID() << " updated.";
      mirrors->updateNode(updatedMirror);
      mirrorsUpdated = true;
    }
  }
  if (!mirrorsUpdated) {
    return std::shared_ptr<SwitchState>(nullptr);
  }
  auto updatedState = state->clone();
  updatedState->resetMirrors(mirrors);
  return updatedState;
}

bool MirrorManager::hasMirrorChanges(const StateDelta& delta) {
  return (sw_->getState()->getMirrors()->size() > 0) &&
      (!isEmpty(delta.getMirrorsDelta()) || !isEmpty(delta.getFibsDelta()) ||
       std::any_of(
           std::begin(delta.getVlansDelta()),
           std::end(delta.getVlansDelta()),
           [](const VlanDelta& vlanDelta) {
             return !isEmpty(vlanDelta.getArpDelta()) ||
                 !isEmpty(vlanDelta.getNdpDelta());
           }));
}

} // namespace facebook::fboss
