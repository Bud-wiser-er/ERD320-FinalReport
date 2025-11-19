@echo off
echo ======================================================
echo Generating PlantUML diagrams for SNC subsystem
echo ======================================================
echo.

cd /d "%~dp0"

REM Check if plantuml.jar exists, if not download it
if not exist "plantuml.jar" (
    echo Downloading PlantUML...
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/plantuml/plantuml/releases/download/v1.2024.7/plantuml-1.2024.7.jar' -OutFile 'plantuml.jar'"
    if errorlevel 1 (
        echo ERROR: Failed to download PlantUML
        pause
        exit /b 1
    )
    echo PlantUML downloaded successfully.
    echo.
)

REM Create images directory if it doesn't exist
if not exist "images" mkdir images

REM Change to diagrams directory
cd diagrams

echo Generating all diagrams from diagrams folder...
echo.

REM Generate all diagrams using PlantUML
echo [1/13] Generating objectives tree...
java -jar ..\plantuml.jar -tpng objectives_tree.puml
if errorlevel 1 (echo   FAILED) else (echo   SUCCESS)

echo [2/13] Generating state machine...
java -jar ..\plantuml.jar -tpng state_machine.puml
if errorlevel 1 (echo   FAILED) else (echo   SUCCESS)

echo [3/13] Generating ICD sketch...
java -jar ..\plantuml.jar -tpng icd_sketch.puml
if errorlevel 1 (echo   FAILED) else (echo   SUCCESS)

echo [4/13] Generating ConOps swimlane...
java -jar ..\plantuml.jar -tpng conops_swimlane.puml
if errorlevel 1 (echo   FAILED) else (echo   SUCCESS)

echo [5/13] Generating architecture block (legacy)...
java -jar ..\plantuml.jar -tpng architecture_block.puml
if errorlevel 1 (echo   FAILED) else (echo   SUCCESS)

echo [6/13] Generating Main ESP32 architecture...
java -jar ..\plantuml.jar -tpng architecture_main_esp32.puml
if errorlevel 1 (echo   FAILED) else (echo   SUCCESS)

echo [7/13] Generating WiFi ESP32 architecture...
java -jar ..\plantuml.jar -tpng architecture_wifi_esp32.puml
if errorlevel 1 (echo   FAILED) else (echo   SUCCESS)

echo [8/13] Generating architecture interfaces...
java -jar ..\plantuml.jar -tpng architecture_interfaces.puml
if errorlevel 1 (echo   FAILED) else (echo   SUCCESS)

echo [9/13] Generating dataflow (legacy)...
java -jar ..\plantuml.jar -tpng dataflow.puml
if errorlevel 1 (echo   FAILED) else (echo   SUCCESS)

echo [10/13] Generating main control dataflow...
java -jar ..\plantuml.jar -tpng dataflow_main_control.puml
if errorlevel 1 (echo   FAILED) else (echo   SUCCESS)

echo [11/13] Generating telemetry dataflow...
java -jar ..\plantuml.jar -tpng dataflow_telemetry.puml
if errorlevel 1 (echo   FAILED) else (echo   SUCCESS)

echo [12/13] Generating remote control dataflow...
java -jar ..\plantuml.jar -tpng dataflow_remote_control.puml
if errorlevel 1 (echo   FAILED) else (echo   SUCCESS)

echo [13/13] Generating HMI wireframe...
java -jar ..\plantuml.jar -tpng hmi_wireframe.puml
if errorlevel 1 (echo   FAILED) else (echo   SUCCESS)

echo.
echo Moving all PNG files to images folder...
move /Y *.png ..\images\ >nul 2>&1

cd ..

echo.
echo ======================================================
echo Generation complete!
echo ======================================================
echo.
echo Generated diagrams in images folder:
echo   Core Diagrams:
echo     - objectives_tree.png
echo     - state_machine.png
echo     - icd_sketch.png
echo     - conops_swimlane.png (with color coding)
echo     - hmi_wireframe.png
echo.
echo   Architecture (split for readability):
echo     - architecture_main_esp32.png
echo     - architecture_wifi_esp32.png
echo     - architecture_interfaces.png
echo     - architecture_block.png (legacy - single diagram)
echo.
echo   Dataflow (split for readability):
echo     - dataflow_main_control.png
echo     - dataflow_telemetry.png
echo     - dataflow_remote_control.png
echo     - dataflow.png (legacy - single diagram)
echo.
echo Total: 13 diagrams
echo ======================================================
echo.

pause
