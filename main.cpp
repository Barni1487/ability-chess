#include <iostream>
#include <string>
#include <cctype>
#include <cstdlib>

using namespace std;

// Шахматная доска.
// Большие буквы — белые фигуры, маленькие — чёрные.
// Пробел означает, что клетка пустая.
char board[8][8] = {
    {'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'},
    {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'},
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
    {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'},
    {'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'}
};

// Переменная показывает, кто сейчас ходит.
// true — белые, false — чёрные.
bool whiteTurn = true;


// Проверяем, что координаты клетки находятся внутри доски.
bool isInside(int row, int col) {
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}


// Проверяем, является ли фигура белой.
bool isWhite(char piece) {
    return piece >= 'A' && piece <= 'Z';
}


// Проверяем, стоит ли на клетке фигура противника.
bool isEnemy(char piece, bool movingWhite) {
    if (piece == ' ') {
        return false;
    }

    return isWhite(piece) != movingWhite;
}


// Проверяем, свободны ли клетки между началом и концом хода.
// Эта проверка нужна для ладьи, слона и ферзя.
bool isPathClear(int fromRow, int fromCol, int toRow, int toCol) {
    int rowStep = 0;
    int colStep = 0;

    if (toRow > fromRow) rowStep = 1;
    if (toRow < fromRow) rowStep = -1;
    if (toCol > fromCol) colStep = 1;
    if (toCol < fromCol) colStep = -1;

    int row = fromRow + rowStep;
    int col = fromCol + colStep;

    while (row != toRow || col != toCol) {
        if (board[row][col] != ' ') {
            return false;
        }

        row += rowStep;
        col += colStep;
    }

    return true;
}


// Проверяем, может ли выбранная фигура сделать указанный ход.
bool canMove(int fromRow, int fromCol, int toRow, int toCol) {
    if (!isInside(fromRow, fromCol) || !isInside(toRow, toCol)) {
        return false;
    }

    if (fromRow == toRow && fromCol == toCol) {
        return false;
    }

    char piece = board[fromRow][fromCol];
    char target = board[toRow][toCol];

    if (piece == ' ') {
        return false;
    }

    // Нельзя ходить фигурой другого цвета.
    if (isWhite(piece) != whiteTurn) {
        return false;
    }

    // Нельзя вставать на клетку со своей фигурой.
    if (target != ' ' && !isEnemy(target, whiteTurn)) {
        return false;
    }

    int rowDiff = toRow - fromRow;
    int colDiff = toCol - fromCol;
    int absRow = abs(rowDiff);
    int absCol = abs(colDiff);

    char type = static_cast<char>(tolower(piece));

    // Проверяем правила хода для каждого вида фигуры.
    switch (type) {
    case 'p': {
        int direction;
        int startRow;

        if (whiteTurn) {
            direction = -1;
            startRow = 6;
        } else {
            direction = 1;
            startRow = 1;
        }

        // Пешка может идти на одну клетку вперёд.
        if (colDiff == 0 && target == ' ' && rowDiff == direction) {
            return true;
        }

        // С начальной позиции пешка может пройти две клетки.
        if (colDiff == 0 && target == ' ' &&
            fromRow == startRow && rowDiff == 2 * direction &&
            board[fromRow + direction][fromCol] == ' ') {
            return true;
        }

        // Пешка берёт фигуру по диагонали.
        if (absCol == 1 && rowDiff == direction &&
            isEnemy(target, whiteTurn)) {
            return true;
        }

        return false;
    }

    case 'r': // Ладья ходит по прямой.
        return (rowDiff == 0 || colDiff == 0) &&
               isPathClear(fromRow, fromCol, toRow, toCol);

    case 'b': // Слон ходит по диагонали.
        return absRow == absCol &&
               isPathClear(fromRow, fromCol, toRow, toCol);

    case 'q': // Ферзь ходит как ладья или слон.
        return (rowDiff == 0 || colDiff == 0 || absRow == absCol) &&
               isPathClear(fromRow, fromCol, toRow, toCol);

    case 'n': // Конь ходит буквой «Г» и может перепрыгивать фигуры.
        return (absRow == 2 && absCol == 1) ||
               (absRow == 1 && absCol == 2);

    case 'k': // Король пока ходит на одну клетку в любую сторону.
        return absRow <= 1 && absCol <= 1;

    default:
        return false;
    }
}


// Выводим шахматную доску в консоль.
void printBoard() {
    cout << "\n    a   b   c   d   e   f   g   h\n";
    cout << "  +---+---+---+---+---+---+---+---+\n";

    for (int row = 0; row < 8; row++) {
        cout << 8 - row << " |";

        for (int col = 0; col < 8; col++) {
            cout << " " << board[row][col] << " |";
        }

        cout << " " << 8 - row << "\n";
        cout << "  +---+---+---+---+---+---+---+---+\n";
    }

    cout << "    a   b   c   d   e   f   g   h\n\n";
}


// Переводим запись клетки, например e2, в номера строки и столбца.
bool readPosition(string position, int& row, int& col) {
    if (position.length() != 2) {
        return false;
    }

    char letter = static_cast<char>(
        tolower(static_cast<unsigned char>(position[0]))
    );
    char number = position[1];

    if (letter < 'a' || letter > 'h' || number < '1' || number > '8') {
        return false;
    }

    col = letter - 'a';
    row = 8 - (number - '0');

    return true;
}


// Главная функция: запускает игру и принимает ходы игроков.
int main() {
    cout << "=============================\n";
    cout << "        ABILITY CHESS\n";
    cout << "          Version 0.1\n";
    cout << "=============================\n";
    cout << "Введите ход, например: e2 e4\n";
    cout << "Чтобы закончить игру, введите q.\n";
    cout << "В этой версии пока нет проверки шаха и мата.\n";

    string from;
    string to;

    while (true) {
        printBoard();

        if (whiteTurn) {
            cout << "Ход белых: ";
        } else {
            cout << "Ход чёрных: ";
        }

        cin >> from;

        if (from == "q" || from == "Q") {
            cout << "Игра завершена.\n";
            break;
        }

        cin >> to;

        int fromRow, fromCol;
        int toRow, toCol;

        // Проверяем, правильно ли игрок ввёл координаты.
        if (!readPosition(from, fromRow, fromCol) ||
            !readPosition(to, toRow, toCol)) {
            cout << "Ошибка! Введите координаты, например e2 e4.\n";
            continue;
        }

        // Если ход не подходит под правила, просим попробовать ещё раз.
        if (!canMove(fromRow, fromCol, toRow, toCol)) {
            cout << "Такой ход сделать нельзя. Попробуйте ещё раз.\n";
            continue;
        }

        // Перемещаем фигуру на новую клетку.
        char piece = board[fromRow][fromCol];
        board[toRow][toCol] = piece;
        board[fromRow][fromCol] = ' ';

        // Если пешка дошла до края доски, превращаем её в ферзя.
        if (piece == 'P' && toRow == 0) {
            board[toRow][toCol] = 'Q';
            cout << "Белая пешка превратилась в ферзя!\n";
        }

        if (piece == 'p' && toRow == 7) {
            board[toRow][toCol] = 'q';
            cout << "Чёрная пешка превратилась в ферзя!\n";
        }

        // Передаём ход другому игроку.
        whiteTurn = !whiteTurn;
    }

    return 0;
}
