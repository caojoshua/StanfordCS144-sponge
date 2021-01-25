#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

uint64_t TCPReceiver::assembled_bytes() const {
    return stream_out().bytes_written();
}

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader header = seg.header();
    uint32_t seqno = header.seqno.raw_value();

    // If SYN has not been received, check the header for SYN, and set the approriate member
    // variables. Return if header does not contain SYN.
    if (!_syn_recv) {
        if (header.syn) {
            _syn_recv = true;
            _isn = seqno;
            ++seqno; // Skip SYN byte, since its not a data byte.
        }
        else
          return;
    }

    // Push the data to the reassembler.
    std::string data = seg.payload().copy();
    uint32_t reassembler_index = seqno - _isn - 1;
    _reassembler.push_substring(data, reassembler_index, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn_recv)
        return optional<WrappingInt32>{};

    uint32_t ack_bytes = assembled_bytes();
    if (_syn_recv)
        ++ack_bytes;
    if (stream_out().input_ended())
        ++ack_bytes;

    return optional<WrappingInt32>{_isn + ack_bytes};
}

size_t TCPReceiver::window_size() const {
  optional<WrappingInt32> ackno = this->ackno();
  if (ackno.has_value()) {
      size_t last_ack_seqno = _isn + 1 + _capacity;
      return last_ack_seqno - ackno.value().raw_value();
  }
  return _capacity;
}
