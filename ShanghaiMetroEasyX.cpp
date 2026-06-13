// ShanghaiMetroEasyX.cpp
// EasyX + C++17 single-file metro transfer guide demo.
// Compile on Windows with Visual Studio after installing EasyX.
// Project settings: Character Set = Use Unicode Character Set.

#include <graphics.h>
#include <conio.h>
#include <windows.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cwctype>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

const int WIN_W = 1400;
const int WIN_H = 820;
const int MAP_X = 10;
const int MAP_Y = 10;
const int MAP_W = 930;
const int MAP_H = 790;
const int PANEL_X = 960;
const int PANEL_Y = 10;
const int PANEL_W = 420;
const int PANEL_H = 790;
const int INF = 1000000000;
const int DEFAULT_EDGE_MIN = 3;
const int DEFAULT_TRANSFER_MIN = 2;

wstring trim(const wstring& s) {
    int l = 0;
    int r = (int)s.size() - 1;
    while (l <= r && iswspace(s[l])) l++;
    while (r >= l && iswspace(s[r])) r--;
    if (l > r) return L"";
    return s.substr(l, r - l + 1);
}

vector<wstring> splitLines(const wstring& s) {
    vector<wstring> lines;
    wstringstream ss(s);
    wstring line;
    while (getline(ss, line)) lines.push_back(line);
    return lines;
}

int toInt(const wstring& s, int defValue) {
    try {
        wstring t = trim(s);
        if (t.empty()) return defValue;
        return stoi(t);
    } catch (...) {
        return defValue;
    }
}

int parseTime(const wstring& s) {
    wstring t = trim(s);
    if (t.empty()) return -1;
    size_t p = t.find(L':');
    if (p == wstring::npos) return -2;
    try {
        int h = stoi(t.substr(0, p));
        int m = stoi(t.substr(p + 1));
        if (h < 0 || h > 23 || m < 0 || m > 59) return -2;
        return h * 60 + m;
    } catch (...) {
        return -2;
    }
}

wstring twoDigit(int x) {
    wchar_t buf[8];
    swprintf_s(buf, L"%02d", x);
    return buf;
}

wstring formatTime(int minute) {
    if (minute < 0 || minute >= INF / 2) return L"--:--";
    int day = minute / 1440;
    int m = minute % 1440;
    int h = m / 60;
    int mm = m % 60;
    wstring s = twoDigit(h) + L":" + twoDigit(mm);
    if (day > 0) s += L"+" + to_wstring(day) + L"天";
    return s;
}

wstring minText(int m) {
    return to_wstring(m) + L"分钟";
}

struct Edge {
    int to;
    int line;
    int minutes;
};

struct Station {
    wstring name;
    int x;
    int y;
    set<int> lines;
};

struct MetroLine {
    wstring name;
    COLORREF color;
    vector<int> stations;   // station order on this line
    int first = 6 * 60;     // terminal first departure
    int last = 22 * 60;     // terminal last departure
    int interval = 5;       // departure frequency
};

struct PathNode {
    int station = -1;
    int line = -1;          // line used to arrive here. -1 for start
    int arriveTime = -1;    // used only in time route
};

struct PathResult {
    vector<PathNode> path;
    int transfers = 0;
    int stops = 0;
    int totalMinutes = 0;
    bool withTime = false;
    wstring error;
};

struct PrevInfo {
    int prevStation = -1;
    int prevLine = -1;
};

class MetroGraph {
public:
    vector<Station> stations;
    vector<MetroLine> lines;
    vector<vector<Edge>> adj;
    map<wstring, int> stationId;
    map<wstring, int> lineId;

    int addLine(const wstring& rawName, COLORREF color, int first = 360, int last = 1320, int interval = 5) {
        wstring name = trim(rawName);
        if (name.empty()) return -1;
        auto it = lineId.find(name);
        if (it != lineId.end()) {
            int id = it->second;
            lines[id].first = first;
            lines[id].last = last;
            lines[id].interval = max(1, interval);
            return id;
        }
        MetroLine line;
        line.name = name;
        line.color = color;
        line.first = first;
        line.last = last;
        line.interval = max(1, interval);
        int id = (int)lines.size();
        lines.push_back(line);
        lineId[name] = id;
        return id;
    }

    int getOrAddStation(const wstring& rawName, int x, int y) {
        wstring name = trim(rawName);
        if (name.empty()) return -1;
        auto it = stationId.find(name);
        if (it != stationId.end()) {
            int id = it->second;
            stations[id].x = x;
            stations[id].y = y;
            return id;
        }
        Station s;
        s.name = name;
        s.x = x;
        s.y = y;
        int id = (int)stations.size();
        stations.push_back(s);
        stationId[name] = id;
        adj.push_back(vector<Edge>());
        return id;
    }

    bool hasStation(const wstring& name) const {
        return stationId.find(trim(name)) != stationId.end();
    }

    bool hasLine(const wstring& name) const {
        return lineId.find(trim(name)) != lineId.end();
    }

    int findStation(const wstring& name) const {
        auto it = stationId.find(trim(name));
        if (it == stationId.end()) return -1;
        return it->second;
    }

    int findLine(const wstring& name) const {
        auto it = lineId.find(trim(name));
        if (it == lineId.end()) return -1;
        return it->second;
    }

    bool addStationToLine(const wstring& lineName,
                          const wstring& stationName,
                          int x,
                          int y,
                          const wstring& prevStationName,
                          int edgeMinutes,
                          wstring& message) {
        int lid = findLine(lineName);
        if (lid < 0) {
            message = L"线路不存在，请先添加线路。";
            return false;
        }
        int sid = getOrAddStation(stationName, x, y);
        if (sid < 0) {
            message = L"站点名称不能为空。";
            return false;
        }
        stations[sid].lines.insert(lid);

        int prev = -1;
        wstring prevName = trim(prevStationName);
        if (!prevName.empty()) {
            prev = findStation(prevName);
            if (prev < 0) {
                message = L"连接前站不存在。";
                return false;
            }
            stations[prev].lines.insert(lid);
        }

        vector<int>& order = lines[lid].stations;
        if (find(order.begin(), order.end(), sid) == order.end()) {
            if (prev >= 0) {
                auto it = find(order.begin(), order.end(), prev);
                if (it != order.end()) order.insert(it + 1, sid);
                else order.push_back(sid);
            } else {
                order.push_back(sid);
            }
        }

        if (prev >= 0) {
            addUndirectedEdge(prev, sid, lid, max(1, edgeMinutes));
        }
        message = L"添加/修改站点成功。";
        return true;
    }

    void addLineSequence(const wstring& lineName, COLORREF color,
                         const vector<pair<wstring, POINT>>& seq,
                         int edgeMinutes = DEFAULT_EDGE_MIN) {
        int lid = addLine(lineName, color);
        int prev = -1;
        for (auto item : seq) {
            int sid = getOrAddStation(item.first, item.second.x, item.second.y);
            stations[sid].lines.insert(lid);
            if (find(lines[lid].stations.begin(), lines[lid].stations.end(), sid) == lines[lid].stations.end()) {
                lines[lid].stations.push_back(sid);
            }
            if (prev >= 0) addUndirectedEdge(prev, sid, lid, edgeMinutes);
            prev = sid;
        }
    }

    void addUndirectedEdge(int a, int b, int line, int minutes) {
        addDirectedEdge(a, b, line, minutes);
        addDirectedEdge(b, a, line, minutes);
    }

    void addDirectedEdge(int a, int b, int line, int minutes) {
        for (auto& e : adj[a]) {
            if (e.to == b && e.line == line) {
                e.minutes = minutes;
                return;
            }
        }
        Edge e;
        e.to = b;
        e.line = line;
        e.minutes = minutes;
        adj[a].push_back(e);
    }

    int edgeMinutes(int line, int a, int b) const {
        if (a < 0 || a >= (int)adj.size()) return DEFAULT_EDGE_MIN;
        for (const Edge& e : adj[a]) {
            if (e.to == b && e.line == line) return e.minutes;
        }
        return DEFAULT_EDGE_MIN;
    }

    int stationIndexOnLine(int line, int station) const {
        if (line < 0 || line >= (int)lines.size()) return -1;
        const vector<int>& v = lines[line].stations;
        for (int i = 0; i < (int)v.size(); ++i) {
            if (v[i] == station) return i;
        }
        return -1;
    }

    vector<int> prefixMinutes(int line) const {
        vector<int> pre;
        if (line < 0 || line >= (int)lines.size()) return pre;
        const vector<int>& v = lines[line].stations;
        pre.assign(v.size(), 0);
        for (int i = 1; i < (int)v.size(); ++i) {
            pre[i] = pre[i - 1] + edgeMinutes(line, v[i - 1], v[i]);
        }
        return pre;
    }

    int nextTrainDeparture(int line, int from, int to, int earliest) const {
        if (line < 0 || line >= (int)lines.size()) return INF;
        const MetroLine& L = lines[line];
        int i = stationIndexOnLine(line, from);
        int j = stationIndexOnLine(line, to);
        if (i < 0 || j < 0) return INF;

        vector<int> pre = prefixMinutes(line);
        if (pre.empty()) return INF;
        int total = pre.back();
        int offset = 0;
        if (j > i) offset = pre[i];
        else offset = total - pre[i];

        int firstAtStation = L.first + offset;
        int lastAtStation = L.last + offset;
        if (earliest <= firstAtStation) return firstAtStation;
        if (earliest > lastAtStation) return INF;

        int diff = earliest - firstAtStation;
        int k = (diff + L.interval - 1) / L.interval;
        int t = firstAtStation + k * L.interval;
        if (t > lastAtStation) return INF;
        return t;
    }

    PathResult routeNoTime(const wstring& startName, const wstring& endName) const {
        PathResult res;
        int s = findStation(startName);
        int t = findStation(endName);
        if (s < 0) {
            res.error = L"起点站不存在。";
            return res;
        }
        if (t < 0) {
            res.error = L"终点站不存在。";
            return res;
        }
        if (s == t) {
            res.path.push_back({s, -1, -1});
            return res;
        }

        int n = (int)stations.size();
        int m = (int)lines.size();
        int lineCount = m + 1;
        auto idx = [lineCount](int station, int line) {
            return station * lineCount + (line + 1);
        };
        auto decode = [lineCount](int key) {
            pair<int, int> r;
            r.first = key / lineCount;
            r.second = key % lineCount - 1;
            return r;
        };

        struct Cost {
            int transfer;
            int stop;
            int minute;
        };
        auto better = [](const Cost& a, const Cost& b) {
            if (a.transfer != b.transfer) return a.transfer < b.transfer;
            if (a.stop != b.stop) return a.stop < b.stop;
            return a.minute < b.minute;
        };
        struct QNode {
            int key;
            Cost c;
        };
        struct Cmp {
            bool operator()(const QNode& a, const QNode& b) const {
                if (a.c.transfer != b.c.transfer) return a.c.transfer > b.c.transfer;
                if (a.c.stop != b.c.stop) return a.c.stop > b.c.stop;
                return a.c.minute > b.c.minute;
            }
        };

        vector<Cost> dist(n * lineCount, {INF, INF, INF});
        vector<int> prev(n * lineCount, -1);
        priority_queue<QNode, vector<QNode>, Cmp> pq;

        int startKey = idx(s, -1);
        dist[startKey] = {0, 0, 0};
        pq.push({startKey, dist[startKey]});

        while (!pq.empty()) {
            QNode cur = pq.top();
            pq.pop();
            if (cur.c.transfer != dist[cur.key].transfer || cur.c.stop != dist[cur.key].stop || cur.c.minute != dist[cur.key].minute) continue;
            pair<int, int> now = decode(cur.key);
            int u = now.first;
            int curLine = now.second;
            for (const Edge& e : adj[u]) {
                int addTransfer = (curLine >= 0 && curLine != e.line) ? 1 : 0;
                Cost nc;
                nc.transfer = cur.c.transfer + addTransfer;
                nc.stop = cur.c.stop + 1;
                nc.minute = cur.c.minute + e.minutes + (addTransfer ? DEFAULT_TRANSFER_MIN : 0);
                int nk = idx(e.to, e.line);
                if (better(nc, dist[nk])) {
                    dist[nk] = nc;
                    prev[nk] = cur.key;
                    pq.push({nk, nc});
                }
            }
        }

        int bestKey = -1;
        Cost best = {INF, INF, INF};
        for (int l = -1; l < m; ++l) {
            int k = idx(t, l);
            if (better(dist[k], best)) {
                best = dist[k];
                bestKey = k;
            }
        }
        if (bestKey < 0 || best.transfer >= INF) {
            res.error = L"两站之间暂时没有可达路径。";
            return res;
        }

        vector<int> keys;
        for (int k = bestKey; k != -1; k = prev[k]) keys.push_back(k);
        reverse(keys.begin(), keys.end());
        for (int k : keys) {
            pair<int, int> p = decode(k);
            res.path.push_back({p.first, p.second, -1});
        }
        res.transfers = best.transfer;
        res.stops = best.stop;
        res.totalMinutes = best.minute;
        return res;
    }

    PathResult routeWithTime(const wstring& startName, const wstring& endName, int currentTime) const {
        PathResult res;
        res.withTime = true;
        int s = findStation(startName);
        int t = findStation(endName);
        if (s < 0) {
            res.error = L"起点站不存在。";
            return res;
        }
        if (t < 0) {
            res.error = L"终点站不存在。";
            return res;
        }
        if (s == t) {
            res.path.push_back({s, -1, currentTime});
            return res;
        }

        int n = (int)stations.size();
        int m = (int)lines.size();
        int lineCount = m + 1;
        auto idx = [lineCount](int station, int line) {
            return station * lineCount + (line + 1);
        };
        auto decode = [lineCount](int key) {
            pair<int, int> r;
            r.first = key / lineCount;
            r.second = key % lineCount - 1;
            return r;
        };

        struct Cost {
            int arrive;
            int transfer;
            int stop;
        };
        auto better = [](const Cost& a, const Cost& b) {
            if (a.arrive != b.arrive) return a.arrive < b.arrive;
            if (a.transfer != b.transfer) return a.transfer < b.transfer;
            return a.stop < b.stop;
        };
        struct QNode {
            int key;
            Cost c;
        };
        struct Cmp {
            bool operator()(const QNode& a, const QNode& b) const {
                if (a.c.arrive != b.c.arrive) return a.c.arrive > b.c.arrive;
                if (a.c.transfer != b.c.transfer) return a.c.transfer > b.c.transfer;
                return a.c.stop > b.c.stop;
            }
        };

        vector<Cost> dist(n * lineCount, {INF, INF, INF});
        vector<int> prev(n * lineCount, -1);
        priority_queue<QNode, vector<QNode>, Cmp> pq;
        int startKey = idx(s, -1);
        dist[startKey] = {currentTime, 0, 0};
        pq.push({startKey, dist[startKey]});

        while (!pq.empty()) {
            QNode cur = pq.top();
            pq.pop();
            if (cur.c.arrive != dist[cur.key].arrive || cur.c.transfer != dist[cur.key].transfer || cur.c.stop != dist[cur.key].stop) continue;
            pair<int, int> now = decode(cur.key);
            int u = now.first;
            int curLine = now.second;
            for (const Edge& e : adj[u]) {
                int ready = cur.c.arrive;
                int addTransfer = 0;
                if (curLine >= 0 && curLine != e.line) {
                    ready += DEFAULT_TRANSFER_MIN;
                    addTransfer = 1;
                }

                int depart = ready;
                if (curLine != e.line) {
                    depart = nextTrainDeparture(e.line, u, e.to, ready);
                    if (depart >= INF) continue;
                }

                Cost nc;
                nc.arrive = depart + e.minutes;
                nc.transfer = cur.c.transfer + addTransfer;
                nc.stop = cur.c.stop + 1;
                int nk = idx(e.to, e.line);
                if (better(nc, dist[nk])) {
                    dist[nk] = nc;
                    prev[nk] = cur.key;
                    pq.push({nk, nc});
                }
            }
        }

        int bestKey = -1;
        Cost best = {INF, INF, INF};
        for (int l = -1; l < m; ++l) {
            int k = idx(t, l);
            if (better(dist[k], best)) {
                best = dist[k];
                bestKey = k;
            }
        }
        if (bestKey < 0 || best.arrive >= INF) {
            res.error = L"当前时间没有可达列车，或两站之间暂时没有可达路径。";
            return res;
        }

        vector<int> keys;
        for (int k = bestKey; k != -1; k = prev[k]) keys.push_back(k);
        reverse(keys.begin(), keys.end());
        for (int k : keys) {
            pair<int, int> p = decode(k);
            res.path.push_back({p.first, p.second, dist[k].arrive});
        }
        res.transfers = best.transfer;
        res.stops = best.stop;
        res.totalMinutes = best.arrive - currentTime;
        return res;
    }
};

MetroGraph metro;
PathResult currentRoute;
wstring resultText = L"请输入起点站、终点站，点击查询。\n若输入当前时间，结果会计算每站到达时间。";

COLORREF autoColor(int index) {
    static COLORREF colors[] = {
        RGB(220, 20, 60), RGB(30, 144, 255), RGB(50, 160, 80),
        RGB(255, 140, 0), RGB(148, 0, 211), RGB(0, 170, 170),
        RGB(180, 120, 20), RGB(90, 90, 90), RGB(255, 105, 180),
        RGB(120, 170, 0), RGB(135, 80, 60), RGB(0, 110, 190),
        RGB(220, 80, 160), RGB(150, 150, 20)
    };
    return colors[index % (sizeof(colors) / sizeof(colors[0]))];
}

struct TextBox {
    int x = 0, y = 0, w = 0, h = 0;
    wstring text;
    wstring hint;
    bool focused = false;
};

struct Button {
    int x = 0, y = 0, w = 0, h = 0;
    wstring text;
    int id = 0;
};

vector<TextBox*> allBoxes;
vector<Button> buttons;

TextBox tbStart, tbEnd, tbTime;
TextBox tbLineName, tbFirst, tbLast, tbFreq;
TextBox tbStationLine, tbStationName, tbX, tbY, tbPrev, tbEdgeTime;
TextBox* focusedBox = nullptr;

void drawRoundRect(int x, int y, int w, int h, COLORREF fill, COLORREF border) {
    setfillcolor(fill);
    setlinecolor(border);
    solidroundrect(x, y, x + w, y + h, 6, 6);
    roundrect(x, y, x + w, y + h, 6, 6);
}

void drawTextBox(TextBox& tb) {
    COLORREF border = tb.focused ? RGB(30, 120, 220) : RGB(180, 180, 180);
    drawRoundRect(tb.x, tb.y, tb.w, tb.h, RGB(255, 255, 255), border);
    setbkmode(TRANSPARENT);
    settextstyle(18, 0, L"Microsoft YaHei");
    if (trim(tb.text).empty()) {
        settextcolor(RGB(150, 150, 150));
        outtextxy(tb.x + 8, tb.y + 6, tb.hint.c_str());
    } else {
        settextcolor(RGB(20, 20, 20));
        wstring show = tb.text;
        if ((int)show.size() > 18) show = show.substr(show.size() - 18);
        outtextxy(tb.x + 8, tb.y + 6, show.c_str());
    }
}

void drawButton(const Button& b) {
    drawRoundRect(b.x, b.y, b.w, b.h, RGB(235, 242, 255), RGB(80, 130, 210));
    setbkmode(TRANSPARENT);
    settextcolor(RGB(20, 70, 150));
    settextstyle(18, 0, L"Microsoft YaHei");
    int tx = b.x + 12;
    int ty = b.y + 7;
    outtextxy(tx, ty, b.text.c_str());
}

bool inRect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

void addButton(int x, int y, int w, int h, const wstring& text, int id) {
    Button b;
    b.x = x; b.y = y; b.w = w; b.h = h; b.text = text; b.id = id;
    buttons.push_back(b);
}

void label(int x, int y, const wstring& s, int size = 17, COLORREF c = RGB(30, 30, 30)) {
    setbkmode(TRANSPARENT);
    settextcolor(c);
    settextstyle(size, 0, L"Microsoft YaHei");
    outtextxy(x, y, s.c_str());
}

void setupControls() {
    int x = PANEL_X + 20;
    int y = PANEL_Y + 56;
    int labelW = 82;
    int boxW = 285;
    int h = 34;

    tbStart = {x + labelW, y, boxW, h, L"人民广场", L"起点站"};
    y += 42;
    tbEnd = {x + labelW, y, boxW, h, L"陆家嘴", L"终点站"};
    y += 42;
    tbTime = {x + labelW, y, boxW, h, L"08:00", L"当前时间，可空，例如 08:30"};
    y += 46;
    addButton(x, y, 175, 36, L"查询并高亮路径", 1);
    addButton(x + 190, y, 140, 36, L"清除路径", 2);

    y += 78;
    tbLineName = {x + labelW, y, boxW, h, L"18号线", L"线路名"};
    y += 42;
    tbFirst = {x + labelW, y, 82, h, L"06:00", L"首班"};
    tbLast = {x + labelW + 96, y, 82, h, L"22:00", L"末班"};
    tbFreq = {x + labelW + 192, y, 93, h, L"5", L"间隔分钟"};
    y += 46;
    addButton(x, y, 330, 36, L"添加/修改线路时刻", 3);

    y += 78;
    tbStationLine = {x + labelW, y, boxW, h, L"18号线", L"所属线路"};
    y += 42;
    tbStationName = {x + labelW, y, boxW, h, L"新站点", L"站点名"};
    y += 42;
    tbX = {x + labelW, y, 82, h, L"500", L"x"};
    tbY = {x + labelW + 96, y, 82, h, L"300", L"y"};
    tbEdgeTime = {x + labelW + 192, y, 93, h, L"3", L"耗时"};
    y += 42;
    tbPrev = {x + labelW, y, boxW, h, L"", L"连接前站，可空"};
    y += 46;
    addButton(x, y, 330, 36, L"添加站点/修改相邻站耗时", 4);

    allBoxes.clear();
    allBoxes.push_back(&tbStart);
    allBoxes.push_back(&tbEnd);
    allBoxes.push_back(&tbTime);
    allBoxes.push_back(&tbLineName);
    allBoxes.push_back(&tbFirst);
    allBoxes.push_back(&tbLast);
    allBoxes.push_back(&tbFreq);
    allBoxes.push_back(&tbStationLine);
    allBoxes.push_back(&tbStationName);
    allBoxes.push_back(&tbX);
    allBoxes.push_back(&tbY);
    allBoxes.push_back(&tbEdgeTime);
    allBoxes.push_back(&tbPrev);
}

wstring lineName(int id) {
    if (id < 0 || id >= (int)metro.lines.size()) return L"";
    return metro.lines[id].name;
}

wstring stationName(int id) {
    if (id < 0 || id >= (int)metro.stations.size()) return L"";
    return metro.stations[id].name;
}

wstring buildGuide(const PathResult& r, const wstring& start, const wstring& target, int inputTime) {
    if (!r.error.empty()) return r.error;
    wstringstream out;
    out << L"起点站：" << start << L"\n";
    out << L"终点站：" << target << L"\n";

    if (r.path.size() <= 1) {
        out << L"起点和终点相同，无需乘车。\n";
        return out.str();
    }

    out << L"经过站数：" << r.stops << L"\n";
    out << L"换乘次数：" << r.transfers << L"\n";
    out << L"预计总耗时：" << minText(r.totalMinutes) << L"\n";
    if (r.withTime) {
        out << L"出发查询时间：" << formatTime(inputTime) << L"\n";
        out << L"预计到达时间：" << formatTime(r.path.back().arriveTime) << L"\n";
    }
    out << L"\n乘车方案：\n";

    vector<wstring> transfers;
    int curLine = r.path[1].line;
    int segmentStart = r.path[0].station;
    int step = 1;
    out << step++ << L". 在【" << stationName(segmentStart) << L"】乘坐【" << lineName(curLine) << L"】。\n";

    for (int i = 2; i < (int)r.path.size(); ++i) {
        int nextLine = r.path[i].line;
        if (nextLine != curLine) {
            int transferStation = r.path[i - 1].station;
            transfers.push_back(stationName(transferStation));
            out << step++ << L". 乘坐【" << lineName(curLine) << L"】："
                << stationName(segmentStart) << L" → " << stationName(transferStation) << L"。\n";
            out << step++ << L". 在【" << stationName(transferStation) << L"】换乘【"
                << lineName(nextLine) << L"】，默认换乘 " << DEFAULT_TRANSFER_MIN << L" 分钟。\n";
            segmentStart = transferStation;
            curLine = nextLine;
        }
    }
    out << step++ << L". 乘坐【" << lineName(curLine) << L"】："
        << stationName(segmentStart) << L" → " << target << L"。\n";
    out << step++ << L". 到达终点【" << target << L"】。\n";

    out << L"\n换乘站：";
    if (transfers.empty()) out << L"无";
    else {
        for (int i = 0; i < (int)transfers.size(); ++i) {
            if (i) out << L"、";
            out << transfers[i];
        }
    }
    out << L"\n";

    out << L"完整路径：\n";
    for (int i = 0; i < (int)r.path.size(); ++i) {
        if (i) out << L" → ";
        out << stationName(r.path[i].station);
    }
    out << L"\n";

    if (r.withTime) {
        out << L"\n每站预计到达时间：\n";
        for (int i = 0; i < (int)r.path.size(); ++i) {
            out << formatTime(r.path[i].arriveTime) << L"  " << stationName(r.path[i].station) << L"\n";
        }
    }
    return out.str();
}

void doQuery() {
    wstring s = trim(tbStart.text);
    wstring t = trim(tbEnd.text);
    if (s.empty() || t.empty()) {
        resultText = L"起点站和终点站不能为空。";
        currentRoute = PathResult();
        return;
    }
    int tm = parseTime(tbTime.text);
    if (tm == -2) {
        resultText = L"当前时间格式错误，应为 HH:MM，例如 08:30；不输入则只计算普通换乘路径。";
        currentRoute = PathResult();
        return;
    }
    if (tm >= 0) currentRoute = metro.routeWithTime(s, t, tm);
    else currentRoute = metro.routeNoTime(s, t);
    resultText = buildGuide(currentRoute, s, t, tm);
}

void doClearRoute() {
    currentRoute = PathResult();
    resultText = L"已清除路径高亮。";
}

void doAddLine() {
    wstring name = trim(tbLineName.text);
    if (name.empty()) {
        resultText = L"线路名不能为空。";
        return;
    }
    int first = parseTime(tbFirst.text);
    int last = parseTime(tbLast.text);
    int freq = toInt(tbFreq.text, 5);
    if (first < 0 || last < 0 || first >= last || freq <= 0) {
        resultText = L"线路时刻格式错误。首班/末班使用 HH:MM，间隔必须为正整数。";
        return;
    }
    bool existed = metro.hasLine(name);
    metro.addLine(name, autoColor((int)metro.lines.size()), first, last, freq);
    resultText = existed ? L"已修改线路时刻。" : L"已添加新线路。";
}

void doAddStation() {
    wstring ln = trim(tbStationLine.text);
    wstring sn = trim(tbStationName.text);
    int x = toInt(tbX.text, -1);
    int y = toInt(tbY.text, -1);
    int edge = toInt(tbEdgeTime.text, DEFAULT_EDGE_MIN);
    if (ln.empty() || sn.empty()) {
        resultText = L"所属线路和站点名不能为空。";
        return;
    }
    if (x < MAP_X || x > MAP_X + MAP_W || y < MAP_Y || y > MAP_Y + MAP_H) {
        resultText = L"站点坐标应在左侧地图区域内；也可以直接点击地图自动填入 x、y。";
        return;
    }

    if (!metro.hasLine(ln)) {
        int first = parseTime(tbFirst.text);
        int last = parseTime(tbLast.text);
        int freq = toInt(tbFreq.text, 5);
        if (first < 0) first = 360;
        if (last < 0) last = 1320;
        if (freq <= 0) freq = 5;
        metro.addLine(ln, autoColor((int)metro.lines.size()), first, last, freq);
    }

    wstring msg;
    bool ok = metro.addStationToLine(ln, sn, x, y, tbPrev.text, edge, msg);
    resultText = msg;
    if (ok) {
        tbStart.text = sn;
        tbStationName.text = L"";
    }
}

void drawMap() {
    setfillcolor(RGB(250, 252, 255));
    solidrectangle(MAP_X, MAP_Y, MAP_X + MAP_W, MAP_Y + MAP_H);
    setlinecolor(RGB(210, 220, 235));
    rectangle(MAP_X, MAP_Y, MAP_X + MAP_W, MAP_Y + MAP_H);

    label(MAP_X + 18, MAP_Y + 15, L"上海地铁网络结构示意图", 24, RGB(0, 100, 190));
    label(MAP_X + 20, MAP_Y + 48, L"提示：点击地图可自动填入新增站点坐标；红色粗线为查询路径。", 16, RGB(90, 90, 90));

    set<int> highlightedStations;
    set<wstring> highlightedEdges;
    if (!currentRoute.path.empty()) {
        for (auto p : currentRoute.path) highlightedStations.insert(p.station);
        for (int i = 1; i < (int)currentRoute.path.size(); ++i) {
            int a = currentRoute.path[i - 1].station;
            int b = currentRoute.path[i].station;
            int l = currentRoute.path[i].line;
            int u = min(a, b), v = max(a, b);
            highlightedEdges.insert(to_wstring(l) + L"_" + to_wstring(u) + L"_" + to_wstring(v));
        }
    }

    set<wstring> drawn;
    for (int a = 0; a < (int)metro.adj.size(); ++a) {
        for (const Edge& e : metro.adj[a]) {
            int u = min(a, e.to), v = max(a, e.to);
            wstring key = to_wstring(e.line) + L"_" + to_wstring(u) + L"_" + to_wstring(v);
            if (drawn.count(key)) continue;
            drawn.insert(key);
            const Station& A = metro.stations[a];
            const Station& B = metro.stations[e.to];
            bool hi = highlightedEdges.count(key) > 0;
            setlinecolor(hi ? RGB(255, 40, 40) : metro.lines[e.line].color);
            setlinestyle(PS_SOLID, hi ? 8 : 4);
            line(A.x, A.y, B.x, B.y);
        }
    }
    setlinestyle(PS_SOLID, 1);

    for (int i = 0; i < (int)metro.stations.size(); ++i) {
        const Station& s = metro.stations[i];
        bool transfer = s.lines.size() > 1;
        bool hi = highlightedStations.count(i) > 0;
        int r = hi ? 8 : (transfer ? 6 : 5);
        setlinecolor(hi ? RGB(180, 0, 0) : RGB(40, 40, 40));
        setfillcolor(hi ? RGB(255, 245, 120) : RGB(255, 255, 255));
        fillcircle(s.x, s.y, r);
        setbkmode(TRANSPARENT);
        settextstyle(14, 0, L"Microsoft YaHei");
        settextcolor(hi ? RGB(180, 0, 0) : RGB(30, 30, 30));
        outtextxy(s.x + 7, s.y - 9, s.name.c_str());
    }

    int lx = MAP_X + MAP_W - 145;
    int ly = MAP_Y + 80;
    label(lx, ly - 28, L"线路图例", 17, RGB(60, 60, 60));
    for (int i = 0; i < (int)metro.lines.size() && i < 22; ++i) {
        int yy = ly + i * 25;
        setlinecolor(metro.lines[i].color);
        setlinestyle(PS_SOLID, 5);
        line(lx, yy + 8, lx + 35, yy + 8);
        setlinestyle(PS_SOLID, 1);
        settextstyle(14, 0, L"Microsoft YaHei");
        settextcolor(RGB(50, 50, 50));
        outtextxy(lx + 42, yy, metro.lines[i].name.c_str());
    }
}

void drawPanel() {
    setfillcolor(RGB(248, 248, 248));
    solidrectangle(PANEL_X, PANEL_Y, PANEL_X + PANEL_W, PANEL_Y + PANEL_H);
    setlinecolor(RGB(210, 210, 210));
    rectangle(PANEL_X, PANEL_Y, PANEL_X + PANEL_W, PANEL_Y + PANEL_H);

    label(PANEL_X + 20, PANEL_Y + 18, L"地铁换乘指南系统", 24, RGB(20, 80, 160));

    int x = PANEL_X + 20;
    int y = PANEL_Y + 56;
    int labelW = 82;
    label(x, y + 8, L"起点站"); drawTextBox(tbStart);
    y += 42;
    label(x, y + 8, L"终点站"); drawTextBox(tbEnd);
    y += 42;
    label(x, y + 8, L"当前时间"); drawTextBox(tbTime);
    for (const auto& b : buttons) {
        if (b.id == 1 || b.id == 2) drawButton(b);
    }

    y = PANEL_Y + 236;
    label(x, y, L"动态添加/修改线路", 18, RGB(0, 120, 90));
    y += 28;
    label(x, y + 8, L"线路名"); drawTextBox(tbLineName);
    y += 42;
    label(x, y + 8, L"时刻"); drawTextBox(tbFirst); drawTextBox(tbLast); drawTextBox(tbFreq);
    for (const auto& b : buttons) {
        if (b.id == 3) drawButton(b);
    }

    y = PANEL_Y + 392;
    label(x, y, L"动态添加/修改站点", 18, RGB(0, 120, 90));
    y += 28;
    label(x, y + 8, L"线路名"); drawTextBox(tbStationLine);
    y += 42;
    label(x, y + 8, L"站点名"); drawTextBox(tbStationName);
    y += 42;
    label(x, y + 8, L"坐标/耗时"); drawTextBox(tbX); drawTextBox(tbY); drawTextBox(tbEdgeTime);
    y += 42;
    label(x, y + 8, L"前一站"); drawTextBox(tbPrev);
    for (const auto& b : buttons) {
        if (b.id == 4) drawButton(b);
    }

    int ry = PANEL_Y + 602;
    label(PANEL_X + 20, ry, L"换乘指南", 19, RGB(160, 80, 0));
    setfillcolor(RGB(255, 255, 255));
    setlinecolor(RGB(215, 215, 215));
    solidrectangle(PANEL_X + 20, ry + 30, PANEL_X + PANEL_W - 20, PANEL_Y + PANEL_H - 18);
    rectangle(PANEL_X + 20, ry + 30, PANEL_X + PANEL_W - 20, PANEL_Y + PANEL_H - 18);

    vector<wstring> lines = splitLines(resultText);
    settextstyle(15, 0, L"Microsoft YaHei");
    settextcolor(RGB(30, 30, 30));
    setbkmode(TRANSPARENT);
    int yy = ry + 38;
    int maxLine = 11;
    for (int i = 0; i < (int)lines.size() && i < maxLine; ++i) {
        wstring show = lines[i];
        if ((int)show.size() > 34) show = show.substr(0, 33) + L"...";
        outtextxy(PANEL_X + 30, yy, show.c_str());
        yy += 20;
    }
    if ((int)lines.size() > maxLine) {
        settextcolor(RGB(120, 120, 120));
        outtextxy(PANEL_X + 30, yy, L"...... 结果较长，已显示前几行。可缩短线路或扩大窗口。 ");
    }
}

void drawAll() {
    cleardevice();
    drawMap();
    drawPanel();
}

void handleChar(wchar_t ch) {
    if (!focusedBox) return;
    if (ch == 8) { // Backspace
        if (!focusedBox->text.empty()) focusedBox->text.pop_back();
    } else if (ch == 13) { // Enter
        doQuery();
    } else if (ch >= 32) {
        if (focusedBox->text.size() < 40) focusedBox->text.push_back(ch);
    }
}

void handleImeChar(unsigned int code) {
    // WM_IME_CHAR 在某些 EasyX/MSVC 环境下会把中文作为 GBK 双字节放进 msg.ch。
    // 直接转 wchar_t 会显示成乱码，所以这里先按系统 ANSI 代码页转成 Unicode。
    if (!focusedBox) return;

    char mb[3] = {0, 0, 0};
    int len = 0;
    if (code <= 0xFF) {
        mb[0] = static_cast<char>(code & 0xFF);
        len = 1;
    } else {
        mb[0] = static_cast<char>((code >> 8) & 0xFF);
        mb[1] = static_cast<char>(code & 0xFF);
        len = 2;
    }

    wchar_t out[4] = {0, 0, 0, 0};
    int n = MultiByteToWideChar(CP_ACP, 0, mb, len, out, 4);
    if (n > 0) {
        for (int i = 0; i < n; ++i) handleChar(out[i]);
    }
}

void handleKeyDown(const ExMessage& msg, bool& running) {
    if (msg.vkcode == VK_ESCAPE) {
        running = false;
        return;
    }
    if (!focusedBox) return;

    if (msg.vkcode == VK_BACK) {
        handleChar(8);
    } else if (msg.vkcode == VK_RETURN) {
        handleChar(13);
    } else if (msg.vkcode == VK_DELETE) {
        focusedBox->text.clear();
    } else if ((GetKeyState(VK_CONTROL) & 0x8000) && (msg.vkcode == 'A')) {
        // 简化处理：Ctrl+A 直接清空当前输入框，方便重新输入。
        focusedBox->text.clear();
    }
    // 注意：普通字符不要在 WM_KEYDOWN 里追加。
    // 中文输入法提交的字符应走 WM_CHAR / WM_IME_CHAR，否则容易出现乱码。
}

void handleClick(int mx, int my) {
    bool hitBox = false;
    for (TextBox* tb : allBoxes) {
        tb->focused = false;
        if (inRect(mx, my, tb->x, tb->y, tb->w, tb->h)) {
            focusedBox = tb;
            tb->focused = true;
            hitBox = true;
        }
    }
    if (!hitBox) focusedBox = nullptr;

    for (const auto& b : buttons) {
        if (inRect(mx, my, b.x, b.y, b.w, b.h)) {
            if (b.id == 1) doQuery();
            else if (b.id == 2) doClearRoute();
            else if (b.id == 3) doAddLine();
            else if (b.id == 4) doAddStation();
            return;
        }
    }

    if (mx >= MAP_X && mx <= MAP_X + MAP_W && my >= MAP_Y && my <= MAP_Y + MAP_H) {
        tbX.text = to_wstring(mx);
        tbY.text = to_wstring(my);
        resultText = L"已把地图点击位置填入新增站点坐标。";
    }
}

POINT P(int x, int y) {
    POINT p;
    p.x = x;
    p.y = y;
    return p;
}

void buildDefaultMetro() {
    metro.addLineSequence(L"1号线", RGB(220, 20, 60), {
        {L"莘庄", P(80, 570)}, {L"上海南站", P(190, 535)}, {L"徐家汇", P(305, 465)},
        {L"陕西南路", P(420, 420)}, {L"人民广场", P(515, 370)}, {L"新闸路", P(525, 335)},
        {L"上海火车站", P(540, 300)}, {L"中山北路", P(560, 255)}, {L"上海马戏城", P(590, 210)}
    });

    metro.addLineSequence(L"2号线", RGB(30, 144, 255), {
        {L"虹桥火车站", P(85, 315)}, {L"虹桥2号航站楼", P(130, 315)}, {L"淞虹路", P(195, 315)},
        {L"中山公园", P(285, 325)}, {L"江苏路", P(350, 345)}, {L"静安寺", P(420, 355)},
        {L"南京西路", P(470, 365)}, {L"人民广场", P(515, 370)}, {L"南京东路", P(570, 370)},
        {L"陆家嘴", P(630, 360)}, {L"东昌路", P(690, 355)}, {L"世纪大道", P(740, 350)},
        {L"上海科技馆", P(795, 360)}, {L"龙阳路", P(850, 385)}, {L"张江高科", P(890, 425)},
        {L"川沙", P(900, 490)}, {L"浦东机场", P(900, 560)}
    });

    metro.addLineSequence(L"3号线", RGB(50, 160, 80), {
        {L"上海南站", P(190, 535)}, {L"宜山路", P(260, 480)}, {L"虹桥路", P(260, 405)},
        {L"中山公园", P(285, 325)}, {L"金沙江路", P(330, 260)}, {L"曹杨路", P(390, 260)},
        {L"镇坪路", P(460, 270)}, {L"上海火车站", P(540, 300)}, {L"宝山路", P(590, 295)},
        {L"虹口足球场", P(650, 285)}, {L"赤峰路", P(710, 260)}
    });

    metro.addLineSequence(L"4号线", RGB(255, 140, 0), {
        {L"宜山路", P(260, 480)}, {L"上海体育馆", P(300, 505)}, {L"上海体育场", P(350, 515)},
        {L"大木桥路", P(420, 525)}, {L"鲁班路", P(485, 520)}, {L"西藏南路", P(560, 510)},
        {L"南浦大桥", P(630, 500)}, {L"塘桥", P(690, 465)}, {L"蓝村路", P(725, 420)},
        {L"浦电路", P(745, 385)}, {L"世纪大道", P(740, 350)}, {L"浦东大道", P(710, 310)},
        {L"大连路", P(660, 305)}, {L"海伦路", P(620, 300)}, {L"宝山路", P(590, 295)}
    });

    metro.addLineSequence(L"7号线", RGB(148, 0, 211), {
        {L"美兰湖", P(360, 90)}, {L"上海大学", P(390, 160)}, {L"镇坪路", P(460, 270)},
        {L"静安寺", P(420, 355)}, {L"常熟路", P(390, 405)}, {L"肇嘉浜路", P(370, 460)},
        {L"龙华中路", P(420, 560)}, {L"后滩", P(560, 600)}, {L"龙阳路", P(850, 385)},
        {L"花木路", P(890, 360)}
    });

    metro.addLineSequence(L"8号线", RGB(0, 170, 170), {
        {L"市光路", P(690, 120)}, {L"虹口足球场", P(650, 285)}, {L"西藏北路", P(600, 320)},
        {L"曲阜路", P(555, 340)}, {L"人民广场", P(515, 370)}, {L"大世界", P(520, 405)},
        {L"老西门", P(530, 445)}, {L"陆家浜路", P(540, 480)}, {L"西藏南路", P(560, 510)},
        {L"中华艺术宫", P(595, 570)}, {L"东方体育中心", P(640, 640)}
    });

    metro.addLineSequence(L"9号线", RGB(120, 170, 0), {
        {L"松江南站", P(60, 720)}, {L"佘山", P(120, 665)}, {L"七宝", P(205, 610)},
        {L"漕河泾开发区", P(260, 540)}, {L"徐家汇", P(305, 465)}, {L"肇嘉浜路", P(370, 460)},
        {L"嘉善路", P(430, 465)}, {L"打浦桥", P(485, 465)}, {L"马当路", P(530, 465)},
        {L"小南门", P(610, 450)}, {L"商城路", P(690, 420)}, {L"世纪大道", P(740, 350)},
        {L"杨高中路", P(820, 320)}
    });

    metro.addLineSequence(L"10号线", RGB(180, 120, 20), {
        {L"虹桥火车站", P(85, 315)}, {L"虹桥2号航站楼", P(130, 315)}, {L"虹桥路", P(260, 405)},
        {L"交通大学", P(335, 430)}, {L"上海图书馆", P(380, 425)}, {L"陕西南路", P(420, 420)},
        {L"新天地", P(500, 430)}, {L"老西门", P(530, 445)}, {L"豫园", P(555, 410)},
        {L"南京东路", P(570, 370)}, {L"天潼路", P(595, 335)}, {L"四川北路", P(620, 320)},
        {L"海伦路", P(620, 300)}, {L"五角场", P(760, 210)}
    });

    metro.addLineSequence(L"11号线", RGB(135, 80, 60), {
        {L"嘉定北", P(185, 120)}, {L"南翔", P(260, 190)}, {L"祁连山路", P(330, 230)},
        {L"曹杨路", P(390, 260)}, {L"江苏路", P(350, 345)}, {L"交通大学", P(335, 430)},
        {L"徐家汇", P(305, 465)}, {L"上海游泳馆", P(320, 520)}, {L"龙华", P(360, 565)},
        {L"云锦路", P(440, 605)}, {L"东方体育中心", P(640, 640)}, {L"迪士尼", P(880, 650)}
    });

    metro.addLineSequence(L"12号线", RGB(0, 110, 190), {
        {L"七莘路", P(170, 570)}, {L"漕宝路", P(245, 535)}, {L"龙华中路", P(420, 560)},
        {L"嘉善路", P(430, 465)}, {L"陕西南路", P(420, 420)}, {L"南京西路", P(470, 365)},
        {L"汉中路", P(500, 325)}, {L"曲阜路", P(555, 340)}, {L"天潼路", P(595, 335)},
        {L"国际客运中心", P(655, 340)}, {L"大连路", P(660, 305)}, {L"巨峰路", P(850, 230)}
    });

    metro.addLineSequence(L"13号线", RGB(220, 80, 160), {
        {L"金运路", P(150, 250)}, {L"金沙江西路", P(220, 250)}, {L"真北路", P(285, 250)},
        {L"金沙江路", P(330, 260)}, {L"隆德路", P(390, 300)}, {L"武宁路", P(440, 315)},
        {L"汉中路", P(500, 325)}, {L"南京西路", P(470, 365)}, {L"淮海中路", P(455, 405)},
        {L"新天地", P(500, 430)}, {L"马当路", P(530, 465)}, {L"世博大道", P(550, 560)},
        {L"长清路", P(600, 610)}, {L"张江路", P(845, 505)}
    });
}

int main() {
    initgraph(WIN_W, WIN_H);
    setbkcolor(RGB(255, 255, 255));
    BeginBatchDraw();

    buildDefaultMetro();
    setupControls();

    bool running = true;
    while (running) {
        ExMessage msg;
        while (peekmessage(&msg, EX_MOUSE | EX_KEY | EX_CHAR)) {
            if (msg.message == WM_LBUTTONDOWN) {
                handleClick(msg.x, msg.y);
            } else if (msg.message == WM_KEYDOWN) {
                handleKeyDown(msg, running);
            } else if (msg.message == WM_CHAR) {
                if (msg.ch >= 32) handleChar((wchar_t)msg.ch);
            } else if (msg.message == WM_IME_CHAR) {
                handleImeChar((unsigned int)msg.ch);
            }
        }
        drawAll();
        FlushBatchDraw();
        Sleep(16);
    }

    EndBatchDraw();
    closegraph();
    return 0;
}
