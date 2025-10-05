#include <array>
#include <vector>
#include <memory>
#include <iostream>
#include <random>
#include <algorithm>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <sys/wait.h>

using namespace std;

const string ASSERTS_DIR =  "sdl3_chess/assets/";
const string STOCKFISH_PATH =  "sdl3_chess/stockfish/stockfish";
//const string ASSERTS_DIR =  "../../../assets/";
//const string STOCKFISH_PATH =  "../../../stockfish/stockfish";

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

    // Castling rights (not tracked, so always "-")
    fen += " -";

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
    GameState& gameState() { return _state; }
    void start();
    vector<Square> getPossibleMoves() const;
    bool isValidMove(Square to) const;
    bool select(Square sq);
    void unselect() { _state.clearSelection(); _state.possibleMoves().clear(); }
    bool move(Square to);
    bool isGameOver() const { return false; } // Placeholder
};

void Game::start()
{
    _state.initializeBoard();
}

vector<Square> Game::getPossibleMoves() const
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
    bool validMove = _state.getSelectedSquare() != GameState::INVALID_SQUARE &&
                     ranges::find(_state.possibleMoves(), to) != _state.possibleMoves().end();
    if (validMove)
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
    }
    _state.clearSelection();
    _state.possibleMoves().clear();
    return validMove;
}

//------------------------------------------------------------------------------
// Chess Engine Interface (Stockfish)
//------------------------------------------------------------------------------
class StockfishEngine
{
    pid_t engineProcess;
    int toEnginePipe[2];   // Parent writes to engine
    int fromEnginePipe[2]; // Parent reads from engine
public:
    StockfishEngine() : engineProcess(-1), toEnginePipe{-1, -1}, fromEnginePipe{-1, -1} {}
    ~StockfishEngine() { stop(); }

    void setElo(int elo) {
        sendCommand("setoption name UCI_LimitStrength value true");
        sendCommand("setoption name UCI_Elo value " + to_string(elo));
    }
    bool start(const string& path);
    void stop();
    bool isRunning() const { return engineProcess != -1; }
    void sendCommand(const string& cmd);
    string readResponse();
    string getMove(string positionFEN, int timeMs);
};

bool StockfishEngine::start(const string& path)
{
    if (isRunning()) return false;

    if (pipe(toEnginePipe) == -1 || pipe(fromEnginePipe) == -1)
    {
        perror("pipe");
        return false;
    }

    engineProcess = fork();
    if (engineProcess == -1)
    {
        perror("fork");
        return false;
    }

    if (engineProcess == 0) // Child process
    {
        close(toEnginePipe[1]);
        close(fromEnginePipe[0]);

        dup2(toEnginePipe[0], STDIN_FILENO);
        dup2(fromEnginePipe[1], STDOUT_FILENO);
        dup2(fromEnginePipe[1], STDERR_FILENO);

        close(toEnginePipe[0]);
        close(fromEnginePipe[1]);

        execl(path.c_str(), path.c_str(), nullptr);
        perror("execl");
        exit(1);
    }
    else // Parent process
    {
        close(toEnginePipe[0]);
        close(fromEnginePipe[1]);
    }
    sendCommand("uci");
    while (true)
    {
        string response = readResponse();
        if (response.find("uciok") != string::npos) break;
    }
    return true;
}

void StockfishEngine::stop()
{
    if (!isRunning()) return;

    close(toEnginePipe[1]);
    close(fromEnginePipe[0]);

    int status;
    waitpid(engineProcess, &status, 0);
    engineProcess = -1;
}

void StockfishEngine::sendCommand(const string& cmd)
{
    if (!isRunning()) return;

    string command = cmd + "\n";
    write(toEnginePipe[1], command.c_str(), command.size());
}

string StockfishEngine::readResponse()
{
    if (!isRunning()) return "";

    char buffer[1024];
    ssize_t count = read(fromEnginePipe[0], buffer, sizeof(buffer) - 1);
    if (count > 0)
    {
        buffer[count] = '\0';
        return string(buffer);
    }
    return "";
}

string StockfishEngine::getMove(string positionFEN, int timeMs)
{
    if (!isRunning()) return "";

    sendCommand("position fen " + positionFEN);
    sendCommand("go movetime " + to_string(timeMs));

    string bestMove;
    while (true)
    {
        string response = readResponse();
        if (response.find("bestmove") != string::npos)
        {
            size_t pos = response.find("bestmove");
            size_t end = response.find(' ', pos);
            if (end == string::npos) end = response.size();
            bestMove = response.substr(pos + 9, 4);
            break;
        }
    }
    return bestMove;
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
            int x = e.button.x;
            int y = e.button.y;
            int boardX = (x - 8) / 16;
            int boardY = 7 - (y - 8) / 16;
            std::cout << "Mouse click at (" << x << ", " << y << "), board square (" << boardX << ", " << boardY << ")\n";
        }
    }

    return UserInput::NONE;
}

int main(int argc, char* argv[])
{
    const int UI_POLL_PERIOD_MS = 100;

    Game game;
    SdlRenderer sdlRenderer(game.gameState());

    StockfishEngine engine;
    engine.start(STOCKFISH_PATH);
    engine.setElo(1320);

    game.start();

    bool needRedraw = true;
    bool paused = false;
    bool quit = false;
    bool needMoveRecalc = true;
    PieceColor aiColor = PieceColor::BLACK;

    while (!quit)
    {
        if (needRedraw)
        {
            sdlRenderer.render();
            needRedraw = false;
        }
        if (needMoveRecalc)
        {
            needMoveRecalc = false;
            string fen = game.gameState().generateFen();
            std::cout << "FEN: " << fen << std::endl;
            string bestMove = engine.getMove(fen, 10);
            std::cout << "Stockfish best move: " << bestMove << std::endl;
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
                    bool moved = game.move({toX, toY});
                    needMoveRecalc = true;
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
            }
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                int x = e.button.x;
                int y = e.button.y;
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
                                bool moved = game.move(sq);
                                needMoveRecalc  = true;
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
                int x = e.button.x;
                int y = e.button.y;
                auto sq = sdlRenderer.getSquareAtScreenPos(x, y);
                if (sq != GameState::INVALID_SQUARE && game.gameState().getSelectedSquare() != GameState::INVALID_SQUARE)
                {
                    if (game.isValidMove(sq))
                    {
                        bool moved = game.move(sq);
                        needMoveRecalc  = true;
                    }
                }
            }
            needRedraw = true;
        }
        
        bool gameOver = game.isGameOver();
        if (gameOver)
        {
            game.start();
        }
    }
    return 0;
}
