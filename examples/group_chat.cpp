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

/**
 *  todo:
 * server gruppen spiechern user (+monitor)
 */

template<typename T, typename... Args>
unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(std::forward<Args>(args)...));
}

void hamcast_request(const string& groupname) {
    cout << "[LOG] Someone requested group information for: '"+groupname+"'.";
    group_ptr group = group::get("hamcast", groupname);
    self->join(group);
    //auto group = group::get("local", groupname);
    //join(group);
    reply(atom("answer"), groupname, group);
}

class server : public event_based_actor {

    typedef map<actor_ptr, string> user_map;
    
    actor_ptr m_printer;
    user_map  m_users;
    
    void init() {
        become (
            // server commands
            /* the server can also use create */
            on(atom("quit")) >> [=]() {
                for(auto& grp : joined_groups()) {
                    send(grp, atom("in"), "Server shutting down, bye!");
                }
                quit();
            },

            on(atom("request"), atom("local"), arg_match) >> [=](const string& groupname) {
                send(m_printer, "[LOG] Someone requested group information for: '"+groupname+"'.");
                auto group = group::get("local", groupname);
                join(group);
                reply(atom("answer"), groupname, group);
            },
            on(atom("request"), atom("hamcast"), arg_match) >> hamcast_request,
            /*
            on(atom("request"), atom("hamcast"), arg_match) >> [=](const string& groupname) {
                send(m_printer, "[LOG] Someone requested group information for: '"+groupname+"'.");
                auto group = group::get("hamcast", groupname);
                join(group);
                //auto group = group::get("local", groupname);
                //join(group);
                reply(atom("answer"), groupname, group);

            },*/
            on(atom("join"), arg_match) >> [=](const string& username){
                m_users[last_sender()] = username;
                monitor(last_sender());
            },

            // incoming chat messages
            on(atom("in"), arg_match) >> [=](const string& message) {
                send(m_printer, "[LOG] "+message);
            },
            on(atom("DOWN"), arg_match) >> [=](std::uint32_t err) {
                auto itr = m_users.find(last_sender());
                if(itr != m_users.end()) {
                    for(auto& grp : joined_groups()) {
                        send(grp, atom("in"), itr->second+" left.");
                    }
                }
            },

            // errors
            others() >> [=]() {
                cout << "[!!!] unexpected message: '" << to_string(last_dequeued()) << "'." << endl;
            }
        );
    }
    
 public:
    
    server(actor_ptr printer) : m_printer(printer) { }

};

class client : public event_based_actor {

    // typedef std::map<string, group_ptr> group_map;
    typedef set<pair<string, group_ptr> > group_set;

    string    m_username;
    string    m_server_host;
    uint16_t  m_server_port;
    actor_ptr m_server;
    actor_ptr m_printer;

    group_set m_joined;

    void init() {
        try {
            stringstream strstr;
            strstr << "Connecting to server at '" << m_server_host << ":" << m_server_port << "'.";
            send(m_printer, strstr.str());
            m_server = remote_actor(m_server_host, m_server_port);
            send(m_printer, "Connection established. Hi, "+m_username+"!");
        }
        catch (network_error exc) {
            send(m_printer, string("[!!!] Error: ") + exc.what());
            quit();
        }
        become (
            // local commands
            on(atom("send"), arg_match) >> [=](const string& groupname, const string& message) {
                auto itr = find_if(begin(m_joined), end(m_joined), [&groupname](const group_set::value_type& kvp) {
                    return kvp.first == groupname;
                });
                if(itr != m_joined.end()) {
                    send(itr->second, atom("in"), m_username+": "+message);
                }
                else {
                    send(m_printer, "Can't send message to unknown group: '"+groupname+"'.");
                }
            },
            on(atom("broadcast"), arg_match) >> [=](const string& message) {
                for(auto& dest : m_joined) {
                    // send(m_printer, "sending '"+message+"' to '"+dest.first+"'.");
                    send(dest.second, atom("in"), m_username+": "+message);
                }
            },
            // on(atom("create"), arg_match) >> [=](const string& groupname) {
            //     send(m_server, atom("create"), groupname);
            // },
            on(atom("join"), atom("local"), arg_match) >> [=](const string& groupname) {
                //send(m_printer, to_string(last_dequeued()));
                send(m_server, atom("request"), atom("local"), groupname);
            },
            on(atom("join"), atom("hamcast"), arg_match) >> [=](const string& groupname) {
                //send(m_printer, to_string(last_dequeued()));
                //send(m_printer, "received join for hamcast group: "+groupname);
                send(m_server, atom("request"), atom("hamcast"), groupname);
            },
            on(atom("quit")) >> [=]() {
                quit();
            },

            // replys
            on(atom("answer"), arg_match) >> [=](const string& groupname, const group_ptr& group) {
                send(m_printer, "Joining group: '"+groupname+"'.");
                join(group);
                m_joined.insert(make_pair(groupname, group));
                send(m_server, atom("join"), m_username);
                send(group, atom("in"), m_username+" joined "+groupname+".");

            },
            on(atom("created"), arg_match) >> [=](const string& groupname) {
                send(m_printer, "Created group: '"+groupname+"'.");
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
            others() >> [=]() {
                send(m_printer, string("[!!!] unexpected message: '") + to_string(last_dequeued()));
            }
        );
    }

 public:

    client(const string& username, const string& server_host, uint16_t server_port, actor_ptr printer) : m_username(username), m_server_host(server_host), m_server_port(server_port), m_printer(printer) { }

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

void foobar() {
    cout << "foobar!" << endl;
}

auto main(int argc, char* argv[]) -> int {

    cout << __FILE__ " line " << __LINE__ << endl;
    boost::thread t(foobar);

    cout << __FILE__ " line " << __LINE__ << endl;
    hamcast::multicast_socket ms;
    cout << __FILE__ " line " << __LINE__ << endl;
    ms.join("ip://239.255.0.1:1234");
    cout << __FILE__ " line " << __LINE__ << endl;

    vector<string> args(argv + 1, argv + argc);

    cout.unsetf(std::ios_base::unitbuf);

    auto printer = spawn<print_actor>();

    bool ask_for_type = true;
    bool is_server = false;
    if (!args.empty()) {
        if(args.front() == "s" || args.front() == "server") {
            is_server = true;
            ask_for_type = false;
        }
        else if (args.front() == "c" || args.front()  == "client") {
            is_server = false;
            ask_for_type = false;
        }
    }
    if(ask_for_type) {
        send(printer, "Do you want to start a server or a client (s/c)");
        for(bool done = false; !done; ) {
            string input;
            getline(cin, input);
            if(input == "s") {
                is_server = true;
                done = true;
            }  
            else if (input == "c") {
                is_server = false;
                done = true;
            }
            else if (input == "cow") {
                send(printer, "mooo!");
            }
            else {
                send(printer, "Valid answer are: 's' and 'c'.");
            }
        }
    }

    group::add_module(make_unique<hamcast_group_module>());

    if(is_server) { // server 
        send(printer, "Starting chat server ... ");

        auto server_actor = spawn<server>(printer);
        srand(time(NULL));
        uint16_t port = 20283;
        for(bool done = false; !done; ) {
            try {
                //port = (rand()%(static_cast<int>(numeric_limits<uint16_t>::max()) - 1024))+1024;
                publish(server_actor, port);
                done = true;
                stringstream strstr;
                strstr << "Now running on port: '" << port << "'.";
                send(printer, strstr.str());
            } catch (bind_failure&) { }
        }
        for(bool done = false; !done; ) {
            string input;
            getline(cin, input);
            if(input.size() > 0) {
                if(input.front() == '/') {
                    input.erase(input.begin());
                    vector<string> values = split(input, ' ');

                    match (values) (
                        on("create", val<string>) >> [&](const string& groupname) {
                            send(server_actor, atom("create"), groupname);
                        },
                        on("quit") >> [&] {
                            done = true;
                        },
                        others() >> [&] {
                            send(printer, "available commands:\n /create GROUPNAME\n /quit\n");
                        }
                    );
                }
            }
        }
        send(server_actor, atom("quit"));
    } // server

    else { // client
        string username;
        for (bool done = false; !done ; ) {
            send(printer, "So what is your name for chatting?");
            getline(cin, username);
            string tmp;
            tmp.reserve(username.size());
            copy_if(begin(username), end(username), back_inserter(tmp), [](char c) { return c != ' '; });
            if (tmp.size() < 3 ) {
                send(printer, "Please pick a longer name, my friend."); //"+secretname+".");
            }
            else if ( tmp.size() > 12) {
                send(printer, "Please pick a shorter name, my friend."); //"+secretname+".");
            }
            else {
                done = true;
            }
        }

        string   host = "localhost";
        uint16_t port;
        {
            send(printer, "Now i need the server data, meaning host and port.\nWhat is the host (default: localhost)");
            for(bool done = false; !done; ) {
                string tmp;
                getline(cin, tmp);
                if(tmp == "") {
                    done = true;
                }
                else if(tmp.find(' ') == string::npos && tmp.size() > 0 ) {
                    done = true;
                    host = tmp;
                }
                else {
                    send(printer, "The hostname should not contain spaces. Let us try that again.");
                }
            }
            send(printer, "And the port?");
            for(bool done = false; !done; ) {
                string input;
                getline(cin, input);
                if(istringstream(input) >> port) {
                    done = true;
                }
                else {
                    send(printer, "This should be number.");
                }
            }
        }

        send(printer, "Starting client.");
        auto client_actor = spawn<client>(username, host, port, printer);
        for(bool done = false; !done; ) {
            string input;
            getline(cin, input);
            if(input.size() > 0) {
                if(input.front() == '/') {
                    input.erase(input.begin());
                    vector<string> values = split(input, ' ');

                    match (values) (
                        on("send", val<string>, val<string>, any_vals) >> [&](const string& groupname, const string& message) {
                            input.erase(0,6+groupname.size());
                            send(client_actor, atom("send"), groupname, input);
                        },
                        // on("create", val<string>) >> [&](const string& groupname) {
                        //     send(client_actor, atom("create"), groupname);
                        // },
                        on("join", val<string>) >> [&](const string& groupname) {
                            send(client_actor, atom("join"), atom("local"), groupname);
                        },
                        on("join", "hamcast", arg_match )>> [&](const string& groupname) {
                            send(client_actor, atom("join"), atom("hamcast"), groupname);
                        },
                        on("quit") >> [&] {
                            done = true;
                        },
                        others() >> [&] {
                            copy(begin(values), end(values), ostream_iterator<string>(cout, " "));
                            //send(printer, "available commands:\n /create GROUPNAME\n /join GROUPNAME\n /quit");
                        }
                    );
                }
                else {
                    send(client_actor, atom("broadcast"), input);
                }
            }
        }
        send(client_actor, atom("quit"));
    } // client

    send(printer, atom("quit"));
    await_all_others_done();
    shutdown();
    return 0;
}
