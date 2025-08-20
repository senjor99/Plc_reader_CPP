#include <pcap.h>
#include "misc.h"
#include <datatype.hpp>

namespace profinet
{
    class DCP_Device
    {
        public:
        DCP_Device()=default;
        ~DCP_Device()=default;
        std::string ip;
        std::string MAC;
        std::string StationName;
        std::string Familiy;
        static DCP_Device create(std::map<std::string,std::string> dev);
    };

    class PcapClient
    {
        private:
            int comm;
            int payload;
            int package;
            
            pcap_t* handle;
            pcap_if_t* net_cards;
            pcap_if_t net_card;

            char errbuf[PCAP_ERRBUF_SIZE];
            
            bool _SendPackage();
            bool _waitAnswer();
            void _openSniffer();
            void _package_build();
            void _package_process();
            pcap_if_t get_netCards();

        public:
            PcapClient();
            ~PcapClient();
            std::vector<PcapClient> identifyAll(const int& comm,const int&payload,const int& package);
    };

    void package_helper(u_char* a,int len);

    void _build_header(u_char* a,int len);

    void _build_message(u_char* a,int len);

    struct IfScore 
    {
        pcap_if_t* dev;
        int score;
    };

    bool looks_loopback(const pcap_if_t* d);
    bool looks_virtual_or_vpn(const pcap_if_t* d);
    bool looks_wifi(const pcap_if_t* d);
    bool has_ipv4(const pcap_if_t* d);

};

