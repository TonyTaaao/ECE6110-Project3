#include "p2pCampusHelper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-nix-vector-helper.h"
#include <iostream>
#include <string>

using namespace ns3;

PointToPointCampusHelper::PointToPointCampusHelper(uint32_t maxInner, uint32_t maxOuter, PointToPointHelper inner,
                                                   PointToPointHelper outer, Ptr<UniformRandomVariable> rnd, uint32_t hubNum,
                                                   uint32_t simInst)
 {
	uint32_t i,j;
  NodeContainer nc[8];
  hub = CreateObject<Node>(simInst);
  dummyNode =  CreateObject<Node>(simInst);
  allNodes.Add(hub);
  infected[hub] = true;
  allNodes.Add(dummyNode);
  infected[dummyNode] = true;
  /*This function creates nodes based on the probailities defined*/
	for (i=0;i<maxInner;i++)
  {
    if (rnd->GetValue() <= INNER_NODE_PROBABILITY)
    {
      //std::cout << "i " << i << std::endl;
      Ptr <Node> temp = CreateObject<Node> (simInst);
      innerNodes.Add(temp);
      allNodes.Add(temp);
      infected[temp] = true;
      for (j = 0;j < maxOuter;j++)
      {
        if (rnd->GetValue() <= OUTER_NODE_PROBABILITY)
        {
         //std::cout << "j " << j << std::endl;
         Ptr <Node> itemp = CreateObject<Node> (simInst);
         nc[i].Add(itemp);
         allNodes.Add(itemp);
         infected[itemp] = false; //was temp changed to itemp <-Chris
        }
        else
        {
          nc[i].Add(dummyNode);
        }
      }
      innerAvailable.push_back(true);
    }
    else
    {
      nc[i].Add(dummyNode);
      nc[i].Add(dummyNode);
      innerNodes.Add(dummyNode);
      outerNodes.push_back(nc[i]);
      innerAvailable.push_back(false);
    }
    outerNodes.push_back(nc[i]);
  }

  /*Following piece of code makes links between nodes*/
  outerDevices.resize(innerNodes.GetN());
  innerDevices.resize(innerNodes.GetN());
  NodeContainer nodeContainer;
  for (i=0;i<innerNodes.GetN();i++)
  {
    nodeContainer = NodeContainer (innerNodes.Get(i), hub);
    innerDevices[i].Add(inner.Install(nodeContainer));
    outerDevices[i].resize(outerNodes[i].GetN());
    for (j=0;j<outerNodes[i].GetN();j++)
    {
      if (innerAvailable[i] == true)
        nodeContainer = NodeContainer (outerNodes[i].Get(j),innerNodes.Get(i));
      else
        nodeContainer = NodeContainer (outerNodes[i].Get(j),hub);
      outerDevices[i][j].Add(outer.Install(nodeContainer));
    }
  }

  /*Installing nixVector stack on all nodes*/
  Ipv4NixVectorHelper nixRouting;
  InternetStackHelper stack;
  stack.SetRoutingHelper (nixRouting);
  stack.Install (allNodes);


  Ipv4AddressHelper address;
  std::string ip_base = "";
  std::string ip = "";
  std::cout << innerDevices.size() << " " << outerDevices[0].size() << std::endl;

  for(i=0;i<innerDevices.size();i++)
  {
    ip_base.append(std::to_string(hubNum+1));
    ip_base.append(".");
    ip_base.append(std::to_string(i+2));
    ip_base.append(".");
    for (j=0;j<outerDevices[i].size();j++)
    {
      ip = ip_base + std::to_string(j+2) + ".0";
      address.SetBase (ip.c_str(), "255.255.255.0");
      ipContainer.Add(address.Assign (outerDevices[i][j]));
      ip = "";
    }
    ip = ip_base + std::to_string(1) + ".0";
    address.SetBase (ip.c_str(), "255.255.255.0");
    ipContainer.Add(address.Assign (innerDevices[i]));
    ip = "";
    ip_base = "";
  }
/*
 for (i=0;i<allNodes.GetN();i++)
  {
    Ptr<Node> tmpPtr = allNodes.Get(i);
    for (j=1;j<tmpPtr->GetNDevices();j++)
    {
      std::cout << i << " " << j ;
      Ptr<Ipv4> ipv4 = tmpPtr->GetObject<Ipv4> ();
      Ipv4InterfaceAddress iaddr = ipv4->GetAddress (j,0);
      Ipv4Address ipAddr = iaddr.GetLocal ();
      ipAddr.Print(std::cout);
      std::cout << std::endl;
    }
  }*/
}

PointToPointCampusHelper::~PointToPointCampusHelper() {

}

