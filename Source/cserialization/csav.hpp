#pragma once
#include <filesystem>
#include <iostream>
#include <fstream>
#include <numeric>
#include <cassert>
#include "xlz4/lz4.h"
#include "packing.hpp"
#include "node.hpp"

#define XLZ4_CHUNK_SIZE 0x40000

struct node_desc
{
  std::string name;
  int32_t next_idx, child_idx;
  uint32_t data_offset, data_size;

  friend std::istream& operator>>(std::istream& is, node_desc& ed)
  {
    ed.name = read_str(is);
    is.read((char*)&ed.next_idx, 16);
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, const node_desc& ed)
  {
    write_str(os, ed.name);
    os.write((char*)&ed.next_idx, 16);
    return os;
  }
};

struct compressed_chunk_desc
{
  static const size_t serialized_size = 12;

  // data_size is uncompressed size
  uint32_t offset, size, data_size, data_offset;

  friend std::istream& operator>>(std::istream& is, compressed_chunk_desc& cd)
  {
    is.read((char*)&cd.offset, 12);
    cd.data_offset = 0;
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, const compressed_chunk_desc& cd)
  {
    os.write((char*)&cd.offset, 12);
    return os;
  }
};

class csav
{
public:
  std::filesystem::path filepath;
  uint32_t v1, v2, v3, uk0, uk1;
  std::string suk;
  std::vector<node_desc> node_descs;
  std::shared_ptr<const node_t> root_node;

public:
  bool open_with_progress(std::filesystem::path path, float& progress);
  bool save_with_progress(std::filesystem::path path, float& progress);

protected:
  std::shared_ptr<const node_t> make_blob_node(const std::vector<char>& nodedata, uint32_t start_offset, uint32_t end_offset)
  {
    auto node = node_t::create_shared(BLOB_NODE_IDX, "datablob");
    auto& nc_node = node->nonconst();

    nc_node.data().assign(
      nodedata.begin() + start_offset,
      nodedata.begin() + end_offset
    );

    return node;
  }

  std::shared_ptr<const node_t> read_node(const std::vector<char>& nodedata, node_desc& desc, int32_t idx)
  {
    uint32_t cur_offset = desc.data_offset + 4;
    uint32_t end_offset = desc.data_offset + desc.data_size;

    if (end_offset > nodedata.size())
      return nullptr;
    if (*(uint32_t*)(nodedata.data() + desc.data_offset) != idx && idx != ROOT_NODE_IDX)
      return nullptr;

    auto node = node_t::create_shared(idx, desc.name);
    auto& nc_node = node->nonconst();
    auto& nc_children = nc_node.children();

    if (desc.child_idx >= 0)
    {
      int last, i = desc.child_idx;
      while (i >= 0)
      {
        last = i;
        if (i >= node_descs.size()) // corruption ?
          return nullptr;

        auto& childdesc = node_descs[i];

        if (childdesc.data_offset > cur_offset) {
          nc_children.push_back(
            make_blob_node(nodedata, cur_offset, childdesc.data_offset)
          );
        }

        auto childnode = read_node(nodedata, childdesc, i);
        if (!childnode) // something went wrong
          return nullptr;
          nc_node.children().push_back(childnode);

        cur_offset = childdesc.data_offset + childdesc.data_size;
        i = childdesc.next_idx;
      }

      if (cur_offset < end_offset) {
        nc_children.push_back(
          make_blob_node(nodedata, cur_offset, end_offset)
        );
      }
    }
    else if (cur_offset < end_offset)
    {
      nc_node.data().assign(
        nodedata.begin() + cur_offset,
        nodedata.begin() + end_offset
      );
    }

    return node;
  }

  node_desc* write_node_visitor(std::vector<char>& nodedata, const node_t& node, uint32_t& next_idx)
  {
    if (node.idx() >= 0)
    {
      const uint32_t idx = next_idx++;
      node.nonconst().idx(idx);

      auto& nd = node_descs[idx];
      nd.name = node.name();
      nd.data_offset = (uint32_t)nodedata.size();
      nd.child_idx = node.has_children() ? next_idx : NULL_NODE_IDX;

      char* pIdx = (char*)&idx;
      std::copy(pIdx, pIdx + 4, std::back_inserter(nodedata));
      std::copy(node.data().begin(), node.data().end(), std::back_inserter(nodedata));

      write_node_children(nodedata, node, next_idx);

      nd.next_idx = (next_idx < node_descs.size()) ? next_idx : NULL_NODE_IDX;
      nd.data_size = (uint32_t)nodedata.size() - nd.data_offset;
      return &nd;
    }
    else
    {
      // data blob
      std::copy(node.data().begin(), node.data().end(), std::back_inserter(nodedata));
    }
    return nullptr;
  }

  void write_node_children(std::vector<char>& nodedata, const node_t& node, uint32_t& next_idx)
  {
    node_desc* last_child_desc = nullptr;
    for (auto& c : node.children())
    {
      auto cnd = write_node_visitor(nodedata, *c, next_idx);
      if (cnd != nullptr)
        last_child_desc = cnd;
    }
    if (last_child_desc)
      last_child_desc->next_idx = NULL_NODE_IDX;
  }
};

