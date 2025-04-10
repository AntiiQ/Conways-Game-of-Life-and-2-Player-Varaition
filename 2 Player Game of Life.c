#include "raylib.h"

#include <stdio.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <pthread.h>
// 32, 18
// 192, 108
// 960, 540
#define HOR_CELLS (32)
#define VER_CELLS (18)

#define MAX_ITERATIONS 100

#define BASE (Color) {25, 23, 36, 255}
#define ON (Color) {246, 193, 119, 255}
#define PAUSE (Color) {235, 188, 186, 120}
#define LINE (Color) {38, 35, 58, 100}
#define OFF_GRID (Color) {18, 16, 27, 255}  // Slightly darker than BASE color

#define P1_COLOR BLUE
#define P2_COLOR RED

typedef struct{
    bool state;
    bool p2; // true if cell is of player 2 type
}CellState;

typedef struct{
   int total;
   int p2Num; // true if majority of neighbours are player 2 
}Neighbours;


CellState gridA[VER_CELLS][HOR_CELLS] = {0};
CellState gridB[VER_CELLS][HOR_CELLS] = {0};

// Pointers to the active and next grid
CellState (*grid)[HOR_CELLS] = gridA;
CellState (*nextGrid)[HOR_CELLS] = gridB;


float zoomLevel = 1.0f;            // 1.0 = no zoom, >1.0 = zoomed in, <1.0 = zoomed out
Vector2 cameraPosition = {0, 0};   // Offset for panning (in screen coordinates)
Vector2 lastMousePosition = {0, 0}; // For tracking mouse movement while panning
bool isPanning = false;            // Flag for when user is panning

float gridLeft, gridTop, zoomedWidth, zoomedHeight, cellWidthScaled, cellHeightScaled;

float horizontalDiff;
float verticalDiff;
bool paused = false;

//RenderTexture2D gridTexture;

Image gridImage;
Texture2D gridTex;
Color *pixels;

float cellSize;

#define NUM_THREADS 2

pthread_t threads[NUM_THREADS];

int rowsPerThread = VER_CELLS / NUM_THREADS;

typedef struct {
    int startRow;
    int endRow;
} ThreadArgs;

ThreadArgs threadArgs[NUM_THREADS];

void CopyGrid(void){
    //int n = sizeof(nextGrid) / sizeof(nextGrid[0]);
    //memcpy(grid, nextGrid, n * sizeof(nextGrid[0]));

    //swap grid pointers
    CellState (*temp)[HOR_CELLS] = grid;
    grid = nextGrid;
    nextGrid = temp;
}

// counts the value above and bellow the coordiante given
// returns 0 if its out of bounds
// Used for a slidning window algorithm
Neighbours CountSize3Column(int x, int y){
    int value = 0;
    int type = 0;
    
    // Only count if x is within bounds
    if (x >= 0 && x < HOR_CELLS) {
        // Count center cell if y is in bounds
        if (y >= 0 && y < VER_CELLS) {
            value += grid[y][x].state;
            type  += grid[y][x].p2;
        }
        
        // Count cell above if it's in bounds
        if (y - 1 >= 0) {
            value += grid[y-1][x].state;
            type  += grid[y-1][x].p2;
        }
        
        // Count cell below if it's in bounds
        if (y + 1 < VER_CELLS) {
            value += grid[y+1][x].state;
            type  += grid[y+1][x].p2;
        }
    }
    
    return (Neighbours) {value, type};
}

CellState NextCellState(CellState state, Neighbours TotalNeighbours){
    bool p2Majority = false;
    // if a cell has equal number of neighbours, it randomises color
    if (TotalNeighbours.total == 2 && TotalNeighbours.p2Num == 1){
        return (CellState) {state.state, GetRandomValue(0, 1)};
    } 
    // determine if a cell has more p2 neighbours
    if (TotalNeighbours.total - TotalNeighbours.p2Num < TotalNeighbours.p2Num){
        p2Majority = true;
    }

    if (state.state == true && (TotalNeighbours.total == 2 || TotalNeighbours.total == 3)){
        return (CellState) {true, p2Majority};
    }else if (state.state == false && TotalNeighbours.total == 3){
        return (CellState) {true, p2Majority};
    }else{
        return (CellState) {0, 0};
    }
}

// Update these calculations in one place that runs before rendering
void UpdateGridTransformation(void) {
    // Calculate grid dimensions based on zoom
    zoomedWidth = HOR_CELLS * cellSize * zoomLevel;
    zoomedHeight = VER_CELLS * cellSize * zoomLevel;
    
    // Calculate grid position (top-left corner)
    float screenCenterX = GetScreenWidth() / 2.0f;
    float screenCenterY = GetScreenHeight() / 2.0f;
    gridLeft = screenCenterX - (zoomedWidth / 2.0f) + cameraPosition.x;
    gridTop = screenCenterY - (zoomedHeight / 2.0f) + cameraPosition.y;
    
    // Calculate individual cell size at current zoom
    cellWidthScaled = zoomedWidth / HOR_CELLS;
    cellHeightScaled = zoomedHeight / VER_CELLS;
}

void DisplayGrid(void) {
    /*
    // Draw the entire grid with a single operation
    DrawTexture(gridTexture.texture, 0, 0, WHITE);
    */

    DrawTexturePro(gridTex,
        (Rectangle){0, 0, HOR_CELLS, -VER_CELLS}, // flip Y axis (source)
        (Rectangle){gridLeft, gridTop, zoomedWidth, zoomedHeight}, // destination
        (Vector2){0, 0},
        0.0f,
        WHITE);
}

void CleanUp(void) {
    //UnloadRenderTexture(gridTexture);
    free(pixels);
}

void InitializeGridTexture(void) {
    /*
    // Create a texture the size of the window
    gridTexture = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    
    // Initialize with a blank (white) background
    BeginTextureMode(gridTexture);
    ClearBackground(BASE);
    EndTextureMode();
    */

    gridImage = GenImageColor(HOR_CELLS, VER_CELLS, BASE); // logical size = 1 pixel per cell
    gridTex = LoadTextureFromImage(gridImage);
    UnloadImage(gridImage); 

    // Force pixelated scaling (important!)
    SetTextureFilter(gridTex, TEXTURE_FILTER_POINT);
    pixels = malloc(HOR_CELLS * VER_CELLS * sizeof(Color));
}

void UpdateGridTexture(void) {
    /*
    BeginTextureMode(gridTexture);
    ClearBackground(BASE);
    
    // Draw only alive cells to the texture
    for (int y = 0; y < VER_CELLS; y++) {
        for (int x = 0; x < HOR_CELLS; x++) {
            if (grid[y][x]) {
                DrawRectangle(x * horizontalDiff, y * verticalDiff, 
                              horizontalDiff, verticalDiff, ON);
            }
        }
    }
    
    EndTextureMode();
    */
    Color color;
    for (int y = 0; y < VER_CELLS; y++) {
        for (int x = 0; x < HOR_CELLS; x++) {
            if (grid[y][x].state == false) 
                color = BASE; 
            else if (grid[y][x].p2) 
                color = P2_COLOR;
            else 
                color = P1_COLOR;

            pixels[y * HOR_CELLS + x] = color;
        }
    }
    
    UpdateTexture(gridTex, pixels);
}


void* ThreadTask(void* arg){

    //puts("ThreadTask started");
    ThreadArgs* args = (ThreadArgs*)arg;
    int startRow = args->startRow;
    int endRow = args->endRow;

    Neighbours firstColumn = {0, 0}, secondColumn = {0, 0}, thirdColumn = {0, 0};
    Neighbours total;

    for (int y = startRow; y < endRow; y++){
        //puts("YYY");
        thirdColumn = CountSize3Column(0,y);
        firstColumn = secondColumn =  (Neighbours) {0, 0};

        for(int x = 0; x < HOR_CELLS; x++){
            //puts("XXXXXXXXX");
            CellState state = grid[y][x];
            firstColumn = secondColumn;
            secondColumn = thirdColumn;
            thirdColumn = CountSize3Column(x + 1, y);

            total.total =  firstColumn.total + (secondColumn.total - state.state) + thirdColumn.total;
            total.p2Num = firstColumn.p2Num + (secondColumn.p2Num - state.p2) + thirdColumn.p2Num;

            nextGrid[y][x] = NextCellState(state, total);
        }
    }


    return NULL;
}

void ThreadedIteration(void){
    //ThreadArgs* threadArgs = malloc(NUM_THREADS * sizeof(ThreadArgs));
    
    for (int i = 0; i < NUM_THREADS; i++){
        threadArgs[i].startRow = i * rowsPerThread;
        threadArgs[i].endRow = (i == NUM_THREADS - 1) ? VER_CELLS : (i + 1) * rowsPerThread;
        
        if (pthread_create(&threads[i], NULL, ThreadTask, &threadArgs[i]) != 0){
            puts("Thread creation failed");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_THREADS; i++){
        pthread_join(threads[i], NULL);
    }

    //free(threadArgs);
    CopyGrid();
}



// Add this to handle zoom/pan input:
void handleZoomAndPan(void) {
    // Zoom controls
    float mouseWheel = GetMouseWheelMove();
    if (mouseWheel != 0) {
        // Get mouse position before zoom for better zooming at cursor
        Vector2 mousePos = GetMousePosition();
        
        // Calculate position relative to grid center
        float screenCenterX = GetScreenWidth() / 2.0f;
        float screenCenterY = GetScreenHeight() / 2.0f;
        Vector2 mousePosRelative = {
            (mousePos.x - screenCenterX - cameraPosition.x) / zoomLevel,
            (mousePos.y - screenCenterY - cameraPosition.y) / zoomLevel
        };
        
        // Adjust zoom level
        zoomLevel += mouseWheel * 0.1f * zoomLevel; // Proportional zoom
        
        
        // Lower limit: Allow zooming out to see the entire grid
        float minZoom = fminf(
            (float)GetScreenWidth() / (HOR_CELLS * cellSize),
            (float)GetScreenHeight() / (VER_CELLS * cellSize)
        ) * 1; //0.9f; // 90% of the size that would fit the entire grid
        
        // Upper limit:
        float maxZoom = 500.0f / cellSize;
        
        // Apply limits
        zoomLevel = fmaxf(minZoom, fminf(maxZoom, zoomLevel));
        
        // Adjust camera to zoom at cursor position
        cameraPosition.x = mousePos.x - screenCenterX - mousePosRelative.x * zoomLevel;
        cameraPosition.y = mousePos.y - screenCenterY - mousePosRelative.y * zoomLevel;
    }
    
    // Pan controls
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        isPanning = true;
        lastMousePosition = GetMousePosition();
    }
    
    if (IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) {
        isPanning = false;
    }
    
    if (isPanning) {
        Vector2 currentMousePos = GetMousePosition();
        cameraPosition.x += (currentMousePos.x - lastMousePosition.x);
        cameraPosition.y += (currentMousePos.y - lastMousePosition.y);
        lastMousePosition = currentMousePos;
    }
    
    // Reset view with 'R' key
    if (IsKeyPressed(KEY_R)) {
        zoomLevel = 1.0f;
        cameraPosition = (Vector2){0, 0};
    }
}

void NewGame(void){
    memset(gridA, 0, sizeof(gridA));
    memset(gridB, 0, sizeof(gridB));
}

void playerInput(void){

}

void Draw(const char* string){
    BeginDrawing();

    ClearBackground(OFF_GRID);
    DrawRectangle(
        gridLeft, 
        gridTop, 
        zoomedWidth, 
        zoomedHeight, 
        BASE
    );

    UpdateGridTexture();  // Update the texture with the new grid state
    DisplayGrid();

    float lineThickness = fmax(1.0f, zoomLevel * 0.5f);

    // Draw horizontal grid lines
    for (int y = 0; y <= VER_CELLS; y++) {
        float lineY = gridTop + y * cellHeightScaled;
        Vector2 startPos = {gridLeft, lineY};
        Vector2 endPos = {gridLeft + zoomedWidth, lineY};
        DrawLineEx(startPos, endPos, lineThickness, LINE);
    }

    // Draw vertical grid lines
    for (int x = 0; x <= HOR_CELLS; x++) {
        float lineX = gridLeft + x * cellWidthScaled;
        Vector2 startPos = {lineX, gridTop};
        Vector2 endPos = {lineX, gridTop + zoomedHeight};
        DrawLineEx(startPos, endPos, lineThickness, LINE);
    }

    DrawText(string, 10, 0, 50, ON);

    EndDrawing();

}

void P1Start(int moves){
    bool NextTurn = false;
    char string[64];
    while (!WindowShouldClose() && !NextTurn){
        strcpy(string, TextFormat("Player 1's Starting Turn. Moves Left: %d. Space To Continue.", moves));
        playerInput();
        handleZoomAndPan();
        UpdateGridTransformation();

        if (IsKeyPressed(KEY_SPACE)){
            NextTurn = true;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !isPanning) {

            Vector2 mousePos = GetMousePosition();
        
            // Convert mouse position to grid coordinates
            int x = (int)((mousePos.x - gridLeft) / cellWidthScaled);
            int y = VER_CELLS - 1 - (int)((mousePos.y - gridTop) / cellHeightScaled);
            
            // Check if within bounds
            if (x >= 0 && x < HOR_CELLS && y >= 0 && y < VER_CELLS) {
                if (grid[y][x].state &&  grid[y][x].p2 == false){
                    grid[y][x].state = false;
                    grid[y][x].p2 = false;
                    moves++;
                }

                else if (grid[y][x].state == false  && moves > 0){
                    grid[y][x].state = true;
                    grid[y][x].p2 = false;
                    moves--;
                }
            }
        }

        Draw(string);
        
    }
}

void P2Start(int moves){
    bool NextTurn = false;
    char string[64];
    while (!WindowShouldClose() && !NextTurn){
        strcpy(string, TextFormat("Player 2's Starting Turn. Moves Left: %d. Space To Continue.", moves));
        playerInput();
        handleZoomAndPan();
        UpdateGridTransformation();

        if (IsKeyPressed(KEY_SPACE)){
            NextTurn = true;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !isPanning) {

            Vector2 mousePos = GetMousePosition();
        
            // Convert mouse position to grid coordinates
            int x = (int)((mousePos.x - gridLeft) / cellWidthScaled);
            int y = VER_CELLS - 1 - (int)((mousePos.y - gridTop) / cellHeightScaled);
            
            // Check if within bounds
            if (x >= 0 && x < HOR_CELLS && y >= 0 && y < VER_CELLS) {
                if (grid[y][x].state &&  grid[y][x].p2 == true){
                    grid[y][x].state = false;
                    grid[y][x].p2 = false;
                    moves++;
                }

                else if (grid[y][x].state == false  && moves > 0){
                    grid[y][x].state = true;
                    grid[y][x].p2 = true;
                    moves--;
                }
            }
        }

        Draw(string);
  
    }
}

void P1Add(){
    bool NextTurn = false;
    char string[64];
    while (!WindowShouldClose() && !NextTurn){
        strcpy(string, TextFormat("Player 1's Turn - Part I. Add a blue cell. Space To Continue."));
        playerInput();
        handleZoomAndPan();
        UpdateGridTransformation();

        if (IsKeyPressed(KEY_SPACE)){
            NextTurn = true;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !isPanning) {

            Vector2 mousePos = GetMousePosition();
        
            // Convert mouse position to grid coordinates
            int x = (int)((mousePos.x - gridLeft) / cellWidthScaled);
            int y = VER_CELLS - 1 - (int)((mousePos.y - gridTop) / cellHeightScaled);
            
            // Check if within bounds
            if (x >= 0 && x < HOR_CELLS && y >= 0 && y < VER_CELLS) {
                if (!(grid[y][x].state == true &&  grid[y][x].p2 == false)){
                    grid[y][x].state = true;
                    grid[y][x].p2 = false;
                    NextTurn = true;
                }
            }
        }

        Draw(string);
        
    }
}

void P2Add(){
    bool NextTurn = false;
    char string[64];
    while (!WindowShouldClose() && !NextTurn){
        strcpy(string, TextFormat("Player 2's Turn - Part I. Add a red cell. Space To Continue."));
        playerInput();
        handleZoomAndPan();
        UpdateGridTransformation();

        if (IsKeyPressed(KEY_SPACE)){
            NextTurn = true;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !isPanning) {

            Vector2 mousePos = GetMousePosition();
        
            // Convert mouse position to grid coordinates
            int x = (int)((mousePos.x - gridLeft) / cellWidthScaled);
            int y = VER_CELLS - 1 - (int)((mousePos.y - gridTop) / cellHeightScaled);
            
            // Check if within bounds
            if (x >= 0 && x < HOR_CELLS && y >= 0 && y < VER_CELLS) {
                if (!(grid[y][x].state == true &&  grid[y][x].p2 == true)){
                    grid[y][x].state = true;
                    grid[y][x].p2 = true;
                    NextTurn = true;
                }
            }
        }

        Draw(string);
        
    }
}

void P1Remove(){
    bool NextTurn = false;
    bool moveDone = false;
    char string[64];
    while (!WindowShouldClose() && !NextTurn){
        strcpy(string, TextFormat("Player 1's Turn - Part II. Remove any one cell. Space To Continue."));
        playerInput();
        handleZoomAndPan();
        UpdateGridTransformation();

        if (IsKeyPressed(KEY_SPACE)){
            NextTurn = true;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !isPanning && !moveDone) {

            Vector2 mousePos = GetMousePosition();
        
            // Convert mouse position to grid coordinates
            int x = (int)((mousePos.x - gridLeft) / cellWidthScaled);
            int y = VER_CELLS - 1 - (int)((mousePos.y - gridTop) / cellHeightScaled);
            
            // Check if within bounds
            if (x >= 0 && x < HOR_CELLS && y >= 0 && y < VER_CELLS) {
                if (grid[y][x].state){
                    grid[y][x].state = false;
                    grid[y][x].p2 = false;
                    moveDone = true;
                }
            }
        }

        Draw(string);
        
    }
}

void P2Remove(){
    bool NextTurn = false;
    bool moveDone = false;
    char string[64];
    while (!WindowShouldClose() && !NextTurn){
        strcpy(string, TextFormat("Player 2's Turn - Part II. Remove any one cell. Space To Continue."));
        playerInput();
        handleZoomAndPan();
        UpdateGridTransformation();

        if (IsKeyPressed(KEY_SPACE)){
            NextTurn = true;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !isPanning && !moveDone) {

            Vector2 mousePos = GetMousePosition();
        
            // Convert mouse position to grid coordinates
            int x = (int)((mousePos.x - gridLeft) / cellWidthScaled);
            int y = VER_CELLS - 1 - (int)((mousePos.y - gridTop) / cellHeightScaled);
            
            // Check if within bounds
            if (x >= 0 && x < HOR_CELLS && y >= 0 && y < VER_CELLS) {
                if (grid[y][x].state){
                    grid[y][x].state = false;
                    grid[y][x].p2 = true;
                    moveDone = true;
                }
            }
        }

        Draw(string);
        
    }
}

bool CheckVictory(int* winner) {
    int totalAliveCells = 0;
    int p1Cells = 0;
    int p2Cells = 0;
    
    // Count cells by type
    for (int y = 0; y < VER_CELLS; y++) {
        for (int x = 0; x < HOR_CELLS; x++) {
            if (grid[y][x].state) {
                totalAliveCells++;
                if (grid[y][x].p2) {
                    p2Cells++;
                } else {
                    p1Cells++;
                }
            }
        }
    }
    
    // If no cells alive, it's a draw
    if (totalAliveCells == 0) {
        *winner = 0; // 0 for draw
        return true;
    }
    
    // If all alive cells are of a single type, that player wins
    if (p1Cells > 0 && p2Cells == 0) {
        *winner = 1; // Player 1 wins
        return true;
    } else if (p2Cells > 0 && p1Cells == 0) {
        *winner = 2; // Player 2 wins
        return true;
    }
    
    return false;
}

void Intro(void){

    float delay = 2.0f;

    for (int i = 0; i < HOR_CELLS; i++){
        grid[0][i].state = true;
        grid[0][i].p2 = false;
        grid[VER_CELLS -1][i].state = true;
        grid[VER_CELLS -1][i].p2 = true;
    }
    for (int i = 0; i < VER_CELLS; i++){
        grid[i][0].state = true;
        grid[i][0].p2 = false;
        grid[i][HOR_CELLS - 1].state = true;
        grid[i][HOR_CELLS - 1].p2 = true;
    }


    bool start = false;
    while(!WindowShouldClose() && !start){
        if (IsKeyPressed(KEY_ENTER)){
            start = true;
        }

        
        BeginDrawing();

        UpdateGridTransformation();
        ClearBackground(OFF_GRID);
        DrawRectangle(
            gridLeft, 
            gridTop, 
            zoomedWidth, 
            zoomedHeight, 
            BASE
        );
        UpdateGridTexture();  // Update the texture with the new grid state
        DisplayGrid();

        DrawText("2 Player Game of Life", 
            GetScreenWidth()/2 - MeasureText("2 Player Game of Life", 60)/2, 
            GetScreenHeight()/2 - 30, 
            60, 
            ON);
    
            
        DrawText("Press ENTER for new game", 
            GetScreenWidth()/2 - MeasureText("Press ENTER for new game", 30)/2, 
            GetScreenHeight()/2 + 40, 
            30, 
            WHITE);
            

        EndDrawing();

        if (delay < 0.0f){
            ThreadedIteration();
            delay = 0.5f;
        }
        delay -= GetFrameTime();
    }
    NewGame();

}

int main(void){
    InitWindow(GetMonitorWidth(GetCurrentMonitor()), GetMonitorHeight(GetCurrentMonitor()), "Game of Life");
    SetTargetFPS(144);
    horizontalDiff = GetScreenWidth() / HOR_CELLS;
    verticalDiff = GetScreenHeight() / VER_CELLS;

    cellSize = sqrt((GetScreenHeight() * GetScreenWidth())/(VER_CELLS * HOR_CELLS));


    SetRandomSeed(time(NULL));
    InitializeGridTexture();

    Intro();

    bool gameOver = false;
    int winner = 0;

    P1Start(5);
    P2Start(5);
    ThreadedIteration();

    if (CheckVictory(&winner)){
        UpdateGridTransformation();
        UpdateGridTexture();

        gameOver = true;
    }


    while (!WindowShouldClose()) {
        if (!gameOver) {
            // Players' turns
            P1Add();
            if (WindowShouldClose()) break;
            P1Remove();
            if (WindowShouldClose()) break;
            
            P2Add();
            if (WindowShouldClose()) break;
            P2Remove();
            if (WindowShouldClose()) break;

            ThreadedIteration();

            // Check for victory after a certain number of iterations
            if (CheckVictory(&winner)) {
                UpdateGridTransformation();
                UpdateGridTexture();

                gameOver = true;
                continue;
            }
                 
            
        } else {
            // Game is over, display the winner
            playerInput();
            handleZoomAndPan();
            UpdateGridTransformation();
            
            // Draw the final state
            BeginDrawing();
            ClearBackground(OFF_GRID);
            
            DisplayGrid();
            
            // Display winner message
            const char* winMessage;
            if (winner == 0) {
                winMessage = "DRAW - All cells died";
            } else if (winner == 1) {
                winMessage = "PLAYER 1 WINS!";
            } else {
                winMessage = "PLAYER 2 WINS!";
            }
            
            DrawText(winMessage, 
                    GetScreenWidth()/2 - MeasureText(winMessage, 60)/2, 
                    GetScreenHeight()/2 - 30, 
                    60, 
                    winner == 1 ? P1_COLOR : (winner == 2 ? P2_COLOR : WHITE));
            
            DrawText("Press ENTER for new game", 
                    GetScreenWidth()/2 - MeasureText("Press ENTER for new game", 30)/2, 
                    GetScreenHeight()/2 + 40, 
                    30, 
                    WHITE);
            
            EndDrawing();
            
            // Check for new game
            if (IsKeyPressed(KEY_ENTER)) {
                NewGame();
                gameOver = false;
                winner = 0;

                P1Start(5);
                P2Start(5);
                ThreadedIteration();
                if (CheckVictory(&winner)){
                    UpdateGridTransformation();
                    UpdateGridTexture();
            
                    gameOver = true;
                }
            }
        }
    }
    CleanUp();
    CloseWindow();
    return 0;
}