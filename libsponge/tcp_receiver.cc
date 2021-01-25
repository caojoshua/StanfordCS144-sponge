#include "tcp_receiver.hh"

#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader header = seg.header();
    uint32_t seqno = header.seqno.raw_value();

    // If SYN has not been received, check the header for SYN, and set the approriate member
    // variables. Return if header does not contain SYN.
    if (!_syn_recv) {
        if (header.syn) {
            _syn_recv = true;
            _isn = seqno;
            ++_ack_bytes; // Acknowledge the SYN byte.
            ++seqno; // Skip SYN byte, since its not a data byte.
        }
        else
          return;
    }

    // Push the data to the reassembler.
    std::string data = seg.payload().copy();
    uint32_t reassembler_index = seqno - _isn - 1;
    _reassembler.push_substring(data, reassembler_index, header.fin);
    _ack_bytes += data.size();

    // Acknowledge FIN byte.
    if (stream_out().input_ended())
      ++_ack_bytes;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    return _syn_recv ? optional<WrappingInt32>{_isn + _ack_bytes} : optional<WrappingInt32>{};
}

size_t TCPReceiver::window_size() const {
  return _capacity - unassembled_bytes();
}
