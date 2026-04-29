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

TEST(SmppPdu, ParseSubmitSmBody) {
    // Build a minimal submit_sm body per SMPP 3.4 §4.4.1
    std::vector<uint8_t> body;
    // service_type ""
    body.push_back(0);
    // src_ton=1, src_npi=1, src_addr="441234"
    body.push_back(1); body.push_back(1);
    for (char c : std::string("441234")) body.push_back(c); body.push_back(0);
    // dst_ton=0, dst_npi=1, dst_addr="esme1"
    body.push_back(0); body.push_back(1);
    for (char c : std::string("esme1")) body.push_back(c); body.push_back(0);
    // esm_class=0, protocol_id=0, priority_flag=0
    body.push_back(0); body.push_back(0); body.push_back(0);
    // schedule_delivery_time="", validity_period=""
    body.push_back(0); body.push_back(0);
    // registered_delivery=0, replace_if_present=0, data_coding=0, sm_default_msg_id=0
    body.push_back(0); body.push_back(0); body.push_back(0); body.push_back(0);
    // sm_length=5, short_message="Hello"
    body.push_back(5);
    for (char c : std::string("Hello")) body.push_back(c);

    auto sm = smpp::parse_submit_sm(body);
    EXPECT_EQ(sm.src_ton,       1u);
    EXPECT_EQ(sm.src_npi,       1u);
    EXPECT_EQ(sm.src_addr,      "441234");
    EXPECT_EQ(sm.dst_ton,       0u);
    EXPECT_EQ(sm.dst_npi,       1u);
    EXPECT_EQ(sm.dst_addr,      "esme1");
    EXPECT_EQ(sm.short_message, "Hello");
}

TEST(SmppPdu, ParseSubmitSmEmptyBody) {
    // Must not crash on empty body
    std::vector<uint8_t> body;
    auto sm = smpp::parse_submit_sm(body);
    EXPECT_TRUE(sm.src_addr.empty());
    EXPECT_TRUE(sm.short_message.empty());
}
