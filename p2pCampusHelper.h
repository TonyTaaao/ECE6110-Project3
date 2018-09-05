#pragma once
#include "ns3/core-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

#define INNER_NODE_PROBABILITY	0.8
#define OUTER_NODE_PROBABILITY	0.6

class PointToPointCampusHelper {
	/*TODO :: infected boolean */
public:
	std::map<Ptr<Node> , bool> infected;
	std::vector<bool> innerAvailable;
	Ipv4InterfaceContainer ipContainer;
	std::vector <NetDeviceContainer> innerDevices;
	std::vector <std::vector <NetDeviceContainer> >outerDevices;
	/*Contains all the innerNodes including the hub*/
	NodeContainer innerNodes;
	/*Node container for each innerNode created*/
	std::vector<NodeContainer> outerNodes;
	/*Hub node*/
	Ptr <Node> hub;
	/*All nodes that are supposed to be there but are not will be connected to this*/
	Ptr <Node> dummyNode;
	NodeContainer allNodes;

	PointToPointCampusHelper(uint32_t maxInner, uint32_t maxOuter, PointToPointHelper inner, PointToPointHelper outer,
	 Ptr<UniformRandomVariable> rnd,uint32_t hubNum,uint32_t simInst);

	~PointToPointCampusHelper();
};
