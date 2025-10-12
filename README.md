# SDL3 Experiments Demo
## Tetris
### Output in graphical (SDL3) and terminal mode and with different window sizes
<img width="350" height="702" alt="image" src="https://github.com/user-attachments/assets/8f573363-4334-4561-a2cf-39a4c94a4a4e" />

<img width="1000"  alt="image" src="https://github.com/user-attachments/assets/3d174a93-3efd-44ac-b226-8335b9301db0" />


### Build
```
cd sdl3_tetris
cmake -S . -B out
cmake --build out
```

## Chess
<img width="1400" height="1476" alt="image" src="https://github.com/user-attachments/assets/f139a3b6-ec0b-4031-af16-b556047ca250" />



### Build
```
cd sdl3_chess
cmake -S . -B out
cmake --build out
```

## NES VGM Player
This is a console application. It uses NES APU model from https://github.com/Shim06/Anemoia-ESP32 (output redirected to SDL audio subsystem). It opens VGM (Video Game Music) file format which contains commands like APU register writes, delays and sends these commands to the APU model for music synthesis. It makes a list from all the .vgm files in the current folder and plays them one after another. Keyboard control: n - next track, p - previous track, ESC - quit.



### Build
```
cd nes_vgm_player
cmake -S . -B out
cmake --build out
```
