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

// Shutdown the connection if able to.
void TCPConnection::check_shutdown() {
    // We know the outbound stream is fully ack if it is eof and bytes_in_flight(number of non-ack bytes) == 0
    if (_receiver.stream_out().eof() && _sender.stream_in().eof() && _sender.bytes_in_flight() == 0) {
        _active = false;
        _linger_after_streams_finish = false;
    }
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

size_t TCPConnection::remaining_outbound_capacity() const { return {}; }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return {}; }

size_t TCPConnection::time_since_last_segment_received() const { return {}; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();

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
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _active = false;
        // TODO: kill connection permanently
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

    // Send all the segments to the network.
    send_segments();

    // Shutdown if connection is finished
    check_shutdown();
}

bool TCPConnection::active() const { return _active || _linger_after_streams_finish; }

size_t TCPConnection::write(const string &data) {
    size_t size = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segments();
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    send_segments();
    // TODO: clean up connection and other steps
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segments();
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

            // Send a RST segment to peer
            TCPSegment seg;
            seg.header().rst = true;
            _segments_out.push(seg);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
