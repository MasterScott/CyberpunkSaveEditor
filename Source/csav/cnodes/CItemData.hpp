#pragma once
#include <inttypes.h>
#include <iostream>

#include <cpinternals/cpnames.hpp>
#include <csav/node.hpp>
#include <csav/serializers.hpp>


#pragma pack(push, 1)

struct uk_thing
{
  uint32_t uk4 = 0;
  uint8_t  uk1 = 0;
  uint16_t uk2 = 0;

  friend std::istream& operator>>(std::istream& reader, uk_thing& kt)
  {
    reader >> cbytes_ref(kt.uk4);
    reader >> cbytes_ref(kt.uk1);
    reader >> cbytes_ref(kt.uk2);
    return reader;
  }

  friend std::ostream& operator<<(std::ostream& writer, const uk_thing& kt)
  {
    writer << cbytes_ref(kt.uk4);
    writer << cbytes_ref(kt.uk1);
    writer << cbytes_ref(kt.uk2);
    return writer;
  }

  uint8_t kind() const
  {
    if (uk1 == 1) return 2;
    if (uk1 == 2) return 1;
    if (uk1 == 3) return 0;
    return uk4 != 2 ? 2 : 1;
  }
};

struct CItemID
{
  TweakDBID nameid;
  uk_thing uk;

  CItemID()
    : nameid(), uk()
  {
  }

  friend std::istream& operator>>(std::istream& reader, CItemID& iid)
  {
    reader >> cbytes_ref(iid.nameid.as_u64);
    reader >> iid.uk;
    return reader;
  }

  friend std::ostream& operator<<(std::ostream& writer, const CItemID& iid)
  {
    writer << cbytes_ref(iid.nameid.as_u64);
    writer << iid.uk;
    return writer;
  }

  std::string name() const
  {
    return nameid.name();
  }

  std::string shortname() const
  {
    return nameid.name();
  }
};

struct CUk0ID
{
  TweakDBID nameid;
  uint32_t uk0 = 0;
  float weird_float = FLT_MAX;

  CUk0ID()
    : nameid()
  {
  }

  bool serialize_in(std::istream& reader, const csav_version& ver)
  {
    reader >> cbytes_ref(nameid.as_u64);
    reader >> cbytes_ref(uk0);
    reader >> cbytes_ref(weird_float);
    return true;
  }

  bool serialize_out(std::ostream& writer, const csav_version& ver) const
  {
    writer << cbytes_ref(nameid.as_u64);
    writer << cbytes_ref(uk0);
    writer << cbytes_ref(weird_float);
    return true;
  }

  std::string name() const
  {
    return nameid.name();
  }

  std::string shortname() const
  {
    return nameid.name();
  }
};

#pragma pack(pop)

struct CItemMod // for CItemData kind 0, 2
{
  CItemID iid;
  char cn0[256];
  TweakDBID tdbid1;
  std::list<CItemMod> subs;
  uint32_t uk2 = 0;

  CUk0ID uk3;

  CItemMod()
    : iid(), tdbid1(), uk3()
  {
    std::fill(cn0, cn0 + sizeof(cn0), 0);
  }

  bool serialize_in(std::istream& reader, const csav_version& ver)
  {
    reader >> iid;

    std::string s;
    reader >> cp_plstring_ref(s);
    strcpy_s(cn0, s.c_str());
    reader >> tdbid1;

    size_t cnt = 0;
    reader >> cp_packedint_ref((int64_t&)cnt);
    subs.resize(cnt);
    for (auto& sub : subs)
      sub.serialize_in(reader, ver);

    reader >> cbytes_ref(uk2);

    if (ver.v1 >= 192)
      uk3.serialize_in(reader, ver);
    
    return true;
  }

  bool serialize_out(std::ostream& writer, const csav_version& ver) const
  {
    writer << iid;

    std::string s = cn0;
    writer << cp_plstring_ref(s);
    writer << tdbid1;

    const size_t cnt = subs.size();
    writer << cp_packedint_ref((int64_t&)cnt);
    for (auto& sub : subs)
      sub.serialize_out(writer, ver);

    writer << cbytes_ref(uk2);

    if (ver.v1 >= 192)
      uk3.serialize_out(writer, ver);

    return true;
  }
};

struct CItemData
  : public node_serializable
{
  // CP 1.06
  // serial func at vtbl+0x128
  // ikind0 vtbl 0x143244E30 -> 0x143244F58
  // ikind1 vtbl 0x143244A20 -> 0x143244B48
  // ikind2 vtbl 0x143244C28 -> 0x143244D50

  CItemID iid;

  uint8_t  flags = 0;
  uint32_t uk1_012 = 0;

  // kind 0,1 stuff
  uint32_t quantity = 0;

  // kind 0,2 stuff
  CUk0ID uk3;

  CItemMod root2;

  std::string node_name() const override { return "itemData"; }

  bool from_node_impl(const std::shared_ptr<const node_t>& node, const csav_version& version) override
  {
    if (!node)
      return false;

    node_reader reader(node, version);
    reader >> iid;
    auto kind = iid.uk.kind();

    reader >> cbytes_ref(flags);
    reader >> cbytes_ref(uk1_012);

    if (kind != 2)
      reader >> cbytes_ref(quantity);

    if (kind != 1)
    {
      // uk0id
      if (!uk3.serialize_in(reader, version))
        return false;
      // recursive
      if (!root2.serialize_in(reader, version))
        return false;
    }

    return true;
  }

  std::shared_ptr<const node_t> to_node_impl(const csav_version& version) const override
  {
    node_writer writer(version);
    writer << iid;
    auto kind = iid.uk.kind();

    writer << cbytes_ref(flags);
    writer << cbytes_ref(uk1_012);

    if (kind != 2)
      writer << cbytes_ref(quantity);

    if (kind != 1)
    {
      uk3.serialize_out(writer, version);
      root2.serialize_out(writer, version);
    }

    return writer.finalize(node_name());
  }

  std::string name() const
  {
    return iid.name();
  }
};


