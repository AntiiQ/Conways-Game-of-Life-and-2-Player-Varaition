# Conway-s-Game-Of-Life-2-player-variation
Simple Conway's Game of Life Simulation and a short competitive 2 player variation.

#Conway's Game of Life Simulation
The standard simulation is a simple program which I have developed in order to learn multi-threading and further learn raylib. It follows all the rules of Conway's Game of life

While the simulation runs reasonably well, further improvements could be made to the way it handles each thread. Currently the program is creating 24 threads each frame. A better implementation would create the threads at start-up and reuse them throughout, synchronizing when needed. Additionally, the sliding window algorithm could be refined (by that I mean completely redone) so that the values are reused across vertical cells as well.

A few user inputs are implemented
- 1: Sets target FPS to 30
- 2: Sets target FPS to 144
- 3: Sets target FPS to 1440
- 0: Sets target FPS to 10

- F10: Shows FPS
- TAB: Shows timing metrics

- Panning with left click
- Zooming in with scroll wheel
- Reset position with R

- SPACE BAR: Pause the simulation
- W: Progress the simulation by 1 iteration while it is paused
  
- LEFT CLICK: Turn on the cell that the mouse points to.
- MIDDLE CLICK: Turn off the cell that the mouse points to.
- E: Turn on the border cells
- ENTER: Randomises all the cells
- BACKSPACE: Turns off all the cells

#2 Player Variation of Conway's Game of Life

This is a simple game created in a rush over a few hours. It requires 2 players to act in turns.

In the first turn both players are allowed to place 5 cells anywhere in the grid as long as it doesn't overwrite the other's cells.

In the subsequent turns, each player can: 
1: Convert one cell from the grid to their colour.
2: Turn off one cell from the grid.

The game is over once only one type of cell is left on the grid.

This game follows all Conway's Game of Life standard rules:
1. Any live cell with fewer than two live neighbours dies.
2. Any live cell with two or three live neighbours lives on to the next generation.
3. Any live cell with more than three live neighbours dies.
4. Any dead cell with exactly three live neighbours becomes a live cell.

Hover these rules are added to convert to the 2 player medium:
1. Any cell with 2 live neighbours of one colour, and another 1 live neighbours of the other colour, will become a live cell of the former colour
2. Any live cell with only 2 live neighbours of different colours will have a 50/50 chance of changing colours.

The implementation has much repeated code, especially in the player turns and drawing category, due to me rushing when developing this. If I were to work on this further, this would be the first priority.
