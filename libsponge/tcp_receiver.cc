#include "tcp_receiver.hh"

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
            ++seqno;  // Skip SYN byte, since its not a data byte.
        } else
            return;
    }

    // Push the data to the reassembler.
    uint32_t reassembler_relative_index = seqno;
    uint64_t unwrapped_index =
        unwrap(WrappingInt32{reassembler_relative_index}, WrappingInt32{_isn + 1}, stream_out().bytes_written());
    _reassembler.push_substring(seg.payload().copy(), unwrapped_index, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn_recv)
        return optional<WrappingInt32>{};

    uint32_t ack_bytes = stream_out().bytes_written();

    // Increment if acknowledged SYN byte
    if (_syn_recv)
        ++ack_bytes;

    // Increment if acknowledged FIN byte
    if (stream_out().input_ended())
        ++ack_bytes;

    return optional<WrappingInt32>{_isn + ack_bytes};
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
