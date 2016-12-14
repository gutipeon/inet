//
// Copyright (C) OpenSim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "inet/common/ModuleAccess.h"
#include "inet/visualizer/base/MediumVisualizerBase.h"

namespace inet {

namespace visualizer {

MediumVisualizerBase::~MediumVisualizerBase()
{
    // NOTE: lookup the medium module again because it may have been deleted first
    auto radioMediumModule = getModuleFromPar<cModule>(par("mediumModule"), this, false);
    if (radioMediumModule != nullptr) {
        radioMediumModule->unsubscribe(IRadioMedium::radioAddedSignal, this);
        radioMediumModule->unsubscribe(IRadioMedium::radioRemovedSignal, this);
        radioMediumModule->unsubscribe(IRadioMedium::transmissionAddedSignal, this);
        radioMediumModule->unsubscribe(IRadioMedium::transmissionRemovedSignal, this);
        radioMediumModule->unsubscribe(IRadioMedium::transmissionStartedSignal, this);
        radioMediumModule->unsubscribe(IRadioMedium::transmissionEndedSignal, this);
        radioMediumModule->unsubscribe(IRadioMedium::receptionStartedSignal, this);
        radioMediumModule->unsubscribe(IRadioMedium::receptionEndedSignal, this);
    }
}

void MediumVisualizerBase::initialize(int stage)
{
    VisualizerBase::initialize(stage);
    if (!hasGUI()) return;
    if (stage == INITSTAGE_LOCAL) {
        displaySignals = par("displaySignals");
        signalPropagationAnimationSpeed = par("signalPropagationAnimationSpeed");
        signalTransmissionAnimationSpeed = par("signalTransmissionAnimationSpeed");
        displayTransmissions = par("displayTransmissions");
        displayReceptions = par("displayReceptions");
        displayInterferenceRanges = par("displayInterferenceRanges");
        interferenceRangeColor = cFigure::Color(par("interferenceRangeColor"));
        displayCommunicationRanges = par("displayCommunicationRanges");
        communicationRangeColor = cFigure::Color(par("communicationRangeColor"));
        signalPropagationAnimationSpeed = par("signalPropagationAnimationSpeed");
        signalTransmissionAnimationSpeed = par("signalTransmissionAnimationSpeed");
        radioMedium = getModuleFromPar<IRadioMedium>(par("mediumModule"), this, false);
        if (radioMedium != nullptr) {
            cModule *radioMediumModule = check_and_cast<cModule *>(radioMedium);
            radioMediumModule->subscribe(IRadioMedium::radioAddedSignal, this);
            radioMediumModule->subscribe(IRadioMedium::radioRemovedSignal, this);
            radioMediumModule->subscribe(IRadioMedium::transmissionAddedSignal, this);
            radioMediumModule->subscribe(IRadioMedium::transmissionRemovedSignal, this);
            radioMediumModule->subscribe(IRadioMedium::transmissionStartedSignal, this);
            radioMediumModule->subscribe(IRadioMedium::transmissionEndedSignal, this);
            radioMediumModule->subscribe(IRadioMedium::receptionStartedSignal, this);
            radioMediumModule->subscribe(IRadioMedium::receptionEndedSignal, this);
        }
    }
}

void MediumVisualizerBase::handleParameterChange(const char *name)
{
    if (name != nullptr) {
        if (!strcmp(name, "signalPropagationAnimationSpeed"))
            signalPropagationAnimationSpeed = par("signalPropagationAnimationSpeed");
        else if (!strcmp(name, "signalTransmissionAnimationSpeed"))
            signalTransmissionAnimationSpeed = par("signalTransmissionAnimationSpeed");
    }
}

void MediumVisualizerBase::receiveSignal(cComponent *source, simsignal_t signal, cObject *object, cObject *details)
{
    if (signal == IRadioMedium::radioAddedSignal)
        radioAdded(check_and_cast<IRadio *>(object));
    else if (signal == IRadioMedium::radioRemovedSignal)
        radioRemoved(check_and_cast<IRadio *>(object));
    else if (signal == IRadioMedium::transmissionAddedSignal)
        transmissionAdded(check_and_cast<ITransmission *>(object));
    else if (signal == IRadioMedium::transmissionRemovedSignal)
        transmissionRemoved(check_and_cast<ITransmission *>(object));
    else if (signal == IRadioMedium::transmissionStartedSignal)
        transmissionStarted(check_and_cast<ITransmission *>(object));
    else if (signal == IRadioMedium::transmissionEndedSignal)
        transmissionEnded(check_and_cast<ITransmission *>(object));
    else if (signal == IRadioMedium::receptionStartedSignal)
        receptionStarted(check_and_cast<IReception *>(object));
    else if (signal == IRadioMedium::receptionEndedSignal)
        receptionEnded(check_and_cast<IReception *>(object));
    else
        throw cRuntimeError("Unknown signal");
}

bool MediumVisualizerBase::isSignalPropagationInProgress(const ITransmission *transmission) const
{
    simtime_t now = simTime();
    ICommunicationCache *communicationCache = const_cast<ICommunicationCache *>(radioMedium->getCommunicationCache());
    const IMediumLimitCache *mediumLimitCache = radioMedium->getMediumLimitCache();
    const simtime_t transmissionStartTime = transmission->getStartTime();
    const simtime_t transmissionEndTime = transmission->getEndTime();
    const simtime_t interferenceEndTime = communicationCache->getCachedInterferenceEndTime(transmission);
    simtime_t maxPropagationTime = interferenceEndTime - transmissionEndTime - mediumLimitCache->getMaxTransmissionDuration();
    return (transmissionStartTime <= now && now < transmissionStartTime + maxPropagationTime) ||
           (transmissionEndTime <= now && now < transmissionEndTime + maxPropagationTime);
}

bool MediumVisualizerBase::isSignalTransmissionInProgress(const ITransmission *transmission) const
{
    simtime_t now = simTime();
    return transmission->getStartTime() <= now && now < transmission->getEndTime();
}

} // namespace visualizer

} // namespace inet

