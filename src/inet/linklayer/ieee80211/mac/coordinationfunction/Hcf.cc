//
// Copyright (C) 2016 OpenSim Ltd.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckProcedure.h"
#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HcfFs.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/recipient/RecipientAckProcedure.h"

namespace inet {
namespace ieee80211 {

Define_Module(Hcf);

void Hcf::initialize(int stage)
{
    ModeSetListener::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        mac = check_and_cast<Ieee80211Mac *>(getContainingNicModule(this));
        startRxTimer = new cMessage("startRxTimeout");
        inactivityTimer = new cMessage("blockAckInactivityTimer");
        numEdcafs = par("numEdcafs");
        edca = check_and_cast<Edca *>(getSubmodule("edca"));
        hcca = check_and_cast<Hcca *>(getSubmodule("hcca"));
        tx = check_and_cast<ITx *>(getModuleByPath(par("txModule")));
        rx = check_and_cast<IRx *>(getModuleByPath(par("rxModule")));
        dataAndMgmtRateControl = dynamic_cast<IRateControl*>(getSubmodule("rateControl"));
        originatorBlockAckAgreementPolicy = dynamic_cast<IOriginatorBlockAckAgreementPolicy*>(getSubmodule("originatorBlockAckAgreementPolicy"));
        recipientBlockAckAgreementPolicy = dynamic_cast<IRecipientBlockAckAgreementPolicy*>(getSubmodule("recipientBlockAckAgreementPolicy"));
        rateSelection = check_and_cast<IQoSRateSelection *>(getSubmodule("rateSelection"));
        frameSequenceHandler = new FrameSequenceHandler();
        originatorDataService = check_and_cast<IOriginatorMacDataService *>(getSubmodule(("originatorMacDataService")));
        recipientDataService = check_and_cast<IRecipientQoSMacDataService*>(getSubmodule("recipientMacDataService"));
        originatorAckPolicy = check_and_cast<IOriginatorQoSAckPolicy*>(getSubmodule("originatorAckPolicy"));
        recipientAckPolicy = check_and_cast<IRecipientQoSAckPolicy*>(getSubmodule("recipientAckPolicy"));
        edcaMgmtAndNonQoSRecoveryProcedure = check_and_cast<NonQoSRecoveryProcedure *>(getSubmodule("edcaMgmtAndNonQoSRecoveryProcedure"));
        singleProtectionMechanism = check_and_cast<SingleProtectionMechanism*>(getSubmodule("singleProtectionMechanism"));
        rtsProcedure = new RtsProcedure();
        rtsPolicy = check_and_cast<IRtsPolicy*>(getSubmodule("rtsPolicy"));
        recipientAckProcedure = new RecipientAckProcedure();
        ctsProcedure = new CtsProcedure();
        ctsPolicy = check_and_cast<ICtsPolicy*>(getSubmodule("ctsPolicy"));
        if (originatorBlockAckAgreementPolicy && recipientBlockAckAgreementPolicy) {
            recipientBlockAckAgreementHandler = new RecipientBlockAckAgreementHandler();
            originatorBlockAckAgreementHandler = new OriginatorBlockAckAgreementHandler();
            originatorBlockAckProcedure = new OriginatorBlockAckProcedure();
            recipientBlockAckProcedure = new RecipientBlockAckProcedure();
        }
        for (int ac = 0; ac < numEdcafs; ac++) {
            edcaPendingQueues.push_back(new PendingQueue(par("maxQueueSize"), nullptr));
            edcaDataRecoveryProcedures.push_back(check_and_cast<QoSRecoveryProcedure *>(getSubmodule("edcaDataRecoveryProcedures", ac)));
            edcaAckHandlers.push_back(new QoSAckHandler());
            edcaInProgressFrames.push_back(new InProgressFrames(edcaPendingQueues[ac], originatorDataService, edcaAckHandlers[ac]));
            edcaTxops.push_back(check_and_cast<TxopProcedure *>(getSubmodule("edcaTxopProcedures", ac)));
            stationRetryCounters.push_back(new StationRetryCounters());
        }
    }
}

void Hcf::handleMessage(cMessage* msg)
{
    if (msg == startRxTimer) {
        if (!isReceptionInProgress())
            frameSequenceHandler->handleStartRxTimeout();
    }
    else if (msg == inactivityTimer) {
        if (originatorBlockAckAgreementHandler && recipientBlockAckAgreementHandler) {
            originatorBlockAckAgreementHandler->blockAckAgreementExpired(this, this);
            recipientBlockAckAgreementHandler->blockAckAgreementExpired(this, this);
        }
        else
            throw cRuntimeError("Unknown event");
    }
    else
        throw cRuntimeError("Unknown msg type");
}

void Hcf::processUpperFrame(Ieee80211DataOrMgmtFrame* frame)
{
    Enter_Method("processUpperFrame(%s)", frame->getName());
    EV_INFO << "Processing upper frame: " << frame->getName() << endl;
    AccessCategory ac = AccessCategory(-1);
    if (dynamic_cast<Ieee80211ManagementFrame *>(frame)) // TODO: + non-QoS frames
        ac = AccessCategory::AC_BE;
    else if (auto dataFrame = dynamic_cast<Ieee80211DataFrame *>(frame))
        ac = edca->classifyFrame(dataFrame);
    else
        throw cRuntimeError("Unknown message type");
    EV_INFO << "The upper frame has been classified as a " << printAccessCategory(ac) << " frame." << endl;
    if (edcaPendingQueues[ac]->insert(frame)) {
        EV_INFO << "Frame " << frame->getName() << " has been inserted into the PendingQueue." << endl;
        auto edcaf = edca->getChannelOwner();
        if (edcaf == nullptr || edcaf->getAccessCategory() != ac) {
            EV_DETAIL << "Requesting channel for access category " << printAccessCategory(ac) << endl;
            edca->requestChannelAccess(ac, this);
        }
    }
    else {
        EV_INFO << "Frame " << frame->getName() << " has been dropped because the PendingQueue is full." << endl;
        delete frame;
    }
}

void Hcf::scheduleStartRxTimer(simtime_t timeout)
{
    Enter_Method_Silent();
    scheduleAt(simTime() + timeout, startRxTimer);
}

void Hcf::scheduleInactivityTimer(simtime_t timeout)
{
    Enter_Method_Silent();
    if (inactivityTimer->isScheduled())
        cancelEvent(inactivityTimer);
    scheduleAt(simTime() + timeout, inactivityTimer);
}

void Hcf::processLowerFrame(Ieee80211Frame* frame)
{
    Enter_Method_Silent();
    auto edcaf = edca->getChannelOwner();
    if (edcaf && frameSequenceHandler->isSequenceRunning()) {
        frameSequenceHandler->processResponse(frame);
        cancelEvent(startRxTimer);
    }
    else if (hcca->isOwning())
        throw cRuntimeError("Hcca is unimplemented!");
    else
        recipientProcessReceivedFrame(frame);
}

void Hcf::channelGranted(IChannelAccess* channelAccess)
{
    auto edcaf = check_and_cast<Edcaf*>(channelAccess);
    if (edcaf) {
        AccessCategory ac = edcaf->getAccessCategory();
        edcaTxops[ac]->startTxop(ac);
        startFrameSequence(ac);
        handleInternalCollision(edca->getInternallyCollidedEdcafs());
    }
    else
        throw cRuntimeError("Channel access granted but channel owner not found!");
}

FrameSequenceContext* Hcf::buildContext(AccessCategory ac)
{
    auto qosContext = new QoSContext(originatorAckPolicy, originatorBlockAckProcedure, originatorBlockAckAgreementHandler, edcaTxops[ac]);
    return new FrameSequenceContext(modeSet, edcaInProgressFrames[ac], rtsProcedure, rtsPolicy, nullptr, qosContext);
}

void Hcf::startFrameSequence(AccessCategory ac)
{
    frameSequenceHandler->startFrameSequence(new HcfFs(), buildContext(ac), this);
}

void Hcf::handleInternalCollision(std::vector<Edcaf*> internallyCollidedEdcafs)
{
    for (auto edcaf : internallyCollidedEdcafs) {
        AccessCategory ac = edcaf->getAccessCategory();
        Ieee80211DataOrMgmtFrame *internallyCollidedFrame = edcaInProgressFrames[ac]->getFrameToTransmit();
        bool retryLimitReached = false;
        if (auto dataFrame = dynamic_cast<Ieee80211DataFrame *>(internallyCollidedFrame)) { // TODO: QoSDataFrame
            edcaDataRecoveryProcedures[ac]->dataFrameTransmissionFailed(dataFrame);
            retryLimitReached = edcaDataRecoveryProcedures[ac]->isRetryLimitReached(dataFrame);
        }
        else if (auto mgmtFrame = dynamic_cast<Ieee80211ManagementFrame*>(internallyCollidedFrame)) {
            ASSERT(ac == AccessCategory::AC_BE);
            edcaMgmtAndNonQoSRecoveryProcedure->dataOrMgmtFrameTransmissionFailed(mgmtFrame, stationRetryCounters[AccessCategory::AC_BE]);
            retryLimitReached = edcaMgmtAndNonQoSRecoveryProcedure->isRetryLimitReached(mgmtFrame);
        }
        else // TODO: + NonQoSDataFrame
            throw cRuntimeError("Unknown frame");
        if (retryLimitReached) {
            edcaInProgressFrames[ac]->dropFrame(internallyCollidedFrame);
            if (hasFrameToTransmit(ac))
                edcaf->requestChannel(this);
        }
        else
            edcaf->requestChannel(this);
    }
}

void Hcf::frameSequenceFinished()
{
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        bool startContention = hasFrameToTransmit(); // TODO: outstanding frame
        edcaf->releaseChannel(this);
        AccessCategory ac = edcaf->getAccessCategory();
        edcaTxops[ac]->stopTxop();
        if (startContention)
            edcaf->requestChannel(this);
    }
    else if (hcca->isOwning()) {
        hcca->releaseChannel(this);
        throw cRuntimeError("Hcca is unimplemented!");
    }
    else
        throw cRuntimeError("Frame sequence finished but channel owner not found!");
}

void Hcf::recipientProcessReceivedFrame(Ieee80211Frame* frame)
{
    if (auto dataOrMgmtFrame = dynamic_cast<Ieee80211DataOrMgmtFrame *>(frame))
        recipientAckProcedure->processReceivedFrame(dataOrMgmtFrame, check_and_cast<IRecipientAckPolicy*>(recipientAckPolicy), this);
    if (auto dataFrame = dynamic_cast<Ieee80211DataFrame*>(frame)) {
        if (dataFrame->getType() == ST_DATA_WITH_QOS && recipientBlockAckAgreementHandler)
            recipientBlockAckAgreementHandler->qosFrameReceived(dataFrame, this);
        sendUp(recipientDataService->dataFrameReceived(dataFrame, recipientBlockAckAgreementHandler));
    }
    else if (auto mgmtFrame = dynamic_cast<Ieee80211ManagementFrame*>(frame)) {
        sendUp(recipientDataService->managementFrameReceived(mgmtFrame));
        recipientProcessReceivedManagementFrame(mgmtFrame);
    }
    else { // TODO: else if (auto ctrlFrame = dynamic_cast<Ieee80211ControlFrame*>(frame))
        sendUp(recipientDataService->controlFrameReceived(frame, recipientBlockAckAgreementHandler));
        recipientProcessReceivedControlFrame(frame);
    }
}

void Hcf::recipientProcessReceivedControlFrame(Ieee80211Frame* frame)
{
    if (auto rtsFrame = dynamic_cast<Ieee80211RTSFrame *>(frame))
        ctsProcedure->processReceivedRts(rtsFrame, ctsPolicy, this);
    else if (auto blockAckRequest = dynamic_cast<Ieee80211BasicBlockAckReq*>(frame)) {
        if (recipientBlockAckProcedure)
            recipientBlockAckProcedure->processReceivedBlockAckReq(blockAckRequest, recipientAckPolicy, recipientBlockAckAgreementHandler, this);
    }
    else
        throw cRuntimeError("Unknown control frame");
}

void Hcf::recipientProcessReceivedManagementFrame(Ieee80211ManagementFrame* frame)
{
    if (recipientBlockAckAgreementHandler && originatorBlockAckAgreementHandler) {
        if (auto addbaRequest = dynamic_cast<Ieee80211AddbaRequest *>(frame))
            recipientBlockAckAgreementHandler->processReceivedAddbaRequest(addbaRequest, recipientBlockAckAgreementPolicy, this);
        else if (auto addbaResp = dynamic_cast<Ieee80211AddbaResponse *>(frame))
            originatorBlockAckAgreementHandler->processReceivedAddbaResp(addbaResp, originatorBlockAckAgreementPolicy, this);
        else if (auto delba = dynamic_cast<Ieee80211Delba*>(frame)) {
            if (delba->getInitiator())
                recipientBlockAckAgreementHandler->processReceivedDelba(delba, recipientBlockAckAgreementPolicy);
            else
                originatorBlockAckAgreementHandler->processReceivedDelba(delba, originatorBlockAckAgreementPolicy);
        }
        else
            throw cRuntimeError("Unknown management frame");
    }
    else
        throw cRuntimeError("Unknown management frame");
}

void Hcf::transmissionComplete(Ieee80211Frame *frame)
{
    auto edcaf = edca->getChannelOwner();
    if (edcaf)
        frameSequenceHandler->transmissionComplete();
    else if (hcca->isOwning())
        throw cRuntimeError("Hcca is unimplemented!");
    else
        recipientProcessTransmittedControlResponseFrame(frame);
    delete frame;
}

void Hcf::originatorProcessRtsProtectionFailed(Ieee80211DataOrMgmtFrame* protectedFrame)
{
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        EV_INFO << "RTS frame transmission failed\n";
        AccessCategory ac = edcaf->getAccessCategory();
        bool retryLimitReached = false;
        if (auto dataFrame = dynamic_cast<Ieee80211DataFrame *>(protectedFrame)) {
            edcaDataRecoveryProcedures[ac]->rtsFrameTransmissionFailed(dataFrame);
            retryLimitReached = edcaDataRecoveryProcedures[ac]->isRtsFrameRetryLimitReached(dataFrame);
        }
        else if (auto mgmtFrame = dynamic_cast<Ieee80211ManagementFrame *>(protectedFrame)) {
            edcaMgmtAndNonQoSRecoveryProcedure->rtsFrameTransmissionFailed(mgmtFrame, stationRetryCounters[ac]);
            retryLimitReached = edcaMgmtAndNonQoSRecoveryProcedure->isRtsFrameRetryLimitReached(dataFrame);
        }
        else
            throw cRuntimeError("Unknown frame"); // TODO: QoSDataFrame, NonQoSDataFrame
        if (retryLimitReached) {
            edcaInProgressFrames[ac]->dropFrame(protectedFrame);
            delete protectedFrame;
        }
    }
    else
        throw cRuntimeError("Hcca is unimplemented!");
}

void Hcf::originatorProcessTransmittedFrame(Ieee80211Frame* transmittedFrame)
{
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        AccessCategory ac = edcaf->getAccessCategory();
        if (transmittedFrame->getReceiverAddress().isMulticast())
            edcaDataRecoveryProcedures[ac]->multicastFrameTransmitted();
        if (auto dataFrame = dynamic_cast<Ieee80211DataFrame*>(transmittedFrame))
            originatorProcessTransmittedDataFrame(dataFrame, ac);
        else if (auto mgmtFrame = dynamic_cast<Ieee80211ManagementFrame*>(transmittedFrame))
            originatorProcessTransmittedManagementFrame(mgmtFrame, ac);
        else // TODO: Ieee80211ControlFrame
            originatorProcessTransmittedControlFrame(transmittedFrame, ac);
    }
    else if (hcca->isOwning())
        throw cRuntimeError("Hcca is unimplemented");
    else
        throw cRuntimeError("Frame transmitted but channel owner not found");
}

void Hcf::originatorProcessTransmittedDataFrame(Ieee80211DataFrame* dataFrame, AccessCategory ac)
{
    edcaAckHandlers[ac]->processTransmittedDataOrMgmtFrame(dataFrame);
    if (originatorBlockAckAgreementHandler)
        originatorBlockAckAgreementHandler->processTransmittedDataFrame(dataFrame, originatorBlockAckAgreementPolicy, this);
    if (dataFrame->getAckPolicy() == NO_ACK)
        edcaInProgressFrames[ac]->dropFrame(dataFrame);
}

void Hcf::originatorProcessTransmittedManagementFrame(Ieee80211ManagementFrame* mgmtFrame, AccessCategory ac)
{
    if (auto addbaReq = dynamic_cast<Ieee80211AddbaRequest*>(mgmtFrame)) {
        edcaAckHandlers[ac]->processTransmittedDataOrMgmtFrame(addbaReq);
        if (originatorBlockAckAgreementHandler)
            originatorBlockAckAgreementHandler->processTransmittedAddbaReq(addbaReq);
    }
    else if (auto addbaResp = dynamic_cast<Ieee80211AddbaResponse*>(mgmtFrame)) {
        edcaAckHandlers[ac]->processTransmittedDataOrMgmtFrame(addbaResp);
        recipientBlockAckAgreementHandler->processTransmittedAddbaResp(addbaResp, this);
    }
    else if (auto delba = dynamic_cast<Ieee80211Delba *>(mgmtFrame)) {
        edcaAckHandlers[ac]->processTransmittedDataOrMgmtFrame(delba);
        if (delba->getInitiator())
            originatorBlockAckAgreementHandler->processTransmittedDelba(delba);
        else
            recipientBlockAckAgreementHandler->processTransmittedDelba(delba);
    }
    else
        throw cRuntimeError("Unknown management frame");
}

void Hcf::originatorProcessTransmittedControlFrame(Ieee80211Frame* controlFrame, AccessCategory ac)
{
    if (auto blockAckReq = dynamic_cast<Ieee80211BlockAckReq*>(controlFrame))
        edcaAckHandlers[ac]->processTransmittedBlockAckReq(blockAckReq);
    else if (auto rtsFrame = dynamic_cast<Ieee80211RTSFrame*>(controlFrame))
        rtsProcedure->processTransmittedRts(rtsFrame);
    else
        throw cRuntimeError("Unknown control frame");
}

void Hcf::originatorProcessFailedFrame(Ieee80211DataOrMgmtFrame* failedFrame)
{
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        AccessCategory ac = edcaf->getAccessCategory();
        bool retryLimitReached = false;
        if (auto dataFrame = dynamic_cast<Ieee80211DataFrame *>(failedFrame)) {
            EV_INFO << "Data frame transmission failed\n";
            if (dataFrame->getAckPolicy() == NORMAL_ACK) {
                edcaDataRecoveryProcedures[ac]->dataFrameTransmissionFailed(dataFrame);
                retryLimitReached = edcaDataRecoveryProcedures[ac]->isRetryLimitReached(dataFrame);
                if (dataAndMgmtRateControl) {
                    int retryCount =  edcaDataRecoveryProcedures[ac]->getRetryCount(dataFrame);
                    dataAndMgmtRateControl->frameTransmitted(dataFrame, retryCount, false, retryLimitReached);
                }
            }
            else if (dataFrame->getAckPolicy() == BLOCK_ACK) {
                // TODO:
                // bool lifetimeExpired = lifetimeHandler->isLifetimeExpired(failedFrame);
                // if (lifetimeExpired) {
                //    inProgressFrames->dropFrame(failedFrame);
                //    delete dataFrame;
                // }
            }
            else
                throw cRuntimeError("Unimplemented!");
        }
        else if (auto mgmtFrame = dynamic_cast<Ieee80211ManagementFrame*>(failedFrame)) {
            EV_INFO << "Management frame transmission failed\n";
            edcaMgmtAndNonQoSRecoveryProcedure->dataOrMgmtFrameTransmissionFailed(mgmtFrame, stationRetryCounters[ac]);
            retryLimitReached = edcaMgmtAndNonQoSRecoveryProcedure->isRetryLimitReached(mgmtFrame);
            if (dataAndMgmtRateControl) {
                int retryCount = edcaMgmtAndNonQoSRecoveryProcedure->getRetryCount(mgmtFrame);
                dataAndMgmtRateControl->frameTransmitted(mgmtFrame, retryCount, false, retryLimitReached);
            }
        }
        else
            throw cRuntimeError("Unknown frame"); // TODO: qos, nonqos
        if (retryLimitReached) {
            edcaInProgressFrames[ac]->dropFrame(failedFrame);
            delete failedFrame;
        }
    }
    else
        throw cRuntimeError("Hcca is unimplemented!");
}

void Hcf::originatorProcessReceivedFrame(Ieee80211Frame* frame, Ieee80211Frame* lastTransmittedFrame)
{
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        AccessCategory ac = edcaf->getAccessCategory();
        if (auto dataFrame = dynamic_cast<Ieee80211DataFrame*>(frame))
            originatorProcessReceivedDataFrame(dataFrame, lastTransmittedFrame, ac);
        else if (auto mgmtFrame = dynamic_cast<Ieee80211ManagementFrame *>(frame))
            originatorProcessReceivedManagementFrame(mgmtFrame, lastTransmittedFrame, ac);
        else
            originatorProcessReceivedControlFrame(frame, lastTransmittedFrame, ac);
    }
    else
        throw cRuntimeError("Hcca is unimplemented!");
    delete frame;
}

void Hcf::originatorProcessReceivedManagementFrame(Ieee80211ManagementFrame* frame, Ieee80211Frame* lastTransmittedFrame, AccessCategory ac)
{
    throw cRuntimeError("Unknown management frame");
}

void Hcf::originatorProcessReceivedControlFrame(Ieee80211Frame* frame, Ieee80211Frame* lastTransmittedFrame, AccessCategory ac)
{
    if (auto ackFrame = dynamic_cast<Ieee80211ACKFrame *>(frame)) {
        if (auto dataFrame = dynamic_cast<Ieee80211DataFrame *>(lastTransmittedFrame)) {
            edcaDataRecoveryProcedures[ac]->ackFrameReceived(dataFrame);
            if (dataAndMgmtRateControl) {
                int retryCount;
                if (dataFrame->getRetry())
                    retryCount = edcaDataRecoveryProcedures[ac]->getRetryCount(dataFrame);
                else
                    retryCount = 0;
                dataAndMgmtRateControl->frameTransmitted(dataFrame, retryCount, true, false);
            }
        }
        else if (auto mgmtFrame = dynamic_cast<Ieee80211ManagementFrame *>(lastTransmittedFrame)) {
            edcaMgmtAndNonQoSRecoveryProcedure->ackFrameReceived(mgmtFrame, stationRetryCounters[ac]);
            if (dataAndMgmtRateControl) {
                int retryCount = edcaMgmtAndNonQoSRecoveryProcedure->getRetryCount(dataFrame);
                dataAndMgmtRateControl->frameTransmitted(mgmtFrame, retryCount, true, false);
            }
        }
        else
            throw cRuntimeError("Unknown frame"); // TODO: qos, nonqos frame
        edcaAckHandlers[ac]->processReceivedAck(ackFrame, check_and_cast<Ieee80211DataOrMgmtFrame*>(lastTransmittedFrame));
        edcaInProgressFrames[ac]->dropFrame(check_and_cast<Ieee80211DataOrMgmtFrame*>(lastTransmittedFrame));
    }
    else if (auto blockAck = dynamic_cast<Ieee80211BasicBlockAck *>(frame)) {
        EV_INFO << "BasicBlockAck has arrived" << std::endl;
        edcaDataRecoveryProcedures[ac]->blockAckFrameReceived();
        auto ackedSeqAndFragNums = edcaAckHandlers[ac]->processReceivedBlockAck(blockAck);
        if (originatorBlockAckAgreementHandler)
            originatorBlockAckAgreementHandler->processReceivedBlockAck(blockAck, this);
        EV_INFO << "It has acknowledged the following frames:" << std::endl;
        // FIXME
//        for (auto seqCtrlField : ackedSeqAndFragNums)
//            EV_INFO << "Fragment number = " << seqCtrlField.getSequenceNumber() << " Sequence number = " << (int)seqCtrlField.getFragmentNumber() << std::endl;
        edcaInProgressFrames[ac]->dropFrames(ackedSeqAndFragNums);
    }
    else if (dynamic_cast<Ieee80211RTSFrame*>(frame))
        ; // void
    else if (dynamic_cast<Ieee80211CTSFrame*>(frame))
        edcaDataRecoveryProcedures[ac]->ctsFrameReceived();
    else if (frame->getType() == ST_DATA_WITH_QOS)
        ; // void
    else if (dynamic_cast<Ieee80211BasicBlockAckReq*>(frame))
        ; // void
    else
        throw cRuntimeError("Unknown control frame");
}

void Hcf::originatorProcessReceivedDataFrame(Ieee80211DataFrame* frame, Ieee80211Frame* lastTransmittedFrame, AccessCategory ac)
{
    throw cRuntimeError("Unknown data frame");
}

bool Hcf::hasFrameToTransmit(AccessCategory ac)
{
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        AccessCategory ac = edcaf->getAccessCategory();
        return !edcaPendingQueues[ac]->isEmpty() || edcaInProgressFrames[ac]->hasInProgressFrames();
    }
    else
        throw cRuntimeError("Hcca is unimplemented");
}

bool Hcf::hasFrameToTransmit()
{
    auto edcaf = edca->getChannelOwner();
    if (edcaf) {
        AccessCategory ac = edcaf->getAccessCategory();
        return !edcaPendingQueues[ac]->isEmpty() || edcaInProgressFrames[ac]->hasInProgressFrames();
    }
    else
        throw cRuntimeError("Hcca is unimplemented");
}

void Hcf::sendUp(const std::vector<Ieee80211Frame*>& completeFrames)
{
    for (auto frame : completeFrames) {
        // FIXME: mgmt module does not handle addba req ..
        if (!dynamic_cast<Ieee80211AddbaRequest*>(frame) && !dynamic_cast<Ieee80211AddbaResponse*>(frame) && !dynamic_cast<Ieee80211Delba*>(frame))
            mac->sendUp(frame);
    }
}

void Hcf::transmitFrame(Ieee80211Frame* frame, simtime_t ifs)
{
    auto channelOwner = edca->getChannelOwner();
    if (channelOwner) {
        AccessCategory ac = channelOwner->getAccessCategory();
        auto txop = edcaTxops[ac];
        if (auto dataFrame = dynamic_cast<Ieee80211DataFrame*>(frame)) {
            OriginatorBlockAckAgreement *agreement = nullptr;
            if (originatorBlockAckAgreementHandler)
                agreement = originatorBlockAckAgreementHandler->getAgreement(dataFrame->getReceiverAddress(), dataFrame->getTid());
            auto ackPolicy = originatorAckPolicy->computeAckPolicy(dataFrame, agreement);
            dataFrame->setAckPolicy(ackPolicy);
        }
        setFrameMode(frame, rateSelection->computeMode(frame, txop));
        if (txop->getProtectionMechanism() == TxopProcedure::ProtectionMechanism::SINGLE_PROTECTION)
            frame->setDuration(singleProtectionMechanism->computeDurationField(frame, edcaInProgressFrames[ac]->getPendingFrameFor(frame), edcaTxops[ac], recipientAckPolicy));
        else if (txop->getProtectionMechanism() == TxopProcedure::ProtectionMechanism::MULTIPLE_PROTECTION)
            throw cRuntimeError("Double protection is unsupported");
        else
            throw cRuntimeError("Undefined protection mechanism");
        tx->transmitFrame(frame, ifs, this);
    }
    else
        throw cRuntimeError("Hcca is unimplemented");
}

void Hcf::transmitControlResponseFrame(Ieee80211Frame* responseFrame, Ieee80211Frame* receivedFrame)
{
    const IIeee80211Mode *responseMode = nullptr;
    if (auto rtsFrame = dynamic_cast<Ieee80211RTSFrame*>(receivedFrame))
        responseMode = rateSelection->computeResponseCtsFrameMode(rtsFrame);
    else if (auto blockAckReq = dynamic_cast<Ieee80211BasicBlockAckReq*>(receivedFrame))
        responseMode = rateSelection->computeResponseBlockAckFrameMode(blockAckReq);
    else if (auto dataOrMgmtFrame = dynamic_cast<Ieee80211DataOrMgmtFrame*>(receivedFrame))
        responseMode = rateSelection->computeResponseAckFrameMode(dataOrMgmtFrame);
    else
        throw cRuntimeError("Unknown received frame type");
    setFrameMode(responseFrame, responseMode);
    tx->transmitFrame(responseFrame, modeSet->getSifsTime(), this);
    delete responseFrame;
}

void Hcf::recipientProcessTransmittedControlResponseFrame(Ieee80211Frame* frame)
{
    if (auto ctsFrame = dynamic_cast<Ieee80211CTSFrame*>(frame))
        ctsProcedure->processTransmittedCts(ctsFrame);
    else if (auto blockAck = dynamic_cast<Ieee80211BlockAck*>(frame)) {
        if (recipientBlockAckProcedure)
            recipientBlockAckProcedure->processTransmittedBlockAck(blockAck);
    }
    else if (auto ackFrame = dynamic_cast<Ieee80211ACKFrame*>(frame))
        recipientAckProcedure->processTransmittedAck(ackFrame);
    else
        throw cRuntimeError("Unknown control response frame");
}


void Hcf::processMgmtFrame(Ieee80211ManagementFrame* mgmtFrame)
{
    processUpperFrame(mgmtFrame);
}

void Hcf::setFrameMode(Ieee80211Frame *frame, const IIeee80211Mode *mode) const
 {
    ASSERT(mode != nullptr);
    delete frame->removeControlInfo();
    Ieee80211TransmissionRequest *ctrl = new Ieee80211TransmissionRequest();
    ctrl->setMode(mode);
    frame->setControlInfo(ctrl);
}


bool Hcf::isReceptionInProgress()
{
    return rx->isReceptionInProgress();
}

Hcf::~Hcf()
{
    cancelAndDelete(startRxTimer);
    cancelAndDelete(inactivityTimer);
    delete recipientAckProcedure;
    delete ctsProcedure;
    delete rtsProcedure;
    delete originatorBlockAckAgreementHandler;
    delete recipientBlockAckAgreementHandler;
    delete originatorBlockAckProcedure;
    delete recipientBlockAckProcedure;
    delete frameSequenceHandler;
    for (auto inProgressFrames : edcaInProgressFrames)
        delete inProgressFrames;
    for (auto pendingQueue : edcaPendingQueues)
        delete pendingQueue;
    for (auto ackHandler : edcaAckHandlers)
        delete ackHandler;
    for (auto retryCounter : stationRetryCounters)
        delete retryCounter;
}

} // namespace ieee80211
} // namespace inet
