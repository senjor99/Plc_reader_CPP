#include <profi_DCP.hpp>
#include <sys/types.h>

profinet::PcapClient::PcapClient()
    :net_cards(new pcap_if_t),net_card(get_netCards()){}

pcap_if_t profinet::PcapClient::get_netCards()
{
    
    if (pcap_findalldevs_ex(PCAP_SRC_IF_STRING,NULL,&net_cards,errbuf) != 0 || !net_cards)
        throw std::runtime_error("No netword device found\n");

    pcap_if_t* best = nullptr; int bestScore = INT_MIN;
    for (pcap_if_t* d = net_cards; d; d = d->next) {
        int s = score_interface(d);
        if (s > bestScore) { bestScore = s; best = d; }
    }
    return *best;
};

bool profinet::PcapClient::_SendPackage()
{
    u_char dcp_req[60] ;
    profinet::package_helper(dcp_req,60);

    if(pcap_sendpacket(handle,dcp_req,60)!=0)
        std::cerr<<"No packed has been sent for general reason\n";
    else
        
        _waitAnswer()

};

bool profinet::PcapClient::_waitAnswer()
{
    handle = pcap_open_live(net_card.name,65535,0,500,errbuf);
    if(handle != NULL)
    {
        _package_process(handle)
        return true;
    }
    else return false;
}
void profinet::PcapClient::_package_process()
{
    
}

void profinet::PcapClient::_package_build()
{
    profinet::package_helper();
    pcap_open(pcap_t *p);
    pcap_compile();
    pcap_close(pcap_t *p);
}

void profinet::package_helper(u_char* a,int len)
{
    u_char res;
     profinet::_build_header(res);
     profinet::_build_message(res);
    
    return res;
}

void profinet::_build_header(u_char* a,int len)
{
    pcap_pkthdr header;
}

void profinet::_build_message(u_char* a,int len)
{
    
}

int score_interface(pcap_if_t* d) {
    // Hard exclude
    if (d->flags & PCAP_IF_LOOPBACK) return INT_MIN/2;
    if (profinet::looks_loopback(d) || profinet::looks_virtual_or_vpn(d)) return INT_MIN/2;

    int s = 0;
    if (profinet::looks_wifi(d)) s -= 2;         // preferisci non-wifi
    if (profinet::has_ipv4(d)) s += 2;           // segnale di attività
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

/*
pcap_if_t        // lista interfacce
pcap_t           // handle verso l'interfaccia
struct pcap_pkthdr {   // header pacchetto ricevuto
    struct timeval ts;
    bpf_u_int32 caplen;
    bpf_u_int32 len;
};


pcap_t *pcap_open_live(
    const char *device,   // nome interfaccia
    int snaplen,          // max bytes da catturare (es. 65535)
    int promisc,          // 1 = promiscuous
    int to_ms,            // timeout in ms
    char *errbuf
);


int pcap_sendpacket(
    pcap_t *p,
    const u_char *buf,    // puntatore al pacchetto
    int size              // lunghezza pacchetto
);

const u_char *pcap_next(
    pcap_t *p,
    struct pcap_pkthdr *h
);

int pcap_next_ex(
    pcap_t *p,
    struct pcap_pkthdr **pkt_header,
    const u_char **pkt_data
);

int pcap_compile(
    pcap_t *p,
    struct bpf_program *fp,
    const char *str,
    int optimize,
    bpf_u_int32 netmask
);

int pcap_setfilter(pcap_t *p, struct bpf_program *fp);

char *s(char *errbuf);
int pcap_lookupnet(
    const char *device,
    bpf_u_int32 *netp,
    bpf_u_int32 *maskp,
    char *errbuf
);

const char *pcap_geterr(pcap_t *p);

char *pcap_lookupdev(char *errbuf);
int pcap_lookupnet(
    const char *device,
    bpf_u_int32 *netp,
    bpf_u_int32 *maskp,
    char *errbuf
);

const char *pcap_geterr(pcap_t *p);

*/