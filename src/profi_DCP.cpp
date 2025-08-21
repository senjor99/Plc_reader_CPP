#include <profi_DCP.hpp>
#include <sys/types.h>
#include <random>
#include <cstdint>
#include <stdexcept>
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")

/*-------------------------------------------------------*/
/*------------------ Class Pcap Client ------------------*/ 
/*-------------------------------------------------------*/

profinet::PcapClient::PcapClient()
// Class constructor, just create the instance and rectruit network card information, storing them
// it's necessary to know the network card for send and receive package 
    : net_cards(_get_netCards()){};

std::vector<std::string> profinet::PcapClient::_get_netCards()
// Use the Pcap API to receive the network card as pcap_if_t, a pcap class, then you explore 
// it by the score_interface function
{   
    pcap_if_t* pcap_cards = new pcap_if_t;

    if (pcap_findalldevs_ex(PCAP_SRC_IF_STRING,NULL,&pcap_cards,errbuf) != 0 || !pcap_cards)
        throw std::runtime_error("No network device found\n");
    std::vector<std::string> cards;
    for (pcap_if_t* d = pcap_cards; d; d = d->next) 
       net_cards.push_back(static_cast<std::string>(d->name));
    
    pcap_freealldevs(pcap_cards);
    return cards;
};

std::string profinet::PcapClient::extract_guid() 
{
//
//
    if (net_card == "") return {};
    const char* n = net_card.c_str();
    const char* lb = std::strchr(n, '{');
    const char* rb = lb ? std::strchr(lb, '}') : nullptr;
    if (!lb || !rb || rb <= lb) return {};
    return std::string(lb, rb - lb + 1); 
}

std::array<uint8_t,6> profinet::PcapClient::_get_mac()
//
//
{
    if (net_card == "") throw std::exception("WARNING no network card found");

    std::string guid = extract_guid();
    if (guid.empty()) throw std::exception("WARNING no GUID could be found on the network card");

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
    if (ret != NO_ERROR) throw std::exception("WARNING MAC Addres not found on the network card");


    for (IP_ADAPTER_ADDRESSES* it = aa; it; it = it->Next) {
        if (!it->AdapterName) continue;
        if (guid == it->AdapterName) {
            if (it->PhysicalAddressLength >= 6) {
                std::array<uint8_t,6> mac{};
                std::memcpy(mac.data(), it->PhysicalAddress, 6);
                return mac;
            }
        }
    }
    throw std::exception("WARNING MAC Addres search got no results");
}
 
void profinet::PcapClient::identifyAll()
// Internal class of the client, it get the package, build it with the package_helper, send it and wait for an answer 
// process the answers and store them in the internal variable packages, a list who must need to be popped by the outside program 
{   
    
    const char* card = net_card.c_str();
    live_process = pcap_open_live(card,65535,0,500,errbuf);
    _set_filter(live_process);

    auto tmp = packageHelper::build_DCP(&mac_addr);
    u_char* dcp_req = reinterpret_cast<u_char*>(tmp.data());
    int len = tmp.size();
    
    if(pcap_sendpacket(live_process,dcp_req,len)==PCAP_ERROR ) 
        std::cerr<<"No packed has been sent for general reason"<< pcap_geterr(live_process)<<"\n";
    else
    {   
        u_char x[] = "netsurfer";
        int pkg_nr;
        pcap_handler callback;
        while(pkg_nr <10)
        {
            pkg_nr = pcap_loop(live_process, -1, callback , x);
        }
        pcap_breakloop(live_process);
        _package_process(x,&callback);
    }
    pcap_close(live_process);
};

std::vector<std::string> profinet::PcapClient::get_cards(){ return net_cards;}
void profinet::PcapClient::set_card(std::string in){ net_card = in;}

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

void profinet::PcapClient::_package_process(u_char *user,pcap_handler* handler)
//
//
{

}

/*-------------------------------------------------------*/
/*---------------- Class Package builder ----------------*/ 
/*-------------------------------------------------------*/

std::array<uint8_t,60> packageHelper::build_DCP(std::array<uint8_t,6>* mac_address)
//
//
{
    std::array<uint8_t,frame_len> frame{};
    int idx=0;

    _build_ETH_header(frame,idx,mac_address);
    _build_DCP_header(frame,idx);
    _build_DCP_message(frame,idx);
    for(int i = idx;i<frame_len;i++) frame[i]=0x00;

    return frame;
}

void packageHelper::_build_ETH_header(std::array<uint8_t,frame_len>& frame,int& idx,std::array<uint8_t,6>* mac_address)
//
//
{
    // Destination MAC ADDRESS FF:FF:FF::FF per broadcast
    frame[idx++] = 0xff; frame[idx++] = 0xff;
    frame[idx++] = 0xff; frame[idx++] = 0xff; 
    frame[idx++] = 0xff; frame[idx++] = 0xff;

    // Requester MAC ADDRESS
    for(auto i : *mac_address) frame[idx++] = i;
    
    // EtherType
    frame[idx++] = 0x88;
    frame[idx++] = 0x92;
}

void packageHelper::_build_DCP_header(std::array<uint8_t,frame_len>& frame,int& idx)
//
//
{
    srand(static_cast<unsigned>(time(nullptr))); // seed basato sul tempo
    
    // Frame ID
    frame[idx++] = 0xfe; frame[idx++] = 0xfe;

    // Service ID           //Service Type
    frame[idx++] = 0x05;    frame[idx++] = 0x00;
    
    // Transaction ID
    for(int i =0;i>4;i++)
    {
        int n = rand() % 10;
        frame[idx++] = static_cast<uint8_t>(n & 0xFF);;
    } 

    // ResponseDelay
    frame[idx++] = 0x00; frame[idx++] = 0x00;
    
    // DCPDataLength
    frame[idx++] = 0x00;frame[idx++] = 0x04;

}

void packageHelper::_build_DCP_message(std::array<uint8_t,frame_len>& frame,int& idx)
//
//
{
    // Options              // Suboption            // Blocklen
    frame[idx++] = 0xff;    frame[idx++] = 0xff;    frame[idx++] = 0xff; 
    
}

int score_interface(pcap_if_t* d) 
//
//
{
    // Hard exclude
    if (d->flags & PCAP_IF_LOOPBACK) return INT_MIN/2;
    if (profinet::looks_loopback(d) || profinet::looks_virtual_or_vpn(d)) return INT_MIN/2;

    int s = 0;
    if (profinet::looks_wifi(d)) s -= 2;         
    if (profinet::has_ipv4(d)) s += 2;          
    // bonus parole “buone”
    std::string desc = d->description ? d->description : "";
    if (desc.find("Ethernet") != std::string::npos) s += 2;
    if (desc.find("Intel")    != std::string::npos) s += 1;
    if (desc.find("Realtek")  != std::string::npos) s += 1;

    // Prova rapida: controlla DLT
    char errbuf[PCAP_ERRBUF_SIZE]{};
    if (pcap_t* h = pcap_open_live(d->name, 128, 0, 10, errbuf)) {
        if (pcap_datalink(h) == DLT_EN10MB) s += 2;
        pcap_close(h);
    }

    return s;
}
