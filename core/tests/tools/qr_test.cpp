// QR encoder tests. Reference vectors were generated with two independent encoders installed in
// .toolchains/pyqr: segno 1.6.6 and Project Nayuki's qrcodegen 1.8. For every version 1-40 and ECC
// level L/M/Q/H the payload fills the version exactly; the CRC-32 of the matrix (rows of '0'/'1',
// no quiet zone) is compared (a) with a forced mask against segno, and (b) with automatic mask
// selection against qrcodegen (whose matrices were asserted identical to segno's for that mask).
// segno scores the N3/N4 mask penalties slightly differently, so its automatic choice can differ;
// both choices are valid QR codes. The partially-filled 'EmberMesh' reference comes from qrcodegen:
// segno 1.6.6 appends 8 extra zero padding bits when the bit stream already ends on a codeword
// boundary (encoder.write_padding_bits), so its pad codewords differ from ISO 18004 7.4.10.
#include <string>

#include "doctest.h"
#include "ember/crc.h"
#include "tools/qr.h"

using namespace ember;
using namespace ember::qr;

namespace {
struct Vec { int version, ecc, capacity, mask; uint32_t crc_forced; int auto_mask; uint32_t crc_auto; };
const Vec kVecs[] = {
{1, 0, 17, 1, 0xE9C6B2CAu, 3, 0x56634F1Fu},
{1, 1, 14, 2, 0xCBB43797u, 0, 0x0F3FB512u},
{1, 2, 11, 3, 0x1219864Eu, 0, 0x6A872ACFu},
{1, 3, 7, 4, 0x8031FDD2u, 1, 0x3B334694u},
{2, 0, 32, 2, 0x13E15A97u, 4, 0x6842D234u},
{2, 1, 26, 3, 0xDA426CAAu, 0, 0xD50FCBB4u},
{2, 2, 20, 4, 0x657613DDu, 1, 0x312BD00Eu},
{2, 3, 14, 5, 0x4EBB6A58u, 5, 0x4EBB6A58u},
{3, 0, 53, 3, 0xED46736Du, 0, 0x0966FB7Bu},
{3, 1, 42, 4, 0x2282E727u, 1, 0x7F17E123u},
{3, 2, 32, 5, 0xD33FBCE2u, 4, 0x22A5507Bu},
{3, 3, 24, 6, 0x69F4E7CAu, 0, 0x60693FEFu},
{4, 0, 78, 4, 0xA6E6925Fu, 6, 0x5FA82066u},
{4, 1, 62, 5, 0x99335F06u, 3, 0x802A99A8u},
{4, 2, 46, 6, 0xF3495E3Fu, 0, 0xFA2E4D16u},
{4, 3, 34, 7, 0x8BA689F6u, 1, 0xB3DBAF4Bu},
{5, 0, 106, 5, 0xCC55E65Au, 7, 0x1611A7D8u},
{5, 1, 84, 6, 0x3AD0E38Bu, 4, 0x9CDDABA2u},
{5, 2, 60, 7, 0xAF493B5Eu, 2, 0x0E27746Bu},
{5, 3, 44, 0, 0x256DE8C6u, 2, 0xBB97CB67u},
{6, 0, 134, 6, 0x834EF340u, 4, 0xCFF26639u},
{6, 1, 106, 7, 0xD7487491u, 2, 0x0B62ED13u},
{6, 2, 74, 0, 0xC59072D0u, 0, 0xC59072D0u},
{6, 3, 58, 1, 0x6767C96Fu, 4, 0x89F08E1Bu},
{7, 0, 154, 7, 0x3B42A575u, 4, 0x8B6F5794u},
{7, 1, 122, 0, 0x931CF7C0u, 4, 0x61821BB7u},
{7, 2, 86, 1, 0xD68566EFu, 7, 0x5D010551u},
{7, 3, 64, 2, 0xB9A3135Bu, 0, 0xC562E946u},
{8, 0, 192, 0, 0xF3E829BAu, 2, 0x157152DAu},
{8, 1, 152, 1, 0x574364E6u, 0, 0xCAF10D8Eu},
{8, 2, 108, 2, 0xCAC570D7u, 3, 0x63DFD7E8u},
{8, 3, 84, 3, 0x49FC9D25u, 4, 0x9FB87E7Fu},
{9, 0, 230, 1, 0x7C6F0571u, 2, 0x4A8C61F9u},
{9, 1, 180, 2, 0xF17E7982u, 4, 0xA12EE29Fu},
{9, 2, 130, 3, 0x813D90B9u, 1, 0x7F5DD80Eu},
{9, 3, 98, 4, 0x0A1DB8A5u, 4, 0x0A1DB8A5u},
{10, 0, 271, 2, 0xC9994AB1u, 1, 0x2781E952u},
{10, 1, 213, 3, 0x89F69F39u, 6, 0x27081332u},
{10, 2, 151, 4, 0xA8830A28u, 0, 0x0CBAE38Fu},
{10, 3, 119, 5, 0xA3774A83u, 3, 0x0E29C834u},
{11, 0, 321, 3, 0x990B2FCAu, 3, 0x990B2FCAu},
{11, 1, 251, 4, 0x814D5467u, 6, 0x8E47EF23u},
{11, 2, 177, 5, 0x777D29EFu, 1, 0x7C281B1Eu},
{11, 3, 137, 6, 0xF1F01AFAu, 4, 0xFEFAA1BEu},
{12, 0, 367, 4, 0x5D465C66u, 2, 0x8D7FFCB2u},
{12, 1, 287, 5, 0xB60E4A62u, 2, 0xF27ECA2Fu},
{12, 2, 203, 6, 0x66CFDE74u, 3, 0x87CFFA8Du},
{12, 3, 155, 7, 0x3D36515Cu, 2, 0xC20D3EA4u},
{13, 0, 425, 5, 0xD2466E62u, 0, 0x76775717u},
{13, 1, 331, 6, 0x0B904656u, 2, 0x493B93D1u},
{13, 2, 241, 7, 0xC1685606u, 6, 0xA18B267Du},
{13, 3, 177, 0, 0x2A002F60u, 2, 0x7A57B114u},
{14, 0, 458, 6, 0xC57CC7E6u, 0, 0x070B21CDu},
{14, 1, 362, 7, 0xFB3535A4u, 3, 0x7E89D9D4u},
{14, 2, 258, 0, 0xE46FBA8Bu, 1, 0x2F85AE78u},
{14, 3, 194, 1, 0x3A27E9F7u, 1, 0x3A27E9F7u},
{15, 0, 520, 7, 0x53F4391Bu, 4, 0x27774A7Cu},
{15, 1, 412, 0, 0x09062CC6u, 3, 0xD1AB34FBu},
{15, 2, 292, 1, 0x4DDA30E9u, 3, 0xDD096590u},
{15, 3, 220, 2, 0x9E4031A9u, 7, 0xE78BAEB8u},
{16, 0, 586, 0, 0x1A08F98Cu, 1, 0x6D4E5A33u},
{16, 1, 450, 1, 0xDAA1DA81u, 0, 0xADE7793Eu},
{16, 2, 322, 2, 0x156F1402u, 0, 0x4772992Bu},
{16, 3, 250, 3, 0xA82E76BCu, 0, 0x1770A5CCu},
{17, 0, 644, 1, 0x952745E1u, 7, 0xD7C63430u},
{17, 1, 504, 2, 0x93269B81u, 2, 0x93269B81u},
{17, 2, 364, 3, 0x209C7EAAu, 0, 0x39C95F85u},
{17, 3, 280, 4, 0x7B45989Du, 6, 0x06B879ECu},
{18, 0, 718, 2, 0x60FC3162u, 2, 0x60FC3162u},
{18, 1, 560, 3, 0x846E793Bu, 2, 0xA92F2AD3u},
{18, 2, 394, 4, 0xF0FCDD01u, 7, 0xD440CC78u},
{18, 3, 310, 5, 0xE21AA609u, 2, 0x71B7F51Fu},
{19, 0, 792, 3, 0x1DCBE4E0u, 2, 0x1662F1F6u},
{19, 1, 624, 4, 0x0A2BBB68u, 3, 0x6D844900u},
{19, 2, 442, 5, 0x536FC5AAu, 2, 0x968443E2u},
{19, 3, 338, 6, 0xF5671E69u, 0, 0xA421D58Du},
{20, 0, 858, 4, 0x987E7989u, 3, 0xA2EFD4EFu},
{20, 1, 666, 5, 0xFC2DD6B5u, 3, 0xAA1396E7u},
{20, 2, 482, 6, 0x7C03EBDDu, 5, 0x00578298u},
{20, 3, 382, 7, 0x3F46CE11u, 2, 0xB089538Eu},
{21, 0, 929, 5, 0x8F70DCAEu, 3, 0xC0A4B932u},
{21, 1, 711, 6, 0xC8F002DFu, 2, 0x3A255E83u},
{21, 2, 509, 7, 0x1F744D73u, 1, 0xF22E0D06u},
{21, 3, 403, 0, 0xEBB18191u, 6, 0x678E94F4u},
{22, 0, 1003, 6, 0x44A92E70u, 6, 0x44A92E70u},
{22, 1, 779, 7, 0x2F22F858u, 4, 0x62F0F73Bu},
{22, 2, 565, 0, 0x593D095Au, 7, 0xEF7D892Bu},
{22, 3, 439, 1, 0x32269F3Fu, 6, 0x4ACC0121u},
{23, 0, 1091, 7, 0xA20D0BD7u, 7, 0xA20D0BD7u},
{23, 1, 857, 0, 0x6E337213u, 0, 0x6E337213u},
{23, 2, 611, 1, 0xE3775DD7u, 0, 0xB47E0504u},
{23, 3, 461, 2, 0x1022648Au, 5, 0x0C41BB65u},
{24, 0, 1171, 0, 0x9132F5A8u, 7, 0xA97D711Cu},
{24, 1, 911, 1, 0x4ABA5485u, 4, 0x0C1CD825u},
{24, 2, 661, 2, 0x5F409BB0u, 6, 0xD45A3456u},
{24, 3, 511, 3, 0xC7A32F2Fu, 6, 0x9F5BF0A3u},
{25, 0, 1273, 1, 0xEAC2EEBFu, 6, 0x027B37DBu},
{25, 1, 997, 2, 0x8BDFA1CEu, 3, 0x9130B773u},
{25, 2, 715, 3, 0x944E3F36u, 7, 0x642E9C2Cu},
{25, 3, 535, 4, 0x0A80A50Fu, 4, 0x0A80A50Fu},
{26, 0, 1367, 2, 0x10A689FEu, 7, 0x2A9D2C32u},
{26, 1, 1059, 3, 0x410D9909u, 1, 0x90644E58u},
{26, 2, 751, 4, 0xAB79A910u, 3, 0xD9C3E974u},
{26, 3, 593, 5, 0x551D1DEFu, 4, 0xD21F3F8Cu},
{27, 0, 1465, 3, 0x6FB08D85u, 2, 0x57AF6CA2u},
{27, 1, 1125, 4, 0xE3C1C9A5u, 3, 0x40CECD3Fu},
{27, 2, 805, 5, 0x3BF47C06u, 6, 0x136F77DDu},
{27, 3, 625, 6, 0xC0B1925Au, 5, 0xE82A9981u},
{28, 0, 1528, 4, 0x6CC84453u, 6, 0x420DA7A7u},
{28, 1, 1190, 5, 0x53BBF40Cu, 0, 0xBD928264u},
{28, 2, 868, 6, 0x157F38F3u, 3, 0x5FA6BD5Bu},
{28, 3, 658, 7, 0xC871F4BFu, 3, 0xA07D325Bu},
{29, 0, 1628, 5, 0xF2601764u, 3, 0x19E1E45Du},
{29, 1, 1264, 6, 0xBD1112B7u, 3, 0xCE860C9Du},
{29, 2, 908, 7, 0x71E8CB64u, 1, 0xE6CEB3A0u},
{29, 3, 698, 0, 0x0235BE25u, 6, 0xC8261F67u},
{30, 0, 1732, 6, 0xEB0F84C7u, 5, 0xE966207Cu},
{30, 1, 1370, 7, 0xA7598639u, 4, 0xD63A18C9u},
{30, 2, 982, 0, 0xFA4B8CE4u, 7, 0x913C2B01u},
{30, 3, 742, 1, 0x5861D3D0u, 0, 0x8B9469B1u},
{31, 0, 1840, 7, 0x0D354547u, 3, 0xA195AB24u},
{31, 1, 1452, 0, 0xF448D885u, 2, 0xC76E5046u},
{31, 2, 1030, 1, 0x2B124108u, 6, 0x50921B35u},
{31, 3, 790, 2, 0x10B4DD89u, 6, 0x763E9268u},
{32, 0, 1952, 0, 0xA6FB8F87u, 5, 0xC6A001B5u},
{32, 1, 1538, 1, 0xB30988D4u, 2, 0x4B0C3AA0u},
{32, 2, 1112, 2, 0xB8ED5358u, 7, 0x5980A8ABu},
{32, 3, 842, 3, 0x14CC4E48u, 3, 0x14CC4E48u},
{33, 0, 2068, 1, 0x4AF71AC3u, 5, 0xBD604A99u},
{33, 1, 1628, 2, 0x47EEEA84u, 1, 0x8F66345Cu},
{33, 2, 1168, 3, 0x0B1A393Fu, 3, 0x0B1A393Fu},
{33, 3, 898, 4, 0x896B54A4u, 2, 0x1ACD23DBu},
{34, 0, 2188, 2, 0xA19C46A1u, 3, 0x90C524ABu},
{34, 1, 1722, 3, 0x72462A6Eu, 5, 0x51B53075u},
{34, 2, 1228, 4, 0xD33E8215u, 1, 0xD29CB0A7u},
{34, 3, 958, 5, 0xAF77A91Au, 2, 0xBDDDD10Bu},
{35, 0, 2303, 3, 0x976F0BB3u, 3, 0x976F0BB3u},
{35, 1, 1809, 4, 0x756FCFA7u, 3, 0xDDD0402Du},
{35, 2, 1283, 5, 0xD7AA68A3u, 6, 0x212341D1u},
{35, 3, 983, 6, 0x6F00917Cu, 2, 0x35FDED98u},
{36, 0, 2431, 4, 0x54C4A09Cu, 1, 0x0EB5C7DAu},
{36, 1, 1911, 5, 0x34D606DBu, 1, 0x784D5F22u},
{36, 2, 1351, 6, 0x873BE348u, 0, 0x4F5B8CC1u},
{36, 3, 1051, 7, 0x65877459u, 2, 0x97E55EE4u},
{37, 0, 2563, 5, 0x5F7F83F5u, 2, 0x324C8390u},
{37, 1, 1989, 6, 0x1AA78BEBu, 1, 0x0BAA537Bu},
{37, 2, 1423, 7, 0x035B3143u, 3, 0x04EECD26u},
{37, 3, 1093, 0, 0xD4C4A7D0u, 2, 0x61B670A3u},
{38, 0, 2699, 6, 0x380AF026u, 0, 0x307F55A9u},
{38, 1, 2099, 7, 0xBC3A5931u, 2, 0x434AAA7Cu},
{38, 2, 1499, 0, 0x56D4C8DFu, 2, 0x022B04FEu},
{38, 3, 1139, 1, 0x71C567A8u, 7, 0x3768E342u},
{39, 0, 2809, 7, 0xC4AD816Du, 3, 0x4A7E81CCu},
{39, 1, 2213, 0, 0x42A5B9BCu, 1, 0x0FE90862u},
{39, 2, 1579, 1, 0x6B48AC83u, 0, 0x26041D5Du},
{39, 3, 1219, 2, 0x36A9D5E3u, 4, 0xB0AA828Du},
{40, 0, 2953, 0, 0x58F19F69u, 6, 0xF81B351Eu},
{40, 1, 2331, 1, 0x23984174u, 4, 0x325D7721u},
{40, 2, 1663, 2, 0x8B402967u, 3, 0x33FEAD64u},
{40, 3, 1273, 3, 0x4581C065u, 2, 0xFD3F4466u},

};
uint32_t matrix_crc(const QrCode& q) {
  std::string all;
  for (const auto& r : q.rows()) all += r;
  return crc32(ByteView(std::string_view(all)));
}
Bytes payload(int v, int ei, int n) {
  Bytes b;
  for (int i = 0; i < n; ++i) b.push_back(static_cast<uint8_t>((i * 37 + v * 11 + ei * 5) & 255));
  return b;
}
}  // namespace

TEST_SUITE("tools") {

TEST_CASE("QR: 'EmberMesh' version 1-M matches reference matrix exactly") {
  const char* ref[] = {
    "111111101010101111111",
    "100000101001101000001",
    "101110100011001011101",
    "101110101001101011101",
    "101110100101001011101",
    "100000100001101000001",
    "111111101010101111111",
    "000000001101100000000",
    "101101110101101001011",
    "001011010111100011001",
    "000110111011010100011",
    "011100010101000111000",
    "110001110000101000010",
    "000000001001000011001",
    "111111101001110110100",
    "100000101110000001110",
    "101110100110101111100",
    "101110101101001011010",
    "101110101110101001100",
    "100000100110010010001",
    "111111101010010011100",
  };
  QrCode q;
  std::string err;
  REQUIRE(encode_text("EmberMesh", Ecc::M, &q, &err));
  CHECK(q.version == 1);
  CHECK(q.size == 21);
  CHECK(q.mask == 3);
  auto rows = q.rows();
  REQUIRE(rows.size() == 21);
  for (int i = 0; i < 21; ++i) CHECK(rows[static_cast<size_t>(i)] == ref[i]);
}

TEST_CASE("QR: all 160 version/ECC combinations at full capacity match the reference") {
  int checked = 0;
  for (const auto& v : kVecs) {
    CAPTURE(v.version);
    CAPTURE(v.ecc);
    Ecc e = static_cast<Ecc>(v.ecc);
    CHECK(byte_capacity(v.version, e) == v.capacity);
    Bytes data = payload(v.version, v.ecc, v.capacity);
    EncodeOptions o;
    o.ecc = e;
    o.force_mask = v.mask;
    QrCode q;
    std::string err;
    REQUIRE(encode(ByteView(data), o, &q, &err));
    CHECK(q.version == v.version);
    CHECK(q.size == 17 + 4 * v.version);
    CHECK(matrix_crc(q) == v.crc_forced);
    o.force_mask = -1;
    REQUIRE(encode(ByteView(data), o, &q, &err));
    CHECK(q.mask == v.auto_mask);
    CHECK(matrix_crc(q) == v.crc_auto);
    // One byte more no longer fits this version.
    data.push_back(0);
    o.min_version = o.max_version = v.version;
    CHECK_FALSE(encode(ByteView(data), o, &q, &err));
    ++checked;
  }
  CHECK(checked == 160);
}

TEST_CASE("QR: limits and options") {
  QrCode q;
  std::string err;
  Bytes big(2953, 'a');
  EncodeOptions o;
  o.ecc = Ecc::L;
  CHECK(encode(ByteView(big), o, &q, &err));
  CHECK(q.version == 40);
  big.push_back('a');
  CHECK_FALSE(encode(ByteView(big), o, &q, &err));
  CHECK_FALSE(err.empty());
  Bytes h(1274, 'a');
  o.ecc = Ecc::H;
  CHECK_FALSE(encode(ByteView(h), o, &q, &err));  // H max is 1273
  h.pop_back();
  CHECK(encode(ByteView(h), o, &q, &err));
  CHECK(q.version == 40);
  // Empty payload is valid (version 1).
  CHECK(encode(ByteView(), EncodeOptions(), &q, &err));
  CHECK(q.version == 1);
  // Invalid options.
  o = EncodeOptions();
  o.force_mask = 8;
  CHECK_FALSE(encode(ByteView(), o, &q, &err));
  o = EncodeOptions();
  o.min_version = 0;
  CHECK_FALSE(encode(ByteView(), o, &q, &err));
  o.min_version = 5;
  o.max_version = 4;
  CHECK_FALSE(encode(ByteView(), o, &q, &err));
  // min_version forces a larger symbol.
  o = EncodeOptions();
  o.min_version = 7;
  CHECK(encode_text("hi", Ecc::M, &q, &err));
  CHECK(encode(ByteView(std::string_view("hi")), o, &q, &err));
  CHECK(q.version == 7);
  // ECC boost raises the level when it still fits.
  o = EncodeOptions();
  o.ecc = Ecc::L;
  o.boost_ecc = true;
  CHECK(encode(ByteView(std::string_view("hi")), o, &q, &err));
  CHECK(q.ecc == Ecc::H);
  Ecc e;
  CHECK(parse_ecc("q", &e));
  CHECK(e == Ecc::Q);
  CHECK_FALSE(parse_ecc("X", &e));
  CHECK_FALSE(parse_ecc("", &e));
  CHECK(byte_capacity(0, Ecc::L) == 0);
  CHECK(byte_capacity(41, Ecc::L) == 0);
}

TEST_CASE("QR: structural invariants (finders, timing, dark module)") {
  QrCode q;
  std::string err;
  REQUIRE(encode_text("https://embermesh.example/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", Ecc::H, &q, &err));
  CHECK(q.version == 9);
  int n = q.size;
  // Finder centres dark, ring at distance 2 light.
  CHECK(q.get(3, 3));
  CHECK(q.get(n - 4, 3));
  CHECK(q.get(3, n - 4));
  CHECK_FALSE(q.get(1, 3));
  // Timing pattern alternates.
  for (int i = 8; i < n - 8; ++i) {
    CHECK(q.get(i, 6) == (i % 2 == 0));
    CHECK(q.get(6, i) == (i % 2 == 0));
  }
  CHECK(q.get(8, n - 8));  // dark module
}

}  // TEST_SUITE

