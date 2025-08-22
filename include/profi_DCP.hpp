#pragma once

#include <datatype.hpp>
#include <random>
#include <cstdint>
#include <stdexcept>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <pcap.h>


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

    class Sniffer {
    public:
        void start(pcap_t* handle);
        size_t get_packet_nr();
        Sniffer() =default;

    private:
        static void pcap_cb(u_char* user, const pcap_pkthdr* h, const u_char* bytes) ;

        void onPacket(const pcap_pkthdr* h, const u_char* bytes) ;

        pcap_t* handle_ = nullptr;
        size_t  packets_ = 0;
    };

    class PcapClient
    {
        private:
            int comm;
            int payload;
            int package;
            
            pcap_t* live_process;
            pcap_handler handler;
            bpf_u_int32 netmask;
            std::string net_card = "";
            std::map<std::string,std::string> net_cards;
            std::array<uint8_t,6> mac_addr{};
            std::optional<std::vector<profinet::DCP_Device>> devices;

            char errbuf[PCAP_ERRBUF_SIZE];
            
            std::map<std::string,std::string> _get_netCards();
            std::string extract_guid();
            std::array<uint8_t,6> _get_mac();
            void _package_process(u_char* user, const pcap_pkthdr* h, const u_char* bytes);
            void _set_filter(pcap_t* process);

            public:
            PcapClient();
            ~PcapClient()=default;
            int identifyAll();
            std::optional<std::vector<profinet::DCP_Device>> get_devices();
            std::map<std::string,std::string> get_cards();
            void set_card(std::string in);
    };

    struct IfScore 
    {
        pcap_if_t* dev;
        int score;
    };

};

class packageHelper
{
    /*
    [ Ethernet ]
    FF FF FF FF FF FF   aa bb cc dd ee ff   88 92
    ---------------------------------------------
    0  1  2  3  4  5    6  7  8  9  10 10   12 13
    ^^^^^^^^^^^^^^^^^   ^^^^^^^^^^^^^^^^^   ^^^^^                     
    |                   |                   |
    |                   |                   Ether Type
    |                   |
    |                   Sender MAC Address
    |
    -Destination MAC Address, FF to broadcast 
                    
    [ PN‑DCP ]
    FE FE   05   00   12 34 56 78   00 00   00 04
    ---------------------------------------------
    14 15   16   17   18 19 20 21   22 23   23 24
    ^^^^^    ^    ^   ^^^^^^^^^^^   ^^^^^   ^^^^^
    |        |    |   |             |       |
    |        |    |   |             |       DCP data Len
    |        |    |   |             |       
    |        |    |   |             ResponseDelay      
    |        |    |   |        
    |        |    |   Transaction ID,Random
    |        |    |   
    |        |    Service Type
    |        |
    |        Service ID
    |        
    Frame ID

    [ DCP ]
    FF FF   00 00
    25 26   27 28

    FF  FF  00 00
    ^   ^   ^^^^^
    |   |   |
    |   |   Block len
    |   |   
    |   Suboption
    |   
    Option

    */
    public:

    static std::array<uint8_t,60> build_DCP(std::array<uint8_t,6>* mac_address);

    private:
        static const int frame_len = 60;

        static void _build_ETH_header(std::array<uint8_t,frame_len>& frame,int& idx,std::array<uint8_t,6>* mac_address);
    
        static inline std::array<uint8_t,6> parse_mac(const std::string& s);

        static void _build_DCP_header(std::array<uint8_t,frame_len>& frame,int& idx);
    
        static void _build_DCP_message(std::array<uint8_t,frame_len>& frame,int& idx);

};



