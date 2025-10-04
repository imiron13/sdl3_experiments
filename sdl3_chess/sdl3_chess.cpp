#include <array>
#include <vector>
#include <memory>
#include <iostream>
#include <random>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

using namespace std;

enum class PieceType
{
    KING, QUEEN, ROOK, KNIGHT, BISHOP, PAWN, NONE
};

enum class PieceColor
{
    WHITE, BLACK, NONE
};

using Piece = pair<PieceType, PieceColor>;

const int BOARD_SIZE = 8;

class GameState
{
public:
    void initializeBoard();
    const Piece& pieceAt(int x, int y) const { return _board[x][y]; }
private:
    array<array<Piece, BOARD_SIZE>, BOARD_SIZE> _board;
};

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

class Game
{
    GameState _state;
public:
    GameState& gameState() { return _state; }
    void start();
    bool doTick() { return false; } // return true if game over
};

void Game::start()
{
    _state.initializeBoard();
}

//------------------------------------------------------------------------------
// User interface: render to console
//------------------------------------------------------------------------------
class ConsoleRenderer
{
public:
    ConsoleRenderer(const GameState& gs);
    void render();

private:
    void _cursorToTopLeft();
    const GameState& _gs;
};

ConsoleRenderer::ConsoleRenderer(const GameState& gs)
    : _gs(gs)
{
}

void ConsoleRenderer::render()
{
    _cursorToTopLeft();
    int width = BOARD_SIZE * 2 + 2;
    int height = BOARD_SIZE + 2;
    // Unicode box-drawing characters
    const char* topLeft = "┌";
    const char* topRight = "┐";
    const char* bottomLeft = "└";
    const char* bottomRight = "┘";
    const char* horizontal = "─";
    const char* vertical = "│";

    // Draw top border
    std::cout << topLeft;
    for (int i = 0; i < width - 2; ++i) {
        std::cout << horizontal;
    }
    std::cout << topRight << std::endl;

    // Draw middle part
    for (int i = 0; i < height - 2; ++i) {
        std::cout << vertical;
        for (int j = 0; j < width - 2; ++j) {
            int y = height - 3 - i;
            int x = j / 2;
            if (j % 2 == 0)
            {
                Piece piece = _gs.pieceAt(x, y);
                char symbol = ' ';
                switch (piece.first)
                {
                    case PieceType::KING:   symbol = (piece.second == PieceColor::WHITE) ? 'K' : 'k'; break;
                    case PieceType::QUEEN:  symbol = (piece.second == PieceColor::WHITE) ? 'Q' : 'q'; break;
                    case PieceType::ROOK:   symbol = (piece.second == PieceColor::WHITE) ? 'R' : 'r'; break;
                    case PieceType::BISHOP: symbol = (piece.second == PieceColor::WHITE) ? 'B' : 'b'; break;
                    case PieceType::KNIGHT: symbol = (piece.second == PieceColor::WHITE) ? 'N' : 'n'; break;
                    case PieceType::PAWN:   symbol = (piece.second == PieceColor::WHITE) ? 'P' : 'p'; break;
                }
                std::cout << symbol;
            }
            else std::cout << ' ';

        }
        std::cout << vertical << std::endl;
    }

    // Draw bottom border
    std::cout << bottomLeft;
    for (int i = 0; i < width - 2; ++i) {
        std::cout << horizontal;
    }
    std::cout << bottomRight << std::endl;
}

void ConsoleRenderer::_cursorToTopLeft()
{
    std::cout << "\033[H";
}

//------------------------------------------------------------------------------
// User interface: render in graphical mode
//------------------------------------------------------------------------------
class SdlRenderer
{
public:
    SdlRenderer(const GameState& gs);
    ~SdlRenderer();
    void render();
    void zoomIn();
    void zoomOut();

private:
    bool _initSdl();
    bool _createWindow();
    bool _deleteWindow();
    bool _closeSdl();
    
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
};

SdlRenderer::SdlRenderer(const GameState& gs)
    : _gs(gs)
    , _xsize(16)
    , _ysize(16)
{
    _initSdl();
    _createWindow();
}

SdlRenderer::~SdlRenderer()
{
    _closeSdl();
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
    _window = SDL_CreateWindow("SDL3 Chess", _xborder * 2 + _xsize * BOARD_SIZE, _yborder * 2 + _ysize * BOARD_SIZE, 0);
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
}

void SdlRenderer::render()
{
    SDL_Rect fieldRect(0, 0, _xborder * 2 + BOARD_SIZE * _xsize, _yborder * 2 + BOARD_SIZE * _ysize);
    SDL_FillSurfaceRect(_screenSurface, &fieldRect, SDL_MapSurfaceRGB(_screenSurface, 0xFF, 0xFF, 0xFF));
    fieldRect = SDL_Rect(_xborder, _yborder, BOARD_SIZE * _xsize, BOARD_SIZE * _ysize);
    SDL_FillSurfaceRect(_screenSurface, &fieldRect, SDL_MapSurfaceRGB(_screenSurface, 0, 0, 0));

    for (int y = 0; y < BOARD_SIZE; ++y) 
    {
        for (int x = 0; x < BOARD_SIZE; ++x) 
        {
            #if 0
            if (_gs.isFieldPixelOccupied(x, y))
            {
                SDL_Rect rect(_xborder + x * _xsize + 1, _yborder + y * _ysize + 1, _xsize - 2, _ysize - 2);
                SDL_FillSurfaceRect(_screenSurface, &rect, SDL_MapSurfaceRGB(_screenSurface, 0xFF, 0xFF, 0xFF ));
            }
            else
            {
                SDL_Rect rect(_xborder + x * _xsize, _yborder + y * _ysize, _xsize, _ysize);
                SDL_FillSurfaceRect(_screenSurface, &rect, SDL_MapSurfaceRGB(_screenSurface, 0x00, 0x00, 0x00 ));
            }
            #endif
        }
    }
    SDL_UpdateWindowSurface(_window);
}

//------------------------------------------------------------------------------
// User interface: read keyboard input
//------------------------------------------------------------------------------
enum class UserInput
{
    NONE,
    MOVE_LEFT,
    MOVE_RIGHT,
    ACCELERATE,
    ROTATE,
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
            else if(e.key.key == SDLK_DOWN)
            {
                return UserInput::ACCELERATE;
            }
            else if(e.key.key == SDLK_LEFT)
            {
                return UserInput::MOVE_LEFT;
            }
            else if(e.key.key == SDLK_RIGHT)
            {
                return UserInput::MOVE_RIGHT;
            }
            else if(e.key.key == SDLK_SPACE || e.key.key == SDLK_UP)
            {
                return UserInput::ROTATE;
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
    }

    return UserInput::NONE;
}

int main(int argc, char* argv[])
{
    Game game;
    ConsoleRenderer consoleRenderer(game.gameState());
    SdlRenderer sdlRenderer(game.gameState());
    game.start();
    int subTickDelayMs = 10;
    int subTicksPerTickNormal = 20;

    int subTickCnt = 0;
    bool needRedraw = true;
    bool paused = false;
    bool quit = false;
    while (!quit)
    {
        if (needRedraw)
        {
            consoleRenderer.render();
            sdlRenderer.render();

            needRedraw = false;
        }
        SDL_Delay(subTickDelayMs);

        UserInput userInput;
        while ((userInput = getUserInput()) != UserInput::NONE)
        {
            if (userInput == UserInput::QUIT)
            {
                quit = true;
            }
            else if (userInput == UserInput::PAUSE)
            {
                paused = !paused;
            }
            else if (userInput == UserInput::MOVE_LEFT)
            {
                //game.handleMoveLeft();
            }
            else if (userInput == UserInput::MOVE_RIGHT)
            {
                //game.handleMoveRight();
            }
            else if (userInput == UserInput::ACCELERATE)
            {
                //game.handleAccelerate();
            }
            else if (userInput == UserInput::ROTATE)
            {
                //game.handleRotate();
            }
            else if (userInput == UserInput::ZOOM_IN)
            {
                sdlRenderer.zoomIn();
            }
            else if (userInput == UserInput::ZOOM_OUT)
            {
                sdlRenderer.zoomOut();
            }
            needRedraw = true;
        }
        
        subTickCnt++;
        int subTicksPerTick = subTicksPerTickNormal;
        if (subTickCnt >= subTicksPerTick && !paused)
        {
            bool gameOver = game.doTick();
            if (gameOver)
            {
                game.start();
            }
            subTickCnt = 0;
            needRedraw = true;
        }
    }
    return 0;
}
