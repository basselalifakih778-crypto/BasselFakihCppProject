#include <bits/stdc++.h>
using namespace std;
using VersionId = uint64_t;

static string fmt_time_local(int64_t ts_ns) {
    time_t secs = (time_t)(ts_ns / 1000000000LL);
    tm tmout{};
#if defined(_WIN32)
    if (localtime_s(&tmout, &secs) != 0) return string("invalid-time");
#else
    if (!localtime_r(&secs, &tmout)) return string("invalid-time");
#endif
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmout) == 0)
        return string("invalid-time");
    return string(buf);
}

static uint64_t hash64(string_view s) {
    constexpr uint64_t FNV_OFFSET = 1469598103934665603ULL;
    constexpr uint64_t FNV_PRIME  = 1099511628211ULL;
    uint64_t h = FNV_OFFSET;
    for (unsigned char c : s) {
        h ^= c;
        h *= FNV_PRIME;
    }
    return h;
}


static string to_hex(uint64_t x) {
    ostringstream oss;
    oss << hex << nouppercase << setfill('0') << setw(16) << x;
    return oss.str();
}

struct Version {
    VersionId id{};
    VersionId parent{};
    int64_t   ts_ns{};
    uint64_t  content_hash{};
    string    message; // commit message
    string    content;
};

struct Repo {
    vector<Version> history;
    string working;
    VersionId head{0};

    static int64_t now_ns() {
        using namespace std::chrono;
        return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    }

    VersionId commit(string msg) {

        uint64_t new_hash = hash64(working);

        if (!history.empty() && history.back().content_hash == new_hash) {
            cout<<"no content change\n";
            return head;
        }

        Version v;
        v.id = history.size() + 1;
        v.parent = head;
        v.ts_ns = now_ns();
        v.content_hash = new_hash;
        v.message = std::move(msg);
        v.content = working; // snapshot
        history.push_back(std::move(v));
        head = history.back().id;
        return head;
    }

    const Version* get(VersionId id) const {
        if (id==0 || id > history.size()) return nullptr;
        return &history[id-1];
    }

    void checkout(VersionId id) {
        auto* v = const_cast<Repo*>(this)->get(id);
        if (!v)
        {
            cout<<"no such version\n";
            return;
        }
        working = v->content;
        head = id;
    }

    void save(const string& path) const {
        ofstream os(path); // text mode
        if (!os) {
            cout << "cannot open file for write\n";
            return;
        }

        os << "count " << static_cast<uint64_t>(history.size()) << "\n";

        for (const auto& v : history) {
            os << "id "      << static_cast<uint64_t>(v.id)           << "\n";
            os << "parent "  << static_cast<uint64_t>(v.parent)       << "\n";
            os << "ts_ns "   << static_cast<int64_t>(v.ts_ns)         << "\n";
            os << "hash "    << static_cast<uint64_t>(v.content_hash) << "\n";
            os << "message " << std::quoted(v.message)                << "\n";
            os << "content " << std::quoted(v.content)                << "\n";
            os << "----\n";
        }

        os << "head " << static_cast<uint64_t>(head) << "\n";

        if (!os) cout<<"write failed\n";
    }

    void load(const string& path) {
        ifstream is(path);
        if (!is) cout<<"cannot open file for read\n";

        history.clear();
        working.clear();
        head = 0;

        string key;
        uint64_t count = 0;

        if (!(is >> key) || key != "count") {
            cout << "expected 'count'\n";
            return;
        }
        if (!(is >> count)) {
            cout << "bad count\n";
            return;
        }

        string dummy; getline(is, dummy);


        history.reserve(static_cast<size_t>(count));

        for (uint64_t i = 0; i < count; ++i) {
            Version v{};
            string dummy;

            if (!(is >> key) || key != "id") {
                cout << "expected 'id'\n";
                return;
            }
            if (!(is >> v.id)) {
                cout << "bad id\n";
                return;
            }
            getline(is, dummy);

            if (!(is >> key) || key != "parent") {
                cout << "expected 'parent'\n";
                return;
            }
            if (!(is >> v.parent)) {
                cout << "bad parent\n";
                return;
            }
            getline(is, dummy);

            if (!(is >> key) || key != "ts_ns") {
                cout << "expected 'ts_ns'\n";
                return;
            }
            if (!(is >> v.ts_ns)) {
                cout << "bad ts_ns\n";
                return;
            }
            getline(is, dummy);

            if (!(is >> key) || key != "hash") {
                cout << "expected 'hash'\n";
                return;
            }
            if (!(is >> v.content_hash)) {
                cout << "bad hash\n";
                return;
            }
            getline(is, dummy);

            if (!(is >> key) || key != "message") {
                cout << "expected 'message'\n";
                return;
            }
            if (!(is >> std::quoted(v.message))) {
                cout << "bad message\n";
                return;
            }
            getline(is, dummy);

            if (!(is >> key) || key != "content") {
                cout << "expected 'content'\n";
                return;
            }
            if (!(is >> std::quoted(v.content))) {
                cout << "bad content\n";
                return;
            }
            getline(is, dummy);

            if (!std::getline(is, key)) {
                cout << "missing separator\n";
                return;
            }
            if (key != "----") {
                cout << "expected '----'\n";
                return;
            }

            history.push_back(std::move(v));
        }

        if (!(is >> key) || key != "head")
        {
            cout<<"0expected 'head'\n";
            return;
        }
        if (!(is >> head)){
          cout<<"bad head\n";
            return;
        }
        if (head != 0) {
            const Version* hv = get(head);
            if (hv) working = hv->content;
        }
    }
};

static void help() {
    cout <<
         R"(Commands:
  set "TEXT"              Replace working content
  append "TEXT"           Append to working content
  erase POS LEN           Erase [POS, POS+LEN)
  commit "MSG"            Snapshot current working content (fails if no change)
  log                     List versions
  show ID                 Print content of version
  checkout ID             Set working to version content and move head
  save FILE               Save repo to file
  load FILE               Load repo from file
  print                   Print working content
  help                    Show this help
  exit                    Quit
)";
}

int main() {
    Repo repo;
    help();
    string line;
    while (true) {
        cout << "> " << flush;
        if (!std::getline(cin, line)) {
            if (cin.eof() || cin.bad()) break;
            cin.clear();
            continue;
        }
        if (line.find_first_not_of(" \t\r\n") == string::npos) continue;
        std::istringstream in(line);
        string cmd;
        if (!(in >> cmd)) continue;

        try {
            if (cmd == "set") {
                string rest;
                std::getline(in, rest);
                auto pos = rest.find_first_not_of(' ');
                string s = (pos==string::npos) ? string() : rest.substr(pos);
                if (!s.empty() && s.front()=='"' && s.back()=='"' && s.size()>=2)
                    s = s.substr(1, s.size()-2);
                repo.working = std::move(s);

            } else if (cmd == "append") {
                string rest; std::getline(in, rest);
                auto pos = rest.find_first_not_of(' ');
                string s = (pos==string::npos) ? string() : rest.substr(pos);
                if (!s.empty() && s.front()=='"' && s.back()=='"' && s.size()>=2)
                    s = s.substr(1, s.size()-2);
                repo.working += s;

            } else if (cmd == "erase") {
                string pTok, lenTok;
                if (!(in >> pTok >> lenTok)) { cout << "usage: erase POS LEN\n"; continue; }
                size_t p=0, len=0;
                try {
                    p = stoull(pTok);
                    len = stoull(lenTok);
                } catch (...) { cout << "erase: POS and LEN must be numbers\n"; continue; }
                if (p > repo.working.size()) { cout << "pos out of range\n"; continue; }
                size_t take = min(len, repo.working.size()-p);
                repo.working.erase(p, take);

            } else if (cmd == "commit") {
                string rest; std::getline(in, rest);
                auto pos = rest.find_first_not_of(' ');
                string msg = (pos==string::npos) ? string() : rest.substr(pos);
                if (!msg.empty() && msg.front()=='"' && msg.back()=='"' && msg.size()>=2)
                    msg = msg.substr(1, msg.size()-2);
                auto id = repo.commit(std::move(msg));
                cout << "Committed as " << id << "\n";

            } else if (cmd == "log") {
                for (auto it = repo.history.rbegin(); it != repo.history.rend(); ++it) {
                    const auto &v = *it;
                    cout << "id " << v.id
                         << (v.id == repo.head ? " (HEAD)" : "")
                         << "  parent " << v.parent
                         << "  hash 0x" << to_hex(v.content_hash)
                         << "  time " << fmt_time_local(v.ts_ns)
                         << "  msg: " << v.message << "\n";
                }

            } else if (cmd == "show") {
                string idTok;
                if (!(in >> idTok)) { cout << "usage: show ID\n"; continue; }
                VersionId id{};
                try { id = stoull(idTok); }
                catch (...) { cout << "invalid ID\n"; continue; }
                auto* v = repo.get(id);
                if (!v) { cout << "No such version\n"; continue; }
                cout << v->content << "\n";

            } else if (cmd == "checkout") {
                string idTok;
                if (!(in >> idTok)) { cout << "usage: checkout ID\n"; continue; }
                VersionId id{};
                try { id = stoull(idTok); }
                catch (...) { cout << "invalid ID\n"; continue; }
                repo.checkout(id);
                cout << "Checked out " << id << "\n";

            } else if (cmd == "save") {
                string file;
                if (!(in >> file)) { cout << "usage: save FILE\n"; continue; }
                repo.save(file);
                cout << "Saved to " << file << "\n";

            } else if (cmd == "load") {
                string file;
                if (!(in >> file)) { cout << "usage: load FILE\n"; continue; }
                repo.load(file);
                cout << "Loaded from " << file << "\n";

            } else if (cmd == "print") {
                cout << repo.working << "\n";

            } else if (cmd == "help") {
                help();

            } else if (cmd == "exit" || cmd == "quit") {
                break;

            } else {
                cout << "Unknown command. Type 'help'.\n";
            }
        } catch (const exception& e) {
            cout << "Error: " << e.what() << "\n";
        }
    }
    return 0;
}
