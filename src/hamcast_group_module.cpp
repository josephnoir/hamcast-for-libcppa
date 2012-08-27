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
            cppa::binary_deserializer bd(reinterpret_cast<const char*>(mcp.data()), mcp.size());
            actor_ptr src;
            arr[0]->deserialize(&src, &bd);
            any_tuple msg;
            arr[1]->deserialize(&msg, &bd);
            //object src_obj;
            //bd >> src_obj;
            //auto src = ::cppa::get<actor_ptr>(src_obj);
            //object msg_obj;
            //bd >> msg_obj;
            //auto msg = ::cppa::get<any_tuple>(msg_obj);
            send_all_subscribers(src.get(), msg);
        }
    }

 protected:

    process_information_ptr m_process;
    util::shared_spinlock m_shared_mtx;
    set<channel_ptr> m_subscribers;
    hamcast::multicast_socket m_sck;
    thread m_recv_thread;

 public:

//    hamcast_group(hamcast_group_module* mod, string id, process_information_ptr parent = process_information::get())
//    : group(mod, move(id)), m_process(move(parent)), m_sck() {
//        m_recv_thread = thread(run_recv_loop, this);
//        m_sck.join(id);
//    }

    hamcast_group(hamcast_group_module* mod, string id, process_information_ptr parent = process_information::get())
    : group(mod, move(id)), m_process(move(parent)), m_sck() {
        cout << "[hamcast_group] with arguments: id:" << m_identifier << endl;
        cout << "[hamcast_group] starting receive loop" << endl;
        m_recv_thread = thread([&](){ recv_loop(); });
        cout << "[hamcast_group] joining group" << endl;
        m_sck.join(m_identifier);
        cout << "[hamcast_group] group created" << endl;
    }

    void send_all_subscribers(actor* sender, const any_tuple& msg) {
        shared_guard guard(m_shared_mtx);
        for(auto& s : m_subscribers) {
            s->enqueue(sender, msg);
        }
    }

    void enqueue(actor* sender, any_tuple msg) {
        //serialize
        actor_ptr ptr = sender;
        util::buffer wr_buf;
        binary_serializer bs(&wr_buf);
        bs << ptr;
        bs << msg;
        // skip first 4 bytes, because binary_serializer stores the size
        // of the remaining buffer
        m_sck.send(m_identifier, wr_buf.size(), wr_buf.data());
    }

    pair<bool, size_t> add_subscriber(const channel_ptr& who) {
        exclusive_guard guard(m_shared_mtx);
        if(m_subscribers.insert(who).second){
            return {true, m_subscribers.size()};
        }
        return {false, m_subscribers.size()};
    }

    pair<bool, size_t> erase_subscriber(const channel_ptr& who) {
        exclusive_guard guard(m_shared_mtx);
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

void run_recv_loop(hamcast_group* _this) {
    _this->recv_loop();
}

hamcast_group_module::hamcast_group_module()
: super("hamcast"), m_actor_utype(uniform_typeid<actor_ptr>()){ }

group_ptr hamcast_group_module::get(const string& identifier) {
    cout << "[hc_group_get] for " << identifier << endl;
    shared_guard guard(m_instance_mtx);
    auto i = m_instances.find(identifier);
    if(i != m_instances.end()) {
        cout << "[hc_group_get] found group for " << identifier << endl;
        return i->second;
    }
    else {
        cout << "[hc_group_get] NO group found for " << identifier << endl;
        if(hamcast::uri(identifier).empty()) {
            throw invalid_argument("Identifer must be a valid URI.");
        }
        cout << "[hc_group_get] " << identifier << " is a valid URI, creating new group." << endl;
        group_ptr tmp(new hamcast_group(this, identifier));
        cout << "[hc_group_get] created group for " << identifier << "adding to 'known' groups." << endl;
        {
            upgrade_guard uguard(guard);
            auto p  = m_instances.insert(make_pair(identifier, tmp));
            cout << "[hc_group_get] returning" << endl;
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

} // namespace cppa

