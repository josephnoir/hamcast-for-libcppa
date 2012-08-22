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

using namespace cppa;

typedef std::lock_guard<util::shared_spinlock> exclusive_guard;
typedef util::shared_lock_guard<util::shared_spinlock> shared_guard;
typedef util::upgrade_lock_guard<util::shared_spinlock> upgrade_guard;

class hamcast_group_module;

class hamcast_group : public group {

 private:
    void revc_loop();

 protected:

    process_information_ptr m_process;
    util::shared_spinlock m_shared_mtx;
    std::set<channel_ptr> m_subscribers;
    hamcast::multicast_socket m_sck;
    std::thread m_recv_thread;

 public:

    hamcast_group(hamcast_group_module* mod, std::string id,
                  process_information_ptr parent = process_information::get());

    void send_all_subscribers(actor* sender, const any_tuple& msg);
    void enqueue(actor* sender, any_tuple msg);

    std::pair<bool, size_t> add_subscriber(const channel_ptr& who);
    std::pair<bool, size_t> erase_subscriber(const channel_ptr& who);
    group::subscription subscribe(const channel_ptr& who);
    void unsubscribe(const channel_ptr& who);

    void serialize(serializer* sink);
    inline const process_information& process() const;
    inline const process_information_ptr& process_ptr() const;
};

} // namespace cppa
