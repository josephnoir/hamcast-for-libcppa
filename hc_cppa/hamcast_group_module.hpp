#include <map>
#include <thread>

#include "cppa/cppa.hpp"
#include "cppa/group.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"

#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"
#include "cppa/util/upgrade_lock_guard.hpp"

#include "hamcast/hamcast.hpp"

namespace cppa {

class hamcast_group;

class hamcast_group_module : public group::module {

    typedef group::module super;

 public:

    hamcast_group_module();
    group_ptr get(const std::string& identifier);
    intrusive_ptr<group> deserialize(deserializer* source);
    void serialize(hamcast_group* what, serializer* sink);

 private:

    typedef std::map<std::string, group_ptr> hamcast_group_map;

    const uniform_type_info* m_actor_utype;
    util::shared_spinlock m_instance_mtx;
    hamcast_group_map m_instances;
    util::shared_spinlock m_proxies_mtx;
    std::map<process_information, hamcast_group_map> m_proxies;

};

std::unique_ptr<group::module> make_hamcast_group_module();

} // namespace cppa
