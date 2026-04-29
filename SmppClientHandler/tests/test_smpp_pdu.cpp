#include <gtest/gtest.h>
#include "smpp_pdu.hpp"

TEST(SmppPdu, DecodeHeader) {
    uint8_t buf[16] = {};
    smpp::write_u32(buf,    32);
    smpp::write_u32(buf+4,  smpp::BIND_TRANSMITTER);
    smpp::write_u32(buf+8,  0);
    smpp::write_u32(buf+12, 7);
    auto h = smpp::decode_header(buf);
    EXPECT_EQ(h.command_length,  32u);
    EXPECT_EQ(h.command_id,      smpp::BIND_TRANSMITTER);
    EXPECT_EQ(h.command_status,  0u);
    EXPECT_EQ(h.sequence_number, 7u);
}

TEST(SmppPdu, MakeResponseHeaderOnly) {
    auto pdu = smpp::make_response(smpp::ENQUIRE_LINK_RESP, smpp::ESME_ROK, 42);
    ASSERT_EQ(pdu.size(), 16u);
    EXPECT_EQ(smpp::read_u32(pdu.data()),    16u);
    EXPECT_EQ(smpp::read_u32(pdu.data()+4),  smpp::ENQUIRE_LINK_RESP);
    EXPECT_EQ(smpp::read_u32(pdu.data()+8),  smpp::ESME_ROK);
    EXPECT_EQ(smpp::read_u32(pdu.data()+12), 42u);
}

TEST(SmppPdu, MakeResponseWithSystemId) {
    auto pdu = smpp::make_response(smpp::BIND_TRANSMITTER_RESP, smpp::ESME_ROK, 1, "smsc");
    EXPECT_EQ(pdu.size(), 16u + 5u);  // "smsc\0"
    EXPECT_EQ(smpp::read_u32(pdu.data()), 21u);
}

TEST(SmppPdu, BindRespId) {
    EXPECT_EQ(smpp::bind_resp_id(smpp::BIND_TRANSMITTER), smpp::BIND_TRANSMITTER_RESP);
    EXPECT_EQ(smpp::bind_resp_id(smpp::BIND_RECEIVER),    smpp::BIND_RECEIVER_RESP);
    EXPECT_EQ(smpp::bind_resp_id(smpp::BIND_TRANSCEIVER), smpp::BIND_TRANSCEIVER_RESP);
}

TEST(SmppPdu, ReadCstr) {
    std::vector<uint8_t> buf = {'e','s','m','e','1',0,'p','a','s','s',0};
    size_t off = 0;
    EXPECT_EQ(smpp::read_cstr(buf, off), "esme1");
    EXPECT_EQ(off, 6u);
    EXPECT_EQ(smpp::read_cstr(buf, off), "pass");
}
