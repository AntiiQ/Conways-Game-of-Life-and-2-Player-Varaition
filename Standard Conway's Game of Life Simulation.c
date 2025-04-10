#include "raylib.h"

#include <stdio.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <pthread.h>

//Chance for a cell to be intisalised as alive
#define CHANCE 60

#define BASE (Color) {25, 23, 36, 255}
#define ON (Color) {246, 193, 119, 255}
#define PAUSE (Color) {235, 188, 186, 120}
#define LINE (Color) {38, 35, 58, 100}
#define OFF_GRID (Color) {18, 16, 27, 255}  // Slightly darker than BASE color

int horCells, verCells;


// Pointers to the active and next grid
bool (*grid);
bool (*nextGrid);

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

double displayTime, maxThreadTime, processingTime;

#define NUM_THREADS 24

pthread_t threads[NUM_THREADS];

int rowsPerThread;

typedef struct {
    int startRow;
    int endRow;
} ThreadArgs;

ThreadArgs threadArgs[NUM_THREADS];

void CopyGrid(void){
    //int n = sizeof(nextGrid) / sizeof(nextGrid[0]);
    //memcpy(grid, nextGrid, n * sizeof(nextGrid[0]));

    //swap grid pointers
    bool *temp = grid;
    grid = nextGrid;
    nextGrid = temp;
}

// counts the value above and bellow the coordiante given
// returns 0 if its out of bounds
// Used for a slidning window algorithm
int CountSize3Column(int x, int y){
    int value = 0;
    
    // Only count if x is within bounds
    if (x >= 0 && x < horCells) {
        // Count center cell if y is in bounds
        if (y >= 0 && y < verCells) {
            value += grid[y * horCells + x];
        }
        
        // Count cell above if it's in bounds
        if (y - 1 >= 0) {
            value += grid[(y-1) * horCells + x];
        }
        
        // Count cell below if it's in bounds
        if (y + 1 < verCells) {
            value += grid[(y+1) * horCells + x];
        }
    }
    
    return value;
}

bool NextCellState(bool state, int neighbourNumber){
    if (state == true && (neighbourNumber == 2 || neighbourNumber == 3)){
        return true;
    }else if (state == false && neighbourNumber == 3){
        return true;
    }else{
        return false;
    }
}

// Update these calculations in one place that runs before rendering
void UpdateGridTransformation(void) {
    // Calculate grid dimensions based on zoom
    zoomedWidth = horCells * cellSize * zoomLevel;
    zoomedHeight = verCells * cellSize * zoomLevel;
    
    // Calculate grid position (top-left corner)
    float screenCenterX = GetScreenWidth() / 2.0f;
    float screenCenterY = GetScreenHeight() / 2.0f;
    gridLeft = screenCenterX - (zoomedWidth / 2.0f) + cameraPosition.x;
    gridTop = screenCenterY - (zoomedHeight / 2.0f) + cameraPosition.y;
    
    // Calculate individual cell size at current zoom
    cellWidthScaled = zoomedWidth / horCells;
    cellHeightScaled = zoomedHeight / verCells;
}

void DisplayGrid(void) {
    /*
    // Draw the entire grid with a single operation
    DrawTexture(gridTexture.texture, 0, 0, WHITE);
    */

    DrawTexturePro(gridTex,
        (Rectangle){0, 0, horCells, -verCells}, // flip Y axis (source)
        (Rectangle){gridLeft, gridTop, zoomedWidth, zoomedHeight}, // destination
        (Vector2){0, 0},
        0.0f,
        WHITE);
}

void CleanUp(void) {
    //UnloadRenderTexture(gridTexture);
    free(pixels);
    free(grid);
    free(nextGrid);
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

    gridImage = GenImageColor(horCells, verCells, BASE); // logical size = 1 pixel per cell
    gridTex = LoadTextureFromImage(gridImage);
    UnloadImage(gridImage); // Don't need CPU-side image anymore

    // Force pixelated scaling (important!)
    SetTextureFilter(gridTex, TEXTURE_FILTER_POINT);
    pixels = malloc(horCells * verCells * sizeof(Color));
}

void UpdateGridTexture(void) {
    /*
    BeginTextureMode(gridTexture);
    ClearBackground(BASE);
    
    // Draw only alive cells to the texture
    for (int y = 0; y < verCells; y++) {
        for (int x = 0; x < horCells; x++) {
            if (grid[y][x]) {
                DrawRectangle(x * horizontalDiff, y * verticalDiff, 
                              horizontalDiff, verticalDiff, ON);
            }
        }
    }
    
    EndTextureMode();
    */

    for (int y = 0; y < verCells; y++) {
        for (int x = 0; x < horCells; x++) {
            pixels[y * horCells + x] = grid[y * horCells + x] ? ON : BASE;
        }
    }
    
    
    UpdateTexture(gridTex, pixels);
}

void InitialiseRandomGrid(void){

    for (int y = 0; y < verCells; y++){
        for(int x = 0; x < horCells; x++){
            grid[y * horCells + x] = GetRandomValue(0, 100) > CHANCE? true: false;
        }
    }
    UpdateGridTexture();
}

void* ThreadTask(void* arg){

    double tempTime = GetTime();

    //puts("ThreadTask started");
    ThreadArgs* args = (ThreadArgs*)arg;
    int startRow = args->startRow;
    int endRow = args->endRow;

    int firstColumn = 0, secondColumn = 0, thirdColumn = 0;

    for (int y = startRow; y < endRow; y++){
        //puts("YYY");
        thirdColumn = CountSize3Column(0,y);
        firstColumn = secondColumn =  0;

        for(int x = 0; x < horCells; x++){
            //puts("XXXXXXXXX");
            bool state = grid[y * horCells + x];
            firstColumn = secondColumn;
            secondColumn = thirdColumn;
            thirdColumn = CountSize3Column(x + 1, y);

            nextGrid[y * horCells + x] = NextCellState(state, firstColumn + (secondColumn - state)+ thirdColumn);
        }
    }

    if (GetTime() - tempTime > maxThreadTime){
        maxThreadTime = GetTime() - tempTime;
    }
    return NULL;
}

void ThreadedIteration(void){
    //ThreadArgs* threadArgs = malloc(NUM_THREADS * sizeof(ThreadArgs));
    
    for (int i = 0; i < NUM_THREADS; i++){
        threadArgs[i].startRow = i * rowsPerThread;
        threadArgs[i].endRow = (i == NUM_THREADS - 1) ? verCells : (i + 1) * rowsPerThread;
        
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

void DisplayTimings(void) {
    const int fontSize = 20;
    int y = GetScreenHeight() - (fontSize * 4);  // two lines, no padding

    char displayTimeText[64];
    char threadTimeText[64];
    char frameTimeText[64];
    char processingTimeText[64];

    snprintf(displayTimeText, sizeof(displayTimeText), "Draw Time: %.3f ms", displayTime * 1000.0);
    snprintf(processingTimeText, sizeof(processingTimeText), "Processing Time: %.3f ms", processingTime * 1000.0);
    snprintf(threadTimeText, sizeof(threadTimeText), "Max Thread Time: %.3f ms", maxThreadTime * 1000.0);
    snprintf(frameTimeText, sizeof(frameTimeText), "Frame Time: %.3f ms", GetFrameTime() * 1000.0);

    DrawText(displayTimeText, 10, y, fontSize, RAYWHITE);
    DrawText(processingTimeText, 10, y + fontSize, fontSize, RAYWHITE);
    DrawText(threadTimeText, 10, y + fontSize * 2, fontSize, RAYWHITE);
    DrawText(frameTimeText, 10, y + fontSize * 3, fontSize, RAYWHITE);
}

int map(int value, int fromLow, int fromHigh, int toLow, int toHigh) {
    if (fromHigh == fromLow) return toLow; // Avoid division by zero

    int output = (value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow;

    // Clamp correctly regardless of direction
    if (toLow < toHigh) {
        if (output < toLow) output = toLow;
        if (output > toHigh) output = toHigh;
    } else {
        if (output > toLow) output = toLow;
        if (output < toHigh) output = toHigh;
    }

    return output;
}

void TurnOnEdges(void) {
    // Top and bottom rows
    for (int i = 0; i < horCells; i++) {
        grid[0 * horCells + i] = true;                  // Top row
        grid[(verCells - 1) * horCells + i] = true;     // Bottom row
    }
    // Left and right columns
    for (int i = 0; i < verCells; i++) {
        grid[i * horCells + 0] = true;                  // Left column
        grid[i * horCells + (horCells - 1)] = true;     // Right column
    }
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
            (float)GetScreenWidth() / (horCells * cellSize),
            (float)GetScreenHeight() / (verCells * cellSize)
        ) * 1; //0.9f; // 90% of the size that would fit the entire grid
        
        // Upper limit: Don't allow zooming in beyond 30 pixels per cell
        // This prevents pixelation while still allowing close inspection
        float maxZoom = 500.0f / cellSize;
        
        // Apply limits
        zoomLevel = fmaxf(minZoom, fminf(maxZoom, zoomLevel));
        
        // Adjust camera to zoom at cursor position
        cameraPosition.x = mousePos.x - screenCenterX - mousePosRelative.x * zoomLevel;
        cameraPosition.y = mousePos.y - screenCenterY - mousePosRelative.y * zoomLevel;
    }
    
    // Pan controls - same as before
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

void playerInput(void){
    if (IsKeyDown(KEY_ENTER)){
        InitialiseRandomGrid();
    }
    if (IsKeyDown(KEY_BACKSPACE)){
        memset(grid, 0, verCells * horCells * sizeof(bool));
        memset(nextGrid, 0, verCells * horCells * sizeof(bool));
    }
    if (IsKeyDown(KEY_E)){
        TurnOnEdges();
    }

    if (IsKeyPressed(KEY_SPACE)){
        paused = !paused;
    }
    if (IsKeyPressed(KEY_ZERO)){
        SetTargetFPS(10);
    }
    if (IsKeyPressed(KEY_ONE)){
        SetTargetFPS(30);
    }
    if (IsKeyPressed(KEY_TWO)){
        SetTargetFPS(144);
    }
    if (IsKeyPressed(KEY_THREE)){
        SetTargetFPS(1440);
    }

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !isPanning) {
        Vector2 mousePos = GetMousePosition();
    
        // Convert mouse position to grid coordinates
        int x = (int)((mousePos.x - gridLeft) / cellWidthScaled);
        int y = verCells - 1 - (int)((mousePos.y - gridTop) / cellHeightScaled);
        
        // Check if within bounds
        if (x >= 0 && x < horCells && y >= 0 && y < verCells) {
            grid[y * horCells + x] = true;
        }
    }
    if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON) && !isPanning) {
        Vector2 mousePos = GetMousePosition();
    
        // Convert mouse position to grid coordinates
        int x = (int)((mousePos.x - gridLeft) / cellWidthScaled);
        int y = verCells - 1 - (int)((mousePos.y - gridTop) / cellHeightScaled);
        
        // Check if within bounds
        if (x >= 0 && x < horCells && y >= 0 && y < verCells) {
            grid[y * horCells + x] = false;
        }
    }
}

int main(int argc, char *argv[]){


    InitWindow(GetMonitorWidth(GetCurrentMonitor()), GetMonitorHeight(GetCurrentMonitor()), "Game of Life");
    SetTargetFPS(30);
    //ToggleFullscreen();
    //ToggleBorderlessWindowed();

    horCells = GetScreenWidth();
    verCells = GetScreenHeight();

    rowsPerThread = verCells / NUM_THREADS;

    grid = malloc(verCells * horCells * sizeof(bool));
    nextGrid = malloc(verCells * horCells * sizeof(bool));

    if (grid == NULL || nextGrid == NULL) {
        printf("Memory allocation failed\n");
        CloseWindow();
        return 1;
    }



    //horizontalDiff = GetScreenWidth() / horCells;
    //verticalDiff = GetScreenHeight() / verCells;

    cellSize = sqrt((GetScreenHeight() * GetScreenWidth())/(verCells * horCells));
    double tempTime;

    SetRandomSeed(time(NULL));
    InitializeGridTexture();

    while(!WindowShouldClose()){

        tempTime = GetTime();
        maxThreadTime = 0;
        
        
        // Update grid state if not paused
        if (!paused) {
            ThreadedIteration(); // Multi-threaded version
        }else{
            // for sigle generation processing
            if (IsKeyPressed(KEY_W)){
                ThreadedIteration();
            }
        }
        
        playerInput();
        handleZoomAndPan();
        UpdateGridTransformation();
        
        processingTime = GetTime() - tempTime;

        BeginDrawing();
        tempTime = GetTime();

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
        
        if (IsKeyDown(KEY_F10))
        {
            DrawText(TextFormat("%d", GetFPS()), 10, 0, 100, GREEN);
        }
        
        if (paused) {
            DrawText("PAUSED", (int) GetScreenWidth() / 2 - MeasureText("PAUSED", 250) / 2, GetScreenHeight()/ 18, 250, PAUSE);
        }
        
        //
        if (IsKeyDown(KEY_M)){
            float lineThickness = fmax(1.0f, zoomLevel * 0.5f);

            // Draw grid lines aligned with cell boundaries (optimistic)
            for (int x = 0; x <= horCells; x++) {
                float lineX = gridLeft + x * cellWidthScaled;
                Vector2 startPos = {lineX, gridTop};
                Vector2 endPos = {lineX, gridTop + zoomedHeight};
                DrawLineEx(startPos, endPos, lineThickness, LINE);
            }
            
            for (int y = 0; y <= verCells; y++) {
                float lineY = gridTop + y * cellHeightScaled;
                Vector2 startPos = {gridLeft, lineY};
                Vector2 endPos = {gridLeft + zoomedWidth, lineY};
                DrawLineEx(startPos, endPos, lineThickness, LINE);
            }
        }
        
        if (IsKeyDown(KEY_TAB)){
            DisplayTimings();
        }
        
        displayTime = GetTime() - tempTime;
        EndDrawing();
    }
    CleanUp();
    CloseWindow();
    return 0;
}