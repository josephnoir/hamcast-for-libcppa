#include <map>
#include <set>
#include <thread>
#include <stdexcept>

#include "cppa/cppa.hpp"
#include "cppa/group.hpp"
#include "cppa/object.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"
#include "cppa/util/upgrade_lock_guard.hpp"

#include "cppa/network/default_actor_addressing.hpp"

#include "hamcast/hamcast.hpp"

#include "hc_cppa/hamcast_group_module.hpp"

using namespace std;

namespace cppa {

typedef lock_guard<util::shared_spinlock> exclusive_guard;
typedef util::shared_lock_guard<util::shared_spinlock> shared_guard;
typedef util::upgrade_lock_guard<util::shared_spinlock> upgrade_guard;

class hamcast_group;

void run_recv_loop(hamcast_group* _this);

class hamcast_group : public group {

    friend void run_recv_loop(hamcast_group*);

 private:

    void recv_loop() {
        auto& arr = detail::static_types_array<actor_ptr, any_tuple>::arr;
        for(;;) {
            hamcast::multicast_packet mcp = m_sck.receive();
            cppa::binary_deserializer bd(reinterpret_cast<const char*>(mcp.data()), mcp.size(), &m_addressing);
            actor_ptr src;
            any_tuple msg;
            { // lifetime scope of guard
                exclusive_guard guard(m_addressing_mtx);
                arr[0]->deserialize(&src, &bd);
                arr[1]->deserialize(&msg, &bd);
            }
            send_all_subscribers(src.get(), msg);
        }
    }

 protected:

    process_information_ptr m_process;
    set<channel_ptr> m_subscribers;
    hamcast::multicast_socket m_sck;
    thread m_recv_thread;
    network::default_actor_addressing m_addressing;

    util::shared_spinlock m_subscribers_mtx;
    util::shared_spinlock m_addressing_mtx;

 public:
    hamcast_group(hamcast_group_module* mod, string id, process_information_ptr parent = process_information::get())
    : group(mod, move(id)), m_process(move(parent)), m_sck() {
        m_recv_thread = thread([&](){ recv_loop(); });
        m_sck.join(m_identifier);
    }

    void send_all_subscribers(actor* sender, const any_tuple& msg) {
        shared_guard guard(m_subscribers_mtx);
        for(auto& s : m_subscribers) {
            s->enqueue(sender, msg);
        }
    }

    void enqueue(actor* sender, any_tuple msg) {
        //serialize
        actor_ptr ptr = sender;
        util::buffer wr_buf;
        binary_serializer bs(&wr_buf, &m_addressing);
        { // lifetime scope of guard
            exclusive_guard guard(m_addressing_mtx);
            bs << ptr;
            bs << msg;
        }
        // skip first 4 bytes, because binary_serializer stores the size
        // of the remaining buffer
        m_sck.send(m_identifier, wr_buf.size(), wr_buf.data());
    }

    pair<bool, size_t> add_subscriber(const channel_ptr& who) {
        exclusive_guard guard(m_subscribers_mtx);
        if(m_subscribers.insert(who).second){
            return {true, m_subscribers.size()};
        }
        return {false, m_subscribers.size()};
    }

    pair<bool, size_t> erase_subscriber(const channel_ptr& who) {
        exclusive_guard guard(m_subscribers_mtx);
        auto erased_one = m_subscribers.erase(who) > 0;
        return {erased_one, m_subscribers.size()};
    }

    group::subscription subscribe(const channel_ptr& who) {
        if(add_subscriber(who).first) {
            return {who, this};
        }
        return {};
    }

    void unsubscribe(const channel_ptr& who) {
        erase_subscriber(who);
    }

    void serialize(serializer* sink) {
        static_cast<hamcast_group_module*>(m_module)->serialize(this, sink);
    }

    inline const process_information& process() const {
        return *m_process;
    }

    inline const process_information_ptr& process_ptr() const {
        return m_process;
    }

};

hamcast_group_module::hamcast_group_module()
: super("hamcast"), m_actor_utype(uniform_typeid<actor_ptr>()){ }

group_ptr hamcast_group_module::get(const string& identifier) {
    shared_guard guard(m_instance_mtx);
    auto i = m_instances.find(identifier);
    if(i != m_instances.end()) {
        return i->second;
    }
    else {
        if(hamcast::uri(identifier).empty()) {
            throw invalid_argument("Identifer must be a valid URI.");
        }
        group_ptr tmp(new hamcast_group(this, identifier));
        {
            upgrade_guard uguard(guard);
            auto p  = m_instances.insert(make_pair(identifier, tmp));
            return p.first->second;
        }
    }
}

intrusive_ptr<group> hamcast_group_module::deserialize(deserializer* source) {
    auto pv_identifier = source->read_value(pt_u8string);
    auto& identifier = cppa::get<string>(pv_identifier);
    try {
        return get(identifier);
    } catch (...) {
        return nullptr;
    }
}

void hamcast_group_module::serialize(hamcast_group* ptr, serializer* sink) {
    sink->write_value(ptr->identifier());
}

std::unique_ptr<group::module> make_hamcast_group_module() {
    return std::unique_ptr<group::module>(new hamcast_group_module);
}


} // namespace cppa

