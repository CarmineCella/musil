// system.h -- system library for Musil
//

#include "musil.h"
#include "csv_tools.h"

#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <functional>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include<sys/socket.h>
#include<arpa/inet.h>

AtomPtr fn_schedule(AtomPtr node, AtomPtr env) {
    args_check(node, 2);
    AtomPtr thunk     = type_check(node->tail.at(0), LAMBDA);
    AtomPtr delayAtom = type_check(node->tail.at(1), ARRAY);
    int delay_ms      = static_cast<int>(delayAtom->array[0]);
    AtomPtr thunk_clone = clone(thunk);
    AtomPtr env_clone   = clone(env);
    std::thread([thunk_clone, env_clone, delay_ms]() {
        try {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            AtomPtr call = make_atom();
            call->tail.push_back(thunk_clone);
            eval(call, env_clone);
        } catch (const std::exception& e) {
            std::cerr << "[schedule] error: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[schedule] unknown error" << std::endl;
        }
    }).detach();
    return make_atom();
}
AtomPtr fn_sleep(AtomPtr params, AtomPtr env) {
    std::valarray<Real>& a = type_check(params->tail.at(0), ARRAY)->array;
    int delay_ms = static_cast<int>(a[0]);
    if (delay_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
    return make_atom(); // ()
}
AtomPtr fn_clock (AtomPtr params, AtomPtr env) {
    return make_atom (clock ());
}
AtomPtr fn_dirlist (AtomPtr params, AtomPtr env) {
    std::string path = type_check (params->tail.at (0), STRING)->lexeme;
    DIR *dir;
    AtomPtr ll =  make_atom();
    struct dirent *ent;
    if ((dir = opendir (path.c_str())) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            ll->tail.push_back (make_atom ((std::string) "\"" + ent->d_name));
        }
        closedir (dir);
    }
    return ll;
}
AtomPtr fn_filestat (AtomPtr params, AtomPtr env) {
    struct stat fileStat;
    AtomPtr ll =  make_atom();
    std::string filename = type_check (params->tail.at(0), STRING)->lexeme;
    int r = stat (filename.c_str (), &fileStat);
    ll->tail.push_back( make_atom(r == 0));
    if (r < 0) return ll;

    ll->tail.push_back( make_atom(fileStat.st_size));
    ll->tail.push_back( make_atom(fileStat.st_nlink));
    std::stringstream tt;
    tt << ((S_ISDIR(fileStat.st_mode)) ? "d" : "-");
    tt << ((fileStat.st_mode & S_IRUSR) ? "r" : "-");
    tt << ((fileStat.st_mode & S_IWUSR) ? "w" : "-");
    tt << ((fileStat.st_mode & S_IXUSR) ? "x" : "-");
    tt << ((fileStat.st_mode & S_IRGRP) ? "r" : "-");
    tt << ((fileStat.st_mode & S_IWGRP) ? "w" : "-");
    tt << ((fileStat.st_mode & S_IXGRP) ? "x" : "-");
    tt << ((fileStat.st_mode & S_IROTH) ? "r" : "-");
    tt << ((fileStat.st_mode & S_IWOTH) ? "w" : "-");
    tt << ((fileStat.st_mode & S_IXOTH) ? "x" : "-");
    ll->tail.push_back(make_atom((std::string) "\"" + tt.str ().c_str ()));
    return ll;
}
AtomPtr fn_getvar (AtomPtr params, AtomPtr env) {
    char* c = getenv (type_check (params->tail.at (0), STRING)->lexeme.c_str ());
    if (c) return make_atom ((std::string) "\"" + (std::string) c);
    else return make_atom("");
}
AtomPtr fn_addpaths (AtomPtr params, AtomPtr env) {
    if (params->tail.size () == 0) {
        AtomPtr list = make_atom ();
        for (unsigned i = 0; i < env->paths.size (); ++i) {
            AtomPtr s =make_atom (env->paths.at (i));
            s->type = STRING;
            list->tail.push_back (s);
        }
        return list;
    } else {
        for (unsigned i = 0; i < params->tail.size (); ++i) {
            env->paths.push_back (type_check (params->tail.at (i), STRING)->lexeme);
        }
        return make_atom (params->tail.size ());
    }
}
AtomPtr fn_clearpaths (AtomPtr params, AtomPtr env) {
    env->paths.resize (1); // keep home folder
    return make_atom (env->paths.size ());
}
#define MESSAGE_SIZE 4096
AtomPtr fn_udprecv (AtomPtr n, AtomPtr env) {
    struct sockaddr_in server, client;
    char client_message[MESSAGE_SIZE];
    memset (client_message, 0, sizeof (char) * MESSAGE_SIZE);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        return  make_atom (0);
    }

    server.sin_addr.s_addr = inet_addr(type_check (n->tail.at(0), STRING)->lexeme.c_str ());
    server.sin_family = AF_INET;
    server.sin_port = htons((long)type_check (n->tail.at(1), ARRAY)->array[0]);

if(::bind(sock,(struct sockaddr *)&server , sizeof(server)) < 0) {
        return make_atom(0);
    }
    int c = sizeof(struct sockaddr_in);
    if (recvfrom(sock, client_message, MESSAGE_SIZE, 0, 
        (struct sockaddr *) &client, (socklen_t*) &c) < 0) {
        return  make_atom(0);   
    }

    ::close (sock);
    return make_atom ((std::string) "\"" + client_message);
}
class OSCstring {
public:
    OSCstring () { osc_msg = 0; }
    ~OSCstring () {
        if (osc_msg) delete [] osc_msg;
    }
    const char* encode (const std::string& msg, size_t& out_sz) {
        size_t in_sz = msg.size (); 
        int pad = (padding (in_sz) == 0 ? 4 : padding (in_sz));
        out_sz = in_sz + pad + 4;
        if (osc_msg) delete [] osc_msg;
        osc_msg = new char[out_sz];
        for (unsigned i = 0; i < in_sz; ++i) osc_msg[i] = msg[i];
        for (unsigned i = in_sz; i < in_sz + pad; ++i) osc_msg[i] = '\0';
        osc_msg[in_sz + pad] = ',';
        osc_msg[in_sz + pad + 1] = '\0';
        osc_msg[in_sz + pad + 2] = '\0';
        osc_msg[in_sz + pad + 3] = '\0';
        return osc_msg;
    }
private:
    bool isAligned(size_t n)  {
        return (n & 3) == 0;
    }
    size_t align(size_t n) {
        return (n + 3) & -4;
    }
    size_t padding(size_t n) {
        return align(n) - n;
    }
    char* osc_msg;
};
AtomPtr fn_udpsend (AtomPtr n, AtomPtr env) {
    struct sockaddr_in server;
    char server_reply[MESSAGE_SIZE];
    memset (server_reply, 0, sizeof (char) * MESSAGE_SIZE);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        return  make_atom(0);
    }
    
    server.sin_addr.s_addr = inet_addr(type_check (n->tail.at (0), STRING)->lexeme.c_str ());
    server.sin_family = AF_INET;
    server.sin_port = htons((long)type_check (n->tail.at(1), ARRAY)->array[0]);
    bool is_osc = false;
    if (n->tail.size () == 4) is_osc = (bool) type_check (n->tail.at (3), ARRAY)->array[0];

    std::stringstream nf;
    print (n->tail.at(2), nf);
    int res = 0;
    if (is_osc) {
        OSCstring enc;
        size_t sz = 0;
        const char* out = enc.encode (nf.str ().c_str (), sz);
        res = sendto(sock, out, sz, 0, (struct sockaddr *)&server , sizeof(server));
    } else {
        res = sendto(sock, nf.str ().c_str (), nf.str ().size (), 0, (struct sockaddr *)&server , sizeof(server));
    }
    if (res < 0) return  make_atom(0);
    ::close (sock);
    return  make_atom (1);
}
AtomPtr add_system (AtomPtr env) {
    add_op ("%schedule", &fn_schedule, 2, env);
    add_op ("sleep",  &fn_sleep,   1, env);
    add_op ("clock", &fn_clock, 0, env);
    add_op ("dirlist", &fn_dirlist, 1, env);
    add_op ("filestat", &fn_filestat, 1, env);
    add_op ("getvar", &fn_getvar, 1, env);
    add_op ("addpaths", &fn_addpaths, 0, env);
    add_op ("clearpaths", &fn_clearpaths, 0, env);
    add_op ("udpsend", &fn_udpsend, 3, env);
    add_op ("udprecv", &fn_udprecv, 2, env);
    return env;
}

// eof
