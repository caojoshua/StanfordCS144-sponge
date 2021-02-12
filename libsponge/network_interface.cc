#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

// Sends dgram to address.
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const EthernetAddress &address) {
    EthernetFrame frame;
    frame.payload() = dgram.serialize();
    EthernetHeader &header = frame.header();
    header.src = _ethernet_address;
    header.dst = address;
    header.type = EthernetHeader::TYPE_IPv4;
    _frames_out.push(frame);
}

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address), _ip_address_raw(_ip_address.ipv4_numeric()) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    EthernetFrame frame;
    auto iter = _ip_to_ethernet.find(next_hop_ip);

    if (iter == _ip_to_ethernet.cend()) {
        ARPMessage arp;
        arp.opcode = ARPMessage::OPCODE_REQUEST;
        arp.sender_ethernet_address = _ethernet_address;
        arp.sender_ip_address = _ip_address_raw;
        arp.target_ip_address = next_hop_ip;

        frame.payload() = arp.serialize();
        EthernetHeader &header = frame.header();
        header.dst = ETHERNET_BROADCAST;
        header.src = _ethernet_address;
        header.type = EthernetHeader::TYPE_ARP;

        _frames_out.push(frame);
        _datagram_queue[next_hop_ip].push_back(dgram);
    } else {
        send_datagram(dgram, iter->second.address);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    EthernetHeader header = frame.header();
    if (header.dst != _ethernet_address && header.dst != ETHERNET_BROADCAST)
        return {};

    if (header.type == EthernetHeader::TYPE_IPv4) {
        IPv4Datagram datagram;
        if (datagram.parse(frame.payload()) == ParseResult::NoError)
            return datagram;
    }

    else if (header.type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp;
        if (arp.parse(frame.payload()) != ParseResult::NoError)
            return {};

        _ip_to_ethernet[arp.sender_ip_address].address = arp.sender_ethernet_address;

        auto find = _datagram_queue.find(arp.sender_ip_address);
        if (find != _datagram_queue.cend()) {
            std::list<InternetDatagram> dgrams = _datagram_queue[arp.sender_ip_address];
            while (!dgrams.empty()) {
                send_datagram(dgrams.front(), arp.sender_ethernet_address);
                dgrams.pop_front();
            }
            _datagram_queue.erase(find);
        }
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }
