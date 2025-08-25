
#include <profi_DCP.hpp>

#ifdef _WIN32
    #include <misc.h>
#endif

using namespace std::chrono;

#pragma comment(lib, "iphlpapi.lib")

/* ---------------- Platform-specific utilities ---------------- */

#ifdef _WIN32

/// \brief Windows: extract GUID from selected pcap adapter string.
/// \return "{...}" GUID substring or empty string if not found.
std::string profinet::PcapClient::_extract_guid() 
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

    std::string guid =_extract_guid();
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
    std::string profinet::PcapClient::_extract_guid()
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
    : net_cards(_get_netCards()),devices(std::make_shared<std::vector<profinet::DCP_Device>>()),lock(false){};

/// \brief Enumerate NICs and build "index: label" -> pcap name map.
/// \throws std::runtime_error if pcap_findalldevs fails.
std::map<std::string,std::string> profinet::PcapClient::_get_netCards()
{
    pcap_if_t* alldevs = nullptr;
    std::map<std::string, std::string> cards;

    #ifdef _WIN32
        // Windows
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
    const char* card = net_card.c_str();
    live_process = pcap_open_live(card,65535,1,500,errbuf);
    time_out= std::chrono::steady_clock::now()+seconds(120) ;
    mac_addr=_get_mac();
    
    auto tmp = packageHelper::build_DCP(&mac_addr,XID);
    _set_filter(live_process);
   
    u_char* dcp_req = reinterpret_cast<u_char*>(tmp.data());
    
    int len = tmp.size();

    if(pcap_sendpacket(live_process,dcp_req,len)!=0 )
    {
        std::cerr<<"No packed has been sent error: "<< pcap_geterr(live_process)<<"\n";
        return pcap_res;
    }
    else
    {   
        std::cout<<"sent frame, len: "<<std::to_string(len)<<"\n";
        parser = profinet::PackageParser(devices,&lock,live_process);
        pcap_setnonblock(live_process, 1, errbuf);
    }    
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
    char filter[512];
    std::snprintf
    (
        filter, sizeof(filter),  
        "ether proto 0x8892 and ("
        "ether[18] = 0x%02X and ether[19] = 0x%02X and ether[20] = 0x%02X and ether[21] = 0x%02X)",
        XID[0],XID[1],XID[2],XID[3]
    );

    if (pcap_compile(process, &prg, filter, 1, PCAP_NETMASK_UNKNOWN ) != PCAP_ERROR )
    {
        if(pcap_setfilter(process, &prg) == PCAP_ERROR ) 
            std::cerr<<"Error raiser on network filter"<< pcap_geterr(process)<<"\n";
    }
    else 
        std::cerr<<"Something wrong in the network filter"<< pcap_geterr(process)<<"\n";

    pcap_freecode(&prg);
}

/// \brief Get discovered DCP devices (if discovery ran).
const std::shared_ptr<std::vector<profinet::DCP_Device>> profinet::PcapClient::get_devices()
{
    if(time_out.has_value()&&parser.has_value())
    {
        if(std::chrono::steady_clock::now()>time_out.value()) pcap_close(live_process);
        parser.value().start();
    }
    return devices;
}

const bool profinet::PcapClient::get_lock()const{ return lock;};

/* ---------------- Frame builder ---------------- */

/// \brief Build a 60-byte PN-DCP Identify request Ethernet frame.
std::array<uint8_t,60> packageHelper::build_DCP(std::array<uint8_t,6>* mac_address,std::array<uint8_t,4>& XID)
{
    std::array<uint8_t,frame_len> frame{};
    int idx=0;
       
    _build_ETH_header(frame,idx,mac_address);
    _build_DCP_header(frame,idx,XID);
    _build_DCP_message(frame,idx);
    if(idx<60)for(int i = idx;i<frame_len;i++) frame[i]=0x00;


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
    static std::mt19937 rng(std::random_device{}());
    
    for (int i = 0; i < 4; ++i) 
    {
        uint8_t r = std::uniform_int_distribution<int>(0,255)(rng);
        frame[idx++] = r; XID[i] = r;
    }

    // ResponseDelay
    frame[idx++] = 0x00; frame[idx++] = 0x80;
    
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
profinet::PackageParser::PackageParser(std::shared_ptr<std::vector<profinet::DCP_Device>> _objs,bool* _lock,pcap_t* handle)
    :objs(_objs),lock(_lock),handle_(handle){}

    /// \brief pcap callback trampoline -> onPacket().
void profinet::PackageParser::pcap_cb(u_char* user, const pcap_pkthdr* h, const u_char* bytes) 
{
    auto* self = reinterpret_cast<PackageParser*>(user);
    self->onPacket(h, bytes);
}

/// \brief Per-packet parser: decode DCP and append device if new/PLC-like.
void profinet::PackageParser::onPacket(const pcap_pkthdr* h, const u_char* bytes) 
{
    *lock = true;
    auto dev = profinet::DCP_Device::create(h->caplen,bytes);
    bool exists  = false;
    if(dev.has_value()&&dev.value().isPLC()) 
    {
        if(objs->size()>1)
            for(size_t i = 0 ;i < objs->size();i++)
                if( objs->at(i).ip.value().get_ip()==dev.value().ip.value().get_ip())
                {
                    exists = true;
                    break;
                }
        
        if(!exists) 
        {
            objs->push_back(dev.value());
            std::sort(objs->begin(), objs->end(),
              [](const DCP_Device& a, const DCP_Device& b) {
                  return a.StationName.value() < b.StationName.value(); // lexicografico
              });
        }
    }
    *lock = false;
}

/// \brief Start pcap loop (bounded by packet count/time).
void profinet::PackageParser::start() 
{
    const int pkg_target = 64;
    packets_ = pcap_dispatch(handle_, pkg_target, &PackageParser::pcap_cb,reinterpret_cast<u_char*>(this));
}




/* ---------------- DCP_Device ---------------- */

/// \brief Heuristic PLC detection from stored fields.
bool profinet::DCP_Device::isPLC(){ return Family.has_value() && Family.value().find("S7") != Family.value().npos; }

/// \brief Get TLV class and base on it populate DCP_Device information.
void profinet::DCP_Device::add_TLV(TLV tlv)
{
    if(tlv.get_option() == 0x02) 
        switch (tlv.get_suboption())
        {
            case 0x01:
                Family =  std::string(tlv.get_body().begin(), tlv.get_body().end());
                break;
            case 0x02:
                StationName  = std::string(tlv.get_body().begin(), tlv.get_body().end());
                StationName  = StationName.value().substr(0,StationName.value().find("."));
                break;
            
            default:
                break;
        }
    if(tlv.get_option() == 0x01)
        switch (tlv.get_suboption())
            {
            case 0x02:
                ip = profinet::IPParams::create(tlv.get_body());
                break;
            
            default:
                break;
            }
    else return;
};

/// \brief Parse a captured PN-DCP reply buffer and return a DCP_Device.
std::optional<profinet::DCP_Device> profinet::DCP_Device::create(int len,const u_char* package)
{
    
    std::vector<uint8_t> data(package, package + len);   
    
    

    int tlvs_len = static_cast<int>((data[24] << 8) | data[25]);
    int starting_block = len-tlvs_len;

    if(starting_block!=26)return std::nullopt;

	std::vector<uint8_t> tlvs(data.begin()+starting_block,data.begin()+starting_block+tlvs_len);

    auto self = profinet::DCP_Device();
	while( tlvs.size()>0)
    {
		std::optional<TLV> tlv =TLV::create(tlvs);
		if(tlv.has_value())
        {    
            int padding = static_cast<int>(tlv.value().get_len())  %2 == 0  ? 0 : 1;
            tlvs.erase(tlvs.begin(),tlvs.begin()+4+tlv.value().get_len()+padding);
            if(tlv.has_value()) self.add_TLV(tlv.value());
        }
        else
        {
            std::cerr<<"Something wrong on tlv creation";
            return std::nullopt;
        }
    }

    return self;
}

/* ---------------- TLV ---------------- */

/// \brief Simple TLV constructor.
profinet::TLV::TLV(uint8_t opt, uint8_t sub_opt,int len)
    :option(opt),sub_option(sub_opt),len(len){};
        
/// \brief Return TLV Option.
uint8_t profinet::TLV::get_option(){return option;}

/// \brief Return TLV Sub-Option.
uint8_t profinet::TLV::get_suboption(){return sub_option;}

/// \brief Return TLV body length.
int profinet::TLV::get_len(){return len;}
        
/// \brief Return TLV body parse in bytes.
const std::vector<uint8_t>& profinet::TLV::get_body(){ return body; }

/// \brief Parse a TLV from a vector of byte.
std::optional<profinet::TLV> profinet::TLV::create(std::vector<uint8_t>& bt)
{
    if(bt.size() < 4)return std::nullopt;
    
    int L = (static_cast<int>(bt[2]) << 8) | bt[3];
    
    TLV self = TLV(bt[0],bt[1],L);
    
    self.body.assign(bt.begin() + 6, bt.begin() + 4 + L);

    return self;
}
