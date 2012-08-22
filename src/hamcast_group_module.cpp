#include <stdexcept>

#include "hc_cppa/hamcast_group_module.hpp"

#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"

//#include "hamcast/hamcast.hpp"

namespace {

namespace cppa {

hamcast_group::hamcast_group(hamcast_group_module* mod, std::string id,
                             process_information_ptr parent = process_information::get())
    : m_sck(), m_recv_thread(recv_loop){
    m_sck.join(id);
}

void hamcast_group::revc_loop() {
    for(;;) {
        hamcast::multicast_packet mcp(m_sck.receive());
        actor_ptr src;
        {
            cppa::binary_deserializer bd(mcp.data(), sizeof actor_ptr);
            object tmp;
            bd >> tmp;
            src = get<actor_ptr>(tmp);
        }
        any_tuple msg;
        {
            cppa::binary_deserializer bd(mcp.data()+sizeof actor_ptr, mcp.size-sizeof actor_ptr);
//            object tmp;
//            bd >> tmp;
//            msg = get<any_tuple>(tmp);
            uniform_typeid<any_tuple>()->deserialize(&msg, &bd);
        }
        send_all_subscribers(&src, msg);
    }
}

void hamcast_group::send_all_subscribers(actor* sender, const any_tuple& msg) {
    shared_guard guard(m_shared_mtx);
    for(auto& s : m_subscribers) {
        s->enqueue(sender, msg);
    }
}

void hamcast_group::enqueue(actor* sender, any_tuple msg) {
    //serialize
    util::buffer wr_buf;
    {
        binary_serializer bs(&wr_buf);
        bs << *sender;  // is this right?
        bs << msg;
    }
    m_sck.send(m_identifier, wr_buf.size(), wr_buf.data());
}

std::pair<bool, size_t> hamcast_group::add_subscriber(const channel_ptr& who) {
    exclusive_guard guard(m_shared_mtx);
    if(m_subscribers.insert(who).second){
        return {true, m_subscribers.size()};
    }
    return {false, m_subscribers.size()};
}

std::pair<bool, size_t> hamcast_group::erase_subscriber(const channel_ptr& who) {
    exclusive_guard guard guard(m_shared_mtx);
    auto erased_one = m_subscribers.erase(who) > 0;
    return {erased_one, m_subscribers.size()};
}

group::subscription hamcast_group::subscribe(const channel_ptr& who) {
    if(add_subscriber(who).first) {
        return {who, this};
    }
    return {};
}

void hamcast_group::unsubscribe(const channel_ptr& who) {
    erase_subscriber(who);
}

//void hamcast_group::serialize(serializer* sink) {
//
//}

inline const process_information& hamcast_group::process() const {
    return *m_process;
}

inline const process_information_ptr& hamcast_group::process_ptr() const {
    return m_process;
}

} // namespace cppa

using namespace cppa;


class hamcast_group_module : public group::module {

    typedef intrusive_ptr<hamcast_group> hamcast_group_ptr;
    typedef group::module super;

 public:

    hamcast_group_module()
    : super("hamcast"), m_actor_utype(uniform_typeid<actor_ptr>()){ }

    group_ptr get(const std::string& identifier) {
        shared_guard guard(m_instance_mtx);
        auto i = m_instances.find(identifier);
        if(i != m_instances.end()) {
            return i->second;
        }
        else {
            // todo: check if identifier is a valid uri -> throw exception
            if(hamcast::uri(identifier).empty()) {
                throw std::invalid_argument("Identifer must be a valid URI.");
            }
            hamcast_group_ptr tmp(new hamcast_group(this, identifier));
            {
                upgrade_guard uguard(guard);
                auto p  = m_instances.insert(std::make_pair(identifider, tmp));
                return p.first->second;
            }
        }
    }

    intrusive_ptr<group> deserialize(deserializer* source) {
        auto pv_identifier = source->read_value(pt_u8string);
        auto& identifier = cppa::get<std::string>(pv_identifier);
        try {
            return get(identifier);
        } catch (...) {
            return nullptr;
        }
    }

    void serialize(hamcast_group* ptr, serializer* sink) {
        sink->write_value(ptr->identifier());
    }

 private:

    typedef std::map<std::string, hamcast_group_ptr> hacmast_group_map;

    const uniform_type_info* m_actor_utype;
    util::shared_spinlock m_instance_mtx;
    hamcast_group_map m_instances;
    util::shared_spinlock m_proxies_mtx;
    std::map<process_information, hamcast_group_module> m_proxies;

};


} // namespace anonymous
