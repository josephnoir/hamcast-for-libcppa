#include <set>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <time.h>
#include <cstdlib>

#include "cppa/cppa.hpp"
#include "cppa/match.hpp"

#include "hc_cppa/hamcast_group_module.hpp"

using namespace cppa;
using namespace std;

class server : public event_based_actor {

    typedef map<actor_ptr, string> user_map;

    actor_ptr m_printer;
    user_map  m_users;
    uint16_t  m_port;

    void init() {
        become (
            // server commands
            on(atom("quit")) >> [=]() {
                quit();
            },
            on(atom("info")) >> [=]() {
                    stringstream  strstr;
                    strstr << "Port: \n > " << m_port << "\nConnected users:\n";
                    for(auto& user : m_users) {
                        strstr << " > " << user.second << "\n";
                    }
                    send(m_printer, strstr.str());
            },

            // from clients
            on(atom("request"), atom("local"), arg_match) >> [=](const string& groupname) {
                send(m_printer, "[LOG] Someone requested group information for: '"+groupname+"'.");
                auto group = group::get("local", groupname);
                join(group);
                reply(atom("answer"), groupname, group);
            },
            on(atom("request"), atom("hamcast"), arg_match) >> [=](const string& groupname) {
                send(m_printer, "[LOG] Someone requested group information for: '"+groupname+"'.");
                auto group = group::get("hamcast", groupname);
                join(group);
                //auto group = group::get("local", groupname);
                //join(group);
                reply(atom("answer"), groupname, group);

            },
            on(atom("join"), arg_match) >> [=](const string& username){
                m_users[last_sender()] = username;
                monitor(last_sender());
            },
            on(atom("DOWN"), arg_match) >> [=](std::uint32_t err) {
                auto itr = m_users.find(last_sender());
                if(itr != m_users.end()) {
                    for(auto& grp : joined_groups()) {
                        send(grp, atom("in"), itr->second+" left.");
                    }
                }
            },

            // incoming chat messages
            on(atom("in"), arg_match) >> [=](const string& message) {
                send(m_printer, "[LOG] "+message);
            },

            // errors
            others() >> [=]() {
                cout << "[!!!] unexpected message: '" << to_string(last_dequeued()) << "'." << endl;
            }
        );
    }

 public:

            server(actor_ptr printer, uint16_t port) : m_printer(printer), m_port(port) { }

};

class client : public event_based_actor {

//    struct joined_group {
//        string m_groupname;
//        group_ptr m_group;
//        bool m_leave_on_server_disco;
//        joined_group(string groupname, group_ptr group, bool leave_on_server_disco) : m_groupname(groupname), m_group(group), m_leave_on_server_disco(leave_on_server_disco) { }
//        const string& get_greoupname() { return m_group; }
//        const group_ptr& get_group() { return m_group; }
//        bool leave_on_server_disconnect() { return m_leave_on_server_disco; }
//    };

    typedef set<pair<string, group_ptr> > group_set;

    string    m_username;
    actor_ptr m_server;
    actor_ptr m_printer;
    group_set m_joined;
    bool connected;

    void init() {
        become (
            // local commands
            on(atom("send"), arg_match) >> [=](const string& groupname, const string& message) {
                auto itr = find_if(begin(m_joined), end(m_joined), [&groupname](const group_set::value_type& kvp) {
                    return kvp.first == groupname;
                });
                if(itr != m_joined.end()) {
                    send(itr->second, atom("in"), m_username+": "+message);
                }
            },
            on(atom("broadcast"), arg_match) >> [=](const string& message) {
                for(auto& dest : m_joined) {
                    send(dest.second, atom("in"), m_username+": "+message);
                }
            },
            on(atom("join"), atom("local"), arg_match) >> [=](const string& groupname) {
                if(connected) {
                    send(m_server, atom("request"), atom("local"), groupname);
                }
                else {
                    send(m_printer, "You need to be connected to a server to do that.");
                }
            },
            on(atom("join"), atom("hamcast"), arg_match) >> [=](const string& groupname) {
                if(connected) {
                    send(m_server, atom("request"), atom("hamcast"), groupname);
                }
                else {
                    send(m_printer, "You need to be connected to a server to do that.");
                }
            },
            on(atom("quit")) >> [=]() {
                quit();
            },
            on(atom("connect"), arg_match) >> [=](const string& host, uint16_t port) {
                if(connected) {
                    send(m_printer, "You cann only be connected to one server at a time");
                }
                else {
                    try {
                        stringstream strstr;
                        strstr << "Connecting to a server at '" << host << ":" << port << "'.";
                        send(m_printer, strstr.str());
                        auto new_server = remote_actor(host, port);
                        m_server = new_server;
                        monitor(new_server);
                        connected = true;
                        send(m_printer, "Connection established.");
                        send(m_server, atom("join"), m_username);
                    }
                    catch (network_error exc) {
                        send(m_printer, string("[!!!] Could not connect: ") + exc.what());
                    }
                }
            },

            // replys
            on(atom("answer"), arg_match) >> [=](const string& groupname, const group_ptr& group) {
                send(m_printer, "Joining group: '"+groupname+"'.");
                join(group);
                m_joined.insert(make_pair(groupname, group));
                send(group, atom("in"), m_username+" joined "+groupname+".");

            },
            on(atom("answer"), atom("local"), arg_match) >> [=](const string& groupname, const group_ptr& group) {
                send(m_printer, "Joining group: '"+groupname+"'.");
                join(group);
                m_joined.insert(make_pair(groupname, group));
                send(group, atom("in"), m_username+" joined "+groupname+".");

            },
            on(atom("answer"), atom("hamcast"), arg_match) >> [=](const string& groupname, const group_ptr& group) {
                send(m_printer, "Joining group: '"+groupname+"'.");
                join(group);
                m_joined.insert(make_pair(groupname, group));
                send(group, atom("in"), m_username+" joined "+groupname+".");

            },
            on(atom("in"), arg_match) >> [=](const string& message) {
                if(last_sender() != this) {
                    send(m_printer, message);
                }
            },

            // server requests
            /*   empty  */

            // errors
            on(atom("failed"), atom("info"), arg_match) >> [=](const string& groupname) {
                send(m_printer, "Failed to get information for group: '"+groupname+"'.");
            },
            on(atom("DOWN"), arg_match) >> [=](std::uint32_t err) {
                send(m_printer, "Server shut down!");
                connected = false;
            },
            others() >> [=]() {
                send(m_printer, string("[!!!] unexpected message: '") + to_string(last_dequeued()));
            }
        );
    }

 public:

            client(const string& username, actor_ptr printer) : m_username(username), m_printer(printer), connected(false) { }

};

class print_actor : public event_based_actor {
    void init() {
        become (
            on(atom("quit")) >> [] {
                self->quit();
            },
            on_arg_match >> [](const string& str) {
                cout << (str + "\n");
            }
        );
    }
};

inline vector<std::string> split(const string& str, char delim) {
    vector<std::string> result;
    stringstream strs{str};
    string tmp;
    while (std::getline(strs, tmp, delim)) result.push_back(tmp);
    return result;
}

template<typename T>
auto conv(const string& str) -> option<T> {
    T result;
    if (istringstream(str) >> result) return result;
    return {};
}

void print_usage(actor_ptr printer) {
    send(printer, "Usage: group_chat --type=<server|client>\n --type, -t\t\tcan be: server, s, client, c\n --name, -n\t\tusername (only needed for client)\n --host, -h\t\thostname (only needed for client)\n --port, -p\t\tport for server/client");
}

std::function<option<string> (const string&)> get_extractor(const string& identifier) {
    auto tmp = [&](const string& kvp) -> option<string> {
        auto vec = split(kvp, '=');
        if (vec.size() == 2) {
            if (vec.front() == "--"+identifier) {
                return vec.back();
            }
        }
        return {};
    };
    return tmp;
}


auto main(int argc, char* argv[]) -> int {

    vector<string> args(argv + 1, argv + argc);

    cout.unsetf(std::ios_base::unitbuf);

    auto printer = spawn<print_actor>();
    group::add_module(make_hamcast_group_module());

    bool has_type = false;
    bool has_name = false;
    bool has_host = false;
    bool has_port = false;

    string type;
    string name;
    string host;
    uint16_t port;

    auto toint = [](const string& str) -> option<int> {
        char* endptr = nullptr;
        int result = static_cast<int>(strtol(str.c_str(), &endptr, 10));
        if (endptr != nullptr && *endptr == '\0') {
            return result;
        }
        return {};
    };

    bool success = match_stream<string>(begin(args), end(args)) (
        (on("-n", arg_match) || on(get_extractor("name"))) >> [&](const string& input) -> bool {
            string tmp;
            tmp.reserve(input.size());
            copy_if(begin(input), end(input), back_inserter(tmp), [](char c) { return c != ' '; });
            if (tmp.size() > 2 && input.size() < 20) {
                name = input;
                has_name = true;
                return true;
            }
            else {
                send(printer, "Not a valid user name.");
                return false;
            }
        },
        (on("-t", arg_match) || on(get_extractor("type"))) >> [&](const string& input) -> bool {
            if(input == "c" || input == "client") {
                type = "c";
                type = input;
                has_type = true;
                return true;
            }
            else if(input == "s" || input == "server") {
                type = "s";
                type = input;
                has_type = true;
                return true;
            }
            else {
                send(printer, "not a valid type");
                return false;
            }
        },
        (on("-h", arg_match) || on(get_extractor("host"))) >> [&](const string& input) -> bool {
            if(input.find(' ') == string::npos && input.size() > 0) {
                host = input;
                has_host = true;
                return true;
            }
            else {
                send(printer, "Not a valid hostname.");
                return false;
            }
        },
        (on("-p", arg_match) || on(get_extractor("port"))) >> [&](const string& input) -> bool {
            auto tmp = toint(input);
            if(tmp) {
                port = *tmp;
                has_port = true;
                return true;
            }
            else {
                return false;
            }
        }
    );

    if(!success || !has_type) {
        print_usage(printer);
        send(printer, atom("quit"));
        await_all_others_done();
        shutdown();
        return 0;
    }

    if(type == "s") { // server
        if(!has_port) {
            send(printer, "No port was given, randomness 4tw!");
            port = (rand()%(static_cast<int>(numeric_limits<uint16_t>::max()) - 1024))+1024;
        }
        auto server_actor = spawn<server>(printer, port);
        try {
            publish(server_actor, port);
            stringstream strstr;
            strstr << "Now running on port: '" << port << "'.";
            send(printer, strstr.str());
        } catch(bind_failure&) {
            stringstream strstr;
            strstr << "problem binding server to port: " << port << "'.";
            send(printer, strstr.str());
        }
        for(bool done = false; !done; ) {
            string input;
            getline(cin, input);
            if(input.size() > 0) {
                input.erase(input.begin());
                vector<string> values = split(input, ' ');
                match (values) (
                    on("info") >> [&] {
                        send(server_actor, atom("info"));
                    },
                    on("quit") >> [&] {
                        done = true;
                    },
                    others() >> [&] {
                        send(printer, "available commands:\n /info\n /quit\n");
                    }
                );
            }
        }
        send(server_actor, atom("quit"));
    } // server

    else { // is_client <=> !is_server
        if(!has_name) {
            for (bool done = false; !done ; ) {
                send(printer, "So what is your name for chatting?");
                getline(cin, name);
                string tmp;
                tmp.reserve(name.size());
                copy_if(begin(name), end(name), back_inserter(tmp), [](char c) { return c != ' '; });
                if (tmp.size() < 3  || name.size() > 20) {
                    send(printer, "Please pick a another name.");
                }
                else {
                    done = true;
                    has_name = true;
                }
            }
        }
        send(printer, "Starting client.");
        auto client_actor = spawn<client>(name, printer);

        if(has_host && has_port) {
            stringstream strstr;
            strstr << "Connecting to '" << host << "' on port '" << port << "'.";
            send(printer, strstr.str());
            send(client_actor, atom("connect"), host, port);
        }
        auto get_command = [&](){
            string input;
            bool done = false;
            getline(cin, input);
            if(input.size() > 0) {
                if(input.front() == '/') {
                    input.erase(input.begin());
                    vector<string> values = split(input, ' ');
                    match (values) (
                        on("send", val<string>, any_vals) >> [&](const string& groupname) {
                            if(values.size() > 2) {
                                input.erase(0,values[0].size()+values[1].size()+2);
                                send(client_actor, atom("send"), groupname, input);
                            }
                            else {
                                send(printer, "not without a message my friend ...");
                            }
                        },
                        on("join", val<string>) >> [&](const string& groupname) {
                            send(client_actor, atom("join"), atom("local"), groupname);
                        },
                        on("join", "hamcast", arg_match )>> [&](const string& groupname) {
                            send(client_actor, atom("join"), atom("hamcast"), groupname);
                        },
                        on("connect", val<string>, conv<uint16_t>) >> [&](const string& host, uint16_t port) {
                            send(client_actor, atom("connect"), host, port);
                        },
                        on("quit") >> [&] {
                            done = true;
                        },
                        others() >> [&] {
                            send(printer, "available commands:\n /connect HOST PORT\n /join GROUPNAME\n /join hamcast URI\n /send GROUPNAME MESSAGE\n /quit");
                        }
                    );
                }
                else {
                    send(client_actor, atom("broadcast"), input);
                }
            }
            return !done;
        };
        auto running = get_command();
        while(running) {
            running = get_command();
        }
        send(client_actor, atom("quit"));
    } // client

    send(printer, atom("quit"));
    await_all_others_done();
    shutdown();
    return 0;
}
