#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPConnection::connect_sender() {
    _active = true;
    _sender.fill_window();
}

void TCPConnection::update_sender() {
    _sender.fill_window();
    send_segments();
}

// Update the connection status. Should be called everytime a segment is received.
void TCPConnection::update_connection_status() {
    if (_receiver.stream_out().eof()) {
        // Don't linger the stream if the inbound stream ends before the outbound stream.
        if (!_sender.stream_in().eof())
            _linger_after_streams_finish = false;

        // If both streams reached eof and all outbound bytes ar ack (bytes_in_flight() == 0)
        // Linger if should linger after streams finish
        // Shutdown otherwise.
        else if (bytes_in_flight() == 0) {
            if (_linger_after_streams_finish)
                _lingering = true;
            else
                shutdown();
        }
    }
}

void TCPConnection::shutdown() {
    _active = false;
    _linger_after_streams_finish = false;
}

void TCPConnection::streams_set_err() {
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
}

void TCPConnection::send_rst() {
    TCPSegment seg;
    seg.header().rst = true;
    _segments_out.push(seg);
    shutdown();
    streams_set_err();
}

// Send all the segments enqueued by the sender
void TCPConnection::send_segments() {
    std::queue<TCPSegment> &sender_segments_out = _sender.segments_out();
    while (!sender_segments_out.empty()) {
        TCPSegment seg = sender_segments_out.front();
        TCPHeader &header = seg.header();
        std::optional<WrappingInt32> ackno = _receiver.ackno();

        // If ackno exists, add ackno and window size to the header.
        if (ackno) {
            header.ack = true;
            header.ackno = ackno.value();
            header.win = _receiver.window_size();
        }
        _segments_out.push(seg);
        sender_segments_out.pop();
    }
}

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    _time_since_last_segment_received = 0;

    if (!_active) {
        // Activate the connection.
        if (header.syn)
            connect_sender();
        // Abort on receiving segment when connection is not active.
        else
            return;
    }

    // If segment has reset flag, kill connection.
    if (header.rst) {
        shutdown();
        streams_set_err();
        return;
    }

    // Send the segment to the receiver.
    _receiver.segment_received(seg);

    // Send the segment to the sender
    if (header.ack)
        _sender.ack_received(header.ackno, header.win);

    // Send an empty segment to ensure at least one segment is sent.
    if (_sender.segments_out().empty() && seg.length_in_sequence_space() > 0)
        _sender.send_empty_segment();

    send_segments();
    update_connection_status();
}

bool TCPConnection::active() const { return _active || _linger_after_streams_finish; }

size_t TCPConnection::write(const string &data) {
    size_t size = _sender.stream_in().write(data);
    update_sender();
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;

    // Send the time update to the sender and send the segments over the network.
    _sender.tick(ms_since_last_tick);
    send_segments();

    // Send a RST segment if the connection sent too many consecutive retransmissions.
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        // Empty the segments queue to remove the recent retransmitted segment.
        while (!_segments_out.empty())
            _segments_out.pop();
        send_rst();
    }

    // Abort the stream if the connection is lingering and waiting too long.
    if (_lingering && _linger_after_streams_finish && time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
        _active = false;
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    update_sender();
}

void TCPConnection::connect() {
    // Fill window with SYN byte and send to network.
    connect_sender();
    send_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            send_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
