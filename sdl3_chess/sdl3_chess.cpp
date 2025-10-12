#include <array>
#include <vector>
#include <memory>
#include <iostream>
#include <random>
#include <algorithm>
#include <string>
#include <fstream>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h> // _getcwd
#define popen _popen
#define pclose _pclose
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#define getcwd _getcwd
#endif
#else
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>d
#endif
#include <SDL3/SDL_main.h>

using namespace std;

const string PROJECT_ROOT = "..\\..\\..\\..\\";
//const string PROJECT_ROOT = "";
const string ASSERTS_DIR =  PROJECT_ROOT + "assets\\";
const string STOCKFISH_PATH = PROJECT_ROOT +  "stockfish\\stockfish.exe";

enum class PieceType
{
    KING, QUEEN, ROOK, KNIGHT, BISHOP, PAWN, NONE
};

enum class PieceColor
{
    WHITE, BLACK, NONE
};

using Piece = pair<PieceType, PieceColor>;
using Square = pair<int, int>; // (x, y)

const int BOARD_SIZE = 8;

//------------------------------------------------------------------------------
// Game State
//------------------------------------------------------------------------------
class GameState
{
public:
    static inline const Square INVALID_SQUARE = {-1, -1};

    void initializeBoard();
    Piece& pieceAt(Square sq) { return _board[sq.first][sq.second]; }
    const Piece& pieceAt(Square sq) const { return _board[sq.first][sq.second]; }

    void selectSquare(Square sq) { _selectedSquare = sq; }
    void movePiece(Square from, Square to);
    Square getSelectedSquare() const { return _selectedSquare; }
    void clearSelection() { _selectedSquare = INVALID_SQUARE; }
    PieceColor currentTurn() const { return _turn; }
    void setTurn(PieceColor color) { _turn = color; }
    void switchTurn() { _turn = (_turn == PieceColor::WHITE) ? PieceColor::BLACK : PieceColor::WHITE; }
    vector<Square>& possibleMoves() { return _possibleMoves; }
    const vector<Square>& possibleMoves() const { return _possibleMoves; }
    string generateFen();

private:
    array<array<Piece, BOARD_SIZE>, BOARD_SIZE> _board;
    Square _selectedSquare = INVALID_SQUARE;
    PieceColor _turn = PieceColor::WHITE;
    vector<Square> _possibleMoves;
};

string GameState::generateFen()
{
    string fen;
    for (int y = BOARD_SIZE - 1; y >= 0; --y) {
        int empty = 0;
        for (int x = 0; x < BOARD_SIZE; ++x) {
            const Piece& piece = _board[x][y];
            if (piece.first == PieceType::NONE) {
                ++empty;
            } else {
                if (empty > 0) {
                    fen += to_string(empty);
                    empty = 0;
                }
                char c = ' ';
                switch (piece.first) {
                    case PieceType::KING:   c = 'k'; break;
                    case PieceType::QUEEN:  c = 'q'; break;
                    case PieceType::ROOK:   c = 'r'; break;
                    case PieceType::BISHOP: c = 'b'; break;
                    case PieceType::KNIGHT: c = 'n'; break;
                    case PieceType::PAWN:   c = 'p'; break;
                    default: break;
                }
                if (piece.second == PieceColor::WHITE)
                    c = toupper(c);
                fen += c;
            }
        }
        if (empty > 0) fen += to_string(empty);
        if (y > 0) fen += '/';
    }

    // Turn
    fen += ' ';
    fen += (_turn == PieceColor::WHITE) ? 'w' : 'b';

    // Castling rights - check if kings and rooks are in their initial squares
    string castling;
    // White king-side
    if (pieceAt({4,0}).first == PieceType::KING && pieceAt({4,0}).second == PieceColor::WHITE &&
        pieceAt({7,0}).first == PieceType::ROOK && pieceAt({7,0}).second == PieceColor::WHITE)
        castling += 'K';
    // White queen-side
    if (pieceAt({4,0}).first == PieceType::KING && pieceAt({4,0}).second == PieceColor::WHITE &&
        pieceAt({0,0}).first == PieceType::ROOK && pieceAt({0,0}).second == PieceColor::WHITE)
        castling += 'Q';
    // Black king-side
    if (pieceAt({4,7}).first == PieceType::KING && pieceAt({4,7}).second == PieceColor::BLACK &&
        pieceAt({7,7}).first == PieceType::ROOK && pieceAt({7,7}).second == PieceColor::BLACK)
        castling += 'k';
    // Black queen-side
    if (pieceAt({4,7}).first == PieceType::KING && pieceAt({4,7}).second == PieceColor::BLACK &&
        pieceAt({0,7}).first == PieceType::ROOK && pieceAt({0,7}).second == PieceColor::BLACK)
        castling += 'q';
    if (castling.empty()) castling = "-";
    fen += " " + castling;

    // En passant (not tracked, so always "-")
    fen += " -";

    // Halfmove clock and fullmove number (not tracked, so always "0 1")
    fen += " 0 1";

    return fen;
}

void GameState::initializeBoard()
{
    fill(&_board[0][0], &_board[0][0] + BOARD_SIZE * BOARD_SIZE, Piece{PieceType::NONE, PieceColor::NONE});
    for (int x = 0; x < BOARD_SIZE; ++x)
    {
        _board[x][1] = {PieceType::PAWN, PieceColor::WHITE};
        _board[x][6] = {PieceType::PAWN, PieceColor::BLACK};
    }

    _board[0][0] = {PieceType::ROOK, PieceColor::WHITE};
    _board[1][0] = {PieceType::KNIGHT, PieceColor::WHITE};
    _board[2][0] = {PieceType::BISHOP, PieceColor::WHITE};
    _board[3][0] = {PieceType::QUEEN, PieceColor::WHITE};
    _board[4][0] = {PieceType::KING, PieceColor::WHITE};
    _board[5][0] = {PieceType::BISHOP, PieceColor::WHITE};
    _board[6][0] = {PieceType::KNIGHT, PieceColor::WHITE};
    _board[7][0] = {PieceType::ROOK, PieceColor::WHITE};

    _board[0][7] = {PieceType::ROOK, PieceColor::BLACK};
    _board[1][7] = {PieceType::KNIGHT, PieceColor::BLACK};
    _board[2][7] = {PieceType::BISHOP, PieceColor::BLACK};
    _board[3][7] = {PieceType::QUEEN, PieceColor::BLACK};
    _board[4][7] = {PieceType::KING, PieceColor::BLACK};
    _board[5][7] = {PieceType::BISHOP, PieceColor::BLACK};
    _board[6][7] = {PieceType::KNIGHT, PieceColor::BLACK};
    _board[7][7] = {PieceType::ROOK, PieceColor::BLACK};
}

void GameState::movePiece(Square from, Square to)
{
    if (from != INVALID_SQUARE && to != INVALID_SQUARE)
    {
        _board[to.first][to.second] = _board[from.first][from.second];
        _board[from.first][from.second] = {PieceType::NONE, PieceColor::NONE};
    }
}

//------------------------------------------------------------------------------
// Game Logic
//------------------------------------------------------------------------------
class Game
{
    GameState _state;
public:
    enum class GameResult
    {
        ONGOING, WHITE_WIN, BLACK_WIN, DRAW
    };
    GameState& gameState() { return _state; }
    const GameState& gameState() const { return _state; }
    
    void start();
    vector<Square> getPossibleMoveCandidates() const;
    vector<Square> getPossibleMoves() const;
    vector<Square> getAllPossibleMoves() const;
    bool isValidMove(Square to) const;
    bool select(Square sq);
    void unselect() { _state.clearSelection(); _state.possibleMoves().clear(); }
    bool move(Square to);
    GameResult isGameOver() const;
    static bool isCheck(const Game& game, PieceColor color);
    static bool isValidState(const Game& game);
    string gameResultToString(GameResult result) const {
        switch (result) {
            case GameResult::ONGOING: return "Ongoing";
            case GameResult::WHITE_WIN: return "White wins";
            case GameResult::BLACK_WIN: return "Black wins";
            case GameResult::DRAW: return "Draw";
            default: return "Unknown";
        }
    }
};

void Game::start()
{
    _state.initializeBoard();
    _state.clearSelection();
    _state.possibleMoves().clear();
    _state.setTurn(PieceColor::WHITE);
}

vector<Square> Game::getAllPossibleMoves() const
{
    Game tempGame = *this;
    vector<Square> allMoves;
    for (int x = 0; x < BOARD_SIZE; ++x)
    {
        for (int y = 0; y < BOARD_SIZE; ++y)
        {
            Piece piece = tempGame.gameState().pieceAt({x, y});
            if (piece.first != PieceType::NONE && piece.second == tempGame.gameState().currentTurn())
            {
                tempGame.select({x, y});
                auto moves = tempGame.getPossibleMoves();
                allMoves.insert(allMoves.end(), moves.begin(), moves.end());
                tempGame.unselect();
            }
        }
    }
    return allMoves;
}

Game::GameResult Game::isGameOver() const 
{ 
    if (getAllPossibleMoves().empty())
    {
        if (isCheck(*this, _state.currentTurn()))
        {
            // Checkmate
            return (_state.currentTurn() == PieceColor::WHITE) ? GameResult::BLACK_WIN : GameResult::WHITE_WIN;
        }
        else
        {
            // Stalemate
            return GameResult::DRAW;
        }
    }
    else
    {
        return GameResult::ONGOING;
    }
}

bool Game::isCheck(const Game& game, PieceColor color)
{
    Square kingPos = GameState::INVALID_SQUARE;
    for (int x = 0; x < BOARD_SIZE; ++x)
    {
        for (int y = 0; y < BOARD_SIZE; ++y)
        {
            Piece piece = game.gameState().pieceAt({x, y});
            if (piece.first == PieceType::KING && piece.second == color)
            {
                kingPos = {x, y};
                break;
            }
        }
        if (kingPos != GameState::INVALID_SQUARE) break;
    }
    if (kingPos == GameState::INVALID_SQUARE) return false;

    for (int x = 0; x < BOARD_SIZE; ++x)
    {
        for (int y = 0; y < BOARD_SIZE; ++y)
        {
            Piece piece = game.gameState().pieceAt({x, y});
            if (piece.first != PieceType::NONE && piece.second != color)
            {
                Game tempGame = game;
                tempGame.gameState().selectSquare({x, y});
                tempGame.gameState().setTurn(piece.second);
                auto moves = tempGame.getPossibleMoveCandidates();
                if (ranges::find(moves, kingPos) != moves.end())
                {
                    return true;
                }
            }
        }
    }

    return false;
}

bool Game::isValidState(const Game& game)
{
    if (game.gameState().currentTurn() != PieceColor::WHITE && game.gameState().currentTurn() != PieceColor::BLACK)
    {
        return false;
    }

    if (isCheck(game, PieceColor::WHITE) && game.gameState().currentTurn() == PieceColor::BLACK)
    {
        return false;
    }

    if (isCheck(game, PieceColor::BLACK) && game.gameState().currentTurn() == PieceColor::WHITE)
    {
        return false;
    }

    return true;
}

vector<Square> Game::getPossibleMoveCandidates() const
{
    vector<Square> possibleMoves;
    if (_state.getSelectedSquare() != GameState::INVALID_SQUARE)
    {
        Piece piece = _state.pieceAt(_state.getSelectedSquare());
        if (piece.first != PieceType::NONE && piece.second == _state.currentTurn())
        {
            if (piece.first == PieceType::PAWN)
            {
                int direction = (piece.second == PieceColor::WHITE) ? 1 : -1;
                Square front = {_state.getSelectedSquare().first, _state.getSelectedSquare().second + direction};
                if (front.second >= 0 && front.second < BOARD_SIZE && _state.pieceAt(front).first == PieceType::NONE)
                {
                    possibleMoves.push_back(front);
                }
                Square doubleFront = {_state.getSelectedSquare().first, _state.getSelectedSquare().second + 2 * direction};
                if ((piece.second == PieceColor::WHITE && _state.getSelectedSquare().second == 1) ||
                    (piece.second == PieceColor::BLACK && _state.getSelectedSquare().second == 6))
                {
                    if (doubleFront.second >= 0 && doubleFront.second < BOARD_SIZE && _state.pieceAt(doubleFront).first == PieceType::NONE && _state.pieceAt(front).first == PieceType::NONE)
                    {
                        possibleMoves.push_back(doubleFront);
                    }
                }

                // Capture moves
                Square captureLeft = {_state.getSelectedSquare().first - 1, _state.getSelectedSquare().second + direction};
                Square captureRight = {_state.getSelectedSquare().first + 1, _state.getSelectedSquare().second + direction};
                if (captureLeft.first >= 0 && captureLeft.first < BOARD_SIZE && captureLeft.second >= 0 && captureLeft.second < BOARD_SIZE)
                {
                    Piece target = _state.pieceAt(captureLeft);
                    if (target.first != PieceType::NONE && target.second != piece.second)
                    {
                        possibleMoves.push_back(captureLeft);
                    }
                }
                if (captureRight.first >= 0 && captureRight.first < BOARD_SIZE && captureRight.second >= 0 && captureRight.second < BOARD_SIZE)
                {
                    Piece target = _state.pieceAt(captureRight);
                    if (target.first != PieceType::NONE && target.second != piece.second)
                    {
                        possibleMoves.push_back(captureRight);
                    }
                }
            }
            else if (piece.first == PieceType::KNIGHT)
            {
                vector<Square> knightMoves = {
                    {1, 2}, {2, 1}, {2, -1}, {1, -2},
                    {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}
                };
                for (const auto& move : knightMoves)
                {
                    Square target = {_state.getSelectedSquare().first + move.first, _state.getSelectedSquare().second + move.second};
                    if (target.first >= 0 && target.first < BOARD_SIZE && target.second >= 0 && target.second < BOARD_SIZE)
                    {
                        Piece targetPiece = _state.pieceAt(target);
                        if (targetPiece.first == PieceType::NONE || targetPiece.second != piece.second)
                        {
                            possibleMoves.push_back(target);
                        }
                    }
                }
            }
            else if (piece.first == PieceType::KING)
            {
                vector<Square> kingMoves = {
                    {1, 1}, {1, 0}, {1, -1},
                    {0, -1}, {-1, -1}, {-1, 0},
                    {-1, 1}, {0, 1}
                };
                for (const auto& move : kingMoves)
                {
                    Square target = {_state.getSelectedSquare().first + move.first, _state.getSelectedSquare().second + move.second};
                    if (target.first >= 0 && target.first < BOARD_SIZE && target.second >= 0 && target.second < BOARD_SIZE)
                    {
                        Piece targetPiece = _state.pieceAt(target);
                        if (targetPiece.first == PieceType::NONE || targetPiece.second != piece.second)
                        {
                            possibleMoves.push_back(target);
                        }
                    }
                }

                // castling
                // Simple castling logic: allow castling if king and rook are in initial positions and squares between are empty
                // No check/checkmate validation, no castling rights tracking, no move history

                int y = (_state.currentTurn() == PieceColor::WHITE) ? 0 : 7;
                Square kingStart = {4, y};
                if (_state.getSelectedSquare() == kingStart)
                {
                    // King-side castling
                    Square rookKingSide = {7, y};
                    Piece rookPiece = _state.pieceAt(rookKingSide);
                    if (rookPiece.first == PieceType::ROOK && rookPiece.second == piece.second)
                    {
                        bool empty = true;
                        for (int x = 5; x < 7; ++x)
                        {
                            if (_state.pieceAt({x, y}).first != PieceType::NONE)
                            {
                                empty = false;
                                break;
                            }
                        }
                        if (empty)
                        {
                            possibleMoves.push_back({6, y});
                        }
                    }
                    // Queen-side castling
                    Square rookQueenSide = {0, y};
                    rookPiece = _state.pieceAt(rookQueenSide);
                    if (rookPiece.first == PieceType::ROOK && rookPiece.second == piece.second)
                    {
                        bool empty = true;
                        for (int x = 1; x < 4; ++x)
                        {
                            if (_state.pieceAt({x, y}).first != PieceType::NONE)
                            {
                                empty = false;
                                break;
                            }
                        }
                        if (empty)
                        {
                            possibleMoves.push_back({2, y});
                        }
                    }
                }
            }
            else if (piece.first == PieceType::ROOK || piece.first == PieceType::BISHOP || piece.first == PieceType::QUEEN)
            {
                vector<Square> directions;
                if (piece.first == PieceType::ROOK || piece.first == PieceType::QUEEN)
                {
                    directions.insert(directions.end(), {{1, 0}, {-1, 0}, {0, 1}, {0, -1}});
                }
                if (piece.first == PieceType::BISHOP || piece.first == PieceType::QUEEN)
                {
                    directions.insert(directions.end(), {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}});
                }

                for (const auto& dir : directions)
                {
                    Square target = _state.getSelectedSquare();
                    while (true)
                    {
                        target.first += dir.first;
                        target.second += dir.second;
                        if (target.first < 0 || target.first >= BOARD_SIZE || target.second < 0 || target.second >= BOARD_SIZE)
                        {
                            break;
                        }
                        Piece targetPiece = _state.pieceAt(target);
                        if (targetPiece.first == PieceType::NONE)
                        {
                            possibleMoves.push_back(target);
                        }
                        else
                        {
                            if (targetPiece.second != piece.second)
                            {
                                possibleMoves.push_back(target);
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    return possibleMoves;
}

vector<Square> Game::getPossibleMoves() const
{
    vector<Square> filteredMoves;
    auto possibleMoves = getPossibleMoveCandidates();
    for (auto candidateMove : possibleMoves)
    {
        Game tempGame = *this;
        bool moved = tempGame.move(candidateMove);
        if (moved && isValidState(tempGame))
        {
            filteredMoves.push_back(candidateMove);
        }
    }

    return filteredMoves;
}

bool Game::select(Square sq)
{
    Piece piece = _state.pieceAt(sq);
    if (piece.first != PieceType::NONE && piece.second == _state.currentTurn())
    {
        _state.selectSquare(sq);
        _state.possibleMoves() = getPossibleMoves();
        return true;
    }
    return false;
}

bool Game::isValidMove(Square to) const
{
    return _state.getSelectedSquare() != GameState::INVALID_SQUARE &&
           ranges::find(_state.possibleMoves(), to) != _state.possibleMoves().end();
}

bool Game::move(Square to)
{
    bool isCastling = to.second == _state.getSelectedSquare().second &&
                        abs(to.first - _state.getSelectedSquare().first) == 2 &&
                        _state.pieceAt(_state.getSelectedSquare()).first == PieceType::KING;
    if (isCastling)
    {
        // Move the rook as well
        int y = _state.getSelectedSquare().second;
        if (to.first == 6) // King-side castling
        {
            _state.movePiece({7, y}, {5, y});
        }
        else if (to.first == 2) // Queen-side castling
        {
            _state.movePiece({0, y}, {3, y});
        }
    }
    _state.movePiece(_state.getSelectedSquare(), to);

    bool isPromotion = _state.pieceAt(to).first == PieceType::PAWN &&
                        (to.second == 0 || to.second == BOARD_SIZE - 1);
    if (isPromotion)
    {
        _state.pieceAt(to) = Piece { PieceType::QUEEN, _state.pieceAt(to).second }; // Auto-promote to queen
    }
    _state.clearSelection();
    _state.switchTurn();
    _state.possibleMoves().clear();
    return true;
}

//------------------------------------------------------------------------------
// Chess Engine Interface (Stockfish)
//------------------------------------------------------------------------------
class StockfishEngine
{
public:
    StockfishEngine() : running(false) {}
    ~StockfishEngine() { stop(); }

    bool start(const std::string& path);
    void stop();
    void sendCommand(const std::string& cmd);
    std::string readResponse();
    std::string getMove(std::string positionFEN, int timeMs);
    void setElo(int elo);

    bool isRunning() const { return running; }

private:
#ifdef _WIN32
    HANDLE hProcess = NULL;
    HANDLE hChildStdinWr = NULL;
    HANDLE hChildStdoutRd = NULL;
#else
    pid_t pid = -1;
    int to_engine = -1;
    int from_engine = -1;
#endif
    bool running = false;
};

bool StockfishEngine::start(const std::string& path)
{
    stop();
#ifdef _WIN32
    SECURITY_ATTRIBUTES saAttr{ sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hChildStdinRd, hChildStdoutWr;

    // Create pipes for stdin and stdout
    if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) return false;
    if (!SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0)) return false;
    if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)) return false;
    if (!SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0)) return false;

    PROCESS_INFORMATION piProcInfo{};
    STARTUPINFOA siStartInfo{};
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdError = hChildStdoutWr;
    siStartInfo.hStdOutput = hChildStdoutWr;
    siStartInfo.hStdInput = hChildStdinRd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    std::string cmd = "\"" + path + "\"";
    BOOL success = CreateProcessA(
        NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &siStartInfo, &piProcInfo);

    CloseHandle(hChildStdoutWr);
    CloseHandle(hChildStdinRd);

    if (!success) {
        CloseHandle(hChildStdoutRd);
        CloseHandle(hChildStdinWr);
        return false;
    }

    hProcess = piProcInfo.hProcess;
    CloseHandle(piProcInfo.hThread);

    running = true;
#else
    int in_pipe[2], out_pipe[2];
    if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) return false;

    pid = fork();
    if (pid == 0) {
        // Child
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[1]);
        close(out_pipe[0]);
        execl(path.c_str(), path.c_str(), (char*)NULL);
        _exit(1);
    } else if (pid > 0) {
        // Parent
        close(in_pipe[0]);
        close(out_pipe[1]);
        to_engine = in_pipe[1];
        from_engine = out_pipe[0];
        fcntl(from_engine, F_SETFL, O_NONBLOCK); // Non-blocking read
        running = true;
    } else {
        return false;
    }
#endif

    sendCommand("uci");
    std::string response;
    for (int i = 0; i < 10; ++i) {
        response = readResponse();
        if (response.find("uciok") != std::string::npos) break;
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
    return running;
}

void StockfishEngine::stop()
{
#ifdef _WIN32
    if (hProcess) {
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
        hProcess = NULL;
    }
    if (hChildStdinWr) { CloseHandle(hChildStdinWr); hChildStdinWr = NULL; }
    if (hChildStdoutRd) { CloseHandle(hChildStdoutRd); hChildStdoutRd = NULL; }
#else
    if (pid > 0) {
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);
        pid = -1;
    }
    if (to_engine != -1) { close(to_engine); to_engine = -1; }
    if (from_engine != -1) { close(from_engine); from_engine = -1; }
#endif
    running = false;
}

void StockfishEngine::sendCommand(const std::string& cmd)
{
    if (!running) return;
    std::string command = cmd + "\n";
#ifdef _WIN32
    DWORD written;
    WriteFile(hChildStdinWr, command.c_str(), (DWORD)command.size(), &written, NULL);
#else
    write(to_engine, command.c_str(), command.size());
#endif
}

std::string StockfishEngine::readResponse()
{
    if (!running) return "";
    std::string result;
    char buffer[1024];
#ifdef _WIN32
    DWORD read = 0;
    while (true) {
        if (!ReadFile(hChildStdoutRd, buffer, sizeof(buffer) - 1, &read, NULL) || read == 0)
            break;
        buffer[read] = 0;
        result += buffer;
        if (result.find("bestmove") != std::string::npos || result.find("uciok") != std::string::npos)
            break;
        Sleep(10);
    }
#else
    ssize_t n;
    for (int i = 0; i < 100; ++i) {
        n = read(from_engine, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = 0;
            result += buffer;
            if (result.find("bestmove") != std::string::npos || result.find("uciok") != std::string::npos)
                break;
        } else {
            usleep(10000);
        }
    }
#endif
    return result;
}

std::string StockfishEngine::getMove(std::string positionFEN, int timeMs)
{
    if (!running) return "";
    sendCommand("position fen " + positionFEN);
    sendCommand("go movetime " + std::to_string(timeMs));
    std::string bestMove;
    Sleep(timeMs + 100);
    for (int i = 0; i < 100; ++i) {
        std::string response = readResponse();
        auto pos = response.find("bestmove");
        if (pos != std::string::npos) {
            size_t moveStart = pos + 9;
            while (moveStart < response.size() && response[moveStart] == ' ') ++moveStart;
            size_t moveEnd = moveStart;
            while (moveEnd < response.size() && response[moveEnd] != ' ' && response[moveEnd] != '\n') ++moveEnd;
            bestMove = response.substr(moveStart, moveEnd - moveStart);
            break;
        }
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
    return bestMove;
}

void StockfishEngine::setElo(int elo) {
    sendCommand("setoption name UCI_LimitStrength value true");
    sendCommand("setoption name UCI_Elo value " + std::to_string(elo));
}

//------------------------------------------------------------------------------
// User interface: render in graphical mode
//------------------------------------------------------------------------------
class Sprite {
public:
    Sprite(SDL_Renderer* renderer, const std::string& file) 
        : renderer(renderer), texture(nullptr), width(0), height(0) 
    {
        loadFromFile(file);
    }

    ~Sprite() {
        if (texture) {
            SDL_DestroyTexture(texture);
        }
    }

    // Disallow copy
    Sprite(const Sprite&) = delete;
    Sprite& operator=(const Sprite&) = delete;

    // Allow move
    Sprite(Sprite&& other) noexcept 
        : renderer(other.renderer), texture(other.texture),
          width(other.width), height(other.height)
    {
        other.texture = nullptr;
    }

    Sprite& operator=(Sprite&& other) noexcept {
        if (this != &other) {
            if (texture) SDL_DestroyTexture(texture);
            renderer = other.renderer;
            texture = other.texture;
            width = other.width;
            height = other.height;
            other.texture = nullptr;
        }
        return *this;
    }

    void draw(int x, int y, int w = -1, int h = -1) {
        SDL_FRect dst;
        dst.x = static_cast<float>(x);
        dst.y = static_cast<float>(y);
        dst.w = (w > 0) ? static_cast<float>(w) : static_cast<float>(width);
        dst.h = (h > 0) ? static_cast<float>(h) : static_cast<float>(height);

        SDL_RenderTexture(renderer, texture, nullptr, &dst);
    }

    int getWidth() const { return width; }
    int getHeight() const { return height; }

private:
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    int width, height;

    void loadFromFile(const std::string& file) {
        SDL_Surface* surface = IMG_Load(file.c_str());
        if (!surface) {
            throw std::runtime_error(std::string("Failed to load image: ") + SDL_GetError());
        }

        // Ensure the surface has alpha (e.g. for transparent PNGs)
        SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        
        SDL_DestroySurface(surface);
        if (!converted) {
            throw std::runtime_error(std::string("Failed to convert surface: ") + SDL_GetError());
        }

        texture = SDL_CreateTextureFromSurface(renderer, converted);
        width = converted->w;
        height = converted->h;

        SDL_DestroySurface(converted);

        if (!texture) {
            throw std::runtime_error(std::string("Failed to create texture: ") + SDL_GetError());
        }
    }
};

class SdlRenderer
{
public:
    SdlRenderer(const GameState& gs);
    ~SdlRenderer();
    void render();
    void zoomIn();
    void zoomOut();
    Square getSquareAtScreenPos(int x, int y);

private:
    bool _initSdl();
    bool _createWindow();
    bool _deleteWindow();
    bool _closeSdl();
    bool _loadResources();
    static const int _xborder = 8;
    static const int _yborder = 8;
    const GameState& _gs;
    int _displayWidth;
    int _displayHeight;
    int _xsize;
    int _ysize;
    int _maxSize;
    SDL_Window* _window;
    SDL_Surface* _screenSurface;
    SDL_Renderer* _renderer;
    int _pieceTypeToIndex(Piece piece);
    vector<unique_ptr<Sprite>> _pieceSprites;
};

SdlRenderer::SdlRenderer(const GameState& gs)
    : _gs(gs)
    , _xsize(16)
    , _ysize(16)
{
    _initSdl();
    _createWindow();
    char cwd[PATH_MAX];
    setvbuf(stdout, NULL, _IONBF, 0);
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::cout << "Current working directory: " << cwd << std::endl;
    } else {
        std::cerr << "getcwd() error" << std::endl;
    }
    _loadResources();
}

SdlRenderer::~SdlRenderer()
{
    _closeSdl();
}

bool SdlRenderer::_loadResources()
{
    _pieceSprites.clear();
    _pieceSprites.push_back(make_unique<Sprite>(_renderer, ASSERTS_DIR + "white-king.png"));
    _pieceSprites.push_back(make_unique<Sprite>(_renderer, ASSERTS_DIR + "white-queen.png"));
    _pieceSprites.push_back(make_unique<Sprite>(_renderer, ASSERTS_DIR + "white-rook.png"));
    _pieceSprites.push_back(make_unique<Sprite>(_renderer, ASSERTS_DIR + "white-bishop.png"));
    _pieceSprites.push_back(make_unique<Sprite>(_renderer, ASSERTS_DIR + "white-knight.png"));
    _pieceSprites.push_back(make_unique<Sprite>(_renderer, ASSERTS_DIR + "white-pawn.png"));
    _pieceSprites.push_back(make_unique<Sprite>(_renderer, ASSERTS_DIR + "black-king.png"));
    _pieceSprites.push_back(make_unique<Sprite>(_renderer, ASSERTS_DIR + "black-queen.png"));
    _pieceSprites.push_back(make_unique<Sprite>(_renderer, ASSERTS_DIR + "black-rook.png"));
    _pieceSprites.push_back(make_unique<Sprite>(_renderer, ASSERTS_DIR + "black-bishop.png"));
    _pieceSprites.push_back(make_unique<Sprite>(_renderer, ASSERTS_DIR + "black-knight.png"));
    _pieceSprites.push_back(make_unique<Sprite>(_renderer, ASSERTS_DIR + "black-pawn.png"));
    return true;
}

int SdlRenderer::_pieceTypeToIndex(Piece piece)
{
    if (piece.second == PieceColor::WHITE)
    {
        switch (piece.first)
        {
            case PieceType::KING:   return 0;
            case PieceType::QUEEN:  return 1;
            case PieceType::ROOK:   return 2;
            case PieceType::BISHOP: return 3;
            case PieceType::KNIGHT: return 4;
            case PieceType::PAWN:   return 5;
            default:                return -1;
        }
    }
    else if (piece.second == PieceColor::BLACK)
    {
        switch (piece.first)
        {
            case PieceType::KING:   return 6;
            case PieceType::QUEEN:  return 7;
            case PieceType::ROOK:   return 8;
            case PieceType::BISHOP: return 9;
            case PieceType::KNIGHT: return 10;
            case PieceType::PAWN:   return 11;
            default:                return -1;
        }
    }
    return -1;
}

Square SdlRenderer::getSquareAtScreenPos(int x, int y)
{
    int boardX = (x - _xborder) / _xsize;
    int boardY = BOARD_SIZE - 1 - (y - _yborder) / _ysize;
    if (boardX >= 0 && boardX < BOARD_SIZE && boardY >= 0 && boardY < BOARD_SIZE)
    {
        return Square{boardX, boardY};
    }
    return GameState::INVALID_SQUARE;
}

bool SdlRenderer::_initSdl()
{
    bool success = true;

    if (SDL_Init(SDL_INIT_VIDEO) == false)
    {
        SDL_Log("SDL could not initialize! SDL error: %s\n", SDL_GetError());
        success = false;
    }
    else
    {
        const SDL_DisplayMode* displayMode = SDL_GetCurrentDisplayMode(1);
        _displayWidth = displayMode->w;
        _displayHeight = displayMode->h;
        int windowPanelWidth = 20;
        int windowPanelHeight = 50;
        int taskBarHeight = _displayHeight / 20;
        _maxSize = min((_displayWidth - _xborder * 2 - windowPanelWidth) / BOARD_SIZE, (_displayHeight - _yborder * 2 - taskBarHeight - windowPanelHeight) / BOARD_SIZE);
        _xsize = _maxSize;
        _ysize = _maxSize;
    }

    return success;
}

bool SdlRenderer::_closeSdl()
{
    _deleteWindow();
    SDL_Quit();
    return true;
}

bool SdlRenderer::_createWindow()
{
    bool success = true;
    SDL_CreateWindowAndRenderer("SDL3 Chess", _xborder * 2 + _xsize * BOARD_SIZE, _yborder * 2 + _ysize * BOARD_SIZE, 0, &_window, &_renderer);
    if (!_window)
    {
        SDL_Log( "Window could not be created! SDL error: %s\n", SDL_GetError() );
        success = false;
    }
    else
    {
        _screenSurface = SDL_GetWindowSurface(_window);
    }
    return success;    
}

bool SdlRenderer::_deleteWindow()
{
    SDL_DestroyRenderer(_renderer);
    SDL_DestroyWindow(_window);
    _window = nullptr;
    _screenSurface = nullptr;
    return true;
}

void SdlRenderer::zoomIn()
{
    if (_xsize * 2 <= _maxSize && _ysize * 2 <= _maxSize)
    {
        _xsize *= 2;
        _ysize *= 2;
    }
    else
    {
        _xsize = _maxSize;
        _ysize = _maxSize;
    }
    _deleteWindow();
    _createWindow();
    _loadResources();
}

void SdlRenderer::zoomOut()
{
    if (_xsize > 1 && _ysize > 1)
    {
        _xsize /= 2;
        _ysize /= 2;
    }
    _deleteWindow();
    _createWindow();
    _loadResources();
}

void SdlRenderer::render()
{
    auto possibleMoves = _gs.possibleMoves();
    for (int y = 0; y < BOARD_SIZE; ++y) 
    {
        for (int x = 0; x < BOARD_SIZE; ++x) 
        {
            SDL_FRect rect;
            rect.x = static_cast<float>(_xborder + x * _xsize);
            rect.y = static_cast<float>(_yborder + (BOARD_SIZE - 1 - y) * _ysize);
            rect.w = static_cast<float>(_xsize);
            rect.h = static_cast<float>(_ysize);
            if ((x + y) % 2 == 0)
            {
                SDL_SetRenderDrawColor(_renderer, 0x0, 0x40, 0x60, 0xFF);
            }
            else
            {
                SDL_SetRenderDrawColor(_renderer, 0xE0, 0xE0, 0xE0, 0xFF);
            }

            if (_gs.getSelectedSquare() == Square{x, y})
            {
                SDL_SetRenderDrawColor(_renderer, 0xFF, 0xA0, 0x00, 0xFF);
            }
            SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_NONE);
            SDL_RenderFillRect(_renderer, &rect);

            if (ranges::find(possibleMoves, Square{x, y}) != possibleMoves.end())
            {
                SDL_SetRenderDrawColor(_renderer, 0x00, 0xFF, 0x00, 0x80);
                SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);
                SDL_RenderFillRect(_renderer, &rect);
            }            
            Piece piece = _gs.pieceAt(Square{x, y});
            int spriteIndex = _pieceTypeToIndex(piece);
            if (spriteIndex >= 0 && spriteIndex < static_cast<int>(_pieceSprites.size()))
            {
                _pieceSprites[spriteIndex]->draw(_xborder + x * _xsize, _yborder + (BOARD_SIZE - 1 - y) * _ysize, _xsize, _ysize);
            }
        }
    }
    SDL_RenderPresent(_renderer);
}

//------------------------------------------------------------------------------
// User interface: read keyboard input
//------------------------------------------------------------------------------
enum class UserInput
{
    NONE,
    PAUSE,
    ZOOM_IN,
    ZOOM_OUT,
    QUIT,
};

UserInput getUserInput()
{
    SDL_Event e;
    while (SDL_PollEvent(&e) == true)
    {
        if(e.type == SDL_EVENT_QUIT)
        {
            return UserInput::QUIT;
        }
        else if(e.type == SDL_EVENT_KEY_DOWN)
        {
            if(e.key.key == SDLK_ESCAPE)
            {
                return UserInput::QUIT;
            }            
            else if(e.key.key == SDLK_EQUALS)
            {
                return UserInput::ZOOM_IN;
            }
            else if(e.key.key == SDLK_MINUS)
            {
                return UserInput::ZOOM_OUT;
            }
            else if(e.key.key == SDLK_P)
            {
                return UserInput::PAUSE;
            }
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            int x = (int)e.button.x;
            int y = (int)e.button.y;
            int boardX = (x - 8) / 16;
            int boardY = 7 - (y - 8) / 16;
            std::cout << "Mouse click at (" << x << ", " << y << "), board square (" << boardX << ", " << boardY << ")\n";
        }
    }

    return UserInput::NONE;
}

int LoadStockfishEloFromINI(const std::string& filename, int defaultElo) {
    std::ifstream inifile(filename);
    if (!inifile) return defaultElo;
    std::string line;
    while (std::getline(inifile, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);
            if (key == "elo") {
                try {
                    return std::stoi(value);
                } catch (...) {
                    return defaultElo;
                }
            }
        }
    }
    return defaultElo;
}

int main(int argc, char* argv[])
{
    const int UI_POLL_PERIOD_MS = 100;
    const std::string settingsPath = "settings.ini";
    const int defaultElo = 1320;
    int elo = LoadStockfishEloFromINI(settingsPath, defaultElo);

    Game game;
    SdlRenderer sdlRenderer(game.gameState());

    StockfishEngine engine;
    bool engineStarted = engine.start(STOCKFISH_PATH);
    if (engineStarted)
    {
        std::cout << "Stockfish engine started successfully." << std::endl;
    }
    else
    {
        std::cerr << "Failed to start Stockfish engine." << std::endl;
    }
    
    PieceColor aiColor = PieceColor::NONE;
    if (engineStarted)
    {
        engine.setElo(elo);
        aiColor = PieceColor::BLACK;
    }

    game.start();

    bool needRedraw = true;
    bool paused = false;
    bool quit = false;

    while (!quit)
    {
        if (needRedraw)
        {
            sdlRenderer.render();
            needRedraw = false;
        }
        SDL_Delay(UI_POLL_PERIOD_MS);

        if (game.gameState().currentTurn() == aiColor && !paused)
        {
            string fen = game.gameState().generateFen();
            string bestMove = engine.getMove(fen, 100);
            if (bestMove.size() >= 4)
            {
                int fromX = bestMove[0] - 'a';
                int fromY = bestMove[1] - '1';
                int toX = bestMove[2] - 'a';
                int toY = bestMove[3] - '1';
                if (fromX >= 0 && fromX < BOARD_SIZE && fromY >= 0 && fromY < BOARD_SIZE &&
                    toX >= 0 && toX < BOARD_SIZE && toY >= 0 && toY < BOARD_SIZE)
                {
                    game.select({fromX, fromY});
                    //if (game.isValidMove({toX, toY}))  // Stockfish knows rules better than us
                    {
                        game.move({toX, toY});
                    }
                    needRedraw = true;
                }
            }
        }

        SDL_Event e;
        while (SDL_PollEvent(&e) == true)
        {
            if(e.type == SDL_EVENT_QUIT)
            {
                quit = true;
            }
            else if(e.type == SDL_EVENT_KEY_DOWN)
            {
                if(e.key.key == SDLK_ESCAPE)
                {
                    quit = true;
                }            
                else if(e.key.key == SDLK_EQUALS)
                {
                    sdlRenderer.zoomIn();
                }
                else if(e.key.key == SDLK_MINUS)
                {
                    sdlRenderer.zoomOut();
                }
                else if(e.key.key == SDLK_P)
                {
                    paused = !paused;
                }
                else if(e.key.key == SDLK_R)
                {
                    game.start();
                    needRedraw = true;
                }
            }
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                int x = (int)e.button.x;
                int y = (int)e.button.y;
                if (e.button.button == SDL_BUTTON_LEFT)
                {
                    auto sq = sdlRenderer.getSquareAtScreenPos(x, y);
                    if (sq != GameState::INVALID_SQUARE)
                    {
                        Piece piece = game.gameState().pieceAt(sq);
                        if (game.gameState().currentTurn() == piece.second && piece.second != PieceColor::NONE)
                        {
                            game.select(sq);
                        }
                        else
                        {
                            auto selected = game.gameState().getSelectedSquare();
                            if (selected != GameState::INVALID_SQUARE)
                            {
                                if (game.isValidMove(sq))
                                {
                                    game.move(sq);
                                }
                            }
                        }
                    }
                }
                else
                {
                    game.unselect();
                }
            }
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT)
            {
                int x = (int)e.button.x;
                int y = (int)e.button.y;
                auto sq = sdlRenderer.getSquareAtScreenPos(x, y);
                if (sq != GameState::INVALID_SQUARE && game.gameState().getSelectedSquare() != GameState::INVALID_SQUARE)
                {
                    if (game.isValidMove(sq))
                    {
                        bool moved = game.move(sq);
                    }
                }
            }
            needRedraw = true;
        }
        
        bool gameOver = (game.isGameOver() != Game::GameResult::ONGOING);
        if (gameOver)
        {
            //printf("Game over. %s\n", game.gameResultToString(game.isGameOver()).c_str());
            //printf("Press R to restart or ESC to quit.\n");
        }
    }
    return 0;
}
