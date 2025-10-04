#include <array>
#include <vector>
#include <memory>
#include <iostream>
#include <random>
#include <algorithm>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

using namespace std;

constexpr int kNumTetrominoes = 7;
constexpr int kNumRotates = 4;
constexpr int kMaxTetrominoWidth = 4;
constexpr int kMaxTetrominoHeight = 4;

enum TetrominoType
{
    TETROMINO_NONE,
    TETROMINO_O, 
    TETROMINO_I, 
    TETROMINO_S, 
    TETROMINO_Z, 
    TETROMINO_L, 
    TETROMINO_J, 
    TETROMINO_T
};

enum Rotate
{
    ROTATE_0,
    ROTATE_90,
    ROTATE_180,
    ROTATE_270,
};

struct Point
{
    int x;
    int y;
};

using TetrominoMask = array<unsigned, kMaxTetrominoHeight>;
array<array<TetrominoMask, kNumRotates>, kNumTetrominoes + 1> gTetrominoMasks =
    {{
        {{ /* TETROMINO_NONE */
            /* ROTATE_0 */ {{ 
                0b0000,
                0b0000,
                0b0000,
                0b0000
            }},
            /* ROTATE_90 */ {{ 
                0b0000,
                0b0000,
                0b0000,
                0b0000
            }},
            /* ROTATE_180 */ {{
                0b0000,
                0b0000,
                0b0000,
                0b0000
            }},
            /* ROTATE_270 */ {{
                0b0000,
                0b0000,
                0b0000,
                0b0000
            }},
        }},
        {{ /* TETROMINO_O */
            /* ROTATE_0*/ {{
                0b0110,
                0b0110,
                0b0000,
                0b0000
            }},
            /* ROTATE_90*/ {{
                0b0110,
                0b0110,
                0b0000,
                0b0000
            }},
            /* ROTATE_180*/ {{
                0b0110,
                0b0110,
                0b0000,
                0b0000
            }},
            /* ROTATE_270*/ {{
                0b0110,
                0b0110,
                0b0000,
                0b0000
            }},
        }},

        {{ /*TETROMINO_I*/
            /* ROTATE_0 */ {{ 
                0b0000,
                0b1111,
                0b0000,
                0b0000
            }},
            /* ROTATE_90 */ {{ 
                0b0010,
                0b0010,
                0b0010,
                0b0010
            }},
            /* ROTATE_180 */ {{
                0b0000,
                0b0000,
                0b1111,
                0b0000
            }},
            /* ROTATE_270 */ {{
                0b0100,
                0b0100,
                0b0100,
                0b0100
            }},
        }},
        {{ /*TETROMINO_S*/
            /* ROTATE_0 */ {{
                0b0110,
                0b1100,
                0b0000,
                0b0000
            }},
            /* ROTATE_90 */ {{
                0b0100,
                0b0110,
                0b0010,
                0b0000
            }},
            /* ROTATE_180 */ {{
                0b0000,
                0b0110,
                0b1100,
                0b0000
            }},
            /* ROTATE_270 */ {{
                0b1000,
                0b1100,
                0b0100,
                0b0000
            }},
        }},
        {{ /*TETROMINO_Z*/
            /* ROTATE_0 */ {{
                0b1100,
                0b0110,
                0b0000,
                0b0000
            }},
            /* ROTATE_90 */ {{
                0b0010,
                0b0110,
                0b0100,
                0b0000
            }},
            /* ROTATE_180 */ {{
                0b0000,
                0b1100,
                0b0110,
                0b0000
            }},
            /* ROTATE_270 */ {{
                0b0100,
                0b1100,
                0b1000,
                0b0000
            }},
        }},
        {{ /*TETROMINO_L*/
            /* ROTATE_0 */ {{
                0b0010,
                0b1110,
                0b0000,
                0b0000
            }},
            /* ROTATE_90 */ {{
                0b0100,
                0b0100,
                0b0110,
                0b0000
            }},
            /* ROTATE_180 */ {{
                0b0000,
                0b1110,
                0b1000,
                0b0000
            }},
            /* ROTATE_270 */ {{
                0b1100,
                0b0100,
                0b0100,
                0b0000
            }},
        }},
        {{ /*TETROMINO_J*/
            /* ROTATE_0 */ {{
                0b1000,
                0b1110,
                0b0000,
                0b0000
            }},
            /* ROTATE_90 */ {{
                0b0110,
                0b0100,
                0b0100,
                0b0000
            }},
            /* ROTATE_180 */ {{
                0b0000,
                0b1110,
                0b0010,
                0b0000
            }},
            /* ROTATE_270 */ {{
                0b0100,
                0b0100,
                0b1100,
                0b0000
            }},
        }},
        {{ /* TETROMINO_T*/
            /* ROTATE_0 */ {{
                0b0100,
                0b1110,
                0b0000,
                0b0000
            }},
            /* ROTATE_90 */ {{
                0b0100,
                0b0110,
                0b0100,
                0b0000
            }},
            /* ROTATE_180 */ {{
                0b0000,
                0b1110,
                0b0100,
                0b0000
            }},
            /* ROTATE_270 */ {{
                0b0100,
                0b1100,
                0b0100,
                0b0000
            }},
        }},
    }};

class Field
{
public:
    Field(int width, int height);

    int width() const {
        return _width;
    }

    int height() const {
        return _height;
    }

    bool get(int x, int y) const {
        return _fieldMap.at(ofs(x, y));
    }
    void set(int x, int y, bool val=true) {
        _fieldMap.at(ofs(x, y)) = val;
    }
    void clear(int x, int y) {
        set(x, y, false);
    }

    void clear() {
        fill(_fieldMap.begin(), _fieldMap.end(), false);
    }

    bool isLineFull(int y) const;
    void deleteLine(int y);

private:
    int ofs(int x, int y) const { 
        return  y * _width + x; 
    }

    int _width;
    int _height;
    vector<bool> _fieldMap;
};

bool Field::isLineFull(int y) const
{
    for (int x = 0; x < _width; x++)
    {
        if (get(x, y) == false)
        {
            return false;
        }
    }
    return true;
}

Field::Field(int width, int height)
    : _width(width)
    , _height(height)
    , _fieldMap(width * height)
{
    clear();
}

void Field::deleteLine(int y)
{
    for (int cy = y; cy > 0; cy--)
    {
        for (int cx = 0; cx < _width; cx++)
        {
            set(cx, cy, get(cx, cy -1));
        }
    }
    for (int cx = 0; cx < _width; cx++)
    {
        set(cx, 0, false);
    }
}

class Tetromino
{
public:
    Tetromino();
    Tetromino(TetrominoType type, Rotate rotate, Point pos);
    Point pos() const               { return _pos; }
    void setPos(Point pos)          { _pos = pos; }
    Rotate rotate() const           { return _rotate; }
    void setRotate(Rotate rotate)   { _rotate = rotate; }
    TetrominoType type() const      { return _type; }
    TetrominoMask mask() const      { return gTetrominoMasks[_type][_rotate]; }
    void move(int dx, int dy);

private:
    TetrominoType _type;
    Rotate _rotate;
    Point _pos;  // upper left
};

Tetromino::Tetromino(TetrominoType type, Rotate rotate, Point pos)
    : _type(type)
    , _rotate(rotate)
    , _pos(pos)
{

}

Tetromino::Tetromino()
    : Tetromino(TETROMINO_NONE, Rotate::ROTATE_0, Point(0, 0))
{
}

void Tetromino::move(int dx, int dy)
{    _pos.x = _pos.x + dx;
    _pos.y = _pos.y + dy;
}

class GameState
{
public:
    GameState(int width = 10, int height = 20)
        : _field(make_unique<Field>(width, height))
        , _tetromino()
    {
    }

    Field& field() { return *_field; }
    const Field& field() const { return *_field; }
    Tetromino& tetromino() { return _tetromino; }
    const Tetromino& tetromino() const { return _tetromino; }
    void setTetromino(const Tetromino& tetromino) { _tetromino = tetromino; }
    bool isFieldPixelOccupied(int x, int y) const;
    bool isValidTetrominoPosition(Tetromino &tetr) const;

private:
    unique_ptr<Field> _field;
    Tetromino _tetromino;
};

bool GameState::isValidTetrominoPosition(Tetromino &tetr) const
{
    auto mask = tetr.mask();
    for (int mx = 0; mx < mask.size(); mx++)
    {
        for (int my = 0; my < kMaxTetrominoWidth; my++)
        {
            if (mask[mx] & (1 << my))
            {
                int x = tetr.pos().x + my;
                int y = tetr.pos().y + mx;
                if (x < 0 || x >= field().width() || y < 0 ||y >= field().height() || field().get(x, y))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

bool GameState::isFieldPixelOccupied(int x, int y) const
{
    if (field().get(x, y))
    {
        return true;
    }
    else
    {
        auto mask = _tetromino.mask();
        for (int dy = 0; dy < mask.size(); dy++)
        {
            for (int dx = 0; dx < kMaxTetrominoWidth; dx++)
            {
                if (mask[dy] & (1 << dx))
                {
                    int tx = _tetromino.pos().x + dx;
                    int ty = _tetromino.pos().y + dy;
                    if (tx == x && ty == y)
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    }
}

class Game
{
public:
    Game(int width, int height)
        : _state(width, height)
        , _isAccelerated(false)
        , _rng(std::random_device()())
    {
    }

    GameState& gameState() { return _state; }
    void start();
    bool doTick();
    bool isAccelerated() { return _isAccelerated; }
    void handleMoveLeft();
    void handleMoveRight();
    void handleAccelerate();
    void handleRotate();

private:
    bool _canMoveTetromino(int dx, int dy) const;
    bool _canRotateTetromino() const;
    bool _canAddTetromino(Tetromino& tetr) const;
    void _moveTetromino(int dx, int dy);
    Tetromino _getRandomTetromino();
    void _fixTetromino();
    GameState _state;
    bool _isAccelerated;
    std::default_random_engine _rng;
};

void Game::start()
{
    // random seed
    std::random_device r;
    
    _state.field().clear();
    Tetromino tetr(_getRandomTetromino());
    _state.setTetromino(tetr);
}

bool Game::doTick()
{
    bool gameOver = false;
    if (_canMoveTetromino(0, 1))
    {
        _moveTetromino(0, 1);
    }
    else
    {
        _fixTetromino();
        int y0 = _state.tetromino().pos().y;
        for (int y = y0; y < y0 + kMaxTetrominoHeight; y++)
        {
            if (y < _state.field().height() && _state.field().isLineFull(y))
            {
                _state.field().deleteLine(y);
            }
        }
        Tetromino tetr(_getRandomTetromino());
        if (_state.isValidTetrominoPosition(tetr))
        {
            _state.setTetromino(tetr);
            _isAccelerated = false;
        }
        else
        {
            gameOver = true;
        }
    }
    return gameOver;
}

bool Game::_canMoveTetromino(int dx, int dy) const
{
    Tetromino tetr(_state.tetromino().type(), _state.tetromino().rotate(), 
        Point(_state.tetromino().pos().x + dx, _state.tetromino().pos().y + dy));
    return _state.isValidTetrominoPosition(tetr);
}

bool Game::_canRotateTetromino() const
{
    auto newRotate = static_cast<Rotate>((static_cast<int>(_state.tetromino().rotate()) + 1) % kNumRotates);
    Tetromino tetr(_state.tetromino().type(), newRotate, _state.tetromino().pos());
    return _state.isValidTetrominoPosition(tetr);
}

bool Game::_canAddTetromino(Tetromino& tetr) const
{
    return _state.isValidTetrominoPosition(tetr);
}

void Game::_moveTetromino(int dx, int dy)
{
    _state.tetromino().move(dx, dy);
}

Tetromino Game::_getRandomTetromino()
{
    std::uniform_int_distribution<int> uniform_dist(1, kNumTetrominoes);
    int tetrominoIdx = uniform_dist(_rng);
    return Tetromino(static_cast<TetrominoType>(tetrominoIdx), ROTATE_0, Point((_state.field().width() - 4) / 2, 0));
}

void Game::handleMoveLeft()
{
    if (_canMoveTetromino(-1, 0))
    {
        _moveTetromino(-1, 0);
    }
}

void Game::handleMoveRight()
{
    if (_canMoveTetromino(+1, 0))
    {
        _moveTetromino(+1, 0);
    }
}

void Game::handleAccelerate()
{
    _isAccelerated = true;
}

void Game::handleRotate()
{
    if (_canRotateTetromino())
    {
        auto newRotate = static_cast<Rotate>((static_cast<int>(_state.tetromino().rotate()) + 1) % kNumRotates);
        _state.tetromino().setRotate(newRotate);
    }
}

void Game::_fixTetromino()
{
    auto mask = _state.tetromino().mask();
    for (int dy = 0; dy < mask.size(); dy++)
    {
        for (int dx = 0; dx < kMaxTetrominoWidth; dx++)
        {
            if (mask[dy] & (1 << dx))
            {
                int x = _state.tetromino().pos().x + dx;
                int y = _state.tetromino().pos().y + dy;
                _state.field().set(x, y, true);
            }
        }
    }
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
    int width = _gs.field().width() * 2 + 2;
    int height = _gs.field().height() + 2;
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
            int y = i;
            int x = j / 2;
            if (_gs.isFieldPixelOccupied(x, y))
            {
                std::cout << "█";
            }
            else
            {
                std::cout << " ";
            }
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
    if (SDL_Init(SDL_INIT_VIDEO) == false)
    {
        SDL_Log("SDL could not initialize! SDL error: %s\n", SDL_GetError());
        return false;
    }
    else
    {
        SDL_DisplayID displayID = SDL_GetPrimaryDisplay();
        const SDL_DisplayMode* displayMode = SDL_GetCurrentDisplayMode(displayID);
        if (!displayMode)
        {
            SDL_Log("SDL_GetCurrentDisplayMode failed: %s\n", SDL_GetError());
            return false;
        }
        _displayWidth = displayMode->w;
        _displayHeight = displayMode->h;
        int windowPanelWidth = 20;
        int windowPanelHeight = 50;
        int taskBarHeight = _displayHeight / 20;
        _maxSize = min((_displayWidth - _xborder * 2 - windowPanelWidth) / _gs.field().width(), (_displayHeight - _yborder * 2 - taskBarHeight - windowPanelHeight) / _gs.field().height());
        _xsize = _maxSize;
        _ysize = _maxSize;
    }

    return true;
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
    _window = SDL_CreateWindow("SDL3 Tetris", _xborder * 2 + _xsize * _gs.field().width(), _yborder * 2 + _ysize * _gs.field().height(), 0);
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
    SDL_Rect fieldRect(0, 0, _xborder * 2 + _gs.field().width() * _xsize, _yborder * 2 + _gs.field().height() * _ysize);
    SDL_FillSurfaceRect(_screenSurface, &fieldRect, SDL_MapSurfaceRGB(_screenSurface, 0xFF, 0xFF, 0xFF));
    fieldRect = SDL_Rect(_xborder, _yborder, _gs.field().width() * _xsize, _gs.field().height() * _ysize);
    SDL_FillSurfaceRect(_screenSurface, &fieldRect, SDL_MapSurfaceRGB(_screenSurface, 0, 0, 0));

    for (int y = 0; y < _gs.field().height(); ++y) 
    {
        for (int x = 0; x < _gs.field().width(); ++x) 
        {
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
    Game game(10,20);
    //ConsoleRenderer consoleRenderer(game.gameState());
    SdlRenderer sdlRenderer(game.gameState());
    game.start();
    int subTickDelayMs = 10;
    int subTicksPerTickNormal = 20;
    int subTicksPerTickAccelerated = 5;

    int subTickCnt = 0;
    bool needRedraw = true;
    bool paused = false;
    bool quit = false;
    while (!quit)
    {
        if (needRedraw)
        {
            //consoleRenderer.render();
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
                game.handleMoveLeft();
            }
            else if (userInput == UserInput::MOVE_RIGHT)
            {
                game.handleMoveRight();
            }
            else if (userInput == UserInput::ACCELERATE)
            {
                game.handleAccelerate();
            }
            else if (userInput == UserInput::ROTATE)
            {
                game.handleRotate();
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
        int subTicksPerTick = game.isAccelerated() ? subTicksPerTickAccelerated : subTicksPerTickNormal;
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
