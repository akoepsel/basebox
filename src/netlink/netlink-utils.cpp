/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cassert>
#include <map>
#include <memory>
#include <vector>

#include <glog/logging.h>
#include <netlink/route/link.h>

#include "netlink-utils.hpp"

#define lt_names "unknown", "unsupported", "bridge", "tun", "vlan"

namespace basebox {

std::map<std::string, enum link_type> kind2lt;
std::vector<std::string> lt2names = {lt_names};

static void init_kind2lt_map() {
  enum link_type lt = LT_UNKNOWN;
  for (const auto &n : lt2names) {
    assert(lt != LT_MAX);
    kind2lt.emplace(n, lt);
    lt = static_cast<enum link_type>(lt + 1);
  }
}

enum link_type kind_to_link_type(const char *type) noexcept {
  if (type == nullptr)
    return LT_UNKNOWN;

  assert(lt2names.size() == LT_MAX);

  if (kind2lt.size() == 0) {
    init_kind2lt_map();
  }

  auto it = kind2lt.find(std::string(type)); // XXX string_view

  if (it != kind2lt.end())
    return it->second;

  VLOG(1) << __FUNCTION__ << ": type=" << type << " not supported";
  return LT_UNSUPPORTED;
}

void get_bridge_ports(int br_ifindex, struct nl_cache *link_cache,
                      std::deque<rtnl_link *> *link_list) noexcept {
  assert(link_cache);
  assert(link_list);

  std::unique_ptr<rtnl_link, void (*)(rtnl_link *)> filter(rtnl_link_alloc(),
                                                           &rtnl_link_put);
  assert(filter && "out of memory");

  rtnl_link_set_family(filter.get(), AF_BRIDGE);
  rtnl_link_set_master(filter.get(), br_ifindex);

  nl_cache_foreach_filter(link_cache, OBJ_CAST(filter.get()),
                          [](struct nl_object *obj, void *arg) {
                            assert(arg);
                            std::deque<rtnl_link *> *list =
                                static_cast<std::deque<rtnl_link *> *>(arg);

                            VLOG(3) << __FUNCTION__ << ": found bridge port "
                                    << obj;
                            list->push_back(LINK_CAST(obj));
                          },
                          link_list);
}

} // namespace basebox
