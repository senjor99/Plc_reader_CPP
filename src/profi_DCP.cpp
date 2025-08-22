#include <profi_DCP.hpp>
#include <sys/types.h>
#include <misc.h>
#include <chrono>
#include <iomanip>
#include <sstream>
using namespace std::chrono;

#pragma comment(lib, "iphlpapi.lib")

/*-------------------------------------------------------*/
/*------------------ Class Pcap Client ------------------*/ 
/*-------------------------------------------------------*/

profinet::PcapClient::PcapClient()
// Class constructor, just create the instance and rectruit network card information, storing them
// it's necessary to know the network card for send and receive package 
    : net_cards(_get_netCards()){};

std::map<std::string,std::string> profinet::PcapClient::_get_netCards()
// Use the Pcap API to receive the network card as pcap_if_t, a pcap class, then you explore 
// it by the score_interface function
{   
    pcap_if_t* pcap_cards = new pcap_if_t;

    if (pcap_findalldevs_ex(PCAP_SRC_IF_STRING,NULL,&pcap_cards,errbuf) != 0 || !pcap_cards)
        throw std::runtime_error("No network device found\n");
    std::map<std::string,std::string> cards;
    int ii=0;
    std::string name;
    for (pcap_if_t* d = pcap_cards; d; d = d->next) 
    {
        name =d->description;
        if(name.find("'"))
        {
            name = name.substr(name.find("'")+1,name.size());
            name = name.substr(0,name.find("'"));
        }
        name = std::to_string(ii)+": "+name;
        net_cards[name] = static_cast<std::string>(d->name);
        ii++;
    }
    pcap_freealldevs(pcap_cards);
    return cards;
};

std::string profinet::PcapClient::extract_guid() 
{
//
//
    if (net_card == "") return {};
    const char* n = net_card.c_str();
    const char* lb = strchr(n, '{');
    const char* rb = lb ? strchr(lb, '}') : nullptr;
    if (!lb || !rb || rb <= lb) return {};
    return std::string(lb, rb - lb + 1); 
}

std::array<uint8_t,6> profinet::PcapClient::_get_mac()
//
//
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
 
int profinet::PcapClient::identifyAll()
// Internal class of the client, it get the package, build it with the package_helper, send it and wait for an answer 
// process the answers and store them in the internal variable packages, a list who must need to be popped by the outside program 
{   
    int pcap_res;
    auto time_out_Comm = steady_clock::now() + seconds(5); 
    const char* card = net_card.c_str();
    live_process = pcap_open_live(card,65535,1,500,errbuf);
    //_set_filter(live_process);
    mac_addr=_get_mac();
    auto tmp = packageHelper::build_DCP(&mac_addr);
    u_char* dcp_req = reinterpret_cast<u_char*>(tmp.data());
    int len = tmp.size();
        
    if(pcap_res=pcap_sendpacket(live_process,dcp_req,len)==PCAP_ERROR )
    {
        std::cerr<<"No packed has been sent for general reason"<< pcap_geterr(live_process)<<"\n";
        return pcap_res;
    }
    else
    {   
        std::cout << "package sent\n";
        int pkg_nr;
        profinet::Sniffer loop_handler = profinet::Sniffer();
        bool time =   steady_clock::now()<time_out_Comm && loop_handler.get_packet_nr() != PCAP_ERROR;
        bool packet = loop_handler.get_packet_nr() <=10;
        bool error = loop_handler.get_packet_nr() != PCAP_ERROR;

        pcap_setnonblock(live_process, 1, errbuf);

        std::cout<<"time: "<<time<<" packet "<<packet<<" error "<<error<<" qty: "<<loop_handler.get_packet_nr()<<" \n";
        //while(loop_handler.get_packet_nr() <=10 && steady_clock::now()<time_out_Comm && loop_handler.get_packet_nr() != PCAP_ERROR)
        //{
            //std::cout << "packet: "<<std::to_string(pkg_nr)<<" looping\n";
            loop_handler.start(live_process);
        //}
        pcap_breakloop(live_process);
        std::cout<<"exit Loop\n";
    }
    pcap_close(live_process);
    return 0;
};

std::map<std::string,std::string> profinet::PcapClient::get_cards()
//
//
{ return net_cards;}

void profinet::PcapClient::set_card(std::string in)
//
//
{ net_card = in;}

void profinet::PcapClient:: _set_filter(pcap_t* process)
//
//
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

std::optional<std::vector<profinet::DCP_Device>> profinet::PcapClient::get_devices()
//
//
{return devices;}

void profinet::PcapClient::_package_process(u_char* user, const pcap_pkthdr* h, const u_char* bytes) 
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
    if(idx<60)for(int i = idx;i==frame_len;i++) frame[i]=0x00;

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
        int n = rand() % 99;
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

/*-------------------------------------------------------*/
/*------------------ Pcap Loop Handler ------------------*/ 
/*-------------------------------------------------------*/

void profinet::Sniffer::start(pcap_t* handle) 
//
//
{
    handle_ = handle;
    int pkg_nr = pcap_loop(handle_, 800, &Sniffer::pcap_cb, reinterpret_cast<u_char*>(this));
    std::cout << "packet: "<<std::to_string(pkg_nr)<<" looping\n";
}


void profinet::Sniffer::pcap_cb(u_char* user, const pcap_pkthdr* h, const u_char* bytes) 
//
//
{
    auto* self = reinterpret_cast<Sniffer*>(user);
    self->onPacket(h, bytes);
}

// --- helpers -------------------------------------------------
static inline std::string hex_bytes(const u_char* p, size_t n, bool with_spaces=true) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < n; ++i) {
        oss << std::setw(2) << unsigned(p[i]);
        if (with_spaces && i+1 < n) oss << ' ';
    }
    return oss.str();
}
static inline uint16_t rd16be(const u_char* p){ return (uint16_t(p[0])<<8) | p[1]; }

// --- 1) dump intero frame -----------------------------------
static inline void dump_dcp_frame_raw(const pcap_pkthdr* h, const u_char* bytes) {
    printf("LEN=%u cap=%u\n", h->len, h->caplen);
    // tutto il frame in righe da 16
    for (unsigned i=0; i<h->caplen; i+=16) {
        unsigned chunk = (i+16 <= h->caplen) ? 16 : (h->caplen - i);
        printf("%04X: %s\n", i, hex_bytes(bytes + i, chunk, true).c_str());
    }
}

// --- 2) dump solo blocchi DCP (TLV) -------------------------
static inline void dump_dcp_blocks_raw(const pcap_pkthdr* h, const u_char* bytes) {
    //if (h->caplen < 26) return;
    if (!(bytes[12]==0x88 && bytes[13]==0x92)) return; // non DCP/PNIO

    uint16_t dataLen = rd16be(bytes + 24);
    unsigned tlv_start = 26;
    unsigned tlv_end   = tlv_start + dataLen;
    if (tlv_end > h->caplen) tlv_end = h->caplen;

    // a) stringa unica SENZA spazi (solo area TLV)
    std::string packed = hex_bytes(bytes + tlv_start, tlv_end - tlv_start, /*with_spaces=*/false);
    //printf("DCP_TLV_RAW(no-spaces): %s\n", packed.c_str());

    // b) anche per-blocco: [opt/sub len] data(hex)
    unsigned idx = tlv_start;
    while (idx + 4 <= tlv_end) {
        uint8_t opt = bytes[idx+0];
        uint8_t sub = bytes[idx+1];
        uint16_t bl = rd16be(bytes + idx + 2);
        idx += 4;

        unsigned blk_end = (idx + bl <= tlv_end) ? (idx + bl) : tlv_end;
        std::string blk = hex_bytes(bytes + idx, blk_end - idx, false);

        printf("%s\n", blk.c_str());

        idx = blk_end;
        if (bl & 1) { // padding a 2 byte
            if (idx < tlv_end) idx += 1;
        }
    }
}

void profinet::Sniffer::onPacket(const pcap_pkthdr* h, const u_char* bytes) 
//
//
{
    // dump intero frame (comodo per capire layout)
    //dump_dcp_frame_raw(h, bytes);

    // dump solo l’area DCP TLV (raw no-spaces + per-blocco)
    dump_dcp_blocks_raw(h, bytes);

}



size_t profinet::Sniffer::get_packet_nr()
//
//
{return packets_;} 
