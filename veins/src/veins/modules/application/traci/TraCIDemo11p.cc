//
// Copyright (C) 2006-2011 Christoph Sommer <christoph.sommer@uibk.ac.at>
//
// Documentation for these modules is at http://veins.car2x.org/
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "veins/modules/application/traci/TraCIDemo11p.h"

#include "veins/modules/application/traci/TraCIDemo11pMessage_m.h"

using namespace veins;

Define_Module(veins::TraCIDemo11p);

void TraCIDemo11p::initialize(int stage)
{
    DemoBaseApplLayer::initialize(stage);
    if (stage == 0) {
        sentMessage = false;
        lastDroveAt = simTime();
        currentSubscribedServiceId = -1;
        Route_updated = false;
    }
}

void TraCIDemo11p::onWSA(DemoServiceAdvertisment* wsa)
{
    if (currentSubscribedServiceId == -1) {
        mac->changeServiceChannel(static_cast<Channel>(wsa->getTargetChannel()));
        currentSubscribedServiceId = wsa->getPsid();
        if (currentOfferedServiceId != wsa->getPsid()) {
            stopService();
            startService(static_cast<Channel>(wsa->getTargetChannel()), wsa->getPsid(), "Mirrored Traffic Service");
        }
    }
}

void TraCIDemo11p::onWSM(BaseFrame1609_4* frame)
{

    if (findHost()->getIndex() != 0 ){  //accident node don't forward msg

        TraCIDemo11pMessage* wsm = check_and_cast<TraCIDemo11pMessage*>(frame);
        findHost()->getDisplayString().setTagArg("i", 1, "green");

        //  ENABLE/DISABLE vehicles rerouting during simulation. See omnetpp.ini

        if (reroute && !Route_updated){
            std::list<std::string> route_before = traciVehicle->getPlannedRoadIds();
            //for (std::list<std::string>::iterator i = route.begin(); i != route.end(); ++i) {std::cout << ' ' << *i;}
            if (mobility->getRoadId()[0] != ':') traciVehicle->changeRoute(wsm->getDemoData(), 9999);
            std::list<std::string> route_after = traciVehicle->getPlannedRoadIds();
            if (route_before.size() != route_after.size()){Route_updated = true;}
        }
        // Metrics capturing //
        DemoBaseApplLayer::controlMessage(wsm, "rx", simTime().dbl(), Route_updated);

        if (!sentMessage) {
            sentMessage = true;
            // repeat the received traffic update once in 2 seconds plus some random delay
            // se puede probar variando los 2 seconds a ver como reacciona
            wsm->setSenderAddress(myId);
            wsm->setSerial(1);  //set serial to 3 para que contar msg enviados
            simtime_t s_time = simTime() + 2 + uniform(0.01, 0.2);
            scheduleAt(s_time, wsm->dup());
        }
    }
}

void TraCIDemo11p::handleSelfMsg(cMessage* msg)
{
    if (TraCIDemo11pMessage* wsm = dynamic_cast<TraCIDemo11pMessage*>(msg)) {
        // send this message on the service channel until the counter is 3 or higher.
        // this code only runs when channel switching is enabled

       sendDown(wsm->dup());
       // Metrics capturing //
       DemoBaseApplLayer::controlMessage(wsm, "tx", simTime().dbl(), Route_updated);        // send accident msg parameters

       //if (findHost()->getIndex() == 0){wsm->setSerial(1);}
       //else{wsm->setSerial(wsm->getSerial() + 1);}

       wsm->setSerial(wsm->getSerial() + 1);

       if (wsm->getSerial() > 1) { // send just 1 msg
            // stop service advertisements
            stopService();
            delete (wsm);
        }
        // If more than one msg is required enable:
        //else {
        //    simtime_t send_msg_time = simTime() + InterTrafficMessage;
        //    scheduleAt(send_msg_time, wsm);
        //}
    }
    else {
        DemoBaseApplLayer::handleSelfMsg(msg);
    }
}

void TraCIDemo11p::handlePositionUpdate(cObject* obj)     // Update nodes position in map -> SUMO connection
{
    DemoBaseApplLayer::handlePositionUpdate(obj);
    if (TrafficService){            // Traffic Information Service enable/disable
        if (simTime() >= AccidentStart && findHost()->getIndex() == 0){
            if (mobility->getSpeed() <= 1 && !sentMessage) {  // Accident node[0]. See omnetpp.ini
                findHost()->getDisplayString().setTagArg("i", 1, "red");
                sentMessage = true;

                TraCIDemo11pMessage* wsm = new TraCIDemo11pMessage();
                populateWSM(wsm);
                wsm->setDemoData(mobility->getRoadId().c_str());

                // host is standing still due to crash

                if (dataOnSch) {
                    startService(Channel::sch2, 42, "Traffic Information Service");
                    // started service and server advertising, schedule message to self to send later
                    simtime_t s_time = computeAsynchronousSendingTime(1, ChannelType::service);
                    scheduleAt(s_time, wsm);
                }else{
                    // send right away on CCH, because channel switching is disabled
                    sendDown(wsm);

                }
            }
        }
    }
}
