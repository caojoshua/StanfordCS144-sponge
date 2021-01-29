#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPSender::send_segment(TCPSegment segment) {
    segment.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(segment);

    uint16_t length = segment.length_in_sequence_space();
    _window_size -= length;
    _next_seqno += length;

    // Add to outstanding segments sorted by first seqno.
    // TODO: maybe need to use wrapping int
    uint32_t seqno = segment.header().seqno.raw_value();
    auto iter = _outstanding_segments.cbegin();
    auto end = _outstanding_segments.cend();
    while (iter != end) {
        if (seqno < iter->header().seqno.raw_value()) {
            _outstanding_segments.insert(iter, segment);
            return;
        }
        ++iter;
    }
    _outstanding_segments.push_back(segment);
}

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {
    std::cout << "new\n";

    // Send a segment with only the SYN byte.
    Buffer buffer("");
    TCPSegment segment;
    segment.parse(buffer);
    segment.header().syn = true;
    send_segment(segment);
}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t bytes = 0;
    for (TCPSegment segment : _outstanding_segments)
        bytes += segment.length_in_sequence_space();
    return bytes;
}

void TCPSender::fill_window() {
    // Send the FIN byte if input ended.
    if (_stream.buffer_empty() || _window_size == 0)
        return;

    // Add the segment to the outbound queue.
    // TODO: not sure if need to manually set header flags eg. SYN, FIN, WIN
    const uint16_t bytes_to_read =
        _window_size < TCPConfig::MAX_PAYLOAD_SIZE ? _window_size : TCPConfig::MAX_PAYLOAD_SIZE;
    TCPSegment segment;
    segment.payload() = Buffer(_stream.read(bytes_to_read));
    send_segment(segment);
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // Return if invalid seqno
    if (ackno.raw_value() > _isn.raw_value() + _next_seqno)
        return;

    // Remove acknowledged outstanding segments.
    auto iter = _outstanding_segments.cbegin();
    auto end = _outstanding_segments.cend();
    TCPHeader iter_header = iter->header();
    while (iter != end && iter_header.seqno.raw_value() + iter_header.win < ackno.raw_value()) {
        auto temp_iter = iter;
        ++iter;
        iter_header = iter->header();
        _outstanding_segments.erase(temp_iter);
    }

    _window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {}
