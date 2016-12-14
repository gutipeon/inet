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

#include <algorithm>
#include "inet/common/ModuleAccess.h"
#include "inet/visualizer/base/TracingObstacleLossVisualizerBase.h"

namespace inet {

namespace visualizer {

using namespace inet::physicallayer;

void TracingObstacleLossVisualizerBase::initialize(int stage)
{
    VisualizerBase::initialize(stage);
    if (!hasGUI()) return;
    if (stage == INITSTAGE_LOCAL) {
        subscriptionModule = *par("subscriptionModule").stringValue() == '\0' ? getSystemModule() : getModuleFromPar<cModule>(par("subscriptionModule"), this);
        subscriptionModule->subscribe(ITracingObstacleLoss::obstaclePenetratedSignal, this);
        displayIntersection = par("displayIntersection");
        displayFaceNormalVector = par("displayFaceNormalVector");
        intersectionLineColor = cFigure::parseColor(par("intersectionLineColor"));
        intersectionLineStyle = cFigure::parseLineStyle(par("intersectionLineStyle"));
        intersectionLineWidth = par("intersectionLineWidth");
        faceNormalLineColor = cFigure::parseColor(par("faceNormalLineColor"));
        faceNormalLineStyle = cFigure::parseLineStyle(par("faceNormalLineStyle"));
        faceNormalLineWidth = par("faceNormalLineWidth");
        fadeOutMode = par("fadeOutMode");
        fadeOutHalfLife = par("fadeOutHalfLife");
    }
}

void TracingObstacleLossVisualizerBase::refreshDisplay() const
{
    AnimationPosition currentAnimationPosition;
    std::vector<const ObstacleLossVisualization *> removedObstacleLossVisualizations;
    for (auto obstacleLossVisualization : obstacleLossVisualizations) {
        double delta;
        if (!strcmp(fadeOutMode, "simulationTime"))
            delta = (currentAnimationPosition.getSimulationTime() - obstacleLossVisualization->obstacleLossAnimationPosition.getSimulationTime()).dbl();
        else if (!strcmp(fadeOutMode, "animationTime"))
            delta = currentAnimationPosition.getAnimationTime() - obstacleLossVisualization->obstacleLossAnimationPosition.getAnimationTime();
        else if (!strcmp(fadeOutMode, "realTime"))
            delta = currentAnimationPosition.getRealTime() - obstacleLossVisualization->obstacleLossAnimationPosition.getRealTime();
        else
            throw cRuntimeError("Unknown fadeOutMode: %s", fadeOutMode);
        auto alpha = std::min(1.0, std::pow(2.0, -delta / fadeOutHalfLife));
        if (alpha < 0.01)
            removedObstacleLossVisualizations.push_back(obstacleLossVisualization);
        else
            setAlpha(obstacleLossVisualization, alpha);
    }
    for (auto obstacleLossVisualization : removedObstacleLossVisualizations) {
        const_cast<TracingObstacleLossVisualizerBase *>(this)->removeObstacleLossVisualization(obstacleLossVisualization);
        delete obstacleLossVisualization;
    }
}

void TracingObstacleLossVisualizerBase::receiveSignal(cComponent *source, simsignal_t signal, cObject *object, cObject *details)
{
    Enter_Method_Silent();
    if (signal == ITracingObstacleLoss::obstaclePenetratedSignal) {
        if (displayIntersection || displayFaceNormalVector) {
            auto event = static_cast<ITracingObstacleLoss::ObstaclePenetratedEvent *>(object);
            auto obstacleLossVisualization = createObstacleLossVisualization(event->object, event->intersection1, event->intersection2, event->normal1, event->normal2);
            addObstacleLossVisualization(obstacleLossVisualization);
        }
    }
    else
        throw cRuntimeError("Unknown signal");
}

void TracingObstacleLossVisualizerBase::addObstacleLossVisualization(const ObstacleLossVisualization *obstacleLossVisualization)
{
    obstacleLossVisualizations.push_back(obstacleLossVisualization);
}

void TracingObstacleLossVisualizerBase::removeObstacleLossVisualization(const ObstacleLossVisualization *obstacleLossVisualization)
{
    obstacleLossVisualizations.erase(std::remove(obstacleLossVisualizations.begin(), obstacleLossVisualizations.end(), obstacleLossVisualization), obstacleLossVisualizations.end());
}

} // namespace visualizer

}// namespace inet
