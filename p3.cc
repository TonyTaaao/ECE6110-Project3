#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mpi-interface.h"
#include "p2pCampusHelper.h"
#include <string>
#include "ns3/tag.h"
#include <chrono>
#include <ctime>


#define LINKBW            "100Mbps"
#define HUBBW            "1000Mbps"
#define INNER_DELAY       "5ms"
#define OUTER_DELAY       "8ms"
#define NUM_HUBS          4u
#define NUM_INNER_NODES   8u
#define NUM_OUTER_NODES   2u


uint32_t rank;
uint32_t count;

class MyTag : public Tag
{
public:
	static TypeId GetTypeId (void);
	virtual TypeId GetInstanceTypeId (void) const;
	virtual uint32_t GetSerializedSize (void) const;
	virtual void Serialize (TagBuffer i) const;
	virtual void Deserialize (TagBuffer i);
	virtual void Print (std::ostream &os) const;

	// these are our accessors to our tag structure
	void SetRxIDO (uint8_t r_id_o);
	uint8_t GetRxIDO (void) const;
	void SetRxIDI (uint8_t r_id_i);
	uint8_t GetRxIDI (void) const;
	void SetRxIDH (uint8_t r_id_h);
	uint8_t GetRxIDH (void) const;


private:
	uint8_t rx_id_o;
	uint8_t rx_id_i;
	uint8_t rx_id_h;
};

TypeId MyTag::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::MyTag").SetParent<Tag>().AddConstructor<MyTag> ().AddAttribute ("RxId","Rx Id", EmptyAttributeValue (),MakeUintegerAccessor (&MyTag::GetRxIDO),MakeUintegerChecker<uint8_t> ());
	return tid;
}
TypeId MyTag::GetInstanceTypeId (void) const{return GetTypeId ();}

uint32_t MyTag::GetSerializedSize (void) const{return 3;}

void MyTag::Serialize (TagBuffer i) const
{
	i.WriteU8(rx_id_o);
	i.WriteU8(rx_id_i);
	i.WriteU8(rx_id_h);
}

void MyTag::Deserialize (TagBuffer i)
{

	rx_id_o = i.ReadU8();
	rx_id_i = i.ReadU8();
	rx_id_h = i.ReadU8();
}

void MyTag::Print (std::ostream &os) const{os << "" << rx_id_o;}

void MyTag::SetRxIDO (uint8_t r_id_o){rx_id_o = r_id_o;}
uint8_t MyTag::GetRxIDO (void) const{return rx_id_o;}
void MyTag::SetRxIDI (uint8_t r_id_i){rx_id_i = r_id_i;}
uint8_t MyTag::GetRxIDI (void) const{return rx_id_i;}
void MyTag::SetRxIDH (uint8_t r_id_h){rx_id_h = r_id_h;}
uint8_t MyTag::GetRxIDH (void) const{return rx_id_h;}


class AssignToSimulator
{
private:
	int val[4];
public:
	AssignToSimulator(int numInstances)
{
		if (numInstances == 1)
		{
			for (int i=0;i<4;i++)
			{
				val[i] = 0;
			}
		}
		else if (numInstances == 2)
		{
			val[0]=val[1] = 0;
			val[3]=val[2] = 1;
		}
		if (numInstances ==  4 )
		{
			for (int i=0;i<4;i++)
			{
				val[i] = i;
			}
		}
}

	int getSimInstNum(int hubNum)
	{
		return val[hubNum];
	}
};

void dummyEvent()
{
}
//Declaration of global variables
void ReceivePacket (Ptr<Socket>);
void CreateVulnerableSinks();
std::vector <PointToPointCampusHelper> topology;
TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
void StartWorm(Ptr<Node>,uint32_t);
double stopTime = 10;
NodeContainer hAhB, hBhC, hChD, hDhA;
Ipv4InterfaceContainer iAiB, iBiC, iCiD, iDiA;
double ScanRate=5;
std::string ScanPattern = "Uniform";
void PrintTimeInNs();
void InfectedCount(uint32_t);
void GetNodeStartWork(uint32_t,uint32_t, uint32_t);
std::stringstream fileOut;
std::ofstream fWrite;

int main(int argc, char** argv) {
	Packet::EnablePrinting();
	Packet::EnableChecking();
	setbuf (stdout,NULL);
	std::cout<<"START PROGRAM ACTUAL TIME,";
	PrintTimeInNs();
	std::cout<<std::endl;
	std::string BackboneDelay = "120ms"; //20ms
	std::string SyncType = "Yawns";
	std::string pathOut=".";
	std::string fileName = "Output";
	CommandLine cmd;
	cmd.AddValue ("ScanRate", "Rate to generate worm traffic (5,10,20)ms", ScanRate);
	cmd.AddValue ("ScanPattern", "Scanning pattern (Uniform Local Sequential)", ScanPattern);
	cmd.AddValue ("BackboneDelay", "Delay for backbone links (lookahead)", BackboneDelay);
	cmd.AddValue ("SyncType", "Conservative algorithm (Yawns, Null)", SyncType);
	cmd.AddValue ("Filename", "Name of Output file ",fileName);
	cmd.Parse (argc, argv);

	/*This is used to set the type of synchronisation between different simulator Instances*/
	if(SyncType.compare("Null"))
	{
		GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::NullMessageSimulatorImpl"));
	}
	else
	{
		GlobalValue::Bind ("SimulatorImplementationType",StringValue ("ns3::DistributedSimulatorImpl"));
	}

	SeedManager::SetSeed (15);

	/*This section of the code decides how to assign each node to each process*/
	MpiInterface::Enable(&argc, &argv);

	rank = MpiInterface::GetSystemId();
	count = MpiInterface::GetSize();

	AssignToSimulator simAssigner(count);

	PointToPointHelper innerLinks;
	innerLinks.SetDeviceAttribute ("DataRate", StringValue (LINKBW ));
	innerLinks.SetChannelAttribute ("Delay", StringValue (INNER_DELAY));

	PointToPointHelper outerLinks;
	outerLinks.SetDeviceAttribute ("DataRate", StringValue (LINKBW));
	outerLinks.SetChannelAttribute ("Delay", StringValue (OUTER_DELAY));

	Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
	uv->SetAttribute ("Min", DoubleValue (0.0));
	uv->SetAttribute ("Max", DoubleValue (1.0));

	for (uint32_t i=0;i<NUM_HUBS;i++)
	{
		topology.push_back(PointToPointCampusHelper(NUM_INNER_NODES,NUM_OUTER_NODES,innerLinks,outerLinks,uv,i,
				simAssigner.getSimInstNum(i)));
	}

	hAhB = NodeContainer (topology[0].hub, topology[1].hub);
	hBhC = NodeContainer (topology[1].hub, topology[2].hub);
	hChD = NodeContainer (topology[2].hub, topology[3].hub);
	hDhA = NodeContainer (topology[3].hub, topology[0].hub);
	PointToPointHelper hubLinkA2B;
	hubLinkA2B.SetDeviceAttribute ("DataRate", StringValue (HUBBW ));
	hubLinkA2B.SetChannelAttribute ("Delay", StringValue (BackboneDelay));
	PointToPointHelper hubLinkB2C;
	hubLinkB2C.SetDeviceAttribute ("DataRate", StringValue (HUBBW ));
	hubLinkB2C.SetChannelAttribute ("Delay", StringValue (BackboneDelay));
	PointToPointHelper hubLinkC2D;
	hubLinkC2D.SetDeviceAttribute ("DataRate", StringValue (HUBBW ));
	hubLinkC2D.SetChannelAttribute ("Delay", StringValue (BackboneDelay));
	PointToPointHelper hubLinkD2A;
	hubLinkD2A.SetDeviceAttribute ("DataRate", StringValue (HUBBW ));
	hubLinkD2A.SetChannelAttribute ("Delay", StringValue (BackboneDelay));

	NetDeviceContainer devhAhB = hubLinkA2B.Install (hAhB);
	NetDeviceContainer devhBhC = hubLinkB2C.Install (hBhC);
	NetDeviceContainer devhChD = hubLinkC2D.Install (hChD);
	NetDeviceContainer devhDhA = hubLinkD2A.Install (hDhA);

	Ipv4AddressHelper ipv4;
	ipv4.SetBase ("1.1.1.0", "255.255.255.0");
	iAiB = ipv4.Assign (devhAhB);
	ipv4.SetBase ("2.1.1.0", "255.255.255.0");
	iBiC = ipv4.Assign (devhBhC);
	ipv4.SetBase ("3.1.1.0", "255.255.255.0");
	iCiD = ipv4.Assign (devhChD);
	ipv4.SetBase ("4.1.1.0", "255.255.255.0");
	iDiA = ipv4.Assign (devhDhA);

	//Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	CreateVulnerableSinks();

	Ptr<UniformRandomVariable> rndHub = CreateObject<UniformRandomVariable> ();
	rndHub->SetAttribute ("Min", DoubleValue (1.0));
	rndHub->SetAttribute ("Max", DoubleValue (4.0));
	uint32_t randomHub = rndHub->GetInteger();
	Ptr<Node> tmpNode = topology[3].allNodes.Get(7);

	InfectedCount(0);

	fileOut << pathOut << "/" << fileName + ".csv";
	remove(fileOut.str ().c_str ());

	fWrite.open(fileOut.str ().c_str (), std::ios::out|std::ios::app);

//	innerLinks.EnablePcapAll("Links");

	StartWorm(tmpNode,randomHub);

	
	Simulator::Schedule(Seconds (stopTime),dummyEvent);
	Simulator::Stop (Seconds (stopTime));
	Simulator::Run ();
	Simulator::Destroy ();
	fWrite.close();
	MpiInterface::Disable();
	return 0;
}

void SendPacket(Ptr<Socket> sock,std::string IpAddr)
{
	//sending custom packets to the nodes...

	//char test[10];
	std::ostringstream msg;
	msg << IpAddr.c_str()<<'\0';
	Ptr<Packet> packet = Create<Packet> ((uint8_t*)msg.str().c_str(), msg.str().length());
	sock->Send(packet);
	std::cout << Simulator::Now().GetSeconds() << std::endl;
}

void StartWorm(Ptr<Node> nodeId,uint32_t startHub)
{
	uint32_t countPack = 1;
	AssignToSimulator assign(count);
	uint32_t i;
	for ( i = 0; i < NUM_HUBS ;i++)
	{
		if (topology[i].infected.find(nodeId) != topology[i].infected.end())
			break;
	}


	if (rank == (uint32_t)assign.getSimInstNum(i))
	{
		if (ScanPattern=="Sequential")
		{
			//Worm Application Sequential.
			/*uint32_t  __attribute__((unused))rx_h;
			uint32_t  __attribute__((unused))rx_i;
			uint32_t  __attribute__((unused))rx_o;*/
			Ptr<ns3::Socket> srcSkt = Socket::CreateSocket(nodeId, tid);
			std::string ipAddrString;
			uint32_t hubCounter = 1,hubNum=NUM_HUBS;
			for (uint32_t i=startHub;i<=hubNum;i++)
			{
				ipAddrString = ipAddrString + std::to_string(i) +".";
				std::string ipAddrP1 = ipAddrString;
				/*rx_h = i;*/
				for (uint32_t j=2;j<=NUM_INNER_NODES+1;j++)
				{
					ipAddrString = ipAddrString + std::to_string(j) +".";
					std::string ipAddrP2 = ipAddrString;
					/*rx_i=j;*/
					for (uint32_t k=1;k<=NUM_OUTER_NODES+1;k++)
					{
						/*rx_o=k;*/
						ipAddrString = ipAddrString + std::to_string(k)+".1";
						static const InetSocketAddress srcAddr = InetSocketAddress(Ipv4Address(ipAddrString.c_str()),7667);
						srcSkt->Connect (srcAddr);
						Simulator::Schedule (MilliSeconds (ScanRate * countPack), &SendPacket,srcSkt, ipAddrString);
						ipAddrString = ipAddrP2;
						countPack ++;
					}
					ipAddrString = ipAddrP1;
				}
				ipAddrString = "";
				//std::cout<<hubCounter<<"i"<<i<<"hubNum"<<hubNum<<std::endl;
				if (hubCounter<4&&i==4){i=0;hubNum = hubNum-hubCounter;}
				hubCounter++;
			}
		}
		if (ScanPattern == "Uniform"||ScanPattern =="Local")
		{
			//Worm Application Uniform Random and Local Random (For Local check receivedpacket callback to see implementation of higher prob of local.
			/*uint32_t rx_h,rx_i,rx_o;
			*/Ptr<ns3::Socket> srcSkt = Socket::CreateSocket(nodeId, tid);
			std::string ipAddrString;
			uint32_t hubCounter = 1,hubNum=NUM_HUBS;
			for (uint32_t i=startHub;i<=hubNum;i++)
			{

				ipAddrString = ipAddrString + std::to_string(i) +".";
				std::string ipAddrP1 = ipAddrString;
				//rx_h = i;
				for (uint32_t j=2;j<=NUM_INNER_NODES+1;j++)
				{
					Ptr<UniformRandomVariable> rndInner = CreateObject<UniformRandomVariable> ();
					rndInner->SetAttribute ("Min", DoubleValue (2.0));
					rndInner->SetAttribute ("Max", DoubleValue (9.0));
					uint32_t randomInner = rndInner->GetInteger();
					ipAddrString = ipAddrString + std::to_string(randomInner) +".";
					std::string ipAddrP2 = ipAddrString;
					//rx_i=randomInner;
					for (uint32_t k=1;k<=NUM_OUTER_NODES+1;k++)
					{
						Ptr<UniformRandomVariable> rndOuter = CreateObject<UniformRandomVariable> ();
						rndOuter->SetAttribute ("Min", DoubleValue (1.0));
						rndOuter->SetAttribute ("Max", DoubleValue (3.0));
						uint32_t randomOuter = rndOuter->GetInteger();
						//rx_o=randomOuter;
						ipAddrString = ipAddrString + std::to_string(randomOuter)+".1";
						static const InetSocketAddress srcAddr = InetSocketAddress(Ipv4Address(ipAddrString.c_str()),7667);
						srcSkt->Connect (srcAddr);
						Simulator::Schedule (MilliSeconds (ScanRate * countPack ), &SendPacket, srcSkt,ipAddrString);
						ipAddrString = ipAddrP2;
						countPack++;
					}
					ipAddrString = ipAddrP1;
				}
				ipAddrString = "";
				//std::cout<<hubCounter<<"i"<<i<<"hubNum"<<hubNum<<std::endl;
				if (hubCounter<4&&i==4){i=0;hubNum = hubNum-hubCounter;}
				hubCounter++;
			}
		}
	}
}

void PrintTimeInNs()
{
	std::cout<<",";
	std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();

	typedef std::chrono::duration<int, std::ratio_multiply<std::chrono::hours::period, std::ratio<8>>::type> Days; /* UTC: +8:00 */

	Days days = std::chrono::duration_cast<Days>(duration);
	duration -= days;
	auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
	duration -= hours;
	auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
	duration -= minutes;
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
	duration -= seconds;
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
	duration -= milliseconds;
	auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration);
	duration -= microseconds;
	auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);

	std::cout << minutes.count() << ":"<< seconds.count() << ":"<< milliseconds.count() << ":"<< microseconds.count() << ":"<< nanoseconds.count() << ",";
	fWrite<< hours.count() << ":"<< minutes.count() << ":"<< seconds.count() << ":"<< milliseconds.count() << ":"<< microseconds.count() << ":"<< nanoseconds.count() << ",";

}

void InfectedCount(uint32_t hubNum)
{
	//used for printing the infected count//
	uint32_t infectedHubA=0,notInfectedHubA=0,infectedHubB=0,notInfectedHubB=0,infectedHubC=0,notInfectedHubC=0,infectedHubD=0,notInfectedHubD=0;
	std::map<Ptr<Node>,bool>::iterator it;
	if (hubNum == 0)
	{
		it=topology[0].infected.begin();
		while(it!=topology[0].infected.end())
		{
			if(it->second==true){infectedHubA++;}
			else if(it->second==false){notInfectedHubA++;
			/*	Ptr<Node>tmpNode = it->first;
			for (uint32_t j=0;j<tmpNode->GetNDevices();j++)
			{
				std::cout<<"\n";
				Ptr<Ipv4> ipv4 = tmpNode->GetObject<Ipv4> ();
				Ipv4InterfaceAddress iaddr = ipv4->GetAddress (j,0);
				Ipv4Address ipAddr = iaddr.GetLocal ();
				ipAddr.Print(std::cout);
			}*/
			}
			it++;
		}
		it=topology[1].infected.begin();
		while(it!=topology[1].infected.end())
		{
			if(it->second==true){infectedHubB++;}
			else if(it->second==false){notInfectedHubB++;}
			it++;
		}
		it=topology[2].infected.begin();
		while(it!=topology[2].infected.end())
		{
			if(it->second==true){infectedHubC++;}
			else if(it->second==false){notInfectedHubC++;}
			it++;
		}
		it=topology[3].infected.begin();
		while(it!=topology[3].infected.end())
		{
			if(it->second==true){infectedHubD++;}
			else if(it->second==false){notInfectedHubD++;}
			it++;
		}
		std::cout<<infectedHubA+infectedHubB+infectedHubC+infectedHubD<<","<<(notInfectedHubA)+(notInfectedHubB)+(notInfectedHubC)+(notInfectedHubD)-4;
		 	fWrite<<","<<notInfectedHubA<<","<<(notInfectedHubB-2)<<","<<(notInfectedHubC-1)<<","<<(notInfectedHubD-1)<<","<<(notInfectedHubA)+(notInfectedHubB)+(notInfectedHubC)+(notInfectedHubD)-4<<","<<notInfectedHubA+(notInfectedHubB)+(notInfectedHubC)+(notInfectedHubD)+infectedHubA+infectedHubB+infectedHubC+infectedHubD-4<<",";
		 		//std::cout<<std::endl;
	}
	else if(hubNum == 1)
	{
		it=topology[0].infected.begin();
		while(it!=topology[0].infected.end())
		{
			if(it->second==true){infectedHubA++;}
			else if(it->second==false){notInfectedHubA++;}
			it++;
		}
		std::cout<<"hubAInfected#,"<<infectedHubA<<","<<",hubA_NOT_Infected#,"<<notInfectedHubA<<",";;
	}
	else if(hubNum == 2)
	{
		it=topology[1].infected.begin();
		while(it!=topology[1].infected.end())
		{
			if(it->second==true){infectedHubB++;}
			else if(it->second==false){notInfectedHubB++;}
			it++;
		}
		std::cout<<"hubBInfected#,"<<infectedHubB<<","<<",hubB_NOT_Infected#,"<<(notInfectedHubB-2)<<",";;
	}
	else if(hubNum == 3)
	{
		it=topology[2].infected.begin();
		while(it!=topology[2].infected.end())
		{
			if(it->second==true){infectedHubC++;}
			else if(it->second==false){notInfectedHubC++;}
			it++;
		}
		std::cout<<"hubCInfected#,"<<infectedHubC<<","<<",hubC_NOT_Infected#,"<<(notInfectedHubC-1)<<",";
	}
	else if(hubNum == 4)
	{
		it=topology[3].infected.begin();
		while(it!=topology[3].infected.end())
		{
			if(it->second==true){infectedHubD++;}
			else if(it->second==false){notInfectedHubD++;}
			it++;
		}
		std::cout<<"hubDInfected#,"<<infectedHubD<<","<<",hubD_NOT_Infected#,"<<(notInfectedHubD-1)<<",";
	}
	infectedHubA=0;notInfectedHubA=0;infectedHubB=0;notInfectedHubB=0;infectedHubC=0;notInfectedHubC=0;infectedHubD=0;notInfectedHubD=0;
}

void GetNodeStartWork(uint32_t hN,uint32_t iN, uint32_t oN)
{
	//First Part is used to get the node from Ip address // Unusual way to get it, but could not find an easier way. Could make map in topology.
	std::string ipString = std::to_string(hN)+"."+std::to_string(iN)+"."+std::to_string(oN)+".1";
	Ptr<Node> rxNode;
	for(uint32_t i =0;i<topology[hN-1].allNodes.GetN();i++)
	{
		for (uint32_t j=1;j<topology[hN-1].allNodes.Get(i)->GetNDevices();j++)
		{
			Ptr<Ipv4> ipv4 = topology[hN-1].allNodes.Get(i)->GetObject<Ipv4> ();
			Ipv4InterfaceAddress iaddr = ipv4->GetAddress (j,0);
			Ipv4Address ipAddr = iaddr.GetLocal ();
			std::string tmpStr;
			std::ostringstream oss;
			ipAddr.Print(oss);
			if(oss.str()==ipString.c_str())
			{
				rxNode = topology[hN-1].allNodes.Get(i);
				break;
			}
		}
	}
	//start the worm for nodes that are not infected (as they get infected)
	if(topology[hN-1].infected[rxNode]==false)
	{
		//std::cout<<"GOT TO HERE"<<std::endl;
		Ptr<UniformRandomVariable> rndN = CreateObject<UniformRandomVariable> ();
		rndN->SetAttribute ("Min", DoubleValue (1.0));
		rndN->SetAttribute ("Max", DoubleValue (100.0));
		uint32_t randomLocal = rndN->GetInteger();
		if(ScanPattern == "Local"&&randomLocal<68){	StartWorm(rxNode,hN);}
		else
		{
			rndN->SetAttribute ("Min", DoubleValue (1.0));
			rndN->SetAttribute ("Max", DoubleValue (4.0));
			uint32_t randomHub = rndN->GetInteger();
			StartWorm(rxNode,randomHub);
		}
		topology[hN-1].infected[rxNode]=true;
	}
}

void CreateVulnerableSinks()
{
	//Create Socket Sink For All Nodes -Hobs
	AssignToSimulator assign(count);
	uint32_t i = 0,j=0;
	ns3::InetSocketAddress sckAddr = InetSocketAddress(Ipv4Address::GetAny(), 7667);
	for (i = 0; i<NUM_HUBS; i++)
	{
		for(j=0;j<topology[i].allNodes.GetN();j++)
		{
			if (rank == (uint32_t)assign.getSimInstNum(i))
			{
				Ptr<ns3::Socket> sinkSkt = Socket::CreateSocket(topology[i].allNodes.Get(j), tid);
				sinkSkt->Bind(sckAddr);
				sinkSkt->SetRecvCallback(MakeCallback(&ReceivePacket));
			}
		}
	}
	//Create Socket Sinks For Hubs

	for (i = 0; i<NUM_HUBS; i++)
	{
		if (rank == (uint32_t)assign.getSimInstNum(i))
		{
			Ptr<ns3::Socket> sinkSkt = Socket::CreateSocket(topology[i].hub, tid);
			sinkSkt->Bind(sckAddr);
			sinkSkt->SetRecvCallback(MakeCallback(&ReceivePacket));
		}
	}

}
void ReceivePacket (Ptr<Socket> socket)
{
	Ptr<Packet> packet;
	MyTag tagcopy;
	while ((packet = socket->Recv ()))
	{
		uint32_t hN,iN,oN;
		uint8_t *buf = new uint8_t(packet->GetSize());
		packet->CopyData(buf , packet->GetSize());
		//std::cout<<buf<<std::endl;
		std::string token;
		std::ostringstream msg;
		msg << buf<<'\0';
		std::string pData = msg.str().c_str();
		uint32_t countIp=0;
		while(token !=pData)
		{
			token = pData.substr(0,pData.find_first_of("."));
			pData = pData.substr(pData.find_first_of(".")+1);
			//std::cout<<token.c_str()<<std::endl;
			if (countIp == 0){hN = std::atoi(token.c_str());}
			if (countIp == 1){iN = std::atof(token.c_str());}
			if (countIp == 2){oN = std::atof(token.c_str());}
			countIp++;
		}
		//std::cout<<"GOT PACKET TO,"<<buf<< ", SimulatorTime,"<<Simulator::Now().GetSeconds()<<", ActualTime";
		std::cout<<Simulator::Now().GetSeconds();
			fWrite <<(uint32_t)tagcopy.GetRxIDH()<<"."<<(uint32_t)tagcopy.GetRxIDI()<<"."<<(uint32_t)tagcopy.GetRxIDO()<<".1,"<<(uint32_t)tagcopy.GetRxIDH()<<"," <<Simulator::Now().GetSeconds()<<",";
		 	PrintTimeInNs();
		 InfectedCount(0);
		 std::cout<<std::endl;
		 	fWrite<<"\n";
		  	GetNodeStartWork(hN,iN,oN);
	}
}
