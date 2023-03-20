/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/qsfp_service/test/TransceiverManagerTestHelper.h"

#include "fboss/qsfp_service/module/tests/MockSffModule.h"
#include "fboss/qsfp_service/module/tests/MockTransceiverImpl.h"

#include <gmock/gmock.h>

namespace facebook::fboss {

using namespace ::testing;

class QsfpModuleTest : public TransceiverManagerTestHelper {
 public:
  void SetUp() override {
    TransceiverManagerTestHelper::SetUp();
    setupQsfp();
  }

  void setupQsfp() {
    auto transceiverImpl = std::make_unique<NiceMock<MockTransceiverImpl>>();
    // So we can check what happens during testing
    transImpl_ = transceiverImpl.get();
    qsfp_ = static_cast<MockSffModule*>(
        transceiverManager_->overrideTransceiverForTesting(
            kTcvrID,
            std::make_unique<MockSffModule>(
                transceiverManager_.get(), std::move(transceiverImpl))));
    qsfp_->setVendorPN();

    gflags::SetCommandLineOptionWithMode(
        "tx_enable_interval", "0", gflags::SET_FLAGS_DEFAULT);

    gflags::SetCommandLineOptionWithMode(
        "customize_interval", "0", gflags::SET_FLAGS_DEFAULT);

    EXPECT_EQ(transImpl_->getName().toString(), qsfp_->getNameString());
    EXPECT_CALL(*transImpl_, detectTransceiver()).WillRepeatedly(Return(true));
    qsfp_->detectPresence();
  }

  void triggerPortsChanged(
      const TransceiverManager::OverrideTcvrToPortAndProfile&
          newTcvrToPortAndProfile,
      bool needResetDataPath = false) {
    transceiverManager_->setOverrideTcvrToPortAndProfileForTesting(
        newTcvrToPortAndProfile);
    // Refresh once to trigger iphy
    transceiverManager_->refreshStateMachines();
    // Then we can call programTransceiver() directly so that we can check the
    // error case
    transceiverManager_->programTransceiver(kTcvrID, needResetDataPath);
  }

  NiceMock<MockTransceiverImpl>* transImpl_;
  MockSffModule* qsfp_;
  const TransceiverID kTcvrID = TransceiverID(0);
};

TEST_F(QsfpModuleTest, setRateSelect) {
  ON_CALL(*qsfp_, setRateSelectIfSupported(_, _, _))
      .WillByDefault(
          Invoke(qsfp_, &MockSffModule::actualSetRateSelectIfSupported));
  ON_CALL(*qsfp_, customizationSupported()).WillByDefault(Return(true));
  EXPECT_CALL(*qsfp_, setPowerOverrideIfSupportedLocked(_)).Times(AtLeast(1));
  EXPECT_CALL(*qsfp_, setCdrIfSupported(_, _, _)).Times(AtLeast(1));

  // every customize call does 1 write for enabling tx, plus any other
  // writes for settings changes.
  {
    InSequence a;
    // Unsupported
    EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(0);
    qsfp_->customizeTransceiver(cfg::PortSpeed::FORTYG);
    qsfp_->customizeTransceiver(cfg::PortSpeed::HUNDREDG);

    // Using V1
    qsfp_->setRateSelect(
        RateSelectState::EXTENDED_RATE_SELECT_V1,
        RateSelectSetting::FROM_6_6GB_AND_ABOVE);
    qsfp_->customizeTransceiver(cfg::PortSpeed::FORTYG);

    qsfp_->setRateSelect(
        RateSelectState::EXTENDED_RATE_SELECT_V2,
        RateSelectSetting::LESS_THAN_12GB);
    // 40G + LESS_THAN_12GB -> no change
    qsfp_->customizeTransceiver(cfg::PortSpeed::FORTYG);
    // 100G + LESS_THAN_12GB -> needs change
    EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(2);
    qsfp_->customizeTransceiver(cfg::PortSpeed::HUNDREDG);

    qsfp_->setRateSelect(
        RateSelectState::EXTENDED_RATE_SELECT_V2,
        RateSelectSetting::FROM_24GB_to_26GB);
    // 40G + FROM_24GB_to_26GB -> needs change
    EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(2);
    qsfp_->customizeTransceiver(cfg::PortSpeed::FORTYG);
    // 100G + FROM_24GB_to_26GB -> no change
    EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(0);
    qsfp_->customizeTransceiver(cfg::PortSpeed::HUNDREDG);
  }
}

TEST_F(QsfpModuleTest, retrieveRateSelectSetting) {
  auto data = qsfp_->getRateSelectSettingValue(RateSelectState::UNSUPPORTED);
  EXPECT_EQ(data, RateSelectSetting::UNSUPPORTED);

  data = qsfp_->getRateSelectSettingValue(
      RateSelectState::APPLICATION_RATE_SELECT);
  EXPECT_EQ(data, RateSelectSetting::UNSUPPORTED);

  EXPECT_CALL(*qsfp_, getSettingsValue(_, _))
      .WillRepeatedly(Return(0b01010101));
  data = qsfp_->getRateSelectSettingValue(
      RateSelectState::EXTENDED_RATE_SELECT_V1);
  EXPECT_EQ(data, RateSelectSetting::FROM_2_2GB_TO_6_6GB);

  data = qsfp_->getRateSelectSettingValue(
      RateSelectState::EXTENDED_RATE_SELECT_V2);
  EXPECT_EQ(data, RateSelectSetting::FROM_12GB_TO_24GB);
}

TEST_F(QsfpModuleTest, setCdr) {
  ON_CALL(*qsfp_, setCdrIfSupported(_, _, _))
      .WillByDefault(Invoke(qsfp_, &MockSffModule::actualSetCdrIfSupported));
  ON_CALL(*qsfp_, customizationSupported()).WillByDefault(Return(true));

  EXPECT_CALL(*qsfp_, setPowerOverrideIfSupportedLocked(_)).Times(AtLeast(1));
  EXPECT_CALL(*qsfp_, setRateSelectIfSupported(_, _, _)).Times(AtLeast(1));

  // every customize call does 1 write for enabling tx, plus any other
  // writes for settings changes.
  {
    InSequence a;
    EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(0);
    // Unsupported
    qsfp_->customizeTransceiver(cfg::PortSpeed::FORTYG);
    qsfp_->customizeTransceiver(cfg::PortSpeed::HUNDREDG);

    qsfp_->setCdrState(FeatureState::DISABLED, FeatureState::DISABLED);
    // Disabled + 40G
    qsfp_->customizeTransceiver(cfg::PortSpeed::FORTYG);
    // Disabled + 100G
    EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(1);
    qsfp_->customizeTransceiver(cfg::PortSpeed::HUNDREDG); // CHECK

    qsfp_->setCdrState(FeatureState::ENABLED, FeatureState::ENABLED);
    // Enabled + 40G
    EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(1);
    qsfp_->customizeTransceiver(cfg::PortSpeed::FORTYG); // CHECK
    // Enabled + 100G
    EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(0);
    qsfp_->customizeTransceiver(cfg::PortSpeed::HUNDREDG); // CHECK

    // One of rx an tx enabled with the other disabled
    qsfp_->setCdrState(FeatureState::DISABLED, FeatureState::ENABLED);
    // 40G
    EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(1);
    qsfp_->customizeTransceiver(cfg::PortSpeed::FORTYG); // CHECK
    // 100G
    EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(1);
    qsfp_->customizeTransceiver(cfg::PortSpeed::HUNDREDG);
  }
}

TEST_F(QsfpModuleTest, portsChangedAllDown25G) {
  ON_CALL(*qsfp_, getTransceiverInfo()).WillByDefault(Return(qsfp_->fakeInfo_));
  ON_CALL(*qsfp_, customizationSupported()).WillByDefault(Return(true));

  // should customize w/ 25G
  EXPECT_CALL(*qsfp_, setCdrIfSupported(cfg::PortSpeed::TWENTYFIVEG, _, _))
      .Times(1);

  triggerPortsChanged(
      {{kTcvrID,
        {
            {PortID(1), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
            {PortID(2), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
            {PortID(3), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
            {PortID(4), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
        }}});
}

TEST_F(QsfpModuleTest, portsChanged50G) {
  ON_CALL(*qsfp_, getTransceiverInfo()).WillByDefault(Return(qsfp_->fakeInfo_));
  ON_CALL(*qsfp_, customizationSupported()).WillByDefault(Return(true));

  // should customize w/ 50G
  EXPECT_CALL(*qsfp_, setCdrIfSupported(cfg::PortSpeed::FIFTYG, _, _)).Times(1);

  // We only store enabled ports
  triggerPortsChanged(
      {{kTcvrID,
        {
            {PortID(1), cfg::PortProfileID::PROFILE_50G_2_NRZ_CL74_COPPER},
            {PortID(3), cfg::PortProfileID::PROFILE_50G_2_NRZ_CL74_COPPER},
        }}});
}

TEST_F(QsfpModuleTest, portsChangedOnePortPerModule) {
  setupQsfp();
  ON_CALL(*qsfp_, getTransceiverInfo()).WillByDefault(Return(qsfp_->fakeInfo_));
  ON_CALL(*qsfp_, customizationSupported()).WillByDefault(Return(true));

  EXPECT_CALL(*qsfp_, setCdrIfSupported(cfg::PortSpeed::HUNDREDG, _, _))
      .Times(1);

  triggerPortsChanged(
      {{kTcvrID,
        {
            {PortID(1), cfg::PortProfileID::PROFILE_100G_4_NRZ_CL91_OPTICAL},
        }}});
}

TEST_F(QsfpModuleTest, portsChangedSpeedMismatch) {
  EXPECT_ANY_THROW(
      triggerPortsChanged(
          {{kTcvrID,
            {
                {PortID(1), cfg::PortProfileID::PROFILE_50G_2_NRZ_CL74_COPPER},
                {PortID(3),
                 cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
            }}}););
}

TEST_F(QsfpModuleTest, skipCustomizingForRefresh) {
  // With new state machine, we no longer use Transceiver::refresh() to
  // customize transceiver. Instead, the state machine will use
  // programTransceiver() to do the job following the new port programming order
  EXPECT_CALL(*qsfp_, setCdrIfSupported(_, _, _)).Times(0);
  TransceiverManager::OverrideTcvrToPortAndProfile oneTcvrTo4X25G = {
      {kTcvrID,
       {
           {PortID(1), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
           {PortID(2), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
           {PortID(3), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
           {PortID(4), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
       }}};
  transceiverManager_->setOverrideTcvrToPortAndProfileForTesting(
      oneTcvrTo4X25G);
  // Refresh once to trigger iphy
  transceiverManager_->refreshStateMachines();
  qsfp_->refresh();
}

TEST_F(QsfpModuleTest, skipCustomizingMissingIphyPorts) {
  // should not customize if we don't have knowledge of all ports
  EXPECT_CALL(*qsfp_, setCdrIfSupported(_, _, _)).Times(0);
  transceiverManager_->programTransceiver(kTcvrID, false);
}

TEST_F(QsfpModuleTest, skipCustomizingMissingTransceiver) {
  ON_CALL(*qsfp_, getTransceiverInfo()).WillByDefault(Return(qsfp_->fakeInfo_));
  // Trigger transceiver removal
  EXPECT_CALL(*transImpl_, detectTransceiver()).WillRepeatedly(Return(false));
  qsfp_->detectPresence();

  EXPECT_CALL(*qsfp_, setCdrIfSupported(_, _, _)).Times(0);
  triggerPortsChanged(
      {{kTcvrID,
        {
            {PortID(1), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
            {PortID(2), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
            {PortID(3), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
            {PortID(4), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
        }}});
}

TEST_F(QsfpModuleTest, skipCustomizingCopperTransceiver) {
  ON_CALL(*qsfp_, getTransceiverInfo()).WillByDefault(Return(qsfp_->fakeInfo_));
  // Should get detected as copper and skip all customization
  EXPECT_CALL(*qsfp_, getQsfpTransmitterTechnology())
      .WillRepeatedly(Return(TransmitterTechnology::COPPER));

  EXPECT_CALL(*qsfp_, setCdrIfSupported(_, _, _)).Times(0);
  triggerPortsChanged(
      {{kTcvrID,
        {
            {PortID(1), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
            {PortID(2), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
            {PortID(3), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
            {PortID(4), cfg::PortProfileID::PROFILE_25G_1_NRZ_NOFEC_OPTICAL},
        }}});
}

TEST_F(QsfpModuleTest, updateQsfpDataPartial) {
  // Ensure that partial updates don't ever call writeTranscevier,
  // which needs to gain control of the bus and slows the call
  // down drastically.
  EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(0);
  qsfp_->actualUpdateQsfpData(false);
}

TEST_F(QsfpModuleTest, updateQsfpDataFull) {
  // Bit of a hack to ensure we have flatMem_ == false.
  ON_CALL(*transImpl_, readTransceiver(_, _))
      .WillByDefault(DoAll(
          InvokeWithoutArgs(qsfp_, &MockSffModule::setFlatMem), Return(0)));

  // Full updates do need to write to select higher pages
  EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(AtLeast(1));

  qsfp_->actualUpdateQsfpData(true);
}

TEST_F(QsfpModuleTest, readTransceiver) {
  // Skip the length field and confirm that the length of data in response is 1.
  // Page is also skipped so there should not be a write to byte 127.
  EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(0);
  EXPECT_CALL(*transImpl_, readTransceiver(_, _)).Times(1);
  TransceiverIOParameters param;
  param.offset() = 0;
  auto buf = qsfp_->readTransceiver(param);
  EXPECT_EQ(buf->length(), 1);

  // Test for a specific length
  EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(0);
  EXPECT_CALL(*transImpl_, readTransceiver(_, _)).Times(1);
  param.length() = 10;
  buf = qsfp_->readTransceiver(param);
  EXPECT_EQ(buf->length(), *param.length());

  // Set the page
  EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(1);
  EXPECT_CALL(*transImpl_, readTransceiver(_, _)).Times(1);
  param.page() = 3;
  buf = qsfp_->readTransceiver(param);
  EXPECT_EQ(buf->length(), *param.length());

  // Test on a transceiver that fails detection
  EXPECT_CALL(*transImpl_, detectTransceiver()).WillRepeatedly(Return(false));
  EXPECT_CALL(*transImpl_, readTransceiver(_, _)).Times(0);
  qsfp_->detectPresence();
  buf = qsfp_->readTransceiver(param);
  EXPECT_EQ(buf->length(), 0);
}

TEST_F(QsfpModuleTest, writeTransceiver) {
  // Expect a call to writeTransceiver and the result to be successful
  EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(1);
  TransceiverIOParameters param;
  param.offset() = 0x23;
  EXPECT_EQ(qsfp_->writeTransceiver(param, 0xab), true);

  // Set the page
  EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(2);
  param.page() = 3;
  EXPECT_EQ(qsfp_->writeTransceiver(param, 0xde), true);

  // Test on a transceiver that fails detection, the result should be false
  EXPECT_CALL(*transImpl_, detectTransceiver()).WillRepeatedly(Return(false));
  EXPECT_CALL(*transImpl_, writeTransceiver(_, _)).Times(0);
  qsfp_->detectPresence();
  EXPECT_EQ(qsfp_->writeTransceiver(param, 0xac), false);
}

TEST_F(QsfpModuleTest, populateSnapshots) {
  auto snapshots = qsfp_->getTransceiverSnapshots().getSnapshots();
  EXPECT_TRUE(snapshots.empty());
  qsfp_->refresh();
  snapshots = qsfp_->getTransceiverSnapshots().getSnapshots();
  EXPECT_FALSE(snapshots.empty());

  // fill the buffer
  for (auto i = 1; i < snapshots.maxSize(); i++) {
    qsfp_->refresh();
  }
  snapshots = qsfp_->getTransceiverSnapshots().getSnapshots();

  // Verify that we stay at the max size
  EXPECT_EQ(snapshots.size(), snapshots.maxSize());
  qsfp_->refresh();
  snapshots = qsfp_->getTransceiverSnapshots().getSnapshots();
  EXPECT_EQ(snapshots.size(), snapshots.maxSize());
}

TEST_F(QsfpModuleTest, getNewTcvrInfo) {
  qsfp_->refresh(); // trigger TcvrInfo update
  qsfp_->useActualGetTransceiverInfo();
  auto info = qsfp_->getTransceiverInfo();

  // First check that we have actual content instead of just equal because
  // it's empty on both new and old. Part number is set by the mock module and
  // should always be non-empty.
  EXPECT_TRUE(
      info.tcvrState()->vendor() &&
      !info.tcvrState()->vendor()->partNumber()->empty());

  EXPECT_EQ(info.tcvrState()->present(), info.present());
  EXPECT_EQ(info.tcvrState()->transceiver(), info.transceiver());
  EXPECT_EQ(info.tcvrState()->port(), info.port());
  EXPECT_EQ(info.tcvrState()->mediaLaneSignals(), info.mediaLaneSignals());
  EXPECT_EQ(info.tcvrState()->vendor(), info.vendor());
  EXPECT_EQ(info.tcvrState()->cable(), info.cable());
  EXPECT_EQ(info.tcvrState()->thresholds(), info.thresholds());
  EXPECT_EQ(info.tcvrState()->settings(), info.settings());
  EXPECT_EQ(info.tcvrState()->hostLaneSignals(), info.hostLaneSignals());
  EXPECT_EQ(info.tcvrState()->signalFlag(), info.signalFlag());
  EXPECT_EQ(
      info.tcvrState()->extendedSpecificationComplianceCode(),
      info.extendedSpecificationComplianceCode());
  EXPECT_EQ(
      info.tcvrState()->transceiverManagementInterface(),
      info.transceiverManagementInterface());
  EXPECT_EQ(info.tcvrState()->identifier(), info.identifier());
  EXPECT_EQ(info.tcvrState()->status(), info.status());
  EXPECT_EQ(info.tcvrState()->eepromCsumValid(), info.eepromCsumValid());
  EXPECT_EQ(
      info.tcvrState()->moduleMediaInterface(), info.moduleMediaInterface());

  EXPECT_EQ(info.tcvrStats()->sensor(), info.sensor());
  EXPECT_EQ(info.tcvrStats()->channels(), info.channels());
  EXPECT_EQ(info.tcvrStats()->stats(), info.stats());
  EXPECT_EQ(info.tcvrStats()->vdmDiagsStats(), info.vdmDiagsStats());
  EXPECT_EQ(
      info.tcvrStats()->vdmDiagsStatsForOds(), info.vdmDiagsStatsForOds());
  EXPECT_EQ(info.tcvrStats()->remediationCounter(), info.remediationCounter());
}

} // namespace facebook::fboss
