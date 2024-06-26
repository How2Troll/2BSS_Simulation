#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/buildings-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("WifiBSSSimulation");

/* populate ARP tables */
void PopulateARPcache()
{
    NS_LOG_INFO("Populating ARP Cache");
    Ptr<ArpCache> arp = CreateObject<ArpCache>();
    arp->SetAliveTimeout(Seconds(3600 * 24 * 365));

    for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
    {
        Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol>();
        NS_ASSERT(ip != nullptr); //0 -> nullptr
        ObjectVectorValue interfaces;
        ip->GetAttribute("InterfaceList", interfaces);

        for (ObjectVectorValue::Iterator j = interfaces.Begin(); j != interfaces.End(); j++)
        {
            Ptr<Ipv4Interface> ipIface = (*j).second->GetObject<Ipv4Interface>();
            NS_ASSERT(ipIface != nullptr);
            Ptr<NetDevice> device = ipIface->GetDevice();
            NS_ASSERT(device != nullptr);
            Mac48Address addr = Mac48Address::ConvertFrom(device->GetAddress());

            for (uint32_t k = 0; k < ipIface->GetNAddresses(); k++)
            {
                Ipv4Address ipAddr = ipIface->GetAddress(k).GetLocal();
                if (ipAddr == Ipv4Address::GetLoopback())
                    continue;

                ArpCache::Entry *entry = arp->Add(ipAddr);
                Ipv4Header ipv4Hdr;
                ipv4Hdr.SetDestination(ipAddr);
                Ptr<Packet> p = Create<Packet>(100);
                entry->MarkWaitReply(ArpCache::Ipv4PayloadHeaderPair(p, ipv4Hdr));
                entry->MarkAlive(addr);
            }
        }
    }

    for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
    {
        Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol>();
        NS_ASSERT(ip != nullptr);
        ObjectVectorValue interfaces;
        ip->GetAttribute("InterfaceList", interfaces);

        for (ObjectVectorValue::Iterator j = interfaces.Begin(); j != interfaces.End(); j++)
        {
            Ptr<Ipv4Interface> ipIface = (*j).second->GetObject<Ipv4Interface>();
            ipIface->SetAttribute("ArpCache", PointerValue(arp));
        }
    }
}

void installTrafficGenerator(Ptr<ns3::Node> fromNode, Ptr<ns3::Node> toNode, int port, std::string offeredLoad, int packetSize, int simulationTime, int warmupTime, uint8_t tosValue)
{
    NS_LOG_INFO("Installing traffic generator from node " << fromNode->GetId() << " to node " << toNode->GetId());

    Ptr<Ipv4> ipv4 = toNode->GetObject<Ipv4>();           // Get Ipv4 instance of the node
    Ipv4Address addr = ipv4->GetAddress(1, 0).GetLocal(); // Get Ipv4InterfaceAddress of xth interface.

    ApplicationContainer sourceApplications, sinkApplications;

    double min = 0.0;
    double max = 1.0;
    Ptr<UniformRandomVariable> fuzz = CreateObject<UniformRandomVariable>();
    fuzz->SetAttribute("Min", DoubleValue(min));
    fuzz->SetAttribute("Max", DoubleValue(max));

    InetSocketAddress sinkSocket(addr, port);
    sinkSocket.SetTos(tosValue);
    OnOffHelper onOffHelper("ns3::UdpSocketFactory", sinkSocket);
    onOffHelper.SetConstantRate(DataRate(offeredLoad + "Mbps"), packetSize);
    sourceApplications.Add(onOffHelper.Install(fromNode)); //fromNode
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkSocket);
    sinkApplications.Add(packetSinkHelper.Install(toNode)); //toNode

    sinkApplications.Start(Seconds(warmupTime));
    sinkApplications.Stop(Seconds(simulationTime));
    sourceApplications.Start(Seconds(warmupTime + fuzz->GetValue()));
    sourceApplications.Stop(Seconds(simulationTime));
}

int main(int argc, char *argv[])
{
    NS_LOG_UNCOND("Starting the WiFi BSS Simulation");
    double duration = 20.0;   // seconds
    double d3 = 140;        // meters
    double d2 = 2;
    double powSta = 15.0;    // dBm
    double powAp = 20.0;     // dBm
    double ccaEdTrSta = -62; // dBm Signal Detection is then -82dBm
    double ccaEdTrAp = -62;  // dBm
    uint32_t mcs = 5;        // MCS value
    uint32_t mcsLegacy = 5;        // MCS value
    double interval = 0.001; // seconds
    bool enableObssPd = false;
    double obssPdThreshold = -72.0; // dBm
    int packetSize = 1472;
    int nSTA =  1;
    int nSTALegacy = 0;
    int nAP = 2;
    std::string offeredLoad = "100"; //Mbps per station
    int simulationTime = 5; //default 20
    int warmupTime = 5;
    bool BE = true;
    double r = 20;
    bool rtsCts = false;
    double minimumRssi = -82; // dBm
    int scenario = 1;

    CommandLine cmd(__FILE__);

    cmd.AddValue("duration", "Duration of simulation (s)", duration);
    cmd.AddValue("interval", "Inter packet interval (s)", interval);
    cmd.AddValue("enableObssPd", "Enable/disable OBSS_PD", enableObssPd);
    cmd.AddValue("obssPdThreshold", "obssPdThreshold", obssPdThreshold);
    cmd.AddValue("d3", "Distance between AP1 and AP2 (m)", d3); //most likely D1
    cmd.AddValue("d2", "Distance between AP and STA (m)", d2);
    cmd.AddValue("mcs", "The constant MCS value to transmit HE PPDUs", mcs);
    cmd.AddValue("mcsLegacy", "The constant MCS value to transmit HE PPDUs", mcsLegacy);
    cmd.AddValue("offeredLoad", "offered load per station", offeredLoad);
    cmd.AddValue("BE", "transmission of BK traffic", BE);
    cmd.AddValue("r", "radius",r);
    cmd.AddValue("nSTA", "number of stations", nSTA);
    cmd.AddValue("nSTALegacy", "number of stations Legacy", nSTALegacy);
    cmd.AddValue("rtsCts", "enable/disable RTS CTS", rtsCts);
    cmd.AddValue("scenario", "Select scenario (1 or 2)", scenario);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Creating node containers");
    NodeContainer wifiApNodes;
    wifiApNodes.Create(nAP);

    NodeContainer wifiStaNodes[nAP];
    NodeContainer wifiStaNodesLegacy[nAP];

    for (int i = 0; i < nAP; ++i)
    {
        wifiStaNodes[i].Create(nSTA);
        wifiStaNodesLegacy[i].Create(nSTALegacy);
    }

    YansWifiPhyHelper spectrumPhy;
    Ptr<YansWifiChannel> spectrumChannel;
    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default ();

    //change it for buildings
    channelHelper.AddPropagationLoss("ns3::OhBuildingsPropagationLossModel");
    channelHelper.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    spectrumChannel = channelHelper.Create ();

    spectrumPhy.SetChannel (spectrumChannel);
    spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel",
                                         "MinimumRssi", DoubleValue (minimumRssi));
    spectrumPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

    /************** 802.11AX ****************/

    WifiHelper wifiLegacy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);
    wifiLegacy.SetStandard (WIFI_STANDARD_80211a);

    if (enableObssPd)
    {
        wifi.SetObssPdAlgorithm("ns3::ConstantObssPdAlgorithm",
                                "ObssPdLevel", DoubleValue(obssPdThreshold));
    }

    WifiMacHelper mac;
    std::ostringstream oss;
    oss << "HeMcs" << mcs;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(oss.str()),
                                 "ControlMode", StringValue(oss.str())
                                ,"FragmentationThreshold", UintegerValue (2500)
                                );

    /* STANDARD 802.11A*/                            
    wifiLegacy.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate54Mbps"),
                                 "ControlMode", StringValue("OfdmRate54Mbps")
                                ,"RtsCtsThreshold",        UintegerValue (rtsCts ? 0 : 2500)
                                ,"FragmentationThreshold", UintegerValue (2500)
                                );
                                 

    /***************** BSS  *****************/

    NetDeviceContainer staDevices[nAP];
    NetDeviceContainer staDevicesLegacy[nAP];
    NetDeviceContainer apDevices;

    for (int i = 0; i < nAP; i++){
        spectrumPhy.Set("TxPowerStart", DoubleValue(powSta));
        spectrumPhy.Set("TxPowerEnd", DoubleValue(powSta));
        spectrumPhy.Set("CcaEdThreshold", DoubleValue(ccaEdTrSta));
        spectrumPhy.Set("RxSensitivity", DoubleValue(-92.0));

        Ssid ssid = Ssid("network-" + std::to_string(i));
        mac.SetType("ns3::StaWifiMac",
                    "QosSupported", BooleanValue(true),
                    "Ssid", SsidValue(ssid));

        NetDeviceContainer staDevice;
        NetDeviceContainer staDeviceLegacy;

        staDevice = wifi.Install(spectrumPhy, mac, wifiStaNodes[i]);
        staDeviceLegacy = wifiLegacy.Install(spectrumPhy, mac, wifiStaNodesLegacy[i]);

        staDevices[i].Add(staDevice);
        staDevicesLegacy[i].Add(staDeviceLegacy);

        spectrumPhy.Set("TxPowerStart", DoubleValue(powAp));
        spectrumPhy.Set("TxPowerEnd", DoubleValue(powAp));
        spectrumPhy.Set("CcaEdThreshold", DoubleValue(ccaEdTrAp));
        spectrumPhy.Set("RxSensitivity", DoubleValue(-92.0));

        mac.SetType("ns3::ApWifiMac",
                    "QosSupported", BooleanValue(true),
                    "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(spectrumPhy, mac, wifiApNodes.Get(i));
        apDevices.Add(apDevice);

        Ptr<WifiNetDevice> apDevice_i = apDevice.Get(0)->GetObject<WifiNetDevice>();
        Ptr<ApWifiMac> apWifiMac = apDevice_i->GetMac()->GetObject<ApWifiMac>();
        if (enableObssPd)
        {
            apDevice_i->GetHeConfiguration()->SetAttribute("BssColor", UintegerValue(i+1));
        }
    }

    for (int i=0; i < nAP; i++){
        for (int j=0; j < nSTA; j++){
            Ptr<NetDevice> dev = wifiStaNodes[i].Get (j)->GetDevice (0);
            Ptr<WifiNetDevice> wifi_dev = DynamicCast<WifiNetDevice> (dev);
            wifi_dev->GetMac ()->SetAttribute ("BE_MaxAmsduSize", UintegerValue (0));
            wifi_dev->GetMac ()->SetAttribute ("BE_MaxAmpduSize", UintegerValue (0));
        }
        for (int j=0; j < nSTALegacy; j++){
            Ptr<NetDevice> dev = wifiStaNodesLegacy[i].Get (j)->GetDevice (0);
            Ptr<WifiNetDevice> wifi_dev = DynamicCast<WifiNetDevice> (dev);
            wifi_dev->GetMac ()->SetAttribute ("BE_MaxAmsduSize", UintegerValue (0));
            wifi_dev->GetMac ()->SetAttribute ("BE_MaxAmpduSize", UintegerValue (0));
        }
    Ptr<NetDevice> dev = wifiApNodes.Get(i)->GetDevice (0);
    Ptr<WifiNetDevice> wifi_dev = DynamicCast<WifiNetDevice> (dev);
    wifi_dev->GetMac ()->SetAttribute ("BE_MaxAmsduSize", UintegerValue (0));
    wifi_dev->GetMac ()->SetAttribute ("BE_MaxAmpduSize", UintegerValue (0));
    }

    MobilityHelper mobility;

    if (scenario == 1)
    {
        // Set positions for Scenario 1
        Ptr<ListPositionAllocator> positionAlloc1 = CreateObject<ListPositionAllocator> ();
        positionAlloc1->Add(Vector(0.0, 0.0, 1.0)); // AP1
        for(int i=0; i<nSTA+nSTALegacy; i++){
            positionAlloc1->Add(Vector(-d2, 0.0, 1.0)); // STA1
        }
        positionAlloc1->Add(Vector(d3, 0.0, 1.0)); // AP2
        for(int i=0; i<nSTA+nSTALegacy; i++){
            positionAlloc1->Add(Vector(d3 + d2, 0.0, 1.0)); // STA2
        }

        mobility.SetPositionAllocator(positionAlloc1);
    }
    else if (scenario == 2)
    {
        // Set positions for Scenario 2
        Ptr<ListPositionAllocator> positionAlloc2 = CreateObject<ListPositionAllocator> ();
        positionAlloc2->Add(Vector(d3, 0.0, 1.0)); // AP2
        for(int i=0; i<nSTA+nSTALegacy; i++){
            positionAlloc2->Add(Vector(d3 + d2, 0.0, 1.0)); // STA2
        }
        positionAlloc2->Add(Vector(0.0, 0.0, 1.0)); // AP1
        for(int i=0; i<nSTA+nSTALegacy; i++){
            positionAlloc2->Add(Vector(-d2, 0.0, 1.0)); // STA1
        }

        mobility.SetPositionAllocator(positionAlloc2);
    }

    //BUILD THE WALL

    Ptr<Building> building = CreateObject<Building> ();
    double buildingWidth = 0.2; // Thickness of the wall
    double buildingLength = 5.0; // Length of the wall
    double buildingHeight = 3.0; // Height of the wall

    building->SetBoundaries (Box (d3/2 - buildingWidth/2, d3/2 + buildingWidth/2, -buildingLength/2, buildingLength/2, 0.0, buildingHeight));
    building->SetBuildingType (Building::Residential);
    building->SetExtWallsType (Building::ConcreteWithWindows);
    building->SetNFloors (1);
    building->SetNRoomsX (1);
    building->SetNRoomsY (1);

    for (int i = 0; i < nAP; i++){
        mobility.Install(wifiApNodes.Get(i));
        mobility.Install(wifiStaNodes[i]);
        mobility.Install(wifiStaNodesLegacy[i]);
    }

    for (int i = 0; i < nAP; ++i) {
        BuildingsHelper::Install(wifiApNodes.Get(i));
        BuildingsHelper::Install(wifiStaNodes[i]);
        if (nSTALegacy > 0) 
        {
            BuildingsHelper::Install(wifiStaNodesLegacy[i]);
        }
    }

    /* Internet Stack */
    InternetStackHelper stack;
    stack.Install(wifiApNodes);
    for(int i=0; i< nAP; i++){
        stack.Install(wifiStaNodes[i]);
        stack.Install(wifiStaNodesLegacy[i]);
    }

    std::cout << std::endl<< "+++++++++++++++++++++++++++++++++++++++++++" << std::endl;
    std::cout<< "OBSS enabled: \t" << enableObssPd << std::endl;
    std::cout<< "CTS enabled: \t" << rtsCts << std::endl;
    std::cout<< "OBSS PD threshold: \t" << obssPdThreshold << std::endl;
    std::cout<< "Distance betwen AP and STA: \t" << d2 << std::endl;
    std::cout<< "Distance between AP: \t" << d3 << std::endl;
    std::cout<< "MCS AX: \t" << mcs << std::endl;
    std::cout<< "MCS Legacy: \t" << mcsLegacy << std::endl;
    std::cout<< "stacje AX: \t" << nSTA << std::endl;
    std::cout<< "stacje legacy: \t" << nSTALegacy << std::endl;
    std::cout<< "offered Load: \t" << offeredLoad << std::endl;
    std::cout<< "Scenario: \t" << scenario << std::endl;
    std::cout<< "+++++++++++++++++++++++++++++++++++++++++++" << std::endl;
    std::cout << std::endl<< "Node positions" << std::endl;
/*wylistowanie polozenia wezlow w przestrzeni*/
    for(int i = 0; i < nAP; i++){
        Ptr<MobilityModel> positionAP = wifiApNodes.Get(i)->GetObject<MobilityModel>();
        Vector pos = positionAP->GetPosition();
        std::cout << "AP BSS: "<< i << "\tx=" << pos.x << ", y=" << pos.y << std::endl;
    }

    for (int i = 0; i < nAP; i++){
        int n = 1;
        for (NodeContainer::Iterator j = wifiStaNodes[i].Begin(); j != wifiStaNodes[i].End(); ++j)
        {
            Ptr<Node> object = *j;
            Ptr<MobilityModel> position = object->GetObject<MobilityModel>();
            Vector pos = position->GetPosition();
            std::cout << "BSS "<< i+1 <<", Sta " << n << ":\tx=" << pos.x << ", y=" << pos.y << std::endl;
            n++;
        }
        for (NodeContainer::Iterator j = wifiStaNodesLegacy[i].Begin(); j != wifiStaNodesLegacy[i].End(); ++j)
        {
            Ptr<Node> object = *j;
            Ptr<MobilityModel> position = object->GetObject<MobilityModel>();
            Vector pos = position->GetPosition();
            std::cout << "BSS "<< i+1 <<",  Legacy Sta " << n << ":\tx=" << pos.x << ", y=" << pos.y << std::endl;
            n++;
        }
    }

    Ipv4AddressHelper address;
    for (int i = 0; i < nAP; ++i)
    {
        std::string addr;
        addr = "192.168." + to_string(i) + ".0";
        const char *cstr = addr.c_str(); // convert to constant char
        address.SetBase(Ipv4Address(cstr), "255.255.255.0");
        address.Assign(apDevices.Get(i));
        address.Assign(staDevices[i]);
        address.Assign(staDevicesLegacy[i]);

    }

    PopulateARPcache();
    if (BE)
    {
        int port = 0;
        for (int i = 0; i < nAP; ++i)
        {
            port += 1000;
            for (int j = 0; j < nSTA; ++j){
                std::cout << "AX port: "<< port << std::endl; 
                installTrafficGenerator(wifiStaNodes[i].Get(j), wifiApNodes.Get(i), port , offeredLoad, packetSize, simulationTime, warmupTime, 0x70);
                port+=2;            
            }
            port +=1;
            for (int j =0; j< nSTALegacy; ++j){
                std::cout << "Legacy port: "<< port << std::endl; 
                installTrafficGenerator(wifiStaNodesLegacy[i].Get(j), wifiApNodes.Get(i), port, offeredLoad, packetSize, simulationTime, warmupTime, 0x70);
                port+=2;
            }
            port +=1;
        }
    }

    FlowMonitorHelper flowMonHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonHelper.InstallAll();

    Simulator::Stop(Seconds(duration));
    Simulator::Run();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowMonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    std::string proto = "UDP";
    std::vector<uint64_t> txBytesPerBss = std::vector<uint64_t>(nAP, 0);
    std::vector<uint64_t> rxBytesPerBss = std::vector<uint64_t>(nAP, 0);
    std::vector<uint64_t> txPacketsPerBss = std::vector<uint64_t>(nAP, 0);
    std::vector<uint64_t> rxPacketsPerBss = std::vector<uint64_t>(nAP, 0);
    std::vector<uint64_t> lostPacketsPerBss = std::vector<uint64_t>(nAP, 0);
    std::vector<double> throughputPerBss = std::vector<double>(nAP, 0.0);
    double throughputAX = 0.0;
    double throughputLegacy = 0.0;

    std::vector<Time> delaySumPerBss = std::vector<Time>(nAP, Seconds(0));
    std::vector<Time> jitterSumPerBss = std::vector<Time>(nAP, Seconds(0));

    int bss;

    for (std::map<FlowId, FlowMonitor::FlowStats>::iterator i = stats.begin(); i != stats.end(); i++)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        int port = t.destinationPort;
        bss = port / 1000;

        for(int j = 1; j <= nAP; j ++){
            if(bss == j){
                txBytesPerBss[bss] += i->second.txBytes;
                rxBytesPerBss[bss] += i->second.rxBytes;
                txPacketsPerBss[bss] += i->second.txPackets;
                rxPacketsPerBss[bss] += i->second.rxPackets;
                lostPacketsPerBss[bss] += i->second.lostPackets;
                throughputPerBss[bss] += (i->second.rxPackets > 0 ? i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 : 0);
                delaySumPerBss[bss] += i->second.delaySum;
                jitterSumPerBss[bss] += i->second.jitterSum;

                double staLoad = (i->second.rxPackets > 0 ? i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 : 0);
                uint64_t staNo = port - bss*1000;

                std::cout << "  Throughput per STA:" << staNo << "\t"<< staLoad << " Mb/s \t"<< std::endl;
                if(port%2 == 0){
                    throughputAX += staLoad;
                }else{
                    throughputLegacy +=staLoad;
                }

            }
        }
    }

    if (BE)
    {
        double totalThroughput = 0.0;
        for (int i = 1; i <= nAP; i++){
            std::cout << "======================= BSS " << i << "======================" << std::endl;
            std::cout << "  Throughput:\t" << throughputPerBss[i] << " Mb/s" << std::endl;
            totalThroughput += throughputPerBss[i];
            std::cout << "  Packet loss:\t" << lostPacketsPerBss[i] << " packets" << std::endl;
            std::cout << "  Delay:\t" << delaySumPerBss[i] << " seconds" << std::endl;
        }
        std::cout << "******************************************************" << std::endl;
        std::cout << "   AX Throughput:\t" << throughputAX << " Mb/s" << std::endl;
        std::cout << "   LEGACY Throughput:\t" << throughputLegacy << " Mb/s" << std::endl;
        std::cout << "   TOTAL Throughput:\t" << totalThroughput << " Mb/s" << std::endl;
    }

    Simulator::Destroy();

    return 0;
}
