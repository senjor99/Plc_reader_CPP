
#include <profi_DCP.hpp>
#include <sys/types.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
    #include <misc.h>
    #include <fstream>
#endif
using namespace std::chrono;

#pragma comment(lib, "iphlpapi.lib")

/* ---------------- Platform-specific utilities ---------------- */

#ifdef _WIN32

/// \brief Windows: extract GUID from selected pcap adapter string.
/// \return "{...}" GUID substring or empty string if not found.
std::string profinet::PcapClient::extract_guid() 
{
    if (net_card == "") return {};
    const char* n = net_card.c_str();
    const char* lb = strchr(n, '{');
    const char* rb = lb ? strchr(lb, '}') : nullptr;
    if (!lb || !rb || rb <= lb) return {};
    return std::string(lb, rb - lb + 1); 
}

/// \brief Windows: resolve MAC address of selected adapter via IP Helper API.
/// \throws std::runtime_error if adapter or MAC cannot be resolved.
std::array<uint8_t,6> profinet::PcapClient::_get_mac()
{
    if (net_card == "") throw std::runtime_error("WARNING no network card found");

    std::string guid = extract_guid();
    if (guid.empty()) throw std::runtime_error("WARNING no GUID could be found on the network card");

    ULONG bufLen = 16 * 1024;
    std::vector<unsigned char> buf(bufLen);
    IP_ADAPTER_ADDRESSES* aa = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());

    DWORD ret = GetAdaptersAddresses(AF_UNSPEC,
                                    GAA_FLAG_SKIP_ANYCAST |
                                    GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST,
                                    nullptr, aa, &bufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buf.resize(bufLen);
        aa = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
        ret = GetAdaptersAddresses(AF_UNSPEC,
                                GAA_FLAG_SKIP_ANYCAST |
                                GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST,
                                nullptr, aa, &bufLen);
    }
    if (ret != NO_ERROR) throw std::runtime_error("WARNING MAC Addres not found on the network card");


    for (IP_ADAPTER_ADDRESSES* it = aa; it; it = it->Next) {
        if (!it->AdapterName) continue;
        if (guid == it->AdapterName) {
            if (it->PhysicalAddressLength >= 6) {
                std::array<uint8_t,6> mac{};
                memcpy(mac.data(), it->PhysicalAddress, 6);
                return mac;
            }
        }
    }
    throw std::runtime_error("WARNING MAC Addres search got no results");
}

#else   // ===== Linux / Unix-like =====

  #ifdef __linux__
    #include <netpacket/packet.h>   // sockaddr_ll
  #elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    #include <net/if_dl.h>          // AF_LINK (macOS/BSD)
  #endif

    /// \brief Linux/BSD: no GUID concept; returns empty string.
    std::string profinet::PcapClient::extract_guid()
    {
        return {}; // non usato su Linux
    }

    /// \brief POSIX: resolve MAC address for the selected interface using getifaddrs().
    /// \throws std::runtime_error if interface or MAC cannot be resolved.
    std::array<uint8_t,6> profinet::PcapClient::_get_mac()
    {
        if (net_card.empty())
            throw std::runtime_error("WARNING no network card found");

        struct ifaddrs* ifaddr = nullptr;
        if (getifaddrs(&ifaddr) == -1)
            throw std::runtime_error("WARNING getifaddrs failed");

        std::array<uint8_t,6> mac{};
        bool found = false;

        for (auto* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) continue;
            if (net_card != ifa->ifa_name) continue;

            #ifdef __linux__
            if (ifa->ifa_addr->sa_family == AF_PACKET) {
                auto* s = reinterpret_cast<struct sockaddr_ll*>(ifa->ifa_addr);
                if (s->sll_halen >= 6) {
                    std::memcpy(mac.data(), s->sll_addr, 6);
                    found = true;
                    break;
                }
            }
            #elif defined(AF_LINK)
            if (ifa->ifa_addr->sa_family == AF_LINK) {
                auto* sdl = reinterpret_cast<struct sockaddr_dl*>(ifa->ifa_addr);
                if (sdl->sdl_alen >= 6) {
                    const unsigned char* hw = reinterpret_cast<unsigned char*>(LLADDR(sdl));
                    std::memcpy(mac.data(), hw, 6);
                    found = true;
                    break;
                }
            }
            #endif
        }

        freeifaddrs(ifaddr);

        if (!found)
            throw std::runtime_error("WARNING MAC Address not found on the network card");

        return mac;
    }

#endif


/* ---------------- Pcap Client ---------------- */

/// \brief Constructor: enumerates interfaces via pcap.
profinet::PcapClient::PcapClient()
    : net_cards(_get_netCards()){};

/// \brief Enumerate NICs and build "index: label" -> pcap name map.
/// \throws std::runtime_error if pcap_findalldevs fails.
std::map<std::string,std::string> profinet::PcapClient::_get_netCards()
{
    pcap_if_t* alldevs = nullptr;
    std::map<std::string, std::string> cards;

    #ifdef _WIN32
        // Windows: usa la sorgente di default
        if (pcap_findalldevs_ex(PCAP_SRC_IF_STRING, nullptr, &alldevs, errbuf) != 0 || !alldevs) {
            throw std::runtime_error(std::string("No network device found: ") + errbuf);
        }
    #else
        // Linux/Unix
        if (pcap_findalldevs(&alldevs, errbuf) != 0 || !alldevs) {
            throw std::runtime_error(std::string("No network device found: ") + errbuf);
        }
    #endif

    int idx = 0;
    for (pcap_if_t* d = alldevs; d; d = d->next) {
        // descrizione può essere NULL
        const char* desc = d->description ? d->description : d->name;
        std::string name = desc ? std::string(desc) : std::string(d->name ? d->name : "");

        // pulizia opzionale di apici singoli: fallo solo se *presenti*
        size_t p1 = name.find('\'');
        if (p1 != std::string::npos) {
            size_t p2 = name.find('\'', p1 + 1);
            if (p2 != std::string::npos && p2 > p1)
                name = name.substr(p1 + 1, p2 - (p1 + 1));
        }

        // chiave "N: Nome leggibile", valore = nome di interfaccia pcap (su Linux: "eth0", "enp3s0"...)
        std::string key = std::to_string(idx) + ": " + name;
        cards[key] = d->name ? std::string(d->name) : std::string();  // mai dereferenziare NULL
        ++idx;
    }

    pcap_freealldevs(alldevs);
    return cards;  // ← ora ritorni la mappa riempita
};

/// \brief Send DCP Identify request and run a capture loop to collect responses.
/// \details Opens the selected NIC in promiscuous mode, installs 0x8892 BPF, sends frame,
/// enables nonblocking mode and runs a bounded pcap_loop. Closes the handle on exit.
int profinet::PcapClient::identifyAll()
{   
    int pcap_res;
    auto time_out_Comm = steady_clock::now() + seconds(5); 
    const char* card = net_card.c_str();
    live_process = pcap_open_live(card,65535,1,500,errbuf);
    _set_filter(live_process);
    mac_addr=_get_mac();
    
    auto tmp = packageHelper::build_DCP(&mac_addr,XID);
   
    u_char* dcp_req = reinterpret_cast<u_char*>(tmp.data());
    
    int len = tmp.size();
    for(auto i: tmp) printf("%02X ",i); std::cout<<"\n";
    if(pcap_sendpacket(live_process,dcp_req,len)!=0 )
    {
        std::cerr<<"No packed has been sent for general reason"<< pcap_geterr(live_process)<<"\n";
        return pcap_res;
    }
    else
    {   
        std::cout << "package sent\n";
        int pkg_nr;
        auto objs = std::vector<profinet::DCP_Device>();
        auto loop_handler = profinet::Sniffer(&objs);
        pcap_setnonblock(live_process, 1, errbuf);

        loop_handler.start(live_process);
        pcap_breakloop(live_process);

        std::cout<<"exit Loop\n";
    }
    pcap_close(live_process);
    
    return 0;
};

/// \brief Return cached NIC map.
std::map<std::string,std::string> profinet::PcapClient::get_cards(){ return net_cards;}

/// \brief Set active NIC (pcap adapter string).
void profinet::PcapClient::set_card(std::string in){ net_card = in;}

/// \brief Install BPF filter "ether proto 0x8892".
void profinet::PcapClient:: _set_filter(pcap_t* process)
{
    bpf_program prg{};
    const char* filter= "ether proto 0x8892";

    if (pcap_compile(process, &prg, filter, 1, netmask) != PCAP_ERROR )
    {
        if(pcap_setfilter(process, &prg) == PCAP_ERROR ) std::cerr<<"Error raiser on network filter"<< pcap_geterr(process)<<"\n";
    }
    else std::cerr<<"Something wrong in the network filter"<< pcap_geterr(process)<<"\n";
    pcap_freecode(&prg);
}

/// \brief Get discovered DCP devices (if discovery ran).
std::optional<std::vector<profinet::DCP_Device>> profinet::PcapClient::get_devices(){return devices;}

/// \brief Optional inline packet processor (unused for now).
void profinet::PcapClient::_package_process(u_char* user, const pcap_pkthdr* h, const u_char* bytes) 
{

}

/* ---------------- Frame builder ---------------- */

/// \brief Build a 60-byte PN-DCP Identify request Ethernet frame.
std::array<uint8_t,60> packageHelper::build_DCP(std::array<uint8_t,6>* mac_address,std::array<uint8_t,4>& XID)
{
    std::array<uint8_t,frame_len> frame{};
    int idx=0;
       
    _build_ETH_header(frame,idx,mac_address);
    _build_DCP_header(frame,idx,XID);
    _build_DCP_message(frame,idx);
    if(idx<60)for(int i = idx;i==frame_len;i++) frame[i]=0x00;


    return frame;
}

/// \brief Fill destination multicast, source MAC and EtherType (0x8892).
void packageHelper::_build_ETH_header(std::array<uint8_t,frame_len>& frame,int& idx,std::array<uint8_t,6>* mac_address)
{
    // Destination MAC ADDRESS FF:FF:FF:FF:FF:FF broadcast Ethernet
    /*
    frame[idx++] = 0xff; frame[idx++] = 0xff;
    frame[idx++] = 0xff; frame[idx++] = 0xff; 
    frame[idx++] = 0xff; frame[idx++] = 0xff;
    */
    // Destination MAC ADDRESS 01:0E:CF:00:00:00 broadcast Profinet
    frame[idx++] = 0x01; frame[idx++] = 0x0e;
    frame[idx++] = 0xcf; frame[idx++] = 0x00; 
    frame[idx++] = 0x00; frame[idx++] = 0x00;

    // Requester MAC ADDRESS
    for(auto i : *mac_address) frame[idx++] = i;
    
    // EtherType x8892 on Profinet Network
    frame[idx++] = 0x88;
    frame[idx++] = 0x92;
}

/// \brief Fill PN-DCP Identify header (FrameID 0xFEFE, ServiceID 0x05).
void packageHelper::_build_DCP_header(std::array<uint8_t,frame_len>& frame,int& idx,std::array<uint8_t,4>& XID)
{
     
    
    // Frame ID
    frame[idx++] = 0xfe; frame[idx++] = 0xfe;

    // Service ID           //Service Type
    frame[idx++] = 0x05;    frame[idx++] = 0x00;
    
    // Transaction ID random
    int n;
    srand(time(NULL));
    for(int i =0;i<=3;i++)
    {   
        n = rand();
        frame[idx++] = static_cast<uint8_t>(n);
        XID[i] = static_cast<uint8_t>(n);
    } 

    // ResponseDelay
    frame[idx++] = 0x00; frame[idx++] = 0x00;
    
    // DCPDataLength
    frame[idx++] = 0x00;frame[idx++] = 0x04;

}

/// \brief Append DCP option/suboption blocks for Identify (All-Selector).
void packageHelper::_build_DCP_message(std::array<uint8_t,frame_len>& frame,int& idx)
{
    // Options                          
    frame[idx++] = 0xff;    
    // Suboption
    frame[idx++] = 0xff;
    // Blocklen
    frame[idx++] = 0x00;  
    frame[idx++] = 0x00; 
    
}

/* ---------------- Sniffer ---------------- */

/// \brief Ctor: store external results vector pointer.
profinet::Sniffer::Sniffer(std::vector<profinet::DCP_Device>* _objs)
    :objs(_objs){}

/// \brief pcap callback trampoline -> onPacket().
void profinet::Sniffer::pcap_cb(u_char* user, const pcap_pkthdr* h, const u_char* bytes) 
{
    auto* self = reinterpret_cast<Sniffer*>(user);
    self->onPacket(h, bytes);
}

/// \brief Start pcap loop (bounded by packet count/time).
void profinet::Sniffer::start(pcap_t* handle) 
{
    handle_ = handle;
    int pkg_nr = pcap_loop(handle_, 800, &Sniffer::pcap_cb, reinterpret_cast<u_char*>(this));
    std::cout << "packet: "<<std::to_string(pkg_nr)<<" looping\n";
}

/// \brief Per-packet parser: decode DCP and append device if new/PLC-like.
void profinet::Sniffer::onPacket(const pcap_pkthdr* h, const u_char* bytes) 
{
    std::vector<uint8_t> data(bytes, bytes + h->caplen);
        
    for(auto i: data)  {
        printf("%02X ",i);
    }

    std::cout<<"\n";
    /*
    if(! data.size() < 60)
    {
        auto obj =profinet::DCP_Device::create(h->caplen,bytes);
        if(obj.ip.has_value()) std::cout<<obj.ip.value()<<" ";
        if(obj.MAC.has_value()) std::cout<<obj.MAC.value()<<" ";
        if(obj.StationName.has_value()) std::cout<<obj.StationName.value()<<" ";
        if(obj.Familiy.has_value()) std::cout<<obj.Familiy.value()<<" ";
        std::cout<<"somthing\n";
    }
    
    
    bool any;

    if(objs->size()>0) for(int i = 0; i < objs->size() ;i++) 
    {
        profinet::DCP_Device& el = objs->at(i);
        if(el.Familiy.has_value()&& obj.Familiy.has_value()) any |= el.Familiy.value() == obj.Familiy;
        if(el.MAC.has_value()&& obj.MAC.has_value()) any |= el.MAC.value() == obj.MAC;
        if(el.ip.has_value()&& obj.ip.has_value()) any |= el.ip.value() == obj.Familiy;
        if(el.StationName.has_value()&& obj.StationName.has_value()) any |= el.StationName.value() == obj.StationName;
    }
    if(!any&&obj.isPLC())
    objs->push_back(obj);*/
}

/// \brief Number of processed packets.
size_t profinet::Sniffer::get_packet_nr(){return packets_;} 


/* ---------------- DCP_Device ---------------- */

/// \brief Heuristic PLC detection from stored fields.
bool profinet::DCP_Device::isPLC()
{
    bool hasPlc;
    bool hasValues = Familiy.has_value()&&ip.has_value();
    if(Familiy.has_value()) hasPlc=to_lowercase(Familiy.value()).find("plc");

    return hasPlc||hasValues;
}

class TLV
{
    private:
        std::string option;
        std::string sub_option;
        int length;
        std::vector<uint8_t> bytes_;
        std::string body;
    public:
        static TLV create(std::vector<uint8_t> btyes,int len)
        {
            
        }

};

/// \brief Parse a captured PN-DCP reply buffer into a DCP_Device.
profinet::DCP_Device profinet::DCP_Device::create(int len,const u_char* package)
{

    auto self = profinet::DCP_Device();
    const int _lenght = len;
    std::vector<uint8_t> data(package, package + len);
	std::string hexString =std::to_string(data[24])+std::to_string(data[25]);
	
    int tlvs_len=std::stoi(hexString, nullptr, 16);

	int starting_block = len-tlvs_len;
	
	if(starting_block!=26)return;
    
	std::vector<uint8_t> tlvs(data[starting_block],data[starting_block+tlvs_len]);

	while( tlvs.size()>0)
    {
		TLV tlv =TLV::create(tlvs,TLVS);
		res=tlv.process()
		padding = 0 if tlv.length %2 == 0 else 1
		for i in range(0,4+tlv.length+padding):
			tlvs.pop(0)
		try:
			res = bytes.fromhex(res).decode("utf-8")
		except:
			pass

		if("plc".lower() in res.lower()):
			print(res)
}
    return self;
}
