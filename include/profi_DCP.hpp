
#pragma once

#include <datatype.hpp>
#include <random>
#include <cstdint>
#include <stdexcept>
#include <pcap.h>
#include <cstring>

#ifdef _WIN32
    #define NOMINMAX
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #include <misc.h>
 
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
#endif
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
    /**
     * @brief Simple DTO for a PN-DCP device discovered on the wire.
     * @details Optional fields are filled if present in received DCP blocks.
     */
    class DCP_Device
    {
        public:
        DCP_Device()=default;
        ~DCP_Device()=default;
        /// IP address (dotted), if present in DCP response.
        std::optional<std::string> ip;
        
        /// MAC address as "aa:bb:cc:dd:ee:ff", if present.
        std::optional<std::string> MAC;
        
        /// Profinet station name (string), if present.
        std::optional<std::string> StationName;
        
        /// Device family/product string or code (vendor specific), if present.
        std::optional<std::string> Familiy;
        
        /// @brief Factory: parse a PN-DCP reply into a device object.
        /// @param len Captured length in bytes.
        /// @param package Pointer to the full L2 frame buffer.
        /// @return Parsed device (may have only partial fields set).
        static DCP_Device create(int len,u_char package);
        
        /// @brief Heuristic: whether this device looks like a PLC.
        /// @return true if Family contains "plc" (case-insensitive) or key fields are present.
        bool isPLC();
    };

    /**
     * @brief Thin wrapper around pcap_loop to collect PN-DCP responses.
     * @details Non-owning: it writes results into an external vector passed in ctor.
     */
    class Sniffer {
    public:
    
        /// @brief Start capturing on the given pcap handle; calls onPacket per frame.
        void start(pcap_t* handle);
        
        /// @brief Number of packets processed so far.
        size_t get_packet_nr();
        
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
        std::optional<std::vector<profinet::DCP_Device>> devices;

        char errbuf[PCAP_ERRBUF_SIZE];
        
        /// @brief Enumerate available network interfaces (pcap names + friendly labels).
        std::map<std::string,std::string> _get_netCards();
    
        /// @brief Extract interface GUID from a Windows pcap adapter name (no-op on Linux).
        std::string extract_guid();

        /// @brief Get interface MAC address for the selected NIC.
        std::array<uint8_t,6> _get_mac();

        /// @brief (Optional) inline packet processor if using pcap_dispatch/loop with non-static cb.
        void _package_process(u_char* user, const pcap_pkthdr* h, const u_char* bytes);

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
        std::optional<std::vector<profinet::DCP_Device>> get_devices();
        
        /// @brief Map of "idx: friendly name" -> pcap adapter string.
        std::map<std::string,std::string> get_cards();
        
        /// @brief Select active NIC for send/receive (pcap adapter string).
        void set_card(std::string in);
    };

    /// @brief Interface + score pair (reserved for NIC ranking heuristics) DEPRECATED.
    struct IfScore 
    {
        pcap_if_t* dev;
        int score;
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
    
        /// @brief Build a PN-DCP Identify request frame.
        /// @param mac_address Source MAC to place in Ethernet header.
        /// @return A 60-byte frame ready for pcap_sendpacket().
        static std::array<uint8_t,60> build_DCP(std::array<uint8_t,6>* mac_address);

    private:
        
        static const int frame_len = 60;
        
        /// @brief Build Ethernet L2 header (dest, src, EtherType).
        static void _build_ETH_header(std::array<uint8_t,frame_len>& frame,int& idx,std::array<uint8_t,6>* mac_address);
    
        /// @brief Parse "aa:bb:cc:dd:ee:ff" into 6 bytes.
        static inline std::array<uint8_t,6> parse_mac(const std::string& s);

        /// @brief Build the PN-DCP header (FrameID, ServiceID/Type, TransactionID...).
        static void _build_DCP_header(std::array<uint8_t,frame_len>& frame,int& idx);
    
        /// @brief Build the DCP block list for Identify (Option/Suboption/BlockLen...).
        static void _build_DCP_message(std::array<uint8_t,frame_len>& frame,int& idx);

};



