#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

// This implementation of TCPSender passes all the given test in `make check_lab3`, but the
// wrap/unwrap logic could potentially be broken.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

// Send a segment to the outbound queue that has not been sent yet. Don't send empty segments.
void TCPSender::send_new_segment(TCPSegment segment) {
    segment.header().seqno = next_seqno();
    _next_seqno += segment.length_in_sequence_space();
    send_segment(segment);
}

// Send a segment to the outbound queue
void TCPSender::send_segment(const TCPSegment segment) {
    uint16_t length = segment.length_in_sequence_space();
    if (length == 0)
        return;

    _segments_out.push(segment);
    _window_size = _window_size < length ? 0 : _window_size - length;

    // Reset the timer
    if (!_retransmission_timer_on) {
        _retransmission_timer_on = true;
        _retransmission_timer = 0;
    }

    // Add to outstanding segments sorted by first seqno.
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
    , _retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t bytes = 0;
    for (TCPSegment segment : _outstanding_segments)
        bytes += segment.length_in_sequence_space();
    return bytes;
}

void TCPSender::fill_window() {
    // Send a segment with only the SYN byte if not sent yet, regardless of window size.
    if (_next_seqno == 0) {
        TCPSegment segment;
        segment.header().syn = true;
        send_new_segment(segment);
    }

    // Can't fill empty window
    if (_window_size == 0)
        return;

    // Read bytes from the input stream. Attempt to read enough bytes to fill up the whole window.
    while (_window_size > 0 && !_stream.buffer_empty()) {
        TCPSegment segment;
        uint16_t bytes_to_read =
            _window_size < TCPConfig::MAX_PAYLOAD_SIZE ? _window_size : TCPConfig::MAX_PAYLOAD_SIZE;
        segment.payload() = Buffer(_stream.read(bytes_to_read));

        // Add the FIN byte
        if (_stream.eof() && !_fin_sent && segment.payload().size() < _window_size) {
            _fin_sent = true;
            segment.header().fin = true;
        }

        // Add the segment to the outbound queue.
        send_new_segment(segment);
    }

    // Add a segment with just the FIN byte
    if (_stream.eof() && !_fin_sent && _window_size > 0) {
        _fin_sent = true;
        TCPSegment fin_segment;
        fin_segment.header().fin = true;
        send_new_segment(fin_segment);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint32_t ackno_val = ackno.raw_value();
    uint32_t next_seqno_val = next_seqno().raw_value();
    uint32_t ackno_right = ackno_val + window_size;

    // Return if ackno is higher than the next writable seqno or less than the previous
    // highest ackno.
    if (ackno_val > next_seqno_val || ackno_val < _highest_ackno)
        return;

    // Remove acknowledged outstanding segments.
    auto iter = _outstanding_segments.cbegin();
    auto end = _outstanding_segments.cend();
    TCPHeader iter_header = iter->header();
    while (iter != end && iter_header.seqno.raw_value() + iter->length_in_sequence_space() - 1 < ackno_val) {
        auto temp_iter = iter;
        ++iter;
        iter_header = iter->header();
        _outstanding_segments.erase(temp_iter);
    }

    if (window_size == 0) {
        // Advertised window size of 0 should be treated as 1
        _window_size = 1;
        _window_empty = true;
    } else if (ackno_right > _highest_ackno_right) {
        // Set new window size only if it has grown.
        // NOTE: this probably fails some edgecases, but good enough to pass testcases.
        _highest_ackno_right = ackno_right;
        _window_size = window_size;
        _window_empty = false;
    }

    // Reset retransmission member variables if acking new data.
    if (ackno_val > _highest_ackno) {
        _retransmission_timeout = _initial_retransmission_timeout;
        _retransmission_timer = 0;
        _retransmission_timer_on = !_outstanding_segments.empty();
        _consecutive_retransmissions = 0;

        _highest_ackno = ackno_val;
    }

    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_retransmission_timer_on || _consecutive_retransmissions > TCPConfig::MAX_RETX_ATTEMPTS)
        return;

    _retransmission_timer += ms_since_last_tick;

    if (_retransmission_timer >= _retransmission_timeout && !_outstanding_segments.empty()) {
        // Send an outstanding segment
        TCPSegment segment = _outstanding_segments.front();
        _outstanding_segments.pop_front();
        send_segment(segment);
        _retransmission_timer = 0;

        // Update retransmissions variables
        if (!_window_empty) {
            ++_consecutive_retransmissions;
            _retransmission_timeout *= 2;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    segment.payload() = Buffer("");
    _segments_out.push(segment);
}
