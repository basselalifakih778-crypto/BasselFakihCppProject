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
    string    message;
    string    content;
};

struct Repo {
    vector<Version> history;
    string working;

    unordered_map<string, VersionId> branches;
    string current_branch{"main"};
    bool detached{false};         
    VersionId head{0};             

    Repo() {
        branches[current_branch] = 0;
        head = 0;
    }

    static int64_t now_ns() {
        using namespace std::chrono;
        return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    }

    VersionId commit(string msg) {
        uint64_t new_hash = hash64(working);

        if (!history.empty() && history.back().content_hash == new_hash) {
            cout << "no content change\n";
            return head;
        }

        Version v;
        v.id = history.size() + 1;
        v.parent = head;
        v.ts_ns = now_ns();
        v.content_hash = new_hash;
        v.message = std::move(msg);
        v.content = working; 
        history.push_back(std::move(v));
        head = history.back().id;

        if (!detached) {
            branches[current_branch] = head;
        }
        return head;
    }

    const Version* get(VersionId id) const {
        if (id==0 || id > history.size()) return nullptr;
        return &history[id-1];
    }

    void checkout_version(VersionId id) {
        auto* v = const_cast<Repo*>(this)->get(id);
        if (!v) {
            cout << "no such version\n";
            return;
        }
        working = v->content;
        head = id;
        detached = true; 
    }

    bool switch_branch(const string& name) {
        auto it = branches.find(name);
        if (it == branches.end()) {
            cout << "no such branch\n";
            return false;
        }
        current_branch = name;
        detached = false;
        head = it->second;

        if (head == 0) {
            working.clear();
        } else {
            const Version* v = get(head);
            if (!v) {
                cout << "branch head invalid, resetting\n";
                working.clear();
                head = 0;
                branches[current_branch] = 0;
            } else {
                working = v->content;
            }
        }
        return true;
    }

    bool create_branch(const string& name, VersionId at) {
        if (branches.count(name)) {
            cout << "branch already exists\n";
            return false;
        }
        if (at != 0 && !get(at)) {
            cout << "no such version\n";
            return false;
        }
        branches[name] = at;
        cout << "Created branch '" << name << "' at " << at << "\n";
        return true;
    }

    bool delete_branch(const string& name) {
        if (!branches.count(name)) {
            cout << "no such branch\n";
            return false;
        }
        if (name == current_branch && !detached) {
            cout << "cannot delete current branch\n";
            return false;
        }
        branches.erase(name);
        cout << "Deleted branch '" << name << "'\n";
        return true;
    }

    void list_branches() const {
        for (const auto& [nm, hid] : branches) {
            bool is_cur = (!detached && nm == current_branch);
            cout << (is_cur ? "* " : "  ") << nm << " -> " << hid;
            const Version* v = (hid ? &history[hid-1] : nullptr);
            if (v) cout << "  (hash 0x" << to_hex(v->content_hash) << " msg: " << v->message << ")";
            cout << "\n";
        }
        if (detached) {
            cout << "* (detached) HEAD -> " << head << "\n";
        }
    }

    void status() const {
        cout << "HEAD: " << head
             << (detached ? " (detached)\n" : (" on branch '" + current_branch + "'\n"));
    }


    
    vector<VersionId> chain_from(VersionId tip) const {
        vector<VersionId> out;
        while (tip != 0) {
            out.push_back(tip);
            const Version* v = get(tip);
            if (!v) break;
            tip = v->parent;
        }
        return out;
    }

    void print_chain(vector<VersionId> ids, const string& label, VersionId head_id) const {
        reverse(ids.begin(), ids.end());
        cout << "=== " << label << " ===\n";
        for (VersionId id : ids) {
            const auto& v = history[id-1];
            cout << "id " << v.id
                 << (id == head_id ? " (HEAD)" : "")
                 << "  parent " << v.parent
                 << "  hash 0x" << to_hex(v.content_hash)
                 << "  time " << fmt_time_local(v.ts_ns)
                 << "  msg: " << v.message << "\n";
        }
        if (ids.empty()) cout << "(no commits)\n";
    }

    void save(const string& path) const {
        ofstream os(path); 
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

        os << "branches " << branches.size() << "\n";
        for (const auto& [nm, hid] : branches) {
            os << "bname " << std::quoted(nm) << " bhead " << static_cast<uint64_t>(hid) << "\n";
        }
        os << "current_branch " << std::quoted(current_branch) << "\n";
        os << "detached " << (detached ? 1 : 0) << "\n";

        os << "head " << static_cast<uint64_t>(head) << "\n";

        if (!os) cout<<"write failed\n";
    }

    void load(const string& path) {
        ifstream is(path);
        if (!is) { cout<<"cannot open file for read\n"; return; }

        history.clear();
        working.clear();
        head = 0;
        branches.clear();
        current_branch = "main";
        detached = false;

        string key;
        uint64_t count = 0;

        if (!(is >> key) || key != "count") { cout << "expected 'count'\n"; return; }
        if (!(is >> count)) { cout << "bad count\n"; return; }
        string dummy; getline(is, dummy);

        history.reserve(static_cast<size_t>(count));
        for (uint64_t i = 0; i < count; ++i) {
            Version v{};
            if (!(is >> key) || key != "id") { cout << "expected 'id'\n"; return; }
            if (!(is >> v.id)) { cout << "bad id\n"; return; }
            getline(is, dummy);

            if (!(is >> key) || key != "parent") { cout << "expected 'parent'\n"; return; }
            if (!(is >> v.parent)) { cout << "bad parent\n"; return; }
            getline(is, dummy);

            if (!(is >> key) || key != "ts_ns") { cout << "expected 'ts_ns'\n"; return; }
            if (!(is >> v.ts_ns)) { cout << "bad ts_ns\n"; return; }
            getline(is, dummy);

            if (!(is >> key) || key != "hash") { cout << "expected 'hash'\n"; return; }
            if (!(is >> v.content_hash)) { cout << "bad hash\n"; return; }
            getline(is, dummy);

            if (!(is >> key) || key != "message") { cout << "expected 'message'\n"; return; }
            if (!(is >> std::quoted(v.message))) { cout << "bad message\n"; return; }
            getline(is, dummy);

            if (!(is >> key) || key != "content") { cout << "expected 'content'\n"; return; }
            if (!(is >> std::quoted(v.content))) { cout << "bad content\n"; return; }
            getline(is, dummy);

            if (!std::getline(is, key)) { cout << "missing separator\n"; return; }
            if (key != "----") { cout << "expected '----'\n"; return; }

            history.push_back(std::move(v));
        }

        size_t bcount = 0;
        if (!(is >> key) || key != "branches") { cout << "expected 'branches'\n"; return; }
        if (!(is >> bcount)) { cout << "bad branches count\n"; return; }
        getline(is, dummy);

        for (size_t i = 0; i < bcount; ++i) {
            string nm;
            VersionId hid{};
            if (!(is >> key) || key != "bname") { cout << "expected 'bname'\n"; return; }
            if (!(is >> std::quoted(nm))) { cout << "bad branch name\n"; return; }
            if (!(is >> key) || key != "bhead") { cout << "expected 'bhead'\n"; return; }
            if (!(is >> hid)) { cout << "bad bhead\n"; return; }
            getline(is, dummy);
            branches[nm] = hid;
        }

        if (!(is >> key) || key != "current_branch") { cout << "expected 'current_branch'\n"; return; }
        if (!(is >> std::quoted(current_branch))) { cout << "bad current_branch\n"; return; }
        getline(is, dummy);

        int det = 0;
        if (!(is >> key) || key != "detached") { cout << "expected 'detached'\n"; return; }
        if (!(is >> det)) { cout << "bad detached\n"; return; }
        detached = (det != 0);
        getline(is, dummy);

        if (!(is >> key) || key != "head") { cout<<"expected 'head'\n"; return; }
        if (!(is >> head)) { cout<<"bad head\n"; return; }

        if (head != 0) {
            const Version* hv = get(head);
            if (hv) working = hv->content;
        } else {
            working.clear();
        }
        if (branches.empty()) branches["main"] = 0;
    }
};

static void help() {
    cout <<
         R"(Commands:
  set "TEXT"              Replace working content
  append "TEXT"           Append to working content
  erase POS LEN           Erase [POS, POS+LEN)
  commit "MSG"            Snapshot current working content (fails if no change)

  log [--all]             Show history for current branch (or all branches)
  blog NAME               Show history for a specific branch
  show ID                 Print content of version
  checkout ID             Set working to version content (enter detached HEAD)

  branch NAME [AT_ID]     Create a new branch at HEAD or at AT_ID
  branches                List branches
  switch NAME             Switch to branch NAME (leave detached, if any)
  delete-branch NAME      Delete a branch (not the current one)
  status                  Show branch/HEAD state

  save FILE               Save repo (with branches) to file
  load FILE               Load repo (with branches) from file

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
                cout << "Committed as " << id << (repo.detached ? " (detached)\n" : (" on branch " + repo.current_branch + "\n"));

            } else if (cmd == "log") {
                string opt;
                if (in >> opt && opt == "--all") {
                    for (const auto& kv : repo.branches) {
                        const string& nm = kv.first;
                        VersionId hid = kv.second;
                        auto ids = repo.chain_from(hid);
                        repo.print_chain(ids, "branch " + nm, hid);
                    }
                } else {
                    bool det = repo.detached;
                    VersionId tip = det ? repo.head : repo.branches.at(repo.current_branch);
                    auto ids = repo.chain_from(tip);
                    string label = det ? "(detached)" : ("branch " + repo.current_branch);
                    repo.print_chain(ids, label, tip);
                }

            } else if (cmd == "blog") {
                string name;
                if (!(in >> name)) { cout << "usage: blog NAME\n"; continue; }
                auto it = repo.branches.find(name);
                if (it == repo.branches.end()) { cout << "no such branch\n"; continue; }
                auto ids = repo.chain_from(it->second);
                repo.print_chain(ids, "branch " + name, it->second);

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
                repo.checkout_version(id);
                cout << "Checked out " << id << " (detached)\n";

            } else if (cmd == "branch") {
                string name; string atTok;
                if (!(in >> name)) { cout << "usage: branch NAME [AT_ID]\n"; continue; }
                VersionId at = repo.head;
                if (in >> atTok) {
                    try { at = stoull(atTok); }
                    catch (...) { cout << "invalid AT_ID\n"; continue; }
                }
                repo.create_branch(name, at);

            } else if (cmd == "branches") {
                repo.list_branches();

            } else if (cmd == "switch") {
                string name;
                if (!(in >> name)) { cout << "usage: switch NAME\n"; continue; }
                if (repo.switch_branch(name)) {
                    cout << "Switched to branch " << name << "\n";
                }

            } else if (cmd == "delete-branch") {
                string name;
                if (!(in >> name)) { cout << "usage: delete-branch NAME\n"; continue; }
                repo.delete_branch(name);

            } else if (cmd == "status") {
                repo.status();

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
