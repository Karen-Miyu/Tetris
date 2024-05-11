#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define SQUARE_SIZE             20   //Definindo o tamanho do quadrado

#define Row    12 //Definindo a largura
#define Col      20 //Definindo a coluna

#define LATERAL_SPEED           10 //Velocidade lateral
#define TURNING_SPEED           12 //Velocidade de rotação
#define FAST_FALL_AWAIT_COUNTER 30 //Contador de espera de queda rápida

#define FADING_TIME             33 //Tempo de desaparecimento

typedef enum GridSquare { EMPTY, MOVING, FULL, BLOCK, FADING } GridSquare;

static const int screenWidth = 800;
static const int screenHeight = 450;

static bool gameOver = false;
static bool pause = false;
static bool sair = false;

// Matrizes
static GridSquare grid[Row][Col]; //Representa o tamanho do campo do jogo
static GridSquare piece[4][4]; //peça atual
static GridSquare incomingPiece[4][4]; //próxima peça a entrar no jogo

// Estas variáveis mantêm o registo da posição da peça ativa
static int piecePositionX = 0;
static int piecePositionY = 0;

// Parâmetros do jogo
static Color fadingColor;
//static int fallingSpeed;           // In frames

static bool beginPlay = true;      // This var is only true at the begining of the game, used for the first matrix creations
static bool pieceActive = false;
static bool detection = false;
static bool lineToDelete = false;

// Statistics
static int level = 1;
static int lines = 0;

// Contadores
static int gravityMovementCounter = 0; //Contador de movimentação para baixo
static int lateralMovementCounter = 0; //Contador de movimentos laterais
static int turnMovementCounter = 0;  //Contador de movimento de rotação
static int fastFallMovementCounter = 0;  //Contador de movimento de queda rápido

static int fadeLineCounter = 0; //Contador das linhas que desapareceu

static int gravitySpeed = 30; //Velocidade de queda
int targetFPS = 60;

static void InitGame(void);         // Initialize game
static void UpdateGame(void);       // Update game (one frame)
static void DrawGame(void);         // Draw game (one frame)
static void UpdateDrawFrame(void);  // Update and Draw (one frame)

static bool Createpiece(); //Cria uma nova peça adicionando no topo do campo de jogo
static void GetRandompiece(); //Escolhe uma peça aleatoriamente para a próxima entrada
static void ResolveFallingMovement(bool* detection, bool* pieceActive); //Resolve o movimento de queda da peça
static bool ResolveLateralMovement(); //Resolve o movimento lateral da peça
static bool ResolveTurnMovement(); //Resolve o movimento de rotação da peça
static void CheckDetection(bool* detection);  //Verifica se houve colisão entre as peças
static void CheckCompletion(bool* lineToDelete); //Verifica se uma linha foi completa
static int DeleteCompleteLines(); //Remove as linhas completadas

int main(void)
{
    InitWindow(screenWidth, screenHeight, "Tetris");

    Image Background = LoadImage("../Background.png");
    ImageResize(&Background, 820, 450);
    Texture2D bgTexture = LoadTextureFromImage(Background);
    UnloadImage(Background);

    InitGame();

    SetTargetFPS(targetFPS);
        
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        if (IsKeyPressed('E')) sair = !sair;

        if (sair) {
            CloseWindow();
            return 0;
        }

        UpdateDrawFrame();

        DrawTexture(bgTexture, 0, 0, WHITE);
    }
    UnloadTexture(bgTexture);
    CloseWindow();        // Close window and OpenGL context

    return 0;
}

// Initialize game variables
void InitGame(void)
{
    // Initialize game statistics
    level = 1;
    lines = 0;

    fadingColor = GRAY;

    piecePositionX = 0;
    piecePositionY = 1;

    pause = false;

    beginPlay = true;
    pieceActive = false;
    detection = false;
    lineToDelete = false;

    // Counters
    gravityMovementCounter = 0;
    lateralMovementCounter = 0;
    turnMovementCounter = 0;
    fastFallMovementCounter = 0;

    fadeLineCounter = 0;
    gravitySpeed = 30;

     //Estes loop criam o frame do jogo, colocando ou não o preenchimento
    for (int i = 0; i < Row; i++)
    {
        for (int j = 0; j < Col; j++)
        {
            if ((j == Col - 1) || (j == 0) || (i == 0) || (i == Row - 1)) grid[i][j] = BLOCK;
            else grid[i][j] = EMPTY;
        }
    }

     //Permite deixar a matriz vazio para inicializar corretamente uma nova peça aleatoriamente, que ocupará o mesmo espaço que ocupou a anterior
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            incomingPiece[i][j] = EMPTY;
        }
    }
}

// Update game (one frame)
void UpdateGame(void)
{
    if (!gameOver)
    {
        if (IsKeyPressed('P')) pause = !pause;

        if (!pause)
        {
            if (!lineToDelete)
            {
                if (!pieceActive)
                {
                    // Get another piece
                    pieceActive = Createpiece();

                    // We leave a little time before starting the fast falling down
                    fastFallMovementCounter = 0;
                }
                else    // Piece falling
                {
                    // Counters update
                    fastFallMovementCounter++;
                    gravityMovementCounter++;
                    lateralMovementCounter++;
                    turnMovementCounter++;

                    // We make sure to move if we've pressed the key this frame
                    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) lateralMovementCounter = LATERAL_SPEED;
                    if (IsKeyPressed(KEY_DOWN)) turnMovementCounter = TURNING_SPEED;

                    // Fall down
                    if (IsKeyDown(KEY_UP) && (fastFallMovementCounter >= FAST_FALL_AWAIT_COUNTER))
                    {
                        // We make sure the piece is going to fall this frame
                        gravityMovementCounter += gravitySpeed;
                    }

                    if (gravityMovementCounter >= gravitySpeed)
                    {
                        // Basic falling movement
                        CheckDetection(&detection);

                        // Check if the piece has collided with another piece or with the boundings
                        ResolveFallingMovement(&detection, &pieceActive);

                        // Check if we fullfilled a line and if so, erase the line and pull down the the lines above
                        CheckCompletion(&lineToDelete);

                        gravityMovementCounter = 0;
                    }

                    // Move laterally at player's will
                    if (lateralMovementCounter >= LATERAL_SPEED)
                    {
                        // Update the lateral movement and if success, reset the lateral counter
                        if (!ResolveLateralMovement()) lateralMovementCounter = 0;
                    }

                    // Turn the piece at player's will
                    if (turnMovementCounter >= TURNING_SPEED)
                    {
                        // Update the turning movement and reset the turning counter
                        if (ResolveTurnMovement()) turnMovementCounter = 0;
                    }
                }

                // Game over logic
                //Verifica se as duas linhas do top estão cheias
                for (int j = 18; j < 20; j++)
                {
                    for (int i = 1; i < Row - 1; i++)
                    {
                        if (grid[i][j] == FULL)
                        {
                            gameOver = true;
                        }
                    }
                }
            }
            else
            {
                // Animation when deleting lines
                fadeLineCounter++;
               
                //Animação do fade-out da remoção da linha
                if (fadeLineCounter % 8 < 4) fadingColor = BLUE;
                else fadingColor = BLUE;

                //Coloca um certo tempo para a realização do fade-out 
                if (fadeLineCounter >= FADING_TIME)
                {
                    int deletedLines = 0;
                    deletedLines = DeleteCompleteLines(); //Armazena as linhas deletadas no tabuleiro
                    if (deletedLines) {
                        targetFPS--;
                        SetTargetFPS(targetFPS);//Serve para acompanhar o progresso do usuário
                    }
                    fadeLineCounter = 0;
                    lineToDelete = false;

                    lines += deletedLines;
                }
            }
        }
    }
    else
    {
        if (IsKeyPressed(KEY_ENTER))
        {
            InitGame();
            gameOver = false;
        }
    }
}

// Draw game (one frame)
void DrawGame(void)
{
    BeginDrawing();

    //ClearBackground(BLUED); //Fundo do jogo

    if (!gameOver)
    {
        // Draw gameplay area
        Vector2 offset; //Representa um vetor bidimensional

        //Antes do -50, está centralizando o grid. Com o -50, está movendo para a esquerda
        offset.x = screenWidth / 2 - (Row * SQUARE_SIZE / 2) - 50;
        // Antes do + SQUARE_SIZE*2, está centralizando na vertical. Depois do + ... está ajustando a posição para cima
        offset.y = screenHeight / 2 - ((Col - 1) * SQUARE_SIZE / 2) + SQUARE_SIZE * 2;

        offset.y -= 50;     // NOTE: Harcoded position!
        
        float controller = offset.x;
        

        for (int j = 0; j < Col; j++)
        {
            for (int i = 0; i < Row; i++)
            {
                // Draw each square of the grid
                //Desenha as linhas do grid
                if (grid[i][j] == EMPTY)
                {
                    DrawLine(offset.x, offset.y, offset.x + SQUARE_SIZE, offset.y, DARKGRAY);
                    DrawLine(offset.x, offset.y, offset.x, offset.y + SQUARE_SIZE, DARKGRAY);
                    DrawLine(offset.x + SQUARE_SIZE, offset.y, offset.x + SQUARE_SIZE, offset.y + SQUARE_SIZE, DARKGRAY);
                    DrawLine(offset.x, offset.y + SQUARE_SIZE, offset.x + SQUARE_SIZE, offset.y + SQUARE_SIZE, DARKGRAY);
                    offset.x += SQUARE_SIZE;
                }
                //Pintará de 'DARKBLUE' os tetrinos já parados
                else if (grid[i][j] == FULL)
                {
                    DrawRectangle(offset.x, offset.y, SQUARE_SIZE, SQUARE_SIZE, DARKGRAY);
                    offset.x += SQUARE_SIZE;
                }
                //Pintará de 'BLUE' os tetrinos em movimento
                else if (grid[i][j] == MOVING)
                {
                    DrawRectangle(offset.x, offset.y, SQUARE_SIZE, SQUARE_SIZE, BLUE);
                    offset.x += SQUARE_SIZE;
                }
                //Pintará de 'SKYBLUE' os tetrinos em movimento
                else if (grid[i][j] == BLOCK)
                {
                    DrawRectangle(offset.x, offset.y, SQUARE_SIZE, SQUARE_SIZE, BLACK);
                    offset.x += SQUARE_SIZE;
                }
                else if (grid[i][j] == FADING)
                {
                    DrawRectangle(offset.x, offset.y, SQUARE_SIZE, SQUARE_SIZE, fadingColor);
                    offset.x += SQUARE_SIZE;
                }
            }

            offset.x = controller;
            offset.y += SQUARE_SIZE;
        }

        // Draw incoming piece (hardcoded)
        offset.x = 500;
        offset.y = 45;

        int controler = offset.x;

        //desenha as peças do jogo
        for (int j = 0; j < 4; j++)
        {
            for (int i = 0; i < 4; i++)
            {
                if (incomingPiece[i][j] == EMPTY)
                {
                    DrawLine(offset.x, offset.y, offset.x + SQUARE_SIZE, offset.y, DARKGRAY);
                    DrawLine(offset.x, offset.y, offset.x, offset.y + SQUARE_SIZE, DARKGRAY);
                    DrawLine(offset.x + SQUARE_SIZE, offset.y, offset.x + SQUARE_SIZE, offset.y + SQUARE_SIZE, DARKGRAY);
                    DrawLine(offset.x, offset.y + SQUARE_SIZE, offset.x + SQUARE_SIZE, offset.y + SQUARE_SIZE, DARKGRAY);
                    offset.x += SQUARE_SIZE;
                }
                else if (incomingPiece[i][j] == MOVING)
                {
                    DrawRectangle(offset.x, offset.y, SQUARE_SIZE, SQUARE_SIZE, DARKGRAY);
                    offset.x += SQUARE_SIZE;
                }
            }

            offset.x = controler;
            offset.y += SQUARE_SIZE;
        }
        DrawText("PRÓXIMO:", offset.x, offset.y - 100, 10, LIGHTGRAY);
        DrawText(TextFormat("LINHAS:      %04i", lines), offset.x, offset.y + 20, 10, LIGHTGRAY);
        DrawText("PRESSIONE 'P' PARA PAUSAR", offset.x, offset.y + 40, 11, LIGHTGRAY);
        DrawText("PRESSIONE 'E' PARA SAIR", offset.x, offset.y + 60, 11, LIGHTGRAY);

        if (pause) DrawText("JOGO PAUSADO", screenWidth / 2 - MeasureText("JOGO PAUSADO", 54) / 2, screenHeight / 2 - 40, 40, WHITE);

    }
    else DrawText("PRESSIONE [ENTER] PARA JOGAR DE NOVO", GetScreenWidth() / 2 - MeasureText("PRESSIONE [ENTER] PARA JOGAR DE NOVO", 20) / 2, GetScreenHeight() / 2 - 50, 20, WHITE);
    EndDrawing();
}

// Update and Draw (one frame)
void UpdateDrawFrame(void)
{
    UpdateGame();
    DrawGame();
}

static bool Createpiece()
{
    piecePositionX = (int)((Row - 4) / 2);
    piecePositionY = Col-5;

    // If the game is starting and you are going to create the first piece, we create an extra one
    if (beginPlay)
    {
        GetRandompiece();
        beginPlay = false;
    }

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            piece[i][j] = incomingPiece[i][j]; //Matriz inicializada com células vazias
        }
    }

    //A peça em movimento, ou seja em atuação no game, são copiados para um local no grid[][], campo do jogo.
    GetRandompiece();

    for (int i = piecePositionX; i < piecePositionX + 4; i++)
    {
        for (int j = piecePositionY; j < piecePositionY + 4; j++)
        {
            if (piece[i - (int)piecePositionX][j - piecePositionY] == MOVING) grid[i][j] = MOVING;
        }
    }

    return true;
}

static void GetRandompiece()
{
    int random = GetRandomValue(0, 6);

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            incomingPiece[i][j] = EMPTY;
        }
    }

    switch (random)
    {
    case 0: { incomingPiece[1][1] = MOVING; incomingPiece[2][1] = MOVING; incomingPiece[1][2] = MOVING; incomingPiece[2][2] = MOVING; } break;    //Cube
    case 1: { incomingPiece[1][1] = MOVING; incomingPiece[1][2] = MOVING; incomingPiece[1][3] = MOVING; incomingPiece[2][3] = MOVING; } break;    //L
    case 2: { incomingPiece[1][3] = MOVING; incomingPiece[2][1] = MOVING; incomingPiece[2][2] = MOVING; incomingPiece[2][3] = MOVING; } break;    //L inversa
    case 3: { incomingPiece[0][1] = MOVING; incomingPiece[1][1] = MOVING; incomingPiece[2][1] = MOVING; incomingPiece[3][1] = MOVING; } break;    //Recta
    case 4: { incomingPiece[1][1] = MOVING; incomingPiece[1][2] = MOVING; incomingPiece[1][3] = MOVING; incomingPiece[2][2] = MOVING; } break;    //Creu tallada
    case 5: { incomingPiece[1][1] = MOVING; incomingPiece[2][1] = MOVING; incomingPiece[2][2] = MOVING; incomingPiece[3][2] = MOVING; } break;    //S
    case 6: { incomingPiece[1][2] = MOVING; incomingPiece[2][2] = MOVING; incomingPiece[2][1] = MOVING; incomingPiece[3][1] = MOVING; } break;    //S inversa
    }
}

static void ResolveFallingMovement(bool* detection, bool* pieceActive)
{
    // If we finished moving this piece, we stop it
    if (*detection)
    {
        for (int j = 1; j < Col - 1; j++)
        {
            for (int i = 1; i < Row - 1; i++)
            {
                if (grid[i][j] == MOVING)
                {
                    grid[i][j] = FULL;
                    *detection = false;
                    *pieceActive = false;
                }
            }
        }
    }
    else    //Movendo a peça para baixo
    {
        for (int j = 1; j < Col-1; j++)
        {
            for (int i = Row-2; i >= 1; i--)
            {
                if (grid[i][j] == MOVING)
                {
                    grid[i][j-1] = MOVING;
                    grid[i][j] = EMPTY;
                }
            }
        }

        piecePositionY--;
    }
}

static bool ResolveLateralMovement()
{
    bool collision = false;

    // Piece movement
    if (IsKeyDown(KEY_LEFT))        // Move left
    {
        // Check if is possible to move to left
        for (int j = Col - 2; j >= 0; j--)
        {
            for (int i = 1; i < Row - 1; i++)
            {
                if (grid[i][j] == MOVING)
                {
                    // Check if we are touching the left wall or we have a full square at the left
                    if ((i - 1 == 0) || (grid[i - 1][j] == FULL)) collision = true;
                }
            }
        }

        // If able, move left
        if (!collision)
        {
            for (int j = Col - 2; j >= 0; j--)
            {
                for (int i = 1; i < Row - 1; i++)             // We check the matrix from left to right
                {
                    // Move everything to the left
                    if (grid[i][j] == MOVING)
                    {
                        grid[i - 1][j] = MOVING;
                        grid[i][j] = EMPTY;
                    }
                }
            }

            piecePositionX--;
        }
    }
    else if (IsKeyDown(KEY_RIGHT))  // Move right
    {
        // Check if is possible to move to right
        for (int j = Col - 2; j >= 0; j--)
        {
            for (int i = 1; i < Row - 1; i++)
            {
                if (grid[i][j] == MOVING)
                {
                    // Check if we are touching the right wall or we have a full square at the right
                    if ((i + 1 == Row - 1) || (grid[i + 1][j] == FULL))
                    {
                        collision = true;

                    }
                }
            }
        }

        // If able move right
        if (!collision)
        {
            for (int j = Col - 2; j >= 0; j--)
            {
                for (int i = Row - 1; i >= 1; i--)             // We check the matrix from right to left
                {
                    // Move everything to the right
                    if (grid[i][j] == MOVING)
                    {
                        grid[i + 1][j] = MOVING;
                        grid[i][j] = EMPTY;
                    }
                }
            }

            piecePositionX++;
        }
    }

    return collision;
}

//Movimento de rotação
static bool ResolveTurnMovement()
{
    // Input for turning the piece
    if (IsKeyDown(KEY_DOWN))
    {
        GridSquare aux;
        bool checker = false;

        // Check all turning possibilities
        if ((grid[piecePositionX + 3][piecePositionY] == MOVING) &&
            (grid[piecePositionX][piecePositionY] != EMPTY) &&
            (grid[piecePositionX][piecePositionY] != MOVING)) checker = true;

        if ((grid[piecePositionX + 3][piecePositionY + 3] == MOVING) &&
            (grid[piecePositionX + 3][piecePositionY] != EMPTY) &&
            (grid[piecePositionX + 3][piecePositionY] != MOVING)) checker = true;

        if ((grid[piecePositionX][piecePositionY + 3] == MOVING) &&
            (grid[piecePositionX + 3][piecePositionY + 3] != EMPTY) &&
            (grid[piecePositionX + 3][piecePositionY + 3] != MOVING)) checker = true;

        if ((grid[piecePositionX][piecePositionY] == MOVING) &&
            (grid[piecePositionX][piecePositionY + 3] != EMPTY) &&
            (grid[piecePositionX][piecePositionY + 3] != MOVING)) checker = true;

        if ((grid[piecePositionX + 1][piecePositionY] == MOVING) &&
            (grid[piecePositionX][piecePositionY + 2] != EMPTY) &&
            (grid[piecePositionX][piecePositionY + 2] != MOVING)) checker = true;

        if ((grid[piecePositionX + 3][piecePositionY + 1] == MOVING) &&
            (grid[piecePositionX + 1][piecePositionY] != EMPTY) &&
            (grid[piecePositionX + 1][piecePositionY] != MOVING)) checker = true;

        if ((grid[piecePositionX + 2][piecePositionY + 3] == MOVING) &&
            (grid[piecePositionX + 3][piecePositionY + 1] != EMPTY) &&
            (grid[piecePositionX + 3][piecePositionY + 1] != MOVING)) checker = true;

        if ((grid[piecePositionX][piecePositionY + 2] == MOVING) &&
            (grid[piecePositionX + 2][piecePositionY + 3] != EMPTY) &&
            (grid[piecePositionX + 2][piecePositionY + 3] != MOVING)) checker = true;

        if ((grid[piecePositionX + 2][piecePositionY] == MOVING) &&
            (grid[piecePositionX][piecePositionY + 1] != EMPTY) &&
            (grid[piecePositionX][piecePositionY + 1] != MOVING)) checker = true;

        if ((grid[piecePositionX + 3][piecePositionY + 2] == MOVING) &&
            (grid[piecePositionX + 2][piecePositionY] != EMPTY) &&
            (grid[piecePositionX + 2][piecePositionY] != MOVING)) checker = true;

        if ((grid[piecePositionX + 1][piecePositionY + 3] == MOVING) &&
            (grid[piecePositionX + 3][piecePositionY + 2] != EMPTY) &&
            (grid[piecePositionX + 3][piecePositionY + 2] != MOVING)) checker = true;

        if ((grid[piecePositionX][piecePositionY + 1] == MOVING) &&
            (grid[piecePositionX + 1][piecePositionY + 3] != EMPTY) &&
            (grid[piecePositionX + 1][piecePositionY + 3] != MOVING)) checker = true;

        if ((grid[piecePositionX + 1][piecePositionY + 1] == MOVING) &&
            (grid[piecePositionX + 1][piecePositionY + 2] != EMPTY) &&
            (grid[piecePositionX + 1][piecePositionY + 2] != MOVING)) checker = true;

        if ((grid[piecePositionX + 2][piecePositionY + 1] == MOVING) &&
            (grid[piecePositionX + 1][piecePositionY + 1] != EMPTY) &&
            (grid[piecePositionX + 1][piecePositionY + 1] != MOVING)) checker = true;

        if ((grid[piecePositionX + 2][piecePositionY + 2] == MOVING) &&
            (grid[piecePositionX + 2][piecePositionY + 1] != EMPTY) &&
            (grid[piecePositionX + 2][piecePositionY + 1] != MOVING)) checker = true;

        if ((grid[piecePositionX + 1][piecePositionY + 2] == MOVING) &&
            (grid[piecePositionX + 2][piecePositionY + 2] != EMPTY) &&
            (grid[piecePositionX + 2][piecePositionY + 2] != MOVING)) checker = true;

        //Gira a peça 90º no sentido horário
        if (!checker)
        {
            aux = piece[0][0];
            piece[0][0] = piece[3][0];
            piece[3][0] = piece[3][3];
            piece[3][3] = piece[0][3];
            piece[0][3] = aux;

            aux = piece[1][0];
            piece[1][0] = piece[3][1];
            piece[3][1] = piece[2][3];
            piece[2][3] = piece[0][2];
            piece[0][2] = aux;

            aux = piece[2][0];
            piece[2][0] = piece[3][2];
            piece[3][2] = piece[1][3];
            piece[1][3] = piece[0][1];
            piece[0][1] = aux;

            aux = piece[1][1];
            piece[1][1] = piece[2][1];
            piece[2][1] = piece[2][2];
            piece[2][2] = piece[1][2];
            piece[1][2] = aux;
        }

        for (int j = Col - 2; j >= 0; j--)
        {
            for (int i = 1; i < Row - 1; i++)
            {
                if (grid[i][j] == MOVING)
                {
                    grid[i][j] = EMPTY;
                }
            }
        }

        //Atualiza a posição da peça no grid do jogo
        for (int i = piecePositionX; i < piecePositionX + 4; i++)
        {
            for (int j = piecePositionY+1; j < piecePositionY + 5; j++)
            {
                if (piece[i - piecePositionX][j - piecePositionY-1] == MOVING)
                {
                    grid[i][j] = MOVING;
                }
            }
        }

        return true;
    }

    return false;
}

static void CheckDetection(bool* detection)
{
    for (int j = 1; j <= Col-2; j++)
    {
        for (int i = 1; i < Row - 1; i++)
        {
            if ((grid[i][j] == MOVING) && ((grid[i][j - 1] == FULL) || (grid[i][j - 1] == BLOCK))) *detection = true;
        }
    }
}

static void CheckCompletion(bool* lineToDelete)
{
    int calculator = 0;

    for (int j = 1; j <= Col - 2; j++)
    {
        calculator = 0;
        for (int i = 1; i < Row - 1; i++)
        {
            // Count each square of the line
            if (grid[i][j] == FULL)
            {
                calculator++;
            }

            // Check if we completed the whole line
            if (calculator == Row - 2)
            {
                *lineToDelete = true;
                calculator = 0;
                // points++;

                // Mark the completed line
                for (int z = 1; z < Row - 1; z++)
                {
                    grid[z][j] = FADING;
                }
            }
        }
    }
}

static int DeleteCompleteLines()
{
    int deletedLines = 0;

    // Erase the completed line
    for (int j = 1; j <= Col-2; j++)
    {
        while (grid[1][j] == FADING)
        {
            for (int i = 1; i < Row - 1; i++)
            {
                grid[i][j] = EMPTY;
            }

            for (int j2 = j + 1; j2 <= Col-2; j2++)
            {
                for (int i2 = 1; i2 < Row - 1; i2++)
                {
                    if (grid[i2][j2] == FULL)
                    {
                        grid[i2][j2 + 1] = FULL;
                        grid[i2][j2] = EMPTY;
                    }
                    else if (grid[i2][j2] == FADING)
                    {
                        grid[i2][j2 + 1] = FADING;
                        grid[i2][j2] = EMPTY;
                    }
                }
            }

            deletedLines++;
        }
    }

    return deletedLines;
}