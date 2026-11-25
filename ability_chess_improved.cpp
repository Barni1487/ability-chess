// ABILITY CHESS — графические шахматы на C++ и SFML 2.5+
// Возможности: легальные ходы, шах/мат/пат, рокировка, en passant,
// выбор превращения пешки, таймер 5+3, игра вдвоём/против простого ИИ,
// оценка материала, подсветка ходов, анимация и история.
// Сборка Linux: g++ -std=c++17 main.cpp -o ability-chess -lsfml-graphics -lsfml-window -lsfml-system
// Windows: подключите SFML 2.5+ к проекту Visual Studio и добавьте
// sfml-graphics, sfml-window, sfml-system в Linker -> Input.

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

struct Move {
    int from = -1, to = -1;
    char promotion = 0;
    bool enPassant = false, castle = false;
    bool operator==(const Move& o) const {
        return from == o.from && to == o.to && promotion == o.promotion &&
            enPassant == o.enPassant && castle == o.castle;
    }
};

struct State {
    array<char, 64> b{};
    bool white = true;
    bool wk = true, wq = true, bk = true, bq = true;
    int ep = -1, half = 0, full = 1;
};

static int fileOf(int s) { return s % 8; }
static int rankOf(int s) { return s / 8; }
static bool inside(int r, int f) { return r >= 0 && r < 8 && f >= 0 && f < 8; }
static int sq(int r, int f) { return r * 8 + f; }
static bool isWhite(char p) { return p >= 'A' && p <= 'Z'; }
static bool isPiece(char p) { return p != '.'; }
static char lowerPiece(char p) { return static_cast<char>(tolower(static_cast<unsigned char>(p))); }
static bool hasOnlyKing(const State& s, bool white) {
    bool foundKing = false;
    for (char piece : s.b) {
        if (!isPiece(piece) || isWhite(piece) != white) continue;
        if (lowerPiece(piece) != 'k') return false;
        foundKing = true;
    }
    return foundKing;
}
static string squareName(int s) {
    string x = "a1";
    x[0] = char('a' + fileOf(s));
    x[1] = char('8' - rankOf(s));
    return x;
}
static int value(char p) {
    switch (lowerPiece(p)) {
    case 'p': return 100; case 'n': case 'b': return 300;
    case 'r': return 500; case 'q': return 900; case 'k': return 20000;
    }
    return 0;
}
static State initialState() {
    State s;
    const string rows[8] = {
        "rnbqkbnr", "pppppppp", "........", "........",
        "........", "........", "PPPPPPPP", "RNBQKBNR"
    };
    for (int r = 0;r < 8;r++) for (int f = 0;f < 8;f++) s.b[sq(r, f)] = rows[r][f];
    return s;
}
static int kingSquare(const State& s, bool white) {
    char k = white ? 'K' : 'k';
    for (int i = 0;i < 64;i++) if (s.b[i] == k) return i;
    return -1;
}
static bool attacked(const State& s, int target, bool byWhite) {
    int tr = rankOf(target), tf = fileOf(target);
    int pawnSourceR = tr + (byWhite ? 1 : -1);
    for (int df : {-1, 1}) {
        int f = tf + df;
        if (inside(pawnSourceR, f) && s.b[sq(pawnSourceR, f)] == (byWhite ? 'P' : 'p')) return true;
    }
    const int knr[8] = { -2,-2,-1,-1,1,1,2,2 }, knf[8] = { -1,1,-2,2,-2,2,-1,1 };
    for (int i = 0;i < 8;i++) {
        int r = tr + knr[i], f = tf + knf[i];
        if (inside(r, f) && s.b[sq(r, f)] == (byWhite ? 'N' : 'n')) return true;
    }
    for (int dr = -1;dr <= 1;dr++) for (int df = -1;df <= 1;df++) {
        if (!dr && !df) continue;
        int r = tr + dr, f = tf + df;
        if (inside(r, f) && s.b[sq(r, f)] == (byWhite ? 'K' : 'k')) return true;
    }
    const int dirs[8][2] = { {-1,0},{1,0},{0,-1},{0,1},{-1,-1},{-1,1},{1,-1},{1,1} };
    for (int d = 0;d < 8;d++) {
        int r = tr + dirs[d][0], f = tf + dirs[d][1], distance = 1;
        while (inside(r, f)) {
            char p = s.b[sq(r, f)];
            if (isPiece(p)) {
                if (isWhite(p) == byWhite) {
                    char t = lowerPiece(p);
                    if (t == 'q' || (distance == 1 && t == 'k') ||
                        (d < 4 && t == 'r') || (d >= 4 && t == 'b')) return true;
                }
                break;
            }
            r += dirs[d][0]; f += dirs[d][1]; distance++;
        }
    }
    return false;
}
static bool inCheck(const State& s, bool white) {
    int k = kingSquare(s, white);
    return k >= 0 && attacked(s, k, !white);
}
static State makeMove(const State& s, const Move& m) {
    State n = s;
    char p = n.b[m.from], captured = n.b[m.to];
    n.b[m.from] = '.';
    if (m.enPassant) n.b[m.to + (s.white ? 8 : -8)] = '.';
    n.b[m.to] = m.promotion ? (s.white ? char(toupper(m.promotion)) : m.promotion) : p;
    if (m.castle) {
        if (m.to > m.from) {
            int rookFrom = s.white ? sq(7, 7) : sq(0, 7);
            int rookTo = m.to - 1; n.b[rookTo] = n.b[rookFrom]; n.b[rookFrom] = '.';
        }
        else {
            int rookFrom = s.white ? sq(7, 0) : sq(0, 0);
            int rookTo = m.to + 1; n.b[rookTo] = n.b[rookFrom]; n.b[rookFrom] = '.';
        }
    }
    if (p == 'K') n.wk = n.wq = false;
    if (p == 'k') n.bk = n.bq = false;
    if (m.from == sq(7, 7) || m.to == sq(7, 7)) n.wk = false;
    if (m.from == sq(7, 0) || m.to == sq(7, 0)) n.wq = false;
    if (m.from == sq(0, 7) || m.to == sq(0, 7)) n.bk = false;
    if (m.from == sq(0, 0) || m.to == sq(0, 0)) n.bq = false;
    n.ep = -1;
    if (lowerPiece(p) == 'p' && abs(rankOf(m.to) - rankOf(m.from)) == 2)
        n.ep = (m.from + m.to) / 2;
    n.half = (lowerPiece(p) == 'p' || isPiece(captured) || m.enPassant) ? 0 : s.half + 1;
    if (!s.white) n.full++;
    n.white = !s.white;
    return n;
}
static vector<Move> pseudoMoves(const State& s) {
    vector<Move> out;
    auto add = [&](int from, int to, bool ep = false, bool castle = false) {
        char p = s.b[from];
        if (lowerPiece(p) == 'p' && (rankOf(to) == 0 || rankOf(to) == 7)) {
            for (char q : {'q', 'r', 'b', 'n'}) out.push_back({ from,to,q,ep,castle });
        }
        else out.push_back({ from,to,0,ep,castle });
        };
    for (int from = 0;from < 64;from++) {
        char p = s.b[from]; if (!isPiece(p) || isWhite(p) != s.white) continue;
        int r = rankOf(from), f = fileOf(from); char t = lowerPiece(p);
        if (t == 'p') {
            int dir = s.white ? -1 : 1, start = s.white ? 6 : 1, nr = r + dir;
            if (inside(nr, f) && s.b[sq(nr, f)] == '.') {
                add(from, sq(nr, f));
                if (r == start && s.b[sq(r + 2 * dir, f)] == '.') add(from, sq(r + 2 * dir, f));
            }
            for (int df : {-1, 1}) {
                int nf = f + df;
                if (!inside(nr, nf)) continue;
                int to = sq(nr, nf);
                if (isPiece(s.b[to]) && isWhite(s.b[to]) != s.white) add(from, to);
                else if (to == s.ep) add(from, to, true);
            }
        }
        else if (t == 'n') {
            const int dr[8] = { -2,-2,-1,-1,1,1,2,2 }, df[8] = { -1,1,-2,2,-2,2,-1,1 };
            for (int i = 0;i < 8;i++) { int nr = r + dr[i], nf = f + df[i]; if (inside(nr, nf)) { int to = sq(nr, nf);if (!isPiece(s.b[to]) || isWhite(s.b[to]) != s.white)add(from, to); } }
        }
        else {
            vector<pair<int, int>> ds;
            if (t == 'b' || t == 'q') for (int dr : {-1, 1}) for (int df : {-1, 1}) ds.push_back({ dr,df });
            if (t == 'r' || t == 'q') { ds.push_back({ -1,0 });ds.push_back({ 1,0 });ds.push_back({ 0,-1 });ds.push_back({ 0,1 }); }
            if (t == 'k') for (int dr = -1;dr <= 1;dr++) for (int df = -1;df <= 1;df++) if (dr || df) ds.push_back({ dr,df });
            for (auto [dr, df] : ds) {
                int nr = r + dr, nf = f + df;
                while (inside(nr, nf)) {
                    int to = sq(nr, nf);
                    if (!isPiece(s.b[to])) add(from, to);
                    else { if (isWhite(s.b[to]) != s.white)add(from, to);break; }
                    if (t == 'k') break;
                    nr += dr;nf += df;
                }
            }
            if (t == 'k' && !inCheck(s, s.white)) {
                if (s.white && from == sq(7, 4)) {
                    if (s.wk && s.b[sq(7, 5)] == '.' && s.b[sq(7, 6)] == '.' && s.b[sq(7, 7)] == 'R' &&
                        !attacked(s, sq(7, 5), false) && !attacked(s, sq(7, 6), false)) add(from, sq(7, 6), false, true);
                    if (s.wq && s.b[sq(7, 1)] == '.' && s.b[sq(7, 2)] == '.' && s.b[sq(7, 3)] == '.' && s.b[sq(7, 0)] == 'R' &&
                        !attacked(s, sq(7, 3), false) && !attacked(s, sq(7, 2), false)) add(from, sq(7, 2), false, true);
                }
                if (!s.white && from == sq(0, 4)) {
                    if (s.bk && s.b[sq(0, 5)] == '.' && s.b[sq(0, 6)] == '.' && s.b[sq(0, 7)] == 'r' &&
                        !attacked(s, sq(0, 5), true) && !attacked(s, sq(0, 6), true)) add(from, sq(0, 6), false, true);
                    if (s.bq && s.b[sq(0, 1)] == '.' && s.b[sq(0, 2)] == '.' && s.b[sq(0, 3)] == '.' && s.b[sq(0, 0)] == 'r' &&
                        !attacked(s, sq(0, 3), true) && !attacked(s, sq(0, 2), true)) add(from, sq(0, 2), false, true);
                }
            }
        }
    }
    return out;
}
static vector<Move> legalMoves(const State& s) {
    vector<Move> result;
    for (const Move& m : pseudoMoves(s)) {
        State n = makeMove(s, m);
        if (!inCheck(n, s.white)) result.push_back(m);
    }
    return result;
}
static string moveText(const Move& m) {
    string x = squareName(m.from) + "-" + squareName(m.to);
    if (m.promotion) { x += "="; x += char(toupper(m.promotion)); }
    return x;
}
static int materialScore(const State& s) {
    int score = 0;
    for (char p : s.b) if (isPiece(p)) score += (isWhite(p) ? 1 : -1) * value(p);
    return score;
}
static int evaluate(const State& s) {
    int score = materialScore(s);
    // Небольшой бонус за развитие и центральные поля.
    for (int i = 0;i < 64;i++) if (isPiece(s.b[i])) {
        char p = lowerPiece(s.b[i]); int center = 3 - (abs(3 - fileOf(i)) + abs(3 - rankOf(i))) / 2;
        if (p == 'n' || p == 'b') score += (isWhite(s.b[i]) ? 1 : -1) * center * 5;
    }
    return s.white ? score : -score;
}
static int minimax(const State& s, int depth, int alpha, int beta) {
    auto moves = legalMoves(s);
    if (moves.empty()) return inCheck(s, s.white) ? -100000 - depth : 0;
    if (depth == 0) return evaluate(s);
    int best = -1000000;
    for (const Move& m : moves) {
        int score = -minimax(makeMove(s, m), depth - 1, -beta, -alpha);
        best = max(best, score); alpha = max(alpha, score);
        if (alpha >= beta) break;
    }
    return best;
}
static Move chooseComputerMove(const State& s) {
    auto moves = legalMoves(s); Move best = moves.front(); int bestScore = -1000000;
    for (const Move& m : moves) {
        int score = -minimax(makeMove(s, m), 2, -1000000, 1000000);
        if (score > bestScore) { bestScore = score;best = m; }
    }
    return best;
}
static string pieceName(char p) {
    switch (lowerPiece(p)) {
    case 'p': return "пешка"; case 'n': return "конь"; case 'b': return "слон";
    case 'r': return "ладья"; case 'q': return "ферзь"; case 'k': return "король";
    }
    return "";
}

int main() {
    const string windowTitle = "ABILITY CHESS — шахматы";
    sf::RenderWindow window(sf::VideoMode(1040, 760), sf::String::fromUtf8(windowTitle.begin(), windowTitle.end()), sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    sf::Font font;
    vector<string> fonts = {
#ifdef _WIN32
        "C:/Windows/Fonts/arial.ttf","C:/Windows/Fonts/segoeui.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
#endif
        "arial.ttf","DejaVuSans.ttf"
    };
    bool fontLoaded = false; for (const auto& f : fonts) if (font.loadFromFile(f)) { fontLoaded = true;break; }
    if (!fontLoaded) cerr << "Не найден шрифт. Установите DejaVu Sans или положите arial.ttf рядом с программой.\n";

    const float boardX = 28, boardY = 72, cell = 78;
    const float sideX = 680, sideW = 328;
    const string pieceOrder = "PNBRQKpnbrqk";
    array<sf::Texture, 12> pieceTextures;
    for (size_t i = 0; i < pieceOrder.size(); ++i) {
        const char pieceChar = pieceOrder[i];
        string fileName = isWhite(pieceChar) ? "w" : "b";
        fileName += static_cast<char>(toupper(static_cast<unsigned char>(pieceChar)));
        const string path = "assets/pieces/" + fileName + ".png";
        if (!pieceTextures[i].loadFromFile(path)) cerr << "Не удалось загрузить фигуру: " << path << '\n';
        pieceTextures[i].setSmooth(true);
    }
    State state = initialState();
    vector<State> states{ state };
    vector<string> history;
    vector<Move> selectedMoves;
    int selected = -1;
    bool showMenu = true;
    bool vsComputer = false, gameOver = false, promotionPending = false;
    bool whiteBelongsToPlayerOne = true;
    bool whiteHasMoved = false, blackHasMoved = false;
    string gameOverPopup;
    auto toBoardSquare = [&](int displayRow, int displayFile) {
        return whiteBelongsToPlayerOne
            ? sq(displayRow, displayFile)
            : sq(7 - displayRow, 7 - displayFile);
    };
    auto toDisplaySquare = [&](int square) {
        const int row = rankOf(square), file = fileOf(square);
        return whiteBelongsToPlayerOne
            ? pair<int, int>{ row, file }
            : pair<int, int>{ 7 - row, 7 - file };
    };
    vector<Move> promotionMoves;
    string message = "Белые ходят. Выберите фигуру мышью.";
    float whiteTime = 300.f, blackTime = 300.f;
    const float increment = 3.f;
    auto last = chrono::steady_clock::now();
    bool animating = false; Move animMove; float animProgress = 0.f; char animPiece = '.';
    bool aiThinking = false;
    future<Move> aiFuture;
    unsigned int gameGeneration = 0, aiGeneration = 0;
    const float moveAnimationSeconds = 0.32f;

    auto reset = [&]() {
        ++gameGeneration;
        state = initialState();states.assign(1, state);history.clear();selected = -1;selectedMoves.clear();
        gameOver = false;promotionPending = false;promotionMoves.clear();whiteTime = blackTime = 300.f;
        whiteHasMoved = blackHasMoved = false;
        gameOverPopup.clear();
        message = "Новая партия. Белые ходят.";animating = false;
        };
    auto finishStatus = [&]() {
        auto moves = legalMoves(state);
        if (moves.empty()) {
            gameOver = true;
            if (inCheck(state, state.white)) {
                gameOverPopup = state.white == whiteBelongsToPlayerOne
                    ? "Вам объявлен мат"
                    : "Вы объявили мат королю соперника";
            }
            else gameOverPopup = "Пат, ничья";
        }
        else if (state.half >= 100) { gameOver = true; gameOverPopup = "Ничья по правилу 50 ходов"; }
        };
    auto commit = [&](Move m) {
        char moving = state.b[m.from];
        if (state.white) whiteHasMoved = true; else blackHasMoved = true;
        State next = makeMove(state, m);
        string notation = (state.white ? "Белые: " : "Чёрные: ") + moveText(m);
        if (m.promotion) notation += " (превращение в " + pieceName(m.promotion) + ")";
        history.push_back(notation);
        state = next;states.push_back(state);
        if (moving == 'P' || moving == 'p' || isPiece(states[states.size() - 2].b[m.to])) {
            // increment is awarded after every completed move, independently of capture.
        }
        if (state.white) blackTime += increment; else whiteTime += increment;
        selected = -1;selectedMoves.clear();promotionPending = false;promotionMoves.clear();
        animating = true;animMove = m;animPiece = m.promotion ? (state.white ? char(toupper(m.promotion)) : m.promotion) : moving;animProgress = 0;
        finishStatus();
        if (!gameOver) message = string(state.white ? "Ход белых." : "Ход чёрных.") + (inCheck(state, state.white) ? " ШАХ!" : "");
        };

    while (window.isOpen()) {
        auto now = chrono::steady_clock::now();
        float dt = chrono::duration<float>(now - last).count(); last = now;
        const bool currentSideHasMoved = state.white ? whiteHasMoved : blackHasMoved;
        if (!showMenu && !gameOver && !promotionPending && !animating && currentSideHasMoved) {
            if (state.white) whiteTime = max(0.f, whiteTime - dt); else blackTime = max(0.f, blackTime - dt);
            if (whiteTime <= 0 || blackTime <= 0) {
                gameOver = true;
                const bool expiredWhite = whiteTime <= 0;
                const bool playerOneLost = expiredWhite == whiteBelongsToPlayerOne;
                const bool opponentIsWhite = !expiredWhite;
                if (hasOnlyKing(state, opponentIsWhite)) {
                    const string expiredColor = expiredWhite ? "белых" : "чёрных";
                    const string insufficientColor = opponentIsWhite ? "белых" : "чёрных";
                    gameOverPopup = "Время " + expiredColor + " закончилось,\nно у " + insufficientColor + " недостаточно фигур для мата. Ничья";
                }
                else {
                    gameOverPopup = vsComputer
                        ? (playerOneLost ? "Время вышло. Вы проиграли" : "Время вышло. Вы победили")
                        : (expiredWhite ? "Время белых истекло. Победили чёрные" : "Время чёрных истекло. Победили белые");
                }
            }
        }
        if (animating) {
            animProgress = min(1.f, animProgress + dt / moveAnimationSeconds);
            if (animProgress >= 1.f) animating = false;
        }
        if (aiThinking && !animating && aiFuture.valid() && aiFuture.wait_for(chrono::milliseconds(0)) == future_status::ready) {
            const Move computerMove = aiFuture.get();
            aiThinking = false;
            if (aiGeneration == gameGeneration && !showMenu && vsComputer && state.white != whiteBelongsToPlayerOne && !gameOver && !promotionPending && !animating)
                commit(computerMove);
        }
        if (!showMenu && vsComputer && state.white != whiteBelongsToPlayerOne && !gameOver && !promotionPending && !aiThinking && !aiFuture.valid()) {
            const State position = state;
            aiGeneration = gameGeneration;
            aiThinking = true;
            message = "Компьютер думает...";
            aiFuture = async(launch::async, [position]() { return chooseComputerMove(position); });
        }

        sf::Event ev;
        while (window.pollEvent(ev)) {
            if (ev.type == sf::Event::Closed) window.close();
            if (ev.type == sf::Event::MouseButtonPressed && ev.mouseButton.button == sf::Mouse::Left) {
                float mx = ev.mouseButton.x, my = ev.mouseButton.y;
                if (showMenu) {
                    if (my >= 386.f && my < 466.f) {
                        if (mx >= 260.f && mx < 510.f) {
                            vsComputer = true;
                            reset();
                            showMenu = false;
                        }
                        else if (mx >= 530.f && mx < 780.f) {
                            vsComputer = false;
                            reset();
                            showMenu = false;
                        }
                    }
                    continue;
                }
                // Controls in the right sidebar: return to menu and restart.
                if (mx >= sideX && mx < sideX + sideW && my >= 16 && my < 62) {
                    if (mx < sideX + 160) { reset(); showMenu = true; }
                    else if (mx >= sideX + 168) {
                        whiteBelongsToPlayerOne = !whiteBelongsToPlayerOne;
                        reset();
                        message = "Цвета игроков поменялись. Белые ходят.";
                    }
                    continue;
                }
                if (promotionPending) {
                    for (size_t i = 0;i < promotionMoves.size();i++) {
                        float px = 650 + (i % 4) * 82, py = 580;
                        if (mx >= px && mx < px + 70 && my >= py && my < py + 58) { commit(promotionMoves[i]);break; }
                    }
                    continue;
                }
                if (gameOver || animating || (vsComputer && state.white != whiteBelongsToPlayerOne)) continue;
                int displayFile = int((mx - boardX) / cell), displayRow = int((my - boardY) / cell);
                if (mx >= boardX && mx < boardX + 8 * cell && my >= boardY && my < boardY + 8 * cell) {
                    int target = toBoardSquare(displayRow, displayFile);
                    if (selected < 0) {
                        if (isPiece(state.b[target]) && isWhite(state.b[target]) == state.white) {
                            selected = target;selectedMoves.clear();
                            for (auto m : legalMoves(state)) if (m.from == selected) selectedMoves.push_back(m);
                        }
                    }
                    else {
                        vector<Move> choices;
                        for (auto m : selectedMoves) if (m.to == target) choices.push_back(m);
                        if (!choices.empty()) {
                            if (choices.size() > 1) { promotionPending = true;promotionMoves = choices;message = "Выберите фигуру превращения:"; }
                            else commit(choices[0]);
                        }
                        else if (isPiece(state.b[target]) && isWhite(state.b[target]) == state.white) {
                            selected = target;selectedMoves.clear();
                            for (auto m : legalMoves(state)) if (m.from == selected) selectedMoves.push_back(m);
                        }
                        else { selected = -1;selectedMoves.clear(); }
                    }
                }
            }
        }

        window.clear(sf::Color(24, 29, 39));
        auto drawText = [&](const string& str, float x, float y, unsigned size, sf::Color color = sf::Color::White) {
            if (!fontLoaded) return;
            sf::Text t(sf::String::fromUtf8(str.begin(), str.end()), font, size);t.setFillColor(color);t.setPosition(x, y);window.draw(t);
            };
        if (showMenu) {
            const sf::Color panelBg(17, 20, 27), panelEdge(54, 60, 70), accent(238, 198, 111);
            sf::RectangleShape menuPanel({ 650.f, 320.f });
            menuPanel.setPosition(195.f, 220.f);
            menuPanel.setFillColor(panelBg);
            menuPanel.setOutlineThickness(1.f);
            menuPanel.setOutlineColor(panelEdge);
            window.draw(menuPanel);

            auto drawCenteredText = [&](const string& str, float centerX, float centerY, unsigned size, sf::Color color) {
                if (!fontLoaded) return;
                sf::Text text(sf::String::fromUtf8(str.begin(), str.end()), font, size);
                text.setFillColor(color);
                const sf::FloatRect bounds = text.getLocalBounds();
                text.setOrigin(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
                text.setPosition(centerX, centerY);
                window.draw(text);
            };
            drawCenteredText("ABILITY CHESS", 520.f, 286.f, 32, accent);
            drawCenteredText("Выберите режим игры", 520.f, 337.f, 18, sf::Color(190, 195, 202));

            const auto drawMenuButton = [&](float x, const string& label, bool primary) {
                sf::RectangleShape button({ 250.f, 80.f });
                button.setPosition(x, 386.f);
                button.setFillColor(primary ? sf::Color(65, 91, 61) : sf::Color(42, 48, 59));
                button.setOutlineThickness(1.f);
                button.setOutlineColor(primary ? sf::Color(112, 166, 75) : panelEdge);
                window.draw(button);
                drawCenteredText(label, x + 125.f, 426.f, 18, sf::Color(245, 246, 239));
            };
            drawMenuButton(260.f, "Игра с компьютером", true);
            drawMenuButton(530.f, "Игра с другом", false);
            drawCenteredText("ABILITY CHESS · шахматы", 520.f, 486.f, 13, sf::Color(125, 132, 142));
        }
        else {
        drawText("ABILITY CHESS", 28, 18, 27, sf::Color(238, 198, 111));
        // Board and highlights
        for (int displayRow = 0;displayRow < 8;displayRow++) for (int displayFile = 0;displayFile < 8;displayFile++) {
            int s = toBoardSquare(displayRow, displayFile); sf::RectangleShape tile({ cell,cell });tile.setPosition(boardX + displayFile * cell, boardY + displayRow * cell);
            bool light = (displayRow + displayFile) % 2 == 0;tile.setFillColor(light ? sf::Color(238, 216, 181) : sf::Color(181, 136, 99));window.draw(tile);
            if (s == selected) { tile.setFillColor(sf::Color(235, 205, 75, 190));window.draw(tile); }
            for (auto m : selectedMoves) if (m.to == s) {
                sf::CircleShape dot(isPiece(state.b[s]) ? 27.f : 10.f);
                dot.setOrigin(dot.getRadius(), dot.getRadius());dot.setPosition(boardX + displayFile * cell + cell / 2, boardY + displayRow * cell + cell / 2);
                dot.setFillColor(isPiece(state.b[s]) ? sf::Color(180, 45, 45, 125) : sf::Color(40, 100, 55, 150));window.draw(dot);
            }
            if (isPiece(state.b[s]) && !(animating && s == animMove.to)) {
                const size_t textureIndex = pieceOrder.find(state.b[s]);
                if (textureIndex < pieceTextures.size() && pieceTextures[textureIndex].getSize().x > 0) {
                    const sf::Vector2u textureSize = pieceTextures[textureIndex].getSize();
                    const float pieceSize = cell * 0.9f;
                    sf::Sprite piece(pieceTextures[textureIndex]);
                    piece.setScale(pieceSize / static_cast<float>(textureSize.x), pieceSize / static_cast<float>(textureSize.y));
                    piece.setPosition(boardX + displayFile * cell + (cell - pieceSize) / 2.f, boardY + displayRow * cell + (cell - pieceSize) / 2.f);
                    window.draw(piece);
                }
            }
        }
        if (animating) {
            const pair<int, int> fromDisplay = toDisplaySquare(animMove.from);
            const pair<int, int> toDisplay = toDisplaySquare(animMove.to);
            const float x1 = boardX + fromDisplay.second * cell + cell / 2.f;
            const float y1 = boardY + fromDisplay.first * cell + cell / 2.f;
            const float x2 = boardX + toDisplay.second * cell + cell / 2.f;
            const float y2 = boardY + toDisplay.first * cell + cell / 2.f;
            const size_t textureIndex = pieceOrder.find(animPiece);
            if (textureIndex < pieceTextures.size() && pieceTextures[textureIndex].getSize().x > 0) {
                const sf::Vector2u textureSize = pieceTextures[textureIndex].getSize();
                const float pieceSize = cell * 0.9f;
                sf::Sprite moving(pieceTextures[textureIndex]);
                moving.setOrigin(textureSize.x / 2.f, textureSize.y / 2.f);
                moving.setScale(pieceSize / static_cast<float>(textureSize.x), pieceSize / static_cast<float>(textureSize.y));
                const float easedProgress = animProgress * animProgress * (3.f - 2.f * animProgress);
                moving.setPosition(x1 + (x2 - x1) * easedProgress, y1 + (y2 - y1) * easedProgress);
                window.draw(moving);
            }
        }

        // Coordinates are drawn after the pieces so they stay visible in each square.
        for (int displayRow = 0; displayRow < 8; ++displayRow) {
            const bool lightSquare = (displayRow % 2 == 0);
            const sf::Color coordinateColor = lightSquare ? sf::Color(92, 60, 38) : sf::Color(255, 238, 211);
            const int rankLabel = whiteBelongsToPlayerOne ? 8 - displayRow : displayRow + 1;
            drawText(to_string(rankLabel), boardX + 3.f, boardY + displayRow * cell + 2.f, 14, coordinateColor);
        }
        for (int displayFile = 0; displayFile < 8; ++displayFile) {
            const bool lightSquare = ((7 + displayFile) % 2 == 0);
            const sf::Color coordinateColor = lightSquare ? sf::Color(92, 60, 38) : sf::Color(255, 238, 211);
            const char fileLabel = whiteBelongsToPlayerOne ? char('a' + displayFile) : char('h' - displayFile);
            drawText(string(1, fileLabel), boardX + displayFile * cell + cell - 16.f, boardY + 8.f * cell - 19.f, 14, coordinateColor);
        }
        // Lichess-inspired player cards and move list.
        const sf::Color panelBg(12, 13, 15), panelAlt(20, 22, 24), panelText(218, 221, 225), mutedText(145, 150, 156), activeGreen(112, 166, 49), latestMoveBg(34, 49, 64);
        auto clockText = [](float t) {
            const int tenths = max(0, static_cast<int>(t * 10.f));
            ostringstream out;
            out << setfill('0') << setw(2) << tenths / 600 << ':' << setw(2) << (tenths / 10) % 60 << '.' << tenths % 10;
            return out.str();
        };
        auto drawPlayerCard = [&](float y, const string& name, float time, bool active) {
            sf::RectangleShape card({ sideW, 100.f });
            card.setPosition(sideX, y);
            card.setFillColor(panelBg);
            card.setOutlineThickness(1.f);
            card.setOutlineColor(sf::Color(35, 37, 40));
            window.draw(card);
            sf::RectangleShape turnBar({ active ? 4.f : 2.f, 84.f });
            turnBar.setPosition(sideX, y + 8.f);
            turnBar.setFillColor(active ? activeGreen : sf::Color(62, 65, 69));
            window.draw(turnBar);
            sf::CircleShape dot(5.f);
            dot.setPosition(sideX + 14.f, y + 15.f);
            dot.setFillColor(active ? activeGreen : sf::Color(95, 99, 104));
            window.draw(dot);
            drawText(name, sideX + 30.f, y + 9.f, 14, panelText);
            drawText("5 + 3", sideX + sideW - 54.f, y + 9.f, 13, mutedText);
            drawText(clockText(time), sideX + 14.f, y + 37.f, 42, active ? sf::Color(245, 246, 239) : sf::Color(177, 180, 184));
        };
        const string blackPlayerLabel = vsComputer
            ? (whiteBelongsToPlayerOne ? "ЧЁРНЫЕ · КОМПЬЮТЕР" : "ЧЁРНЫЕ · ВЫ")
            : (whiteBelongsToPlayerOne ? "ЧЁРНЫЕ · ИГРОК 2" : "ЧЁРНЫЕ · ИГРОК 1");
        const string whitePlayerLabel = vsComputer
            ? (whiteBelongsToPlayerOne ? "БЕЛЫЕ · ВЫ" : "БЕЛЫЕ · КОМПЬЮТЕР")
            : (whiteBelongsToPlayerOne ? "БЕЛЫЕ · ИГРОК 1" : "БЕЛЫЕ · ИГРОК 2");
        const bool topIsWhite = !whiteBelongsToPlayerOne;
        drawPlayerCard(72.f, topIsWhite ? whitePlayerLabel : blackPlayerLabel,
            topIsWhite ? whiteTime : blackTime, state.white == topIsWhite && !gameOver);

        sf::RectangleShape modeButton({ 160.f, 42.f });
        modeButton.setPosition(sideX, 16.f);
        const sf::Vector2i buttonMousePosition = sf::Mouse::getPosition(window);
        const bool menuButtonHovered = buttonMousePosition.x >= sideX && buttonMousePosition.x < sideX + 160.f &&
            buttonMousePosition.y >= 16.f && buttonMousePosition.y < 58.f;
        modeButton.setFillColor(menuButtonHovered ? sf::Color(62, 79, 102) : panelAlt);
        modeButton.setOutlineThickness(1.f);
        modeButton.setOutlineColor(menuButtonHovered ? sf::Color(126, 157, 105) : sf::Color(47, 50, 54));
        window.draw(modeButton);
        drawText("В меню", sideX + 16.f, 29.f, 15, panelText);
        sf::RectangleShape newGameButton({ 160.f, 42.f });
        newGameButton.setPosition(sideX + 168.f, 16.f);
        const bool newGameHovered = buttonMousePosition.x >= sideX + 168.f && buttonMousePosition.x < sideX + sideW &&
            buttonMousePosition.y >= 16.f && buttonMousePosition.y < 58.f;
        newGameButton.setFillColor(newGameHovered ? sf::Color(62, 79, 102) : panelAlt);
        newGameButton.setOutlineThickness(1.f);
        newGameButton.setOutlineColor(newGameHovered ? sf::Color(126, 157, 105) : sf::Color(47, 50, 54));
        window.draw(newGameButton);
        drawText("Новая партия", sideX + 182.f, 29.f, 15, panelText);

        sf::RectangleShape movePanel({ sideW, 394.f });
        movePanel.setPosition(sideX, 180.f);
        movePanel.setFillColor(panelBg);
        movePanel.setOutlineThickness(1.f);
        movePanel.setOutlineColor(sf::Color(35, 37, 40));
        window.draw(movePanel);
        drawText("ХОДЫ", sideX + 12.f, 189.f, 14, mutedText);
        drawText("№", sideX + 14.f, 213.f, 12, mutedText);
        drawText("БЕЛЫЕ", sideX + 50.f, 213.f, 12, mutedText);
        drawText("ЧЁРНЫЕ", sideX + 190.f, 213.f, 12, mutedText);
        sf::RectangleShape headerLine({ sideW - 20.f, 1.f });
        headerLine.setPosition(sideX + 10.f, 232.f);
        headerLine.setFillColor(sf::Color(39, 41, 44));
        window.draw(headerLine);

        const auto shortMove = [](string entry) {
            const size_t separator = entry.find(": ");
            return separator == string::npos ? entry : entry.substr(separator + 2);
        };
        constexpr int visibleTurnRows = 10;
        const int totalTurnRows = static_cast<int>((history.size() + 1) / 2);
        const int firstTurn = max(0, totalTurnRows - visibleTurnRows);
        const int shownTurnRows = min(visibleTurnRows, totalTurnRows);
        for (int row = 0; row < shownTurnRows; ++row) {
            const int turn = firstTurn + row;
            const float y = 235.f + row * 29.f;
            if (row % 2 == 1) {
                sf::RectangleShape stripe({ sideW - 20.f, 24.f });
                stripe.setPosition(sideX + 10.f, y);
                stripe.setFillColor(panelAlt);
                window.draw(stripe);
            }
            const size_t whiteIndex = static_cast<size_t>(turn * 2);
            const size_t blackIndex = whiteIndex + 1;
            if (whiteIndex + 1 == history.size() || blackIndex == history.size() - 1) {
                const float x = blackIndex == history.size() - 1 ? sideX + 185.f : sideX + 45.f;
                sf::RectangleShape highlight({ 132.f, 23.f });
                highlight.setPosition(x, y);
                highlight.setFillColor(latestMoveBg);
                window.draw(highlight);
            }
            drawText(to_string(turn + 1), sideX + 15.f, y + 4.f, 13, mutedText);
            if (whiteIndex < history.size()) drawText(shortMove(history[whiteIndex]), sideX + 52.f, y + 4.f, 14, panelText);
            if (blackIndex < history.size()) drawText(shortMove(history[blackIndex]), sideX + 192.f, y + 4.f, 14, panelText);
        }
        if (history.empty()) drawText("Партия началась", sideX + 14.f, 244.f, 14, mutedText);

        const int material = materialScore(state);
        string materialLabel = "Материал: поровну";
        if (material > 0) materialLabel = "Материал: белые +" + to_string(material / 100) + " пеш.";
        else if (material < 0) materialLabel = "Материал: чёрные +" + to_string(-material / 100) + " пеш.";
        drawText(materialLabel, sideX + 12.f, 548.f, 13, mutedText);
        drawPlayerCard(596.f, whiteBelongsToPlayerOne ? whitePlayerLabel : blackPlayerLabel,
            whiteBelongsToPlayerOne ? whiteTime : blackTime, state.white == whiteBelongsToPlayerOne && !gameOver);
        if (promotionPending) {
            sf::RectangleShape panel({ 350,70 });panel.setPosition(640, 565);panel.setFillColor(sf::Color(47, 57, 72));window.draw(panel);
            const char* names[] = { "Q — ферзь","R — ладья","B — слон","N — конь" };
            for (int i = 0;i < 4;i++) { sf::RectangleShape b({ 78,48 });b.setPosition(650 + i * 84, 576);b.setFillColor(sf::Color(75, 95, 125));window.draw(b);drawText(names[i], 653 + i * 84, 590, 12); }
        }
        if (gameOver && !animating && !gameOverPopup.empty()) {
            sf::RectangleShape boardShade({ 8.f * cell, 8.f * cell });
            boardShade.setPosition(boardX, boardY);
            boardShade.setFillColor(sf::Color(4, 7, 11, 155));
            window.draw(boardShade);

            sf::RectangleShape resultPanel({ 540.f, 160.f });
            resultPanel.setPosition(boardX + 42.f, boardY + 232.f);
            resultPanel.setFillColor(sf::Color(18, 22, 29, 250));
            resultPanel.setOutlineThickness(2.f);
            resultPanel.setOutlineColor(sf::Color(190, 151, 78));
            window.draw(resultPanel);

            if (fontLoaded) {
                const string heading = "ПАРТИЯ ОКОНЧЕНА";
                sf::Text headingText(sf::String::fromUtf8(heading.begin(), heading.end()), font, 14);
                headingText.setFillColor(sf::Color(153, 160, 170));
                const sf::FloatRect headingBounds = headingText.getLocalBounds();
                headingText.setOrigin(headingBounds.left + headingBounds.width / 2.f, headingBounds.top + headingBounds.height / 2.f);
                headingText.setPosition(boardX + 312.f, boardY + 271.f);
                window.draw(headingText);

                sf::Text resultText(sf::String::fromUtf8(gameOverPopup.begin(), gameOverPopup.end()), font, 21);
                resultText.setFillColor(sf::Color(245, 246, 239));
                const sf::FloatRect bounds = resultText.getLocalBounds();
                resultText.setOrigin(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
                resultText.setPosition(boardX + 312.f, boardY + 320.f);
                window.draw(resultText);
            }
        }
        }
        window.display();
    }
    return 0;
}
