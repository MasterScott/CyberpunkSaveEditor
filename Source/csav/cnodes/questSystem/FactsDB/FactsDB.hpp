#pragma once
#include <inttypes.h>

#include <cpinternals/cpnames.hpp>
#include <csav/node.hpp>
#include <csav/serializers.hpp>
#include <csav/csystem/CSystem.hpp>

#include <csav/cnodes/questSystem/FactsDB/FactsTable.hpp>

namespace CSAV::Nodes {

struct FactsDB
  : public node_serializable
{
  FactsDB() = default;

  std::string node_name() const override { return "FactsDB"; }

  const std::vector<FactsTable>& tables() const { return m_tables; }
  std::vector<FactsTable>& tables() { return m_tables; }

protected:
  bool from_node_impl(const std::shared_ptr<const node_t>& node, const csav_version& version) override
  {
    if (!node)
      return false;

    m_raw = node;

    node_reader reader(node, version);

    size_t cnt = 0;
    reader >> cp_packedint_ref((int64_t&)cnt);
    if (cnt > 10)
      cnt = 10;

    m_tables.resize(cnt);

    for (auto& tbl : m_tables)
    {
      auto tbl_node = reader.read_child("FactsTable");
      if (!tbl_node)
        return false;

      if (!tbl.from_node(tbl_node, version))
        return false;
    }

    return reader.at_end();
  }

  std::shared_ptr<const node_t> to_node_impl(const csav_version& version) const override
  {
    node_writer writer(version);

    try
    {
      size_t cnt = m_tables.size();
      writer << cp_packedint_ref((int64_t&)cnt);

      for (auto& tbl : m_tables)
      {
        auto tbl_node = tbl.to_node(version);
        if (!tbl_node)
          return nullptr;

        writer.write_child(tbl_node);
      }
    }
    catch (std::exception&)
    {
      return nullptr;
    }

    return writer.finalize(node_name());
  }

  std::vector<FactsTable> m_tables;

  // Temporary
  std::shared_ptr<const node_t> m_raw;
};

} // namespace CSAV::Nodes

