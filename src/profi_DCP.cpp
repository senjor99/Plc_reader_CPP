
#include <profi_DCP.hpp>
#include <sys/types.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <format>
#include <fstream>

#include <array>
#ifdef _WIN32
    #include <misc.h>
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
    //for(auto i: tmp) printf("%02X ",i); std::cout<<"\n";
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
    char filter[512];
    std::snprintf
    (
        filter, sizeof(filter),  
        "ether proto 0x8892 and ("
        "ether[18] = 0x%02X and ether[19] = 0x%02X and ether[20] = 0x%02X and ether[21] = 0x%02X)",
        XID[0],XID[1],XID[2],XID[3]
    );

    if (pcap_compile(process, &prg, filter, 1, netmask) != PCAP_ERROR )
    {
        if(pcap_setfilter(process, &prg) == PCAP_ERROR ) 
            std::cerr<<"Error raiser on network filter"<< pcap_geterr(process)<<"\n";
    }
    else 
        std::cerr<<"Something wrong in the network filter"<< pcap_geterr(process)<<"\n";

    pcap_freecode(&prg);
}

/// \brief Get discovered DCP devices (if discovery ran).
const std::vector<profinet::DCP_Device>* profinet::PcapClient::get_devices()const{return &devices;}

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
    auto dev = profinet::DCP_Device::create(h->caplen,bytes);
    if(dev.has_value()&&dev.value().isPLC()) objs->push_back(dev.value());
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
                StationName = std::string(tlv.get_body().begin(), tlv.get_body().end());
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
		std::optional<TLV> tlv =TLV::create(tlvs,len);
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
std::optional<profinet::TLV> profinet::TLV::create(std::vector<uint8_t>& bt,int len)
{
    if(bt.size() < 4)return std::nullopt;
    
    int L = (static_cast<int>(bt[2]) << 8) | bt[3];
    
    TLV self = TLV(bt[0],bt[1],L);
    
    self.body.assign(bt.begin() + 6, bt.begin() + 4 + L);

    return self;
}

/*----------------------------DEBUG-----------------------------*/
/*
static uint8_t hex_nibble(char c) {
    switch (c)
    {
    case '0' : return 0x0;
    case '1' : return 0x1;
    case '2' : return 0x2;
    case '3' : return 0x3;
    case '4' : return 0x4;
    case '5' : return 0x5;
    case '6' : return 0x6;
    case '7' : return 0x7;
    case '8' : return 0x8;
    case '9' : return 0x9;
    case 'A' : return 0xa;
    case 'B' : return 0xb;
    case 'C' : return 0xc;
    case 'D' : return 0xd;
    case 'E' : return 0xe;
    case 'F' : return 0xf;

    default:
        break;
    }
}

static std::pair<u_char*,int> hex_line_to_bytes(std::string line) 
{
    int idx = 0;
    const size_t len = line.size(); 
    std::array<u_char,500> res;
    for (int i = 0;i < line.size();i+=2) 
    {
        uint8_t first = hex_nibble(line[i]);
        uint8_t second = hex_nibble(line[i+1]);
        res[idx] = (first << 4 )| second;
        idx++;
    }
    u_char* res_ptr = res.begin();
    std::pair<u_char*,int> result(res_ptr,idx);
    return result;
}

int main()
{

    std::ifstream in(std::filesystem::current_path().string()+"/log.txt");
    if (!in) {
        std::cerr << "Non riesco ad aprire "<<std::filesystem::current_path().string()+"/log.txt"<<"\n";
        return 1;
    }

    std::string line;
    size_t line_no = 0;
    while (std::getline(in, line)) {

        ++line_no;



        try {
            // se serve il cast esplicito:
            const u_char* ptr = reinterpret_cast<const u_char*>(line.data());
            std::pair<u_char*,int> _line = hex_line_to_bytes(line);
            std::optional<profinet::DCP_Device> dev = profinet::DCP_Device::create(_line.second,_line.first);
            if(dev.has_value()&&dev.value().isPLC())
            {
                // TODO: stampa ciò che ti interessa
                if (dev.value().StationName.has_value())
                    std::cout << "Linea " << line_no << " StationName: " << dev.value().StationName.value() << "\n";
                if (dev.value().Family.has_value())
                    std::cout << "Linea " << line_no << " Family: " << dev.value().Family.value() << "\n";
                if (dev.value().ip.has_value())
                    std::cout << "Linea " << line_no << " IP: " << dev.value().ip.value().get_ip() << "\n";
                std::cout<<"\n";
            }   
        } catch (const std::exception& e) {
            std::cerr << "Linea " << line_no << ": errore parsing DCP: " << e.what() << "\n";
        }
    }

    return 0;
}
*/