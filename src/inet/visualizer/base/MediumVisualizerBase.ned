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

package inet.visualizer.base;

//
// This is a base module for radio medium visualizer simple modules. It keeps
// track of ongoing transmissions, ongoing receptions, and propagating signals.
// It subscribes for notifications at the module determined by the mediumModule
// parameter. The actual visualization of the radio medium is done in derived
// modules.
//
// @see ~MediumCanvasVisualizer, ~MediumOsgVisualizer, ~IMediumVisualizer, ~MediumVisualizerBase
//
simple MediumVisualizerBase extends VisualizerBase
{
    parameters:
        string mediumModule = default("radioMedium"); // module path of the medium module that is displayed, refers to a top level submodule by default
        string networkNodeVisualizerModule = default("^.networkNodeVisualizer"); // module path of the network node visualizer module

        bool displaySignals = default(false); // display signals propagating through the medium, disabled by default
        double signalPropagationAnimationSpeed = default(0); // animation speed while the very beginning or end of a signal is propagating on the medium, value must be in the range (0, +inf)
        double signalTransmissionAnimationSpeed = default(0); // animation speed while a signal is being transmitted on the medium, value must be in the range (0, +inf)

        bool displayTransmissions = default(false); // display an image where a transmission is currently in progress, disabled by default
        string transmissionImage = default("misc/transmission"); // transmission image for ongoing transmissions

        bool displayReceptions = default(false); // display an image where a reception is currently in progress, disabled by default
        string receptionImage = default("misc/reception"); // reception image for ongoing receptions

        bool displayCommunicationRanges = default(false); // display communication ranges as circles around radios, enabled by default
        string communicationRangeColor = default("blue"); // communication range circle color, blue by default

        bool displayInterferenceRanges = default(false); // display interference ranges as circles around radios, disabled by default
        string interferenceRangeColor = default("gray"); // interference range circle color, gray by default

        @class(MediumVisualizerBase);
}
