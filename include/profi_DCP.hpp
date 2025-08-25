
#pragma once

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #include <pcap.h>

 
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <pcap.h>
#endif

#include <datatype.hpp>
#include <random>
#include <cstdint>
#include <stdexcept>
#include <cstring>
#include <cstdlib> 
#include <ctime>

/**
 * @brief Profinet DCP discovery and packet utilities.
 * @details
 * Implements a minimal Profinet DCP Identify workflow using libpcap:
 *  - Build and send a DCP Identify request (EtherType 0x8892)
 *  - Sniff replies and extract device metadata (MAC, IP, StationName, Family)
 *  - Expose discovered devices to the rest of the app
 */
namespace profinet
{

    /* ---------------- IP Parameters ---------------- */


    /**
     * @brief Simple DTO for a PN-DCP device discovered on the wire.
     * @details Optional fields are filled if present in received DCP blocks.
     */
    class IPParams 
    {
        private:
        std::string ip;
        std::string mask;
        std::string gateway;
        public:
        static std::optional<IPParams> create(const std::vector<uint8_t>& body) {
            if (body.size() < 12) 
            {
                std::cerr<<"ip too short "<<std::to_string(body.size())<<"\n";
                return std::nullopt;
            }
            IPParams p;
            p.ip       = to_ipv4(&body[0]);
            p.mask     = to_ipv4(&body[4]);
            if (body.size() > 12) p.gateway  = to_ipv4(&body[8]);
            return p;
        }
        
        static std::string to_ipv4(const uint8_t* b) {
            return std::to_string(b[0]) + "." + std::to_string(b[1]) + "." +
                std::to_string(b[2]) + "." + std::to_string(b[3]);
        }
        const std::string get_ip() const {return ip;};
        std::string get_mask(){return mask;};
        std::string get_gateway(){return gateway;};
    };

    /* -------------------- TLV ---------------------- */

    /**
     * @brief Simple DTO for a TLV package.
     */
    class TLV
    {
        private:
            uint8_t option;
            uint8_t sub_option;
            int len;
            std::vector<uint8_t> body;
        public:
        
            TLV(uint8_t opt, uint8_t sub_opt,int len);  
            uint8_t get_option();
            uint8_t get_suboption();
            int get_len();
            const std::vector<uint8_t>& get_body();

            static std::optional<TLV> create(std::vector<uint8_t>& bt,int len);
    };

    /* ----------------- DCP Device ------------------ */

    /**
     * @brief Simple DTO for parsing DCP block into DCP device.
     * @details Optional fields are filled if present in received DCP blocks.
     */
    class DCP_Device
    {
        public:
        DCP_Device()=default;
        ~DCP_Device()=default;
        /// IP address (dotted), if present in DCP response.
        std::optional<IPParams> ip;
        
        /// MAC address as "aa:bb:cc:dd:ee:ff", if present.
        std::optional<std::string> MAC;
        
        /// Profinet station name (string), if present.
        std::optional<std::string> StationName;
        
        /// Device family/product string or code (vendor specific), if present.
        std::optional<std::string> Family;
        
        /// @brief Factory: parse a PN-DCP reply into a device object.
        /// @param len Captured length in bytes.
        /// @param package Pointer to the full L2 frame buffer.
        /// @return Parsed device (may have only partial fields set).
        static std::optional<profinet::DCP_Device> create(int len,const u_char* package);
        
        /// @brief Heuristic: whether this device looks like a PLC.
        /// @return true if Family contains "plc" (case-insensitive) or key fields are present.
        bool isPLC();

        void add_TLV(TLV tlv);
    };

    /* ------------------ Sniffer -------------------- */

    /**
     * @brief Thin wrapper around pcap_loop to collect PN-DCP responses.
     * @details Non-owning: it writes results into an external vector passed in ctor.
     */
    class Sniffer {
    public:
    
        /// @brief Start capturing on the given pcap handle; calls onPacket per frame.
        void start(pcap_t* handle);

        /// @brief Construct a sniffer that appends results into \p _objs .
        Sniffer(std::vector<DCP_Device>* _objs);

    private:
        
        /// @brief Static pcap callback trampoline -> onPacket().
        static void pcap_cb(u_char* user, const pcap_pkthdr* h, const u_char* bytes) ;
        
        /// @brief User-defined callback: parse a single captured frame.
        void onPacket(const pcap_pkthdr* h, const u_char* bytes) ;
        
        std::vector<DCP_Device>* objs;  ///< External results vector (non-owning).
        pcap_t* handle_ = nullptr;      ///< Active pcap handle.
        size_t  packets_ = 0;           ///< Packets processed.
    };

    /* ---------------- Pcap Client ------------------ */
    
    /**
     * @brief Profinet DCP client using libpcap for send/receive.
     * @details
     * Responsibilities:
     *  - Enumerate NICs via pcap
     *  - Build and send a broadcast Identify request
     *  - Capture responses (via Sniffer) and expose them
     */
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
        std::array<uint8_t,4> XID{};
        std::vector<profinet::DCP_Device> devices;

        char errbuf[PCAP_ERRBUF_SIZE];
        
        /// @brief Enumerate available network interfaces (pcap names + friendly labels).
        std::map<std::string,std::string> _get_netCards();
    
        /// @brief Extract interface GUID from a Windows pcap adapter name (no-op on Linux).
        std::string extract_guid();

        /// @brief Get interface MAC address for the selected NIC.
        std::array<uint8_t,6> _get_mac();

        /// @brief Install a BPF filter (EtherType 0x8892 for Profinet).
        void _set_filter(pcap_t* process);

    public:
        
        /// @brief Construct and cache network interface list.
        PcapClient();
        ~PcapClient()=default;
        
        /// @brief Send a DCP Identify and collect responses.
        /// @return 0 on success; PCAP_ERROR on failure.
        int identifyAll();
        
        /// @brief Get last discovered device list (if any).
        const std::vector<profinet::DCP_Device>* get_devices()const;
        
        /// @brief Map of "idx: friendly name" -> pcap adapter string.
        std::map<std::string,std::string> get_cards();
        
        /// @brief Select active NIC for send/receive (pcap adapter string).
        void set_card(std::string in);
    };

};

/**
 * @brief Helper to build raw PN-DCP Identify frames.
 * @details Produces a 60-byte Ethernet frame:
 *  - Dest MAC: 01:0E:CF:00:00:00 (Profinet multicast) or FF:FF:FF:FF:FF:FF
 *  - EtherType: 0x8892 (Profinet)
 *  - PN-DCP Identify Request payload
 */
class packageHelper
{
    /*
    [ Ethernet ]        
    01 0E CF 00 00 00   00 0C 29 15 37 A3   88 92 
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
    FE FE   05   00   8A B4 83 20   00 00 00 04 FF FF 
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
    FF  FF  00 00
    
    25 26   27 28
    ^   ^   ^^^^^
    |   |   |
    |   |   Block len
    |   |   
    |   Suboption
    |   
    Option

    */
    public:
    
        /// @brief Build a PN-DCP Identify request frame.
        /// @param mac_address Source MAC to place in Ethernet header.
        /// @return A 60-byte frame ready for pcap_sendpacket().
        static std::array<uint8_t,60> build_DCP(std::array<uint8_t,6>* mac_address,std::array<uint8_t,4>& XID);

    private:
        
        static const int frame_len = 60;
        
        /// @brief Build Ethernet L2 header (dest, src, EtherType).
        static void _build_ETH_header(std::array<uint8_t,frame_len>& frame,int& idx,std::array<uint8_t,6>* mac_address);
    
        /// @brief Parse "aa:bb:cc:dd:ee:ff" into 6 bytes.
        static inline std::array<uint8_t,6> parse_mac(const std::string& s);

        /// @brief Build the PN-DCP header (FrameID, ServiceID/Type, TransactionID...).
        static void _build_DCP_header(std::array<uint8_t,frame_len>& frame,int& idx,std::array<uint8_t,4>& XID);
    
        /// @brief Build the DCP block list for Identify (Option/Suboption/BlockLen...).
        static void _build_DCP_message(std::array<uint8_t,frame_len>& frame,int& idx);

};



