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
static GridSquare piece[4][4]; //Peça atual
static GridSquare incomingPiece[4][4]; //Próxima peça a entrar no jogo

// Estas variáveis mantêm o registo da posição da peça ativa
static int piecePositionX = 0;
static int piecePositionY = 0;

// Parâmetros do jogo
static Color fadingColor;

static bool beginPlay = true;  // Está varriável é apenas true no início do jogo
static bool pieceActive = false;
static bool detection = false;
static bool lineToDelete = false;

// Estatísticas do jogo
static int level = 1;
static int lines = 0;
static int *points = 0;

// Contadores
static int gravityMovementCounter = 0; //Contador de movimentação para baixo
static int lateralMovementCounter = 0; //Contador de movimentos laterais
static int turnMovementCounter = 0;  //Contador de movimento de rotação
static int fastFallMovementCounter = 0;  //Contador de movimento de queda rápido

static int fadeLineCounter = 0; //Contador das linhas que desapareceu

static int gravitySpeed = 30; //Velocidade de queda
int targetFPS = 60;

static void InitGame(void);         // Inicializa o jogo
static void UpdateGame(void);       // Atualiza o jogo
static void DrawGame(void);         // Desenha o design do jogo
static void UpdateDrawFrame(void);  // Atualiza e desenha

static bool Createpiece(); // Cria uma nova peça adicionando embaixo do campo de jogo
static void GetRandompiece(); // Escolhe uma peça aleatoriamente para a próxima entrada
static void ResolveFallingMovement(bool* detection, bool* pieceActive); // Resolve o movimento da subida da peça
static bool ResolveLateralMovement(); // Resolve o movimento lateral da peça
static bool ResolveTurnMovement(); // Resolve o movimento de rotação da peça
static void CheckDetection(bool* detection);  // Verifica se houve colisão entre as peças
static void CheckCompletion(bool* lineToDelete); // Verifica se uma linha foi completa
static int DeleteCompleteLines(); // Remove as linhas completadas

int main(void)
{
    InitWindow(screenWidth, screenHeight, "Tetris");

    Image Background = LoadImage("./Wallpaper.png");
    ImageResize(&Background, 900, 450);
    Texture2D bgTexture = LoadTextureFromImage(Background);
    UnloadImage(Background);

    Vector2 v = { -50, 0 };

    InitGame();

    SetTargetFPS(targetFPS);

    while (!WindowShouldClose())    // Verifica se o jogador clicou na tecla E ou ESC para sair do jogo
    {
        if (IsKeyPressed('E')) sair = !sair;

        if (sair) {
            CloseWindow();
            return 0;
        }

        UpdateDrawFrame();

        DrawTextureEx(bgTexture, v, 0.0f, 1.0f, WHITE);
    }
    UnloadTexture(bgTexture);
    UnloadTexture(bgTexture);

    CloseWindow();        // Fecha a janela e o contexto OpenGL

    return 0;
}

// Inicialização das variáveis do jogo
void InitGame(void)
{
    // Inicialização das estatística do jogo
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

    // Contadores
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

    // Permite deixar a matriz vazio para inicializar corretamente uma nova peça aleatoriamente, que ocupará o mesmo espaço que ocupou a anterior
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            incomingPiece[i][j] = EMPTY;
        }
    }
}

// Atualização do jogo
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
                    // Cria outra peça
                    pieceActive = Createpiece();

                    fastFallMovementCounter = 0;
                }
                else    // Peça caindo
                {
                    // Atualização de contadores
                    fastFallMovementCounter++;
                    gravityMovementCounter++;
                    lateralMovementCounter++;
                    turnMovementCounter++;

                    // Movimentação das peças
                    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) lateralMovementCounter = LATERAL_SPEED;
                    if (IsKeyPressed(KEY_DOWN)) turnMovementCounter = TURNING_SPEED;

                    // Subindo mais rápido
                    if (IsKeyDown(KEY_UP) && (fastFallMovementCounter >= FAST_FALL_AWAIT_COUNTER))
                    {
                        gravityMovementCounter += gravitySpeed;
                    }

                    if (gravityMovementCounter >= gravitySpeed)
                    {
                        // Movimento básico de subida
                        CheckDetection(&detection);

                        // Verificar se o pedaõ é colidido com outro pedaço ou com a borda
                        ResolveFallingMovement(&detection, &pieceActive);

                        // Verificar se a linha é preenchida completamente, se sim, a linha  é apagada e é puxado a linha debaixo para cima
                        CheckCompletion(&lineToDelete);

                        gravityMovementCounter = 0;
                    }

                    // Movimentação para os lados da peça
                    if (lateralMovementCounter >= LATERAL_SPEED)
                    {
                        // Atualização do movimento lateral e se sucesso, reseta o contador lateral
                        if (!ResolveLateralMovement()) lateralMovementCounter = 0;
                    }

                    // Gira a peça
                    if (turnMovementCounter >= TURNING_SPEED)
                    {
                        // Atualização do movimento de girar e reseta o contador
                        if (ResolveTurnMovement()) turnMovementCounter = 0;
                    }
                }

                // Lógico de fim de jogo
                //Verifica se as duas linhas do top estão cheias
                for (int j = 18; j < 20; j++)
                {
                    for (int i = 1; i < Row - 1; i++)
                    {
                        if (grid[i][j] == FULL)
                        {
                            gameOver = true;
                            break;
                        }
                    }
                }
            }
            else
            {
                // Animação quando deletado as linha(s)
                fadeLineCounter++;

                // Animação do fade-out da remoção da linha
                if (fadeLineCounter % 8 < 4) fadingColor = DARKBLUE;
                else fadingColor = MAROON;

                // Coloca um certo tempo para a realização do fade-out 
                if (fadeLineCounter >= FADING_TIME)
                {
                    int deletedLines = 0;
                    deletedLines = DeleteCompleteLines(); // Armazena as linhas deletadas no tabuleiro
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

// Desenho do jogo
void DrawGame(void)
{
    BeginDrawing();

    ClearBackground(RAYWHITE); // Fundo do jogo

    if (!gameOver)
    {
        // Desenho da área de jogo
        Vector2 offset; // Representa um vetor bidimensional

        // Antes do -50, está centralizando o grid. Com o -50, está movendo para a esquerda
        offset.x = screenWidth / 2 - (Row * SQUARE_SIZE / 2) - 50;
        // Antes do + SQUARE_SIZE*2, está centralizando na vertical. Depois do + ... está ajustando a posição para cima
        offset.y = screenHeight / 2 - ((Col - 1) * SQUARE_SIZE / 2) + SQUARE_SIZE * 2;

        offset.y -= 50;    

        int controller = offset.x;

        for (int j = 0; j < Col; j++)
        {
            for (int i = 0; i < Row; i++)
            {
                // Desenha as linhas do grid
                if (grid[i][j] == EMPTY)
                {
                    DrawLine(offset.x, offset.y, offset.x + SQUARE_SIZE, offset.y, GRAY);
                    DrawLine(offset.x, offset.y, offset.x, offset.y + SQUARE_SIZE, GRAY);
                    DrawLine(offset.x + SQUARE_SIZE, offset.y, offset.x + SQUARE_SIZE, offset.y + SQUARE_SIZE, GRAY);
                    DrawLine(offset.x, offset.y + SQUARE_SIZE, offset.x + SQUARE_SIZE, offset.y + SQUARE_SIZE, GRAY);
                    offset.x += SQUARE_SIZE;
                }
                // Pintará de 'DARKBLUE' os tetrinos já parados
                else if (grid[i][j] == FULL)
                {
                    DrawRectangle(offset.x, offset.y, SQUARE_SIZE, SQUARE_SIZE, DARKGRAY);
                    offset.x += SQUARE_SIZE;
                }
                // Pintará de 'BLUE' os tetrinos em movimento
                else if (grid[i][j] == MOVING)
                {
                    DrawRectangle(offset.x, offset.y, SQUARE_SIZE, SQUARE_SIZE, BLUE);
                    offset.x += SQUARE_SIZE;
                }
                // Pintará de 'SKYBLUE' os tetrinos em movimento
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

        // Desenha as peças de entrada
        offset.x = 500;
        offset.y = 45;

        int controler = offset.x;

        // Desenha as peças do jogo
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
        DrawText(TextFormat("LINHAS:      %04i", points), offset.x, offset.y + 20, 10, LIGHTGRAY);
        DrawText("PRESSIONE 'P' PARA PAUSAR", offset.x, offset.y + 40, 11, LIGHTGRAY);
        DrawText("PRESSIONE 'E' PARA SAIR", offset.x, offset.y + 60, 11, LIGHTGRAY);

        if (pause) DrawText("JOGO PAUSADO", screenWidth / 2 - MeasureText("JOGO PAUSADO", 54) / 2, screenHeight / 2 - 40, 40, WHITE);

    }
    else DrawText("PRESSIONE [ENTER] PARA JOGAR DE NOVO", GetScreenWidth() / 2 - MeasureText("PRESSIONE [ENTER] PARA JOGAR DE NOVO", 20) / 2, GetScreenHeight() / 2 - 50, 20, WHITE);
    EndDrawing();
}

// Atualização e desenho
void UpdateDrawFrame(void)
{
    UpdateGame();
    DrawGame();
}

static bool Createpiece()
{
    piecePositionX = (int)((Row - 4) / 2);
    piecePositionY = Col - 4;

    // Se o jogo iniciar e a peça de entrada já for criada, uma outra peça será gerada
    if (beginPlay)
    {
        GetRandompiece();
        beginPlay = false;
    }
    bool collision = false;
    CheckDetection(&collision);
        if (!collision) {
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    piece[i][j] = incomingPiece[i][j]; // Matriz inicializada com células vazias
                }
            }

            // A peça em movimento, ou seja em atuação no game, são copiados para um local no grid[][], campo do jogo.
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
    case 1: { incomingPiece[1][0] = MOVING; incomingPiece[1][1] = MOVING; incomingPiece[1][2] = MOVING; incomingPiece[2][2] = MOVING; } break;    //L
    case 2: { incomingPiece[1][2] = MOVING; incomingPiece[2][0] = MOVING; incomingPiece[2][1] = MOVING; incomingPiece[2][2] = MOVING; } break;    //L inversa
    case 3: { incomingPiece[0][1] = MOVING; incomingPiece[1][1] = MOVING; incomingPiece[2][1] = MOVING; incomingPiece[3][1] = MOVING; } break;    //Recta
    case 4: { incomingPiece[1][0] = MOVING; incomingPiece[1][1] = MOVING; incomingPiece[1][2] = MOVING; incomingPiece[2][1] = MOVING; } break;    //Creu tallada
    case 5: { incomingPiece[1][1] = MOVING; incomingPiece[2][1] = MOVING; incomingPiece[2][2] = MOVING; incomingPiece[3][2] = MOVING; } break;    //S
    case 6: { incomingPiece[1][2] = MOVING; incomingPiece[2][2] = MOVING; incomingPiece[2][1] = MOVING; incomingPiece[3][1] = MOVING; } break;    //S inversa
    }
}

static void ResolveFallingMovement(bool* detection, bool* pieceActive)
{
    // Se finalizado o movimento das peças, então é parado 
    if (*detection)
    {
        for (int j = Col-2; j >= 0; j--)
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
    else    // Movendo a peça para cima
    {
        for (int j = 1; j < Col - 1; j++)
        {
            for (int i = Row - 2; i >= 1; i--)
            {
                if (grid[i][j] == MOVING)
                {
                    grid[i][j - 1] = MOVING;
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

    // Peça em movimentação

    if (IsKeyDown(KEY_LEFT))        // Move left
    {
        // Verificar se é possível mover para a esquerda 
        for (int j = Col - 2; j >= 0; j--)
        {
            for (int i = 1; i < Row - 1; i++)
            {
                if (grid[i][j] == MOVING)
                {
                    // Verificar se está tocando na borda à esquerda ou se possui um quadrado completo à esquerda 
                    if ((i - 1 == 0) || (grid[i - 1][j] == FULL)) collision = true; break;
                }
            }
        }

        // Se capaz, mover para a esquerda
        if (!collision)
        {
            for (int j = Col - 2; j >= 0; j--)
            {
                for (int i = 1; i < Row - 1; i++)            
                {
                    // Movemr tudo para a esquerda
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
    else if (IsKeyDown(KEY_RIGHT))  // Mover para a direita
    {
        // Verificar se é possível mover para a direita
        for (int j = Col - 2; j >= 0; j--)
        {
            for (int i = 1; i < Row - 1; i++)
            {
                if (grid[i][j] == MOVING)
                {
                    // Verificar se está tocando na borda à direita ou se possui um quadrado completo à direita 
                    if ((i + 1 == Row - 1) || (grid[i + 1][j] == FULL))
                    {
                        collision = true;
                        break;

                    }
                }
            }
        }

        // Se capaz mover para a direita
        if (!collision)
        {
            for (int j = Col - 2; j >= 0; j--)
            {
                for (int i = Row - 1; i >= 1; i--)             
                {
                    // Mover tudo para a direita
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

// Movimento de rotação
static bool ResolveTurnMovement()
{
    // Entrada para a rotação da peça
    if (IsKeyDown(KEY_DOWN))
    {
        GridSquare aux;
        bool checker = false;

        // Verificar todas as possibilidades de rotação
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
        
        // Gira a peça 90º no sentido horário
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

        // Atualiza a posição da peça no grid do jogo
        for (int i = piecePositionX; i < piecePositionX + 4; i++)
        {
            for (int j = piecePositionY; j < piecePositionY + 4; j++)
            {
                if (piece[i - piecePositionX][j - piecePositionY] == MOVING)
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
    for (int j = Col-2; j >= 0; j--)
    {
        for (int i = 1; i < Row - 1; i++)
        {
            if ((grid[i][j] == MOVING) && ((grid[i][j - 1] == FULL) || (grid[i][j - 1] == BLOCK))) * detection = true;
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
            // Conte cada quadrado da linha
            if (grid[i][j] == FULL)
            {
                calculator++;
            }

            // Verificar se toda a linha é completada
            if (calculator == Row - 2)
            {
                *lineToDelete = true;
                calculator = 0;
                *points++;

                // Marcar a linha completada
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
    for (int j = Col-2; j >= 0; j--)
    {
        while (grid[1][j] == FADING)
        {
            for (int i = 1; i < Row - 1; i++)
            {
                grid[i][j] = EMPTY;
            }

            for (int j2 = j+1; j2 <= Col - 2; j2++)
            {
                for (int i2 = 1; i2 < Row - 1; i2++)
                {
                    if (grid[i2][j2] == FULL)
                    {
                        grid[i2][j2 - 1] = FULL;
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
