#include <catch2/catch.hpp>
#include <mmbkpp/stream/package_preproc.hpp>
#include <mego/err/ec.h>
#include <mego/util/std/time.h>

#include <vector>
#include <string>
#include <cstring>

namespace {

// Convert a const char array to memepp::buffer_view (the API requires const MemeByte_t*).
static memepp::buffer_view bv(const char* data, mmint_t size)
{
    return memepp::buffer_view{
        reinterpret_cast<memepp::buffer_view::const_pointer>(data), size
    };
}

struct TestCtx
{
    std::vector<std::string> received;
    bool   calc_len_fail = false;
    bool   chksum_fail   = false;
    size_t forced_len    = 0; // if > 0 use this length instead of first byte
};

// calc_len: length is taken from the first byte of `curr`
static mgpp::err s_calc_len(
    const memepp::buffer_view& curr, const memepp::buffer_view& /*wait*/,
    size_t* len, void* ud)
{
    auto* ctx = static_cast<TestCtx*>(ud);
    if (ctx->calc_len_fail)
        return mgpp::err{ MGEC__ERR };
    if (ctx->forced_len > 0) {
        *len = ctx->forced_len;
        return {};
    }
    if (curr.empty())
        return mgpp::err{ MGEC__INVAL };
    *len = static_cast<unsigned char>(curr.at(0));
    return {};
}

static mgpp::err s_chksum(const memepp::buffer_view& /*buf*/, void* ud)
{
    auto* ctx = static_cast<TestCtx*>(ud);
    if (ctx->chksum_fail)
        return mgpp::err{ MGEC__ERR };
    return {};
}

static mgpp::err s_recv(
    const memepp::buffer_view& buf,
    mmbkpp::stream::package_preproc* /*pp*/,
    void* ud)
{
    auto* ctx = static_cast<TestCtx*>(ud);
    ctx->received.emplace_back(
        reinterpret_cast<const char*>(buf.data()), static_cast<size_t>(buf.size()));
    return {};
}

static mmbkpp::stream::package_preproc make_pp(TestCtx& ctx)
{
    mmbkpp::stream::package_preproc pp(&ctx);
    pp.set_recv_cb(&s_recv);
    pp.set_calc_len_cb(&s_calc_len);
    pp.set_checksum_succ_cb(&s_chksum);
    return pp;
}

static const mgu_timestamp_t k_now = 0;

} // namespace

// ---------------------------------------------------------------------------
// TC#1 – default limit values after construction
// ---------------------------------------------------------------------------
TEST_CASE("package_preproc - constructor default limit sizes", "[package_preproc]")
{
    mmbkpp::stream::package_preproc pp;
    REQUIRE(pp.min_limit_package_size() == 0);
    REQUIRE(pp.max_limit_package_size() == SIZE_MAX);
}

// ---------------------------------------------------------------------------
// TC#2 – set/get min and max limit sizes
// ---------------------------------------------------------------------------
TEST_CASE("package_preproc - set and get limit sizes", "[package_preproc]")
{
    mmbkpp::stream::package_preproc pp;
    pp.set_min_limit_package_size(8);
    pp.set_max_limit_package_size(1024);
    REQUIRE(pp.min_limit_package_size() == 8);
    REQUIRE(pp.max_limit_package_size() == 1024);
}

// ---------------------------------------------------------------------------
// TC#3 – single complete packet → recv_cb fires exactly once
// ---------------------------------------------------------------------------
TEST_CASE("package_preproc - single complete packet fires recv_cb", "[package_preproc]")
{
    TestCtx ctx;
    auto pp = make_pp(ctx);

    const char pkt[] = {'\x04', 'a', 'b', 'c'}; // length=4
    auto err = pp.raw_input(bv(pkt, mmint_t(sizeof(pkt))), k_now);

    REQUIRE(err.ok());
    REQUIRE(ctx.received.size() == 1);
    REQUIRE(ctx.received[0].size() == 4);
    REQUIRE(ctx.received[0][1] == 'a');
    REQUIRE(ctx.received[0][2] == 'b');
    REQUIRE(ctx.received[0][3] == 'c');
}

// ---------------------------------------------------------------------------
// TC#4 – packet split across two raw_input calls → recv_cb fires on second call
// ---------------------------------------------------------------------------
TEST_CASE("package_preproc - split packet across two calls fires recv_cb on second", "[package_preproc]")
{
    TestCtx ctx;
    auto pp = make_pp(ctx);

    // first half: length byte says 4, but only 2 bytes available
    const char part1[] = {'\x04', 'x'};
    auto err = pp.raw_input(bv(part1, mmint_t(sizeof(part1))), k_now);
    REQUIRE(err.ok());
    REQUIRE(ctx.received.empty()); // not enough data yet

    // second half: remaining 2 bytes
    const char part2[] = {'y', 'z'};
    err = pp.raw_input(bv(part2, mmint_t(sizeof(part2))), k_now);
    REQUIRE(err.ok());
    REQUIRE(ctx.received.size() == 1);
    REQUIRE(ctx.received[0].size() == 4);
    REQUIRE(ctx.received[0][1] == 'x');
    REQUIRE(ctx.received[0][2] == 'y');
    REQUIRE(ctx.received[0][3] == 'z');
}

// ---------------------------------------------------------------------------
// TC#5 – two complete packets in one buffer:
//         first fires immediately, second via poll()
// ---------------------------------------------------------------------------
TEST_CASE("package_preproc - two packets in one buffer: first fires immediately, second via poll", "[package_preproc]")
{
    TestCtx ctx;
    auto pp = make_pp(ctx);

    // two 3-byte packets back-to-back
    const char buf[] = {'\x03', 'A', 'B', '\x03', 'C', 'D'};
    auto err = pp.raw_input(bv(buf, mmint_t(sizeof(buf))), k_now);
    REQUIRE(err.ok());
    REQUIRE(ctx.received.size() == 1); // second packet is queued in wait_cache
    REQUIRE(ctx.received[0][1] == 'A');

    err = pp.poll(k_now);
    REQUIRE(err.ok());
    REQUIRE(ctx.received.size() == 2);
    REQUIRE(ctx.received[1][1] == 'C');
}

// ---------------------------------------------------------------------------
// TC#6 – buffer smaller than min_limit_package_size → recv_cb NOT called
// ---------------------------------------------------------------------------
TEST_CASE("package_preproc - buffer below min_limit: recv_cb not called", "[package_preproc]")
{
    TestCtx ctx;
    auto pp = make_pp(ctx);
    pp.set_min_limit_package_size(10);

    const char buf[] = {'\x03', 'A', 'B'};
    auto err = pp.raw_input(bv(buf, mmint_t(sizeof(buf))), k_now);
    REQUIRE(err.ok());
    REQUIRE(ctx.received.empty());
}

// ---------------------------------------------------------------------------
// TC#7 – calc_len exceeds max_limit_package_size → raw_input returns error
// ---------------------------------------------------------------------------
TEST_CASE("package_preproc - calc_len exceeds max_limit: error returned", "[package_preproc]")
{
    TestCtx ctx;
    ctx.forced_len = 10; // calc_len will return 10
    auto pp = make_pp(ctx);
    pp.set_max_limit_package_size(5); // only 5 allowed

    const char buf[] = {'\x0A', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
    auto err = pp.raw_input(bv(buf, mmint_t(sizeof(buf))), k_now);

    REQUIRE(!err.ok()); // MGEC__PROTO
    REQUIRE(ctx.received.empty());
}

// ---------------------------------------------------------------------------
// TC#8 – head_match set, packet starts with matching head → recv_cb fires
// ---------------------------------------------------------------------------
TEST_CASE("package_preproc - head_match: packet with matching head fires recv_cb", "[package_preproc]")
{
    TestCtx ctx;
    ctx.forced_len = 4; // ignore first-byte protocol; fixed length
    auto pp = make_pp(ctx);

    // set 1-byte head marker
    const char head[] = {'\xAB'};
    pp.set_head_match(bv(head, 1));

    const char pkt[] = {'\xAB', 'X', 'Y', 'Z'}; // starts with head, forced_len=4
    auto err = pp.raw_input(bv(pkt, mmint_t(sizeof(pkt))), k_now);
    REQUIRE(err.ok());
    REQUIRE(ctx.received.size() == 1);
    REQUIRE(ctx.received[0][1] == 'X');
}

// ---------------------------------------------------------------------------
// TC#9 – head_match set, junk bytes before head: first call returns without firing
//         recv_cb; aligned second call fires recv_cb
// ---------------------------------------------------------------------------
TEST_CASE("package_preproc - head_match: junk before head skipped, second call succeeds", "[package_preproc]")
{
    TestCtx ctx;
    ctx.forced_len = 3;
    auto pp = make_pp(ctx);

    const char head[] = {'\xAB'};
    pp.set_head_match(bv(head, 1));

    // two junk bytes + one-byte head → raw_input_part returns EAGAIN at offset 2
    // raw_input swallows EAGAIN and returns 0; recv_cb not fired
    const char buf_junk[] = {'\x00', '\x00', '\xAB', 'P', 'Q'};
    auto err = pp.raw_input(bv(buf_junk, mmint_t(sizeof(buf_junk))), k_now);
    REQUIRE(err.ok());
    REQUIRE(ctx.received.empty());

    // aligned buffer starting with the head byte
    const char buf_aligned[] = {'\xAB', 'P', 'Q'};
    err = pp.raw_input(bv(buf_aligned, mmint_t(sizeof(buf_aligned))), k_now);
    REQUIRE(err.ok());
    REQUIRE(ctx.received.size() == 1);
    REQUIRE(ctx.received[0][1] == 'P');
}

// ---------------------------------------------------------------------------
// TC#10 – head_match set, no matching byte in buffer → recv_cb never fires
// ---------------------------------------------------------------------------
TEST_CASE("package_preproc - head_match: no matching byte, recv_cb never fires", "[package_preproc]")
{
    TestCtx ctx;
    ctx.forced_len = 3;
    auto pp = make_pp(ctx);

    const char head[] = {'\xAB'};
    pp.set_head_match(bv(head, 1));

    const char buf[] = {'\x01', '\x02', '\x03', '\x04'};
    auto err = pp.raw_input(bv(buf, mmint_t(sizeof(buf))), k_now);
    REQUIRE(err.ok());
    REQUIRE(ctx.received.empty());
}

// ---------------------------------------------------------------------------
// TC#11 – checksum callback returns error → raw_input returns that error
// ---------------------------------------------------------------------------
TEST_CASE("package_preproc - checksum failure propagates error", "[package_preproc]")
{
    TestCtx ctx;
    ctx.chksum_fail = true;
    auto pp = make_pp(ctx);

    const char pkt[] = {'\x03', 'M', 'N'};
    auto err = pp.raw_input(bv(pkt, mmint_t(sizeof(pkt))), k_now);
    REQUIRE(!err.ok());
    REQUIRE(ctx.received.empty());
}

// ---------------------------------------------------------------------------
// TC#12 – calc_len callback returns error → raw_input returns that error
// ---------------------------------------------------------------------------
TEST_CASE("package_preproc - calc_len failure propagates error", "[package_preproc]")
{
    TestCtx ctx;
    ctx.calc_len_fail = true;
    auto pp = make_pp(ctx);

    const char pkt[] = {'\x03', 'M', 'N'};
    auto err = pp.raw_input(bv(pkt, mmint_t(sizeof(pkt))), k_now);
    REQUIRE(!err.ok());
    REQUIRE(ctx.received.empty());
}

// ---------------------------------------------------------------------------
// TC#13 – clear_recv_cache() discards partial data; next complete packet succeeds
// ---------------------------------------------------------------------------
TEST_CASE("package_preproc - clear_recv_cache resets state for fresh packet", "[package_preproc]")
{
    TestCtx ctx;
    auto pp = make_pp(ctx);

    // send partial packet: announces length 4 but only 2 bytes arrive
    const char partial[] = {'\x04', 'A'};
    auto err = pp.raw_input(bv(partial, mmint_t(sizeof(partial))), k_now);
    REQUIRE(err.ok());
    REQUIRE(ctx.received.empty());

    // clear the accumulated state
    pp.clear_recv_cache();

    // now send a fresh complete packet
    const char pkt[] = {'\x03', 'B', 'Z'};
    err = pp.raw_input(bv(pkt, mmint_t(sizeof(pkt))), k_now);
    REQUIRE(err.ok());
    REQUIRE(ctx.received.size() == 1);
    REQUIRE(ctx.received[0][1] == 'B');
    REQUIRE(ctx.received[0][2] == 'Z');
}
