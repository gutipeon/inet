//
// Copyright (C) 2015 Andras Varga
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

#include "Tx.h"
#include "ContentionTx.h"
#include "ImmediateTx.h"
#include "IUpperMac.h"
#include "IMacRadioInterface.h"

namespace inet {
namespace ieee80211 {

Define_Module(Tx);

Tx::Tx()
{
    for (int i = 0; i < MAX_NUM_CONTENTIONTX; i++)
        contentionTx[i] = nullptr;
}

Tx::~Tx()
{
    for (int i = 0; i < MAX_NUM_CONTENTIONTX; i++)
        delete contentionTx[i];
    delete immediateTx;
}

void Tx::initialize()
{
    IMacRadioInterface *mac = check_and_cast<IMacRadioInterface *>(getParentModule());  //TODO
    IUpperMac *upperMac = check_and_cast<IUpperMac *>(getModuleByPath("^.upperMac"));  //TODO

    numContentionTx = 4; //TODO
    ASSERT(numContentionTx <= MAX_NUM_CONTENTIONTX);
    for (int i = 0; i < numContentionTx; i++)
        contentionTx[i] = new ContentionTx(this, mac, upperMac, i); //TODO factory method
    immediateTx = new ImmediateTx(this, mac, upperMac); //TODO factory method

}

void Tx::handleMessage(cMessage *msg)
{
    if (msg->getContextPointer() != nullptr)
        ((MacPlugin *)msg->getContextPointer())->handleMessage(msg);
    else
        ASSERT(false);
}

void Tx::transmitContentionFrame(int txIndex, Ieee80211Frame *frame, simtime_t ifs, simtime_t eifs, int cwMin, int cwMax, simtime_t slotTime, int retryCount, ICallback *completionCallback)
{
    Enter_Method("transmitContentionFrame()");
    ASSERT(txIndex >= 0 && txIndex < numContentionTx);
    take(frame);
    contentionTx[txIndex]->transmitContentionFrame(frame, ifs, eifs, cwMin, cwMax, slotTime, retryCount, completionCallback);
}

void Tx::transmitImmediateFrame(Ieee80211Frame *frame, simtime_t ifs, ICallback *completionCallback)
{
    Enter_Method("transmitImmediateFrame()");
    take(frame);
    immediateTx->transmitImmediateFrame(frame, ifs, completionCallback);
}

void Tx::mediumStateChanged(bool mediumFree)
{
    Enter_Method("mediumState(%s)", mediumFree ? "FREE" : "BUSY");
    for (int i = 0; i < numContentionTx; i++)
        contentionTx[i]->mediumStateChanged(mediumFree);
}

void Tx::radioTransmissionFinished()
{
    Enter_Method("radioTransmissionFinished()");
    immediateTx->radioTransmissionFinished();
    for (int i = 0; i < numContentionTx; i++)
        contentionTx[i]->radioTransmissionFinished();
}

void Tx::lowerFrameReceived(bool isFcsOk)
{
    Enter_Method("lowerFrameReceived(%s)", isFcsOk ? "OK" : "CORRUPT");
    for (int i = 0; i < numContentionTx; i++)
        contentionTx[i]->lowerFrameReceived(isFcsOk);
}

} // namespace ieee80211
} // namespace inet
