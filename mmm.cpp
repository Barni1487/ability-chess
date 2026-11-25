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
    for (int r=0;r<8;r++) for(int f=0;f<8;f++) s.b[sq(r,f)] = rows[r][f];
    return s;
}
static int kingSquare(const State& s, bool white) {
    char k = white ? 'K' : 'k';
    for (int i=0;i<64;i++) if (s.b[i] == k) return i;
    return -1;
}
static bool attacked(const State& s, int target, bool byWhite) {
    int tr=rankOf(target), tf=fileOf(target);
    int pawnSourceR = tr + (byWhite ? 1 : -1);
    for (int df : {-1,1}) {
        int f=tf+df;
        if (inside(pawnSourceR,f) && s.b[sq(pawnSourceR,f)] == (byWhite?'P':'p')) return true;
    }
    const int knr[8]={-2,-2,-1,-1,1,1,2,2}, knf[8]={-1,1,-2,2,-2,2,-1,1};
    for(int i=0;i<8;i++) {
        int r=tr+knr[i], f=tf+knf[i];
        if(inside(r,f) && s.b[sq(r,f)]==(byWhite?'N':'n')) return true;
    }
    for(int dr=-1;dr<=1;dr++) for(int df=-1;df<=1;df++) {
        if(!dr&&!df) continue;
        int r=tr+dr,f=tf+df;
        if(inside(r,f) && s.b[sq(r,f)]==(byWhite?'K':'k')) return true;
    }
    const int dirs[8][2]={{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{-1,1},{1,-1},{1,1}};
    for(int d=0;d<8;d++) {
        int r=tr+dirs[d][0], f=tf+dirs[d][1], distance=1;
        while(inside(r,f)) {
            char p=s.b[sq(r,f)];
            if(isPiece(p)) {
                if(isWhite(p)==byWhite) {
                    char t=lowerPiece(p);
                    if(t=='q' || (distance==1 && t=='k') ||
                       (d<4 && t=='r') || (d>=4 && t=='b')) return true;
                }
                break;
            }
            r+=dirs[d][0]; f+=dirs[d][1]; distance++;
        }
    }
    return false;
}
static bool inCheck(const State& s, bool white) {
    int k=kingSquare(s,white);
    return k>=0 && attacked(s,k,!white);
}
static State makeMove(const State& s, const Move& m) {
    State n=s;
    char p=n.b[m.from], captured=n.b[m.to];
    n.b[m.from]='.';
    if(m.enPassant) n.b[m.to+(s.white?8:-8)]='.';
    n.b[m.to]=m.promotion ? (s.white ? char(toupper(m.promotion)) : m.promotion) : p;
    if(m.castle) {
        if(m.to>m.from) {
            int rookFrom=s.white?sq(7,7):sq(0,7);
            int rookTo=m.to-1; n.b[rookTo]=n.b[rookFrom]; n.b[rookFrom]='.';
        } else {
            int rookFrom=s.white?sq(7,0):sq(0,0);
            int rookTo=m.to+1; n.b[rookTo]=n.b[rookFrom]; n.b[rookFrom]='.';
        }
    }
    if(p=='K') n.wk=n.wq=false;
    if(p=='k') n.bk=n.bq=false;
    if(m.from==sq(7,7)||m.to==sq(7,7)) n.wk=false;
    if(m.from==sq(7,0)||m.to==sq(7,0)) n.wq=false;
    if(m.from==sq(0,7)||m.to==sq(0,7)) n.bk=false;
    if(m.from==sq(0,0)||m.to==sq(0,0)) n.bq=false;
    n.ep=-1;
    if(lowerPiece(p)=='p' && abs(rankOf(m.to)-rankOf(m.from))==2)
        n.ep=(m.from+m.to)/2;
    n.half=(lowerPiece(p)=='p'||isPiece(captured)||m.enPassant)?0:s.half+1;
    if(!s.white) n.full++;
    n.white=!s.white;
    return n;
}
static vector<Move> pseudoMoves(const State& s) {
    vector<Move> out;
    auto add=[&](int from,int to,bool ep=false,bool castle=false) {
        char p=s.b[from];
        if(lowerPiece(p)=='p' && (rankOf(to)==0 || rankOf(to)==7)) {
            for(char q : {'q','r','b','n'}) out.push_back({from,to,q,ep,castle});
        } else out.push_back({from,to,0,ep,castle});
    };
    for(int from=0;from<64;from++) {
        char p=s.b[from]; if(!isPiece(p)||isWhite(p)!=s.white) continue;
        int r=rankOf(from), f=fileOf(from); char t=lowerPiece(p);
        if(t=='p') {
            int dir=s.white?-1:1, start=s.white?6:1, nr=r+dir;
            if(inside(nr,f)&&s.b[sq(nr,f)]=='.') {
                add(from,sq(nr,f));
                if(r==start && s.b[sq(r+2*dir,f)]=='.') add(from,sq(r+2*dir,f));
            }
            for(int df : {-1,1}) {
                int nf=f+df;
                if(!inside(nr,nf)) continue;
                int to=sq(nr,nf);
                if(isPiece(s.b[to])&&isWhite(s.b[to])!=s.white) add(from,to);
                else if(to==s.ep) add(from,to,true);
            }
        } else if(t=='n') {
            const int dr[8]={-2,-2,-1,-1,1,1,2,2}, df[8]={-1,1,-2,2,-2,2,-1,1};
            for(int i=0;i<8;i++) { int nr=r+dr[i],nf=f+df[i]; if(inside(nr,nf)) {int to=sq(nr,nf);if(!isPiece(s.b[to])||isWhite(s.b[to])!=s.white)add(from,to);} }
        } else {
            vector<pair<int,int>> ds;
            if(t=='b'||t=='q') for(int dr:{-1,1}) for(int df:{-1,1}) ds.push_back({dr,df});
            if(t=='r'||t=='q') {ds.push_back({-1,0});ds.push_back({1,0});ds.push_back({0,-1});ds.push_back({0,1});}
            if(t=='k') for(int dr=-1;dr<=1;dr++) for(int df=-1;df<=1;df++) if(dr||df) ds.push_back({dr,df});
            for(auto [dr,df]:ds) {
                int nr=r+dr,nf=f+df;
                while(inside(nr,nf)) {
                    int to=sq(nr,nf);
                    if(!isPiece(s.b[to])) add(from,to);
                    else {if(isWhite(s.b[to])!=s.white)add(from,to);break;}
                    if(t=='k') break;
                    nr+=dr;nf+=df;
                }
            }
            if(t=='k' && !inCheck(s,s.white)) {
                if(s.white && from==sq(7,4)) {
                    if(s.wk && s.b[sq(7,5)]=='.'&&s.b[sq(7,6)]=='.'&&s.b[sq(7,7)]=='R'&&
                       !attacked(s,sq(7,5),false)&&!attacked(s,sq(7,6),false)) add(from,sq(7,6),false,true);
                    if(s.wq && s.b[sq(7,1)]=='.'&&s.b[sq(7,2)]=='.'&&s.b[sq(7,3)]=='.'&&s.b[sq(7,0)]=='R'&&
                       !attacked(s,sq(7,3),false)&&!attacked(s,sq(7,2),false)) add(from,sq(7,2),false,true);
                }
                if(!s.white && from==sq(0,4)) {
                    if(s.bk && s.b[sq(0,5)]=='.'&&s.b[sq(0,6)]=='.'&&s.b[sq(0,7)]=='r'&&
                       !attacked(s,sq(0,5),true)&&!attacked(s,sq(0,6),true)) add(from,sq(0,6),false,true);
                    if(s.bq && s.b[sq(0,1)]=='.'&&s.b[sq(0,2)]=='.'&&s.b[sq(0,3)]=='.'&&s.b[sq(0,0)]=='r'&&
                       !attacked(s,sq(0,3),true)&&!attacked(s,sq(0,2),true)) add(from,sq(0,2),false,true);
                }
            }
        }
    }
    return out;
}
static vector<Move> legalMoves(const State& s) {
    vector<Move> result;
    for(const Move& m:pseudoMoves(s)) {
        State n=makeMove(s,m);
        if(!inCheck(n,s.white)) result.push_back(m);
    }
    return result;
}
static string moveText(const Move& m) {
    string x=squareName(m.from)+"-"+squareName(m.to);
    if(m.promotion) { x+="="; x+=char(toupper(m.promotion)); }
    return x;
}
static int materialScore(const State& s) {
    int score=0;
    for(char p:s.b) if(isPiece(p)) score+=(isWhite(p)?1:-1)*value(p);
    return score;
}
static int evaluate(const State& s) {
    int score=materialScore(s);
    // Небольшой бонус за развитие и центральные поля.
    for(int i=0;i<64;i++) if(isPiece(s.b[i])) {
        char p=lowerPiece(s.b[i]); int center=3-(abs(3-fileOf(i))+abs(3-rankOf(i)))/2;
        if(p=='n'||p=='b') score+=(isWhite(s.b[i])?1:-1)*center*5;
    }
    return s.white?score:-score;
}
static int minimax(const State& s,int depth,int alpha,int beta) {
    auto moves=legalMoves(s);
    if(moves.empty()) return inCheck(s,s.white)?-100000-depth:0;
    if(depth==0) return evaluate(s);
    int best=-1000000;
    for(const Move& m:moves) {
        int score=-minimax(makeMove(s,m),depth-1,-beta,-alpha);
        best=max(best,score); alpha=max(alpha,score);
        if(alpha>=beta) break;
    }
    return best;
}
static Move chooseComputerMove(const State& s) {
    auto moves=legalMoves(s); Move best=moves.front(); int bestScore=-1000000;
    for(const Move& m:moves) {
        int score=-minimax(makeMove(s,m),2,-1000000,1000000);
        if(score>bestScore) {bestScore=score;best=m;}
    }
    return best;
}
static string pieceName(char p) {
    switch(lowerPiece(p)) {
        case 'p': return "пешка"; case 'n': return "конь"; case 'b': return "слон";
        case 'r': return "ладья"; case 'q': return "ферзь"; case 'k': return "король";
    }
    return "";
}

int main() {
    sf::RenderWindow window(sf::VideoMode(1040,760),"ABILITY CHESS — шахматы",sf::Style::Titlebar|sf::Style::Close);
    window.setFramerateLimit(60);
    sf::Font font;
    vector<string> fonts={
#ifdef _WIN32
        "C:/Windows/Fonts/arial.ttf","C:/Windows/Fonts/segoeui.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
#endif
        "arial.ttf","DejaVuSans.ttf"
    };
    bool fontLoaded=false; for(const auto& f:fonts) if(font.loadFromFile(f)){fontLoaded=true;break;}
    if(!fontLoaded) cerr<<"Не найден шрифт. Установите DejaVu Sans или положите arial.ttf рядом с программой.\n";

    const float boardX=28, boardY=72, cell=78;
    State state=initialState();
    vector<State> states{state};
    vector<string> history;
    vector<Move> selectedMoves;
    int selected=-1;
    bool vsComputer=false, gameOver=false, promotionPending=false;
    vector<Move> promotionMoves;
    string message="Белые ходят. Выберите фигуру мышью.";
    float whiteTime=300.f, blackTime=300.f;
    const float increment=3.f;
    auto last=chrono::steady_clock::now();
    bool animating=false; Move animMove; float animProgress=0.f; char animPiece='.';
    bool aiThinking=false;

    auto reset=[&](){
        state=initialState();states.assign(1,state);history.clear();selected=-1;selectedMoves.clear();
        gameOver=false;promotionPending=false;promotionMoves.clear();whiteTime=blackTime=300.f;
        message="Новая партия. Белые ходят.";animating=false;
    };
    auto finishStatus=[&](){
        auto moves=legalMoves(state);
        if(moves.empty()) {
            gameOver=true;
            if(inCheck(state,state.white)) message=string("МАТ! Победили ")+(state.white?"чёрные.":"белые.");
            else message="ПАТ! Ничья.";
        } else if(state.half>=100) { gameOver=true; message="Ничья по правилу 50 ходов."; }
    };
    auto commit=[&](Move m){
        char moving=state.b[m.from];
        State next=makeMove(state,m);
        string notation=(state.white?"Белые: ":"Чёрные: ")+moveText(m);
        if(m.promotion) notation+=" (превращение в "+pieceName(m.promotion)+")";
        history.push_back(notation);
        state=next;states.push_back(state);
        if(moving=='P'||moving=='p'||isPiece(states[states.size()-2].b[m.to])) {
            // increment is awarded after every completed move, independently of capture.
        }
        if(state.white) blackTime+=increment; else whiteTime+=increment;
        selected=-1;selectedMoves.clear();promotionPending=false;promotionMoves.clear();
        animating=true;animMove=m;animPiece=m.promotion?(state.white?char(toupper(m.promotion)):m.promotion):moving;animProgress=0;
        finishStatus();
        if(!gameOver) message=string(state.white?"Ход белых.":"Ход чёрных.")+(inCheck(state,state.white)?" ШАХ!":"");
    };

    while(window.isOpen()) {
        auto now=chrono::steady_clock::now();
        float dt=chrono::duration<float>(now-last).count(); last=now;
        if(!gameOver && !promotionPending && !animating) {
            if(state.white) whiteTime=max(0.f,whiteTime-dt); else blackTime=max(0.f,blackTime-dt);
            if(whiteTime<=0||blackTime<=0) {gameOver=true;message=whiteTime<=0?"Время белых истекло. Победили чёрные.":"Время чёрных истекло. Победили белые.";}
        }
        if(animating) { animProgress+=dt*5.f; if(animProgress>=1.f) animating=false; }
        if(vsComputer && !state.white && !gameOver && !promotionPending && !animating && !aiThinking) {
            aiThinking=true; Move m=chooseComputerMove(state); commit(m); aiThinking=false;
        }

        sf::Event ev;
        while(window.pollEvent(ev)) {
            if(ev.type==sf::Event::Closed) window.close();
            if(ev.type==sf::Event::MouseButtonPressed && ev.mouseButton.button==sf::Mouse::Left) {
                float mx=ev.mouseButton.x,my=ev.mouseButton.y;
                // Верхние кнопки режима и перезапуска.
                if(mx>=760&&mx<=1005&&my>=22&&my<=60) {
                    if(mx<880) {vsComputer=!vsComputer;reset();message=vsComputer?"Режим: против компьютера.":"Режим: два игрока.";}
                    else reset();
                    continue;
                }
                if(promotionPending) {
                    for(size_t i=0;i<promotionMoves.size();i++) {
                        float px=650+(i%4)*82, py=580;
                        if(mx>=px&&mx<px+70&&my>=py&&my<py+58) {commit(promotionMoves[i]);break;}
                    }
                    continue;
                }
                if(gameOver||animating||(vsComputer&&!state.white)) continue;
                int f=int((mx-boardX)/cell), r=int((my-boardY)/cell);
                if(mx>=boardX&&mx<boardX+8*cell&&my>=boardY&&my<boardY+8*cell) {
                    int target=sq(r,f);
                    if(selected<0) {
                        if(isPiece(state.b[target])&&isWhite(state.b[target])==state.white) {
                            selected=target;selectedMoves.clear();
                            for(auto m:legalMoves(state)) if(m.from==selected) selectedMoves.push_back(m);
                        }
                    } else {
                        vector<Move> choices;
                        for(auto m:selectedMoves) if(m.to==target) choices.push_back(m);
                        if(!choices.empty()) {
                            if(choices.size()>1) {promotionPending=true;promotionMoves=choices;message="Выберите фигуру превращения:";}
                            else commit(choices[0]);
                        } else if(isPiece(state.b[target])&&isWhite(state.b[target])==state.white) {
                            selected=target;selectedMoves.clear();
                            for(auto m:legalMoves(state)) if(m.from==selected) selectedMoves.push_back(m);
                        } else {selected=-1;selectedMoves.clear();}
                    }
                }
            }
        }

        window.clear(sf::Color(24,29,39));
        auto drawText=[&](const string& str,float x,float y,unsigned size,sf::Color color=sf::Color::White) {
            if(!fontLoaded) return;
            sf::Text t(str,font,size);t.setFillColor(color);t.setPosition(x,y);window.draw(t);
        };
        drawText("ABILITY CHESS",28,18,27,sf::Color(238,198,111));
        // buttons
        sf::RectangleShape modeBtn({112,38});modeBtn.setPosition(760,20);modeBtn.setFillColor(sf::Color(65,91,125));window.draw(modeBtn);
        drawText(vsComputer?"Против ИИ":"Два игрока",768,27,15);
        sf::RectangleShape resetBtn({112,38});resetBtn.setPosition(885,20);resetBtn.setFillColor(sf::Color(65,91,125));window.draw(resetBtn);
        drawText("Заново",907,27,16);

        // Board and highlights
        for(int r=0;r<8;r++) for(int f=0;f<8;f++) {
            int s=sq(r,f); sf::RectangleShape tile({cell,cell});tile.setPosition(boardX+f*cell,boardY+r*cell);
            bool light=(r+f)%2==0;tile.setFillColor(light?sf::Color(238,216,181):sf::Color(181,136,99));window.draw(tile);
            if(s==selected) {tile.setFillColor(sf::Color(235,205,75,190));window.draw(tile);}
            for(auto m:selectedMoves) if(m.to==s) {
                sf::CircleShape dot(isPiece(state.b[s])?27.f:10.f);
                dot.setOrigin(dot.getRadius(),dot.getRadius());dot.setPosition(boardX+f*cell+cell/2,boardY+r*cell+cell/2);
                dot.setFillColor(isPiece(state.b[s])?sf::Color(180,45,45,125):sf::Color(40,100,55,150));window.draw(dot);
            }
            if(fontLoaded && isPiece(state.b[s]) && !(animating&&s==animMove.to)) {
                string label(1,state.b[s]);sf::Text piece(label,font,48);
                bool white=isWhite(state.b[s]);piece.setFillColor(white?sf::Color(250,248,238):sf::Color(25,28,35));
                piece.setOutlineColor(white?sf::Color(35,39,48):sf::Color(235,230,215));piece.setOutlineThickness(2);
                auto bounds=piece.getLocalBounds();piece.setOrigin(bounds.left+bounds.width/2,bounds.top+bounds.height/2);
                piece.setPosition(boardX+f*cell+cell/2,boardY+r*cell+cell/2-2);window.draw(piece);
            }
            if(f==0) drawText(to_string(8-r),boardX+3,boardY+r*cell+3,12,sf::Color(55,55,55));
            if(r==7) drawText(string(1,char('a'+f)),boardX+f*cell+cell-15,boardY+7*cell+cell-20,12,sf::Color(55,55,55));
        }
        if(animating&&fontLoaded) {
            float x1=boardX+fileOf(animMove.from)*cell+cell/2, y1=boardY+rankOf(animMove.from)*cell+cell/2;
            float x2=boardX+fileOf(animMove.to)*cell+cell/2, y2=boardY+rankOf(animMove.to)*cell+cell/2;
            sf::CircleShape moving(25);moving.setOrigin(25,25);moving.setPosition(x1+(x2-x1)*animProgress,y1+(y2-y1)*animProgress);
            moving.setFillColor(isWhite(animPiece)?sf::Color(250,248,238):sf::Color(25,28,35));
            moving.setOutlineColor(isWhite(animPiece)?sf::Color(35,39,48):sf::Color(235,230,215));moving.setOutlineThickness(2);window.draw(moving);
            sf::Text t(string(1,animPiece),font,42);t.setFillColor(moving.getFillColor());t.setOutlineColor(moving.getOutlineColor());t.setOutlineThickness(2);
            t.setPosition(moving.getPosition().x-12,moving.getPosition().y-26);window.draw(t);
        }

        drawText("БЕЛЫЕ",680,90,17,sf::Color(230,230,230));
        auto clockText=[](float t){int sec=max(0,int(t));ostringstream o;o<<setfill('0')<<setw(2)<<sec/60<<":"<<setw(2)<<sec%60;return o.str();};
        drawText(clockText(whiteTime),680,116,31,state.white?sf::Color(255,218,120):sf::Color::White);
        drawText("ЧЁРНЫЕ",680,174,17,sf::Color(230,230,230));
        drawText(clockText(blackTime),680,200,31,!state.white?sf::Color(255,218,120):sf::Color::White);
        drawText("Контроль времени: 5 + 3",680,250,15,sf::Color(180,190,205));
        drawText("Оценка материала",680,294,18,sf::Color(238,198,111));
        int mat=materialScore(state);
        drawText(string("Белые: ")+(mat>=0?"+":"")+to_string(mat/100)+" пеш.",680,322,15);
        drawText(string("Чёрные: ")+(mat<=0?"+":"")+to_string(-mat/100)+" пеш.",680,346,15);
        drawText("Фигуры: P пешка, N конь, B слон",680,390,13,sf::Color(190,198,210));
        drawText("R ладья, Q ферзь, K король",680,410,13,sf::Color(190,198,210));
        drawText("История ходов",680,450,19,sf::Color(238,198,111));
        for(size_t i=0;i<history.size()&&i<9;i++) drawText(to_string(i+1)+". "+history[i],680,480+22*i,13);
        drawText(message,28,710,16,gameOver?sf::Color(255,130,130):sf::Color(230,230,230));
        if(promotionPending) {
            sf::RectangleShape panel({350,70});panel.setPosition(640,565);panel.setFillColor(sf::Color(47,57,72));window.draw(panel);
            const char* names[]={"Q — ферзь","R — ладья","B — слон","N — конь"};
            for(int i=0;i<4;i++){sf::RectangleShape b({78,48});b.setPosition(650+i*84,576);b.setFillColor(sf::Color(75,95,125));window.draw(b);drawText(names[i],653+i*84,590,12);}
        }
        window.display();
    }
    return 0;
}
