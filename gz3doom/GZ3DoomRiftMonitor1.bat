REM This .bat file is for launching GZ3Doom in Oculus Rift mode
REM Drag and Drop of .pk3 and PWAD files is supported
REM Change working folder to .bat location, so we can find gz3doom, even if a file from another folder is dragged onto this .bat
cd /d "%~dp0"
REM Place extra arguments first, since that seems to work better for brutal doom pk3 file.
gz3doom.exe %* +vr_mode 8 -width 640 -height 480 +oculardium_optimosa
REM pause is for debugging
REM pause
