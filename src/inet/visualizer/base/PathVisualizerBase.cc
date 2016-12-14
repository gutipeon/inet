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

#include "inet/common/LayeredProtocolBase.h"
#include "inet/common/ModuleAccess.h"
#include "inet/mobility/contract/IMobility.h"
#include "inet/visualizer/base/PathVisualizerBase.h"

namespace inet {

namespace visualizer {

PathVisualizerBase::PathVisualization::PathVisualization(const std::vector<int>& path) :
    ModulePath(path)
{
}

PathVisualizerBase::~PathVisualizerBase()
{
    // NOTE: lookup the module again because it may have been deleted first
    subscriptionModule = getModuleFromPar<cModule>(par("subscriptionModule"), this, false);
    if (subscriptionModule != nullptr) {
        subscriptionModule->unsubscribe(LayeredProtocolBase::packetSentToUpperSignal, this);
        subscriptionModule->unsubscribe(LayeredProtocolBase::packetReceivedFromUpperSignal, this);
        subscriptionModule->unsubscribe(LayeredProtocolBase::packetReceivedFromLowerSignal, this);
    }
}

void PathVisualizerBase::initialize(int stage)
{
    VisualizerBase::initialize(stage);
    if (!hasGUI()) return;
    if (stage == INITSTAGE_LOCAL) {
        subscriptionModule = getModuleFromPar<cModule>(par("subscriptionModule"), this);
        subscriptionModule->subscribe(LayeredProtocolBase::packetSentToUpperSignal, this);
        subscriptionModule->subscribe(LayeredProtocolBase::packetReceivedFromUpperSignal, this);
        subscriptionModule->subscribe(LayeredProtocolBase::packetReceivedFromLowerSignal, this);
        packetNameMatcher.setPattern(par("packetNameFilter"), false, true, true);
        if (strcmp(par("lineColor"), "auto"))
            lineColor = cFigure::Color(par("lineColor"));
        lineStyle = cFigure::parseLineStyle(par("lineStyle"));
        lineWidth = par("lineWidth");
        lineShift = par("lineShift");
        lineShiftMode = par("lineShiftMode");
        lineContactSpacing = par("lineContactSpacing");
        lineContactMode = par("lineContactMode");
        fadeOutMode = par("fadeOutMode");
        fadeOutHalfLife = par("fadeOutHalfLife");
        lineManager = LineManager::getLineManager(visualizerTargetModule->getCanvas());
    }
}

void PathVisualizerBase::refreshDisplay() const
{
    AnimationPosition currentAnimationPosition;
    std::vector<const PathVisualization *> removedPathVisualizations;
    for (auto it : pathVisualizations) {
        auto pathVisualization = it.second;
        double delta;
        if (!strcmp(fadeOutMode, "simulationTime"))
            delta = (currentAnimationPosition.getSimulationTime() - pathVisualization->lastUsageAnimationPosition.getSimulationTime()).dbl();
        else if (!strcmp(fadeOutMode, "animationTime"))
            delta = currentAnimationPosition.getAnimationTime() - pathVisualization->lastUsageAnimationPosition.getAnimationTime();
        else if (!strcmp(fadeOutMode, "realTime"))
            delta = currentAnimationPosition.getRealTime() - pathVisualization->lastUsageAnimationPosition.getRealTime();
        else
            throw cRuntimeError("Unknown fadeOutMode: %s", fadeOutMode);
        auto alpha = std::min(1.0, std::pow(2.0, -delta / fadeOutHalfLife));
        if (alpha < 0.01)
            removedPathVisualizations.push_back(pathVisualization);
        else
            setAlpha(pathVisualization, alpha);
    }
    for (auto path : removedPathVisualizations) {
        auto sourceAndDestination = std::pair<int, int>(path->moduleIds.front(), path->moduleIds.back());
        const_cast<PathVisualizerBase *>(this)->removePathVisualization(sourceAndDestination, path);
        delete path;
    }
}

const PathVisualizerBase::PathVisualization *PathVisualizerBase::createPathVisualization(const std::vector<int>& path) const
{
    return new PathVisualization(path);
}

const PathVisualizerBase::PathVisualization *PathVisualizerBase::getPathVisualization(std::pair<int, int> sourceAndDestination, const std::vector<int>& path)
{
    auto range = pathVisualizations.equal_range(sourceAndDestination);
    for (auto it = range.first; it != range.second; it++)
        if (it->second->moduleIds == path)
            return it->second;
    return nullptr;
}

void PathVisualizerBase::addPathVisualization(std::pair<int, int> sourceAndDestination, const PathVisualization *pathVisualization)
{
    pathVisualizations.insert(std::pair<std::pair<int, int>, const PathVisualization *>(sourceAndDestination, pathVisualization));
}

void PathVisualizerBase::removePathVisualization(std::pair<int, int> sourceAndDestination, const PathVisualization *pathVisualization)
{
    auto range = pathVisualizations.equal_range(sourceAndDestination);
    for (auto it = range.first; it != range.second; it++) {
        if (it->second == pathVisualization) {
            pathVisualizations.erase(it);
            break;
        }
    }
}

const std::vector<int> *PathVisualizerBase::getIncompletePath(int treeId)
{
    auto it = incompletePaths.find(treeId);
    if (it == incompletePaths.end())
        return nullptr;
    else
        return &it->second;
}

void PathVisualizerBase::addToIncompletePath(int treeId, cModule *module)
{
    incompletePaths[treeId].push_back(module->getId());
}

void PathVisualizerBase::removeIncompletePath(int treeId)
{
    incompletePaths.erase(incompletePaths.find(treeId));
}

void PathVisualizerBase::updatePath(const std::vector<int>& moduleIds)
{
    auto key = std::pair<int, int>(moduleIds.front(), moduleIds.back());
    const PathVisualization *path = getPathVisualization(key, moduleIds);
    if (path == nullptr) {
        path = createPathVisualization(moduleIds);
        addPathVisualization(key, path);
    }
    else {
        path->lastUsageAnimationPosition = AnimationPosition();
    }
}

void PathVisualizerBase::receiveSignal(cComponent *source, simsignal_t signal, cObject *object, cObject *details)
{
    if (signal == LayeredProtocolBase::packetReceivedFromUpperSignal) {
        if (isPathEnd(static_cast<cModule *>(source))) {
            auto packet = check_and_cast<cPacket *>(object);
            if (packetNameMatcher.matches(packet->getFullName())) {
                auto treeId = packet->getTreeId();
                auto module = check_and_cast<cModule *>(source);
                addToIncompletePath(treeId, getContainingNode(module));
            }
        }
    }
    else if (signal == LayeredProtocolBase::packetReceivedFromLowerSignal) {
        if (isPathEnd(static_cast<cModule *>(source))) {
            auto packet = check_and_cast<cPacket *>(object);
            if (packetNameMatcher.matches(packet->getFullName())) {
                auto treeId = packet->getEncapsulatedPacket()->getTreeId();
                auto module = check_and_cast<cModule *>(source);
                addToIncompletePath(treeId, getContainingNode(module));
            }
        }
        else if (isPathElement(static_cast<cModule *>(source))) {
            auto packet = check_and_cast<cPacket *>(object);
            if (packetNameMatcher.matches(packet->getFullName())) {
                auto encapsulatedPacket = packet->getEncapsulatedPacket()->getEncapsulatedPacket();
                if (encapsulatedPacket != nullptr) {
                    auto treeId = encapsulatedPacket->getTreeId();
                    auto module = check_and_cast<cModule *>(source);
                    addToIncompletePath(treeId, getContainingNode(module));
                }
            }
        }
    }
    else if (signal == LayeredProtocolBase::packetSentToUpperSignal) {
        if (isPathEnd(static_cast<cModule *>(source))) {
            auto packet = check_and_cast<cPacket *>(object);
            if (packetNameMatcher.matches(packet->getFullName())) {
                auto treeId = packet->getTreeId();
                auto path = getIncompletePath(treeId);
                updatePath(*path);
                removeIncompletePath(treeId);
            }
        }
    }
    else
        throw cRuntimeError("Unknown signal");
}

} // namespace visualizer

} // namespace inet

