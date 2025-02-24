#pragma once
#include <inttypes.h>

#include <cpinternals/CFact.hpp>
#include <csav/node.hpp>
#include <csav/serializers.hpp>


namespace CSAV::Nodes {

struct FactsTable
  : public node_serializable
{
  FactsTable() = default;

  std::string node_name() const override { return "FactsTable"; }

  const std::vector<CP::CFact>& facts() const { return m_facts; }
  std::vector<CP::CFact>& facts() { return m_facts; }

protected:
  bool from_node_impl(const std::shared_ptr<const node_t>& node, const csav_version& version) override
  {
    if (!node)
      return false;

    m_raw = node;

    node_reader reader(node, version);

    size_t cnt = 0;
    reader >> cp_packedint_ref((int64_t&)cnt);

    std::vector<uint32_t> m_arr1; m_arr1.resize(cnt);
    std::vector<uint32_t> m_arr2; m_arr2.resize(cnt);

    reader.read((char*)m_arr1.data(), 4 * cnt);
    reader.read((char*)m_arr2.data(), 4 * cnt);

    m_facts.reserve(cnt);
    auto it2 = m_arr2.begin();
    for (auto& it1 : m_arr1)
    {
      m_facts.emplace_back(it1, *(it2++));
    }

    return reader.at_end();
  }

  std::shared_ptr<const node_t> to_node_impl(const csav_version& version) const override
  {
    node_writer writer(version);

    try
    {
      size_t cnt = m_facts.size();
      writer << cp_packedint_ref((int64_t&)cnt);

      std::vector<uint32_t> m_arr1; m_arr1.reserve(cnt);
      std::vector<uint32_t> m_arr2; m_arr2.reserve(cnt);
      
      for (auto& f : m_facts)
      {
        m_arr1.emplace_back(f.hash());
        m_arr2.emplace_back(f.value());
      }

      writer.write((char*)m_arr1.data(), 4 * cnt);
      writer.write((char*)m_arr2.data(), 4 * cnt);
    }
    catch (std::exception&)
    {
      return nullptr;
    }

    return writer.finalize(node_name());
  }

  std::vector<CP::CFact> m_facts;

  // Temporary
  std::shared_ptr<const node_t> m_raw;
};

} // namespace CSAV::Nodes

