// Copyright 2004-present Facebook. All Rights Reserved.

#include "fboss/agent/DsfSubscriber.h"

namespace facebook::fboss {

std::vector<std::string> DsfSubscriber::getSystemPortsPath() {
  return {"agent", "switchState", "systemPortMap"};
}

std::vector<std::string> DsfSubscriber::getInterfacesPath() {
  return {"agent", "switchState", "interfaceMap"};
}
} // namespace facebook::fboss
