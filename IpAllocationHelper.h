/*Allocation of /16 blocks*/
#ifndef IPALLOCHELPER_H
#define IPALLOCHELPER_H

#include <vector>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include "ns3/core-module.h"

using namespace ns3;
 

class NodeIPAlloc
{
  private:
    static NodeIPAlloc * __instance;
    uint16_t host;
    NodeIPAlloc()
    {
      host = 256;
    }
  public:
    static NodeIPAlloc * getInstance()
    {
      if (__instance == NULL)
      {
        __instance = new NodeIPAlloc();
      }
      return __instance;
    }

    uint16_t getHostBlock()
    {
      host += 1;
      return host-1;
    }
};

class IPBlock
{
private:
  union
  {
    struct 
    {
      uint16_t hostfull;
      uint16_t networkMaxAllocated;
    }Rep1;
    uint8_t ipAddress[4];
  }host;

  uint16_t networkMaxAllocated;
  std::vector <std::string> ipAllocated;
  uint16_t currentLocation;
public:
  IPBlock()
  {
    NodeIPAlloc * instance = NodeIPAlloc::getInstance();
    host.Rep1.hostfull = instance->getHostBlock();
    host.Rep1.networkMaxAllocated = 1;
    currentLocation = 0;
  }

  std::string getSubnetMask()
  {
    return std::string("255.255.0.0");
  }

  std::string getBlockIP()
  {
    if ((host.Rep1.networkMaxAllocated & 0xFF) == 0xFF)
      host.Rep1.networkMaxAllocated += 2;
    
    std::string s= "";
    for (int i =0;i<4;i++)
    {
      s.append(std::to_string(host.ipAddress[i]));
      if (i != 3)
        s.append(".");      
    }
    ipAllocated.push_back(s);

    networkMaxAllocated += 1;
    return s;
  }

  std::string getRandomIpFromAllocation()
  {
      Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
      uv->SetAttribute ("Min", DoubleValue (0.0));
      uv->SetAttribute ("Max", DoubleValue (ipAllocated.size()));

      int loc = ceil(uv->GetValue());

      return ipAllocated[loc];
  }

  std::string getSeqNextIP(int * restart)
  {
    int loc = (currentLocation++)%ipAllocated.size();
    if (loc == 0)
      *restart = 1;
    else
      *restart = 0;
    return ipAllocated[loc];
  }
};

#endif