#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader header = seg.header();
    uint32_t first_seqno = header.seqno.raw_value();

    if (state == LISTEN && header.syn) {
        state = SYN_RECV;
        _ackno = WrappingInt32{first_seqno + 1};
    }

    _reassembler.push_substring(seg.payload().copy(), first_seqno, header.fin);
    if (header.fin)
      state = FIN_RECV;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    return _ackno;
}

size_t TCPReceiver::window_size() const {
  return _capacity - unassembled_bytes();
}
