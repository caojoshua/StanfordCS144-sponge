#include "wrapping_integers.hh"

#include <limits>

#define MAX_SEQNO std::numeric_limits<uint32_t>::max()

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

static int64_t abs(uint64_t _i) {
    int64_t i = static_cast<int64_t>(_i);
    return i >= 0 ? i : -i;
}

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return isn + n; }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
//
// Compute candidate absolute center, left and right seqnos, and return the seqno corresponding
// to the lowest difference with checkpoint. Left and right seqnos require a check to prevent
// computing differences that wrap around uint64_t, which causes unexpected values with signing
// and absolute values.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    static int64_t wrapping_boundary = static_cast<uint64_t>(UINT32_MAX) + 1;

    WrappingInt32 seqno = n - isn.raw_value();
    uint64_t offset = checkpoint - checkpoint % wrapping_boundary;

    // Compute the center seqno.
    uint64_t center_seqno = offset + seqno.raw_value();
    int64_t center_diff = abs(checkpoint - center_seqno);

    // Compute the left seqno.
    if (checkpoint > UINT32_MAX) {
        uint64_t left_seqno = center_seqno - wrapping_boundary;
        int64_t left_diff = abs(checkpoint - left_seqno);
        if (left_diff < center_diff)
            return left_seqno;
    }

    // Compute the right seqno.
    if (checkpoint < UINT64_MAX - UINT32_MAX) {
        uint64_t right_seqno = center_seqno + wrapping_boundary;
        int64_t right_diff = abs(checkpoint - right_seqno);
        if (right_diff < center_diff)
            return right_seqno;
    }

    // Return the center seqno if left and right seqnos were not the lowest difference.
    return center_seqno;
}
